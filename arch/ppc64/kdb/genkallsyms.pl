#!/usr/bin/perl -w
# File genkallsyms.pl created by Todd Inglett at 07:56:04 on Mon Apr 30 2001. 
# use Math::BigInt;

my $me = "genkallsyms";
my $objdump = "objdump";
my $outfile;
my $objfile;
my $verbose = 0;
my $dummy = 0;

my %sections;		# indexed by sect name.  Value is [Name, Size, Addr, Name_off, Sect_idx]
my $sectlist;		# same section references in a list.
my $nextsect = 0;	# index of next free section in sectlist.

my @symbols;		# Value is [Name, sec_ref, Addr, Name_off]
my $nextsym = 0;	# index of next free symbol
my $strtablen = 0;	# string table length.

# my $shift32multiplier = Math::BigInt->new(2**30) * 4;  # 2**32 without going floating point

#
# Parse the cmdline options
#
my $arg;
while (defined($arg = shift)) {
    if ($arg eq '-objdump') {
	$objdump = shift;
    } elsif ($arg eq '-o') {
	$outfile = shift;
    } elsif ($arg eq '-h') {
	usage();
    } elsif ($arg eq '-v') {
	$verbose = 1;
    } elsif ($arg eq '-dummy') {
	$dummy = 1;
    } elsif ($arg !~ m/^-/) {
	unshift @ARGV, $arg;
	last;
    } else {
	die "$me: unknown option $arg\n";
    }
}

usage() unless ($#ARGV == 0 || $dummy);
$objfile = $ARGV[0];	# typically vmlinux

if ($verbose) {
    print "outfile=$outfile\n";
    print "objdump=$objdump\n";
    print "objfile=$objfile\n";
}
( $dummy || -r $objfile ) or die "$me: cannot read $objfile: $!\n";


if (!$dummy) {
    #
    # Read the section table from objdump -h into hash
    # %sections.  Each entry is [Name, Size, Addr, Name_Off, Sect_idx] and
    # the hash is indexed by Name.
    #
    open(S, "$objdump -h $objfile 2>&1 |") or die "$me: cannot $objdump -h $objfile: $!\n";
    my $sline;
    while (defined($sline = <S>)) {
	chomp($sline);
	$sline =~ s/^\s*//;
	next unless $sline =~ m/^[0-9]/;
	my ($idx, $name, $size, $vma, $rest) = split(/\s+/, $sline, 5);
	print "name=$name, size=$size, vma=$vma\n" if $verbose;
	$sectlist[$nextsect] = [$name, $size, $vma, 0, 0];
	$sections{$name} = $sectlist[$nextsect++];
    }
    close(S);
    print "$me: processed $nextsect sections\n" if $verbose;

    # Sort by start addr for readability.  This is not required.
    @sectlist = sort { $a->[2] cmp $b->[2] } @sectlist;
    for (my $i = 0; $i < $nextsect; $i++) {
	$sectlist[$i]->[4] = $i;	# Sect_Idx
    }

    #
    # Now read the symbol table from objdump -t
    #
    open(S, "$objdump -t $objfile 2>&1 |") or die "$me: cannot $objdump -t $objfile: $!\n";
    while (defined($sline = <S>)) {
	last if $sline =~ m/^SYMBOL/;
    }
    while (defined($sline = <S>)) {
	chomp($sline);
	my ($addr, $foo, $type, $sect, $val, $name) = split(/\s+/, $sline);
	# Now type might be empty.
	if (!defined($name)) {
	    $name = $val;
	    $val = $sect;
	    $sect = $type;
	    $type = " ";
	}
	# Weed out symbols which are not useful.
	next if !$name;
	next if $sect eq ".rodata"; # TOC entries, I think.
	next if $sect eq "*ABS*";
	next if $name =~ m/^LC?\.\./;
	# print "$addr,$type,$sect,$val,$name\n" if $verbose;
	my $sec_ref = $sections{$sect};
	$symbols[$nextsym++] = [$name, $sec_ref, $addr, 0];
    }
    close(S);
    print "$me: processed $nextsym symbols\n" if $verbose;
    # Sort by addr for readability.  This is not required.
    @symbols = sort { $a->[2] cmp $b->[2] } @symbols;

    #
    # Now that everything is sorted, we need to assign space for
    # each string in the string table.
    #
    for (my $i = 0; $i < $nextsect; $i++) {
	my $len = length($sectlist[$i]->[0]);
	$sectlist[$i]->[3] = $strtablen;
	$strtablen += $len+1;
    }
    for (my $i = 0; $i < $nextsym; $i++) {
	my $len = length($symbols[$i]->[0]);
	$symbols[$i]->[3] = $strtablen;
	$strtablen += $len+1;
    }
}

if ($outfile) {
    open(OUT, ">$outfile") or die "$me:  cannot open $outfile for write: $!\n";
} else {
    open(OUT, ">&STDOUT");
}

if ($dummy) {
    # Produce dummy output and exit.
    print OUT "long __start___kallsyms = 0;\n";
    print OUT "long __stop___kallsyms = 0;\n";
    exit 0;
}

print OUT "#include <stddef.h>\n";
print OUT "#include <linux/kallsyms.h>\n\n";
print OUT "#define NUM_SECTIONS\t\t", int(keys %sections), "\n";
print OUT "#define NUM_SYMBOLS\t\t", $nextsym, "\n";
print OUT "#define NUM_STRING_CHARS\t", $strtablen, "\n";
print OUT "#define FIRST_SECTION_ADDR\t0x", $sectlist[0]->[2], "UL /* section ", $sectlist[0]->[0], " */\n";
print OUT "#define LAST_SECTION_ADDR\t0x", $sectlist[$nextsect-1]->[2], "UL + 0x", $sectlist[$nextsect-1]->[1], "UL /* section ", $sectlist[$nextsect-1]->[0], " */\n";
print OUT <<EOF;

#define SOFF(secidx) (sizeof(struct kallsyms_section)*secidx)
typedef struct kdata {
	struct kallsyms_header hdr;
	struct kallsyms_section sections[NUM_SECTIONS];
	struct kallsyms_symbol symbols[NUM_SYMBOLS];
	const char strings[NUM_STRING_CHARS];
} kdata_t;

kdata_t __start___kallsyms = {
  {	/* kallsyms_header */
	sizeof(struct kallsyms_header),	/* Size of this header */
	sizeof(kdata_t),		/* Total size of kallsyms data */
	NUM_SECTIONS,			/* Number of section entries */
	offsetof(kdata_t, sections),	/* Offset to first section entry */
	sizeof(struct kallsyms_section),/* Size of one section entry */
	NUM_SYMBOLS,			/* Number of symbol entries */
	offsetof(kdata_t, symbols),	/* Offset to first symbol entry */
	sizeof(struct kallsyms_symbol),	/* Size of one symbol entry */
	offsetof(kdata_t, strings),	/* Offset to first string */
	FIRST_SECTION_ADDR,		/* Start address of first section */
	LAST_SECTION_ADDR,		/* End address of last section */
  },
  {	/* kallsyms_section table */
	/* start_addr, size, name_off, flags */
EOF

for (my $i = 0; $i < $nextsect; $i++) {
    my $sectref = $sectlist[$i];
    print OUT "\t{0x", $sectref->[2], "UL, 0x", $sectref->[1], ", ", $sectref->[3], "},\t /* [$i] ", $sectref->[0], " */\n";
}

print OUT <<EOF;
  },
  {	/* kallsyms_symbol table */
	/* section_off, addr, name_off */
EOF

for (my $i = 0; $i < $nextsym; $i++) {
    my $symref = $symbols[$i];
    my $secref = $symref->[1];
    print OUT "\t{SOFF(", $secref->[4], "), 0x", $symref->[2], "UL, ", $symref->[3], "},  /* ", $symref->[0], " */\n";
}

print OUT <<EOF;
  },
  /* string table */ ""
EOF

for (my $i = 0; $i < $nextsect; $i++) {
    my $sectref = $sectlist[$i];
    print OUT "\t\"", $sectref->[0], "\\0\"\t/* ", $sectref->[3], " */\n";
}
for (my $i = 0; $i < $nextsym; $i++) {
    my $symref = $symbols[$i];
    print OUT "\t\"", $symref->[0], "\\0\"\t/* ", $symref->[3], " */\n";
}


print OUT <<EOF;
};

long __stop___kallsyms = 0;
EOF

sub usage {
    die "Usage:  genkallsyms [-h] -objdump /path/to/objdump -o _ksyms.c vmlinux\n";
}

# Return a BigInt given a 64bit hex number (without a leading 0x)..
# The number is assumed to be 16 chars long.
#sub bighex {
#    my ($hexstr) = @_;
#    my $bighi = Math::BigInt->new(hex(substr($hexstr, 0, 8)));
#    my $biglow = Math::BigInt->new(hex(substr($hexstr, 8, 8)));
#    return $bighi * $shift32multiplier + $biglow;
#}
