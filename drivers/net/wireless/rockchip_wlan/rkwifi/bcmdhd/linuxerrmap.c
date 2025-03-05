/*
 * Linux Error codes
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
#include <typedefs.h>
#include <osl.h>
#include <epivers.h>
#include <bcmutils.h>

#include <linuxerrmap.h>

static int16 linuxbcmerrormap[] = {
	0,			/* 0 */
	-EINVAL,		/* BCME_ERROR */
	-EINVAL,		/* BCME_BADARG */
	-EINVAL,		/* BCME_BADOPTION */
	-EINVAL,		/* BCME_NOTUP */
	-EINVAL,		/* BCME_NOTDOWN */
	-EINVAL,		/* BCME_NOTAP */
	-EINVAL,		/* BCME_NOTSTA */
	-EINVAL,		/* BCME_BADKEYIDX */
	-EINVAL,		/* BCME_RADIOOFF */
	-EINVAL,		/* BCME_NOTBANDLOCKED */
	-EINVAL,		/* BCME_NOCLK */
	-EINVAL,		/* BCME_BADRATESET */
	-EINVAL,		/* BCME_BADBAND */
	-E2BIG,			/* BCME_BUFTOOSHORT */
	-E2BIG,			/* BCME_BUFTOOLONG */
	-EBUSY,			/* BCME_BUSY */
	-EINVAL,		/* BCME_NOTASSOCIATED */
	-EINVAL,		/* BCME_BADSSIDLEN */
	-EINVAL,		/* BCME_OUTOFRANGECHAN */
	-EINVAL,		/* BCME_BADCHAN */
	-EFAULT,		/* BCME_BADADDR */
	-ENOMEM,		/* BCME_NORESOURCE */
	-EOPNOTSUPP,		/* BCME_UNSUPPORTED */
	-EMSGSIZE,		/* BCME_BADLENGTH */
	-EINVAL,		/* BCME_NOTREADY */
	-EPERM,			/* BCME_EPERM */
	-ENOMEM,		/* BCME_NOMEM */
	-EINVAL,		/* BCME_ASSOCIATED */
	-ERANGE,		/* BCME_RANGE */
	-EINVAL,		/* BCME_NOTFOUND */
	-EINVAL,		/* BCME_WME_NOT_ENABLED */
	-EINVAL,		/* BCME_TSPEC_NOTFOUND */
	-EINVAL,		/* BCME_ACM_NOTSUPPORTED */
	-EINVAL,		/* BCME_NOT_WME_ASSOCIATION */
	-EIO,			/* BCME_SDIO_ERROR */
	-ENODEV,		/* BCME_DONGLE_DOWN */
	-EINVAL,		/* BCME_VERSION */
	-EIO,			/* BCME_TXFAIL */
	-EIO,			/* BCME_RXFAIL */
	-ENODEV,		/* BCME_NODEVICE */
	-EINVAL,		/* BCME_NMODE_DISABLED */
	-ENODATA,		/* BCME_NONRESIDENT */
	-EINVAL,		/* BCME_SCANREJECT */
	-EINVAL,		/* BCME_USAGE_ERROR */
	-EIO,			/* BCME_IOCTL_ERROR */
	-EIO,			/* BCME_SERIAL_PORT_ERR */
	-EOPNOTSUPP,		/* BCME_DISABLED, BCME_NOTENABLED */
	-EIO,			/* BCME_DECERR */
	-EIO,			/* BCME_ENCERR */
	-EIO,			/* BCME_MICERR */
	-ERANGE,		/* BCME_REPLAY */
	-EINVAL,		/* BCME_IE_NOTFOUND */
	-EINVAL,		/* BCME_DATA_NOTFOUND */
	-EINVAL,		/* BCME_NOT_GC */
	-EINVAL,		/* BCME_PRS_REQ_FAILED */
	-EINVAL,		/* BCME_NO_P2P_SE */
	-EINVAL,		/* BCME_NOA_PND */
	-EINVAL,		/* BCME_FRAG_Q_FAILED */
	-EINVAL,		/* BCME_GET_AF_FAILED */
	-EINVAL,		/* BCME_MSCH_NOTREADY */
	-EINVAL,		/* BCME_IOV_LAST_CMD */
	-EINVAL,		/* BCME_MINIPMU_CAL_FAIL */
	-EINVAL,		/* BCME_RCAL_FAIL */
	-EINVAL,		/* BCME_LPF_RCCAL_FAIL */
	-EINVAL,		/* BCME_DACBUF_RCCAL_FAIL */
	-EINVAL,		/* BCME_VCOCAL_FAIL */
	-EINVAL,		/* BCME_BANDLOCKED */
	-EINVAL,		/* BCME_BAD_IE_DATA */
	-EINVAL,		/* BCME_REG_FAILED */
	-EINVAL,		/* BCME_NOCHAN */
	-EINVAL,		/* BCME_PKTTOSS */
	-EINVAL,		/* BCME_DNGL_DEVRESET */
	-EINVAL,		/* BCME_ROAM */
	-EOPNOTSUPP,		/* BCME_NO_SIG_FILE */
	-EOPNOTSUPP,		/* BCME_RESP_PENDING */
	-EINVAL,		/* BCME_ACTIVE */
	-EINVAL,		/* BCME_IN_PROGRESS */
	-EINVAL,		/* BCME_NOP */
	-EINVAL,		/* BCME_6GCH_EPERM */
	-EINVAL,		/* BCME_6G_NO_TPE */

/* When an new error code is added to bcmutils.h, add os
 * specific error translation here as well
 */
/* check if BCME_LAST changed since the last time this function was updated */
#if BCME_LAST != BCME_6G_NO_TPE
#error "You need to add a OS error translation in the linuxbcmerrormap \
	for new error code defined in bcmutils.h"
#endif
};

/* translate bcmerrors into linux errors */
int
linux_get_errmap(int bcmerror)
{
	return linuxbcmerrormap[-bcmerror];
}
