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


/* This application shows how to set custom speeds/baudrates */

int main(int argc, char **argv)
{
	int fd;
	unsigned long oldspeed, newspeed;
	char buffer[200];
	int count;
	long value;
	int noprompt = 0;
	int epromindex;
	
	if(argc < 2)
	{
		fprintf(stderr, "Syntax: %s {portname} [-n] {new speed}.\n", *argv);
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
	if(ioctl(fd, ATIS_IOCGSPEED, &oldspeed) < 0)
	{
		perror("ioctl failed.");
		exit(-3);
	}
	/* set up the existing values as defaults */
	newspeed = oldspeed;
	/* gather all new values from the command line */
	/* or via tty input.*/
	if(argc == (noprompt + 3))
	{
		newspeed = atoi(argv[count]);
	}
	
	fprintf(stderr, "speed [%ld/%ld]: ", oldspeed, newspeed);
	
	if(!noprompt)
	{
		if(count = read(0, buffer, 150), count <= 0)
		{
			exit(0);
		}
		buffer[count] = '\0';
		if(buffer[0] != '\n')
		{
			sscanf(buffer, "%ld", &newspeed);
		}
	}
	else
	{
		fprintf(stderr, "\n");
	}
	
	/* This ioctl does the actual register load. */
	if(ioctl(fd, ATIS_IOCSSPEED, &newspeed) < 0)
	{
		perror("ioctl failed.");
		exit(-3);
	}
	
	fflush(stdout);
}
