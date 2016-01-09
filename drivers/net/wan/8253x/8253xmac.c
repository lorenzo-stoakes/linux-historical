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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "ring.h"
#include <linux/socket.h>
#include <net/if.h>

				/* This application sets a pseudo mac address that */
				/* can be used when using the synchronous port in */
				/* synchronous serial ethnernet emulation mode */

int main(int argc, char **argv)
{
  int fd;
  struct ifreq request;
  PSEUDOMAC pmac;
  char buffer[200];
  int count;
  unsigned int uppernib;
  unsigned int lowernib;

  
  if(argc != 3)
    {
      fprintf(stderr, "Syntax: %s {ifname} {macaddr}.\n", *argv);
      fflush(stdout);
      exit(-1);
    }
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd < 0)
    {
      perror("socket failed.");
      fflush(stdout);
      exit(-2);
    }

  strcpy(request.ifr_ifrn.ifrn_name, argv[1]); /* requests go through the socket layer */

  request.ifr_ifru.ifru_data = (char*) &pmac;

  if(ioctl(fd, SAB8253XGETMAC, &request) < 0)
    {
      perror("ioctl failed.");
      fflush(stdout);
      exit(-3);
    }
  for(count = 0; count < 6; ++count)
    {
      buffer[2*count] = (pmac.addr[count] >> 4);
      buffer[2*count] &= 0x0F;
      if(buffer[2*count] < 10)
	{
	  buffer[2*count] += '0';
	}
      else
	{
	  buffer[2*count] += 'a';
	}
      buffer[(2*count)+1] = (pmac.addr[count] & 0x0F);
      if(buffer[(2*count)+1] < 10)
	{
	  buffer[(2*count)+1] += '0';
	}
      else
	{
	  buffer[(2*count)+1] += 'a';
	}
    }
  buffer[12] = 0;
  printf("Old mac addres is %s.\n", buffer);
  if(strlen(argv[2]) != 12)
    {
      printf("Bad size mac address %s.\n", argv[2]);
      fflush(stdout);
      exit(-1);
    }
  for(count = 0; count < 6; ++ count)
    {
      uppernib = argv[2][2*count];
      lowernib = argv[2][(2*count)+1];
      
      switch(uppernib)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  uppernib -= '0';
	  break;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	  uppernib -= 'a';
	  break;
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	  uppernib -= 'A';
	  break;
	default:
	  uppernib = 0;
	  break;
	}
      switch(lowernib)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  lowernib -= '0';
	  break;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	  lowernib -= 'a';
	  break;
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	  lowernib -= 'A';
	  break;
	default:
	  lowernib = 0;
	  break;
	}
      pmac.addr[count] = ((uppernib << 4) | lowernib);
    }
    

  if(ioctl(fd, SAB8253XSETMAC, &request) < 0) /* actually setting the mac address */
    {
      perror("ioctl failed.");
      fflush(stdout);
      exit(-2);
    }

  if(ioctl(fd, SAB8253XGETMAC, &request) < 0) /* getting it back so that value can be verified */
    {
      perror("ioctl failed.");
      exit(-3);
    }
  for(count = 0; count < 6; ++count)
    {
      buffer[2*count] = (pmac.addr[count] >> 4);
      buffer[2*count] &= 0x0F;
      if(buffer[2*count] < 10)
	{
	  buffer[2*count] += '0';
	}
      else
	{
	  buffer[2*count] += 'a';
	}
      buffer[(2*count)+1] = (pmac.addr[count] & 0x0F);
      if(buffer[(2*count)+1] < 10)
	{
	  buffer[(2*count)+1] += '0';
	}
      else
	{
	  buffer[(2*count)+1] += 'a';
	}
    }
  buffer[12] = 0;
  printf("New mac addres is %s.\n", buffer);
  fflush(stdout);
  exit(0);
}
