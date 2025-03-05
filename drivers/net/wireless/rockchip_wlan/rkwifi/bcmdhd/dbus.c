/** @file dbus.c
 *
 * Hides details of USB / SDIO / SPI interfaces and OS details. It is intended to shield details and
 * provide the caller with one common bus interface for all dongle devices. In practice, it is only
 * used for USB interfaces. DBUS is not a protocol, but an abstraction layer.
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

#include <linux/usb.h>
#include "osl.h"
#include "dbus.h"
#include <bcmutils.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_proto.h>
#ifdef PROP_TXSTATUS /* a form of flow control between host and dongle */
#include <dhd_wlfc.h>
#endif
#include <dhd_config.h>
#ifdef WL_CFG80211
#include <wl_cfg80211.h>
#include <wl_cfgp2p.h>
#endif

#include <bcmdevs_legacy.h>
#if defined(BCM_DNGL_EMBEDIMAGE)
#include <bcmsrom_fmt.h>
#include <trxhdr.h>
#include <usbrdl.h>
#include <bcmendian.h>
#include <zutil.h>
#include <sbpcmcia.h>
#include <bcmnvram.h>
#include <bcmdevs.h>
#elif defined(BCM_REQUEST_FW)
#include <bcmsrom_fmt.h>
#include <trxhdr.h>
#include <usbrdl.h>
#include <bcmendian.h>
#include <sbpcmcia.h>
#include <bcmnvram.h>
#include <bcmdevs.h>
#endif /* #if defined(BCM_DNGL_EMBEDIMAGE) */
#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
#include <linux/usb.h>
#endif /* EHCI_FASTPATH_TX || EHCI_FASTPATH_RX */

#if defined(BCM_DNGL_EMBEDIMAGE)
/* zlib file format field ids etc from gzio.c */
#define Z_DEFLATED     8
#define ASCII_FLAG     0x01 /**< bit 0 set: file probably ascii text */
#define HEAD_CRC       0x02 /**< bit 1 set: header CRC present */
#define EXTRA_FIELD    0x04 /**< bit 2 set: extra field present */
#define ORIG_NAME      0x08 /**< bit 3 set: original file name present */
#define COMMENT        0x10 /**< bit 4 set: file comment present */
#define RESERVED       0xE0 /**< bits 5..7: reserved */

/* Rename define */
#ifdef WL_FW_DECOMP
#define UNZIP_ENAB(info)  1
#else
#define UNZIP_ENAB(info)  0

#ifdef inflateInit2
#undef inflateInit2
#define inflateInit2(a, b)  Z_ERRNO
#endif
#define inflate(a, b)       Z_STREAM_ERROR
#define inflateEnd(a)       do {} while (0)
#define crc32(a, b, c)      -1
#define free(a)             do {} while (0)
#endif /* WL_FW_DECOMP */

#elif defined(BCM_REQUEST_FW)
#ifndef VARS_MAX
#define VARS_MAX            8192
#endif
#endif /* #if defined(BCM_DNGL_EMBEDIMAGE) */

#ifdef DBUS_USB_LOOPBACK
extern bool is_loopback_pkt(void *buf);
extern int matches_loopback_pkt(void *buf);
#endif

/** General info for all BUS types */
typedef struct dbus_irbq {
	dbus_irb_t *head;
	dbus_irb_t *tail;
	int cnt;
} dbus_irbq_t;

/**
 * This private structure dhd_bus_t is also declared in dbus_usb_linux.c.
 * All the fields must be consistent in both declarations.
 */
typedef struct dhd_bus {
	dbus_pub_t   pub; /* MUST BE FIRST */
	dhd_pub_t *dhd;

	void        *cbarg;
	dbus_callbacks_t *cbs; /**< callbacks to higher level, eg dhd_linux.c */
	void        *bus_info;
	dbus_intf_t *drvintf;  /**< callbacks to lower level, eg dbus_usb.c or dbus_usb_linux.c */
	uint8       *fw;
	int         fwlen;
	uint32      errmask;
	int         rx_low_watermark;  /**< avoid rx overflow by filling rx with free IRBs */
	int         tx_low_watermark;
	bool        txoff;
	bool        txoverride;   /**< flow control related */
	bool        rxoff;
	bool        tx_timer_ticking;
	uint ctl_completed;

	dbus_irbq_t *rx_q;
	dbus_irbq_t *tx_q;

#ifdef BCMDBG
	int         *txpend_q_hist;
	int         *rxpend_q_hist;
#endif /* BCMDBG */
#ifdef EHCI_FASTPATH_RX
	atomic_t    rx_outstanding;
#endif
	uint8        *nvram;
	int          nvram_len;
	uint8        *image;  /**< buffer for combine fw and nvram */
	int          image_len;
	uint8        *orig_fw;
	int          origfw_len;
	int          decomp_memsize;
	dbus_extdl_t extdl;
	int          nvram_nontxt;
#if defined(BCM_REQUEST_FW)
	void         *firmware;
	void         *nvfile;
#endif
	char		*fw_path;		/* module_param: path to firmware image */
	char		*nv_path;		/* module_param: path to nvram vars file */
	uint64 last_suspend_end_time;
} dhd_bus_t;

struct exec_parms {
	union {
		/* Can consolidate same params, if need be, but this shows
		 * group of parameters per function
		 */
		struct {
			dbus_irbq_t  *q;
			dbus_irb_t   *b;
		} qenq;

		struct {
			dbus_irbq_t  *q;
		} qdeq;
	};
};

#define EXEC_RXLOCK(info, fn, a) \
	info->drvintf->exec_rxlock(dhd_bus->bus_info, ((exec_cb_t)fn), ((struct exec_parms *) a))

#define EXEC_TXLOCK(info, fn, a) \
	info->drvintf->exec_txlock(dhd_bus->bus_info, ((exec_cb_t)fn), ((struct exec_parms *) a))

/*
 * Callbacks common for all BUS
 */
static void dbus_if_send_irb_timeout(void *handle, dbus_irb_tx_t *txirb);
static void dbus_if_send_irb_complete(void *handle, dbus_irb_tx_t *txirb, int status);
static void dbus_if_recv_irb_complete(void *handle, dbus_irb_rx_t *rxirb, int status);
static void dbus_if_errhandler(void *handle, int err);
static void dbus_if_ctl_complete(void *handle, int type, int status);
static void dbus_if_state_change(void *handle, int state);
static void *dbus_if_pktget(void *handle, uint len, bool send);
static void dbus_if_pktfree(void *handle, void *p, bool send);
static struct dbus_irb *dbus_if_getirb(void *cbarg, bool send);
static void dbus_if_rxerr_indicate(void *handle, bool on);

static int dbus_suspend(void *context);
static int dbus_resume(void *context);
static void * dhd_dbus_probe_cb(uint16 bus_no, uint16 slot, uint32 hdrlen);
static void dhd_dbus_disconnect_cb(void *arg);
static void dbus_detach(dhd_bus_t *pub);

/** functions in this file that are called by lower DBUS levels, eg dbus_usb.c */
static dbus_intf_callbacks_t dbus_intf_cbs = {
	dbus_if_send_irb_timeout,
	dbus_if_send_irb_complete,
	dbus_if_recv_irb_complete,
	dbus_if_errhandler,
	dbus_if_ctl_complete,
	dbus_if_state_change,
	NULL,			/**< isr */
	NULL,			/**< dpc */
	NULL,			/**< watchdog */
	dbus_if_pktget,
	dbus_if_pktfree,
	dbus_if_getirb,
	dbus_if_rxerr_indicate
};

/*
 * Need global for probe() and disconnect() since
 * attach() is not called at probe and detach()
 * can be called inside disconnect()
 */
static dbus_intf_t     *g_busintf = NULL;

#if defined(BCM_REQUEST_FW)
int8 *nonfwnvram = NULL; /**< stand-alone multi-nvram given with driver load */
int nonfwnvramlen = 0;
#endif /* #if defined(BCM_REQUEST_FW) */

static void* q_enq(dbus_irbq_t *q, dbus_irb_t *b);
static void* q_enq_exec(struct exec_parms *args);
static dbus_irb_t*q_deq(dbus_irbq_t *q);
static void* q_deq_exec(struct exec_parms *args);
static int   dbus_tx_timer_init(dhd_bus_t *dhd_bus);
static int   dbus_tx_timer_start(dhd_bus_t *dhd_bus, uint timeout);
static int   dbus_tx_timer_stop(dhd_bus_t *dhd_bus);
static int   dbus_irbq_init(dhd_bus_t *dhd_bus, dbus_irbq_t *q, int nq, int size_irb);
static int   dbus_irbq_deinit(dhd_bus_t *dhd_bus, dbus_irbq_t *q, int size_irb);
static int   dbus_rxirbs_fill(dhd_bus_t *dhd_bus);
static int   dbus_send_irb(dbus_pub_t *pub, uint8 *buf, int len, void *pkt, void *info);

#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW))
#if defined(BCM_REQUEST_FW)
extern char * dngl_firmware;
extern unsigned int dngl_fwlen;
#endif  /* #if defined(BCM_REQUEST_FW) */
#ifndef EXTERNAL_FW_PATH
static int dbus_get_nvram(dhd_bus_t *dhd_bus);
static int dbus_jumbo_nvram(dhd_bus_t *dhd_bus);
static int dbus_select_nvram(dhd_bus_t *dhd_bus, int8 *jumbonvram, int jumbolen,
uint16 boardtype, uint16 boardrev, int8 **nvram, int *nvram_len);
#endif /* !EXTERNAL_FW_PATH */
#ifndef BCM_REQUEST_FW
static int dbus_zlib_decomp(dhd_bus_t *dhd_bus);
extern void *dbus_zlib_calloc(int num, int size);
extern void dbus_zlib_free(void *ptr);
#endif
#endif /* defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW) */

/* function */
void
dbus_flowctrl_tx(void *dbi, bool on)
{
	dhd_bus_t *dhd_bus = dbi;

	if (dhd_bus == NULL)
		return;

	DBUSTRACE(("%s on %d\n", __FUNCTION__, on));

	if (dhd_bus->txoff == on)
		return;

	dhd_bus->txoff = on;

	if (dhd_bus->cbs && dhd_bus->cbs->txflowcontrol)
		dhd_bus->cbs->txflowcontrol(dhd_bus->cbarg, on);
}

/**
 * if lower level DBUS signaled a rx error, more free rx IRBs should be allocated or flow control
 * should kick in to make more free rx IRBs available.
 */
static void
dbus_if_rxerr_indicate(void *handle, bool on)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;

	DBUSTRACE(("%s, on %d\n", __FUNCTION__, on));

	if (dhd_bus == NULL)
		return;

	if (dhd_bus->txoverride == on)
		return;

	dhd_bus->txoverride = on;	/* flow control */

	if (!on)
		dbus_rxirbs_fill(dhd_bus);

}

/** q_enq()/q_deq() are executed with protection via exec_rxlock()/exec_txlock() */
static void*
q_enq(dbus_irbq_t *q, dbus_irb_t *b)
{
	ASSERT(q->tail != b);
	ASSERT(b->next == NULL);
	b->next = NULL;
	if (q->tail) {
		q->tail->next = b;
		q->tail = b;
	} else
		q->head = q->tail = b;

	q->cnt++;

	return b;
}

static void*
q_enq_exec(struct exec_parms *args)
{
	return q_enq(args->qenq.q, args->qenq.b);
}

static dbus_irb_t*
q_deq(dbus_irbq_t *q)
{
	dbus_irb_t *b;

	b = q->head;
	if (b) {
		q->head = q->head->next;
		b->next = NULL;

		if (q->head == NULL)
			q->tail = q->head;

		q->cnt--;
	}
	return b;
}

static void*
q_deq_exec(struct exec_parms *args)
{
	return q_deq(args->qdeq.q);
}

/**
 * called during attach phase. Status @ Dec 2012: this function does nothing since for all of the
 * lower DBUS levels dhd_bus->drvintf->tx_timer_init is NULL.
 */
static int
dbus_tx_timer_init(dhd_bus_t *dhd_bus)
{
	if (dhd_bus && dhd_bus->drvintf && dhd_bus->drvintf->tx_timer_init)
		return dhd_bus->drvintf->tx_timer_init(dhd_bus->bus_info);
	else
		return DBUS_ERR;
}

static int
dbus_tx_timer_start(dhd_bus_t *dhd_bus, uint timeout)
{
	if (dhd_bus == NULL)
		return DBUS_ERR;

	if (dhd_bus->tx_timer_ticking)
		return DBUS_OK;

	if (dhd_bus->drvintf && dhd_bus->drvintf->tx_timer_start) {
		if (dhd_bus->drvintf->tx_timer_start(dhd_bus->bus_info, timeout) == DBUS_OK) {
			dhd_bus->tx_timer_ticking = TRUE;
			return DBUS_OK;
		}
	}

	return DBUS_ERR;
}

static int
dbus_tx_timer_stop(dhd_bus_t *dhd_bus)
{
	if (dhd_bus == NULL)
		return DBUS_ERR;

	if (!dhd_bus->tx_timer_ticking)
		return DBUS_OK;

	if (dhd_bus->drvintf && dhd_bus->drvintf->tx_timer_stop) {
		if (dhd_bus->drvintf->tx_timer_stop(dhd_bus->bus_info) == DBUS_OK) {
			dhd_bus->tx_timer_ticking = FALSE;
			return DBUS_OK;
		}
	}

	return DBUS_ERR;
}

/** called during attach phase. */
static int
dbus_irbq_init(dhd_bus_t *dhd_bus, dbus_irbq_t *q, int nq, int size_irb)
{
	int i;
	dbus_irb_t *irb;

	ASSERT(q);
	ASSERT(dhd_bus);

	for (i = 0; i < nq; i++) {
		/* MALLOCZ dbus_irb_tx or dbus_irb_rx, but cast to simple dbus_irb_t linkedlist */
		irb = (dbus_irb_t *) MALLOCZ(dhd_bus->pub.osh, size_irb);
		if (irb == NULL) {
			ASSERT(irb);
			return DBUS_ERR;
		}

		/* q_enq() does not need to go through EXEC_xxLOCK() during init() */
		q_enq(q, irb);
	}

	return DBUS_OK;
}

/** called during detach phase or when attach failed */
static int
dbus_irbq_deinit(dhd_bus_t *dhd_bus, dbus_irbq_t *q, int size_irb)
{
	dbus_irb_t *irb;

	ASSERT(q);
	ASSERT(dhd_bus);

	/* q_deq() does not need to go through EXEC_xxLOCK()
	 * during deinit(); all callbacks are stopped by this time
	 */
	while ((irb = q_deq(q)) != NULL) {
		MFREE(dhd_bus->pub.osh, irb, size_irb);
	}

	if (q->cnt)
		DBUSERR(("deinit: q->cnt=%d > 0\n", q->cnt));
	return DBUS_OK;
}

/** multiple code paths require the rx queue to be filled with more free IRBs */
static int
dbus_rxirbs_fill(dhd_bus_t *dhd_bus)
{
	int err = DBUS_OK;

#ifdef EHCI_FASTPATH_RX
	while (atomic_read(&dhd_bus->rx_outstanding) < 100) /* TODO: Improve constant */
	{
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		/* NOCOPY force new packet allocation */
		optimize_submit_rx_request(&dhd_bus->pub, 1, NULL, NULL);
#else
		/* Copy mode - allocate own buffer to be reused */
		void *buf = MALLOCZ(dhd_bus->pub.osh, 4000); /* usbos_info->rxbuf_len */
		optimize_submit_rx_request(&dhd_bus->pub, 1, NULL, buf);
		/* ME: Need to check result and set err = DBUS_ERR_RXDROP */
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY */
		atomic_inc(&dhd_bus->rx_outstanding);
	}
#else /* EHCI_FASTPATH_RX */

	dbus_irb_rx_t *rxirb;
	struct exec_parms args;

	ASSERT(dhd_bus);
	if (dhd_bus->pub.busstate != DBUS_STATE_UP) {
		DBUSERR(("dbus_rxirbs_fill: DBUS not up \n"));
		return DBUS_ERR;
	} else if (!dhd_bus->drvintf || (dhd_bus->drvintf->recv_irb == NULL)) {
		/* Lower edge bus interface does not support recv_irb().
		 * No need to pre-submit IRBs in this case.
		 */
		return DBUS_ERR;
	}

	/* The dongle recv callback is freerunning without lock. So multiple callbacks(and this
	 *  refill) can run in parallel. While the rxoff condition is triggered outside,
	 *  below while loop has to check and abort posting more to avoid RPC rxq overflow.
	 */
	args.qdeq.q = dhd_bus->rx_q;
	while ((!dhd_bus->rxoff) &&
	       (rxirb = (EXEC_RXLOCK(dhd_bus, q_deq_exec, &args))) != NULL) {
		err = dhd_bus->drvintf->recv_irb(dhd_bus->bus_info, rxirb);
		if (err == DBUS_ERR_RXDROP || err == DBUS_ERR_RXFAIL) {
			/* Add the the free rxirb back to the queue
			 * and wait till later
			 */
			bzero(rxirb, sizeof(dbus_irb_rx_t));
			args.qenq.q = dhd_bus->rx_q;
			args.qenq.b = (dbus_irb_t *) rxirb;
			EXEC_RXLOCK(dhd_bus, q_enq_exec, &args);
			break;
		} else if (err != DBUS_OK) {
			int i = 0;
			while (i++ < 100) {
				DBUSERR(("%s :: memory leak for rxirb note?\n", __FUNCTION__));
			}
		}
	}
#endif /* EHCI_FASTPATH_RX */
	return err;
} /* dbus_rxirbs_fill */

/** called when the DBUS interface state changed. */
void
dbus_flowctrl_rx(dbus_pub_t *pub, bool on)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	if (dhd_bus == NULL)
		return;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus->rxoff == on)
		return;

	dhd_bus->rxoff = on;

	if (dhd_bus->pub.busstate == DBUS_STATE_UP) {
		if (!on) {
			/* post more irbs, resume rx if necessary */
			dbus_rxirbs_fill(dhd_bus);
			if (dhd_bus && dhd_bus->drvintf->recv_resume) {
				dhd_bus->drvintf->recv_resume(dhd_bus->bus_info);
			}
		} else {
			/* ??? cancell posted irbs first */

			if (dhd_bus && dhd_bus->drvintf->recv_stop) {
				dhd_bus->drvintf->recv_stop(dhd_bus->bus_info);
			}
		}
	}
}

/**
 * Several code paths in this file want to send a buffer to the dongle. This function handles both
 * sending of a buffer or a pkt.
 */
static int
dbus_send_irb(dbus_pub_t *pub, uint8 *buf, int len, void *pkt, void *info)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_OK;
#ifndef EHCI_FASTPATH_TX
	dbus_irb_tx_t *txirb = NULL;
	int txirb_pending;
	struct exec_parms args;
#endif /* EHCI_FASTPATH_TX */

	if (dhd_bus == NULL)
		return DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus->pub.busstate == DBUS_STATE_UP ||
		dhd_bus->pub.busstate == DBUS_STATE_SLEEP) {
#ifdef EHCI_FASTPATH_TX
		struct ehci_qtd *qtd;
		int token = EHCI_QTD_SET_CERR(3);
		int len;

		ASSERT(buf == NULL); /* Not handled */
		ASSERT(pkt != NULL);

		qtd = optimize_ehci_qtd_alloc(GFP_KERNEL);

		if (qtd == NULL)
			return DBUS_ERR;

		len = PKTLEN(pub->osh, pkt);

		len = ROUNDUP(len, sizeof(uint32));

#ifdef BCMDBG
		/* The packet length is already padded to not to be multiple of 512 bytes
		 * in bcm_rpc_tp_buf_send_internal(), so it should not be 512*N bytes here.
		 */
		if (len % EHCI_BULK_PACKET_SIZE == 0) {
			DBUSERR(("%s: len = %d (multiple of 512 bytes)\n", __FUNCTION__, len));
			return DBUS_ERR_TXDROP;
		}
#endif /* BCMDBG */

		optimize_qtd_fill_with_rpc(pub, 0, qtd, pkt, token, len);
		err = optimize_submit_async(qtd, 0);

		if (err) {
			optimize_ehci_qtd_free(qtd);
			err = DBUS_ERR_TXDROP;
		}

		/* ME: Add timeout? */
#else
		args.qdeq.q = dhd_bus->tx_q;
		if (dhd_bus->drvintf)
			txirb = EXEC_TXLOCK(dhd_bus, q_deq_exec, &args);

		if (txirb == NULL) {
			DBUSERR(("Out of tx dbus_bufs\n"));
			return DBUS_ERR;
		}

		if (pkt != NULL) {
			txirb->pkt = pkt;
			txirb->buf = NULL;
			txirb->len = 0;
		} else if (buf != NULL) {
			txirb->pkt = NULL;
			txirb->buf = buf;
			txirb->len = len;
		} else {
			ASSERT(0); /* Should not happen */
		}
		txirb->info = info;
		txirb->arg = NULL;
		txirb->retry_count = 0;

		if (dhd_bus->drvintf && dhd_bus->drvintf->send_irb) {
			/* call lower DBUS level send_irb function */
			err = dhd_bus->drvintf->send_irb(dhd_bus->bus_info, txirb);
			if (err == DBUS_ERR_TXDROP) {
				/* tx fail and no completion routine to clean up, reclaim irb NOW */
				DBUSERR(("%s: send_irb failed, status = %d\n", __FUNCTION__, err));
				bzero(txirb, sizeof(dbus_irb_tx_t));
				args.qenq.q = dhd_bus->tx_q;
				args.qenq.b = (dbus_irb_t *) txirb;
				EXEC_TXLOCK(dhd_bus, q_enq_exec, &args);
			} else {
				dbus_tx_timer_start(dhd_bus, DBUS_TX_TIMEOUT_INTERVAL);
				txirb_pending = dhd_bus->pub.ntxq - dhd_bus->tx_q->cnt;
#ifdef BCMDBG
				dhd_bus->txpend_q_hist[txirb_pending]++;
#endif /* BCMDBG */
				if (txirb_pending > (dhd_bus->tx_low_watermark * 3)) {
					dbus_flowctrl_tx(dhd_bus, TRUE);
				}
			}
		}
#endif /* EHCI_FASTPATH_TX */
	} else {
		err = DBUS_ERR_TXFAIL;
		DBUSTRACE(("%s: bus down, send_irb failed\n", __FUNCTION__));
	}

	return err;
} /* dbus_send_irb */

#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW))

/**
 * Before downloading a firmware image into the dongle, the validity of the image must be checked.
 */
static int
check_file(osl_t *osh, unsigned char *headers)
{
	struct trx_header *trx;
	int actual_len = -1;

	/* Extract trx header */
	trx = (struct trx_header *)headers;
	if (ltoh32(trx->magic) != TRX_MAGIC) {
		printf("Error: trx bad hdr %x\n", ltoh32(trx->magic));
		return -1;
	}

	headers += SIZEOF_TRX(trx);

	/* TRX V1: get firmware len */
	/* TRX V2: get firmware len and DSG/CFG lengths */
	if (ltoh32(trx->flag_version) & TRX_UNCOMP_IMAGE) {
		actual_len = ltoh32(trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]) +
		                     SIZEOF_TRX(trx);
#ifdef BCMTRXV2
		if (ISTRX_V2(trx)) {
			actual_len += ltoh32(trx->offsets[TRX_OFFSETS_DSG_LEN_IDX]) +
				ltoh32(trx->offsets[TRX_OFFSETS_CFG_LEN_IDX]);
		}
#endif
		return actual_len;
	}  else {
		printf("compressed image\n");
	}

	return -1;
}

#ifdef EXTERNAL_FW_PATH
static int
dbus_get_fw_nvram(dhd_bus_t *dhd_bus)
{
	int bcmerror = -1, i;
	uint len, total_len;
	void *nv_image = NULL, *fw_image = NULL;
	char *nv_memblock = NULL, *fw_memblock = NULL;
	char *bufp;
	bool file_exists;
	uint8 nvram_words_pad = 0;
	uint memblock_size = 2048;
	uint8 *memptr;
	int	actual_fwlen;
	struct trx_header *hdr;
	uint32 img_offset = 0;
	int offset = 0;
	char *pfw_path = dhd_bus->fw_path;
	char *pnv_path = dhd_bus->nv_path;

	/* For Get nvram */
	file_exists = ((pnv_path != NULL) && (pnv_path[0] != '\0'));
	if (file_exists) {
		nv_image = dhd_os_open_image1(dhd_bus->dhd, pnv_path);
		if (nv_image == NULL) {
			printf("%s: Open nvram file failed %s\n", __FUNCTION__, pnv_path);
			goto err;
		}
	}
	nv_memblock = MALLOC(dhd_bus->pub.osh, MAX_NVRAMBUF_SIZE);
	if (nv_memblock == NULL) {
		DBUSERR(("%s: Failed to allocate memory %d bytes\n",
		           __FUNCTION__, MAX_NVRAMBUF_SIZE));
		goto err;
	}
	len = dhd_os_get_image_block(nv_memblock, MAX_NVRAMBUF_SIZE, nv_image);
	if (len > 0 && len < MAX_NVRAMBUF_SIZE) {
		bufp = (char *)nv_memblock;
		bufp[len] = 0;
		dhd_bus->nvram_len = process_nvram_vars(bufp, len);
		if (dhd_bus->nvram_len % 4)
			nvram_words_pad = 4 - dhd_bus->nvram_len % 4;
	} else {
		DBUSERR(("%s: error reading nvram file: %d\n", __FUNCTION__, len));
		bcmerror = DBUS_ERR_NVRAM;
		goto err;
	}
	if (nv_image) {
		dhd_os_close_image1(dhd_bus->dhd, nv_image);
		nv_image = NULL;
	}

	/* For Get first block of fw to calculate total_len */
	file_exists = ((pfw_path != NULL) && (pfw_path[0] != '\0'));
	if (file_exists) {
		fw_image = dhd_os_open_image1(dhd_bus->dhd, pfw_path);
		if (fw_image == NULL) {
			printf("%s: Open fw file failed %s\n", __FUNCTION__, pfw_path);
			goto err;
		}
	}
	memptr = fw_memblock = MALLOC(dhd_bus->pub.osh, memblock_size);
	if (fw_memblock == NULL) {
		DBUSERR(("%s: Failed to allocate memory %d bytes\n", __FUNCTION__,
			memblock_size));
		goto err;
	}
	len = dhd_os_get_image_block((char*)memptr, memblock_size, fw_image);
	if ((actual_fwlen = check_file(dhd_bus->pub.osh, memptr)) <= 0) {
		DBUSERR(("%s: bad firmware format!\n", __FUNCTION__));
		goto err;
	}

	total_len = actual_fwlen + dhd_bus->nvram_len + nvram_words_pad;
#if defined(CONFIG_DHD_USE_STATIC_BUF)
	dhd_bus->image = (uint8*)DHD_OS_PREALLOC(dhd_bus->dhd,
		DHD_PREALLOC_MEMDUMP_RAM, total_len);
#else
	dhd_bus->image = MALLOC(dhd_bus->pub.osh, total_len);
#endif /* CONFIG_DHD_USE_STATIC_BUF */
	dhd_bus->image_len = total_len;
	if (dhd_bus->image == NULL) {
		DBUSERR(("%s: malloc failed! size=%d\n", __FUNCTION__, total_len));
		goto err;
	}

	/* Step1: Copy trx header + firmwre */
	memptr = fw_memblock;
	do {
		if (len < 0) {
			DBUSERR(("%s: dhd_os_get_image_block failed (%d)\n", __FUNCTION__, len));
			bcmerror = BCME_ERROR;
			goto err;
		}
		bcopy(memptr, dhd_bus->image+offset, len);
		offset += len;
	} while ((len = dhd_os_get_image_block((char*)memptr, memblock_size, fw_image)));
	/* Step2: Copy NVRAM + pad */
	hdr = (struct trx_header *)dhd_bus->image;
	img_offset = SIZEOF_TRX(hdr) + hdr->offsets[TRX_OFFSETS_DLFWLEN_IDX];
	bcopy(nv_memblock, (uint8 *)(dhd_bus->image + img_offset),
		dhd_bus->nvram_len);
	img_offset += dhd_bus->nvram_len;
	if (nvram_words_pad) {
		bzero(&dhd_bus->image[img_offset], nvram_words_pad);
		img_offset += nvram_words_pad;
	}
#ifdef BCMTRXV2
	/* Step3: Copy DSG/CFG for V2 */
	if (ISTRX_V2(hdr) &&
		(hdr->offsets[TRX_OFFSETS_DSG_LEN_IDX] ||
		hdr->offsets[TRX_OFFSETS_CFG_LEN_IDX])) {
		DBUSERR(("%s: fix me\n", __FUNCTION__));
	}
#endif /* BCMTRXV2 */
	/* Step4: update TRX header for nvram size */
	hdr = (struct trx_header *)dhd_bus->image;
	hdr->len = htol32(total_len);
	/* Pass the actual fw len */
	hdr->offsets[TRX_OFFSETS_NVM_LEN_IDX] =
		htol32(dhd_bus->nvram_len + nvram_words_pad);
	/* Calculate CRC over header */
	hdr->crc32 = hndcrc32((uint8 *)&hdr->flag_version,
		SIZEOF_TRX(hdr) - OFFSETOF(struct trx_header, flag_version),
		CRC32_INIT_VALUE);

	/* Calculate CRC over data */
	for (i = SIZEOF_TRX(hdr); i < total_len; ++i)
			hdr->crc32 = hndcrc32((uint8 *)&dhd_bus->image[i], 1, hdr->crc32);
	hdr->crc32 = htol32(hdr->crc32);

	bcmerror = DBUS_OK;

err:
	if (fw_memblock)
		MFREE(dhd_bus->pub.osh, fw_memblock, MAX_NVRAMBUF_SIZE);
	if (fw_image)
		dhd_os_close_image1(dhd_bus->dhd, fw_image);
	if (nv_memblock)
		MFREE(dhd_bus->pub.osh, nv_memblock, MAX_NVRAMBUF_SIZE);
	if (nv_image)
		dhd_os_close_image1(dhd_bus->dhd, nv_image);

	return bcmerror;
}

/**
 * during driver initialization ('attach') or after PnP 'resume', firmware needs to be loaded into
 * the dongle
 */
static int
dbus_do_download(dhd_bus_t *dhd_bus)
{
	int err = DBUS_OK;

	err = dbus_get_fw_nvram(dhd_bus);
	if (err) {
		DBUSERR(("dbus_do_download: fail to get nvram %d\n", err));
		return err;
	}

	if (dhd_bus->drvintf->dlstart && dhd_bus->drvintf->dlrun) {
		err = dhd_bus->drvintf->dlstart(dhd_bus->bus_info,
			dhd_bus->image, dhd_bus->image_len);
		if (err == DBUS_OK) {
			err = dhd_bus->drvintf->dlrun(dhd_bus->bus_info);
		}
	} else
		err = DBUS_ERR;

	if (dhd_bus->image) {
#if defined(CONFIG_DHD_USE_STATIC_BUF)
		DHD_OS_PREFREE(dhd_bus->dhd, dhd_bus->image, dhd_bus->image_len);
#else
		MFREE(dhd_bus->pub.osh, dhd_bus->image, dhd_bus->image_len);
#endif /* CONFIG_DHD_USE_STATIC_BUF */
		dhd_bus->image = NULL;
		dhd_bus->image_len = 0;
	}

	return err;
} /* dbus_do_download */
#else

/**
 * It is easy for the user to pass one jumbo nvram file to the driver than a set of smaller files.
 * The 'jumbo nvram' file format is essentially a set of nvram files. Before commencing firmware
 * download, the dongle needs to be probed so that the correct nvram contents within the jumbo nvram
 * file is selected.
 */
static int
dbus_jumbo_nvram(dhd_bus_t *dhd_bus)
{
	int8 *nvram = NULL;
	int nvram_len = 0;
	int ret = DBUS_OK;
	uint16 boardrev = 0xFFFF;
	uint16 boardtype = 0xFFFF;

	ret = dbus_select_nvram(dhd_bus, dhd_bus->extdl.vars, dhd_bus->extdl.varslen,
		boardtype, boardrev, &nvram, &nvram_len);

	if (ret == DBUS_JUMBO_BAD_FORMAT)
			return DBUS_ERR_NVRAM;
	else if (ret == DBUS_JUMBO_NOMATCH &&
		(boardtype != 0xFFFF || boardrev  != 0xFFFF)) {
			DBUSERR(("No matching NVRAM for boardtype 0x%02x boardrev 0x%02x\n",
				boardtype, boardrev));
			return DBUS_ERR_NVRAM;
	}
	dhd_bus->nvram = nvram;
	dhd_bus->nvram_len =  nvram_len;

	return DBUS_OK;
}

/** before commencing fw download, the correct NVRAM image to download has to be picked */
static int
dbus_get_nvram(dhd_bus_t *dhd_bus)
{
	int len, i;
	struct trx_header *hdr;
	int	actual_fwlen;
	uint32 img_offset = 0;

	dhd_bus->nvram_len = 0;
	if (dhd_bus->extdl.varslen) {
		if (DBUS_OK != dbus_jumbo_nvram(dhd_bus))
			return DBUS_ERR_NVRAM;
		DBUSERR(("NVRAM %d bytes downloaded\n", dhd_bus->nvram_len));
	}
#if defined(BCM_REQUEST_FW)
	else if (nonfwnvram) {
		dhd_bus->nvram = nonfwnvram;
		dhd_bus->nvram_len = nonfwnvramlen;
		DBUSERR(("NVRAM %d bytes downloaded\n", dhd_bus->nvram_len));
	}
#endif
	if (dhd_bus->nvram) {
		uint8 nvram_words_pad = 0;
		/* Validate the format/length etc of the file */
		if ((actual_fwlen = check_file(dhd_bus->pub.osh, dhd_bus->fw)) <= 0) {
			DBUSERR(("%s: bad firmware format!\n", __FUNCTION__));
			return DBUS_ERR_NVRAM;
		}

		if (!dhd_bus->nvram_nontxt) {
			/* host supplied nvram could be in .txt format
			* with all the comments etc...
			*/
			dhd_bus->nvram_len = process_nvram_vars(dhd_bus->nvram,
				dhd_bus->nvram_len);
		}
		if (dhd_bus->nvram_len % 4)
			nvram_words_pad = 4 - dhd_bus->nvram_len % 4;

		len = actual_fwlen + dhd_bus->nvram_len + nvram_words_pad;
#ifdef USBAP
		/* Allocate virtual memory otherwise it might fail on embedded systems */
		dhd_bus->image = VMALLOC(dhd_bus->pub.osh, len);
#else
#if defined(CONFIG_DHD_USE_STATIC_BUF)
		dhd_bus->image = (uint8*)DHD_OS_PREALLOC(dhd_bus->dhd,
			DHD_PREALLOC_MEMDUMP_RAM, len);
#else
		dhd_bus->image = MALLOCZ(dhd_bus->pub.osh, len);
#endif /* CONFIG_DHD_USE_STATIC_BUF */
#endif /* USBAP */
		dhd_bus->image_len = len;
		if (dhd_bus->image == NULL) {
			DBUSERR(("%s: malloc failed!\n", __FUNCTION__));
			return DBUS_ERR_NVRAM;
		}
		hdr = (struct trx_header *)dhd_bus->fw;
		/* Step1: Copy trx header + firmwre */
		img_offset = SIZEOF_TRX(hdr) + hdr->offsets[TRX_OFFSETS_DLFWLEN_IDX];
		bcopy(dhd_bus->fw, dhd_bus->image, img_offset);
		/* Step2: Copy NVRAM + pad */
		bcopy(dhd_bus->nvram, (uint8 *)(dhd_bus->image + img_offset),
			dhd_bus->nvram_len);
		img_offset += dhd_bus->nvram_len;
		if (nvram_words_pad) {
			bzero(&dhd_bus->image[img_offset],
				nvram_words_pad);
			img_offset += nvram_words_pad;
		}
#ifdef BCMTRXV2
		/* Step3: Copy DSG/CFG for V2 */
		if (ISTRX_V2(hdr) &&
			(hdr->offsets[TRX_OFFSETS_DSG_LEN_IDX] ||
			hdr->offsets[TRX_OFFSETS_CFG_LEN_IDX])) {

			bcopy(dhd_bus->fw + SIZEOF_TRX(hdr) +
				hdr->offsets[TRX_OFFSETS_DLFWLEN_IDX] +
				hdr->offsets[TRX_OFFSETS_NVM_LEN_IDX],
				dhd_bus->image + img_offset,
				hdr->offsets[TRX_OFFSETS_DSG_LEN_IDX] +
				hdr->offsets[TRX_OFFSETS_CFG_LEN_IDX]);

			/* Not needed */
			img_offset += hdr->offsets[TRX_OFFSETS_DSG_LEN_IDX] +
				hdr->offsets[TRX_OFFSETS_CFG_LEN_IDX];
		}
#endif /* BCMTRXV2 */
		/* Step4: update TRX header for nvram size */
		hdr = (struct trx_header *)dhd_bus->image;
		hdr->len = htol32(len);
		/* Pass the actual fw len */
		hdr->offsets[TRX_OFFSETS_NVM_LEN_IDX] =
			htol32(dhd_bus->nvram_len + nvram_words_pad);
		/* Calculate CRC over header */
		hdr->crc32 = hndcrc32((uint8 *)&hdr->flag_version,
			SIZEOF_TRX(hdr) - OFFSETOF(struct trx_header, flag_version),
			CRC32_INIT_VALUE);

		/* Calculate CRC over data */
		for (i = SIZEOF_TRX(hdr); i < len; ++i)
				hdr->crc32 = hndcrc32((uint8 *)&dhd_bus->image[i], 1, hdr->crc32);
		hdr->crc32 = htol32(hdr->crc32);
	} else {
		dhd_bus->image = dhd_bus->fw;
		dhd_bus->image_len = (uint32)dhd_bus->fwlen;
	}

	return DBUS_OK;
} /* dbus_get_nvram */

/**
 * during driver initialization ('attach') or after PnP 'resume', firmware needs to be loaded into
 * the dongle
 */
static int
dbus_do_download(dhd_bus_t *dhd_bus)
{
	int err = DBUS_OK;
#ifndef BCM_REQUEST_FW
	int decomp_override = 0;
#endif
#ifdef BCM_REQUEST_FW
	uint16 boardrev = 0xFFFF, boardtype = 0xFFFF;
	int8 *temp_nvram;
	int temp_len;
#endif

#if defined(BCM_DNGL_EMBEDIMAGE)
	if (dhd_bus->extdl.fw && (dhd_bus->extdl.fwlen > 0)) {
		dhd_bus->fw = (uint8 *)dhd_bus->extdl.fw;
		dhd_bus->fwlen = dhd_bus->extdl.fwlen;
		DBUSERR(("dbus_do_download: using override firmmware %d bytes\n",
			dhd_bus->fwlen));
	} else
		dbus_bus_fw_get(dhd_bus->bus_info, &dhd_bus->fw, &dhd_bus->fwlen,
			&decomp_override);

	if (!dhd_bus->fw) {
		DBUSERR(("dbus_do_download: devid 0x%x / %d not supported\n",
			dhd_bus->pub.attrib.devid, dhd_bus->pub.attrib.devid));
		return DBUS_ERR;
	}
#elif defined(BCM_REQUEST_FW)
	dhd_bus->firmware = dbus_get_fw_nvfile(dhd_bus->pub.attrib.devid,
		dhd_bus->pub.attrib.chiprev, &dhd_bus->fw, &dhd_bus->fwlen,
		DBUS_FIRMWARE, 0, 0, dhd_bus->fw_path);
	if (!dhd_bus->firmware)
		return DBUS_ERR;
#endif /* defined(BCM_DNGL_EMBEDIMAGE) */

	dhd_bus->image = dhd_bus->fw;
	dhd_bus->image_len = (uint32)dhd_bus->fwlen;

#ifndef BCM_REQUEST_FW
	if (UNZIP_ENAB(dhd_bus) && !decomp_override) {
		err = dbus_zlib_decomp(dhd_bus);
		if (err) {
			DBUSERR(("dbus_attach: fw decompress fail %d\n", err));
			return err;
		}
	}
#endif

#if defined(BCM_REQUEST_FW)
	/* check if nvram is provided as separte file */
	nonfwnvram = NULL;
	nonfwnvramlen = 0;
	dhd_bus->nvfile = dbus_get_fw_nvfile(dhd_bus->pub.attrib.devid,
		dhd_bus->pub.attrib.chiprev, (void *)&temp_nvram, &temp_len,
		DBUS_NVFILE, boardtype, boardrev, dhd_bus->nv_path);
	if (dhd_bus->nvfile) {
		int8 *tmp = MALLOCZ(dhd_bus->pub.osh, temp_len);
		if (tmp) {
			bcopy(temp_nvram, tmp, temp_len);
			nonfwnvram = tmp;
			nonfwnvramlen = temp_len;
		} else {
			err = DBUS_ERR;
			goto fail;
		}
	}
#endif /* defined(BCM_REQUEST_FW) */

	err = dbus_get_nvram(dhd_bus);
	if (err) {
		DBUSERR(("dbus_do_download: fail to get nvram %d\n", err));
		return err;
	}

	if (dhd_bus->drvintf->dlstart && dhd_bus->drvintf->dlrun) {
		err = dhd_bus->drvintf->dlstart(dhd_bus->bus_info,
			dhd_bus->image, dhd_bus->image_len);

		if (err == DBUS_OK)
			err = dhd_bus->drvintf->dlrun(dhd_bus->bus_info);
	} else
		err = DBUS_ERR;

	if (dhd_bus->nvram) {
#ifdef USBAP
		VMFREE(dhd_bus->pub.osh, dhd_bus->image, dhd_bus->image_len);
#else
#if defined(CONFIG_DHD_USE_STATIC_BUF)
		DHD_OS_PREFREE(dhd_bus->dhd, dhd_bus->image, dhd_bus->image_len);
#else
		MFREE(dhd_bus->pub.osh, dhd_bus->image, dhd_bus->image_len);
#endif /* CONFIG_DHD_USE_STATIC_BUF */
#endif /* USBAP */
		dhd_bus->image = dhd_bus->fw;
		dhd_bus->image_len = (uint32)dhd_bus->fwlen;
	}

#ifndef BCM_REQUEST_FW
	if (UNZIP_ENAB(dhd_bus) && (!decomp_override) && dhd_bus->orig_fw) {
		MFREE(dhd_bus->pub.osh, dhd_bus->fw, dhd_bus->decomp_memsize);
		dhd_bus->image = dhd_bus->fw = dhd_bus->orig_fw;
		dhd_bus->image_len = dhd_bus->fwlen = dhd_bus->origfw_len;
	}
#endif

#if defined(BCM_REQUEST_FW)
fail:
	if (dhd_bus->firmware) {
		dbus_release_fw_nvfile(dhd_bus->firmware);
		dhd_bus->firmware = NULL;
	}
	if (dhd_bus->nvfile) {
		dbus_release_fw_nvfile(dhd_bus->nvfile);
		dhd_bus->nvfile = NULL;
	}
	if (nonfwnvram) {
		MFREE(dhd_bus->pub.osh, nonfwnvram, nonfwnvramlen);
		nonfwnvram = NULL;
		nonfwnvramlen = 0;
	}
#endif
	return err;
} /* dbus_do_download */
#endif /* EXTERNAL_FW_PATH */
#endif /* defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW) */

/**
 * This function is called when the sent irb times out without a tx response status.
 * DBUS adds reliability by resending timed out IRBs DBUS_TX_RETRY_LIMIT times.
 */
static void
dbus_if_send_irb_timeout(void *handle, dbus_irb_tx_t *txirb)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;

	if ((dhd_bus == NULL) || (dhd_bus->drvintf == NULL) || (txirb == NULL)) {
		return;
	}

	DBUSTRACE(("%s\n", __FUNCTION__));

	/* Timeout in DBUS is fatal until we have a reproducible case. If the time out really
	 * happen, then retry needs to be fixed to maintain the order of IRPs going out
	 * (cancel in reverse order)
	 */
	return;

} /* dbus_if_send_irb_timeout */

/**
 * When lower DBUS level signals that a send IRB completed, either successful or not, the higher
 * level (eg dhd_linux.c) has to be notified, and transmit flow control has to be evaluated.
 */
static void
dbus_if_send_irb_complete(void *handle, dbus_irb_tx_t *txirb, int status)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;
	int txirb_pending;
	struct exec_parms args;
	void *pktinfo;

	if ((dhd_bus == NULL) || (txirb == NULL)) {
		return;
	}

	DBUSTRACE(("%s: status = %d\n", __FUNCTION__, status));

	dbus_tx_timer_stop(dhd_bus);

	/* re-queue BEFORE calling send_complete which will assume that this irb
	   is now available.
	 */
	pktinfo = txirb->info;
	bzero(txirb, sizeof(dbus_irb_tx_t));
	args.qenq.q = dhd_bus->tx_q;
	args.qenq.b = (dbus_irb_t *) txirb;
	EXEC_TXLOCK(dhd_bus, q_enq_exec, &args);

	if (dhd_bus->pub.busstate != DBUS_STATE_DOWN) {
		if ((status == DBUS_OK) || (status == DBUS_ERR_NODEVICE)) {
			if (dhd_bus->cbs && dhd_bus->cbs->send_complete)
				dhd_bus->cbs->send_complete(dhd_bus->cbarg, pktinfo,
					status);

			if (status == DBUS_OK) {
				txirb_pending = dhd_bus->pub.ntxq - dhd_bus->tx_q->cnt;
				if (txirb_pending)
					dbus_tx_timer_start(dhd_bus, DBUS_TX_TIMEOUT_INTERVAL);
				if ((txirb_pending < dhd_bus->tx_low_watermark) &&
					dhd_bus->txoff && !dhd_bus->txoverride) {
					dbus_flowctrl_tx(dhd_bus, OFF);
				}
			}
		} else {
			DBUSERR(("%s: %d WARNING freeing orphan pkt %p\n", __FUNCTION__, __LINE__,
				pktinfo));
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC)
			if (pktinfo)
				if (dhd_bus->cbs && dhd_bus->cbs->send_complete)
					dhd_bus->cbs->send_complete(dhd_bus->cbarg, pktinfo,
						status);
#else
			dbus_if_pktfree(dhd_bus, (void*)pktinfo, TRUE);
#endif /* defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC) */
		}
	} else {
		DBUSERR(("%s: %d WARNING freeing orphan pkt %p\n", __FUNCTION__, __LINE__,
			pktinfo));
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC)
		if (pktinfo)
			if (dhd_bus->cbs && dhd_bus->cbs->send_complete)
				dhd_bus->cbs->send_complete(dhd_bus->cbarg, pktinfo,
					status);
#else
		dbus_if_pktfree(dhd_bus, (void*)pktinfo, TRUE);
#endif /* defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) defined(BCM_RPC_TOC) */
	}
} /* dbus_if_send_irb_complete */

/**
 * When lower DBUS level signals that a receive IRB completed, either successful or not, the higher
 * level (e.g. dhd_linux.c) has to be notified, and fresh free receive IRBs may have to be given
 * to lower levels.
 */
static void
dbus_if_recv_irb_complete(void *handle, dbus_irb_rx_t *rxirb, int status)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;
	int rxirb_pending;
	struct exec_parms args;

	if ((dhd_bus == NULL) || (rxirb == NULL)) {
		return;
	}
	DBUSTRACE(("%s\n", __FUNCTION__));
	if (dhd_bus->pub.busstate != DBUS_STATE_DOWN &&
		dhd_bus->pub.busstate != DBUS_STATE_SLEEP) {
		if (status == DBUS_OK) {
			if ((rxirb->buf != NULL) && (rxirb->actual_len > 0)) {
#ifdef DBUS_USB_LOOPBACK
				if (is_loopback_pkt(rxirb->buf)) {
					matches_loopback_pkt(rxirb->buf);
				} else
#endif
				if (dhd_bus->cbs && dhd_bus->cbs->recv_buf) {
					dhd_bus->cbs->recv_buf(dhd_bus->cbarg, rxirb->buf,
					rxirb->actual_len);
				}
			} else if (rxirb->pkt != NULL) {
				if (dhd_bus->cbs && dhd_bus->cbs->recv_pkt)
					dhd_bus->cbs->recv_pkt(dhd_bus->cbarg, rxirb->pkt);
			} else {
				ASSERT(0); /* Should not happen */
			}

			rxirb_pending = dhd_bus->pub.nrxq - dhd_bus->rx_q->cnt - 1;
#ifdef BCMDBG
			dhd_bus->rxpend_q_hist[rxirb_pending]++;
#endif /* BCMDBG */
			if ((rxirb_pending <= dhd_bus->rx_low_watermark) &&
				!dhd_bus->rxoff) {
				DBUSTRACE(("Low watermark so submit more %d <= %d \n",
					dhd_bus->rx_low_watermark, rxirb_pending));
				dbus_rxirbs_fill(dhd_bus);
			} else if (dhd_bus->rxoff)
				DBUSTRACE(("rx flow controlled. not filling more. cut_rxq=%d\n",
					dhd_bus->rx_q->cnt));
		} else if (status == DBUS_ERR_NODEVICE) {
			DBUSERR(("%s: %d status = %d, buf %p\n", __FUNCTION__, __LINE__, status,
				rxirb->buf));
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
			if (rxirb->buf) {
				PKTFRMNATIVE(dhd_bus->pub.osh, rxirb->buf);
				PKTFREE(dhd_bus->pub.osh, rxirb->buf, FALSE);
			}
#endif /* BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPY || BCM_RPC_TOC */
		} else {
			if (status != DBUS_ERR_RXZLP)
				DBUSERR(("%s: %d status = %d, buf %p\n", __FUNCTION__, __LINE__,
					status, rxirb->buf));
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
			if (rxirb->buf) {
				PKTFRMNATIVE(dhd_bus->pub.osh, rxirb->buf);
				PKTFREE(dhd_bus->pub.osh, rxirb->buf, FALSE);
			}
#endif /* BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPY || BCM_RPC_TOC */
		}
	} else {
		DBUSTRACE(("%s: DBUS down, ignoring recv callback. buf %p\n", __FUNCTION__,
			rxirb->buf));
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		if (rxirb->buf) {
			PKTFRMNATIVE(dhd_bus->pub.osh, rxirb->buf);
			PKTFREE(dhd_bus->pub.osh, rxirb->buf, FALSE);
		}
#endif /* BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPY || BCM_RPC_TOC */
	}
	if (dhd_bus->rx_q != NULL) {
		bzero(rxirb, sizeof(dbus_irb_rx_t));
		args.qenq.q = dhd_bus->rx_q;
		args.qenq.b = (dbus_irb_t *) rxirb;
		EXEC_RXLOCK(dhd_bus, q_enq_exec, &args);
	} else
		MFREE(dhd_bus->pub.osh, rxirb, sizeof(dbus_irb_tx_t));
} /* dbus_if_recv_irb_complete */

/**
 *  Accumulate errors signaled by lower DBUS levels and signal them to higher (eg dhd_linux.c)
 *  level.
 */
static void
dbus_if_errhandler(void *handle, int err)
{
	dhd_bus_t *dhd_bus = handle;
	uint32 mask = 0;

	if (dhd_bus == NULL)
		return;

	switch (err) {
		case DBUS_ERR_TXFAIL:
			dhd_bus->pub.stats.tx_errors++;
			mask |= ERR_CBMASK_TXFAIL;
			break;
		case DBUS_ERR_TXDROP:
			dhd_bus->pub.stats.tx_dropped++;
			mask |= ERR_CBMASK_TXFAIL;
			break;
		case DBUS_ERR_RXFAIL:
			dhd_bus->pub.stats.rx_errors++;
			mask |= ERR_CBMASK_RXFAIL;
			break;
		case DBUS_ERR_RXDROP:
			dhd_bus->pub.stats.rx_dropped++;
			mask |= ERR_CBMASK_RXFAIL;
			break;
		default:
			break;
	}

	if (dhd_bus->cbs && dhd_bus->cbs->errhandler && (dhd_bus->errmask & mask))
		dhd_bus->cbs->errhandler(dhd_bus->cbarg, err);
}

/**
 * When lower DBUS level signals control IRB completed, higher level (eg dhd_linux.c) has to be
 * notified.
 */
static void
dbus_if_ctl_complete(void *handle, int type, int status)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL) {
		DBUSERR(("%s: dhd_bus is NULL\n", __FUNCTION__));
		return;
	}

	if (dhd_bus->pub.busstate != DBUS_STATE_DOWN) {
		if (dhd_bus->cbs && dhd_bus->cbs->ctl_complete)
			dhd_bus->cbs->ctl_complete(dhd_bus->cbarg, type, status);
	}
}

/**
 * Rx related functionality (flow control, posting of free IRBs to rx queue) is dependent upon the
 * bus state. When lower DBUS level signals a change in the interface state, take appropriate action
 * and forward the signaling to the higher (eg dhd_linux.c) level.
 */
static void
dbus_if_state_change(void *handle, int state)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;
	int old_state;

	if (dhd_bus == NULL)
		return;

	if (dhd_bus->pub.busstate == state)
		return;
	old_state = dhd_bus->pub.busstate;
	if (state == DBUS_STATE_DISCONNECT) {
		DBUSERR(("DBUS disconnected\n"));
	}

	/* Ignore USB SUSPEND while not up yet */
	if (state == DBUS_STATE_SLEEP && old_state != DBUS_STATE_UP)
		return;

	DBUSTRACE(("dbus state change from %d to to %d\n", old_state, state));

	/* Don't update state if it's PnP firmware re-download */
	if (state != DBUS_STATE_PNP_FWDL)
		dhd_bus->pub.busstate = state;
	else
		dbus_flowctrl_rx(handle, FALSE);
	if (state == DBUS_STATE_SLEEP)
		dbus_flowctrl_rx(handle, TRUE);
	if (state == DBUS_STATE_UP) {
		dbus_rxirbs_fill(dhd_bus);
		dbus_flowctrl_rx(handle, FALSE);
	}

	if (dhd_bus->cbs && dhd_bus->cbs->state_change)
		dhd_bus->cbs->state_change(dhd_bus->cbarg, state);
}

/** Forward request for packet from lower DBUS layer to higher layer (e.g. dhd_linux.c) */
static void *
dbus_if_pktget(void *handle, uint len, bool send)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;
	void *p = NULL;

	if (dhd_bus == NULL)
		return NULL;

	if (dhd_bus->cbs && dhd_bus->cbs->pktget)
		p = dhd_bus->cbs->pktget(dhd_bus->cbarg, len, send);
	else
		ASSERT(0);

	return p;
}

/** Forward request to free packet from lower DBUS layer to higher layer (e.g. dhd_linux.c) */
static void
dbus_if_pktfree(void *handle, void *p, bool send)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) handle;

	if (dhd_bus == NULL)
		return;

	if (dhd_bus->cbs && dhd_bus->cbs->pktfree)
		dhd_bus->cbs->pktfree(dhd_bus->cbarg, p, send);
	else
		ASSERT(0);
}

/** Lower DBUS level requests either a send or receive IRB */
static struct dbus_irb*
dbus_if_getirb(void *cbarg, bool send)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) cbarg;
	struct exec_parms args;
	struct dbus_irb *irb;

	if ((dhd_bus == NULL) || (dhd_bus->pub.busstate != DBUS_STATE_UP))
		return NULL;

	if (send == TRUE) {
		args.qdeq.q = dhd_bus->tx_q;
		irb = EXEC_TXLOCK(dhd_bus, q_deq_exec, &args);
	} else {
		args.qdeq.q = dhd_bus->rx_q;
		irb = EXEC_RXLOCK(dhd_bus, q_deq_exec, &args);
	}

	return irb;
}

/* Register/Unregister functions are called by the main DHD entry
 * point (e.g. module insertion) to link with the bus driver, in
 * order to look for or await the device.
 */

static dbus_driver_t dhd_dbus = {
	dhd_dbus_probe_cb,
	dhd_dbus_disconnect_cb,
	dbus_suspend,
	dbus_resume
};

/**
 * As part of initialization, higher level (eg dhd_linux.c) requests DBUS to prepare for
 * action.
 */
int
dhd_bus_register(void)
{
	int err;

	DBUSTRACE(("%s: Enter\n", __FUNCTION__));

	err = dbus_bus_register(&dhd_dbus, &g_busintf);

	/* Device not detected */
	if (err == DBUS_ERR_NODEVICE)
		err = DBUS_OK;

	return err;
}

dhd_pub_t *g_pub = NULL;
bool net_attached = FALSE;
void
dhd_bus_unregister(void)
{
	DBUSTRACE(("%s\n", __FUNCTION__));

	DHD_MUTEX_LOCK();
	if (g_pub) {
		g_pub->dhd_remove = TRUE;
		if (!g_pub->bus) {
			dhd_dbus_disconnect_cb(g_pub->bus);
		}
	}
	DHD_MUTEX_UNLOCK();
	dbus_bus_deregister();
}

/** As part of initialization, data structures have to be allocated and initialized */
dhd_bus_t *
dbus_attach(osl_t *osh, int rxsize, int nrxq, int ntxq, dhd_pub_t *pub,
	dbus_callbacks_t *cbs, dbus_extdl_t *extdl, struct shared_info *sh)
{
	dhd_bus_t *dhd_bus;
	int err;

	if ((g_busintf == NULL) || (g_busintf->attach == NULL) || (cbs == NULL))
		return NULL;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if ((nrxq <= 0) || (ntxq <= 0))
		return NULL;

	dhd_bus = MALLOCZ(osh, sizeof(dhd_bus_t));
	if (dhd_bus == NULL) {
		DBUSERR(("%s: malloc failed %zu\n", __FUNCTION__, sizeof(dhd_bus_t)));
		return NULL;
	}

	/* BUS-specific driver interface (at a lower DBUS level) */
	dhd_bus->drvintf = g_busintf;
	dhd_bus->cbarg = pub;
	dhd_bus->cbs = cbs;

	dhd_bus->pub.sh = sh;
	dhd_bus->pub.osh = osh;
	dhd_bus->pub.rxsize = rxsize;

#ifdef EHCI_FASTPATH_RX
	atomic_set(&dhd_bus->rx_outstanding, 0);
#endif

	dhd_bus->pub.nrxq = nrxq;
	dhd_bus->rx_low_watermark = nrxq / 2;	/* keep enough posted rx urbs */
	dhd_bus->pub.ntxq = ntxq;
	dhd_bus->tx_low_watermark = ntxq / 4;	/* flow control when too many tx urbs posted */

	dhd_bus->tx_q = MALLOCZ(osh, sizeof(dbus_irbq_t));
	if (dhd_bus->tx_q == NULL)
		goto error;
	else {
		err = dbus_irbq_init(dhd_bus, dhd_bus->tx_q, ntxq, sizeof(dbus_irb_tx_t));
		if (err != DBUS_OK)
			goto error;
	}

	dhd_bus->rx_q = MALLOCZ(osh, sizeof(dbus_irbq_t));
	if (dhd_bus->rx_q == NULL)
		goto error;
	else {
		err = dbus_irbq_init(dhd_bus, dhd_bus->rx_q, nrxq, sizeof(dbus_irb_rx_t));
		if (err != DBUS_OK)
			goto error;
	}

#ifdef BCMDBG
	dhd_bus->txpend_q_hist = MALLOCZ(osh, dhd_bus->pub.ntxq * sizeof(int));
	if (dhd_bus->txpend_q_hist == NULL)
		goto error;

	dhd_bus->rxpend_q_hist = MALLOCZ(osh, dhd_bus->pub.nrxq * sizeof(int));
	if (dhd_bus->rxpend_q_hist == NULL)
		goto error;
#endif /* BCMDBG */

	dhd_bus->bus_info = (void *)g_busintf->attach(&dhd_bus->pub,
		dhd_bus, &dbus_intf_cbs);
	if (dhd_bus->bus_info == NULL)
		goto error;

	dbus_tx_timer_init(dhd_bus);

#if defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW)
	/* Need to copy external image for re-download */
	if (extdl && extdl->fw && (extdl->fwlen > 0)) {
		dhd_bus->extdl.fw = MALLOCZ(osh, extdl->fwlen);
		if (dhd_bus->extdl.fw) {
			bcopy(extdl->fw, dhd_bus->extdl.fw, extdl->fwlen);
			dhd_bus->extdl.fwlen = extdl->fwlen;
		}
	}

	if (extdl && extdl->vars && (extdl->varslen > 0)) {
		dhd_bus->extdl.vars = MALLOCZ(osh, extdl->varslen);
		if (dhd_bus->extdl.vars) {
			bcopy(extdl->vars, dhd_bus->extdl.vars, extdl->varslen);
			dhd_bus->extdl.varslen = extdl->varslen;
		}
	}
#endif /* defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW) */

	return (dhd_bus_t *)dhd_bus;

error:
	DBUSERR(("%s: Failed\n", __FUNCTION__));
	dbus_detach(dhd_bus);
	return NULL;
} /* dbus_attach */

static void
dbus_detach(dhd_bus_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	osl_t *osh;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return;

	dbus_tx_timer_stop(dhd_bus);

	osh = pub->pub.osh;

	if (dhd_bus->drvintf && dhd_bus->drvintf->detach)
		 dhd_bus->drvintf->detach((dbus_pub_t *)dhd_bus, dhd_bus->bus_info);

	if (dhd_bus->tx_q) {
		dbus_irbq_deinit(dhd_bus, dhd_bus->tx_q, sizeof(dbus_irb_tx_t));
		MFREE(osh, dhd_bus->tx_q, sizeof(dbus_irbq_t));
		dhd_bus->tx_q = NULL;
	}

	if (dhd_bus->rx_q) {
		dbus_irbq_deinit(dhd_bus, dhd_bus->rx_q, sizeof(dbus_irb_rx_t));
		MFREE(osh, dhd_bus->rx_q, sizeof(dbus_irbq_t));
		dhd_bus->rx_q = NULL;
	}

#ifdef BCMDBG
	if (dhd_bus->txpend_q_hist)
		MFREE(osh, dhd_bus->txpend_q_hist, dhd_bus->pub.ntxq * sizeof(int));
	if (dhd_bus->rxpend_q_hist)
		MFREE(osh, dhd_bus->rxpend_q_hist, dhd_bus->pub.nrxq * sizeof(int));
#endif /* BCMDBG */

	if (dhd_bus->extdl.fw && (dhd_bus->extdl.fwlen > 0)) {
		MFREE(osh, dhd_bus->extdl.fw, dhd_bus->extdl.fwlen);
		dhd_bus->extdl.fw = NULL;
		dhd_bus->extdl.fwlen = 0;
	}

	if (dhd_bus->extdl.vars && (dhd_bus->extdl.varslen > 0)) {
		MFREE(osh, dhd_bus->extdl.vars, dhd_bus->extdl.varslen);
		dhd_bus->extdl.vars = NULL;
		dhd_bus->extdl.varslen = 0;
	}

	MFREE(osh, dhd_bus, sizeof(dhd_bus_t));
} /* dbus_detach */

int dbus_dlneeded(dhd_bus_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int dlneeded = DBUS_ERR;

	if (!dhd_bus) {
		DBUSERR(("%s: dhd_bus is NULL\n", __FUNCTION__));
		return DBUS_ERR;
	}

	DBUSTRACE(("%s: state %d\n", __FUNCTION__, dhd_bus->pub.busstate));

	if (dhd_bus->drvintf->dlneeded) {
		dlneeded = dhd_bus->drvintf->dlneeded(dhd_bus->bus_info);
	}
	printf("%s: dlneeded=%d\n", __FUNCTION__, dlneeded);

	/* dlneeded > 0: need to download
	  * dlneeded = 0: downloaded
	  * dlneeded < 0: bus error*/
	return dlneeded;
}

#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW))
int dbus_download_firmware(dhd_bus_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_OK;

	if (!dhd_bus) {
		DBUSERR(("%s: dhd_bus is NULL\n", __FUNCTION__));
		return DBUS_ERR;
	}

	DBUSTRACE(("%s: state %d\n", __FUNCTION__, dhd_bus->pub.busstate));

	dhd_bus->pub.busstate = DBUS_STATE_DL_PENDING;
	err = dbus_do_download(dhd_bus);
	if (err == DBUS_OK) {
		dhd_bus->pub.busstate = DBUS_STATE_DL_DONE;
	} else {
		DBUSERR(("%s: download failed (%d)\n", __FUNCTION__, err));
	}

	return err;
}
#endif /* BCM_DNGL_EMBEDIMAGE || BCM_REQUEST_FW */

/**
 * higher layer requests us to 'up' the interface to the dongle. Prerequisite is that firmware (not
 * bootloader) must be active in the dongle.
 */
int
dbus_up(struct dhd_bus *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_OK;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL) {
		DBUSERR(("%s: dhd_bus is NULL\n", __FUNCTION__));
		return DBUS_ERR;
	}

	if ((dhd_bus->pub.busstate == DBUS_STATE_DL_DONE) ||
		(dhd_bus->pub.busstate == DBUS_STATE_DOWN) ||
		(dhd_bus->pub.busstate == DBUS_STATE_SLEEP)) {
		if (dhd_bus->drvintf && dhd_bus->drvintf->up) {
			err = dhd_bus->drvintf->up(dhd_bus->bus_info);

			if (err == DBUS_OK) {
				dbus_rxirbs_fill(dhd_bus);
			}
		}
	} else
		err = DBUS_ERR;

	return err;
}

/** higher layer requests us to 'down' the interface to the dongle. */
int
dbus_down(dbus_pub_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return DBUS_ERR;

	dbus_tx_timer_stop(dhd_bus);

	if (dhd_bus->pub.busstate == DBUS_STATE_UP ||
		dhd_bus->pub.busstate == DBUS_STATE_SLEEP) {
		if (dhd_bus->drvintf && dhd_bus->drvintf->down)
			return dhd_bus->drvintf->down(dhd_bus->bus_info);
	}

	return DBUS_ERR;
}

int
dbus_shutdown(dbus_pub_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return DBUS_ERR;

	if (dhd_bus->drvintf && dhd_bus->drvintf->shutdown)
		return dhd_bus->drvintf->shutdown(dhd_bus->bus_info);

	return DBUS_OK;
}

int
dbus_stop(struct dhd_bus *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return DBUS_ERR;

	if (dhd_bus->pub.busstate == DBUS_STATE_UP ||
		dhd_bus->pub.busstate == DBUS_STATE_SLEEP) {
		if (dhd_bus->drvintf && dhd_bus->drvintf->stop)
			return dhd_bus->drvintf->stop(dhd_bus->bus_info);
	}

	return DBUS_ERR;
}

int
dbus_send_buf(dbus_pub_t *pub, uint8 *buf, int len, void *info)
{
	return dbus_send_irb(pub, buf, len, NULL, info);
}

static int
dbus_send_pkt(dbus_pub_t *pub, void *pkt, void *info)
{
	return dbus_send_irb(pub, NULL, 0, pkt, info);
}

static int
dbus_send_ctl(struct dhd_bus *pub, uint8 *buf, int len)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	if (dhd_bus == NULL) {
		DBUSERR(("%s: dhd_bus is NULL\n", __FUNCTION__));
		return DBUS_ERR;
	}

	if (dhd_bus->pub.busstate == DBUS_STATE_UP ||
		dhd_bus->pub.busstate == DBUS_STATE_SLEEP) {
		if (dhd_bus->drvintf && dhd_bus->drvintf->send_ctl)
			return dhd_bus->drvintf->send_ctl(dhd_bus->bus_info, buf, len);
	} else {
		DBUSERR(("%s: bustate=%d\n", __FUNCTION__, dhd_bus->pub.busstate));
	}

	return DBUS_ERR;
}

static int
dbus_recv_ctl(struct dhd_bus *pub, uint8 *buf, int len)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	if ((dhd_bus == NULL) || (buf == NULL))
		return DBUS_ERR;

	if (dhd_bus->pub.busstate == DBUS_STATE_UP ||
		dhd_bus->pub.busstate == DBUS_STATE_SLEEP) {
		if (dhd_bus->drvintf && dhd_bus->drvintf->recv_ctl)
			return dhd_bus->drvintf->recv_ctl(dhd_bus->bus_info, buf, len);
	} else {
		DBUSERR(("%s: bustate=%d\n", __FUNCTION__, dhd_bus->pub.busstate));
	}

	return DBUS_ERR;
}

/** Only called via RPC (Dec 2012) */
int
dbus_recv_bulk(dbus_pub_t *pub, uint32 ep_idx)
{
#ifdef EHCI_FASTPATH_RX
	/* 2nd bulk in not supported for EHCI_FASTPATH_RX */
	ASSERT(0);
#else
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	dbus_irb_rx_t *rxirb;
	struct exec_parms args;
	int status;


	if (dhd_bus == NULL)
		return DBUS_ERR;

	args.qdeq.q = dhd_bus->rx_q;
	if (dhd_bus->pub.busstate == DBUS_STATE_UP) {
		if (dhd_bus->drvintf && dhd_bus->drvintf->recv_irb_from_ep) {
			if ((rxirb = (EXEC_RXLOCK(dhd_bus, q_deq_exec, &args))) != NULL) {
				status = dhd_bus->drvintf->recv_irb_from_ep(dhd_bus->bus_info,
					rxirb, ep_idx);
				if (status == DBUS_ERR_RXDROP) {
					bzero(rxirb, sizeof(dbus_irb_rx_t));
					args.qenq.q = dhd_bus->rx_q;
					args.qenq.b = (dbus_irb_t *) rxirb;
					EXEC_RXLOCK(dhd_bus, q_enq_exec, &args);
				}
			}
		}
	}
#endif /* EHCI_FASTPATH_RX */

	return DBUS_ERR; /* ME DVV: This does not seem right :-) */
}

#ifdef INTR_EP_ENABLE
/** only called by dhd_cdc.c (Dec 2012) */
static int
dbus_poll_intr(dbus_pub_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	int status = DBUS_ERR;

	if (dhd_bus == NULL)
		return DBUS_ERR;

	if (dhd_bus->pub.busstate == DBUS_STATE_UP) {
		if (dhd_bus->drvintf && dhd_bus->drvintf->recv_irb_from_ep) {
			status = dhd_bus->drvintf->recv_irb_from_ep(dhd_bus->bus_info,
				NULL, 0xff);
		}
	}
	return status;
}
#endif /* INTR_EP_ENABLE */

/** called by nobody (Dec 2012) */
void *
dbus_pktget(dbus_pub_t *pub, int len)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	if ((dhd_bus == NULL) || (len < 0))
		return NULL;

	return PKTGET(dhd_bus->pub.osh, len, TRUE);
}

/** called by nobody (Dec 2012) */
void
dbus_pktfree(dbus_pub_t *pub, void* pkt)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	if ((dhd_bus == NULL) || (pkt == NULL))
		return;

	PKTFREE(dhd_bus->pub.osh, pkt, TRUE);
}

/** called by nobody (Dec 2012) */
int
dbus_get_stats(dbus_pub_t *pub, dbus_stats_t *stats)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	if ((dhd_bus == NULL) || (stats == NULL))
		return DBUS_ERR;

	bcopy(&dhd_bus->pub.stats, stats, sizeof(dbus_stats_t));

	return DBUS_OK;
}

int
dbus_get_attrib(dhd_bus_t *pub, dbus_attrib_t *attrib)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_ERR;

	if ((dhd_bus == NULL) || (attrib == NULL))
		return DBUS_ERR;

	if (dhd_bus->drvintf && dhd_bus->drvintf->get_attrib) {
		err = dhd_bus->drvintf->get_attrib(dhd_bus->bus_info,
		&dhd_bus->pub.attrib);
	}

	bcopy(&dhd_bus->pub.attrib, attrib, sizeof(dbus_attrib_t));
	return err;
}

int
dbus_get_device_speed(dbus_pub_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;

	if (dhd_bus == NULL)
		return INVALID_SPEED;

	return (dhd_bus->pub.device_speed);
}

int
dbus_set_config(dbus_pub_t *pub, dbus_config_t *config)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_ERR;

	if ((dhd_bus == NULL) || (config == NULL))
		return DBUS_ERR;

	if (dhd_bus->drvintf && dhd_bus->drvintf->set_config) {
		err = dhd_bus->drvintf->set_config(dhd_bus->bus_info,
			config);

		if ((config->config_id == DBUS_CONFIG_ID_AGGR_LIMIT) &&
			(!err) &&
			(dhd_bus->pub.busstate == DBUS_STATE_UP)) {
			dbus_rxirbs_fill(dhd_bus);
		}
	}

	return err;
}

int
dbus_get_config(dbus_pub_t *pub, dbus_config_t *config)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_ERR;

	if ((dhd_bus == NULL) || (config == NULL))
		return DBUS_ERR;

	if (dhd_bus->drvintf && dhd_bus->drvintf->get_config) {
		err = dhd_bus->drvintf->get_config(dhd_bus->bus_info,
		config);
	}

	return err;
}

int
dbus_set_errmask(dbus_pub_t *pub, uint32 mask)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_OK;

	if (dhd_bus == NULL)
		return DBUS_ERR;

	dhd_bus->errmask = mask;
	return err;
}

int
dbus_pnp_resume(dbus_pub_t *pub, int *fw_reload)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_ERR;
	bool fwdl = FALSE;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return DBUS_ERR;

	if (dhd_bus->pub.busstate == DBUS_STATE_UP) {
		return DBUS_OK;
	}

#if defined(BCM_DNGL_EMBEDIMAGE)
	if (dhd_bus->drvintf->device_exists &&
		dhd_bus->drvintf->device_exists(dhd_bus->bus_info)) {
		if (dhd_bus->drvintf->dlneeded) {
			if (dhd_bus->drvintf->dlneeded(dhd_bus->bus_info)) {
				err = dbus_do_download(dhd_bus);
				if (err == DBUS_OK) {
					fwdl = TRUE;
				}
				if (dhd_bus->pub.busstate == DBUS_STATE_DL_DONE)
					dbus_up(&dhd_bus->pub);
			}
		}
	} else {
		return DBUS_ERR;
	}
#endif /* BCM_DNGL_EMBEDIMAGE */


	if (dhd_bus->drvintf->pnp) {
		err = dhd_bus->drvintf->pnp(dhd_bus->bus_info,
			DBUS_PNP_RESUME);
	}

	if (dhd_bus->drvintf->recv_needed) {
		if (dhd_bus->drvintf->recv_needed(dhd_bus->bus_info)) {
			/* Refill after sleep/hibernate */
			dbus_rxirbs_fill(dhd_bus);
		}
	}

#if defined(BCM_DNGL_EMBEDIMAGE)
	if (fwdl == TRUE) {
		dbus_if_state_change(dhd_bus, DBUS_STATE_PNP_FWDL);
	}
#endif /* BCM_DNGL_EMBEDIMAGE */

	if (fw_reload)
		*fw_reload = fwdl;

	return err;
} /* dbus_pnp_resume */

int
dbus_pnp_sleep(dbus_pub_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return DBUS_ERR;

	dbus_tx_timer_stop(dhd_bus);

	if (dhd_bus->drvintf && dhd_bus->drvintf->pnp) {
		err = dhd_bus->drvintf->pnp(dhd_bus->bus_info,
			DBUS_PNP_SLEEP);
	}

	return err;
}

int
dbus_pnp_disconnect(dbus_pub_t *pub)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) pub;
	int err = DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return DBUS_ERR;

	dbus_tx_timer_stop(dhd_bus);

	if (dhd_bus->drvintf && dhd_bus->drvintf->pnp) {
		err = dhd_bus->drvintf->pnp(dhd_bus->bus_info,
			DBUS_PNP_DISCONNECT);
	}

	return err;
}

int
dhd_bus_iovar_op(dhd_pub_t *dhdp, const char *name,
	void *params, int plen, void *arg, int len, bool set)
{
	dhd_bus_t *dhd_bus = (dhd_bus_t *) dhdp->bus;
	int err = DBUS_ERR;

	DBUSTRACE(("%s\n", __FUNCTION__));

	if (dhd_bus == NULL)
		return DBUS_ERR;

	if (dhd_bus->drvintf && dhd_bus->drvintf->iovar_op) {
		err = dhd_bus->drvintf->iovar_op(dhd_bus->bus_info,
			name, params, plen, arg, len, set);
	}

	return err;
}

#ifdef BCMDBG
void
dbus_hist_dump(dbus_pub_t *pub, struct bcmstrbuf *b)
{
	int i = 0, j = 0;
	dbus_info_t *dbus_info = (dbus_info_t *) pub;

	bcm_bprintf(b, "\nDBUS histogram\n");
	bcm_bprintf(b, "txq\n");
	for (i = 0; i < dbus_info->pub.ntxq; i++) {
		if (dbus_info->txpend_q_hist[i]) {
			bcm_bprintf(b, "%d: %d ", i, dbus_info->txpend_q_hist[i]);
			j++;
			if (j % 10 == 0) {
				bcm_bprintf(b, "\n");
			}
		}
	}

	j = 0;
	bcm_bprintf(b, "\nrxq\n");
	for (i = 0; i < dbus_info->pub.nrxq; i++) {
		if (dbus_info->rxpend_q_hist[i]) {
			bcm_bprintf(b, "%d: %d ", i, dbus_info->rxpend_q_hist[i]);
			j++;
			if (j % 10 == 0) {
				bcm_bprintf(b, "\n");
			}
		}
	}
	bcm_bprintf(b, "\n");

	if (dbus_info->drvintf && dbus_info->drvintf->dump)
		dbus_info->drvintf->dump(dbus_info->bus_info, b);
}
#endif /* BCMDBG */

void *
dhd_dbus_txq(const dbus_pub_t *pub)
{
	return NULL;
}

uint
dhd_dbus_hdrlen(const dbus_pub_t *pub)
{
	return 0;
}

void *
dbus_get_devinfo(dbus_pub_t *pub)
{
	return pub->dev_info;
}

#if (defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW)) && !defined(EXTERNAL_FW_PATH)
static int
dbus_select_nvram(dhd_bus_t *dhd_bus, int8 *jumbonvram, int jumbolen,
uint16 boardtype, uint16 boardrev, int8 **nvram, int *nvram_len)
{
	/* Multi board nvram file format is contenation of nvram info with \r
	*  The file format for two contatenated set is
	*  \nBroadcom Jumbo Nvram file\nfirst_set\nsecond_set\nthird_set\n
	*/
	uint8 *nvram_start = NULL, *nvram_end = NULL;
	uint8 *nvram_start_prev = NULL, *nvram_end_prev = NULL;
	uint16 btype = 0, brev = 0;
	int len  = 0;
	char *field;

	*nvram = NULL;
	*nvram_len = 0;

	if (strncmp(BCM_JUMBO_START, jumbonvram, strlen(BCM_JUMBO_START))) {
		/* single nvram file in the native format */
		DBUSTRACE(("%s: Non-Jumbo NVRAM File \n", __FUNCTION__));
		*nvram = jumbonvram;
		*nvram_len = jumbolen;
		return DBUS_OK;
	} else {
		DBUSTRACE(("%s: Jumbo NVRAM File \n", __FUNCTION__));
	}

	/* sanity test the end of the config sets for proper ending */
	if (jumbonvram[jumbolen - 1] != BCM_JUMBO_NVRAM_DELIMIT ||
		jumbonvram[jumbolen - 2] != '\0') {
		DBUSERR(("%s: Bad Jumbo NVRAM file format\n", __FUNCTION__));
		return DBUS_JUMBO_BAD_FORMAT;
	}

	dhd_bus->nvram_nontxt = DBUS_NVRAM_NONTXT;

	nvram_start = jumbonvram;

	while (*nvram_start != BCM_JUMBO_NVRAM_DELIMIT && len < jumbolen) {

		/* consume the  first file info line
		* \nBroadcom Jumbo Nvram file\nfile1\n ...
		*/
		len ++;
		nvram_start ++;
	}

	nvram_end = nvram_start;

	/* search for "boardrev=0xabcd" and "boardtype=0x1234" information in
	* the concatenated nvram config files /sets
	*/

	while (len < jumbolen) {

		if (*nvram_end == '\0') {
			/* end of a config set is marked by multiple null characters */
			len ++;
			nvram_end ++;
			DBUSTRACE(("%s: NULL chr len = %d char = 0x%x\n", __FUNCTION__,
				len, *nvram_end));
			continue;

		} else if (*nvram_end == BCM_JUMBO_NVRAM_DELIMIT) {

			/* config set delimiter is reached */
			/* check if next config set is present or not
			*  return  if next config is not present
			*/

			/* start search the next config set */
			nvram_start_prev = nvram_start;
			nvram_end_prev = nvram_end;

			nvram_end ++;
			nvram_start = nvram_end;
			btype = brev = 0;
			DBUSTRACE(("%s: going to next record len = %d "
					"char = 0x%x \n", __FUNCTION__, len, *nvram_end));
			len ++;
			if (len >= jumbolen) {

				*nvram = nvram_start_prev;
				*nvram_len = (int)(nvram_end_prev - nvram_start_prev);

				DBUSTRACE(("%s: no more len = %d nvram_end = 0x%p",
					__FUNCTION__, len, nvram_end));

				return DBUS_JUMBO_NOMATCH;

			} else {
				continue;
			}

		} else {

			DBUSTRACE(("%s: config str = %s\n", __FUNCTION__, nvram_end));

			if (bcmp(nvram_end, "boardtype", strlen("boardtype")) == 0) {

				field = strchr(nvram_end, '=');
				field++;
				btype = (uint16)bcm_strtoul(field, NULL, 0);

				DBUSTRACE(("%s: btype = 0x%x boardtype = 0x%x \n", __FUNCTION__,
					btype, boardtype));
			}

			if (bcmp(nvram_end, "boardrev", strlen("boardrev")) == 0) {

				field = strchr(nvram_end, '=');
				field++;
				brev = (uint16)bcm_strtoul(field, NULL, 0);

				DBUSTRACE(("%s: brev = 0x%x boardrev = 0x%x \n", __FUNCTION__,
					brev, boardrev));
			}
			if (btype == boardtype && brev == boardrev) {
				/* locate nvram config set end - ie.find '\r' char */
				while (*nvram_end != BCM_JUMBO_NVRAM_DELIMIT)
					nvram_end ++;
				*nvram = nvram_start;
				*nvram_len = (int) (nvram_end - nvram_start);
				DBUSTRACE(("found len = %d nvram_start = 0x%p "
					"nvram_end = 0x%p\n", *nvram_len, nvram_start, nvram_end));
				return DBUS_OK;
			}

			len += (strlen(nvram_end) + 1);
			nvram_end += (strlen(nvram_end) + 1);
		}
	}
	return DBUS_JUMBO_NOMATCH;
} /* dbus_select_nvram */

#endif

#if defined(BCM_DNGL_EMBEDIMAGE)

/* store the global osh handle */
static osl_t *osl_handle = NULL;

/** this function is a combination of trx.c and bcmdl.c plus dbus adaptation */
static int
dbus_zlib_decomp(dhd_bus_t *dhd_bus)
{

	int method, flags, len, status;
	unsigned int uncmp_len, uncmp_crc, dec_crc, crc_init;
	struct trx_header *trx, *newtrx;
	unsigned char *file = NULL;
	unsigned char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */
	z_stream d_stream;
	unsigned char unused;
	int actual_len = -1;
	unsigned char *headers;
	unsigned int trxhdrsize, nvramsize, decomp_memsize, i;

	(void)actual_len;
	(void)unused;
	(void)crc_init;

	osl_handle = dhd_bus->pub.osh;
	dhd_bus->orig_fw = NULL;

	headers = dhd_bus->fw;
	/* Extract trx header */
	trx = (struct trx_header *)headers;
	trxhdrsize = sizeof(struct trx_header);

	if (ltoh32(trx->magic) != TRX_MAGIC) {
		DBUSERR(("%s: Error: trx bad hdr %x\n", __FUNCTION__,
			ltoh32(trx->magic)));
		return -1;
	}

	headers += sizeof(struct trx_header);

	if (ltoh32(trx->flag_version) & TRX_UNCOMP_IMAGE) {
		actual_len = ltoh32(trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]) +
		                     sizeof(struct trx_header);
		DBUSERR(("%s: not a compressed image\n", __FUNCTION__));
		return 0;
	} else {
		/* Extract the gzip header info */
		if ((*headers++ != gz_magic[0]) || (*headers++ != gz_magic[1])) {
			DBUSERR(("%s: Error: gzip bad hdr\n", __FUNCTION__));
			return -1;
		}

		method = (int) *headers++;
		flags = (int) *headers++;

		if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
			DBUSERR(("%s: Error: gzip bad hdr not a Z_DEFLATED file\n", __FUNCTION__));
			return -1;
		}
	}

	/* Discard time, xflags and OS code: */
	for (len = 0; len < 6; len++)
		unused = *headers++;

	if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
		len = (uint32) *headers++;
		len += ((uint32)*headers++)<<8;
		/* len is garbage if EOF but the loop below will quit anyway */
		while (len-- != 0) unused = *headers++;
	}

	if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
		while (*headers++ && (*headers != 0));
	}

	if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
		while (*headers++ && (*headers != 0));
	}

	if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
		for (len = 0; len < 2; len++) unused = *headers++;
	}

	headers++;	/* I need this, why ? */

	/* create space for the uncompressed file */
	/* the space is for trx header, uncompressed image  and nvram file */
	/* with typical compression of 0.6, space double of firmware should be ok */

	decomp_memsize = dhd_bus->fwlen * 2;
	dhd_bus->decomp_memsize = decomp_memsize;
	if (!(file = MALLOCZ(osl_handle, decomp_memsize))) {
		DBUSERR(("%s: check_file : failed malloc\n", __FUNCTION__));
		goto err;
	}

	/* Initialise the decompression struct */
	d_stream.next_in = NULL;
	d_stream.avail_in = 0;
	d_stream.next_out = NULL;
	d_stream.avail_out = decomp_memsize - trxhdrsize;
	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	if (inflateInit2(&d_stream, -15) != Z_OK) {
		DBUSERR(("%s: Err: inflateInit2\n", __FUNCTION__));
		goto err;
	}

	/* Inflate the code */
	d_stream.next_in = headers;
	d_stream.avail_in = ltoh32(trx->len);
	d_stream.next_out = (unsigned char*)(file + trxhdrsize);

	status = inflate(&d_stream, Z_SYNC_FLUSH);

	if (status != Z_STREAM_END)	{
		DBUSERR(("%s: Error: decompression failed\n", __FUNCTION__));
		goto err;
	}

	uncmp_crc = *d_stream.next_in++;
	uncmp_crc |= *d_stream.next_in++<<8;
	uncmp_crc |= *d_stream.next_in++<<16;
	uncmp_crc |= *d_stream.next_in++<<24;

	uncmp_len = *d_stream.next_in++;
	uncmp_len |= *d_stream.next_in++<<8;
	uncmp_len |= *d_stream.next_in++<<16;
	uncmp_len |= *d_stream.next_in++<<24;

	actual_len = (int) (d_stream.next_in - (unsigned char *)trx);

	inflateEnd(&d_stream);

	/* Do a CRC32 on the uncompressed data */
	crc_init = crc32(0L, Z_NULL, 0);
	dec_crc = crc32(crc_init, file + trxhdrsize, uncmp_len);

	if (dec_crc != uncmp_crc) {
		DBUSERR(("%s: decompression: bad crc check \n", __FUNCTION__));
		goto err;
	}
	else {
		DBUSTRACE(("%s: decompression: good crc check \n", __FUNCTION__));
	}

	/* rebuild the new trx header and calculate crc */
	newtrx = (struct trx_header *)file;
	newtrx->magic = trx->magic;
	/* add the uncompressed image flag */
	newtrx->flag_version = trx->flag_version;
	newtrx->flag_version  |= htol32(TRX_UNCOMP_IMAGE);
	newtrx->offsets[TRX_OFFSETS_DLFWLEN_IDX] = htol32(uncmp_len);
	newtrx->offsets[TRX_OFFSETS_JUMPTO_IDX] = trx->offsets[TRX_OFFSETS_JUMPTO_IDX];
	newtrx->offsets[TRX_OFFSETS_NVM_LEN_IDX] = trx->offsets[TRX_OFFSETS_NVM_LEN_IDX];

	nvramsize = ltoh32(trx->offsets[TRX_OFFSETS_NVM_LEN_IDX]);

	/* the original firmware has nvram file appended */
	/* copy the nvram file to uncompressed firmware */

	if (nvramsize) {
		if (nvramsize + uncmp_len > decomp_memsize) {
			DBUSERR(("%s: nvram cannot be accomodated\n", __FUNCTION__));
			goto err;
		}
		bcopy(d_stream.next_in, &file[uncmp_len], nvramsize);
		uncmp_len += nvramsize;
	}

	/* add trx header size to uncmp_len */
	uncmp_len += trxhdrsize;
	/* PR14373 WAR: Pad to multiple of 4096 bytes */
	uncmp_len = ROUNDUP(uncmp_len, 4096);
	newtrx->len	= htol32(uncmp_len);

	/* Calculate CRC over header */
	newtrx->crc32 = hndcrc32((uint8 *)&newtrx->flag_version,
	sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
	CRC32_INIT_VALUE);

	/* Calculate CRC over data */
	for (i = trxhdrsize; i < (uncmp_len); ++i)
				newtrx->crc32 = hndcrc32((uint8 *)&file[i], 1, newtrx->crc32);
	newtrx->crc32 = htol32(newtrx->crc32);

	dhd_bus->orig_fw = dhd_bus->fw;
	dhd_bus->origfw_len = dhd_bus->fwlen;
	dhd_bus->image = dhd_bus->fw = file;
	dhd_bus->image_len = dhd_bus->fwlen = uncmp_len;

	return 0;

err:
	if (file)
		free(file);
	return -1;
} /* dbus_zlib_decomp */

void *
dbus_zlib_calloc(int num, int size)
{
	uint *ptr;
	uint totalsize;

	if (osl_handle == NULL)
		return NULL;

	totalsize = (num * (size + 1));

	ptr  = MALLOCZ(osl_handle, totalsize);

	if (ptr == NULL)
		return NULL;

	/* store the size in the first integer space */

	ptr[0] = totalsize;

	return ((void *) &ptr[1]);
}

void
dbus_zlib_free(void *ptr)
{
	uint totalsize;
	uchar *memptr = (uchar *)ptr;

	if (ptr && osl_handle) {
		memptr -= sizeof(uint);
		totalsize = *(uint *) memptr;
		MFREE(osl_handle, memptr, totalsize);
	}
}

#endif /* #if defined(BCM_DNGL_EMBEDIMAGE) */

#define DBUS_NRXQ	50
#define DBUS_NTXQ	100

static void
dhd_dbus_send_complete(void *handle, void *info, int status)
{
	dhd_pub_t *dhd = (dhd_pub_t *)handle;
	void *pkt = info;

	if ((dhd == NULL) || (pkt == NULL)) {
		DBUSERR(("dhd or pkt is NULL\n"));
		return;
	}

	if (status == DBUS_OK) {
		dhd->dstats.tx_packets++;
	} else {
		DBUSERR(("TX error=%d\n", status));
		dhd->dstats.tx_errors++;
	}
#ifdef PROP_TXSTATUS
	if (DHD_PKTTAG_WLFCPKT(PKTTAG(pkt)) &&
		(dhd_wlfc_txcomplete(dhd, pkt, status == 0) != WLFC_UNSUPPORTED)) {
		return;
	} else
#endif /* PROP_TXSTATUS */
	dhd_txcomplete(dhd, pkt, status == 0);
	PKTFREE(dhd->osh, pkt, TRUE);
}

static void
dhd_dbus_recv_pkt(void *handle, void *pkt)
{
	uchar reorder_info_buf[WLHOST_REORDERDATA_TOTLEN];
	uint reorder_info_len;
	uint pkt_count;
	dhd_pub_t *dhd = (dhd_pub_t *)handle;
	int ifidx = 0;

	if (dhd == NULL) {
		DBUSERR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}

	/* If the protocol uses a data header, check and remove it */
	if (dhd_prot_hdrpull(dhd, &ifidx, pkt, reorder_info_buf,
		&reorder_info_len) != 0) {
		DBUSERR(("rx protocol error\n"));
		PKTFREE(dhd->osh, pkt, FALSE);
		dhd->rx_errors++;
		return;
	}

	if (reorder_info_len) {
		/* Reordering info from the firmware */
		dhd_process_pkt_reorder_info(dhd, reorder_info_buf, reorder_info_len,
			&pkt, &pkt_count);
		if (pkt_count == 0)
			return;
	}
	else {
		pkt_count = 1;
	}
	dhd_rx_frame(dhd, ifidx, pkt, pkt_count, 0);
}

static void
dhd_dbus_recv_buf(void *handle, uint8 *buf, int len)
{
	dhd_pub_t *dhd = (dhd_pub_t *)handle;
	void *pkt;

	if (dhd == NULL) {
		DBUSERR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}

	if ((pkt = PKTGET(dhd->osh, len, FALSE)) == NULL) {
		DBUSERR(("PKTGET (rx) failed=%d\n", len));
		return;
	}

	bcopy(buf, PKTDATA(dhd->osh, pkt), len);
	dhd_dbus_recv_pkt(dhd, pkt);
}

static void
dhd_dbus_txflowcontrol(void *handle, bool onoff)
{
	dhd_pub_t *dhd = (dhd_pub_t *)handle;
	bool wlfc_enabled = FALSE;

	if (dhd == NULL) {
		DBUSERR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}

#ifdef PROP_TXSTATUS
	wlfc_enabled = (dhd_wlfc_flowcontrol(dhd, onoff, !onoff) != WLFC_UNSUPPORTED);
#endif

	if (!wlfc_enabled) {
		dhd_txflowcontrol(dhd, ALL_INTERFACES, onoff);
	}
}

static void
dhd_dbus_errhandler(void *handle, int err)
{
}

static void
dhd_dbus_ctl_complete(void *handle, int type, int status)
{
	dhd_pub_t *dhd = (dhd_pub_t *)handle;

	if (dhd == NULL) {
		DBUSERR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}

	if (type == DBUS_CBCTL_READ) {
		if (status == DBUS_OK)
			dhd->rx_ctlpkts++;
		else
			dhd->rx_ctlerrs++;
	} else if (type == DBUS_CBCTL_WRITE) {
		if (status == DBUS_OK)
			dhd->tx_ctlpkts++;
		else
			dhd->tx_ctlerrs++;
	}

	dhd->bus->ctl_completed = TRUE;
	dhd_os_ioctl_resp_wake(dhd);
}

static void
dhd_dbus_state_change(void *handle, int state)
{
	dhd_pub_t *dhd = (dhd_pub_t *)handle;
	unsigned long flags;
	wifi_adapter_info_t *adapter;
	int wowl_dngldown = 0;

	if (dhd == NULL) {
		DBUSERR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}
	adapter = (wifi_adapter_info_t *)dhd->adapter;
#ifdef WL_EXT_WOWL
	wowl_dngldown = dhd_conf_wowl_dngldown(dhd);
#endif

	if ((dhd->busstate == DHD_BUS_SUSPEND && state == DBUS_STATE_DOWN) ||
			(dhd->hostsleep && wowl_dngldown)) {
		DBUSERR(("%s: switch state %d to %d\n", __FUNCTION__, state, DBUS_STATE_SLEEP));
		state = DBUS_STATE_SLEEP;
	}
	switch (state) {
		case DBUS_STATE_DL_NEEDED:
			DBUSERR(("%s: firmware request cannot be handled\n", __FUNCTION__));
			break;
		case DBUS_STATE_DOWN:
			DHD_LINUX_GENERAL_LOCK(dhd, flags);
			dhd_txflowcontrol(dhd, ALL_INTERFACES, ON);
			DBUSTRACE(("%s: DBUS is down\n", __FUNCTION__));
			dhd->busstate = DHD_BUS_DOWN;
			DHD_LINUX_GENERAL_UNLOCK(dhd, flags);
			break;
		case DBUS_STATE_UP:
			DBUSTRACE(("%s: DBUS is up\n", __FUNCTION__));
			DHD_LINUX_GENERAL_LOCK(dhd, flags);
			dhd_txflowcontrol(dhd, ALL_INTERFACES, OFF);
			dhd->busstate = DHD_BUS_DATA;
			DHD_LINUX_GENERAL_UNLOCK(dhd, flags);
			break;
		default:
			break;
	}

	DBUSERR(("%s: DBUS current state=%d\n", __FUNCTION__, state));
}

static void *
dhd_dbus_pktget(void *handle, uint len, bool send)
{
	dhd_pub_t *dhd = (dhd_pub_t *)handle;
	void *p = NULL;

	if (dhd == NULL) {
		DBUSERR(("%s: dhd is NULL\n", __FUNCTION__));
		return NULL;
	}

	if (send == TRUE) {
		dhd_os_sdlock_txq(dhd);
		p = PKTGET(dhd->osh, len, TRUE);
		dhd_os_sdunlock_txq(dhd);
	} else {
		dhd_os_sdlock_rxq(dhd);
		p = PKTGET(dhd->osh, len, FALSE);
		dhd_os_sdunlock_rxq(dhd);
	}

	return p;
}

static void
dhd_dbus_pktfree(void *handle, void *p, bool send)
{
	dhd_pub_t *dhd = (dhd_pub_t *)handle;

	if (dhd == NULL) {
		DBUSERR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}

	if (send == TRUE) {
#ifdef PROP_TXSTATUS
		if (DHD_PKTTAG_WLFCPKT(PKTTAG(p)) &&
			(dhd_wlfc_txcomplete(dhd, p, FALSE) != WLFC_UNSUPPORTED)) {
			return;
		}
#endif /* PROP_TXSTATUS */

		dhd_os_sdlock_txq(dhd);
		PKTFREE(dhd->osh, p, TRUE);
		dhd_os_sdunlock_txq(dhd);
	} else {
		dhd_os_sdlock_rxq(dhd);
		PKTFREE(dhd->osh, p, FALSE);
		dhd_os_sdunlock_rxq(dhd);
	}
}


static dbus_callbacks_t dhd_dbus_cbs = {
	dhd_dbus_send_complete,
	dhd_dbus_recv_buf,
	dhd_dbus_recv_pkt,
	dhd_dbus_txflowcontrol,
	dhd_dbus_errhandler,
	dhd_dbus_ctl_complete,
	dhd_dbus_state_change,
	dhd_dbus_pktget,
	dhd_dbus_pktfree
};

uint
dhd_bus_chip(struct dhd_bus *bus)
{
	ASSERT(bus != NULL);
	return bus->pub.attrib.devid;
}

uint
dhd_bus_chiprev(struct dhd_bus *bus)
{
	ASSERT(bus);
	ASSERT(bus != NULL);
	return bus->pub.attrib.chiprev;
}

struct device *
dhd_bus_to_dev(struct dhd_bus *bus)
{
	return dbus_get_dev();
}

void
dhd_bus_dump(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	bcm_bprintf(strbuf, "Bus USB\n");
}

void
dhd_bus_clearcounts(dhd_pub_t *dhdp)
{
}

int
dhd_bus_txdata(struct dhd_bus *bus, void *pktbuf)
{
	DBUSTRACE(("%s\n", __FUNCTION__));
	if (bus->txoff) {
		DBUSTRACE(("txoff\n"));
		return BCME_EPERM;
	}
	return dbus_send_pkt(&bus->pub, pktbuf, pktbuf);
}

int
dhd_bus_txctl(struct dhd_bus *bus, uchar *msg, uint msglen)
{
	int timeleft = 0;
	int ret = -1;

	DBUSTRACE(("%s: Enter\n", __FUNCTION__));

	if (bus->dhd->dongle_reset)
		return -EIO;

	bus->ctl_completed = FALSE;
	ret = dbus_send_ctl(bus, (void *)msg, msglen);
	if (ret) {
		DBUSERR(("%s: dbus_send_ctl error %d\n", __FUNCTION__, ret));
		return ret;
	}

	timeleft = dhd_os_ioctl_resp_wait(bus->dhd, &bus->ctl_completed);
	if ((!timeleft) || (!bus->ctl_completed)) {
		DBUSERR(("%s: Txctl timeleft %d ctl_completed %d\n",
			__FUNCTION__, timeleft, bus->ctl_completed));
		ret = -1;
#ifdef OEM_ANDROID
		bus->dhd->hang_reason = HANG_REASON_IOCTL_RESP_TIMEOUT_SCHED_ERROR;
		dhd_os_send_hang_message(bus->dhd);
#endif /* OEM_ANDROID */
	}

#ifdef INTR_EP_ENABLE
	/* If the ctl write is successfully completed, wait for an acknowledgement
	* that indicates that it is now ok to do ctl read from the dongle
	*/
	if (ret != -1) {
		bus->ctl_completed = FALSE;
		if (dbus_poll_intr(bus->pub)) {
			DBUSERR(("%s: dbus_poll_intr not submitted\n", __FUNCTION__));
		} else {
			/* interrupt polling is sucessfully submitted. Wait for dongle to send
			* interrupt
			*/
			timeleft = dhd_os_ioctl_resp_wait(bus->dhd, &bus->ctl_completed);
			if (!timeleft) {
				DBUSERR(("%s: intr poll wait timed out\n", __FUNCTION__));
			}
		}
	}
#endif /* INTR_EP_ENABLE */

	return ret;
}

int
dhd_bus_rxctl(struct dhd_bus *bus, uchar *msg, uint msglen)
{
	int timeleft;
	int ret = -1;

	DBUSTRACE(("%s: Enter\n", __FUNCTION__));

	if (bus->dhd->dongle_reset)
		return -EIO;

	bus->ctl_completed = FALSE;
	ret = dbus_recv_ctl(bus, (uchar*)msg, msglen);
	if (ret) {
		DBUSERR(("%s: dbus_recv_ctl error %d\n", __FUNCTION__, ret));
		goto done;
	}

	timeleft = dhd_os_ioctl_resp_wait(bus->dhd, &bus->ctl_completed);
	if ((!timeleft) || (!bus->ctl_completed)) {
		DBUSERR(("%s: Rxctl timeleft %d ctl_completed %d\n", __FUNCTION__,
			timeleft, bus->ctl_completed));
		ret = -ETIMEDOUT;
		goto done;
	}

	/* XXX FIX: Must return cdc_len, not len, because after query_ioctl()
	 * it subtracts sizeof(cdc_ioctl_t);  The other approach is
	 * to have dbus_recv_ctl() return actual len.
	 */
	ret = msglen;

done:
	return ret;
}

static void
dhd_dbus_advertise_bus_cleanup(dhd_pub_t *dhdp)
{
	unsigned long flags;
	int timeleft;

	DHD_LINUX_GENERAL_LOCK(dhdp, flags);
	dhdp->busstate = DHD_BUS_DOWN_IN_PROGRESS;
	DHD_LINUX_GENERAL_UNLOCK(dhdp, flags);

	timeleft = dhd_os_busbusy_wait_negation(dhdp, &dhdp->dhd_bus_busy_state);
	if ((timeleft == 0) || (timeleft == 1)) {
		DBUSERR(("%s : Timeout due to dhd_bus_busy_state=0x%x\n",
				__FUNCTION__, dhdp->dhd_bus_busy_state));
		ASSERT(0);
	}

	return;
}

static void
dhd_dbus_advertise_bus_remove(dhd_pub_t *dhdp)
{
	unsigned long flags;
	int timeleft;

	DHD_LINUX_GENERAL_LOCK(dhdp, flags);
	if (dhdp->busstate != DHD_BUS_SUSPEND)
		dhdp->busstate = DHD_BUS_REMOVE;
	DHD_LINUX_GENERAL_UNLOCK(dhdp, flags);

	timeleft = dhd_os_busbusy_wait_negation(dhdp, &dhdp->dhd_bus_busy_state);
	if ((timeleft == 0) || (timeleft == 1)) {
		DBUSERR(("%s : Timeout due to dhd_bus_busy_state=0x%x\n",
				__FUNCTION__, dhdp->dhd_bus_busy_state));
		ASSERT(0);
	}

	return;
}

int
dhd_bus_devreset(dhd_pub_t *dhdp, uint8 flag)
{
	int bcmerror = 0;
	unsigned long flags;
	wifi_adapter_info_t *adapter = (wifi_adapter_info_t *)dhdp->adapter;

	if (flag == TRUE) {
		if (!dhdp->dongle_reset) {
			DBUSERR(("%s: == Power OFF ==\n", __FUNCTION__));
			dhd_dbus_advertise_bus_cleanup(dhdp);
			dhd_os_wd_timer(dhdp, 0);
#if !defined(IGNORE_ETH0_DOWN)
			/* Force flow control as protection when stop come before ifconfig_down */
			dhd_txflowcontrol(dhdp, ALL_INTERFACES, ON);
#endif /* !defined(IGNORE_ETH0_DOWN) */
			dbus_stop(dhdp->bus);

			dhdp->dongle_reset = TRUE;
			dhdp->up = FALSE;

			DHD_LINUX_GENERAL_LOCK(dhdp, flags);
			dhdp->busstate = DHD_BUS_DOWN;
			DHD_LINUX_GENERAL_UNLOCK(dhdp, flags);
			wifi_clr_adapter_status(adapter, WIFI_STATUS_FW_READY);

			printf("%s:  WLAN OFF DONE\n", __FUNCTION__);
			/* App can now remove power from device */
		} else
			bcmerror = BCME_ERROR;
	} else {
		/* App must have restored power to device before calling */
		printf("\n\n%s: == WLAN ON ==\n", __FUNCTION__);
		if (dhdp->dongle_reset) {
			/* Turn on WLAN */
			DHD_MUTEX_UNLOCK();
			wait_event_interruptible_timeout(adapter->status_event,
				wifi_get_adapter_status(adapter, WIFI_STATUS_FW_READY),
				msecs_to_jiffies(DHD_FW_READY_TIMEOUT));
			DHD_MUTEX_LOCK();
			bcmerror = dbus_up(dhdp->bus);
			if (bcmerror == BCME_OK) {
				dhdp->dongle_reset = FALSE;
				dhdp->up = TRUE;
#if !defined(IGNORE_ETH0_DOWN)
				/* Restore flow control  */
				dhd_txflowcontrol(dhdp, ALL_INTERFACES, OFF);
#endif
				dhd_os_wd_timer(dhdp, dhd_watchdog_ms);

				DBUSTRACE(("%s: WLAN ON DONE\n", __FUNCTION__));
			} else {
				DBUSERR(("%s: failed to dbus_up with code %d\n", __FUNCTION__, bcmerror));
			}
		}
	}

	return bcmerror;
}

void
dhd_bus_update_fw_nv_path(struct dhd_bus *bus, char *pfw_path,
	char *pnv_path, char *pclm_path, char *pconf_path)
{
	DBUSTRACE(("%s\n", __FUNCTION__));

	if (bus == NULL) {
		DBUSERR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}

	bus->fw_path = pfw_path;
	bus->nv_path = pnv_path;
	bus->dhd->clm_path = pclm_path;
	bus->dhd->conf_path = pconf_path;

	dhd_conf_set_path_params(bus->dhd, bus->fw_path, bus->nv_path);

}

static int
dhd_dbus_sync_dongle(dhd_pub_t *pub, int dlneeded)
{
	int ret = 0;

	if (dlneeded == 0) {
		ret = dbus_up(pub->bus);
		if (ret) {
			DBUSERR(("%s: dbus_up failed!!\n", __FUNCTION__));
			goto exit;
		}
		ret = dhd_sync_with_dongle(pub);
		if (ret < 0) {
			DBUSERR(("%s: failed with code ret=%d\n", __FUNCTION__, ret));
			goto exit;
		}
	}

exit:
	return ret;
}

static int
dbus_suspend(void *context)
{
	int ret = 0;

#if defined(LINUX)
	dhd_bus_t *bus = (dhd_bus_t*)context;
	unsigned long flags;

	DBUSERR(("%s Enter\n", __FUNCTION__));
	if (bus->dhd == NULL) {
		DBUSERR(("bus not inited\n"));
		return BCME_ERROR;
	}
	if (bus->dhd->prot == NULL) {
		DBUSERR(("prot is not inited\n"));
		return BCME_ERROR;
	}

	if (bus->dhd->up == FALSE) {
		return BCME_OK;
	}

	DHD_LINUX_GENERAL_LOCK(bus->dhd, flags);
	if (bus->dhd->busstate != DHD_BUS_DATA && bus->dhd->busstate != DHD_BUS_SUSPEND) {
		DBUSERR(("not in a readystate to LPBK  is not inited\n"));
		DHD_LINUX_GENERAL_UNLOCK(bus->dhd, flags);
		return BCME_ERROR;
	}
	DHD_LINUX_GENERAL_UNLOCK(bus->dhd, flags);
	if (bus->dhd->dongle_reset) {
		DBUSERR(("Dongle is in reset state.\n"));
		return -EIO;
	}

	DHD_LINUX_GENERAL_LOCK(bus->dhd, flags);
	/* stop all interface network queue. */
	dhd_txflowcontrol(bus->dhd, ALL_INTERFACES, ON);
	bus->dhd->busstate = DHD_BUS_SUSPEND;
#if defined(LINUX) || defined(linux)
	if (DHD_BUS_BUSY_CHECK_IN_TX(bus->dhd)) {
		DBUSERR(("Tx Request is not ended\n"));
		bus->dhd->busstate = DHD_BUS_DATA;
		/* resume all interface network queue. */
		dhd_txflowcontrol(bus->dhd, ALL_INTERFACES, OFF);
		DHD_LINUX_GENERAL_UNLOCK(bus->dhd, flags);
		return -EBUSY;
	}
#endif /* LINUX || linux */
	DHD_BUS_BUSY_SET_SUSPEND_IN_PROGRESS(bus->dhd);
	DHD_LINUX_GENERAL_UNLOCK(bus->dhd, flags);

	ret = dhd_os_check_wakelock_all(bus->dhd);

	DHD_LINUX_GENERAL_LOCK(bus->dhd, flags);
	if (ret) {
		bus->dhd->busstate = DHD_BUS_DATA;
		/* resume all interface network queue. */
		dhd_txflowcontrol(bus->dhd, ALL_INTERFACES, OFF);
	} else {
		bus->last_suspend_end_time = OSL_LOCALTIME_NS();
	}
	bus->dhd->hostsleep = 2;
	DHD_BUS_BUSY_CLEAR_SUSPEND_IN_PROGRESS(bus->dhd);
	dhd_os_busbusy_wake(bus->dhd);
	DHD_LINUX_GENERAL_UNLOCK(bus->dhd, flags);

#endif /* LINUX */
	DBUSERR(("%s Exit ret=%d\n", __FUNCTION__, ret));
	return ret;
}

static int
dbus_resume(void *context)
{
	dhd_bus_t *bus = (dhd_bus_t*)context;
	ulong flags;
	int dlneeded = 0;
	int ret = 0;

	DBUSERR(("%s Enter\n", __FUNCTION__));

	if (bus->dhd->up == FALSE) {
		return BCME_OK;
	}

	dlneeded = dbus_dlneeded(bus);
	if (dlneeded == 0) {
		ret = dbus_up(bus);
		if (ret) {
			DBUSERR(("%s: dbus_up failed!!\n", __FUNCTION__));
		}
	}

	DHD_LINUX_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_SET_RESUME_IN_PROGRESS(bus->dhd);
	DHD_LINUX_GENERAL_UNLOCK(bus->dhd, flags);

	DHD_LINUX_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_CLEAR_RESUME_IN_PROGRESS(bus->dhd);
	bus->dhd->hostsleep = 0;
	bus->dhd->busstate = DHD_BUS_DATA;
	dhd_os_busbusy_wake(bus->dhd);
	/* resume all interface network queue. */
	dhd_txflowcontrol(bus->dhd, ALL_INTERFACES, OFF);
	DHD_LINUX_GENERAL_UNLOCK(bus->dhd, flags);
//	dhd_conf_set_suspend_resume(bus->dhd, 0);

	return 0;
}

/*
 * hdrlen is space to reserve in pkt headroom for DBUS
 */
static void *
dhd_dbus_probe_cb(uint16 bus_no, uint16 slot, uint32 hdrlen)
{
	osl_t *osh = NULL;
	dhd_bus_t *bus = NULL;
	dhd_pub_t *pub = NULL;
	uint rxsz;
	int dlneeded = 0, ret = DBUS_OK;
	wifi_adapter_info_t *adapter = NULL;
	bool net_attach_now = TRUE;

	DBUSTRACE(("%s: Enter\n", __FUNCTION__));

	adapter = dhd_wifi_platform_get_adapter(USB_BUS, bus_no, slot);

	if (!g_pub) {
		/* Ask the OS interface part for an OSL handle */
		if (!(osh = osl_attach(NULL, USB_BUS, TRUE))) {
			DBUSERR(("%s: OSL attach failed\n", __FUNCTION__));
			goto fail;
		}

		/* Attach to the dhd/OS interface */
		if (!(pub = dhd_attach(osh, bus, hdrlen, adapter))) {
			DBUSERR(("%s: dhd_attach failed\n", __FUNCTION__));
			goto fail;
		}
	} else {
		pub = g_pub;
		osh = pub->osh;
	}

	if (pub->bus) {
		DBUSERR(("%s: wrong probe\n", __FUNCTION__));
		goto fail;
	}

	rxsz = dhd_get_rxsz(pub);
	bus = dbus_attach(osh, rxsz, DBUS_NRXQ, DBUS_NTXQ, pub, &dhd_dbus_cbs, NULL, NULL);
	if (bus) {
		pub->bus = bus;
		bus->dhd = pub;

		dlneeded = dbus_dlneeded(bus);
		if (dlneeded >= 0 && !g_pub) {
			dhd_conf_reset(pub);
			dhd_conf_set_chiprev(pub, bus->pub.attrib.devid, bus->pub.attrib.chiprev);
			dhd_conf_preinit(pub);
		}

#if defined(BCMDHD_MODULAR) && defined(INSMOD_FW_LOAD)
		if (1)
#else
		if (g_pub || dhd_download_fw_on_driverload)
#endif
		{
			if (dlneeded == 0)
				wifi_set_adapter_status(adapter, WIFI_STATUS_FW_READY);
#ifdef BCM_REQUEST_FW
			else if (dlneeded > 0) {
				struct dhd_conf *conf = pub->conf;
				unsigned long flags;
				bool suspended;
				wifi_clr_adapter_status(adapter, WIFI_STATUS_FW_READY);
				suspended = conf->suspended;
				dhd_set_path(bus->dhd);
				conf->suspended = suspended;
				if (dbus_download_firmware(bus) != DBUS_OK)
					goto fail;
				DHD_LINUX_GENERAL_LOCK(pub, flags);
				if (bus->dhd->busstate != DHD_BUS_SUSPEND)
					bus->dhd->busstate = DHD_BUS_LOAD;
				DHD_LINUX_GENERAL_UNLOCK(pub, flags);
			}
#endif
			else {
				goto fail;
			}
		}
	}
	else {
		DBUSERR(("%s: dbus_attach failed\n", __FUNCTION__));
		goto fail;
	}

#if defined(BCMDHD_MODULAR) && defined(INSMOD_FW_LOAD)
	if (dlneeded > 0)
		net_attach_now = FALSE;
#endif

	if (!net_attached && (net_attach_now || (dlneeded == 0))) {
		if (dhd_dbus_sync_dongle(pub, dlneeded)) {
			goto fail;
		}
		if (dhd_attach_net(bus->dhd, TRUE) != 0) {
			DBUSERR(("%s: Net attach failed!!\n", __FUNCTION__));
			goto fail;
		}
		pub->hang_report  = TRUE;
#if defined(MULTIPLE_SUPPLICANT)
		wl_android_post_init(); // terence 20120530: fix critical section in dhd_open and dhdsdio_probe
#endif
		net_attached = TRUE;
	}
	else if (net_attached && (pub->up == 1) && (dlneeded == 0)) {
		// kernel resume case
		pub->hostsleep = 0;
		ret = dhd_dbus_sync_dongle(pub, dlneeded);
#ifdef WL_CFG80211
		__wl_cfg80211_up_resume(pub);
		wl_cfgp2p_start_p2p_device_resume(pub);
#endif
		dhd_conf_set_suspend_resume(pub, 0);
		if (ret != DBUS_OK)
			goto fail;
	}

	if (!g_pub) {
		g_pub = pub;
	}

	DBUSTRACE(("%s: Exit\n", __FUNCTION__));
	wifi_clr_adapter_status(adapter, WIFI_STATUS_BUS_DISCONNECTED);
	if (net_attached) {
		wifi_set_adapter_status(adapter, WIFI_STATUS_NET_ATTACHED);
		wake_up_interruptible(&adapter->status_event);
		/* This is passed to dhd_dbus_disconnect_cb */
	}
	return bus;

fail:
	if (pub && pub->bus) {
		dbus_detach(pub->bus);
		pub->bus = NULL;
	}
	/* Release resources in reverse order */
	if (!g_pub) {
		if (pub) {
			dhd_detach(pub);
			dhd_free(pub);
		}
		if (osh) {
			osl_detach(osh);
		}
	}

	printf("%s: Failed\n", __FUNCTION__);
	return NULL;
}

static void
dhd_dbus_disconnect_cb(void *arg)
{
	dhd_bus_t *bus = (dhd_bus_t *)arg;
	dhd_pub_t *pub = g_pub;
	osl_t *osh;
	wifi_adapter_info_t *adapter = NULL;

	adapter = (wifi_adapter_info_t *)pub->adapter;

	if (pub && !pub->dhd_remove && bus == NULL) {
		DBUSERR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}
	if (!adapter) {
		DBUSERR(("%s: adapter is NULL\n", __FUNCTION__));
		return;
	}

	printf("%s: Enter dhd_remove=%d on %s\n", __FUNCTION__,
		pub->dhd_remove, adapter->name);
	if (!pub->dhd_remove) {
		/* Advertise bus remove during rmmod */
		dhd_dbus_advertise_bus_remove(bus->dhd);
		dbus_detach(pub->bus);
		pub->bus = NULL;
		wifi_set_adapter_status(adapter, WIFI_STATUS_BUS_DISCONNECTED);
		wake_up_interruptible(&adapter->status_event);
	} else {
		osh = pub->osh;
		dhd_detach(pub);
		if (pub->bus) {
			dbus_detach(pub->bus);
			pub->bus = NULL;
		}
		dhd_free(pub);
		g_pub = NULL;
		net_attached = FALSE;
		wifi_clr_adapter_status(adapter, WIFI_STATUS_NET_ATTACHED);
		if (MALLOCED(osh)) {
			DBUSERR(("%s: MEMORY LEAK %d bytes\n", __FUNCTION__, MALLOCED(osh)));
		}
		osl_detach(osh);
	}

	DBUSTRACE(("%s: Exit\n", __FUNCTION__));
}

int
dhd_bus_sleep(dhd_pub_t *dhdp, bool sleep, uint32 *intstatus)
{
	wifi_adapter_info_t *adapter = (wifi_adapter_info_t *)dhdp->adapter;
	s32 timeout = -1;
	int err = 0;

	timeout = wait_event_interruptible_timeout(adapter->status_event,
		wifi_get_adapter_status(adapter, WIFI_STATUS_BUS_DISCONNECTED),
		msecs_to_jiffies(12000));
	if (timeout <= 0) {
		err = -1;
		DBUSERR(("%s: bus disconnected timeout\n", __FUNCTION__));
	}

	return err;
}

#ifdef LINUX_EXTERNAL_MODULE_DBUS

static int __init
bcm_dbus_module_init(void)
{
	printf("Inserting bcm_dbus module \n");
	return 0;
}

static void __exit
bcm_dbus_module_exit(void)
{
	printf("Removing bcm_dbus module \n");
	return;
}

EXPORT_SYMBOL(dbus_pnp_sleep);
EXPORT_SYMBOL(dhd_bus_register);
EXPORT_SYMBOL(dbus_get_devinfo);
#ifdef BCMDBG
EXPORT_SYMBOL(dbus_hist_dump);
#endif
EXPORT_SYMBOL(dbus_detach);
EXPORT_SYMBOL(dbus_get_attrib);
EXPORT_SYMBOL(dbus_down);
EXPORT_SYMBOL(dbus_pnp_resume);
EXPORT_SYMBOL(dbus_set_config);
EXPORT_SYMBOL(dbus_flowctrl_rx);
EXPORT_SYMBOL(dbus_up);
EXPORT_SYMBOL(dbus_get_device_speed);
EXPORT_SYMBOL(dbus_send_pkt);
EXPORT_SYMBOL(dbus_recv_ctl);
EXPORT_SYMBOL(dbus_attach);
EXPORT_SYMBOL(dhd_bus_unregister);

MODULE_LICENSE("GPL");

module_init(bcm_dbus_module_init);
module_exit(bcm_dbus_module_exit);

#endif  /* #ifdef LINUX_EXTERNAL_MODULE_DBUS */
