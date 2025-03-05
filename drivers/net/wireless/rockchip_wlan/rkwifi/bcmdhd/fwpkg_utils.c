/*
 * Firmware package functionality
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

#ifndef BCMDRIVER
#include <stdio.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0))
#include <stdarg.h>
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0) */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#endif /* BCMDRIVER */

#include <typedefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmstdlib_s.h>
#include <dhd.h>
#ifdef BCMDRIVER
#include <dhd_dbg.h>
#else
#include <errno.h>
#endif /* BCMDRIVER */
#include <fwpkg_utils.h>

#define FWPKG_UNIT_IDX(tag)	(tag-1)

const uint32 FWPKG_HDR_MGCW0 = 0xDEAD2BAD;
const uint32 FWPKG_HDR_MGCW1 = 0xFEE1DEAD;
const uint32 FWPKG_MAX_SUPPORTED_VER = 1;

#ifdef BCMDRIVER
#define FWPKG_ERR(msg)	DHD_ERROR(msg)
#define fwpkg_open(filename)	\
	dhd_os_open_image1(NULL, filename)
#define fwpkg_close(file)	\
	dhd_os_close_image1(NULL, (char *)file)
#define fwpkg_seek(file, offset)	\
	dhd_os_seek_file(file, offset)
#define fwpkg_read(buf, len, file)	\
	dhd_os_get_image_block(buf, len, file)
#define fwpkg_getsize(file)	\
	dhd_os_get_image_size(file)

#else
#if defined(_WIN32)
#define FWPKG_ERR(fmt, ...)
#else
#define FWPKG_ERR(fmt, args...)
#endif /* defined _WIN32 */
#define fwpkg_open(filename)	\
	(void *)fopen(filename, "rb")
#define fwpkg_close(file)	\
	fclose((FILE *)file)
#define fwpkg_seek(file, offset)	\
	fseek((FILE *)file, offset, SEEK_SET)
#define fwpkg_read(buf, len, file)	\
	app_fread(buf, len, file)
#define fwpkg_getsize(file)	\
	app_getsize(file)

#endif /* BCMDRIVER */

#define UNIT_TYPE_IDX(type)	((type) - 1)

static int fwpkg_hdr_validation(fwpkg_hdr_t *hdr, uint32 pkg_len);
static bool fwpkg_parse_rtlvs(fwpkg_info_t *fwpkg, uint32 file_size, char *fname);
static int fwpkg_parse(fwpkg_info_t *fwpkg, char *fname);
static int fwpkg_open_unit(fwpkg_info_t *fwpkg, char *fname,
	uint32 unit_type, FWPKG_FILE **fp);
static uint32 fwpkg_get_unit_size(fwpkg_info_t *fwpkg, uint32 unit_type);

/* open file, if combined fw package parse common header,
 * parse each unit header, keep information
 */
int
fwpkg_init(fwpkg_info_t *fwpkg, char *fname)
{
	if (fwpkg == NULL || fname == NULL) {
		FWPKG_ERR(("fwpkg_init: missing argument\n"));
		return BCME_ERROR;
	}
	/* if status already set, no need to initialize */
	if (fwpkg->status) {
		return BCME_OK;
	}
	bzero(fwpkg, sizeof(*fwpkg));

	return fwpkg_parse(fwpkg, fname);
}

int
fwpkg_open_firmware_img(fwpkg_info_t *fwpkg, char *fname, FWPKG_FILE **fp)
{
	if (fwpkg == NULL || fname == NULL) {
		FWPKG_ERR(("fwpkg_open_firmware: missing argument\n"));
		return BCME_ERROR;
	}

	return fwpkg_open_unit(fwpkg, fname, FWPKG_TAG_FW, fp);
}

int
fwpkg_open_signature_img(fwpkg_info_t *fwpkg, char *fname, FWPKG_FILE **fp)
{
	if (fwpkg == NULL || fname == NULL) {
		FWPKG_ERR(("fwpkg_open_signature: missing argument\n"));
		return BCME_ERROR;
	}

	return fwpkg_open_unit(fwpkg, fname, FWPKG_TAG_SIG, fp);
}

uint32
fwpkg_get_firmware_img_size(fwpkg_info_t *fwpkg)
{
	if (fwpkg == NULL) {
		FWPKG_ERR(("fwpkg_get_firmware_size: missing argument\n"));
		return 0;
	}

	return fwpkg_get_unit_size(fwpkg, FWPKG_TAG_FW);
}

uint32
fwpkg_get_signature_img_size(fwpkg_info_t *fwpkg)
{
	if (fwpkg == NULL) {
		FWPKG_ERR(("fwpkg_get_signature_img_size: missing argument\n"));
		return 0;
	}

	return fwpkg_get_unit_size(fwpkg, FWPKG_TAG_SIG);
}

#ifndef BCMDRIVER
static int
app_getsize(FILE *file)
{
	int len = 0;

	fseek(file, 0, SEEK_END);
	len = (int)ftell(file);
	fseek(file, 0, SEEK_SET);

	return len;
}

static int
app_fread(char *buf, int len, void *file)
{
	int status;

	status = fread(buf, sizeof(uint8), len, (FILE *)file);
	if (status < len) {
		status = -EINVAL;
	}

	return status;
}
#endif /* !BCMDRIVER */

/* check package header information */
static int
fwpkg_hdr_validation(fwpkg_hdr_t *hdr, uint32 pkg_len)
{
	int ret = BCME_ERROR;
	uint32 len = 0;

	/* check megic word0 */
	len += sizeof(hdr->magic_word0);
	if (hdr->magic_word0 != FWPKG_HDR_MGCW0) {
		ret = BCME_UNSUPPORTED;
		goto done;
	}
	/* check megic word1 */
	len += sizeof(hdr->magic_word1);
	if (hdr->magic_word1 != FWPKG_HDR_MGCW1) {
		goto done;
	}
	/* check length */
	len += sizeof(hdr->length);
	if (hdr->length != (pkg_len - len)) {
		goto done;
	}
	/* check version */
	if (hdr->version > FWPKG_MAX_SUPPORTED_VER) {
		goto done;
	}

	ret = BCME_OK;
done:
	return ret;
}

/* parse rtlvs in combined fw package */
static bool
fwpkg_parse_rtlvs(fwpkg_info_t *fwpkg, uint32 file_size, char *fname)
{
	bool ret = FALSE;
	const uint32 l_len = sizeof(uint32);		/* len of rTLV's field length */
	const uint32 t_len = sizeof(uint32);		/* len of rTLV's field type */
	uint32 unit_size = 0, unit_type = 0;
	FWPKG_FILE *file = NULL;
	uint32 left_size = file_size - sizeof(fwpkg_hdr_t);

	while (left_size) {
		file = fwpkg_open(fname);
		if (file == NULL) {
			FWPKG_ERR(("fwpkg_parse_rtlvs: open file fails\n"));
			goto done;
		}

		/* remove length of rTLV's fields type, length */
		left_size -= (t_len + l_len);
		if (fwpkg_seek(file, left_size) != BCME_OK) {
			FWPKG_ERR(("fwpkg_parse_rtlvs: can't get to the rtlv position\n"));
			goto done;
		}
		if (fwpkg_read((char *)&unit_size, l_len, file) < 0) {
			FWPKG_ERR(("fwpkg_parse_rtlvs: can't read rtlv data len\n"));
			goto done;
		}
		if (fwpkg_read((char *)&unit_type, t_len, file) < 0) {
			FWPKG_ERR(("fwpkg_parse_rtlvs: can't read rtlv data type\n"));
			goto done;
		}
		if ((unit_type == 0) || (unit_type >= FWPKG_TAG_LAST)) {
			FWPKG_ERR(("fwpkg_parse_rtlvs: unsupported data type(%d)\n",
				unit_type));
			goto done;
		}
		fwpkg->units[UNIT_TYPE_IDX(unit_type)].type = unit_type;
		fwpkg->units[UNIT_TYPE_IDX(unit_type)].size = unit_size;
		left_size -= unit_size;
		fwpkg->units[UNIT_TYPE_IDX(unit_type)].offset = left_size;

		FWPKG_ERR(("fwpkg_parse_rtlvs: type x%04x, len %d, off %d\n",
			unit_type, unit_size, left_size));

		fwpkg_close(file);
		file = NULL;
	}

	ret = TRUE;
done:
	if (file) {
		fwpkg_close(file);
	}
	return ret;
}

/* parse file if is combined fw package */
static int
fwpkg_parse(fwpkg_info_t *fwpkg, char *fname)
{
	int ret = BCME_ERROR;
	int file_size = 0;
	fwpkg_hdr_t hdr = {0};
	FWPKG_FILE *file = NULL;

	file = fwpkg_open(fname);
	if (file == NULL) {
		FWPKG_ERR(("fwpkg_parse: open file %s fails\n", fname));
		goto done;
	}
	file_size = fwpkg_getsize(file);
	if (!file_size) {
		FWPKG_ERR(("fwpkg_parse: get file size fails\n"));
		goto done;
	}
	/* seek to the last sizeof(fwpkg_hdr_t) bytes in the file */
	if (fwpkg_seek(file, file_size-sizeof(fwpkg_hdr_t)) != BCME_OK) {
		FWPKG_ERR(("fwpkg_parse: can't get to the pkg header offset\n"));
		goto done;
	}
	/* read the last sizeof(fwpkg_hdr_t) bytes of the file to a buffer */
	if (fwpkg_read((char *)&hdr, sizeof(fwpkg_hdr_t), file) < 0) {
		FWPKG_ERR(("fwpkg_parse: can't read from the pkg header offset\n"));
		goto done;
	}
	fwpkg_close(file);
	file = NULL;

	/* if combined firmware package validates it's common header
	 * otherwise return BCME_UNSUPPORTED as it may be
	 * another type of firmware binary
	 */
	ret = fwpkg_hdr_validation(&hdr, file_size);
	if (ret == BCME_ERROR) {
		FWPKG_ERR(("fwpkg_parse: can't parse pkg header\n"));
		goto done;
	}
	/* parse rTLVs only in case of combined firmware package */
	if (ret == BCME_OK) {
		fwpkg->status = FWPKG_COMBND_FLG;
		if (fwpkg_parse_rtlvs(fwpkg, file_size, fname) == FALSE) {
			FWPKG_ERR(("fwpkg_parse: can't parse rtlvs\n"));
			ret = BCME_ERROR;
			goto done;
		}
	} else {
		fwpkg->status = FWPKG_SINGLE_FLG;
	}

	fwpkg->file_size = file_size;
done:
	if (file) {
		fwpkg_close(file);
	}

	return ret;
}

/*
 * opens the package file and seek to requested unit.
 * in case of single binary file just open the file.
 */
static int
fwpkg_open_unit(fwpkg_info_t *fwpkg, char *fname, uint32 unit_type, FWPKG_FILE **fp)
{
	int ret = BCME_OK;
	fwpkg_unit_t *fw_unit = NULL;

	*fp = fwpkg_open(fname);
	if (*fp == NULL) {
		FWPKG_ERR(("fwpkg_open_unit: open file %s fails\n", fname));
		ret = BCME_ERROR;
		goto done;
	}

	/* successfully opened file while status is FWPKG_SINGLE_FLG
	 * means, this is single binary file format.
	 */
	if (IS_FWPKG_SINGLE(fwpkg)) {
		fwpkg->file_size = fwpkg_getsize(*fp);
		ret = BCME_UNSUPPORTED;
		goto done;
	}

	fw_unit = &fwpkg->units[FWPKG_UNIT_IDX(unit_type)];

	/* seek to the last sizeof(fwpkg_hdr_t) bytes in the file */
	if (fwpkg_seek(*fp, fw_unit->offset) != BCME_OK) {
		FWPKG_ERR(("fwpkg_open_unit: can't get to the pkg header offset\n"));
		ret = BCME_ERROR;
		goto done;
	}

done:
	return ret;
}

static uint32
fwpkg_get_unit_size(fwpkg_info_t *fwpkg, uint32 unit_type)
{
	fwpkg_unit_t *fw_unit = NULL;
	uint32 size;

	if (IS_FWPKG_SINGLE(fwpkg)) {
		size = fwpkg->file_size;
		goto done;
	}

	fw_unit = &fwpkg->units[FWPKG_UNIT_IDX(unit_type)];
	size = fw_unit->size;

done:
	return size;
}
