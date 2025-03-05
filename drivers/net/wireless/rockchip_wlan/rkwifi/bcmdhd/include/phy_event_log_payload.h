/*
 * PHY related EVENT_LOG System Definitions
 *
 * This file describes the payloads of PHY related event log entries that are data buffers
 * rather than formatted string entries.
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

#ifndef _PHY_EVENT_LOG_PAYLOAD_H_
#define _PHY_EVENT_LOG_PAYLOAD_H_

#include <typedefs.h>
#include <bcmutils.h>
#include <ethernet.h>
#include <event_log_tag.h>
#include <wlioctl.h>

typedef struct {
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  goodfcs;        /**< Good fcs counters  */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */
} phy_periodic_counters_v1_t;

typedef struct {
	uint32	txallfrm;		/**< total number of frames sent, incl. Data, ACK, RTS,
					* CTS, Control Management
					*  (includes retransmissions)
					*/
	uint32	rxrsptmout;		/**< number of response timeouts for transmitted frames
								* expecting a response
								*/
	uint32	rxstrt;				/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  goodfcs;	/**< Good fcs counters  */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */

	uint32 txctl;
	uint32 rxctl;
	uint32 txbar;
	uint32 rxbar;
	uint32 rxbeaconbss;

	uint32 txrts;
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	txackfrm;	/**< number of ACK frames sent out */

	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */

	uint32 rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */

	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */
	uint32	rxctlucast;	/**< number of received CNTRL frames
						* with good FCS and matching RA
						*/

	uint32 last_bcn_seq_num;	/**< * L_TSF and Seq # from the last beacon received */
	uint32 last_bcn_ltsf;		/**< * L_TSF and Seq # from the last beacon received */

	uint16 counter_noise_request;	/**< counters of requesting noise samples for noisecal> */
	uint16 counter_noise_crsbit;	/**< counters of noise mmt being interrupted
									* due to PHYCRS>
									*/
	uint16 counter_noise_apply;		/**< counts of applying noisecal result> */

	uint8 phylog_noise_mode;		/**< noise cal mode> */
	uint8 total_desense_on;			/**< total desense on flag> */
	uint8 initgain_desense;			/**< initgain desense when total desense is on> */
	uint8 crsmin_init;		/**< crsmin_init threshold when total desense is on> */
	uint8 lna1_gainlmt_desense;	/**< lna1_gain limit desense when applying desense> */
	uint8 clipgain_desense0;		/**< clipgain desense when applying desense > */

	uint32 desense_reason;			/**< desense paraemters to indicate reasons
									* for bphy and ofdm_desense>
									*/
	uint8 lte_ofdm_desense;			/**< ofdm desense dut to lte> */
	uint8 lte_bphy_desense;			/**< bphy desense due to lte> */

	uint8 hw_aci_status;			/**< HW ACI status flag> */
	uint8 aci_clipgain_desense0;		/**< clipgain desense due to aci> */
	uint8 lna1_tbl_desense;			/**< LNA1 table desense due to aci> */

	int8 crsmin_high;			/**< crsmin high when applying desense> */
	int8 weakest_rssi;			/**< weakest link RSSI> */
	int8 phylog_noise_pwr_array[8][2];	/**< noise buffer array> */
} phy_periodic_counters_v5_t;

typedef struct {

	/* RX error related */
	uint32	rxrsptmout;	/* number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxbadplcp;	/* number of parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/* PHY was able to correlate the preamble but not the header */
	uint32	rxnodelim;	/* number of no valid delimiter detected by ampdu parser */
	uint32	bphy_badplcp;	/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadfcs;	/* number of frames for which the CRC check failed in the MAC */
	uint32  rxtoolate;	/* receive too late */
	uint32  rxf0ovfl;	/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/* Rx FIFO1 overflow counters information */
	uint32	rxanyerr;	/* Any RX error that is not counted by other counters. */
	uint32	rxdropped;	/* Frame dropped */
	uint32	rxnobuf;	/* Rx error due to no buffer */
	uint32	rxrunt;		/* Runt frame counter */
	uint32	rxfrmtoolong;	/* Number of received frame that are too long */
	uint32	rxdrop20s;

	/* RX related */
	uint32	rxstrt;		/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;	/* beacons received from member of BSS */
	uint32	rxdtucastmbss;	/* number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/* number of received DATA frames (good FCS and no matching RA) */
	uint32  goodfcs;        /* Good fcs counters  */
	uint32	rxctl;		/* Number of control frames */
	uint32	rxaction;	/* Number of action frames */
	uint32	rxback;		/* Number of block ack frames rcvd */
	uint32	rxctlucast;	/* Number of received unicast ctl frames */
	uint32	rxframe;	/* Number of received frames */

	/* TX related */
	uint32	txallfrm;	/* total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;
} phy_periodic_counters_v3_t;

typedef struct phy_periodic_counters_v4 {
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32  rxdropped;
	uint32  rxcrc;
	uint32  rxnobuf;
	uint32  rxrunt;
	uint32  rxgiant;
	uint32  rxctl;
	uint32  rxaction;
	uint32  rxdrop20s;
	uint32  rxctsucast;
	uint32  rxrtsucast;
	uint32  txctsfrm;
	uint32  rxackucast;
	uint32  rxback;
	uint32  txphyerr;
	uint32  txrtsfrm;
	uint32  txackfrm;
	uint32  txback;
	uint32	rxnodelim;
	uint32	rxfrmtoolong;
	uint32	rxctlucast;
	uint32	txbcnfrm;
	uint32	txdnlfrm;
	uint32	txampdu;
	uint32	txmpdu;
	uint32  txinrtstxop;
	uint32	prs_timeout;
} phy_periodic_counters_v4_t;

/* phy_periodic_counters_v5_t  reserved for 4378 */
typedef struct phy_periodic_counters_v6 {
	/* RX error related */
	uint32	rxrsptmout;	/* number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxbadplcp;	/* number of parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/* PHY was able to correlate the preamble but not the header */
	uint32	rxnodelim;	/* number of no valid delimiter detected by ampdu parser */
	uint32	bphy_badplcp;	/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadfcs;	/* number of frames for which the CRC check failed in the MAC */
	uint32  rxtoolate;	/* receive too late */
	uint32  rxf0ovfl;	/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/* Rx FIFO1 overflow counters information */
	uint32	rxanyerr;	/* Any RX error that is not counted by other counters. */
	uint32	rxdropped;	/* Frame dropped */
	uint32	rxnobuf;	/* Rx error due to no buffer */
	uint32	rxrunt;		/* Runt frame counter */
	uint32	rxfrmtoolong;	/* Number of received frame that are too long */
	uint32	rxdrop20s;

	/* RX related */
	uint32	rxstrt;		/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;	/* beacons received from member of BSS */
	uint32	rxdtucastmbss;	/* number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/* number of received DATA frames (good FCS and no matching RA) */
	uint32  goodfcs;        /* Good fcs counters  */
	uint32	rxctl;		/* Number of control frames */
	uint32	rxaction;	/* Number of action frames */
	uint32	rxback;		/* Number of block ack frames rcvd */
	uint32	rxctlucast;	/* Number of received unicast ctl frames */
	uint32	rxframe;	/* Number of received frames */

	uint32	rxbar;		/* Number of block ack requests rcvd */
	uint32	rxackucast;	/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;	/* number of OBSS beacons received */
	uint32	rxctsucast;	/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;	/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;	/* total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;	/* number of ACK frames sent out */
	uint32	txrtsfrm;	/* number of RTS sent out by the MAC */
	uint32	txctsfrm;	/* number of CTS sent out by the MAC */

	uint32	txctl;		/* Number of control frames txd */
	uint32	txbar;		/* Number of block ack requests txd */
	uint32	txrts;		/* Number of RTS txd */
	uint32	txback;		/* Number of block ack frames txd */
	uint32	txucast;	/* number of unicast tx expecting response other than cts/cwcts */

	/* TX error related */
	uint32	txrtsfail;	/* RTS TX failure count */
	uint32	txphyerr;	/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;
} phy_periodic_counters_v6_t;

typedef struct {
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS,
					* CTS, Control Management
					*  (includes retransmissions)
					*/
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble but not
					* the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS and
					* matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32	goodfcs;		/* Good fcs counters  */

	uint32	txctl;
	uint32	rxctl;
	uint32	txbar;
	uint32	rxbar;
	uint32	rxbeaconobss;

	uint32	txrts;
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	txackfrm;		/* number of ACK frames sent out */

	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txrtsfail;		/* number of rts transmission failure
					* that reach retry limit
					*/
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */

	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	rxback;			/* blockack rxcnt */
	uint32	txback;			/* blockack txcnt */
	uint32	rxctlucast;		/* number of received CNTRL frames
					* with good FCS and matching RA
					*/

	uint32	last_bcn_seq_num;	/* L_TSF and Seq # from the last beacon received */
	uint32	last_bcn_ltsf;		/* L_TSF and Seq # from the last beacon received */

	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	/* RX error related */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */

	uint32	rxframe;		/* Number of received frames */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  missbcn_dbg;		/* Number of beacon missed to receive */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
						* from a previous receive
						*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/

	uint16	counter_noise_request;	/* counters of requesting noise samples for noisecal */
	uint16	counter_noise_crsbit;	/* counters of noise mmt being interrupted
					* due to PHYCRS>
					*/
	uint16	counter_noise_apply;	/* counts of applying noisecal result */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint8	phylog_noise_mode;	/* noise cal mode */
	uint8	total_desense_on;	/* total desense on flag */
	uint8	initgain_desense;	/* initgain desense when total desense is on */
	uint8	crsmin_init;		/* crsmin_init threshold when total desense is on */
	uint8	lna1_gainlmt_desense;	/* lna1_gain limit desense when applying desense */
	uint8	clipgain_desense0;	/* clipgain desense when applying desense */

	uint8	lte_ofdm_desense;	/* ofdm desense dut to lte */
	uint8	lte_bphy_desense;	/* bphy desense due to lte */

	uint8	hw_aci_status;		/* HW ACI status flag */
	uint8	aci_clipgain_desense0;	/* clipgain desense due to aci */
	uint8	lna1_tbl_desense;	/* LNA1 table desense due to aci */

	int8	crsmin_high;		/* crsmin high when applying desense */
	int8	weakest_rssi;		/* weakest link RSSI */

	uint8	max_fp;
	uint8	crsminpwr_initgain;
	uint8	crsminpwr_clip1_high;
	uint8	crsminpwr_clip1_med;
	uint8	crsminpwr_clip1_lo;
	uint8	pad1;
	uint8	pad2;
} phy_periodic_counters_v7_t;

typedef struct phy_periodic_counters_v8 {
	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl;			/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  missbcn_dbg;		/* Number of beacon missed to receive */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;
} phy_periodic_counters_v8_t;

typedef struct phy_periodic_counters_v9 {
	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl;			/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_01;
	uint32  debug_02;
	uint32  debug_03;
	uint32  debug_04;
	uint32  debug_05;

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_06;
	uint16	debug_07;
	uint16	debug_08;
	uint16	debug_09;
	uint16	debug_10;

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8	debug_14;
} phy_periodic_counters_v9_t;

typedef struct phy_periodic_counters_v255 {
	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl;			/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_01;
	uint32  debug_02;
	uint32  debug_03;
	uint32  debug_04;
	uint32  debug_05;

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_06;
	uint16	debug_07;
	uint16	debug_08;
	uint16	debug_09;
	uint16	debug_10;

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8	debug_14;
} phy_periodic_counters_v255_t;

typedef struct phycal_log_cmn {
	uint16 chanspec; /* Current phy chanspec */
	uint8  last_cal_reason;  /* Last Cal Reason */
	uint8  pad1;  /* Padding byte to align with word */
	uint32  last_cal_time; /* Last cal time in sec */
} phycal_log_cmn_t;

typedef struct phycal_log_cmn_v2 {
	uint16 chanspec;	/* current phy chanspec */
	uint8  reason;		/* cal reason */
	uint8  phase;		/* cal phase */
	uint32 time;		/* time at which cal happened in sec */
	uint16 temp;		/* temperature at the time of cal */
	uint16 dur;		/* duration of cal in usec */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_01;	/* reused for slice info */
	uint16 debug_02;
	uint16 debug_03;
	uint16 debug_04;
} phycal_log_cmn_v2_t;

typedef struct phycal_log_core {
	uint16 ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16 ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16 ofdm_txd; /* contain di & dq */
	uint16 bphy_txa; /* BPHY Tx IQ Cal a coeff */
	uint16 bphy_txb; /* BPHY Tx IQ Cal b coeff */
	uint16 bphy_txd; /* contain di & dq */

	uint16 rxa; /* Rx IQ Cal A coeffecient */
	uint16 rxb; /* Rx IQ Cal B coeffecient */
	int32 rxs;  /* FDIQ Slope coeffecient */

	uint8 baseidx; /* TPC Base index */
	uint8 adc_coeff_cap0_adcI; /* ADC CAP Cal Cap0 I */
	uint8 adc_coeff_cap1_adcI; /* ADC CAP Cal Cap1 I */
	uint8 adc_coeff_cap2_adcI; /* ADC CAP Cal Cap2 I */
	uint8 adc_coeff_cap0_adcQ; /* ADC CAP Cal Cap0 Q */
	uint8 adc_coeff_cap1_adcQ; /* ADC CAP Cal Cap1 Q */
	uint8 adc_coeff_cap2_adcQ; /* ADC CAP Cal Cap2 Q */
	uint8 pad; /* Padding byte to align with word */
} phycal_log_core_t;

typedef struct phycal_log_core_v3 {
	uint16 ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16 ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16 ofdm_txd; /* contain di & dq */
	uint16 bphy_txa; /* BPHY Tx IQ Cal a coeff */
	uint16 bphy_txb; /* BPHY Tx IQ Cal b coeff */
	uint16 bphy_txd; /* contain di & dq */

	uint16 rxa; /* Rx IQ Cal A coeffecient */
	uint16 rxb; /* Rx IQ Cal B coeffecient */
	int32 rxs;  /* FDIQ Slope coeffecient */

	uint8 baseidx; /* TPC Base index */
	uint8 adc_coeff_cap0_adcI; /* ADC CAP Cal Cap0 I */
	uint8 adc_coeff_cap1_adcI; /* ADC CAP Cal Cap1 I */
	uint8 adc_coeff_cap2_adcI; /* ADC CAP Cal Cap2 I */
	uint8 adc_coeff_cap0_adcQ; /* ADC CAP Cal Cap0 Q */
	uint8 adc_coeff_cap1_adcQ; /* ADC CAP Cal Cap1 Q */
	uint8 adc_coeff_cap2_adcQ; /* ADC CAP Cal Cap2 Q */
	uint8 pad; /* Padding byte to align with word */

	/* Gain index based txiq ceffiecients for 2G(3 gain indices) */
	uint16 txiqlo_2g_a0; /* 2G TXIQ Cal a coeff for high TX gain */
	uint16 txiqlo_2g_b0; /* 2G TXIQ Cal b coeff for high TX gain */
	uint16 txiqlo_2g_a1; /* 2G TXIQ Cal a coeff for mid TX gain */
	uint16 txiqlo_2g_b1; /* 2G TXIQ Cal b coeff for mid TX gain */
	uint16 txiqlo_2g_a2; /* 2G TXIQ Cal a coeff for low TX gain */
	uint16 txiqlo_2g_b2; /* 2G TXIQ Cal b coeff for low TX gain */

	uint16	rxa_vpoff; /* Rx IQ Cal A coeff Vp off */
	uint16	rxb_vpoff; /* Rx IQ Cal B coeff Vp off */
	uint16	rxa_ipoff; /* Rx IQ Cal A coeff Ip off */
	uint16	rxb_ipoff; /* Rx IQ Cal B coeff Ip off */
	int32	rxs_vpoff; /* FDIQ Slope coeff Vp off */
	int32	rxs_ipoff; /* FDIQ Slope coeff Ip off */
} phycal_log_core_v3_t;

#define PHYCAL_LOG_VER1         (1u)

typedef struct phycal_log_v1 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Numbe of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phycal_log_cmn_t phycal_log_cmn; /* Logging common structure */
	/* This will be a variable length based on the numcores field defined above */
	phycal_log_core_t phycal_log_core[BCM_FLEX_ARRAY];
} phycal_log_v1_t;

typedef struct phy_periodic_log_cmn {
	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */
	uint16  featureflag; /* Currently active feature flags */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	uint32  nrate; /* Current Tx nrate */

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */

	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */
	uint8   pad1; /* Padding byte to align with word */
	uint8   pad2; /* Padding byte to align with word */

	uint32 duration;	/**< millisecs spent sampling this channel */
	uint32 congest_ibss;	/**< millisecs in our bss (presumably this traffic will */
				/**<  move if cur bss moves channels) */
	uint32 congest_obss;	/**< traffic not in our bss */
	uint32 interference;	/**< millisecs detecting a non 802.11 interferer. */

} phy_periodic_log_cmn_t;

typedef struct phy_periodic_log_cmn_v2 {
	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */
	uint16  featureflag; /* Currently active feature flags */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	uint32  nrate; /* Current Tx nrate */

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */

	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */

	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */

	uint8 slice;
	uint8 version;		/* version of fw/ucode for debug purposes */
	bool phycal_disable;		/* Set if calibration is disabled */
	uint8 pad;
	uint16 phy_log_counter;
	uint16 noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16 chan_switch_tm; /* Channel switch time */

	/* HP2P related params */
	uint16 shm_mpif_cnt_val;
	uint16 shm_thld_cnt_val;
	uint16 shm_nav_cnt_val;
	uint16 shm_cts_cnt_val;

	uint16 shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */
	uint32 last_cal_time;		/* Last cal execution time */
	uint16 deaf_count;		/* Depth of stay_in_carrier_search function */
	uint32 ed20_crs0;		/* ED-CRS status on core 0 */
	uint32 ed20_crs1;		/* ED-CRS status on core 1 */
	uint32 noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32 noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32 phywdg_ts;		/* Time-stamp when wd was fired */
	uint32 phywd_dur;			/* Duration of the watchdog */
	uint32 noise_mmt_abort_crs; /* Count of CRS during noise mmt */
	uint32 chanspec_set_ts;		/* Time-stamp when chanspec was set */
	uint32 vcopll_failure_cnt;	/* Number of VCO cal failures
					* (including failures detected in ucode).
					*/
	uint32 dcc_fail_counter;	/* Number of DC cal failures */
	uint32 log_ts;			/* Time-stamp when this log was collected */

	uint16 btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16 btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16  femtemp_read_fail_counter; /* Fem temparature read fail counter */
	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_01;
	uint16 debug_02;
} phy_periodic_log_cmn_v2_t;

typedef struct phy_periodic_log_cmn_v3 {
	uint32  nrate; /* Current Tx nrate */
	uint32	duration;	/**< millisecs spent sampling this channel */
	uint32	congest_ibss;	/**< millisecs in our bss (presumably this traffic will */
				/**<  move if cur bss moves channels) */
	uint32	congest_obss;	/**< traffic not in our bss */
	uint32	interference;	/**< millisecs detecting a non 802.11 interferer. */
	uint32  noise_cfg_exit1;
	uint32  noise_cfg_exit2;
	uint32  noise_cfg_exit3;
	uint32  noise_cfg_exit4;
	uint32	ed20_crs0;
	uint32	ed20_crs1;
	uint32	noise_cal_req_ts;
	uint32	noise_cal_crs_ts;
	uint32	log_ts;
	uint32	last_cal_time;
	uint32	phywdg_ts;
	uint32	chanspec_set_ts;
	uint32	noise_zero_inucode;
	uint32	phy_crs_during_noisemmt;
	uint32	wd_dur;

	int32	deaf_count;

	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */
	uint16  featureflag; /* Currently active feature flags */
	uint16	nav_cntr_l;
	uint16	nav_cntr_h;
	uint16	chanspec_set_last;
	uint16	ucode_noise_fb_overdue;
	uint16	phy_log_counter;
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_dc_cnt_val;
	uint16	shm_txff_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */
	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	bool	phycal_disable;
	uint8   pad; /* Padding byte to align with word */
} phy_periodic_log_cmn_v3_t;

typedef struct phy_periodic_log_cmn_v4 {
	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */

	uint16  featureflag; /* Currently active feature flags */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	uint32  nrate; /* Current Tx nrate */

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */

	uint8   slice;
	uint8   dbgfw_ver;	/* version of fw/ucode for debug purposes */
	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */

	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */

	/* HP2P related params */
	uint16 shm_mpif_cnt_val;
	uint16 shm_thld_cnt_val;
	uint16 shm_nav_cnt_val;
	uint16 shm_cts_cnt_val;

	uint16 shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */
	uint16 deaf_count;		/* Depth of stay_in_carrier_search function */
	uint32 last_cal_time;		/* Last cal execution time */
	uint32 ed20_crs0;		/* ED-CRS status on core 0: TODO change to uint16 */
	uint32 ed20_crs1;		/* ED-CRS status on core 1: TODO change to uint16 */
	uint32 noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32 noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32 phywdg_ts;		/* Time-stamp when wd was fired */
	uint32 phywd_dur;		/* Duration of the watchdog */
	uint32 noise_mmt_abort_crs;	/* Count of CRS during noise mmt: TODO change to uint16 */
	uint32 chanspec_set_ts;		/* Time-stamp when chanspec was set */
	uint32 vcopll_failure_cnt;	/* Number of VCO cal failures
					* (including failures detected in ucode).
					*/
	uint16 dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16 dcc_fail_counter;	/* Number of DC cal failures */
	uint32 log_ts;			/* Time-stamp when this log was collected */

	uint16 btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16 btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16 femtemp_read_fail_counter; /* Fem temparature read fail counter */
	uint16 phy_log_counter;
	uint16 noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16 chan_switch_tm;		/* Channel switch time */

	bool phycal_disable;		/* Set if calibration is disabled */

	/* dccal dcoe & idacc */
	uint8 dcc_err;			/* dccal health check error status */
	uint8 dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8 idacc_num_tries;		/* number of retries on idac cal */

	uint8 dccal_phyrxchain;		/* phy rxchain during dc calibration */
	uint8 dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */
	uint16 dcc_hcfail;		/* dcc health check failure count */
	uint16 dcc_calfail;		/* dcc failure count */

	uint16 noise_mmt_request_cnt;	/* Count of ucode noise cal request from phy */
	uint16 crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */
	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_03;
	uint16 debug_04;
	uint16 debug_05;
} phy_periodic_log_cmn_v4_t;

typedef struct phy_periodic_log_cmn_v5 {

	uint32 nrate;		/* Current Tx nrate */
	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */
	uint32 last_cal_time;	/* Last cal execution time */

	uint32 noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32 noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32 phywdg_ts;		/* Time-stamp when wd was fired */
	uint32 phywd_dur;		/* Duration of the watchdog */
	uint32 chanspec_set_ts;		/* Time-stamp when chanspec was set */
	uint32 vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32 log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32 cca_stats_total_glitch;
	uint32 cca_stats_bphy_glitch;
	uint32 cca_stats_total_badplcp;
	uint32 cca_stats_bphy_badplcp;
	uint32 cca_stats_mbsstime;

	uint32 counter_noise_request;	/* count of noisecal request */
	uint32 counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32 counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32 fullphycalcntr;		/* count of performing single phase cal */
	uint32 multiphasecalcntr;	/* count of performing multi-phase cal */

	uint16 chanspec;		/* Current phy chanspec */
	uint16 vbatmeas;		/* Measured VBAT sense value */
	uint16 featureflag;		/* Currently active feature flags */

	/* HP2P related params */
	uint16 shm_mpif_cnt_val;
	uint16 shm_thld_cnt_val;
	uint16 shm_nav_cnt_val;
	uint16 shm_cts_cnt_val;

	uint16 shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */
	uint16 deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16 ed20_crs0;		/* ED-CRS status on core 0 */
	uint16 ed20_crs1;		/* ED-CRS status on core 1 */

	uint16 dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16 dcc_fail_counter;	/* Number of DC cal failures */

	uint16 btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16 btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16 femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16 phy_log_counter;
	uint16 noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16 chan_switch_tm;		/* Channel switch time */

	uint16 dcc_hcfail;		/* dcc health check failure count */
	uint16 dcc_calfail;		/* dcc failure count */
	uint16 crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16 txpustatus;		/* txpu off definations */
	uint16 tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16 log_event_id;		/* reuse debug_01, logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_02;
	uint16 debug_03;
	uint16 debug_04;
	uint16 debug_05;

	int8 chiptemp;			/* Chip temparature */
	int8 femtemp;			/* Fem temparature */

	uint8 cal_phase_id;		/* Current Multi phase cal ID */
	uint8 rxchain;			/* Rx Chain */
	uint8 txchain;			/* Tx Chain */
	uint8 ofdm_desense;		/* OFDM desense */

	uint8 slice;
	uint8 dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8 bphy_desense;		/* BPHY desense */
	uint8 pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8 dcc_err;			/* dccal health check error status */
	uint8 dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8 idacc_num_tries;		/* number of retries on idac cal */

	uint8 dccal_phyrxchain;		/* phy rxchain during dc calibration */
	uint8 dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8 gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8 gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8 curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8 btcx_mode;			/* btcoex desense mode */
	int8 ltecx_mode;		/* lte coex desense mode */
	uint8 gbd_ofdm_desense;		/* gbd ofdm desense level */
	uint8 gbd_bphy_desense;		/* gbd bphy desense level */
	uint8 current_elna_bypass;	/* init gain desense: elna bypass */
	uint8 current_tia_idx;		/* init gain desense: tia index */
	uint8 current_lpf_idx;		/* init gain desense: lpf index */
	uint8 crs_auto_thresh;		/* crs auto threshold after desense */

	int8 weakest_rssi;		/* weakest link RSSI */
	uint8 noise_cal_mode;		/* noisecal mode */

	bool phycal_disable;		/* Set if calibration is disabled */
	bool hwpwrctrlen;		/* tx hwpwrctrl enable */
} phy_periodic_log_cmn_v5_t;

typedef struct phy_periodic_log_cmn_v6 {
	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/* move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */
	uint16	featureflag;		/* Currently active feature flags */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	measurehold_high;	/* PHY hold activities high16 */
	uint16	measurehold_low;	/* PHY hold activities low16 */
	uint16	btcx_ackpwroffset;	/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint16	debug_04;
	uint16	debug_05;

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */

	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	uint8	amtbitmap;		/* AMT status bitamp */
	uint8	pad1;			/* Padding byte to align with word */
	uint8	pad2;			/* Padding byte to align with word */
	uint8	pad3;			/* Padding byte to align with word */
} phy_periodic_log_cmn_v6_t;

typedef struct phy_periodic_log_cmn_v7 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	uint16	featureflag;		/* Currently active feature flags */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	pad1;			/* Padding byte to align with word */
	uint8	pad2;			/* Padding byte to align with word */
	uint8	pad3;			/* Padding byte to align with word */
} phy_periodic_log_cmn_v7_t;

typedef struct phy_periodic_log_cmn_v8 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	uint16	featureflag;		/* Currently active feature flags */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	measurehold_high;	/* PHY hold activities high16 */
	uint16	measurehold_low;	/* PHY hold activities low16 */
	uint16	tpc_hc_count;		/* RAM A HC counter */
	uint16	tpc_hc_bkoff;		/* RAM A HC core0 power backoff */
	uint16	btcx_ackpwroffset;	/* CoreMask (low8) and ack_pwr_offset (high8) */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	ed_threshold;		/* Threshold applied for ED */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */
	uint32	ed_duration;		/* ccastats: ed_duration */
} phy_periodic_log_cmn_v8_t;

/* Inherited from v8 */
typedef struct phy_periodic_log_cmn_v9 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	int8	ed_threshold0;		/* Threshold applied for ED core 0 */
	int8	ed_threshold1;		/* Threshold applied for ED core 1 */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	ed_threshold;		/* Threshold applied for ED */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */
	uint8	debug_05;
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint32	ed_duration;		/* ccastats: ed_duration */
} phy_periodic_log_cmn_v9_t;

/* Inherited from v9 */
typedef struct phy_periodic_log_cmn_v10 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	pktprocdebug;
	uint16	pktprocdebug2;
	uint16	timeoutstatus;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	ed_threshold;		/* Threshold applied for ED */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_pad01;
	uint16	gci_lst_pad02;
	uint16	gci_lst_pad03;
	uint16	gci_lst_pad04;
	uint16	gci_lst_pad05;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl1;		/* TX_PHYCTL1 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_invts;		/* When gci read request was placed and completed */
	uint16	gci_invtsdn;		/* */
	uint16	gci_invtxs;		/* timestamp for TXCRS assertion */
	uint16	gci_invtxdur;		/* IFS_TX_DUR */
	uint16	gci_invifs;		/* IFS status */
	uint16	gci_chan;		/* For additional ucode data */
	uint16	gci_pad02;		/* For additional ucode data */
	uint16	gci_pad03;		/* For additional ucode data */
	uint16	gci_pad04;		/* For additional ucode data */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	uint8	debug_08;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */
	uint32	debug_11;
	uint32	debug_12;
} phy_periodic_log_cmn_v10_t;

typedef struct phy_periodic_log_cmn_v11 {
	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	debug_15;		/* multipurpose debug counter */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */

	uint16	timeoutstatus;
	uint32	measurehold;		/* PHY hold activities */
	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_pad01;
	uint16	gci_lst_pad02;
	uint16	gci_lst_pad03;
	uint16	gci_lst_pad04;
	uint16	gci_lst_pad05;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;
	uint16	gci_pad01;		/* For additional ucode data */
	uint16	gci_pad02;		/* For additional ucode data */
	uint16	gci_pad03;		/* For additional ucode data */
	uint16	gci_pad04;		/* For additional ucode data */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	uint8	noise_cal_mode;		/* noisecal mode */

	/* Misc general purpose debug counters (will be used for future debugging) */
	int8	ed_threshold0;		/* Threshold applied for ED core 0 */
	int8	ed_threshold1;		/* Threshold applied for ED core 1 */
	uint8	debug_08;
	uint8	debug_09;

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_10;
	uint32	debug_11;
	uint32	debug_12;
	uint32	debug_13;
	uint32	debug_14;
} phy_periodic_log_cmn_v11_t;

typedef struct phy_periodic_log_cmn_v255 {
	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	phylog_noise_mode;	/* Noise mode used */

	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */

	uint16	timeoutstatus;
	uint32	measurehold;		/* PHY hold activities */
	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_pad01;
	uint16	gci_lst_pad02;
	uint16	gci_lst_pad03;
	uint16	gci_lst_pad04;
	uint16	gci_lst_pad05;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;
	uint16	gci_pad01;		/* For additional ucode data */
	uint16	gci_pad02;		/* For additional ucode data */
	uint16	gci_pad03;		/* For additional ucode data */
	uint16	gci_pad04;		/* For additional ucode data */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	noise_cal_mode;		/* noisecal mode */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;
	uint8	debug_09;
	uint8	debug_10;

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_11;
	uint32	debug_12;
	uint32	debug_13;
	uint32	debug_14;
} phy_periodic_log_cmn_v255_t;

typedef struct phy_periodic_log_core {
	uint8	baseindxval; /* TPC Base index */
	int8	tgt_pwr; /* Programmed Target power */
	int8	estpwradj; /* Current Est Power Adjust value */
	int8	crsmin_pwr; /* CRS Min/Noise power */
	int8	rssi_per_ant; /* RSSI Per antenna */
	int8	snr_per_ant; /* SNR Per antenna */
	int8	pad1; /* Padding byte to align with word */
	int8	pad2; /* Padding byte to align with word */
} phy_periodic_log_core_t;

typedef struct phy_periodic_log_core_v3 {
	uint8	baseindxval; /* TPC Base index */
	int8	tgt_pwr; /* Programmed Target power */
	int8	estpwradj; /* Current Est Power Adjust value */
	int8	crsmin_pwr; /* CRS Min/Noise power */
	int8	rssi_per_ant; /* RSSI Per antenna */
	int8	snr_per_ant; /* SNR Per antenna */

	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	uint8	pktproc;	/* pktproc read during dccal health check */

	int8	noise_level;	/* noise level = crsmin_pwr if gdb desense is lesser */
	int8	pad2; /* Padding byte to align with word */
	int8	pad3; /* Padding byte to align with word */
} phy_periodic_log_core_v3_t;

typedef struct phy_periodic_log_core_v4 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint16	debug_01;	/* multipurpose debug register */
	uint16	debug_02;	/* multipurpose debug register */
	uint16	debug_03;	/* multipurpose debug register */
	uint16	debug_04;	/* multipurpose debug register */

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
} phy_periodic_log_core_v4_t;

typedef struct phy_periodic_log_core_v5 {
	uint8	baseindxval;			/* TPC Base index */
	int8	tgt_pwr;			/* Programmed Target power */
	int8	estpwradj;			/* Current Est Power Adjust value */
	int8	crsmin_pwr;			/* CRS Min/Noise power */
	int8	rssi_per_ant;			/* RSSI Per antenna */
	int8	snr_per_ant;			/* SNR Per antenna */
	uint8	fp;
	int8	phylog_noise_pwr_array[8];	/* noise buffer array */
	int8	noise_dbm_ant;			/* from uCode shm read, afer converting to dBm */
} phy_periodic_log_core_v5_t;

#define PHY_NOISE_PWR_ARRAY_SIZE	(8u)
typedef struct phy_periodic_log_core_v6 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint16	curr_tssival;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	tpc_hc_tssi;	/* RAM A HC TSSI value */
	uint16	btcx_antmask;	/* antenna to be used by BT */

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */
	int8	tpc_hc_bidx;	/*  RAM A HC base index */

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v6_t;

typedef struct phy_periodic_log_core_v7 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */
	uint8	pad01;

	uint16	curr_tssival;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_05;
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v7_t;

typedef struct phy_periodic_log_core_v255 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */
	int8	ed_threshold;	/* ed threshold */

	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	curr_tssival;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;
	uint16	debug_05;	/* multipurpose debug register */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;
	uint8	debug_09;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v255_t;

typedef struct phy_periodic_log_core_v2 {
	int32 rxs; /* FDIQ Slope coeffecient */

	uint16	ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16	ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16	ofdm_txd; /* contain di & dq */
	uint16	rxa; /* Rx IQ Cal A coeffecient */
	uint16	rxb; /* Rx IQ Cal B coeffecient */
	uint16	baseidx; /* TPC Base index */

	uint8	baseindxval; /* TPC Base index */

	int8	tgt_pwr; /* Programmed Target power */
	int8	estpwradj; /* Current Est Power Adjust value */
	int8	crsmin_pwr; /* CRS Min/Noise power */
	int8	rssi_per_ant; /* RSSI Per antenna */
	int8	snr_per_ant; /* SNR Per antenna */
	int8	pad1; /* Padding byte to align with word */
	int8	pad2; /* Padding byte to align with word */
} phy_periodic_log_core_v2_t;

/* Shared BTCX Statistics for BTCX and PHY Logging, storing the accumulated values. */
#define BTCX_STATS_PHY_LOGGING_VER 1
typedef struct wlc_btc_shared_stats {
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;	/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
} wlc_btc_shared_stats_v1_t;

#define BTCX_STATS_PHY_LOGGING_VER2 2u
typedef struct wlc_btc_shared_stats_v2 {
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;	/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
	uint16 ackpwroffset;		/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8 prisel_ant_mask;		/* antenna to be used by BT */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;
	uint8	debug_09;
} wlc_btc_shared_stats_v2_t;

#define BTCX_STATS_PHY_LOGGING_VER255 255u
typedef struct wlc_btc_shared_stats_v255 {
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;	/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
	uint16 ackpwroffset;		/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8 prisel_ant_mask;		/* antenna to be used by BT */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;
	uint8	debug_09;
} wlc_btc_shared_stats_v255_t;

/* BTCX Statistics for PHY Logging */
typedef struct wlc_btc_stats_phy_logging {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	wlc_btc_shared_stats_v1_t shared;
} phy_periodic_btc_stats_v1_t;

/* BTCX Statistics for PHY Logging */
typedef struct wlc_btc_stats_phy_logging_v2 {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	wlc_btc_shared_stats_v2_t shared;
} phy_periodic_btc_stats_v2_t;

/* BTCX Statistics for PHY Logging */
typedef struct wlc_btc_stats_phy_logging_v255 {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	wlc_btc_shared_stats_v255_t shared;
} phy_periodic_btc_stats_v255_t;

/* OBSS Statistics for PHY Logging */
typedef struct phy_periodic_obss_stats_v1 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_mit_mode;				/* mitigation mode */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	dynbw_init_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * initiator (txrts+rxcts)
							 */
	uint16	dynbw_resp_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * responder (rxrts+txcts)
							 */
	uint16	dynbw_rxdata_reducebw_cnt;		/*
							 * rx data cnt with reduced bandwidth
							 * as txcts requested
							 */
} phy_periodic_obss_stats_v1_t;

/* OBSS Statistics for PHY Logging */
typedef struct phy_periodic_obss_stats_v2 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_need_updt;				/* BW update needed flag */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	dynbw_init_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * initiator (txrts+rxcts)
							 */
	uint16	dynbw_resp_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * responder (rxrts+txcts)
							 */
	uint16	dynbw_rxdata_reducebw_cnt;		/*
							 * rx data cnt with reduced bandwidth
							 * as txcts requested
							 */
	uint16	obss_mmt_skip_cnt;			/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;			/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;			/* obss reg mismatch between ucode and fw */
	uint8	obss_final_rec_bw;			/* final recommended bw to wlc-Sent to SW */
	uint8	debug01;
	uint16	obss_det_stats[ACPHY_OBSS_SUBBAND_CNT];
} phy_periodic_obss_stats_v2_t;

typedef struct phy_periodic_obss_stats_v3 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_need_updt;				/* BW update needed flag */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	dynbw_init_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * initiator (txrts+rxcts)
							 */
	uint16	dynbw_resp_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * responder (rxrts+txcts)
							 */
	uint16	dynbw_rxdata_reducebw_cnt;		/*
							 * rx data cnt with reduced bandwidth
							 * as txcts requested
							 */
	uint16	obss_mmt_skip_cnt;			/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;			/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;			/* obss reg mismatch between ucode and fw */
	uint8	obss_last_rec_bw;			/* last recommended bw to wlc-Sent to SW */
	uint8	debug_01;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;
} phy_periodic_obss_stats_v3_t;

typedef struct phy_periodic_obss_stats_v255 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_need_updt;				/* BW update needed flag */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	dynbw_init_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * initiator (txrts+rxcts)
							 */
	uint16	dynbw_resp_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * responder (rxrts+txcts)
							 */
	uint16	dynbw_rxdata_reducebw_cnt;		/*
							 * rx data cnt with reduced bandwidth
							 * as txcts requested
							 */
	uint16	obss_mmt_skip_cnt;			/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;			/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;			/* obss reg mismatch between ucode and fw */
	uint8	obss_last_rec_bw;			/* last recommended bw to wlc-Sent to SW */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;
} phy_periodic_obss_stats_v255_t;

/* SmartCCA related PHY Logging */
typedef struct phy_periodic_scca_stats_v1 {
	uint32 asym_intf_cmplx_pwr[2];
	uint32 asym_intf_ncal_time;
	uint32 asym_intf_host_req_mit_turnon_time;
	uint32 core1_smask_val_bk;	/* bt fem control related */
	int32 asym_intf_ed_thresh;

	int16 crsminpoweru0;			/* crsmin thresh */
	int16 crsminpoweroffset0;		/* ac_offset core0 */
	int16 crsminpoweroffset1;		/* ac_offset core1 */
	int16 Core0InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	int16 Core1InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	int16 ed_crsEn;				/* phyreg(ed_crsEn) */
	int16 nvcfg0;				/* LLR deweighting coefficient */
	int16 SlnaRxMaskCtrl0;
	int16 SlnaRxMaskCtrl1;
	uint16 save_SlnaRxMaskCtrl0;
	uint16 save_SlnaRxMaskCtrl1;
	uint16 asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;
	* b[1]:   bool asym_intf_tx_smartcca_on;
	* b[3:2]: bool asym_intf_elna_bypass[2];
	* b[4]:   bool asym_intf_valid_noise_samp;
	* b[5]:   bool asym_intf_fill_noise_buf;
	* b[6]:   bool asym_intf_ncal_discard;
	* b[7]:   bool slna_reg_saved;
	* b[8]:   bool asym_intf_host_ext_usb;		//Host control related variable
	* b[9]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[10]:  bool asym_intf_host_en;		// Host control related variable
	* b[11]:  bool asym_intf_host_enable;
	* b[12]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16 asym_intf_stats;

	uint8 elna_bypass;	/* from bt_desense.elna_bypass in gainovr_shm_config() */
	uint8 btc_mode;		/* from bt_desense in gainovr_shm_config() */
	/* noise at antenna from phy_ac_noise_ant_noise_calc() */
	int8 noise_dbm_ant[2];
	/* in phy_ac_noise_calc(), also used by wl noise */
	int8 noisecalc_cmplx_pwr_dbm[2];
	uint8 gain_applied;		/* from phy_ac_rxgcrs_set_init_clip_gain() */

	int8 asym_intf_tx_smartcca_cm;
	int8 asym_intf_rx_noise_mit_cm;
	int8 asym_intf_avg_noise[2];
	int8 asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8 asym_intf_prev_noise_lvl[2];
	uint8 asym_intf_noise_calc_gain_2g[2];
	uint8 asym_intf_noise_calc_gain_5g[2];
	uint8 asym_intf_ant_noise_idx;
	uint8 asym_intf_least_core_idx;
	uint8 phy_crs_thresh_save[2];
	uint8 asym_intf_initg_desense[2];
	uint8 asym_intf_pending_host_req_type;	/* Set request pending if clk not present */
} phy_periodic_scca_stats_v1_t;

/* SmartCCA related PHY Logging */
typedef struct phy_periodic_scca_stats_v2 {
	uint32 asym_intf_cmplx_pwr[2];
	uint32 asym_intf_ncal_time;
	uint32 asym_intf_host_req_mit_turnon_time;
	uint32 core1_smask_val_bk;	/* bt fem control related */
	int32 asym_intf_ed_thresh;

	int16 crsminpoweru0;			/* crsmin thresh */
	int16 crsminpoweroffset0;		/* ac_offset core0 */
	int16 crsminpoweroffset1;		/* ac_offset core1 */
	int16 Core0InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	int16 Core1InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	int16 ed_crsEn;				/* phyreg(ed_crsEn) */
	int16 nvcfg0;				/* LLR deweighting coefficient */
	int16 SlnaRxMaskCtrl0;
	int16 SlnaRxMaskCtrl1;
	uint16 save_SlnaRxMaskCtrl0;
	uint16 save_SlnaRxMaskCtrl1;
	uint16 asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;
	* b[1]:   bool asym_intf_tx_smartcca_on;
	* b[3:2]: bool asym_intf_elna_bypass[2];
	* b[4]:   bool asym_intf_valid_noise_samp;
	* b[5]:   bool asym_intf_fill_noise_buf;
	* b[6]:   bool asym_intf_ncal_discard;
	* b[7]:   bool slna_reg_saved;
	* b[8]:   bool asym_intf_host_ext_usb;		//Host control related variable
	* b[9]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[10]:  bool asym_intf_host_en;		// Host control related variable
	* b[11]:  bool asym_intf_host_enable;
	* b[12]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16 asym_intf_stats;

	uint8 elna_bypass;	/* from bt_desense.elna_bypass in gainovr_shm_config() */
	uint8 btc_mode;		/* from bt_desense in gainovr_shm_config() */
	/* noise at antenna from phy_ac_noise_ant_noise_calc() */
	int8 noise_dbm_ant[2];
	/* in phy_ac_noise_calc(), also used by wl noise */
	int8 noisecalc_cmplx_pwr_dbm[2];
	uint8 gain_applied;		/* from phy_ac_rxgcrs_set_init_clip_gain() */

	int8 asym_intf_tx_smartcca_cm;
	int8 asym_intf_rx_noise_mit_cm;
	int8 asym_intf_avg_noise[2];
	int8 asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8 asym_intf_prev_noise_lvl[2];
	uint8 asym_intf_noise_calc_gain_2g[2];
	uint8 asym_intf_noise_calc_gain_5g[2];
	uint8 asym_intf_ant_noise_idx;
	uint8 asym_intf_least_core_idx;
	uint8 phy_crs_thresh_save[2];
	uint8 asym_intf_initg_desense[2];
	uint8 asym_intf_pending_host_req_type;	/* Set request pending if clk not present */
	uint16	asym_intf_ncal_crs_stat[12];
	uint8	asym_intf_ncal_crs_stat_idx;
} phy_periodic_scca_stats_v2_t;

/* SmartCCA related PHY Logging - v3 NEWT SmartCCA based */
typedef struct phy_periodic_scca_stats_v3 {
	uint32	asym_intf_ncal_time;
	uint32	asym_intf_host_req_mit_turnon_time;
	int32	asym_intf_ed_thresh;

	int16	crsminpoweru0;			/* crsmin thresh */
	int16	crsminpoweroffset0;		/* ac_offset core0 */
	int16	crsminpoweroffset1;		/* ac_offset core1 */
	int16	ed_crsEn;			/* phyreg(ed_crsEn) */
	int16	nvcfg0;				/* LLR deweighting coefficient */
	int16	SlnaRxMaskCtrl0;
	int16	SlnaRxMaskCtrl1;
	int16	CRSMiscellaneousParam;
	int16	AntDivConfig2059;
	int16	HPFBWovrdigictrl;
	uint16	save_SlnaRxMaskCtrl0;
	uint16	save_SlnaRxMaskCtrl1;
	uint16	asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;	// SmartCCA Rx mititagion enabled
	* b[1]:   bool asym_intf_tx_smartcca_on;	// SmartCCA Tx mititagion enabled
	* b[2]:   bool asym_intf_valid_noise_samp;	// Latest noise sample is valid
	* b[3]:   bool asym_intf_fill_noise_buf;	// Fill the same sample to entire buffer
	* b[4]:   bool asym_intf_ncal_discard;		// Discard current noise sample
	* b[5]:   bool slna_reg_saved;			// SLNA register values are saved
	* b[6]:   bool asym_intf_host_ext_usb;		// Host control related variable
	* b[7]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[8]:   bool asym_intf_host_en;		// Host control related variable
	* b[9]:   bool asym_intf_host_enable;		// Host control related variable
	* b[10]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16	asym_intf_stats;

	uint8	btc_mode;			/* from bt_desense in gainovr_shm_config() */

	/* noise at antenna from phy_ac_noise_calc() */
	int8	noisecalc_cmplx_pwr_dbm[2];
	int8	asym_intf_ant_noise[2];

	int8	asym_intf_tx_smartcca_cm;
	int8	asym_intf_rx_noise_mit_cm;
	int8	asym_intf_avg_noise[2];
	int8	asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8	asym_intf_prev_noise_lvl[2];
	uint8	asym_intf_ant_noise_idx;
	uint8	asym_intf_least_core_idx;
	uint8	asym_intf_pending_host_req_type;
						/* Set request pending if clk not present */
	uint16	asym_intf_ncal_crs_stat;
	uint8	asym_intf_ncal_crs_stat_idx;
	uint8	pad;
} phy_periodic_scca_stats_v3_t;

/* SmartCCA related PHY Logging */
typedef struct phy_periodic_scca_stats_v255 {
	uint32 asym_intf_ncal_time;
	uint32 asym_intf_host_req_mit_turnon_time;
	int32 asym_intf_ed_thresh;

	int16 crsminpoweru0;			/* crsmin thresh */
	int16 crsminpoweroffset0;		/* ac_offset core0 */
	int16 crsminpoweroffset1;		/* ac_offset core1 */
	int16 ed_crsEn;				/* phyreg(ed_crsEn) */
	int16 nvcfg0;				/* LLR deweighting coefficient */
	int16 SlnaRxMaskCtrl0;
	int16 SlnaRxMaskCtrl1;
	int16	CRSMiscellaneousParam;
	int16	AntDivConfig2059;
	int16	HPFBWovrdigictrl;
	uint16 save_SlnaRxMaskCtrl0;
	uint16 save_SlnaRxMaskCtrl1;
	uint16 asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;	// SmartCCA Rx mititagion enabled
	* b[1]:   bool asym_intf_tx_smartcca_on;	// SmartCCA Tx mititagion enabled
	* b[2]:   bool asym_intf_valid_noise_samp;	// Latest noise sample is valid
	* b[3]:   bool asym_intf_fill_noise_buf;	// Fill the same sample to entire buffer
	* b[4]:   bool asym_intf_ncal_discard;		// Discard current noise sample
	* b[5]:   bool slna_reg_saved;			// SLNA register values are saved
	* b[6]:   bool asym_intf_host_ext_usb;		// Host control related variable
	* b[7]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[8]:   bool asym_intf_host_en;		// Host control related variable
	* b[9]:   bool asym_intf_host_enable;		// Host control related variable
	* b[10]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16 asym_intf_stats;

	uint8 btc_mode;		/* from bt_desense in gainovr_shm_config() */

	/* noise at antenna from phy_ac_noise_calc() */
	int8 noisecalc_cmplx_pwr_dbm[2];
	int8	asym_intf_ant_noise[2];

	int8 asym_intf_tx_smartcca_cm;
	int8 asym_intf_rx_noise_mit_cm;
	int8 asym_intf_avg_noise[2];
	int8 asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8 asym_intf_prev_noise_lvl[2];
	uint8 asym_intf_ant_noise_idx;
	uint8 asym_intf_least_core_idx;
	uint8	asym_intf_pending_host_req_type;
						/* Set request pending if clk not present */
	uint16	asym_intf_ncal_crs_stat;
	uint8	asym_intf_ncal_crs_stat_idx;
	uint8	pad;
} phy_periodic_scca_stats_v255_t;

#define PHY_PERIODIC_LOG_VER1         (1u)

typedef struct phy_periodic_log_v1 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_periodic_log_cmn_t phy_perilog_cmn;
	phy_periodic_counters_v1_t counters_peri_log;
	/* This will be a variable length based on the numcores field defined above */
	phy_periodic_log_core_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v1_t;

#define PHYCAL_LOG_VER3		(3u)
#define PHY_PERIODIC_LOG_VER3	(3u)

/* 4387 onwards */
typedef struct phy_periodic_log_v3 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v2_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v3_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v3_t;

#define PHY_PERIODIC_LOG_VER5	(5u)

typedef struct phy_periodic_log_v5 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v4_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v3_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v3_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v5_t;

#define PHY_PERIODIC_LOG_VER6	(6u)

typedef struct phy_periodic_log_v6 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v5_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v6_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v4_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v6_t;

typedef struct phycal_log_v3 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phycal_log_cmn_v2_t phycal_log_cmn; /* Logging common structure */
	/* This will be a variable length based on the numcores field defined above */
	phycal_log_core_v3_t phycal_log_core[BCM_FLEX_ARRAY];
} phycal_log_v3_t;

#define PHY_CAL_EVENTLOG_VER4		(4u)
typedef struct phycal_log_v4 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_phycal_v2_t phy_calibration;
} phycal_log_v4_t;

/* Trunk ONLY */
#define PHY_CAL_EVENTLOG_VER255		(255u)
typedef struct phycal_log_v255 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_phycal_v255_t phy_calibration;
} phycal_log_v255_t;

/* Note: The version 2 is reserved for 4357 only. Future chips must not use this version. */

#define MAX_CORE_4357		(2u)
#define PHYCAL_LOG_VER2		(2u)
#define PHY_PERIODIC_LOG_VER2	(2u)

typedef struct {
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
} phy_periodic_counters_v2_t;

/* Note: The version 2 is reserved for 4357 only. All future chips must not use this version. */

typedef struct phycal_log_core_v2 {
	uint16 ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16 ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16 ofdm_txd; /* contain di & dq */
	uint16 rxa; /* Rx IQ Cal A coeffecient */
	uint16 rxb; /* Rx IQ Cal B coeffecient */
	uint8 baseidx; /* TPC Base index */
	uint8 pad;
	int32 rxs; /* FDIQ Slope coeffecient */
} phycal_log_core_v2_t;

/* Note: The version 2 is reserved for 4357 only. All future chips must not use this version. */

typedef struct phycal_log_v2 {
	uint8  version; /* Logging structure version */
	uint16 length;  /* Length of the entire structure */
	uint8 pad;
	phycal_log_cmn_t phycal_log_cmn; /* Logging common structure */
	phycal_log_core_v2_t phycal_log_core[MAX_CORE_4357];
} phycal_log_v2_t;

/* Note: The version 2 is reserved for 4357 only. All future chips must not use this version. */

typedef struct phy_periodic_log_v2 {
	uint8  version; /* Logging structure version */
	uint16 length;  /* Length of the entire structure */
	uint8 pad;
	phy_periodic_log_cmn_t phy_perilog_cmn;
	phy_periodic_counters_v2_t counters_peri_log;
	phy_periodic_log_core_t phy_perilog_core[MAX_CORE_4357];
} phy_periodic_log_v2_t;

#define PHY_PERIODIC_LOG_VER4	(4u)

/*
 * Note: The version 4 is reserved for 4357 Deafness Debug only.
 * All future chips must not use this version.
 */
typedef struct phy_periodic_log_v4 {
	uint8  version; /* Logging structure version */
	uint8  pad;
	uint16 length;  /* Length of the entire structure */
	phy_periodic_log_cmn_v3_t  phy_perilog_cmn;
	phy_periodic_counters_v4_t counters_peri_log;
	phy_periodic_log_core_v2_t phy_perilog_core[MAX_CORE_4357];
} phy_periodic_log_v4_t;

#define PHY_PERIODIC_LOG_VER7		(7u)
typedef struct phy_periodic_log_v7 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the entire structure */
	phy_periodic_log_cmn_t phy_perilog_cmn;
	phy_periodic_counters_v5_t counters_peri_log;
	phy_periodic_btc_stats_v1_t btc_stats_peri_log;
	/* This will be a variable length based on the numcores field defined above */
	phy_periodic_log_core_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v7_t;

#define PHY_PERIODIC_LOG_VER8		(8u)
typedef struct phy_periodic_log_v8 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the entire structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v6_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v7_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t btc_stats_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v5_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v8_t;

#define PHY_PERIODIC_LOG_VER9	(9u)
typedef struct phy_periodic_log_v9 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v7_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v4_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v9_t;

#define PHY_PERIODIC_LOG_VER10	10u
typedef struct phy_periodic_log_v10 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the entire structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v6_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v7_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t btc_stats_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v5_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v1_t scca_counters_peri_log;
} phy_periodic_log_v10_t;

#define PHY_PERIODIC_LOG_VER11	(11u)
typedef struct phy_periodic_log_v11 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v8_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v6_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v11_t;

#define AMT_MATCH_INFRA_BSSID	(1 << 0)
#define AMT_MATCH_INFRA_MYMAC	(1 << 1)

#define PHY_PERIODIC_LOG_VER20	20u
typedef struct phy_periodic_log_v20 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v9_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v6_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v2_t scca_counters_peri_log;
} phy_periodic_log_v20_t;

#define PHY_PERIODIC_LOG_VER21	21u
typedef struct phy_periodic_log_v21 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v9_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v6_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v2_t scca_counters_peri_log;
} phy_periodic_log_v21_t;

#define PHY_PERIODIC_LOG_VER22	22u
typedef struct phy_periodic_log_v22 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v10_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v2_t scca_counters_peri_log;
} phy_periodic_log_v22_t;

#define PHY_PERIODIC_LOG_VER23	23u
typedef struct phy_periodic_log_v23 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v10_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v23_t;

#define PHY_PERIODIC_LOG_VER24	24u
typedef struct phy_periodic_log_v24 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v10_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v2_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v24_t;

#define PHY_PERIODIC_LOG_VER25	25u
typedef struct phy_periodic_log_v25 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v11_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v3_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v25_t;

/* ************************************************** */
/* The version 255 for the logging data structures    */
/* is for use in trunk ONLY. In release branches the  */
/* next available version for the particular data     */
/* structure should be used.                          */
/* ************************************************** */

#define PHY_PERIODIC_LOG_VER255	255u
typedef struct phy_periodic_log_v255 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v255_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v255_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v255_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v255_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v255_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v255_t scca_counters_peri_log;
} phy_periodic_log_v255_t;
#endif /* _PHY_EVENT_LOG_PAYLOAD_H_ */
