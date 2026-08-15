// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 * Author: Sandy Huang <hjc@rock-chips.com>
 *
 * Debugfs interface of the rockchip region CRC framework
 * (crc_region/crc_key/crc_freeze).  The parsing and printing lives here; all
 * hardware register access, locking and the CRTC core structures stay in the
 * CRTC driver; this unit only accesses them through the
 * rockchip_crtc_crc_funcs callbacks registered by the CRTC driver.
 *
 * The write handlers hold the CRTC's modeset lock across the callbacks (like
 * drm_debugfs_crc.c does around set_crc_source), so the CRTC driver may
 * safely inspect crtc->state while reprogramming the CRC block.
 */
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <drm/drm_crtc.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_crc.h"

/*
 * Find @keyword in @buf with word-boundary check: the character immediately
 * before the match must be a separator (space, tab, comma, newline) or the
 * start of the buffer.  Prevents false matches like "key=" inside "monkey=".
 */
static const char *crc_find_keyword(const char *buf, const char *keyword)
{
	const char *p = buf;
	size_t len = strlen(keyword);

	while ((p = strstr(p, keyword))) {
		if (p == buf || p[-1] == ' ' || p[-1] == '\t' ||
		    p[-1] == ',' || p[-1] == '\n')
			return p;
		p += len;
	}
	return NULL;
}

/*
 * Parse the value token starting at @p, which ends at the next separator
 * (space, tab, comma, newline or end of buffer).  Accepts 0x-prefixed
 * values, like %i did.  Returns 0 on success.
 */
static int crc_parse_value(const char *p, u32 *val)
{
	char tmp[16];
	int len = 0;

	while (p[len] && p[len] != ' ' && p[len] != '\t' &&
	       p[len] != ',' && p[len] != '\n') {
		if (len >= sizeof(tmp) - 1)
			return -EINVAL;
		tmp[len] = p[len];
		len++;
	}
	tmp[len] = '\0';

	return kstrtou32(tmp, 0, val);
}

/*
 * Fetch the region CRC callbacks registered by the CRTC driver;
 * NULL when the CRTC has no region CRC support.
 */
static const struct rockchip_crtc_crc_funcs *crtc_crc_funcs(struct drm_crtc *crtc)
{
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	int pipe = drm_crtc_index(crtc);

	if (!priv->crtc_funcs[pipe])
		return NULL;

	return priv->crtc_funcs[pipe]->crc_funcs;
}

/*
 * Snapshot crtc->crc.source under crc->lock and ask the CRTC driver to
 * latch the resolved target block, so the get/set callbacks below address
 * the per-source arrays selected through crc/control.
 *
 * verify_crc_source must stay free of side effects: crc_control_show
 * ("cat crc/control") calls it on every table entry, so latching the
 * target there would leave it pointing at the last entry and silently
 * misroute region/key config.  The target is latched here instead —
 * from the authoritative crc->source, snapshotted under crc->lock so a
 * concurrent control write cannot free the string mid-copy.
 *
 * crc->source is NULL before the first crc/control write; resolve_target
 * receives NULL then, selecting the CRTC block (same as "auto"/"crtc").
 */
static void rockchip_crc_resolve(struct drm_crtc *crtc,
				 const struct rockchip_crtc_crc_funcs *crc_funcs)
{
	struct drm_crtc_crc *crc = &crtc->crc;
	char source[32];
	bool has_source = false;

	spin_lock_irq(&crc->lock);
	if (crc->source) {
		strscpy(source, crc->source, sizeof(source));
		has_source = true;
	}
	spin_unlock_irq(&crc->lock);

	crc_funcs->resolve_target(crtc, has_source ? source : NULL);
}

/*
 * Match the whole word @word at @p: the match must end at a separator
 * (space, tab, comma, newline) or the end of the buffer, so "alphaxyz"
 * does not match "alpha" and "offset" does not match "off".
 */
static bool match_word(const char *p, const char *word)
{
	size_t len = strlen(word);

	if (strncmp(p, word, len))
		return false;

	return p[len] == '\0' || p[len] == ' ' || p[len] == '\t' ||
	       p[len] == ',' || p[len] == '\n';
}

/*
 * Region CRC configuration: crc_region/crc_key/crc_freeze under the
 * CRTC's debugfs directory.  The parsing and printing lives here; all
 * hardware register access, locking and reprogramming is done by the
 * CRTC driver through the rockchip_crtc_crc_funcs callbacks.
 */
static int rockchip_drm_debugfs_crc_region_show(struct seq_file *s, void *data)
{
	struct drm_crtc *crtc = s->private;
	const struct rockchip_crtc_crc_funcs *crc_funcs = crtc_crc_funcs(crtc);
	struct rockchip_crc_region snap[ROCKCHIP_CRC_REGION_NR];
	u32 mismatch_cnt[ROCKCHIP_CRC_REGION_NR];
	u32 freeze_cnt[ROCKCHIP_CRC_REGION_NR];
	int nr, i;

	if (!crc_funcs) {
		seq_puts(s, "Does not support region CRC\n");
		return 0;
	}

	nr = crc_funcs->get_region_count(crtc);
	if (!nr || nr > ROCKCHIP_CRC_REGION_NR)
		return 0;

	rockchip_crc_resolve(crtc, crc_funcs);
	crc_funcs->get_region(crtc, snap, mismatch_cnt, freeze_cnt);

	if (!crc_funcs->source_active(crtc))
		seq_puts(s, "(source not active; config will apply on crc/data open)\n");

	for (i = 0; i < nr; i++) {
		struct rockchip_crc_region *r = &snap[i];

		if (r->enable) {
			seq_printf(s, "region%d: enable %d %d %d %d",
				   i, r->x1, r->y1, r->x2, r->y2);
			if (r->check_en)
				seq_printf(s, " exp=%#010x", r->expected_crc);
			if (r->key_mode != ROCKCHIP_CRC_KEY_OFF)
				seq_printf(s, " key=%s",
					   r->key_mode == ROCKCHIP_CRC_KEY_COLOR ?
					   "color" : "alpha");
			if (r->freeze_en)
				seq_puts(s, " freeze=on");
			seq_printf(s, " mismatch_cnt=%u freeze_cnt=%u\n",
				   mismatch_cnt[i], freeze_cnt[i]);
		} else {
			seq_printf(s, "region%d: disable mismatch_cnt=%u freeze_cnt=%u\n",
				   i, mismatch_cnt[i], freeze_cnt[i]);
		}
	}

	return 0;
}

static ssize_t rockchip_drm_debugfs_crc_region_write(struct file *file,
						     const char __user *ubuf,
						     size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct drm_crtc *crtc = s->private;
	const struct rockchip_crtc_crc_funcs *crc_funcs = crtc_crc_funcs(crtc);
	struct rockchip_crc_region region;
	char buf[96];
	const char *p;
	u32 val;
	u32 x1, y1, x2, y2;
	int idx, num, ret;
	int consumed, tail;

	if (!crc_funcs)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	/*
	 * "<idx>"              -> disable region
	 * "<idx> x1 y1 x2 y2"  -> enable region (CRC only)
	 * "<idx> x1 y1 x2 y2 [exp=VAL] [key=alpha|color|off] [freeze=0|1]"
	 *      exp= enables mismatch check; key=alpha/color enables this region's
	 *      key function and selects the key mode, key=off disables it,
	 *      freeze=1 enables freeze detection. All optional; anything
	 *      else after the coordinates is rejected.
	 */
	memset(&region, 0, sizeof(region));
	/*
	 * Coordinates are parsed as u32 and checked against U16_MAX below:
	 * %hu would silently truncate out-of-range input (70000 -> 4464)
	 * and could then pass the x2 > x1 ordering check with wrong values.
	 */
	num = sscanf(buf, "%d %n %u %u %u %u %n", &idx, &consumed,
		     &x1, &y1, &x2, &y2, &tail);
	if (num < 1 || idx < 0 || idx >= crc_funcs->get_region_count(crtc))
		return -EINVAL;

	/*
	 * num == 1 with leftover input means a coordinate failed to parse
	 * (the kernel %u rejects sign characters, so e.g. "0 -1 -1 1919
	 * 1079" stops after the index): that is not a plain "disable"
	 * write, reject it instead of silently disabling the region.
	 * %n records the position right after the index and the following
	 * white space; it is always set by then, because %d parsed
	 * (num >= 1).
	 */
	if (num == 1 && buf[consumed])
		return -EINVAL;

	/* coordinates are u16 and must form a non-empty rectangle */
	if (num != 1 && (num != 5 || x2 <= x1 || y2 <= y1 ||
			 x1 > U16_MAX || y1 > U16_MAX ||
			 x2 > U16_MAX || y2 > U16_MAX))
		return -EINVAL;

	if (num == 5) {
		region.x1 = x1;
		region.y1 = y1;
		region.x2 = x2;
		region.y2 = y2;

		region.enable = true;
		/*
		 * Optional keywords: a keyword that is present but whose
		 * value fails to parse is rejected instead of being
		 * silently ignored.  A keyword appearing more than once is
		 * rejected too, consistent with crc_key/crc_freeze.
		 */
		p = crc_find_keyword(buf, "exp=");
		if (p) {
			if (crc_find_keyword(p + 4, "exp="))
				return -EINVAL;
			if (crc_parse_value(p + 4, &val))
				return -EINVAL;
			region.expected_crc = val;
			region.check_en = true;
		}
		p = crc_find_keyword(buf, "key=");
		if (p) {
			if (crc_find_keyword(p + 4, "key="))
				return -EINVAL;
			p += 4;
			if (match_word(p, "alpha"))
				region.key_mode = ROCKCHIP_CRC_KEY_ALPHA;
			else if (match_word(p, "color"))
				region.key_mode = ROCKCHIP_CRC_KEY_COLOR;
			else if (match_word(p, "off"))
				region.key_mode = ROCKCHIP_CRC_KEY_OFF;
			else
				return -EINVAL;
		}
		p = crc_find_keyword(buf, "freeze=");
		if (p) {
			if (crc_find_keyword(p + 7, "freeze="))
				return -EINVAL;
			/* keep in sync with global=: strictly 0|1 */
			if (crc_parse_value(p + 7, &val))
				return -EINVAL;
			if (val > 1)
				return -EINVAL;
			region.freeze_en = !!val;
		}

		/*
		 * Everything past the coordinates must be one of the
		 * keyword=value tokens validated above; reject unknown
		 * trailing content instead of silently ignoring it (tail
		 * is set by the trailing %n, reached only when all four
		 * coordinates parsed, i.e. exactly the num == 5 case).
		 */
		p = buf + tail;
		for (;;) {
			while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\n')
				p++;
			if (!*p)
				break;
			if (!strncmp(p, "exp=", 4) || !strncmp(p, "key=", 4) ||
			    !strncmp(p, "freeze=", 7)) {
				while (*p && *p != ' ' && *p != '\t' &&
				       *p != ',' && *p != '\n')
					p++;
				continue;
			}
			return -EINVAL;
		}
	}

	/*
	 * set_region reprograms the CRC block and reads crtc->state->active
	 * for the power-state check; the mode range check below reads
	 * crtc->mode.  Both are serialized against atomic commits here:
	 * crtc->mode is the legacy mirror written by the async commit
	 * worker (drm_atomic_helper_update_legacy_modeset_state), so
	 * reading it under crtc->mutex keeps the validation consistent
	 * with the config actually applied, matching the discipline
	 * drm_debugfs_crc.c uses around set_crc_source.
	 */
	ret = drm_modeset_lock_single_interruptible(&crtc->mutex);
	if (ret)
		return ret;

	rockchip_crc_resolve(crtc, crc_funcs);

	if (num == 5) {
		const struct drm_display_mode *mode = &crtc->mode;

		/*
		 * Reject rectangles extending past the active mode (x2/y2
		 * are inclusive): the hardware would silently ignore such
		 * a region.  Skipped when no mode is set (hdisplay/vdisplay
		 * == 0) so the config can be prepared before the CRTC is
		 * enabled.
		 */
		if ((mode->hdisplay && region.x2 >= mode->hdisplay) ||
		    (mode->vdisplay && region.y2 >= mode->vdisplay)) {
			drm_modeset_unlock(&crtc->mutex);
			return -EINVAL;
		}
	}

	ret = crc_funcs->set_region(crtc, idx, &region);

	drm_modeset_unlock(&crtc->mutex);
	if (ret)
		return ret;

	return count;
}

static int rockchip_drm_debugfs_crc_key_show(struct seq_file *s, void *data)
{
	struct drm_crtc *crtc = s->private;
	const struct rockchip_crtc_crc_funcs *crc_funcs = crtc_crc_funcs(crtc);
	u32 key_values[ROCKCHIP_CRC_KEY_VALUE_NR] = {};
	u32 key_tie = 0;
	int i;

	if (!crc_funcs) {
		seq_puts(s, "Does not support region CRC\n");
		return 0;
	}

	rockchip_crc_resolve(crtc, crc_funcs);
	crc_funcs->get_key(crtc, key_values, &key_tie);

	for (i = 0; i < ROCKCHIP_CRC_KEY_VALUE_NR; i++)
		seq_printf(s, "%s%#010x", i ? "," : "", key_values[i]);
	seq_printf(s, " key-tie=%#010x\n", key_tie);

	return 0;
}

static ssize_t rockchip_drm_debugfs_crc_key_write(struct file *file,
						  const char __user *ubuf,
						  size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct drm_crtc *crtc = s->private;
	const struct rockchip_crtc_crc_funcs *crc_funcs = crtc_crc_funcs(crtc);
	char buf[96];
	char *p = buf;
	u32 vals[ROCKCHIP_CRC_KEY_VALUE_NR];
	u32 tie_val = 0;
	int n = 0;
	int ret;
	bool tie_set = false;

	if (!crc_funcs)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	/*
	 * KEY_VALUE pool + KEY_TIE. Comma or whitespace separated:
	 *   - plain values are positional KEY_VALUE0,1,2,3 in order (0x allowed),
	 *     trailing unspecified slots are cleared;
	 *   - "key-tie=VAL" sets KEY_TIE (0x allowed), may appear anywhere, at most once.
	 * At least one plain value or a key-tie= is required.
	 *
	 * Parse everything into local variables first; nothing is committed
	 * until the whole input has been validated.
	 */
	while (*p) {
		u32 v;

		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')
			p++;
		if (!*p)
			break;
		if (!strncmp(p, "key-tie=", 8)) {
			/* documented as "at most once": reject a repeat */
			if (tie_set)
				return -EINVAL;
			if (crc_parse_value(p + 8, &v))
				return -EINVAL;
			tie_val = v;
			tie_set = true;
			p += 8;
			while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n')
				p++;
			continue;
		}
		if (n >= ROCKCHIP_CRC_KEY_VALUE_NR)
			return -EINVAL;
		if (crc_parse_value(p, &v))
			return -EINVAL;
		vals[n++] = v;
		while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n')
			p++;
	}

	if (n == 0 && !tie_set)
		return -EINVAL;

	/* serialize against atomic commits, like crc_region above */
	ret = drm_modeset_lock_single_interruptible(&crtc->mutex);
	if (ret)
		return ret;

	rockchip_crc_resolve(crtc, crc_funcs);
	crc_funcs->set_key(crtc, vals, n, tie_val, tie_set);

	drm_modeset_unlock(&crtc->mutex);

	return count;
}

static int rockchip_drm_debugfs_crc_freeze_show(struct seq_file *s, void *data)
{
	struct drm_crtc *crtc = s->private;
	const struct rockchip_crtc_crc_funcs *crc_funcs = crtc_crc_funcs(crtc);
	bool freeze_en[ROCKCHIP_CRC_REGION_NR];
	u32 freeze_cnt[ROCKCHIP_CRC_REGION_NR];
	u32 freeze_num = 0, global_freeze_cnt = 0;
	bool global_freeze_en = false;
	int nr, i;

	if (!crc_funcs) {
		seq_puts(s, "Does not support region CRC\n");
		return 0;
	}

	nr = crc_funcs->get_region_count(crtc);
	if (!nr || nr > ROCKCHIP_CRC_REGION_NR)
		return 0;

	rockchip_crc_resolve(crtc, crc_funcs);
	crc_funcs->get_freeze(crtc, freeze_en, freeze_cnt, &freeze_num,
			      &global_freeze_en, &global_freeze_cnt);

	seq_printf(s, "freeze_num: %u\nglobal_freeze_en: %d global_freeze_cnt: %u\n",
		   freeze_num, global_freeze_en, global_freeze_cnt);
	for (i = 0; i < nr; i++)
		seq_printf(s, "region%d: freeze_en=%d freeze_cnt=%u\n", i,
			   freeze_en[i], freeze_cnt[i]);

	return 0;
}

static ssize_t rockchip_drm_debugfs_crc_freeze_write(struct file *file,
						     const char __user *ubuf,
						     size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct drm_crtc *crtc = s->private;
	const struct rockchip_crtc_crc_funcs *crc_funcs = crtc_crc_funcs(crtc);
	char buf[48];
	const char *p = buf;
	u32 num = 0, val;
	int ret;
	int global_en = 0;
	bool global_set = false, num_set = false;

	if (!crc_funcs)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	/*
	 * "<num> [global=0|1]".  A zero threshold would fire the freeze
	 * interrupt on every frame, so require 1..0xffff.  Nothing is
	 * committed until the whole input has been validated; unknown
	 * tokens are rejected instead of silently ignored.
	 */
	while (*p) {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')
			p++;
		if (!*p)
			break;
		if (!strncmp(p, "global=", 7)) {
			if (global_set)
				return -EINVAL;
			if (crc_parse_value(p + 7, &val))
				return -EINVAL;
			if (val > 1)
				return -EINVAL;
			global_en = val;
			global_set = true;
			p += 7;
			while (*p && *p != ' ' && *p != '\t' &&
			       *p != ',' && *p != '\n')
				p++;
			continue;
		}
		if (num_set)
			return -EINVAL;
		if (crc_parse_value(p, &num))
			return -EINVAL;
		if (!num || num > 0xffff)
			return -EINVAL;
		num_set = true;
		while (*p && *p != ' ' && *p != '\t' &&
		       *p != ',' && *p != '\n')
			p++;
	}

	if (!num_set)
		return -EINVAL;

	/* serialize against atomic commits, like crc_region above */
	ret = drm_modeset_lock_single_interruptible(&crtc->mutex);
	if (ret)
		return ret;

	rockchip_crc_resolve(crtc, crc_funcs);
	crc_funcs->set_freeze(crtc, num, global_set ? global_en : -1);

	drm_modeset_unlock(&crtc->mutex);

	return count;
}

static int rockchip_drm_debugfs_crc_open(struct inode *inode, struct file *file,
					 int (*show)(struct seq_file *s, void *data))
{
	return single_open(file, show, inode->i_private);
}

static int rockchip_drm_debugfs_crc_region_open(struct inode *inode, struct file *file)
{
	return rockchip_drm_debugfs_crc_open(inode, file,
			rockchip_drm_debugfs_crc_region_show);
}

static int rockchip_drm_debugfs_crc_key_open(struct inode *inode, struct file *file)
{
	return rockchip_drm_debugfs_crc_open(inode, file,
			rockchip_drm_debugfs_crc_key_show);
}

static int rockchip_drm_debugfs_crc_freeze_open(struct inode *inode, struct file *file)
{
	return rockchip_drm_debugfs_crc_open(inode, file,
			rockchip_drm_debugfs_crc_freeze_show);
}

static const struct file_operations rockchip_drm_debugfs_crc_region_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_crc_region_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_debugfs_crc_region_write,
};

static const struct file_operations rockchip_drm_debugfs_crc_key_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_crc_key_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_debugfs_crc_key_write,
};

static const struct file_operations rockchip_drm_debugfs_crc_freeze_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_drm_debugfs_crc_freeze_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rockchip_drm_debugfs_crc_freeze_write,
};

/*
 * debugfs file creation failure is never fatal for the driver (the VOP
 * keeps working without the CRC knobs and debugfs reports the problem
 * itself), so there is nothing to return; the files are owned by the
 * CRTC's debugfs directory and removed with it.
 */
void rockchip_drm_crc_debugfs_init(struct drm_crtc *crtc, struct dentry *root)
{
	debugfs_create_file("crc_region", 0644, root, crtc,
			    &rockchip_drm_debugfs_crc_region_fops);
	debugfs_create_file("crc_key", 0644, root, crtc,
			    &rockchip_drm_debugfs_crc_key_fops);
	debugfs_create_file("crc_freeze", 0644, root, crtc,
			    &rockchip_drm_debugfs_crc_freeze_fops);
}
