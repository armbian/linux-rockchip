/*
 * Common stats definitions for clients of dongle
 * ports
 *
 * Copyright (C) 2022, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef _dngl_stats_h_
#define _dngl_stats_h_

typedef struct {
	unsigned long	rx_packets;		/* total packets received */
	unsigned long	tx_packets;		/* total packets transmitted */
	unsigned long	rx_bytes;		/* total bytes received */
	unsigned long	tx_bytes;		/* total bytes transmitted */
	unsigned long	rx_errors;		/* bad packets received */
	unsigned long	tx_errors;		/* packet transmit problems */
	unsigned long	rx_dropped;		/* packets dropped by dongle */
	unsigned long	tx_dropped;		/* packets dropped by dongle */
	unsigned long   multicast;		/* multicast packets received */
} dngl_stats_t;

// remove the conditional after moving all branches to use the new code
#define USE_WIFI_STATS_H

#endif /* _dngl_stats_h_ */
