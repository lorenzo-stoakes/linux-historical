/* -*- linux-c -*- */
/* 
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "8253xioc.h"
#include "Reg9050.h"


/* This application reprograms the eprom associated with the 9050 */

int main(int argc, char **argv)
{
	int fd;
	unsigned short oldeeprom[EPROM9050_SIZE], neweeprom[EPROM9050_SIZE];
	char buffer[200];
	int count;
	int value;
	unsigned short *pointer;
	unsigned short *pointerold;
	int noprompt = 0;
	int epromindex;
	
	if(argc < 2)
	{
		fprintf(stderr, "Syntax: %s {portname} [-n] {prom values}.\n", *argv);
		exit(-1);
	}
	fd = open(argv[1], O_RDWR);
	if(fd < 0)
	{
		perror("open failed.");
		exit(-2);
	}
	
	if((argc > 2) && !strcmp("-n", argv[2]))
	{
		noprompt = 1;
	}
	
	/* get the current values */
	if(ioctl(fd, ATIS_IOCGSEP9050, &oldeeprom) < 0)
	{
		perror("ioctl failed.");
		exit(-3);
	}
	/* set up the existing values as defaults */
	memcpy(neweeprom, oldeeprom, sizeof(oldeeprom));
	/* gather all new values from the command line */
	/* or via tty input.*/
	for(count = (2+noprompt), pointer = neweeprom; count < argc; ++count, ++pointer)
	{
		*pointer = atoi(argv[count]);
	}
	pointer = neweeprom;
	pointerold = oldeeprom;
	for(epromindex = 0; epromindex < EPROM9050_SIZE; ++epromindex)
	{
		fprintf(stderr, "LOCATION %i [%4.4x/%4.4x]: ", epromindex, *pointerold, *pointer);
		
		if(!noprompt)
		{
			if(count = read(0, buffer, 150), count <= 0)
			{
				exit(0);
			}
			buffer[count] = '\0';
			if(buffer[0] != '\n')
			{
				sscanf(buffer, "%x", &value);
				*pointer = (unsigned short) value;
			}
		}
		else
		{
			fprintf(stderr, "\n");
		}
		++pointerold;
		++pointer;
	}
	/* This ioctl does the actual register load. */
	if(ioctl(fd, ATIS_IOCSSEP9050, neweeprom) < 0)
	{
		perror("ioctl failed.");
		exit(-3);
	}
	
	fflush(stdout);
}
