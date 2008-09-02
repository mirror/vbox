/* A couple of routines to implement a low-overhead timer for drivers */

 /*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

/*
 * Sun GPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the General Public License version 2 (GPLv2) at this time for any software where
 * a choice of GPL license versions is made available with the language indicating
 * that GPLv2 or any later version may be used, or where a choice of which version
 * of the GPL is applied is otherwise unspecified.
 */

#include	"etherboot.h"
#include	"timer.h"

/* Machine Independant timer helper functions */

void mdelay(unsigned int msecs)
{
	unsigned int i;
	for(i = 0; i < msecs; i++) {
		udelay(1000);
		poll_interruptions();
	}
}

void waiton_timer2(unsigned int ticks)
{
	load_timer2(ticks);
	while(timer2_running()) {
		poll_interruptions();
	}
}
