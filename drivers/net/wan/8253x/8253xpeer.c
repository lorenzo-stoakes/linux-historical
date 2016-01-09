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
#include <sys/poll.h>

struct pollfd pollarray[2];

/* This application sets up synchronous character mode and loosely
 * emulates putmsg/getmsg use with read and write. */

char buffer[8192];

int main(int argc, char **argv)
{
	int fd;
	int status;
	int prompt = 1;
	int count;
	
	if(argc != 2)
	{
		fprintf(stderr, "Syntax: %s {portname}\n", *argv);
		exit(-1);
	}
	fd = open(argv[1], O_RDWR);
	if(fd < 0)
	{
		perror("open failed.");
		exit(-2);
	}
	do
	{
		if(prompt)
		{
			printf("Enter data: ");
			fflush(stdout);
			prompt = 0;
		}
		pollarray[0].fd = 0;
		pollarray[0].events = POLLIN;
		pollarray[0].revents = 0;
		pollarray[1].fd = fd;
		pollarray[1].events = POLLIN|POLLOUT;
		pollarray[1].revents = 0;
		status = poll(pollarray, 2, 10);
		switch(status)
		{
		case 0:
			break;
			
		case 1:
		case 2:
			if(pollarray[0].revents == POLLIN)
			{
				if(count = read(0, buffer, 150), count <= 0)
				{
					perror("unable to read stdio.\n");
					exit(0);
				}
				buffer[count] = '\0';
				if(count)
				{
					if(pollarray[1].revents & POLLOUT)
					{
						if(write(pollarray[1].fd, buffer, count) <= 0)
						{
							perror("unable to write protodevice.\n");
							exit(-1);
						}
					}
					else
					{
						printf("Write of protodevice would block.\n");
						fflush(stdout);
					}
				}
				prompt = 1;
			}
			if(pollarray[1].revents & POLLIN)
			{
				if(count = read(pollarray[1].fd, buffer, 8192), count <= 0)
				{
					perror("unable to read protodevice.\n");
					exit(0);
				}
				buffer[count] = '\0';
				printf("\nRead: %s", buffer);
				fflush(stdout);
				prompt = 1;
			}
			break;
			
		default:
			break;
		}
	}
	while(status >= 0);
}

