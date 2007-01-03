/* liebert.c - support for Liebert UPS models via MultiLink cable.

   Copyright (C) 2002  Russell Kroll <rkroll@exploits.org>

   Based on old-style multilink.c driver:

   Copyright (C) 2001  Rick Lyons <rick@powerup.com.au>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "main.h"
#include "serial.h"
#include "liebert.h"
#include <sys/ioctl.h>

#define	ML_ONBATTERY	0x55

void upsdrv_shutdown(void)
{
	/* XXX: replace with a proper shutdown function (raise DTR) */

	/* worse yet: stock cables don't support shutdown at all */

	fatalx("shutdown not supported");
}

void upsdrv_initinfo(void)
{
	char	*tmp;

	tmp = getval("mfr");

	if (!tmp)
		dstate_setinfo("ups.mfr", "Liebert");
	else
		dstate_setinfo("ups.mfr", "%s", tmp);

	tmp = getval("model");

	if (!tmp)
		dstate_setinfo("ups.model", "MultiLink");
	else
		dstate_setinfo("ups.model", "%s", tmp);
}

/* require this many OBs or LBs before actually setting it */
#define DEBOUNCE 3

/* normal idle loop - keep up with the current state of the UPS */
void upsdrv_updateinfo(void)
{
	int	flags, ret;
	unsigned char	c;
	unsigned int	ob, lb;
	static	unsigned int	ob_state = 0, ob_last = 0, ob_ctr = 0;
	static	unsigned int	lb_state = 0, lb_last = 0, lb_ctr = 0;

	ob = lb = 0;

	/* the UPS connects RX to TX when on battery, so test for loopback */

	ser_flush_in(upsfd, "", 0);

	c = ML_ONBATTERY;
	ser_send_char(upsfd, c);
	if ((ser_get_char(upsfd, &c, 1, 0) == 1) {
		while (ser_get_char(upsfd, &c, 1, 0) == 1)
			continue;
		if (c == ML_ONBATTERY)
			ob = 1;
	}
	
	ret = ioctl(upsfd, TIOCMGET, &flags);

	if (ret != 0) {
		upslog_with_errno(LOG_INFO, "ioctl failed");
		dstate_datastale();
		return;
	}

	if (flags & TIOCM_CD)
		lb = 1;

	/* state machine below to ensure status changes are debounced */

	/* OB/OL state change: reset counter */
	if (ob_last != ob)
		ob_ctr = 0;
	else
		ob_ctr++;

	upsdebugx(2, "OB: state %d last %d now %d ctr %d",
		ob_state, ob_last, ob, ob_ctr);

	if (ob_ctr >= DEBOUNCE) {

		if (ob != ob_state) {

			upsdebugx(2, "OB: toggling state");

			if (ob_state == 0)
				ob_state = 1;
			else
				ob_state = 0;
		}
	}

	ob_last = ob;

	/* now do it again for LB */

	/* state change: reset counter */
	if (lb_last != lb)
		lb_ctr = 0;
	else
		lb_ctr++;

	upsdebugx(2, "LB: state %d last %d now %d ctr %d",
		lb_state, lb_last, lb, lb_ctr);

	if (lb_ctr >= DEBOUNCE) {

		if (lb != lb_state) {

			upsdebugx(2, "LB: toggling state");

			if (lb_state == 0)
				lb_state = 1;
			else
				lb_state = 0;
		}
	}

	lb_last = lb;

	status_init();

	if (ob_state == 1)
		status_set("OB");	/* on battery */
	else
		status_set("OL");	/* on line */

	if (lb_state == 1)
		status_set("LB");	/* low battery */

	status_commit();
	dstate_dataok();
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Liebert MultiLink UPS driver %s (%s)\n",
		DRV_VERSION, UPS_VERSION);

	experimental_driver = 1;
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "mfr", "Override manufacturer name");
	addvar(VAR_VALUE, "model", "Override model name");
}

void upsdrv_help(void)
{
}

void upsdrv_initups(void)
{
	int rts_bit = TIOCM_RTS;

	upsfd = ser_open(device_path);

	/* raise RTS */
	ioctl(upsfd, TIOCMBIS, &rts_bit);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
