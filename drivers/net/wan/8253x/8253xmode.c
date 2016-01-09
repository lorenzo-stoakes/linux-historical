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

static char *signaling[] =
{
	"OFF",
	"RS232",
	"RS422",
	"RS485",
	"RS449",
	"RS530",
	"V.35"
};

				/* This application shows how to set sigmode
				 * on those devices that support software
				 * programmable signaling. */
int main(int argc, char **argv)
{
	int fd;
	unsigned int oldmode, newmode;
	
	if(argc != 3)
	{
		fprintf(stderr, "Syntax: %s {portname} {new mode}.\n", *argv);
		fprintf(stderr, "{new mode} = off | 232 | 422 | 485 | 449 | 530 | v.35\n");
		exit(-1);
	}
	fd = open(argv[1], O_RDWR);
	if(fd < 0)
	{
		perror("open failed.");
		exit(-2);
	}
	if(!strcmp("off", argv[2]))
	{
		newmode = SP502_OFF_MODE;
	}
	else if(!strcmp("232", argv[2]))
	{
		newmode = SP502_RS232_MODE;
	}
	else if(!strcmp("422", argv[2]))
	{
		newmode = SP502_RS422_MODE;
	}
	else if(!strcmp("485", argv[2]))
	{
		newmode = SP502_RS485_MODE;
	}
	else if(!strcmp("449", argv[2]))
	{
		newmode = SP502_RS449_MODE;
	}
	else if(!strcmp("530", argv[2]))
	{
		newmode = SP502_EIA530_MODE;
	}
	else if(!strcmp("v.35", argv[2]))
	{
		newmode = SP502_V35_MODE;
	}
	else
	{
		fprintf(stderr, "Unknown mode %s.\n", argv[2]);
		fprintf(stderr, "Syntax: %s {portname} {new mode}.\n", *argv);
		fprintf(stderr, "{new mode} = off | 232 | 422 | 485 | 449 | 530 | v.35\n");
		exit(-1);
	}
	
	/* get the current values */
	if(ioctl(fd, ATIS_IOCGSIGMODE, &oldmode) < 0)
	{
		perror("ATIS_IOCGSIGMODE ioctl failed.");
		exit(-3);
	}
	fprintf(stderr, "old mode = %s.\n", signaling[oldmode]);
	
	if(ioctl(fd, ATIS_IOCSSIGMODE, &newmode) < 0)
	{
		perror("ATIS_IOCSSIGMODE ioctl failed.");
		exit(-3);
	}

	/* get the current values */
	if(ioctl(fd, ATIS_IOCGSIGMODE, &oldmode) < 0)
	{
		perror("ATIS_IOCGSIGMODE ioctl failed.");
		exit(-3);
	}	
	fprintf(stderr, "new mode = %s.\n", signaling[oldmode]);
	fflush(stdout);
}
