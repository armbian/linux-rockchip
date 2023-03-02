/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/
#include "dle.h"

/* PCIE 64 */
static struct dle_size_t wde_size0 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	4095, /* lnk_pge_num */
	1, /* unlnk_pge_num */
};

/* SDIO, PCIE STF, USB */
static struct dle_size_t wde_size1 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	768, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* PCIE 128 */
static struct dle_size_t wde_size2 = {
	MAC_AX_WDE_PG_128, /* pge_size */
	2016, /* lnk_pge_num */
	32, /* unlnk_pge_num */
};

/* PCIE SU TP */
static struct dle_size_t wde_size3 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	496, /* lnk_pge_num */
	3600, /* unlnk_pge_num */
};

/* DLFW */
static struct dle_size_t wde_size4 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	0, /* lnk_pge_num */
	4096, /* unlnk_pge_num */
};

/* PCIE BCN TEST */
static struct dle_size_t wde_size5 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	3904, /* lnk_pge_num */
	64, /* unlnk_pge_num */
};

/* PCIE 64 */
static struct dle_size_t wde_size6 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	512, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* PCIE 128 */
static struct dle_size_t wde_size7 = {
	MAC_AX_WDE_PG_128, /* pge_size */
	960, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* PCIE STF, USB */
static struct dle_size_t wde_size8 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	256, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* DLFW */
static struct dle_size_t wde_size9 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	0, /* lnk_pge_num */
	1024, /* unlnk_pge_num */
};

/* LA-PCIE */
static struct dle_size_t wde_size10 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	1408, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* LA-PCIE */
static struct dle_size_t wde_size11 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	256, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* LA-SDIO */
static struct dle_size_t wde_size12 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	328, /* lnk_pge_num */
	56, /* unlnk_pge_num */
};

/* SDIO SCC */
static struct dle_size_t wde_size13 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	128, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* SDIO LA */
static struct dle_size_t wde_size14 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	124, /* lnk_pge_num */
	4, /* unlnk_pge_num */
};

/* LA-USB 8852A*/
static struct dle_size_t wde_size15 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	384, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* LA-USB 8852B*/
static struct dle_size_t wde_size16 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	124, /* lnk_pge_num */
	4, /* unlnk_pge_num */
};

/* 8852C USB */
static struct dle_size_t wde_size17 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	332, /* lnk_pge_num */
	52, /* unlnk_pge_num */
};

/* 8852C DLFW */
static struct dle_size_t wde_size18 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	0, /* lnk_pge_num */
	2048, /* unlnk_pge_num */
};

/* 8852C PCIE SCC */
static struct dle_size_t wde_size19 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	3328, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* 8852C PCIE DBCC */
static struct dle_size_t wde_size20 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	3328, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* 8852C PCIE SCC/DBCC STF */
static struct dle_size_t wde_size21 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	256, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* 8852C PCIE LA */
static struct dle_size_t wde_size22 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	3224, /* lnk_pge_num */
	104, /* unlnk_pge_num */
};

/* PCIE SCC TURBO */
static struct dle_size_t wde_size23 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	1024, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* 8852B USB CABV*/
static struct dle_size_t wde_size24 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	216, /* lnk_pge_num */
	40, /* unlnk_pge_num */
};

/* 8852B USB2.0/USB3.0 SCC*/
static struct dle_size_t wde_size25 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	162, /* lnk_pge_num */
	94, /* unlnk_pge_num */
};

/* 8852AU ccv*/
static struct dle_size_t wde_size26 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	760, /* lnk_pge_num */
	8, /* unlnk_pge_num */
};

/* 8852B USB2.0/USB3.0 SCC TURBO*/
static struct dle_size_t wde_size27 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	220, /* lnk_pge_num */
	36, /* unlnk_pge_num */
};

/* 8852C USB*/
static struct dle_size_t wde_size28 = {
	MAC_AX_WDE_PG_64, /* pge_size */
	384, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* PCIE */
static struct dle_size_t ple_size0 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1520, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* SDIO, USB */
static struct dle_size_t ple_size1 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	3184, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* PCIE STF */
static struct dle_size_t ple_size2 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	3184, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* PCIE SU TP */
static struct dle_size_t ple_size3 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	330, /* lnk_pge_num */
	1206, /* unlnk_pge_num */
};

/* DLFW */
static struct dle_size_t ple_size4 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	64, /* lnk_pge_num */
	1472, /* unlnk_pge_num */
};

/* PCIE BCN TEST */
static struct dle_size_t ple_size5 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1520, /* lnk_pge_num */
	80, /* unlnk_pge_num */
};

/* PCIE 64 */
static struct dle_size_t ple_size6 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	496, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* PCIE STF, USB */
static struct dle_size_t ple_size7 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1392, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* DLFW */
static struct dle_size_t ple_size8 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	64, /* lnk_pge_num */
	960, /* unlnk_pge_num */
};

/* PCIE 128 */
static struct dle_size_t ple_size9 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	576, /* lnk_pge_num */
	0, /* unlnk_pge_num */
};

/* LA-PCIE 8852A*/
static struct dle_size_t ple_size10 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	816, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* LA-PCIE */
static struct dle_size_t ple_size11 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	368, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* LA-SDIO 8852A*/
static struct dle_size_t ple_size12 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1328, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* SDIO SCC */
static struct dle_size_t ple_size13 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	688, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* SDIO LA */
static struct dle_size_t ple_size14 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	432, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* LA-USB 8852A */
static struct dle_size_t ple_size15 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1328, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* LA-USB 8852B */
static struct dle_size_t ple_size16 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	432, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* 8852C USB */
static struct dle_size_t ple_size17 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	3336, /* lnk_pge_num */
	56, /* unlnk_pge_num */
};

/* 8852C DLFW*/
static struct dle_size_t ple_size18 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	2544, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* 8852C PCIE SCC */
static struct dle_size_t ple_size19 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1904, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* 8852C PCIE DBCC */
static struct dle_size_t ple_size20 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1904, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* 8852C PCIE SCC/DBCC STF */
static struct dle_size_t ple_size21 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	3440, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* 8852C PCIE LA */
static struct dle_size_t ple_size22 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1904, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* 8852B PCIE TURBO AB */
static struct dle_size_t ple_size23 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	496, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* PCIE SCC TURBO */
static struct dle_size_t ple_size24 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1008, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/* 8852B USB2.0 SCC*/
static struct dle_size_t ple_size27 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	620, /* lnk_pge_num */
	20, /* unlnk_pge_num */
};

/* 8852B USB2.0 TURBO*/
static struct dle_size_t ple_size28 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1396, /* lnk_pge_num */
	12, /* unlnk_pge_num */
};

/* SDIO SCC TURBO */
static struct dle_size_t ple_size29 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1456, /* lnk_pge_num */
	16, /* unlnk_pge_num */
};

/*8852B  USB 3.0 */
static struct dle_size_t ple_size30 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	632, /* lnk_pge_num */
	8, /* unlnk_pge_num */
};

/* 8852B USB3.0 TURBO */
static struct dle_size_t ple_size31 = {
	MAC_AX_PLE_PG_128, /* pge_size */
	1386, /* lnk_pge_num */
	22, /* unlnk_pge_num */
};

/* PCIE 64 */
static struct wde_quota_t wde_qt0 = {
	3792, /* hif */
	196, /* wcpu */
	0, /* pkt_in */
	107, /* cpu_io */
};

/* SDIO, PCIE STF, USB */
static struct wde_quota_t wde_qt1 = {
	512, /* hif */
	196, /* wcpu */
	0, /* pkt_in */
	60, /* cpu_io */
};

/* PCIE 128 */
static struct wde_quota_t wde_qt2 = {
	1896, /* hif */
	98, /* wcpu */
	0, /* pkt_in */
	22, /* cpu_io */
};

/* PCIE SU TP */
static struct wde_quota_t wde_qt3 = {
	256, /* hif */
	196, /* wcpu */
	0, /* pkt_in */
	44, /* cpu_io */
};

/* DLFW */
static struct wde_quota_t wde_qt4 = {
	0, /* hif */
	0, /* wcpu */
	0, /* pkt_in */
	0, /* cpu_io */
};

/* PCIE BCN TEST */
static struct wde_quota_t wde_qt5 = {
	3666, /* hif */
	196, /* wcpu */
	0, /* pkt_in */
	44, /* cpu_io */
};

/* PCIE 64 */
static struct wde_quota_t wde_qt6 = {
	448, /* hif */
	48, /* wcpu */
	0, /* pkt_in */
	16, /* cpu_io */
};

/* PCIE 128 */
static struct wde_quota_t wde_qt7 = {
	896, /* hif */
	56, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* PCIE STF, USB */
static struct wde_quota_t wde_qt8 = {
	204, /* hif */
	44, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* LA-PCIE 8852A*/
static struct wde_quota_t wde_qt9 = {
	1172, /* hif */
	196, /* wcpu */
	0, /* pkt_in */
	40, /* cpu_io */
};

/* LA-PCIE */
static struct wde_quota_t wde_qt10 = {
	200, /* hif */
	48, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* LA-SDIO 8852A*/
static struct wde_quota_t wde_qt11 = {
	128, /* hif */
	196, /* wcpu */
	0, /* pkt_in */
	4, /* cpu_io */
};

/* SDIO SCC */
static struct wde_quota_t wde_qt12 = {
	112, /* hif */
	8, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* SDIO LA */
static struct wde_quota_t wde_qt13 = {
	112, /* hif */
	4, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* LA USB 8852A */
static struct wde_quota_t wde_qt14 = {
	256, /* hif */
	118, /* wcpu */
	0, /* pkt_in */
	10, /* cpu_io */
};

/* LA USB 8852B */
static struct wde_quota_t wde_qt15 = {
	112, /* hif */
	4, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/*8852C   USB */
static struct wde_quota_t wde_qt16 = {
	292, /* hif */
	20, /* wcpu */
	0, /* pkt_in */
	20, /* cpu_io */
};

/*8852C   DLFW */
static struct wde_quota_t wde_qt17 = {
	0, /* hif */
	0, /* wcpu */
	0, /* pkt_in */
	0, /* cpu_io */
};

/* 8852C PCIE SCC */
static struct wde_quota_t wde_qt18 = {
	3228, /* hif */
	60, /* wcpu */
	0, /* pkt_in */
	40, /* cpu_io */
};

/* 8852C PCIE DBCC */
static struct wde_quota_t wde_qt19 = {
	3218, /* hif */
	60, /* wcpu */
	0, /* pkt_in */
	50, /* cpu_io */
};

/* 8852C PCIE SCC STF */
static struct wde_quota_t wde_qt20 = {
	216, /* hif */
	20, /* wcpu */
	0, /* pkt_in */
	20, /* cpu_io */
};

/* 8852C PCIE DBCC STF */
static struct wde_quota_t wde_qt21 = {
	152, /* hif */
	64, /* wcpu */
	0, /* pkt_in */
	40, /* cpu_io */
};

/* 8852C PCIE LA */
static struct wde_quota_t wde_qt22 = {
	3120, /* hif */
	64, /* wcpu */
	0, /* pkt_in */
	40, /* cpu_io */
};

/* PCIE SCC TRUBO */
static struct wde_quota_t wde_qt23 = {
	960, /* hif */
	48, /* wcpu */
	0, /* pkt_in */
	16, /* cpu_io */
};

/* 8852B USB CABV */
static struct wde_quota_t wde_qt24 = {
	164, /* hif */
	44, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* 8852B USB2.0 SCC */
static struct wde_quota_t wde_qt25 = {
	152, /* hif */
	2, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* 8852B USB2.0 SCC TURBO*/
static struct wde_quota_t wde_qt26 = {
	210, /* hif */
	2, /* wcpu */
	0, /* pkt_in */
	8, /* cpu_io */
};

/* PCIE DBCC */
static struct ple_quota_t ple_qt0 = {
	264, /* cmac0_tx */
	66, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	13, /* mpdu_proc */
	356, /* cmac0_dma */
	94, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* 8852AU ccv */
static struct wde_quota_t wde_qt27 = {
	504, /* hif */
	196, /* wcpu */
	0, /* pkt_in */
	60, /* cpu_io */
};

/* PCIE DBCC */
static struct ple_quota_t ple_qt1 = {
	264, /* cmac0_tx */
	66, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	941, /* cmac0_dma */
	679, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	240, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO */
static struct ple_quota_t ple_qt2 = {
	1536, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	26, /* mpdu_proc */
	360, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO */
static struct ple_quota_t ple_qt3 = {
	1536, /* cmac0_tx */
	0, /* cmac1_tx */
	1149, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	1159, /* mpdu_proc */
	1493, /* cmac0_dma */
	0, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	120, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE SCC */
static struct ple_quota_t ple_qt4 = {
	264, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	13, /* mpdu_proc */
	356, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE SCC */
static struct ple_quota_t ple_qt5 = {
	264, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	1101, /* cmac0_dma */
	0, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	120, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO */
static struct ple_quota_t ple_qt6 = {
	2048, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	26, /* mpdu_proc */
	360, /* cmac0_dma */
	94, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO */
static struct ple_quota_t ple_qt7 = {
	2048, /* cmac0_tx */
	0, /* cmac1_tx */
	530, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	540, /* mpdu_proc */
	874, /* cmac0_dma */
	608, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	240, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE STF SCC */
static struct ple_quota_t ple_qt8 = {
	1536, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	356, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE STF SCC */
static struct ple_quota_t ple_qt9 = {
	2686, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	1506, /* cmac0_dma */
	0, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	120, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE STF DBCC */
static struct ple_quota_t ple_qt10 = {
	2272, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	13, /* mpdu_proc */
	356, /* cmac0_dma */
	94, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE STF DBCC */
static struct ple_quota_t ple_qt11 = {
	2579, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	663, /* cmac0_dma */
	401, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	240, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE SU TP */
static struct ple_quota_t ple_qt12 = {
	66, /* cmac0_tx */
	66, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	13, /* mpdu_proc */
	25, /* cmac0_dma */
	25, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* DLFW */
static struct ple_quota_t ple_qt13 = {
	0, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	0, /* wcpu */
	0, /* mpdu_proc */
	0, /* cmac0_dma */
	0, /* cma1_dma */
	0, /* bb_rpt */
	0, /* wd_rel */
	0, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE BCN TEST */
static struct ple_quota_t ple_qt14 = {
	588, /* cmac0_tx */
	147, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	26, /* mpdu_proc */
	356, /* cmac0_dma */
	89, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	80, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE BCN TEST */
static struct ple_quota_t ple_qt15 = {
	688, /* cmac0_tx */
	247, /* cmac1_tx */
	116, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	126, /* mpdu_proc */
	456, /* cmac0_dma */
	189, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	80, /* cpu_io */
	0, /* tx_rpt */
};

/* USB DBCC */
static struct ple_quota_t ple_qt16 = {
	2048, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	26, /* wcpu */
	13, /* mpdu_proc */
	360, /* cmac0_dma */
	94, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* USB DBCC */
static struct ple_quota_t ple_qt17 = {
	2048, /* cmac0_tx */
	0, /* cmac1_tx */
	515, /* c2h */
	48, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	859, /* cmac0_dma */
	593, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	240, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE 64 */
static struct ple_quota_t ple_qt18 = {
	147, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	4, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE 64 */
static struct ple_quota_t ple_qt19 = {
	147, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	580, /* wcpu */
	13, /* mpdu_proc */
	745, /* cmac0_dma */
	0, /* cma1_dma */
	599, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE STF */
static struct ple_quota_t ple_qt20 = {
	962, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	88, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE STF */
static struct ple_quota_t ple_qt21 = {
	1023, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	149, /* wcpu */
	13, /* mpdu_proc */
	239, /* cmac0_dma */
	0, /* cma1_dma */
	93, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE 128 */
static struct ple_quota_t ple_qt22 = {
	269, /* cmac0_tx */
	0, /* cmac1_tx */
	18, /* c2h */
	20, /* h2c */
	15, /* wcpu */
	28, /* mpdu_proc */
	180, /* cmac0_dma */
	0, /* cma1_dma */
	34, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* LA PCIE 8852A*/
static struct ple_quota_t ple_qt23 = {
	104, /* cmac0_tx */
	26, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	13, /* mpdu_proc */
	356, /* cmac0_dma */
	94, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* LA PCIE 8852A*/
static struct ple_quota_t ple_qt24 = {
	104, /* cmac0_tx */
	26, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	437, /* cmac0_dma */
	175, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	89, /* cpu_io */
	0, /* tx_rpt */
};

/* USB SCC */
static struct ple_quota_t ple_qt25 = {
	1536, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	360, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* USB SCC */
static struct ple_quota_t ple_qt26 = {
	2654, /* cmac0_tx */
	0, /* cmac1_tx */
	1134, /* c2h */
	48, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	1478, /* cmac0_dma */
	0, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	120, /* cpu_io */
	0, /* tx_rpt */
};

/* USB3.0 52B */
static struct ple_quota_t ple_qt27 = {
	286, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	4, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* USB3.0 52B */
static struct ple_quota_t ple_qt28 = {
	286, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	48, /* h2c */
	37, /* wcpu */
	13, /* mpdu_proc */
	211, /* cmac0_dma */
	0, /* cma1_dma */
	65, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE SU TP */
static struct ple_quota_t ple_qt29 = {
	66, /* cmac0_tx */
	66, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	1224, /* cmac0_dma */
	1224, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	240, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE LA */
static struct ple_quota_t ple_qt30 = {
	51, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE LA */
static struct ple_quota_t ple_qt31 = {
	74, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	36, /* wcpu */
	13, /* mpdu_proc */
	201, /* cmac0_dma */
	0, /* cma1_dma */
	55, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* LA SDIO 8852A*/
static struct ple_quota_t ple_qt32 = {
	500, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	26, /* wcpu */
	26, /* mpdu_proc */
	360, /* cmac0_dma */
	94, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* LA SDIO 8852A*/
static struct ple_quota_t ple_qt33 = {
	500, /* cmac0_tx */
	0, /* cmac1_tx */
	222, /* c2h */
	20, /* h2c */
	64, /* wcpu */
	232, /* mpdu_proc */
	566, /* cmac0_dma */
	300, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	214, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO SCC */
static struct ple_quota_t ple_qt34 = {
	320, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	16, /* h2c */
	26, /* wcpu */
	0, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	16, /* bb_rpt */
	1, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO SCC */
static struct ple_quota_t ple_qt35 = {
	320, /* cmac0_tx */
	0, /* cmac1_tx */
	123, /* c2h */
	16, /* h2c */
	133, /* wcpu */
	0, /* mpdu_proc */
	285, /* cmac0_dma */
	0, /* cma1_dma */
	123, /* bb_rpt */
	1, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO LA */
static struct ple_quota_t ple_qt36 = {
	86, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	52, /* wcpu */
	26, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO LA */
static struct ple_quota_t ple_qt37 = {
	86, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	52, /* wcpu */
	26, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* LA USB 8852A*/
static struct ple_quota_t ple_qt38 = {
	512, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	60, /* h2c */
	26, /* wcpu */
	13, /* mpdu_proc */
	360, /* cmac0_dma */
	94, /* cma1_dma */
	32, /* bb_rpt */
	40, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* LA USB 8852A*/
static struct ple_quota_t ple_qt39 = {
	512, /* cmac0_tx */
	0, /* cmac1_tx */
	184, /* c2h */
	60, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	527, /* cmac0_dma */
	261, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	175, /* cpu_io */
	0, /* tx_rpt */
};

/* LA USB 8852B*/
static struct ple_quota_t ple_qt40 = {
	26, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	52, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* LA USB 8852B*/
static struct ple_quota_t ple_qt41 = {
	26, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	48, /* h2c */
	97, /* wcpu */
	13, /* mpdu_proc */
	223, /* cmac0_dma */
	0, /* cma1_dma */
	77, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* USB 52C */
static struct ple_quota_t ple_qt42 = {
	1196, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	130, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	20, /* wd_rel */
	8, /* cpu_io */
	16, /* tx_rpt */
};

/* USB 52C */
static struct ple_quota_t ple_qt43 = {
	2938, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	1755, /* wcpu */
	13, /* mpdu_proc */
	1872, /* cmac0_dma */
	0, /* cma1_dma */
	1774, /* bb_rpt */
	20, /* wd_rel */
	120, /* cpu_io */
	1758, /* tx_rpt */
};

/* DLFW 52C */
static struct ple_quota_t ple_qt44 = {
	0, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	256, /* h2c */
	0, /* wcpu */
	0, /* mpdu_proc */
	0, /* cmac0_dma */
	0, /* cma1_dma */
	0, /* bb_rpt */
	0, /* wd_rel */
	0, /* cpu_io */
	0, /* tx_rpt */
};

/* DLFW 52C */
static struct ple_quota_t ple_qt45 = {
	0, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	256, /* h2c */
	0, /* wcpu */
	0, /* mpdu_proc */
	0, /* cmac0_dma */
	0, /* cma1_dma */
	0, /* bb_rpt */
	0, /* wd_rel */
	0, /* cpu_io */
	0, /* tx_rpt */
};

/* 8852C PCIE SCC */
static struct ple_quota_t ple_qt46 = {
	525, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	62, /* wd_rel */
	8, /* cpu_io */
	16, /* tx_rpt */
};

/* 8852C PCIE SCC */
static struct ple_quota_t ple_qt47 = {
	525, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	1034, /* wcpu */
	13, /* mpdu_proc */
	1199, /* cmac0_dma */
	0, /* cma1_dma */
	1053, /* bb_rpt */
	62, /* wd_rel */
	160, /* cpu_io */
	1037, /* tx_rpt */
};

/* 8852C PCIE DBCC */
static struct ple_quota_t ple_qt48 = {
	525, /* cmac0_tx */
	200, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	178, /* cma1_dma */
	32, /* bb_rpt */
	62, /* wd_rel */
	8, /* cpu_io */
	16, /* tx_rpt */
};

/* 8852C PCIE DBCC */
static struct ple_quota_t ple_qt49 = {
	525, /* cmac0_tx */
	200, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	656, /* wcpu */
	13, /* mpdu_proc */
	821, /* cmac0_dma */
	821, /* cma1_dma */
	675, /* bb_rpt */
	62, /* wd_rel */
	160, /* cpu_io */
	659, /* tx_rpt */
};

/* 8852C PCIE SCC STF */
static struct ple_quota_t ple_qt50 = {
	1248, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	130, /* cmac0_dma */
	130, /* cma1_dma */
	32, /* bb_rpt */
	38, /* wd_rel */
	8, /* cpu_io */
	16, /* tx_rpt */
};

/* 8852C PCIE SCC STF */
static struct ple_quota_t ple_qt51 = {
	3024, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	1789, /* wcpu */
	13, /* mpdu_proc */
	1906, /* cmac0_dma */
	1906, /* cma1_dma */
	1808, /* bb_rpt */
	38, /* wd_rel */
	40, /* cpu_io */
	1792, /* tx_rpt */
};

/* 8852C PCIE DBCC STF */
static struct ple_quota_t ple_qt52 = {
	1664, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	130, /* cmac0_dma */
	130, /* cma1_dma */
	32, /* bb_rpt */
	38, /* wd_rel */
	8, /* cpu_io */
	16, /* tx_rpt */
};

/* 8852C PCIE DBCC STF */
static struct ple_quota_t ple_qt53 = {
	3024, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	1373, /* wcpu */
	13, /* mpdu_proc */
	1490, /* cmac0_dma */
	1490, /* cma1_dma */
	1392, /* bb_rpt */
	38, /* wd_rel */
	160, /* cpu_io */
	1376, /* tx_rpt */
};

/* 8852C PCIE LA */
static struct ple_quota_t ple_qt54 = {
	300, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	356, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	62, /* wd_rel */
	8, /* cpu_io */
	16, /* tx_rpt */
};

/* 8852C PCIE LA */
static struct ple_quota_t ple_qt55 = {
	300, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	1081, /* wcpu */
	13, /* mpdu_proc */
	1424, /* cmac0_dma */
	0, /* cma1_dma */
	1100, /* bb_rpt */
	62, /* wd_rel */
	160, /* cpu_io */
	1084, /* tx_rpt */
};

/* 8852B PCIE AB */
static struct ple_quota_t ple_qt56 = {
	147, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	20, /* h2c */
	68, /* wcpu */
	13, /* mpdu_proc */
	233, /* cmac0_dma */
	0, /* cma1_dma */
	87, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE SCC TURBO */
static struct ple_quota_t ple_qt57 = {
	147, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	13, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* PCIE 64 */
static struct ple_quota_t ple_qt58 = {
	147, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	20, /* h2c */
	68, /* wcpu */
	13, /* mpdu_proc */
	242, /* cmac0_dma */
	0, /* cma1_dma */
	96, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* USB2.0 52B */
static struct ple_quota_t ple_qt61 = {
	130, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	4, /* wcpu */
	13, /* mpdu_proc */
	322, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* USB2.0 52B */
static struct ple_quota_t ple_qt62 = {
	130, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	48, /* h2c */
	37, /* wcpu */
	13, /* mpdu_proc */
	355, /* cmac0_dma */
	0, /* cma1_dma */
	65, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* 8852AU ccv */
static struct ple_quota_t ple_qt63 = {
	2654, /* cmac0_tx */
	0, /* cmac1_tx */
	1134, /* c2h */
	48, /* h2c */
	64, /* wcpu */
	13, /* mpdu_proc */
	1478, /* cmac0_dma */
	0, /* cma1_dma */
	64, /* bb_rpt */
	128, /* wd_rel */
	120, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO SCC TURBO */
static struct ple_quota_t ple_qt66 = {
	836, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	16, /* h2c */
	26, /* wcpu */
	0, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	16, /* bb_rpt */
	1, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* SDIO SCC TURBO */
static struct ple_quota_t ple_qt67 = {
	836, /* cmac0_tx */
	0, /* cmac1_tx */
	375, /* c2h */
	16, /* h2c */
	385, /* wcpu */
	0, /* mpdu_proc */
	537, /* cmac0_dma */
	0, /* cma1_dma */
	375, /* bb_rpt */
	1, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* USB2.0 52B TURBO*/
static struct ple_quota_t ple_qt68 = {
	858, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	4, /* wcpu */
	13, /* mpdu_proc */
	370, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* USB2.0 52B TURBO*/
static struct ple_quota_t ple_qt69 = {
	858, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	48, /* h2c */
	37, /* wcpu */
	13, /* mpdu_proc */
	403, /* cmac0_dma */
	0, /* cma1_dma */
	65, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

/* USB3.0 52B TURBO*/
static struct ple_quota_t ple_qt70 = {
	1040, /* cmac0_tx */
	0, /* cmac1_tx */
	16, /* c2h */
	48, /* h2c */
	4, /* wcpu */
	13, /* mpdu_proc */
	178, /* cmac0_dma */
	0, /* cma1_dma */
	32, /* bb_rpt */
	14, /* wd_rel */
	8, /* cpu_io */
	0, /* tx_rpt */
};

/* USB3.0 52B TURBO*/
static struct ple_quota_t ple_qt71 = {
	1040, /* cmac0_tx */
	0, /* cmac1_tx */
	32, /* c2h */
	48, /* h2c */
	37, /* wcpu */
	13, /* mpdu_proc */
	211, /* cmac0_dma */
	0, /* cma1_dma */
	65, /* bb_rpt */
	14, /* wd_rel */
	24, /* cpu_io */
	0, /* tx_rpt */
};

#if MAC_AX_PCIE_SUPPORT
static struct dle_mem_t dle_mem_pcie_8852a[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size0, &ple_size0, /* wde_size, ple_size */
	 &wde_qt0, &wde_qt0, /* wde_min_qt, wde_max_qt */
	 &ple_qt4, &ple_qt5}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC, /* qta_mode */
	 &wde_size0, &ple_size0, /* wde_size, ple_size */
	 &wde_qt0, &wde_qt0, /* wde_min_qt, wde_max_qt */
	 &ple_qt0, &ple_qt1}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_SCC_STF, /* qta_mode */
	 &wde_size1, &ple_size2, /* wde_size, ple_size */
	 &wde_qt1, &wde_qt1, /* wde_min_qt, wde_max_qt */
	 &ple_qt8, &ple_qt9}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC_STF, /* qta_mode */
	 &wde_size1, &ple_size2, /* wde_size, ple_size */
	 &wde_qt1, &wde_qt1, /* wde_min_qt, wde_max_qt */
	 &ple_qt10, &ple_qt11}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_SU_TP, /* qta_mode */
	 &wde_size3, &ple_size3, /* wde_size, ple_size */
	 &wde_qt3, &wde_qt3, /* wde_min_qt, wde_max_qt */
	 &ple_qt12, &ple_qt29}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size4, &ple_size4, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size10, &ple_size10, /* wde_size, ple_size */
	 &wde_qt9, &wde_qt9, /* wde_min_qt, wde_max_qt */
	 &ple_qt23, &ple_qt24}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_pcie_8852b[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size6, &ple_size6, /* wde_size, ple_size */
	 &wde_qt6, &wde_qt6, /* wde_min_qt, wde_max_qt */
	 &ple_qt18, &ple_qt58}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_SCC_TURBO, /* qta_mode */
	 &wde_size23, &ple_size24, /* wde_size, ple_size */
	 &wde_qt23, &wde_qt23, /* wde_min_qt, wde_max_qt */
	 &ple_qt57, &ple_qt19}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_SCC_STF, /* qta_mode */
	 &wde_size8, &ple_size7, /* wde_size, ple_size */
	 &wde_qt8, &wde_qt8, /* wde_min_qt, wde_max_qt */
	 &ple_qt20, &ple_qt21}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size9, &ple_size8, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size11, &ple_size11, /* wde_size, ple_size */
	 &wde_qt10, &wde_qt10, /* wde_min_qt, wde_max_qt */
	 &ple_qt30, &ple_qt31}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_pcie_8852c[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size19, &ple_size19, /* wde_size, ple_size */
	 &wde_qt18, &wde_qt18, /* wde_min_qt, wde_max_qt */
	 &ple_qt46, &ple_qt47}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC, /* qta_mode */
	 &wde_size20, &ple_size20, /* wde_size, ple_size */
	 &wde_qt19, &wde_qt19, /* wde_min_qt, wde_max_qt */
	 &ple_qt48, &ple_qt49}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_SCC_STF, /* qta_mode */
	 &wde_size21, &ple_size21, /* wde_size, ple_size */
	 &wde_qt20, &wde_qt20, /* wde_min_qt, wde_max_qt */
	 &ple_qt50, &ple_qt51}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC_STF, /* qta_mode */
	 &wde_size21, &ple_size21, /* wde_size, ple_size */
	 &wde_qt21, &wde_qt21, /* wde_min_qt, wde_max_qt */
	 &ple_qt52, &ple_qt53}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size18, &ple_size18, /* wde_size, ple_size */
	 &wde_qt17, &wde_qt17, /* wde_min_qt, wde_max_qt */
	 &ple_qt44, &ple_qt45}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size22, &ple_size22, /* wde_size, ple_size */
	 &wde_qt22, &wde_qt22, /* wde_min_qt, wde_max_qt */
	 &ple_qt54, &ple_qt55}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_pcie_8192xb[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size19, &ple_size19, /* wde_size, ple_size */
	 &wde_qt18, &wde_qt18, /* wde_min_qt, wde_max_qt */
	 &ple_qt46, &ple_qt47}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC, /* qta_mode */
	 &wde_size20, &ple_size20, /* wde_size, ple_size */
	 &wde_qt19, &wde_qt19, /* wde_min_qt, wde_max_qt */
	 &ple_qt48, &ple_qt49}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_SCC_STF, /* qta_mode */
	 &wde_size21, &ple_size21, /* wde_size, ple_size */
	 &wde_qt20, &wde_qt20, /* wde_min_qt, wde_max_qt */
	 &ple_qt50, &ple_qt51}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC_STF, /* qta_mode */
	 &wde_size21, &ple_size21, /* wde_size, ple_size */
	 &wde_qt21, &wde_qt21, /* wde_min_qt, wde_max_qt */
	 &ple_qt52, &ple_qt53}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size18, &ple_size18, /* wde_size, ple_size */
	 &wde_qt17, &wde_qt17, /* wde_min_qt, wde_max_qt */
	 &ple_qt44, &ple_qt45}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size22, &ple_size22, /* wde_size, ple_size */
	 &wde_qt22, &wde_qt22, /* wde_min_qt, wde_max_qt */
	 &ple_qt54, &ple_qt55}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};
#endif

#if MAC_AX_USB_SUPPORT
static struct dle_mem_t dle_mem_usb_8852a[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size1, &ple_size1, /* wde_size, ple_size */
	 &wde_qt1, &wde_qt1, /* wde_min_qt, wde_max_qt */
	 &ple_qt25, &ple_qt26}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC, /* qta_mode */
	 &wde_size1, &ple_size1, /* wde_size, ple_size */
	 &wde_qt1, &wde_qt1, /* wde_min_qt, wde_max_qt */
	 &ple_qt16, &ple_qt17}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size4, &ple_size4, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size15, &ple_size15, /* wde_size, ple_size */
	 &wde_qt14, &wde_qt14, /* wde_min_qt, wde_max_qt */
	 &ple_qt38, &ple_qt39}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_usb_8852a_ccv[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size26, &ple_size1, /* wde_size, ple_size */
	 &wde_qt27, &wde_qt27, /* wde_min_qt, wde_max_qt */
	 &ple_qt25, &ple_qt63}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC, /* qta_mode */
	 &wde_size1, &ple_size1, /* wde_size, ple_size */
	 &wde_qt1, &wde_qt1, /* wde_min_qt, wde_max_qt */
	 &ple_qt16, &ple_qt17}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size4, &ple_size4, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size15, &ple_size15, /* wde_size, ple_size */
	 &wde_qt14, &wde_qt14, /* wde_min_qt, wde_max_qt */
	 &ple_qt38, &ple_qt39}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_usb2_8852b[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size25, &ple_size27, /* wde_size, ple_size */
	 &wde_qt25, &wde_qt25, /* wde_min_qt, wde_max_qt */
	 &ple_qt61, &ple_qt62}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size9, &ple_size8, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size16, &ple_size16, /* wde_size, ple_size */
	 &wde_qt15, &wde_qt15, /* wde_min_qt, wde_max_qt */
	 &ple_qt40, &ple_qt41}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_SCC_TURBO, /* qta_mode */
	 &wde_size27, &ple_size28, /* wde_size, ple_size */
	 &wde_qt26, &wde_qt26, /* wde_min_qt, wde_max_qt */
	 &ple_qt68, &ple_qt69}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_usb3_8852b[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size25, &ple_size30, /* wde_size, ple_size */
	 &wde_qt25, &wde_qt25, /* wde_min_qt, wde_max_qt */
	 &ple_qt27, &ple_qt28}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size9, &ple_size8, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size16, &ple_size16, /* wde_size, ple_size */
	 &wde_qt15, &wde_qt15, /* wde_min_qt, wde_max_qt */
	 &ple_qt40, &ple_qt41}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_SCC_TURBO, /* qta_mode */
	 &wde_size27, &ple_size31, /* wde_size, ple_size */
	 &wde_qt26, &wde_qt26, /* wde_min_qt, wde_max_qt */
	 &ple_qt70, &ple_qt71}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_usb_8852c[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size17, &ple_size17, /* wde_size, ple_size */
	 &wde_qt16, &wde_qt16, /* wde_min_qt, wde_max_qt */
	 &ple_qt42, &ple_qt43}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size18, &ple_size18, /* wde_size, ple_size */
	 &wde_qt17, &wde_qt17, /* wde_min_qt, wde_max_qt */
	 &ple_qt44, &ple_qt45}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size16, &ple_size16, /* wde_size, ple_size */
	 &wde_qt15, &wde_qt15, /* wde_min_qt, wde_max_qt */
	 &ple_qt40, &ple_qt41}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_usb_8192xb[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size17, &ple_size17, /* wde_size, ple_size */
	 &wde_qt16, &wde_qt16, /* wde_min_qt, wde_max_qt */
	 &ple_qt42, &ple_qt43}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size18, &ple_size18, /* wde_size, ple_size */
	 &wde_qt17, &wde_qt17, /* wde_min_qt, wde_max_qt */
	 &ple_qt44, &ple_qt45}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size16, &ple_size16, /* wde_size, ple_size */
	 &wde_qt15, &wde_qt15, /* wde_min_qt, wde_max_qt */
	 &ple_qt40, &ple_qt41}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};
#endif

#if MAC_AX_SDIO_SUPPORT
static struct dle_mem_t dle_mem_sdio_8852a[] = {
	{MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size1, &ple_size1, /* wde_size, ple_size */
	 &wde_qt1, &wde_qt1, /* wde_min_qt, wde_max_qt */
	 &ple_qt2, &ple_qt3}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DBCC, /* qta_mode */
	 &wde_size1, &ple_size1, /* wde_size, ple_size */
	 &wde_qt1, &wde_qt1, /* wde_min_qt, wde_max_qt */
	 &ple_qt6, &ple_qt7}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size4, &ple_size4, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size12, &ple_size12, /* wde_size, ple_size */
	 &wde_qt11, &wde_qt11, /* wde_min_qt, wde_max_qt */
	 &ple_qt32, &ple_qt33}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};

static struct dle_mem_t dle_mem_sdio_8852b[] = {
	{MAC_AX_QTA_DLFW, /* qta_mode */
	 &wde_size9, &ple_size8, /* wde_size, ple_size */
	 &wde_qt4, &wde_qt4, /* wde_min_qt, wde_max_qt */
	 &ple_qt13, &ple_qt13}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_SCC, /* qta_mode */
	 &wde_size13, &ple_size13, /* wde_size, ple_size */
	 &wde_qt12, &wde_qt12, /* wde_min_qt, wde_max_qt */
	 &ple_qt34, &ple_qt35}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_SCC_TURBO, /* qta_mode */
	 &wde_size13, &ple_size29, /* wde_size, ple_size */
	 &wde_qt12, &wde_qt12, /* wde_min_qt, wde_max_qt */
	 &ple_qt66, &ple_qt67}, /* ple_min_qt, ple_max_qt */
	 {MAC_AX_QTA_LAMODE, /* qta_mode */
	 &wde_size14, &ple_size14, /* wde_size, ple_size */
	 &wde_qt13, &wde_qt13, /* wde_min_qt, wde_max_qt */
	 &ple_qt36, &ple_qt37}, /* ple_min_qt, ple_max_qt */
	{MAC_AX_QTA_INVALID, NULL, NULL, NULL, NULL, NULL, NULL},
};
#endif

u32 dle_rsvd_size(struct mac_ax_adapter *adapter, enum mac_ax_qta_mode mode);

u32 dle_is_txq_empty(struct mac_ax_adapter *adapter, u8 *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32, rval32;

	val32 = B_AX_WDE_EMPTY_QUE_CMAC0_ALL_AC | B_AX_WDE_EMPTY_QUE_CMAC0_MBH |
		B_AX_WDE_EMPTY_QUE_CMAC1_MBH | B_AX_WDE_EMPTY_QUE_CMAC0_WMM0 |
		B_AX_WDE_EMPTY_QUE_CMAC0_WMM1 | B_AX_WDE_EMPTY_QUE_OTHERS |
		B_AX_PLE_EMPTY_QUE_DMAC_MPDU_TX | B_AX_PLE_EMPTY_QTA_DMAC_H2C |
		B_AX_PLE_EMPTY_QUE_DMAC_SEC_TX | B_AX_WDE_EMPTY_QUE_DMAC_PKTIN |
		B_AX_WDE_EMPTY_QTA_DMAC_HIF | B_AX_WDE_EMPTY_QTA_DMAC_WLAN_CPU |
		B_AX_WDE_EMPTY_QTA_DMAC_PKTIN | B_AX_WDE_EMPTY_QTA_DMAC_CPUIO |
		B_AX_PLE_EMPTY_QTA_DMAC_B0_TXPL |
		B_AX_PLE_EMPTY_QTA_DMAC_B1_TXPL |
		B_AX_PLE_EMPTY_QTA_DMAC_MPDU_TX |
		B_AX_PLE_EMPTY_QTA_DMAC_CPUIO |
		B_AX_WDE_EMPTY_QTA_DMAC_DATA_CPU |
		B_AX_PLE_EMPTY_QTA_DMAC_WLAN_CPU;

	rval32 = MAC_REG_R32(R_AX_DLE_EMPTY0);
	if (val32 != (rval32 & val32)) {
		*val = DLE_QUEUE_NONEMPTY;
		PLTFM_MSG_TRACE("[TRACE]TXQ non empty 0x%X\n", rval32);
	} else {
		*val = DLE_QUEUE_EMPTY;
		PLTFM_MSG_TRACE("[TRACE]TXQ empty 0x%X\n", rval32);
	}

	return MACSUCCESS;
}

u32 dle_is_rxq_empty(struct mac_ax_adapter *adapter, u8 *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32, rval32;

	val32 = B_AX_WDE_EMPTY_QUE_DMAC_WDRLS | B_AX_PLE_EMPTY_QUE_DMAC_SEC_RX |
		B_AX_PLE_EMPTY_QUE_DMAC_MPDU_RX | B_AX_PLE_EMPTY_QUE_DMAC_HDP |
		B_AX_PLE_EMPTY_QUE_DMAC_PLRLS | B_AX_PLE_EMPTY_QUE_DMAC_CPUIO |
		B_AX_PLE_EMPTY_QTA_DMAC_C2H | B_AX_PLE_EMPTY_QTA_CMAC0_DMA_RX |
		B_AX_PLE_EMPTY_QTA_CMAC1_DMA_RX |
		B_AX_PLE_EMPTY_QTA_CMAC1_DMA_BBRPT |
		B_AX_PLE_EMPTY_QTA_DMAC_WDRLS;

	rval32 = MAC_REG_R32(R_AX_DLE_EMPTY1);
	if (val32 != (rval32 & val32)) {
		*val = DLE_QUEUE_NONEMPTY;
		PLTFM_MSG_TRACE("[TRACE] RXQ non empty 0x%X\n", rval32);
	} else {
		*val = DLE_QUEUE_EMPTY;
		PLTFM_MSG_TRACE("[TRACE] RXQ empty 0x%X\n", rval32);
	}

	return MACSUCCESS;
}

u32 dle_dfi_ctrl(struct mac_ax_adapter *adapter, struct dle_dfi_ctrl_t *ctrl_p)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cnt, ctrl_reg, data_reg, ctrl_data;

	switch (ctrl_p->type) {
	case DLE_CTRL_TYPE_WDE:
		ctrl_reg = R_AX_WDE_DBG_FUN_INTF_CTL;
		data_reg = R_AX_WDE_DBG_FUN_INTF_DATA;
		ctrl_data = SET_WORD(ctrl_p->target,
				     B_AX_WDE_DFI_TRGSEL) |
			    SET_WORD(ctrl_p->addr, B_AX_WDE_DFI_ADDR) |
			    B_AX_WDE_DFI_ACTIVE;
		break;
	case DLE_CTRL_TYPE_PLE:
		ctrl_reg = R_AX_PLE_DBG_FUN_INTF_CTL;
		data_reg = R_AX_PLE_DBG_FUN_INTF_DATA;
		ctrl_data = SET_WORD(ctrl_p->target,
				     B_AX_PLE_DFI_TRGSEL) |
			    SET_WORD(ctrl_p->addr, B_AX_PLE_DFI_ADDR) |
			    B_AX_PLE_DFI_ACTIVE;
		break;
	default:
		PLTFM_MSG_ERR("[ERR] dfi ctrl type %d\n", ctrl_p->type);
		return MACFUNCINPUT;
	}

	MAC_REG_W32(ctrl_reg, ctrl_data);

	cnt = DLE_DFI_WAIT_CNT;
	while (cnt && MAC_REG_R32(ctrl_reg) & B_AX_WDE_DFI_ACTIVE) {
		PLTFM_DELAY_US(DLE_DFI_WAIT_US);
		cnt--;
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR] dle dfi ctrl 0x%X set 0x%X timeout\n",
			      ctrl_reg, ctrl_data);
		return MACPOLLTO;
	}

	ctrl_p->out_data = MAC_REG_R32(data_reg);
	return MACSUCCESS;
}

u32 dle_dfi_freepg(struct mac_ax_adapter *adapter,
		   struct dle_dfi_freepg_t *freepg)
{
	struct dle_dfi_ctrl_t ctrl;
	u32 ret;

	ctrl.type = freepg->dle_type;
	ctrl.target = DLE_DFI_TYPE_FREEPG;
	ctrl.addr = DFI_TYPE_FREEPG_IDX;
	ret = dle_dfi_ctrl(adapter, &ctrl);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dle_dfi_ctrl %d\n", ret);
		return ret;
	}
	freepg->free_headpg = GET_FIELD(ctrl.out_data, B_AX_DLE_FREE_HEADPG);
	freepg->free_tailpg = GET_FIELD(ctrl.out_data, B_AX_DLE_FREE_TAILPG);

	ctrl.addr = DFI_TYPE_FREEPG_PUBNUM;
	ret = dle_dfi_ctrl(adapter, &ctrl);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dle_dfi_ctrl %d\n", ret);
		return ret;
	}
	freepg->pub_pgnum = GET_FIELD(ctrl.out_data, B_AX_DLE_PUB_PGNUM);

	return MACSUCCESS;
}

u32 dle_dfi_quota(struct mac_ax_adapter *adapter,
		  struct dle_dfi_quota_t *quota)
{
	struct dle_dfi_ctrl_t ctrl;
	u32 ret;

	ctrl.type = quota->dle_type;
	ctrl.target = DLE_DFI_TYPE_QUOTA;
	ctrl.addr = quota->qtaid;
	ret = dle_dfi_ctrl(adapter, &ctrl);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dle_dfi_ctrl %d\n", ret);
		return ret;
	}

	quota->rsv_pgnum = GET_FIELD(ctrl.out_data, B_AX_DLE_RSV_PGNUM);
	quota->use_pgnum = GET_FIELD(ctrl.out_data, B_AX_DLE_USE_PGNUM);

	return MACSUCCESS;
}

u32 dle_dfi_qempty(struct mac_ax_adapter *adapter,
		   struct dle_dfi_qempty_t *qempty)
{
	struct dle_dfi_ctrl_t ctrl;
	u32 ret;

	ctrl.type = qempty->dle_type;
	ctrl.target = DLE_DFI_TYPE_QEMPTY;
	ctrl.addr = qempty->grpsel;
	ret = dle_dfi_ctrl(adapter, &ctrl);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dle_dfi_ctrl %d\n", ret);
		return ret;
	}

	qempty->qempty = GET_FIELD(ctrl.out_data, B_AX_DLE_QEMPTY_GRP);

	return MACSUCCESS;
}

u32 mac_is_txq_empty(struct mac_ax_adapter *adapter,
		     struct mac_ax_tx_queue_empty *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct dle_dfi_qempty_t qempty;
	u32 val32, val32_emp0, ret;
	u32 i, j, qnum;

	PLTFM_MEMSET(val, 0xFF, sizeof(struct mac_ax_tx_queue_empty));

	qempty.dle_type = DLE_CTRL_TYPE_WDE;
	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		qnum = WDE_QEMPTY_ACQ_NUM_8852A;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		qnum = WDE_QEMPTY_ACQ_NUM_8852B;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		qnum = WDE_QEMPTY_ACQ_NUM_8852C;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		qnum = WDE_QEMPTY_ACQ_NUM_8192XB;
	} else {
		PLTFM_MSG_ERR("[ERR]wde qempty acq num not define\n");
		return MACCHIPID;
	}
	for (i = 0; i < qnum; i++) {
		qempty.grpsel = i;
		ret = dle_dfi_qempty(adapter, &qempty);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]dle dfi acq empty %d\n", ret);
			return ret;
		}
		for (j = 0 ; j < QEMP_ACQ_GRP_MACID_NUM; j++) {
			val32 = GET_FIEL2(qempty.qempty,
					  j * QEMP_ACQ_GRP_QSEL_SH,
					  QEMP_ACQ_GRP_QSEL_MASK);
			if (val32 != QEMP_ACQ_GRP_QSEL_MASK)
				val->macid_txq_empty[i] &= ~BIT(j);
		}
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		qempty.grpsel = WDE_QEMPTY_MGQ_SEL_8852A;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		qempty.grpsel = WDE_QEMPTY_MGQ_SEL_8852B;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		qempty.grpsel = WDE_QEMPTY_MGQ_SEL_8852C;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		qempty.grpsel = WDE_QEMPTY_MGQ_SEL_8192XB;
	} else {
		PLTFM_MSG_ERR("[ERR]wde qempty mgq sel not define\n");
		return MACCHIPID;
	}
	ret = dle_dfi_qempty(adapter, &qempty);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dle dfi mgq empty %d\n", ret);
		return ret;
	}
	if (!(qempty.qempty & B_CMAC0_MGQ_NORMAL))
		val->band0_mgnt_empty = 0;

	if (is_curr_dbcc(adapter) && !(qempty.qempty & B_CMAC1_MGQ_NORMAL))
		val->band1_mgnt_empty = 0;

	val32 = B_CMAC0_MGQ_NO_PWRSAV | B_CMAC0_CPUMGQ;
	if (is_curr_dbcc(adapter))
		val32 |= B_CMAC1_MGQ_NO_PWRSAV | B_CMAC1_CPUMGQ;
	if ((qempty.qempty & val32) != val32)
		val->fw_txq_empty = 0;

	val32 = B_AX_WDE_EMPTY_QTA_DMAC_WLAN_CPU |
		B_AX_WDE_EMPTY_QTA_DMAC_DATA_CPU |
		B_AX_PLE_EMPTY_QTA_DMAC_WLAN_CPU;
	val32_emp0 = MAC_REG_R32(R_AX_DLE_EMPTY0);
	if (val32 != (val32_emp0 & val32))
		val->fw_txq_empty = 0;

	if (!(val32_emp0 & B_AX_PLE_EMPTY_QTA_DMAC_H2C))
		val->h2c_empty = 0;

	val32 = B_AX_WDE_EMPTY_QUE_OTHERS | B_AX_PLE_EMPTY_QUE_DMAC_MPDU_TX |
		B_AX_WDE_EMPTY_QTA_DMAC_CPUIO | B_AX_PLE_EMPTY_QTA_DMAC_CPUIO |
		B_AX_WDE_EMPTY_QUE_DMAC_PKTIN | B_AX_WDE_EMPTY_QTA_DMAC_HIF |
		B_AX_PLE_EMPTY_QUE_DMAC_SEC_TX | B_AX_WDE_EMPTY_QTA_DMAC_PKTIN |
		B_AX_PLE_EMPTY_QTA_DMAC_B0_TXPL |
		B_AX_PLE_EMPTY_QTA_DMAC_B1_TXPL |
		B_AX_PLE_EMPTY_QTA_DMAC_MPDU_TX;

	if (val32 != (val32_emp0 & val32))
		val->others_empty = 0;

	return MACSUCCESS;
}

u32 mac_is_rxq_empty(struct mac_ax_adapter *adapter,
		     struct mac_ax_rx_queue_empty *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32, val32_emp1;

	PLTFM_MEMSET(val, 0xFF, sizeof(struct mac_ax_rx_queue_empty));

	val32_emp1 = MAC_REG_R32(R_AX_DLE_EMPTY1);
	if (!(val32_emp1 & B_AX_PLE_EMPTY_QTA_CMAC0_DMA_RX))
		val->band0_rxq_empty = 0;

	if (!(val32_emp1 & B_AX_PLE_EMPTY_QTA_CMAC1_DMA_RX))
		val->band1_rxq_empty = 0;

	if (!(val32_emp1 & B_AX_PLE_EMPTY_QTA_DMAC_C2H))
		val->c2h_empty = 0;

	val32 = B_AX_WDE_EMPTY_QUE_DMAC_WDRLS | B_AX_PLE_EMPTY_QUE_DMAC_SEC_RX |
		B_AX_PLE_EMPTY_QUE_DMAC_MPDU_RX | B_AX_PLE_EMPTY_QUE_DMAC_HDP |
		B_AX_PLE_EMPTY_QUE_DMAC_PLRLS | B_AX_PLE_EMPTY_QUE_DMAC_CPUIO |
		B_AX_PLE_EMPTY_QTA_CMAC1_DMA_BBRPT |
		B_AX_PLE_EMPTY_QTA_DMAC_WDRLS;

	if (val32 != (val32_emp1 & val32))
		val->others_empty = 0;

	return MACSUCCESS;
}

u32 mac_chk_allq_empty(struct mac_ax_adapter *adapter, u8 *empty)
{
	u8 chk_cnt, txq_empty, rxq_empty;
	u32 ret;
	*empty = 1;

	for (chk_cnt = 0; chk_cnt < QUEUE_EMPTY_CHK_CNT; chk_cnt++) {
		ret = dle_is_txq_empty(adapter, &txq_empty);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR] TXQ chk fail %d\n", ret);
			return ret;
		}

		ret = dle_is_rxq_empty(adapter, &rxq_empty);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR] TXQ chk fail %d\n", ret);
			return ret;
		}

		if (!(txq_empty & rxq_empty)) {
			PLTFM_MSG_TRACE("[TRACE] CHK TXQ %d RXQ %d\n",
					txq_empty, rxq_empty);
			*empty = 0;
			break;
		}
	}

	return MACSUCCESS;
}

u32 get_dle_rsvd_info(struct mac_ax_adapter *adapter, enum DLE_RSVD_INFO *info)
{
	enum mac_ax_qta_mode mode = adapter->dle_info.qta_mode;
	u32 rsvd_size;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (mode == MAC_AX_QTA_LAMODE)
			*info = DLE_RSVD_INFO_LAMODE;
		else if (mode == MAC_AX_QTA_SCC)
			*info = DLE_RSVD_INFO_FW;
		else
			*info = DLE_RSVD_INFO_NONE;
	} else {
		if (mode == MAC_AX_QTA_LAMODE)
			*info = DLE_RSVD_INFO_LAMODE;
		else
			*info = DLE_RSVD_INFO_NONE;
	}

	rsvd_size = dle_rsvd_size(adapter, mode);
	if (rsvd_size != 0 && *info == DLE_RSVD_INFO_NONE) {
		PLTFM_MSG_ERR("[ERR]get_dle_rsvd_info %d, rsvd_size %d\n",
			      *info, rsvd_size);
		return MACFFCFG;
	}

	return MACSUCCESS;
}

u32 dle_used_size(struct dle_size_t *wde, struct dle_size_t *ple)
{
	return (u32)(wde->pge_size * (wde->lnk_pge_num + wde->unlnk_pge_num)) +
	       (u32)(ple->pge_size * (ple->lnk_pge_num + ple->unlnk_pge_num));
}

u32 dle_rsvd_size(struct mac_ax_adapter *adapter, enum mac_ax_qta_mode mode)
{
	if (mode == MAC_AX_QTA_LAMODE) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
			return DLE_LAMODE_SIZE_8852A;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
			return DLE_LAMODE_SIZE_8852B;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
			return DLE_LAMODE_SIZE_8852C;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			return DLE_LAMODE_SIZE_8192XB;
		else
			return 0;
	} else if (mode == MAC_AX_QTA_SCC) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
			return DLE_SCC_RSVD_SIZE_8852B;
		else
			return 0;
	} else {
		return 0;
	}
}

void dle_func_en(struct mac_ax_adapter *adapter, u8 en)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = MAC_REG_R32(R_AX_DMAC_FUNC_EN);
	if (en == MAC_AX_FUNC_EN)
		val32 |= (B_AX_DLE_WDE_EN | B_AX_DLE_PLE_EN);
	else if (en == MAC_AX_FUNC_DIS)
		val32 &= ~(B_AX_DLE_WDE_EN | B_AX_DLE_PLE_EN);
	else
		return;
	MAC_REG_W32(R_AX_DMAC_FUNC_EN, val32);
}

void dle_clk_en(struct mac_ax_adapter *adapter, u8 en)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = MAC_REG_R32(R_AX_DMAC_CLK_EN);
	if (en == MAC_AX_FUNC_EN)
		val32 |= (B_AX_DLE_WDE_CLK_EN | B_AX_DLE_PLE_CLK_EN);
	else if (en == MAC_AX_FUNC_DIS)
		val32 &= ~(B_AX_DLE_WDE_CLK_EN | B_AX_DLE_PLE_CLK_EN);
	else
		return;
	MAC_REG_W32(R_AX_DMAC_CLK_EN, val32);
}

struct dle_mem_t *get_dle_mem_cfg(struct mac_ax_adapter *adapter,
				  enum mac_ax_qta_mode mode)
{
	struct dle_mem_t *cfg;
	enum mac_ax_intf intf = adapter->hw_info->intf;

	switch (intf) {
#if MAC_AX_SDIO_SUPPORT
	case MAC_AX_INTF_SDIO:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
			cfg = dle_mem_sdio_8852a;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
			cfg = dle_mem_sdio_8852b;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
			cfg = dle_mem_sdio_8852b;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			cfg = dle_mem_sdio_8852b;
		else
			cfg = NULL;
		break;
#endif
#if MAC_AX_USB_SUPPORT
	case MAC_AX_INTF_USB:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) &&
		    (is_cv(adapter, CCV)))
			cfg = dle_mem_usb_8852a_ccv;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
			cfg = dle_mem_usb_8852a;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B) &&
			 (get_usb_mode(adapter) == MAC_AX_USB2))
			cfg = dle_mem_usb2_8852b;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
			cfg = dle_mem_usb3_8852b;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
			cfg = dle_mem_usb_8852a;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			cfg = dle_mem_usb_8192xb;
		else
			cfg = NULL;
		break;
#endif
#if MAC_AX_PCIE_SUPPORT
	case MAC_AX_INTF_PCIE:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
			cfg = dle_mem_pcie_8852a;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
			cfg = dle_mem_pcie_8852b;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
			cfg = dle_mem_pcie_8852c;
		else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			cfg = dle_mem_pcie_8192xb;
		else
			cfg = NULL;
		break;
#endif
	default:
		cfg = NULL;
		break;
	}

	if (!cfg)
		return NULL;
	for (; cfg->mode != MAC_AX_QTA_INVALID; cfg++) {
		if (cfg->mode == mode) {
			adapter->dle_info.wde_pg_size = cfg->wde_size->pge_size;
			adapter->dle_info.ple_pg_size = cfg->ple_size->pge_size;
			adapter->dle_info.qta_mode = mode;
			adapter->dle_info.c0_rx_qta = cfg->ple_min_qt->cma0_dma;
			adapter->dle_info.c1_rx_qta = cfg->ple_min_qt->cma1_dma;
			adapter->dle_info.c0_tx_min = cfg->ple_min_qt->cma0_tx;
			adapter->dle_info.c1_tx_min = cfg->ple_min_qt->cma1_tx;
			return cfg;
		}
	}

	return NULL;
}

u32 dle_mix_cfg(struct mac_ax_adapter *adapter, struct dle_mem_t *cfg)
{
	u8 bound;
	u32 val32;
	struct dle_size_t *size_cfg;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = MAC_REG_R32(R_AX_WDE_PKTBUF_CFG);
	bound = 0;
	size_cfg = cfg->wde_size;

	switch (size_cfg->pge_size) {
	default:
	case MAC_AX_WDE_PG_64:
		val32 = SET_CLR_WORD(val32, S_AX_WDE_PAGE_SEL_64,
				     B_AX_WDE_PAGE_SEL);
		break;
	case MAC_AX_WDE_PG_128:
		val32 = SET_CLR_WORD(val32, S_AX_WDE_PAGE_SEL_128,
				     B_AX_WDE_PAGE_SEL);
		break;
	case MAC_AX_WDE_PG_256:
		PLTFM_MSG_ERR("[ERR]WDE DLE doesn't support 256 byte!\n");
		return MACHWNOSUP;
	}

	val32 = SET_CLR_WORD(val32, bound, B_AX_WDE_START_BOUND);
	val32 = SET_CLR_WORD(val32, size_cfg->lnk_pge_num,
			     B_AX_WDE_FREE_PAGE_NUM);
	MAC_REG_W32(R_AX_WDE_PKTBUF_CFG, val32);

	val32 = MAC_REG_R32(R_AX_PLE_PKTBUF_CFG);
	bound = (size_cfg->lnk_pge_num + size_cfg->unlnk_pge_num)
		* size_cfg->pge_size / DLE_BOUND_UNIT;
	size_cfg = cfg->ple_size;

	switch (size_cfg->pge_size) {
	default:
	case MAC_AX_PLE_PG_64:
		PLTFM_MSG_ERR("[ERR]PLE DLE doesn't support 64 byte!\n");
		return MACHWNOSUP;
	case MAC_AX_PLE_PG_128:
		val32 = SET_CLR_WORD(val32, S_AX_PLE_PAGE_SEL_128,
				     B_AX_PLE_PAGE_SEL);
		break;
	case MAC_AX_PLE_PG_256:
		val32 = SET_CLR_WORD(val32, S_AX_PLE_PAGE_SEL_256,
				     B_AX_PLE_PAGE_SEL);
		break;
	}

	val32 = SET_CLR_WORD(val32, bound, B_AX_PLE_START_BOUND);
	val32 = SET_CLR_WORD(val32, size_cfg->lnk_pge_num,
			     B_AX_PLE_FREE_PAGE_NUM);
	MAC_REG_W32(R_AX_PLE_PKTBUF_CFG, val32);

	return MACSUCCESS;
}

void wde_quota_cfg(struct mac_ax_adapter *adapter,
		   struct wde_quota_t *min_cfg,
		   struct wde_quota_t *max_cfg)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = SET_WORD(min_cfg->hif, B_AX_WDE_Q0_MIN_SIZE) |
		SET_WORD(max_cfg->hif, B_AX_WDE_Q0_MAX_SIZE);
	MAC_REG_W32(R_AX_WDE_QTA0_CFG, val32);

	val32 = SET_WORD(min_cfg->wcpu, B_AX_WDE_Q1_MIN_SIZE) |
		SET_WORD(max_cfg->wcpu, B_AX_WDE_Q1_MAX_SIZE);
	MAC_REG_W32(R_AX_WDE_QTA1_CFG, val32);

	val32 = SET_WORD(min_cfg->pkt_in, B_AX_WDE_Q3_MIN_SIZE) |
		SET_WORD(max_cfg->pkt_in, B_AX_WDE_Q3_MAX_SIZE);
	MAC_REG_W32(R_AX_WDE_QTA3_CFG, val32);

	val32 = SET_WORD(min_cfg->cpu_io, B_AX_WDE_Q4_MIN_SIZE) |
		SET_WORD(max_cfg->cpu_io, B_AX_WDE_Q4_MAX_SIZE);
	MAC_REG_W32(R_AX_WDE_QTA4_CFG, val32);
}

void ple_quota_cfg(struct mac_ax_adapter *adapter,
		   struct ple_quota_t *min_cfg,
		   struct ple_quota_t *max_cfg)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = SET_WORD(min_cfg->cma0_tx, B_AX_PLE_Q0_MIN_SIZE) |
		SET_WORD(max_cfg->cma0_tx, B_AX_PLE_Q0_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA0_CFG, val32);

	val32 = SET_WORD(min_cfg->cma1_tx, B_AX_PLE_Q1_MIN_SIZE) |
		SET_WORD(max_cfg->cma1_tx, B_AX_PLE_Q1_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA1_CFG, val32);

	val32 = SET_WORD(min_cfg->c2h, B_AX_PLE_Q2_MIN_SIZE) |
		SET_WORD(max_cfg->c2h, B_AX_PLE_Q2_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA2_CFG, val32);

	val32 = SET_WORD(min_cfg->h2c, B_AX_PLE_Q3_MIN_SIZE) |
		SET_WORD(max_cfg->h2c, B_AX_PLE_Q3_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA3_CFG, val32);

	val32 = SET_WORD(min_cfg->wcpu, B_AX_PLE_Q4_MIN_SIZE) |
		SET_WORD(max_cfg->wcpu, B_AX_PLE_Q4_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA4_CFG, val32);

	val32 = SET_WORD(min_cfg->mpdu_proc, B_AX_PLE_Q5_MIN_SIZE) |
		SET_WORD(max_cfg->mpdu_proc, B_AX_PLE_Q5_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA5_CFG, val32);

	val32 = SET_WORD(min_cfg->cma0_dma, B_AX_PLE_Q6_MIN_SIZE) |
		SET_WORD(max_cfg->cma0_dma, B_AX_PLE_Q6_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA6_CFG, val32);

	val32 = SET_WORD(min_cfg->cma1_dma, B_AX_PLE_Q7_MIN_SIZE) |
		SET_WORD(max_cfg->cma1_dma, B_AX_PLE_Q7_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA7_CFG, val32);

	val32 = SET_WORD(min_cfg->bb_rpt, B_AX_PLE_Q8_MIN_SIZE) |
		SET_WORD(max_cfg->bb_rpt, B_AX_PLE_Q8_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA8_CFG, val32);

	val32 = SET_WORD(min_cfg->wd_rel, B_AX_PLE_Q9_MIN_SIZE) |
		SET_WORD(max_cfg->wd_rel, B_AX_PLE_Q9_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA9_CFG, val32);

	val32 = SET_WORD(min_cfg->cpu_io, B_AX_PLE_Q10_MIN_SIZE) |
		SET_WORD(max_cfg->cpu_io, B_AX_PLE_Q10_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA10_CFG, val32);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = SET_WORD(min_cfg->tx_rpt, B_AX_PLE_Q11_MIN_SIZE) |
			SET_WORD(max_cfg->tx_rpt, B_AX_PLE_Q11_MAX_SIZE);
		MAC_REG_W32(R_AX_PLE_QTA11_CFG, val32);
	}
}

void dle_quota_cfg(struct mac_ax_adapter *adapter, struct dle_mem_t *cfg)
{
	wde_quota_cfg(adapter, cfg->wde_min_qt, cfg->wde_max_qt);
	ple_quota_cfg(adapter, cfg->ple_min_qt, cfg->ple_max_qt);
}

u32 dle_quota_change(struct mac_ax_adapter *adapter, enum mac_ax_qta_mode mode)
{
	u32 ret = MACSUCCESS;
	struct dle_mem_t *cfg;
	struct cpuio_buf_req_t buf_req;
	struct cpuio_ctrl_t ctrl_para;
	u32 val32_1, val32_2;

	cfg = get_dle_mem_cfg(adapter, mode);
	if (!cfg) {
		PLTFM_MSG_ERR("[ERR]wd/dle mem cfg\n");
		return MACNOITEM;
	}

	val32_1 = dle_used_size(cfg->wde_size, cfg->ple_size);
	val32_2 = adapter->hw_info->fifo_size - dle_rsvd_size(adapter, mode);
	if (val32_1 != val32_2) {
		PLTFM_MSG_ERR("[ERR]dle used size %d not match %d\n",
			      val32_1, val32_2);
		return MACFFCFG;
	}

	dle_quota_cfg(adapter, cfg);

	//Trigger change by enqueue packet
	// Use CPUIO temporarily.
	//WD
	buf_req.len = 0x20; // chris comment
	ret = mac_dle_buf_req_wd(adapter, &buf_req);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]WDE DLE buf req %d\n", ret);
		return ret;
	}

	PLTFM_MEMSET((void *)&ctrl_para, 0, sizeof(ctrl_para));
	ctrl_para.cmd_type = CPUIO_OP_CMD_ENQ_TO_HEAD;
	ctrl_para.start_pktid = buf_req.pktid;
	ctrl_para.end_pktid = buf_req.pktid;
	ctrl_para.pkt_num = 0;
	ctrl_para.dst_pid = WDE_DLE_PORT_ID_WDRLS;
	ctrl_para.dst_qid = WDE_DLE_QUEID_NO_REPORT;
	ret = mac_set_cpuio_wd(adapter, &ctrl_para);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]WDE DLE enqueue to head %d\n", ret);
		return ret;
	}

	//PL
	buf_req.len = 0x20;
	ret = mac_dle_buf_req_pl(adapter, &buf_req);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]PLE DLE buf req %d\n", ret);
		return ret;
	}

	PLTFM_MEMSET((void *)&ctrl_para, 0, sizeof(ctrl_para));
	ctrl_para.cmd_type = CPUIO_OP_CMD_ENQ_TO_HEAD;
	ctrl_para.start_pktid = buf_req.pktid;
	ctrl_para.end_pktid = buf_req.pktid;
	ctrl_para.pkt_num = 0;
	ctrl_para.dst_pid = PLE_DLE_PORT_ID_PLRLS;
	ctrl_para.dst_qid = PLE_DLE_QUEID_NO_REPORT;
	ret = mac_set_cpuio_pl(adapter, &ctrl_para);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]PLE DLE enqueue to head %d\n", ret);
		return ret;
	}
	return ret;
}

u32 dle_init(struct mac_ax_adapter *adapter, enum mac_ax_qta_mode mode,
	     enum mac_ax_qta_mode ext_mode)
{
	u32 ret = MACSUCCESS;
	u32 cnt, val32_1, val32_2;
	struct dle_mem_t *cfg, *ext_cfg;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct dle_mem_t cfg_tmp;
	struct dle_size_t wde_size_tmp, ple_size_tmp;
	struct wde_quota_t wde_min_qt_tmp, wde_max_qt_tmp;
	struct ple_quota_t ple_min_qt_tmp, ple_max_qt_tmp;

	ret = check_mac_en(adapter, 0, MAC_AX_DMAC_SEL);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]chk dmac en %d\n", ret);
		return ret;
	}

	cfg = get_dle_mem_cfg(adapter, mode);
	if (!cfg) {
		ret = MACNOITEM;
		PLTFM_MSG_ERR("[ERR]get_dle_mem_cfg %d\n", mode);
		goto error;
	}

	PLTFM_MEMCPY(&wde_size_tmp, cfg->wde_size, sizeof(struct dle_size_t));
	PLTFM_MEMCPY(&ple_size_tmp, cfg->ple_size, sizeof(struct dle_size_t));
	PLTFM_MEMCPY(&wde_min_qt_tmp, cfg->wde_min_qt, sizeof(struct wde_quota_t));
	PLTFM_MEMCPY(&wde_max_qt_tmp, cfg->wde_max_qt, sizeof(struct wde_quota_t));
	PLTFM_MEMCPY(&ple_min_qt_tmp, cfg->ple_min_qt, sizeof(struct ple_quota_t));
	PLTFM_MEMCPY(&ple_max_qt_tmp, cfg->ple_max_qt, sizeof(struct ple_quota_t));
	cfg_tmp.mode = cfg->mode;
	cfg_tmp.wde_size = &wde_size_tmp;
	cfg_tmp.ple_size = &ple_size_tmp;
	cfg_tmp.wde_min_qt = &wde_min_qt_tmp;
	cfg_tmp.wde_max_qt = &wde_max_qt_tmp;
	cfg_tmp.ple_min_qt = &ple_min_qt_tmp;
	cfg_tmp.ple_max_qt = &ple_max_qt_tmp;
	cfg = &cfg_tmp;

	if (mode == MAC_AX_QTA_DLFW) {
		ext_cfg = get_dle_mem_cfg(adapter, ext_mode);
		if (!ext_cfg) {
			ret = MACNOITEM;
			PLTFM_MSG_ERR("[ERR]get_dle_ext_mem_cfg %d\n",
				      ext_mode);
			goto error;
		}

		cfg->wde_min_qt->wcpu = ext_cfg->wde_min_qt->wcpu;
	}

	val32_1 = dle_used_size(cfg->wde_size, cfg->ple_size);
	val32_2 = adapter->hw_info->fifo_size - dle_rsvd_size(adapter, mode);
	if (val32_1 != val32_2) {
		PLTFM_MSG_ERR("[ERR]dle init used size %d not match %d\n",
			      val32_1, val32_2);
		ret = MACFFCFG;
		goto error;
	}

	dle_func_en(adapter, MAC_AX_FUNC_DIS);
	dle_clk_en(adapter, MAC_AX_FUNC_EN);

	ret = dle_mix_cfg(adapter, cfg);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR] dle mix cfg %d\n", ret);
		goto error;
	}
	dle_quota_cfg(adapter, cfg);

	dle_func_en(adapter, MAC_AX_FUNC_EN);

	cnt = DLE_WAIT_CNT;
	while (cnt--) {
		if ((MAC_REG_R32(R_AX_WDE_INI_STATUS) & WDE_MGN_INI_RDY)
		    == WDE_MGN_INI_RDY)
			break;
		PLTFM_DELAY_US(DLE_WAIT_US);
	}

	if (!++cnt) {
		PLTFM_MSG_ERR("[ERR]WDE cfg ready timeout\n");
		return MACPOLLTO;
	}

	cnt = DLE_WAIT_CNT;
	while (cnt--) {
		if ((MAC_REG_R32(R_AX_PLE_INI_STATUS) & PLE_MGN_INI_RDY)
		    == PLE_MGN_INI_RDY)
			break;
		PLTFM_DELAY_US(DLE_WAIT_US);
	}

	if (!++cnt) {
		PLTFM_MSG_ERR("[ERR]PLE cfg ready timeout\n");
		return MACPOLLTO;
	}

	return ret;
error:
	dle_func_en(adapter, MAC_AX_FUNC_DIS);
	PLTFM_MSG_ERR("[ERR]trxcfg wde 0x8900 = %x\n",
		      MAC_REG_R32(R_AX_WDE_INI_STATUS));
	PLTFM_MSG_ERR("[ERR]trxcfg ple 0x8D00 = %x\n",
		      MAC_REG_R32(R_AX_PLE_INI_STATUS));

	return ret;
}

u32 is_qta_dbcc(struct mac_ax_adapter *adapter, enum mac_ax_qta_mode mode,
		u8 *is_dbcc)
{
	struct dle_mem_t *cfg;

	cfg = get_dle_mem_cfg(adapter, mode);
	if (!cfg) {
		PLTFM_MSG_ERR("[ERR]get_dle_mem_cfg\n");
		return MACNOITEM;
	}

	*is_dbcc = (cfg->ple_min_qt->cma1_dma && cfg->ple_max_qt->cma1_dma) ?
		   1 : 0;

	return MACSUCCESS;
}

u8 is_curr_dbcc(struct mac_ax_adapter *adapter)
{
	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		return 0;

	return adapter->dle_info.c1_rx_qta ? 1 : 0;
}

u32 is_qta_poh(struct mac_ax_adapter *adapter, enum mac_ax_qta_mode mode,
	       u8 *is_poh)
{
	*is_poh = (mode == MAC_AX_QTA_SCC_STF || mode == MAC_AX_QTA_DBCC_STF ||
		   adapter->hw_info->intf != MAC_AX_INTF_PCIE) ? 0 : 1;

	return MACSUCCESS;
}

u32 _patch_redu_rx_qta(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u16 new_qta, qta_min, qta_max, rdu_pg_num;

	if (!(is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	      is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)))
		return MACSUCCESS;

	val32 = GET_FIELD(MAC_REG_R32(R_AX_PLE_PKTBUF_CFG), B_AX_PLE_PAGE_SEL);
	if (val32 == S_AX_PLE_PAGE_SEL_128) {
		rdu_pg_num = PLE_QTA_PG128B_12KB;
	} else if (val32 == S_AX_PLE_PAGE_SEL_256) {
		rdu_pg_num = PLE_QTA_PG128B_12KB / 2;
	} else {
		PLTFM_MSG_ERR("[ERR]PLE page sel %d unsupport\n", val32);
		return MACHWERR;
	}

	val32 = MAC_REG_R32(R_AX_PLE_QTA6_CFG);
	if (!val32) {
		PLTFM_MSG_ERR("[ERR]no rx 0 qta\n");
		return MACHWERR;
	}
	qta_min = GET_FIELD(val32, B_AX_PLE_Q6_MIN_SIZE);
	qta_max = GET_FIELD(val32, B_AX_PLE_Q6_MAX_SIZE);

	adapter->dle_info.c0_ori_max = qta_max;

	new_qta = (qta_max - qta_min) < rdu_pg_num ?
		  qta_min : (qta_max - rdu_pg_num);
	val32 = SET_CLR_WORD(val32, new_qta, B_AX_PLE_Q6_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA6_CFG, val32);

	val32 = MAC_REG_R32(R_AX_PLE_QTA7_CFG);
	if (!val32)
		return MACSUCCESS;
	qta_min = GET_FIELD(val32, B_AX_PLE_Q7_MIN_SIZE);
	qta_max = GET_FIELD(val32, B_AX_PLE_Q7_MAX_SIZE);

	adapter->dle_info.c1_ori_max = qta_max;

	new_qta = (qta_max - qta_min) < rdu_pg_num ?
		  qta_min : (qta_max - rdu_pg_num);
	val32 = SET_CLR_WORD(val32, new_qta, B_AX_PLE_Q7_MAX_SIZE);
	MAC_REG_W32(R_AX_PLE_QTA7_CFG, val32);

	return MACSUCCESS;
}

u32 _patch_restr_rx_qta(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32, w_val32;

	if (!(is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	      is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)))
		return MACSUCCESS;

	val32 = MAC_REG_R32(R_AX_PLE_QTA6_CFG);
	if (!val32) {
		PLTFM_MSG_ERR("[ERR]no rx 0 qta\n");
		return MACHWERR;
	}
	w_val32 = SET_CLR_WORD(val32, adapter->dle_info.c0_ori_max,
			       B_AX_PLE_Q6_MAX_SIZE);
	if (w_val32 != val32)
		MAC_REG_W32(R_AX_PLE_QTA6_CFG, w_val32);

	val32 = MAC_REG_R32(R_AX_PLE_QTA7_CFG);
	if (!val32)
		return MACSUCCESS;
	w_val32 = SET_CLR_WORD(val32, adapter->dle_info.c1_ori_max,
			       B_AX_PLE_Q7_MAX_SIZE);
	if (w_val32 != val32)
		MAC_REG_W32(R_AX_PLE_QTA7_CFG, w_val32);

	return MACSUCCESS;
}

