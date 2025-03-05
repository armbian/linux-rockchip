/* Himax Android Driver Sample Code for common functions
 *
 * Copyright (C) 2021 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*#include "himax_common.h"*/
/*#include "himax_ic_core.h"*/
#include "himax.h"
#include "himax_inspection.h"

#define SUPPORT_FINGER_DATA_CHECKSUM 0x0F
#define TS_WAKE_LOCK_TIMEOUT (5000)
#define FRAME_COUNT 5

#if defined(__EMBEDDED_FW__)
struct firmware g_embedded_fw = {
	.data = _binary___Himax_firmware_bin_start,
};
#endif

#define HIMAX_PROC_TOUCH_FOLDER "android_touch"
#define HIMAX_PROC_SELF_TEST_FILE "self_test"
#define HIMAX_PROC_WP_BP_LOCK_FILE "WPBPlock_node"
#define HIMAX_PROC_FAIL_DET_FILE "Faildet"

#if defined(HX_TP_INSPECT_MODE)
#define HIMAX_PROC_INSPECT_MODE_FILE "Inspect_mode"
#endif

static int himax_chip_num;

static ssize_t himax_self_test(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	size_t ret = 0;

	I("%s: enter, %d\n", __func__, __LINE__);

	if (ts->suspended == 1) {
		E("%s: please do self test in normal active mode\n", __func__);
		return HX_INIT_FAIL;
	}
	himax_int_enable(ts, 0); /* disable irq */

	ts->in_self_test = 1;
	ts->core_fp.fp_chip_self_test(ts, s, v);
	ts->in_self_test = 0;

	himax_int_enable(ts, 1);

	return ret;
}

static void *himax_self_test_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;

	return (void *)((unsigned long)*pos + 1);
}

static void *himax_self_test_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_self_test_seq_stop(struct seq_file *s, void *v)
{
}

static int himax_self_test_seq_read(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	size_t ret = 0;

	if (ts->chip_test_r_flag) {
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
		if (ts->rslt_data)
			seq_printf(s, "%s", ts->rslt_data);
		else
#endif
			seq_puts(s, "No chip test data.\n");
	} else {
		himax_self_test(s, v);
	}

	return ret;
}

static const struct seq_operations himax_self_test_seq_ops = {
	.start = himax_self_test_seq_start,
	.next = himax_self_test_seq_next,
	.stop = himax_self_test_seq_stop,
	.show = himax_self_test_seq_read,
};

static int himax_self_test_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_self_test_seq_ops);
};

static ssize_t himax_self_test_write(struct file *filp, const char __user *buff, size_t len,
				     loff_t *data)
{
	struct himax_ts_data *ts = pde_data(file_inode(filp));
	char buf[80] = { 0 };

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == 'r') {
		ts->chip_test_r_flag = true;
		I("%s: Start to read chip test data.\n", __func__);
	} else {
		ts->chip_test_r_flag = false;
		I("%s: Back to do self test.\n", __func__);
	}

	return len;
}

static void *himax_WPBPlock_node_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;

	return (void *)((unsigned long)*pos + 1);
}

static void *himax_WPBPlock_node_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_WPBPlock_node_seq_stop(struct seq_file *s, void *v)
{
}

static int himax_WPBPlock_node_seq_read(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	size_t ret = 0;
	uint8_t status;

	status = himax_mcu_WP_BP_status(ts);

	if (status == 0x9C) {
		seq_printf(s, "WP BP lock status is lock with value %x.\n", status);
	} else {
		seq_printf(s, "WP BP lock status is unlock with value %x.\n", status);
	}

	return ret;
}

static const struct seq_operations himax_WPBPlock_node_seq_ops = {
	.start = himax_WPBPlock_node_seq_start,
	.next = himax_WPBPlock_node_seq_next,
	.stop = himax_WPBPlock_node_seq_stop,
	.show = himax_WPBPlock_node_seq_read,
};

static int himax_WPBPlock_node_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_WPBPlock_node_seq_ops);
};

static ssize_t himax_WPBPlock_node_write(struct file *filp, const char __user *buff, size_t len,
					 loff_t *data)
{
	struct himax_ts_data *ts = pde_data(file_inode(filp));
	char buf[80] = { 0 };
	int result = 0;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if ((buf[0] == 'd') || (buf[0] == 'D')) {
		I("%s: Start to disable BP lock.\n", __func__);
		result = himax_mcu_WP_BP_disable(ts);
		I("%s: himax_mcu_WP_BP_disable return :%d.\n", __func__, result);

	} else if ((buf[0] == 'e') || (buf[0] == 'E')) {
		I("%s: Start to enable BP lock.\n", __func__);
		result = himax_mcu_WP_BP_enable(ts);
		I("%s: himax_mcu_WP_BP_enable return :%d.\n", __func__, result);
	} else {
		I("%s: Input cmd is incorrect!\n", __func__);
	}

	return len;
}

static void *himax_fail_det_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;

	return (void *)((unsigned long)*pos + 1);
}

static void *himax_fail_det_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_fail_det_seq_stop(struct seq_file *s, void *v)
{
}

static int himax_fail_det_seq_read(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	size_t ret = 0;
	uint8_t data[24] = { 0 };

	ts->core_fp.fp_dd_clk_set(ts, true);
	ts->core_fp.fp_dd_reg_en(ts, true);

	ts->core_fp.fp_dd_reg_read(ts, 0xE5, 0, 8, data, 0);
	I("%s E5_Bank0: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
	  __func__, data[0], data[1], data[2], data[3]);
	I("%s E5_Bank0: para[4]=0x%2.2X,para[5]=0x%2.2X,para[6]=0x%2.2X, para[7]=0x%2.2X\n",
	  __func__, data[4], data[5], data[6], data[7]);
	seq_printf(s,
		   "E5_Bank0: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
		   data[0], data[1], data[2], data[3]);
	seq_printf(s,
		   "E5_Bank0: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
		   data[4], data[5], data[6], data[7]);

	ts->core_fp.fp_dd_reg_read(ts, 0xE5, 0, 8, data, 1);
	I("%s E5_Bank1: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
	  __func__, data[0], data[1], data[2], data[3]);
	I("%s E5_Bank1: para[4]=0x%2.2X,para[5]=0x%2.2X,para[6]=0x%2.2X, para[7]=0x%2.2X\n",
	  __func__, data[4], data[5], data[6], data[7]);

	seq_printf(s,
		   "E5_Bank1: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
		   data[0], data[1], data[2], data[3]);
	seq_printf(s,
		   "E5_Bank1: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
		   data[4], data[5], data[6], data[7]);

	ts->core_fp.fp_dd_clk_set(ts, false);

	return ret;
}

static const struct seq_operations himax_fail_det_seq_ops = {
	.start = himax_fail_det_seq_start,
	.next = himax_fail_det_seq_next,
	.stop = himax_fail_det_seq_stop,
	.show = himax_fail_det_seq_read,
};

static int himax_fail_det_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_fail_det_seq_ops);
};

static ssize_t himax_fail_det_write(struct file *filp, const char __user *buff, size_t len,
				    loff_t *data)
{
	/*not implement write function*/
	return len;
}

static const struct proc_ops himax_proc_self_test_ops = {
	.proc_open = himax_self_test_proc_open,
	.proc_read = seq_read,
	.proc_write = himax_self_test_write,
	.proc_release = seq_release,
};

static const struct proc_ops himax_proc_WPBPlock_node_ops = {
	.proc_open = himax_WPBPlock_node_proc_open,
	.proc_read = seq_read,
	.proc_write = himax_WPBPlock_node_write,
	.proc_release = seq_release,
};

static const struct proc_ops himax_proc_fail_det_ops = {
	.proc_open = himax_fail_det_proc_open,
	.proc_read = seq_read,
	.proc_write = himax_fail_det_write,
	.proc_release = seq_release,
};

#if defined(HX_TP_INSPECT_MODE)
static void himax_bank_search_set(struct himax_ts_data *ts, uint16_t Nframe, uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	/*skip frame 0x100070F4*/
	himax_parse_assign_cmd(addr_skip_frame, tmp_addr, sizeof(tmp_addr));
	himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, false);

	switch (checktype) {
	case HX_ACT_IDLE_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
	case HX_ACT_IDLE_NOISE:
		tmp_data[0] = BS_ACT_IDLE;
		break;
	case HX_LP_RAWDATA:
	case HX_LP_BPN_RAWDATA:
	case HX_LP_ABS_NOISE:
	case HX_LP_WT_NOISE:
		tmp_data[0] = BS_LPWUG;
		break;
	case HX_LP_IDLE_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
	case HX_LP_IDLE_NOISE:
		tmp_data[0] = BS_LP_dile;
		break;
	case HX_RAWDATA:
	case HX_BPN_RAWDATA:
	case HX_SC:
		tmp_data[0] = BS_RAWDATA;
		break;
	case HX_WT_NOISE:
	case HX_ABS_NOISE:
		tmp_data[0] = BS_NOISE;
		break;
	default:
		tmp_data[0] = BS_OPENSHORT;
		break;
	}
	tmp_data[0] = 1;
	himax_mcu_register_write(ts, tmp_addr, 4, tmp_data, 0);
}
static int himax_chip_inspect_mode(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	char buf[64] = { 0 };
	uint32_t cnt = 0;
	bool is_done = false;
	int16_t Low_bound;
	int16_t low_threshold;

	/*
	 * A low threshold value 0xff00 is assign defaultly, for int16_t
	 * type, 0xff00 equal to -256(2^16- 0xff00). So set the low threshold
	 * as -256 directly.
	 */
	low_threshold = -256;

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x74;
	tmp_addr[0] = 0x54;

	switch (ts->inspect_mode_flag) {
	case 0x01: /*Start to do Short_test*/
		tmp_data[3] = 0x5A;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x01;
		tmp_data[0] = 0xA5;
		seq_printf(s, "Item : Short_test.\n");
		break;
	case 0x02: /*Start to do Open_test*/
		tmp_data[3] = 0x5A;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x02;
		tmp_data[0] = 0xA4;
		seq_printf(s, "Item : Open_test.\n");
		break;
	case 0x08: /*Start to do Noise_test*/
		tmp_data[3] = 0x5A;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x08;
		tmp_data[0] = 0x9E;
		seq_printf(s, "Item : Noise_test. \n");
		break;
	case 0x0B: /*Start to do All_test*/
		tmp_data[3] = 0x5A;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x0B;
		tmp_data[0] = 0x9B;
		seq_printf(s, "Item : All_test.\n");
		break;
	default:
		seq_printf(s, "Input cmd is incorrect.\n");
		return false;
	}

	ts->core_fp.fp_sense_off(ts, true);
	if (ts->core_fp.fp_reload_disable != NULL)
		ts->core_fp.fp_reload_disable(ts, 1);
	himax_bank_search_set(ts, 1, 1);
	ts->core_fp.fp_sense_on(ts, 0x01);
	msleep(20);

	himax_mcu_register_write(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);
	himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);
	I("Now register =0x%02X, 0x%02X, 0x%02X, 0x%02X\n", tmp_data[3], tmp_data[2], tmp_data[1],
	  tmp_data[0]);

	while (cnt++ < 8000) {
		ts->core_fp.fp_read_event_stack(ts, buf, 64);
		I("%s : %s = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", __func__, "Waiting for test",
		  buf[0], buf[1], buf[2], buf[3]);
		if ((buf[0] == 0x20) && (buf[1] == 0x19)) {
			is_done = true;
			break;
		}
		msleep(1);
	}

	if (is_done) {
		if (buf[3] == 0x01) {
			switch (ts->inspect_mode_flag) {
			case 0x01: /*Start to do Short_test*/
				seq_printf(s, "Short Test : %s \n",
					   ((buf[7] & 0x01) == 0x01) ? "Fail" : "Pass");
				Low_bound = ((buf[14] << 8) + buf[15]);
				Low_bound = (Low_bound > low_threshold) ?
					    (low_threshold - Low_bound) : Low_bound;
				seq_printf(s, "%s = %5d / %5d\n", "Short Max. / Min.",
					   ((buf[12] << 8) + buf[13]), Low_bound);
				break;
			case 0x02: /*Start to do Open_test*/
				seq_printf(s, "Open  Test : %s \n",
					   ((buf[7] & 0x02) == 0x02) ? "Fail" : "Pass");
				Low_bound = ((buf[18] << 8) + buf[19]);
				Low_bound = (Low_bound > low_threshold) ?
					    (low_threshold - Low_bound) : Low_bound;
				seq_printf(s, "%s = %5d / %5d\n", "Open  Max. / Min.",
					   ((buf[16] << 8) + buf[17]), Low_bound);
				break;
			case 0x08: /*Start to do Noise_test*/
				seq_printf(s, "Noise Test : %s \n",
					   ((buf[7] & 0x08) == 0x08) ? "Fail" : "Pass");
				Low_bound = ((buf[26] << 8) + buf[27]);
				Low_bound = (Low_bound > low_threshold) ?
					    (low_threshold - Low_bound) : Low_bound;
				seq_printf(s, "%s = %5d / %5d\n", "Noise Max. / Min.",
					   ((buf[24] << 8) + buf[25]), Low_bound);
				// seq_printf(s, "%s = %5d / %5d\n", "Noise Max. / Min.", ((buf[24] << 8) + buf[25]), ((buf[26] << 8) + buf[27]));
				break;
			case 0x0B: /*Start to do All_test*/
				seq_printf(s, "Short Test : %s \n",
					   ((buf[7] & 0x01) == 0x01) ? "Fail" : "Pass");
				seq_printf(s, "Open  Test : %s \n",
					   ((buf[7] & 0x02) == 0x02) ? "Fail" : "Pass");
				seq_printf(s, "Noise Test : %s \n",
					   ((buf[7] & 0x08) == 0x08) ? "Fail" : "Pass");
				Low_bound = ((buf[14] << 8) + buf[15]);
				Low_bound = (Low_bound > low_threshold) ?
					    (low_threshold - Low_bound) : Low_bound;
				seq_printf(s, "%s = %5d / %5d\n", "Short Max. / Min.",
					   ((buf[12] << 8) + buf[13]), Low_bound);
				Low_bound = ((buf[18] << 8) + buf[19]);
				Low_bound = (Low_bound > low_threshold) ?
					    (low_threshold - Low_bound) : Low_bound;
				seq_printf(s, "%s = %5d / %5d\n", "Open  Max. / Min.",
					   ((buf[16] << 8) + buf[17]), Low_bound);
				Low_bound = ((buf[26] << 8) + buf[27]);
				Low_bound = (Low_bound > low_threshold) ?
					    (low_threshold - Low_bound) : Low_bound;
				seq_printf(s, "%s = %5d / %5d\n", "Noise Max. / Min.",
					   ((buf[24] << 8) + buf[25]), Low_bound);
				break;
			default:
				seq_printf(s, "Input cmd is incorrect.\n");
			}
		} else if (buf[3] == 0x10)
			seq_printf(s, "%s \n", "Self Test command error.");
		else if (buf[3] == 0x20)
			seq_printf(s, "%s \n", "Self Test command CRC error.");

		I("%s = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", "Test Result", buf[4], buf[5], buf[6],
		  buf[7]);
		I("%s = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", "Result Information", buf[8], buf[9],
		  buf[10], buf[11]);

	} else
		seq_printf(
			s,
			"[ERROR] Inspect Mode has not Completed! Please check if FW support it or not! \n");

	return true;
}

static ssize_t himax_inspect_mode(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));

	I("%s: enter, %d\n", __func__, __LINE__);

	if (ts->suspended == 1) {
		E("%s: please do self test in normal active mode\n", __func__);
		return HX_INIT_FAIL;
	}
	if (ts->inspect_mode_flag == 0x0F) { /*User Call HELP!*/
		seq_printf(s, "\n");
		seq_printf(s, "@ Short Test : %s \n",
			   "#echo Short > Inspect_mode ; #cat Inspect_mode");
		seq_printf(s, "@ Open  Test : %s \n",
			   "#echo Open  > Inspect_mode ; #cat Inspect_mode");
		seq_printf(s, "@ Noise Test : %s \n",
			   "#echo Noise > Inspect_mode ; #cat Inspect_mode");
		seq_printf(s, "@ All Test   : %s \n",
			   "#echo All   > Inspect_mode ; #cat Inspect_mode");
		seq_printf(s, "\n");
		seq_printf(s, "@ Set Inspect_mode Threshold by writing register : \n");
		seq_printf(s,
			   "	Address    |  Threshold Item |    [31:16]    |    [15:0]    \n");
		seq_printf(s,
			   "	0x100074A0 |  Short Test     | High Boundary | Low Boundary \n");
		seq_printf(s,
			   "	0x100074A4 |  Open Test      | High Boundary | Low Boundary \n");
		seq_printf(s,
			   "	0x100074AC |  Noise Test     | High Boundary | Low Boundary \n");
		seq_printf(s,
			   "	0x100074B0 |  Raw data       | High Boundary | Low Boundary \n");
		seq_printf(s, "\n");
		seq_printf(s, "Example : To set High Boundary = 0xFF ; Low Boundary = 0x11\n");
		seq_printf(s, "#echo register,w:x100074A4:x00FF0011 > debug \n");
		seq_printf(s, "\n");
		return true;
	}

	himax_int_enable(ts, 0); /* disable irq */
	// ts->in_self_test = 1;
	himax_chip_inspect_mode(s, v);
	// ts->in_self_test = 0;
	himax_int_enable(ts, 1);

	return 0;
}

static void *himax_inspect_mode_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;

	return (void *)((unsigned long)*pos + 1);
}

static void *himax_inspect_mode_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_inspect_mode_seq_stop(struct seq_file *s, void *v)
{
}

static int himax_inspect_mode_seq_read(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	size_t ret = 0;

	if (ts->chip_test_r_flag) {
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
		if (ts->rslt_data)
			seq_printf(s, "%s", ts->rslt_data);
		else
#endif
			seq_puts(s, "No chip test data.\n");
	} else {
		himax_inspect_mode(s, v);
	}

	return ret;
}

static const struct seq_operations himax_inspect_mode_seq_ops = {
	.start = himax_inspect_mode_seq_start,
	.next = himax_inspect_mode_seq_next,
	.stop = himax_inspect_mode_seq_stop,
	.show = himax_inspect_mode_seq_read,
};

static int himax_inspect_mode_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_inspect_mode_seq_ops);
};

static ssize_t himax_inspect_mode_write(struct file *filp, const char __user *buff, size_t len,
					loff_t *data)
{
	struct himax_ts_data *ts = pde_data(file_inode(filp));
	char buf[80] = { 0 };

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if ((buf[0] == 's') || (buf[0] == 'S')) {
		ts->inspect_mode_flag = 0x01;
		I("%s: Start to do Short_test.\n", __func__);
	} else if ((buf[0] == 'o') || (buf[0] == 'O')) {
		ts->inspect_mode_flag = 0x02;
		I("%s: Start to do Open_test.\n", __func__);
	} else if ((buf[0] == 'n') || (buf[0] == 'N')) {
		ts->inspect_mode_flag = 0x08;
		I("%s: Start to do Noise_test.\n", __func__);
	} else if ((buf[0] == 'a') || (buf[0] == 'A')) {
		ts->inspect_mode_flag = 0x0B;
		I("%s: Start to do All_test.\n", __func__);
	} else if (buf[0] == 'h') {
		ts->inspect_mode_flag = 0x0F;
		I("%s: User Call HELP! Lest's assist user to operate.\n", __func__);
	} else {
		I("%s: Input cmd is incorrect!\n", __func__);
	}

	return len;
}

static const struct proc_ops himax_proc_inspect_mode_ops = {
	//.owner = THIS_MODULE,
	.proc_open = himax_inspect_mode_proc_open,
	.proc_read = seq_read,
	.proc_write = himax_inspect_mode_write,
	.proc_release = seq_release,
};
#endif

static int himax_common_proc_init(struct himax_ts_data *ts)
{
	const char *dir_name = ts->location ? ts->location : HIMAX_PROC_TOUCH_FOLDER;

	ts->debug.procfs.proc_dir = proc_mkdir(dir_name, NULL);

	if (ts->debug.procfs.proc_dir == NULL) {
		E(" %s: himax_touch_proc_dir file create failed!\n", __func__);
		goto fail_1;
	}
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	himax_inspection_init(ts);
#endif
	ts->debug.procfs.self_test =
		proc_create_data(HIMAX_PROC_SELF_TEST_FILE, 0444, ts->debug.procfs.proc_dir,
				 &himax_proc_self_test_ops, ts);
	if (ts->debug.procfs.self_test == NULL) {
		E(" %s: proc self_test file create failed!\n", __func__);
		goto fail_2;
	}

	ts->debug.procfs.wpbplock_node =
		proc_create_data(HIMAX_PROC_WP_BP_LOCK_FILE, 0444, ts->debug.procfs.proc_dir,
				 &himax_proc_WPBPlock_node_ops, ts);
	if (ts->debug.procfs.wpbplock_node == NULL) {
		E(" %s: proc BPlock file create failed!\n", __func__);
		goto fail_3;
	}

	ts->debug.procfs.fail_det =
		proc_create_data(HIMAX_PROC_FAIL_DET_FILE, 0444, ts->debug.procfs.proc_dir,
				 &himax_proc_fail_det_ops, ts);
	if (ts->debug.procfs.fail_det == NULL) {
		E(" %s: proc fail det file create failed!\n", __func__);
		goto fail_4;
	}

#if defined(HX_TP_INSPECT_MODE)
	ts->debug.procfs.inspect_mode =
		proc_create_data(HIMAX_PROC_INSPECT_MODE_FILE, 0644, ts->debug.procfs.proc_dir,
				 &himax_proc_inspect_mode_ops, ts);

	if (ts->debug.procfs.inspect_mode == NULL) {
		E(" %s: proc INSPECT_MODE file create failed!\n", __func__);
		goto fail_5;
	}
#endif
	return 0;
#if defined(HX_TP_INSPECT_MODE)
fail_5:
	remove_proc_entry(HIMAX_PROC_FAIL_DET_FILE, ts->debug.procfs.proc_dir);
#endif
fail_4:
	remove_proc_entry(HIMAX_PROC_WP_BP_LOCK_FILE, ts->debug.procfs.proc_dir);
fail_3:
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, ts->debug.procfs.proc_dir);
fail_2:
	if (ts->location)
		remove_proc_entry(ts->location, NULL);
	else
		remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
fail_1:
	return -ENOMEM;
}

static void himax_common_proc_deinit(struct himax_ts_data *ts)
{
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, ts->debug.procfs.proc_dir);
	remove_proc_entry(HIMAX_PROC_WP_BP_LOCK_FILE, ts->debug.procfs.proc_dir);
	remove_proc_entry(HIMAX_PROC_FAIL_DET_FILE, ts->debug.procfs.proc_dir);
#if defined(HX_TP_INSPECT_MODE)
	remove_proc_entry(HIMAX_PROC_INSPECT_MODE_FILE, ts->debug.procfs.proc_dir);
#endif
	if (ts->location)
		remove_proc_entry(ts->location, NULL);
	else
		remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
}

void himax_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len)
{
	/*I("%s: Entering!\n", __func__);*/
	switch (len) {
	case 1:
		cmd[0] = addr;
		/*I("%s: cmd[0] = 0x%02X\n", __func__, cmd[0]);*/
		break;
	case 2:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		/*I("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X\n",*/
		/*	__func__, cmd[0], cmd[1]);*/
		break;
	case 4:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		cmd[2] = (addr >> 16) % 0x100;
		cmd[3] = addr / 0x1000000;
		/*  I("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X,*/
		/*cmd[2] = 0x%02X,cmd[3] = 0x%02X\n", */
		/* __func__, cmd[0], cmd[1], cmd[2], cmd[3]);*/
		break;
	default:
		E("%s: input length fault,len = %d!\n", __func__, len);
	}
}
EXPORT_SYMBOL(himax_parse_assign_cmd);

int himax_input_register(struct himax_ts_data *ts)
{
	int ret = 0;

	ret = himax_dev_set(ts);

	if (ret < 0) {
		I("%s, input device register fail!\n", __func__);
		ret = INPUT_REGISTER_FAIL;
		goto input_device_fail;
	}

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	set_bit(KEY_POWER, ts->input_dev->keybit);
#endif
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(KEY_APPSELECT, ts->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#if defined(HX_PROTOCOL_A)
	/*ts->input_dev->mtsize = ts->nFinger_support;*/
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 3, 0, 0);
#else
	set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
#if defined(HX_PROTOCOL_B_3PA)
	input_mt_init_slots(ts->input_dev, ts->nFinger_support, INPUT_MT_DIRECT);
#else
	input_mt_init_slots(ts->input_dev, ts->nFinger_support);
#endif
#endif
	I("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n", ts->pdata->abs_x_min,
	  ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, ts->pdata->abs_x_min,
			     ts->pdata->abs_x_max, ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, ts->pdata->abs_y_min,
			     ts->pdata->abs_y_max, ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->pdata->abs_pressure_min,
			     ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
#if !defined(HX_PROTOCOL_A)
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, ts->pdata->abs_pressure_min,
			     ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, ts->pdata->abs_width_min,
			     ts->pdata->abs_width_max, ts->pdata->abs_pressure_fuzz, 0);
#endif
	/*	input_set_abs_params(ts->input_dev, ABS_MT_AMPLITUDE, 0,*/
	/*			((ts->pdata->abs_pressure_max << 16)*/
	/*			| ts->pdata->abs_width_max),*/
	/*			0, 0);*/
	/*	input_set_abs_params(ts->input_dev, ABS_MT_POSITION,*/
	/*			0, (BIT(31)*/
	/*			| (ts->pdata->abs_x_max << 16)*/
	/*			| ts->pdata->abs_y_max),*/
	/*			0, 0);*/

	if (himax_input_register_device(ts->input_dev) == 0) {
		ret = NO_ERR;
	} else {
		E("%s: input register fail\n", __func__);
		ret = INPUT_REGISTER_FAIL;
		goto input_device_fail;
	}

	I("%s, input device registered.\n", __func__);

input_device_fail:
	return ret;
}
EXPORT_SYMBOL(himax_input_register);

static void calcDataSize(struct himax_ts_data *ts_data)
{
	ts_data->x_channel = ts_data->ic_data->HX_RX_NUM;
	ts_data->y_channel = ts_data->ic_data->HX_TX_NUM;
	ts_data->nFinger_support = ts_data->ic_data->HX_MAX_PT;

	ts_data->coord_data_size = 4 * ts_data->nFinger_support;
	ts_data->area_data_size =
		((ts_data->nFinger_support / 4) + (ts_data->nFinger_support % 4 ? 1 : 0)) * 4;
	ts_data->coordInfoSize = ts_data->coord_data_size + ts_data->area_data_size + 4;
	ts_data->raw_data_frame_size =
		128 - ts_data->coord_data_size - ts_data->area_data_size - 4 - 4 - 1;

	if (ts_data->raw_data_frame_size == 0) {
		E("%s: could NOT calculate!\n", __func__);
		return;
	}

	I("%s: coord_dsz:%d,area_dsz:%d,raw_data_fsz:%d", __func__, ts_data->coord_data_size,
	  ts_data->area_data_size, ts_data->raw_data_frame_size);
}

static void calculate_point_number(struct himax_ts_data *ts)
{
	ts->hx_touch_info_point_cnt = ts->ic_data->HX_MAX_PT * 4;

	if ((ts->ic_data->HX_MAX_PT % 4) == 0)
		ts->hx_touch_info_point_cnt += (ts->ic_data->HX_MAX_PT / 4) * 4;
	else
		ts->hx_touch_info_point_cnt += ((ts->ic_data->HX_MAX_PT / 4) + 1) * 4;
}

#if defined(HX_BOOT_UPGRADE)
/*-------------------------------------------------------------------------
*
*	Create: Unknown
*
*	Using: Read FW_VER and CFG_VER value from FW file and compare with 
*		   FW/CFG version from MCU.
*
*	param: None
*
*	Dependency function: himax_mcu_fw_ver_bin
*
*/
static int himax_auto_update_check(struct himax_ts_data *ts)
{
	int32_t ret;
	bool is_testFW = false;

	if ((0x80 & ((uint8_t)(ts->ic_data->vendor_config_ver >> 8))) == 0x80)
		is_testFW = true;

	I("%s: Entering!\n", __func__);
	if (ts->core_fp.fp_fw_ver_bin(ts) == 0) {
		I("%s:ic_data->vendor_arch_ver:%d,g_i_FW_VER:%d,ic_data->vendor_config_ver:%d,g_i_CFG_VER:%d\n",
		  __func__, ts->ic_data->vendor_arch_ver, ts->fw_ver,
		  ts->ic_data->vendor_config_ver, ts->cfg_ver);
		if (((ts->ic_data->vendor_arch_ver < ts->fw_ver) ||
		     (ts->ic_data->vendor_config_ver < ts->cfg_ver)) &&
		    !is_testFW) {
			I("%s: Need update\n", __func__);
			ret = 0;
		} else {
			I("%s: Need not update!\n", __func__);
			ret = 1;
		}
	} else {
		E("%s: FW bin fail!\n", __func__);
		ret = 1;
	}

	return ret;
}

static int i_get_FW(struct himax_ts_data *ts)
{
	int ret = -1;
	int result = NO_ERR;

	I("%s: file name = %s\n", __func__, ts->pdata->fw_name);
	ret = request_firmware(&ts->hxfw, ts->pdata->fw_name, ts->dev);
	if (ret < 0) {
#if defined(__EMBEDDED_FW__)
		ts->hxfw = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)", __func__,
		  g_embedded_fw.size);
#else
		E("%s,%d: error code = %d\n", __func__, __LINE__, ret);
		return OPEN_FILE_FAIL;
#endif
	}

	return result;
}

static int i_update_FW(struct himax_ts_data *ts)
{
	int upgrade_times = 0;
	int8_t ret = 0;
	int8_t result = 0;
	himax_int_enable(ts, 0);

update_retry:

	if (ts->hxfw->size == FW_SIZE_64k)
		ret = ts->core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(
			ts, (unsigned char *)ts->hxfw->data, ts->hxfw->size, false);
	else if (ts->hxfw->size == FW_SIZE_128k)
		ret = ts->core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(
			ts, (unsigned char *)ts->hxfw->data, ts->hxfw->size, false);

	if (ret == 0) {
		upgrade_times++;
		E("%s: TP upgrade error, upgrade_times = %d\n", __func__, upgrade_times);

		if (upgrade_times < 3)
			goto update_retry;
	} else if (ret == -1) {
		E("%s: WP BP disable error \n", __func__);
	} else {
		ts->core_fp.fp_reload_disable(ts, 0);
		ts->core_fp.fp_power_on_init(ts);
		ts->core_fp.fp_read_FW_ver(ts);
		ts->core_fp.fp_touch_information(ts);
		/*upgrade success*/
		I("%s: TP upgrade OK\n", __func__);
	}

	/* need to be review with FW update flow */
	result = himax_mcu_WP_BP_enable(ts);

	if (result < 0) {
		I("%s: WP BP enable fail\n", __func__);
	}

	himax_int_enable(ts, 1);
	return result;
}
#endif
/*
 *static int himax_loadSensorConfig(struct himax_i2c_platform_data *pdata)
 *{
 *	I("%s: initialization complete\n", __func__);
 *	return NO_ERR;
 *}
 */

int himax_report_data_init(struct himax_ts_data *ts)
{
	if (ts->hx_touch_data->hx_coord_buf != NULL) {
		kfree(ts->hx_touch_data->hx_coord_buf);
		ts->hx_touch_data->hx_coord_buf = NULL;
	}
	if (ts->hx_touch_data->hx_rawdata_buf != NULL) {
		kfree(ts->hx_touch_data->hx_rawdata_buf);
		ts->hx_touch_data->hx_rawdata_buf = NULL;
	}
	ts->hx_touch_data->touch_all_size = ts->core_fp.fp_get_touch_data_size(ts);
	ts->hx_touch_data->raw_cnt_max = ts->ic_data->HX_MAX_PT / 4;
	ts->hx_touch_data->raw_cnt_rmd = ts->ic_data->HX_MAX_PT % 4;
	/* more than 4 fingers */
	if (ts->hx_touch_data->raw_cnt_rmd != 0x00) {
		ts->hx_touch_data->rawdata_size =
			ts->core_fp.fp_cal_data_len(ts, ts->hx_touch_data->raw_cnt_rmd,
						    ts->ic_data->HX_MAX_PT,
						    ts->hx_touch_data->raw_cnt_max);

		ts->hx_touch_data->touch_info_size =
			(ts->ic_data->HX_MAX_PT + ts->hx_touch_data->raw_cnt_max + 2) * 4;
	} else { /* less than 4 fingers */
		ts->hx_touch_data->rawdata_size =
			ts->core_fp.fp_cal_data_len(ts, ts->hx_touch_data->raw_cnt_rmd,
						    ts->ic_data->HX_MAX_PT,
						    ts->hx_touch_data->raw_cnt_max);

		ts->hx_touch_data->touch_info_size =
			(ts->ic_data->HX_MAX_PT + ts->hx_touch_data->raw_cnt_max + 1) * 4;
	}
	if ((ts->ic_data->HX_TX_NUM * ts->ic_data->HX_RX_NUM + ts->ic_data->HX_TX_NUM +
	     ts->ic_data->HX_RX_NUM) %
		    ts->hx_touch_data->rawdata_size ==
	    0)
		ts->hx_touch_data->rawdata_frame_size =
			(ts->ic_data->HX_TX_NUM * ts->ic_data->HX_RX_NUM + ts->ic_data->HX_TX_NUM +
			 ts->ic_data->HX_RX_NUM) /
			ts->hx_touch_data->rawdata_size;
	else
		ts->hx_touch_data->rawdata_frame_size =
			(ts->ic_data->HX_TX_NUM * ts->ic_data->HX_RX_NUM + ts->ic_data->HX_TX_NUM +
			 ts->ic_data->HX_RX_NUM) /
				ts->hx_touch_data->rawdata_size +
			1;

	I("%s:rawdata_fsz = %d,HX_MAX_PT:%d,hx_raw_cnt_max:%d\n", __func__,
	  ts->hx_touch_data->rawdata_frame_size, ts->ic_data->HX_MAX_PT,
	  ts->hx_touch_data->raw_cnt_max);
	I("%s:hx_raw_cnt_rmd:%d,g_hx_rawdata_size:%d,touch_info_size:%d\n", __func__,
	  ts->hx_touch_data->raw_cnt_rmd, ts->hx_touch_data->rawdata_size,
	  ts->hx_touch_data->touch_info_size);

	ts->hx_touch_data->hx_coord_buf =
		kzalloc(sizeof(uint8_t) * (ts->hx_touch_data->touch_info_size), GFP_KERNEL);

	if (ts->hx_touch_data->hx_coord_buf == NULL)
		goto mem_alloc_fail_coord_buf;

	ts->hx_touch_data->hx_rawdata_buf =
		kzalloc(sizeof(uint8_t) * (ts->hx_touch_data->touch_all_size -
					   ts->hx_touch_data->touch_info_size),
			GFP_KERNEL);
	if (ts->hx_touch_data->hx_rawdata_buf == NULL)
		goto mem_alloc_fail_rawdata_buf;

	if (ts->target_report_data == NULL) {
		ts->target_report_data =
			kzalloc(sizeof(struct himax_target_report_data), GFP_KERNEL);
		if (ts->target_report_data == NULL)
			goto mem_alloc_fail_report_data;

		ts->target_report_data->x =
			kzalloc(sizeof(int) * (ts->ic_data->HX_MAX_PT), GFP_KERNEL);
		if (ts->target_report_data->x == NULL)
			goto mem_alloc_fail_report_data_x;

		ts->target_report_data->y =
			kzalloc(sizeof(int) * (ts->ic_data->HX_MAX_PT), GFP_KERNEL);
		if (ts->target_report_data->y == NULL)
			goto mem_alloc_fail_report_data_y;

		ts->target_report_data->w =
			kzalloc(sizeof(int) * (ts->ic_data->HX_MAX_PT), GFP_KERNEL);
		if (ts->target_report_data->w == NULL)
			goto mem_alloc_fail_report_data_w;

		ts->target_report_data->finger_id =
			kzalloc(sizeof(int) * (ts->ic_data->HX_MAX_PT), GFP_KERNEL);
		if (ts->target_report_data->finger_id == NULL)
			goto mem_alloc_fail_report_data_fid;
	}

	return NO_ERR;

mem_alloc_fail_report_data_fid:
	kfree(ts->target_report_data->w);
	ts->target_report_data->w = NULL;
mem_alloc_fail_report_data_w:
	kfree(ts->target_report_data->y);
	ts->target_report_data->y = NULL;
mem_alloc_fail_report_data_y:
	kfree(ts->target_report_data->x);
	ts->target_report_data->x = NULL;
mem_alloc_fail_report_data_x:
	kfree(ts->target_report_data);
	ts->target_report_data = NULL;
mem_alloc_fail_report_data:
	kfree(ts->hx_touch_data->hx_rawdata_buf);
	ts->hx_touch_data->hx_rawdata_buf = NULL;
mem_alloc_fail_rawdata_buf:
	kfree(ts->hx_touch_data->hx_coord_buf);
	ts->hx_touch_data->hx_coord_buf = NULL;
mem_alloc_fail_coord_buf:

	E("%s: Failed to allocate memory\n", __func__);
	return MEM_ALLOC_FAIL;
}
EXPORT_SYMBOL(himax_report_data_init);

static void himax_report_data_deinit(struct himax_ts_data *ts)
{
	kfree(ts->target_report_data->finger_id);
	ts->target_report_data->finger_id = NULL;
	kfree(ts->target_report_data->w);
	ts->target_report_data->w = NULL;
	kfree(ts->target_report_data->y);
	ts->target_report_data->y = NULL;
	kfree(ts->target_report_data->x);
	ts->target_report_data->x = NULL;
	kfree(ts->target_report_data);
	ts->target_report_data = NULL;
	kfree(ts->hx_touch_data->hx_rawdata_buf);
	ts->hx_touch_data->hx_rawdata_buf = NULL;
	kfree(ts->hx_touch_data->hx_coord_buf);
	ts->hx_touch_data->hx_coord_buf = NULL;
}

static int himax_ts_work_status(struct himax_ts_data *ts)
{
	/* 1: normal */
	int result = HX_REPORT_COORD;

	ts->hx_touch_data->diag_cmd = ts->diag_cmd;
	if (ts->hx_touch_data->diag_cmd)
		result = HX_REPORT_COORD_RAWDATA;

	/* I("Now Status is %d\n", result); */
	return result;
}

static int himax_touch_get(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
	if (ts->ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	switch (ts_path) {
	/*normal*/
	case HX_REPORT_COORD:
		if (ts->hx_hw_reset_activate) {
			if (!ts->core_fp.fp_read_event_stack(ts, buf, 128)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
			}
		} else {
			if (!ts->core_fp.fp_read_event_stack(ts, buf,
							     ts->hx_touch_data->touch_info_size)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
			}
		}
		break;
	case HX_REPORT_COORD_RAWDATA:
		if (!ts->core_fp.fp_read_event_stack(ts, buf, 128)) {
			E("%s: can't read data from chip!\n", __func__);
			ts_status = HX_TS_GET_DATA_FAIL;
		}
		break;
	default:
		break;
	}

	return ts_status;
}

/* start error_control*/
static int himax_checksum_cal(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
	uint16_t check_sum_cal = 0;
	int32_t i = 0;
	int length = 0;
	int zero_cnt = 0;
	int raw_data_sel = 0;
	int ret_val = ts_status;

	if (ts->ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	/* Normal */
	switch (ts_path) {
	case HX_REPORT_COORD:
		length = ts->hx_touch_data->touch_info_size;
		break;
	case HX_REPORT_COORD_RAWDATA:
		length = ts->hx_touch_data->touch_info_size;
		break;
	default:
		I("%s, Normal error!\n", __func__);
		ret_val = HX_PATH_FAIL;
		goto END_FUNCTION;
	}

	for (i = 0; i < length; i++) {
		check_sum_cal += buf[i];
		if (buf[i] == 0x00)
			zero_cnt++;
	}

	if (check_sum_cal % 0x100 != 0) {
		I("point data_checksum not match check_sum_cal: 0x%02X", check_sum_cal);
		ret_val = HX_CHKSUM_FAIL;
	} else if (zero_cnt == length) {
		if (ts->use_irq)
			I("[HIMAX TP MSG] All Zero event\n");

		ret_val = HX_CHKSUM_FAIL;
	} else {
		raw_data_sel = buf[ts->hx_touch_info_point_cnt] >> 4 & 0x0F;
		/*I("%s:raw_out_sel=%x , hx_touch_data->diag_cmd=%x.\n",*/
		/*		__func__, raw_data_sel,*/
		/*		ts->hx_touch_data->diag_cmd);*/
		/*raw data out not match skip it*/
		if ((raw_data_sel != 0x0F) && (raw_data_sel != ts->hx_touch_data->diag_cmd)) {
			/*I("%s:raw data out not match.\n", __func__);*/
			if (!ts->hx_touch_data->diag_cmd) {
				/*Need to clear event stack here*/
				ts->core_fp.fp_read_event_stack(
					ts, buf, (128 - ts->hx_touch_data->touch_info_size));
				/*I("%s: size =%d, buf[0]=%x ,buf[1]=%x,*/
				/*	buf[2]=%x, buf[3]=%x.\n",*/
				/*	__func__,*/
				/*	(128-ts->hx_touch_data->touch_info_size),*/
				/*	buf[0], buf[1], buf[2], buf[3]);*/
				/*I("%s:also clear event stack.\n", __func__);*/
			}
			ret_val = HX_READY_SERVE;
		}
	}

END_FUNCTION:
	if (ts->ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);
	return ret_val;
}

static int himax_err_ctrl(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
#if defined(HX_RST_PIN_FUNC)
	if (ts->hx_hw_reset_activate) {
		/* drop 1st interrupts after chip reset */
		ts->hx_hw_reset_activate = 0;
		I("[HX_HW_RESET_ACTIVATE]%s:Back from reset,ready to serve.\n", __func__);
		ts_status = HX_RST_OK;
		goto END_FUNCTION;
	}
#endif

	ts_status = himax_checksum_cal(ts, buf, ts_path, ts_status);

END_FUNCTION:
	if (ts->ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end error_control*/

/* start distribute_data*/
static int himax_distribute_touch_data(struct himax_ts_data *ts, uint8_t *buf, int ts_path,
				       int ts_status)
{
	uint8_t hx_state_info_pos = ts->hx_touch_data->touch_info_size - 3;

	if (ts->ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD) {
		memcpy(ts->hx_touch_data->hx_coord_buf, &buf[0],
		       ts->hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(ts->hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 2);
		else
			memset(ts->hx_touch_data->hx_state_info, 0x00,
			       sizeof(ts->hx_touch_data->hx_state_info));

		if ((ts->hx_hw_reset_activate)

		) {
			memcpy(ts->hx_touch_data->hx_rawdata_buf,
			       &buf[ts->hx_touch_data->touch_info_size],
			       ts->hx_touch_data->touch_all_size -
				       ts->hx_touch_data->touch_info_size);
		}
	} else if (ts_path == HX_REPORT_COORD_RAWDATA) {
		memcpy(ts->hx_touch_data->hx_coord_buf, &buf[0],
		       ts->hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(ts->hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 2);
		else
			memset(ts->hx_touch_data->hx_state_info, 0x00,
			       sizeof(ts->hx_touch_data->hx_state_info));

		memcpy(ts->hx_touch_data->hx_rawdata_buf, &buf[ts->hx_touch_data->touch_info_size],
		       ts->hx_touch_data->touch_all_size - ts->hx_touch_data->touch_info_size);

	} else {
		E("%s, Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}

	if (ts->ts_dbg != 0)
		I("%s: End, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end assign_data*/

/* start parse_report_data*/
static int himax_parse_report_points(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	int x = 0, y = 0, w = 0;
	int base = 0;
	int event_id = 0;
	int32_t loop_i = 0;

	if (ts->ts_dbg != 0)
		I("%s: start!\n", __func__);

	ts->old_finger = ts->pre_finger_mask;
	if (ts->hx_point_num == 0) {
		if (ts->ts_dbg != 0)
			I("%s: hx_point_num = 0!\n", __func__);
		return ts_status;
	}
	ts->pre_finger_mask = 0;
	ts->hx_touch_data->finger_num =
		ts->hx_touch_data->hx_coord_buf[ts->coordInfoSize - 4] & 0x0F;
	ts->hx_touch_data->finger_on = 1;
	ts->aa_press = 1;

	ts->target_report_data->finger_num = ts->hx_touch_data->finger_num;
	ts->target_report_data->finger_on = ts->hx_touch_data->finger_on;
	ts->target_report_data->ig_count = ts->hx_touch_data->hx_coord_buf[ts->coordInfoSize - 5];

	if (ts->ts_dbg != 0)
		I("%s:finger_num = 0x%2X, finger_on = %d\n", __func__,
		  ts->target_report_data->finger_num, ts->target_report_data->finger_on);

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
		base = loop_i * 4;
		x = ts->hx_touch_data->hx_coord_buf[base] << 8 |
		    ts->hx_touch_data->hx_coord_buf[base + 1];
		y = (ts->hx_touch_data->hx_coord_buf[base + 2] << 8 |
		     ts->hx_touch_data->hx_coord_buf[base + 3]);

		w = ts->hx_touch_data->hx_coord_buf[(ts->nFinger_support * 4) + loop_i];

		if (ts->ts_dbg != 0)
			D("%s: now parsing[%d]:x=%d, y=%d, w=%d\n", __func__, loop_i, x, y, w);

		if (ts->ic_data->HX_IS_ID_EN) {
			x = (ts->hx_touch_data->hx_coord_buf[base] & 0x3F) << 8 |
			    ts->hx_touch_data->hx_coord_buf[base + 1];
			event_id = ts->hx_touch_data->hx_coord_buf[base] >> 0x06;
			/*if ((event_id == 0) || (event_id == 3)) {*/ /*No touch event or Leave event*/
			if (event_id == 3) { /*No touch event or Leave event*/
				x = 0xFFFF;
				y = 0xFFFF;
			}
			I("%s: now parsing[%d]:x=%d, y=%d, event_id=%d\n", __func__, loop_i, x, y,
			  event_id);
		}

		if (x >= 0 && x <= ts->pdata->abs_x_max && y >= 0 && y <= ts->pdata->abs_y_max) {
			ts->hx_touch_data->finger_num--;

			ts->target_report_data->x[loop_i] = x;
			ts->target_report_data->y[loop_i] = y;
			ts->target_report_data->w[loop_i] = w;
			ts->target_report_data->finger_id[loop_i] = 1;

			/*I("%s: g_target_report_data->x[loop_i]=%d,*/
			/*g_target_report_data->y[loop_i]=%d,*/
			/*g_target_report_data->w[loop_i]=%d",*/
			/*__func__, ts->target_report_data->x[loop_i],*/
			/*ts->target_report_data->y[loop_i],*/
			/*ts->target_report_data->w[loop_i]); */

			if (!ts->first_pressed) {
				ts->first_pressed = 1;
				I("S1@%d, %d\n", x, y);
			}

			ts->pre_finger_data[loop_i][0] = x;
			ts->pre_finger_data[loop_i][1] = y;

			ts->pre_finger_mask = ts->pre_finger_mask + (1 << loop_i);
		} else { /* report coordinates */
			ts->target_report_data->x[loop_i] = x;
			ts->target_report_data->y[loop_i] = y;
			ts->target_report_data->w[loop_i] = w;
			ts->target_report_data->finger_id[loop_i] = 0;

			if (loop_i == 0 && ts->first_pressed == 1) {
				ts->first_pressed = 2;
				I("E1@%d, %d\n", ts->pre_finger_data[0][0],
				  ts->pre_finger_data[0][1]);
			}
		}
	}

	if (ts->ts_dbg != 0) {
		for (loop_i = 0; loop_i < 10; loop_i++)
			D("DBG X=%d  Y=%d ID=%d\n", ts->target_report_data->x[loop_i],
			  ts->target_report_data->y[loop_i],
			  ts->target_report_data->finger_id[loop_i]);

		D("DBG finger number %d\n", ts->target_report_data->finger_num);
	}

	if (ts->ts_dbg != 0)
		I("%s: end!\n", __func__);
	return ts_status;
}

static int himax_parse_report_data(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	if (ts->ts_dbg != 0)
		I("%s: start now_status=%d!\n", __func__, ts_status);

	ts->en_noisefilter =
		(ts->hx_touch_data->hx_coord_buf[ts->hx_touch_info_point_cnt + 2] >> 3);
	/* I("EN_NoiseFilter=%d\n", ts->en_noisefilter); */
	ts->en_noisefilter = ts->en_noisefilter & 0x01;
	/* I("EN_NoiseFilter2=%d\n", ts->en_noisefilter); */
	ts->p_point_num = ts->hx_point_num;

	if (ts->hx_touch_data->hx_coord_buf[ts->hx_touch_info_point_cnt] == 0xff)
		ts->hx_point_num = 0;
	else
		ts->hx_point_num =
			ts->hx_touch_data->hx_coord_buf[ts->hx_touch_info_point_cnt] & 0x0f;

	switch (ts_path) {
	case HX_REPORT_COORD:
		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;
	case HX_REPORT_COORD_RAWDATA:
		/* touch monitor rawdata */
		if (ts->debug.ops.fp_set_diag_cmd) {
			ts->debug.ops.fp_set_diag_cmd(ts, ts->ic_data, ts->hx_touch_data);
			I("%s:raw data_checksum not match\n", __func__);
		} else {
			E("%s,There is no init set_diag_cmd\n", __func__);
		}

		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;

	default:
		E("%s:Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
		break;
	}
	if (ts->ts_dbg != 0)
		I("%s: end now_status=%d!\n", __func__, ts_status);
	return ts_status;
}

/* end parse_report_data*/

static void himax_report_all_leave_event(struct himax_ts_data *ts)
{
	int loop_i = 0;

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
#if !defined(HX_PROTOCOL_A)
		input_mt_slot(ts->input_dev, loop_i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#endif
	}
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);
}

/* start report_point*/
static void himax_finger_report(struct himax_ts_data *ts)
{
	int i = 0;
	bool valid = false;

	if (ts->ts_dbg != 0) {
		I("%s:start hx_touch_data->finger_num=%d\n", __func__,
		  ts->hx_touch_data->finger_num);
	}
	for (i = 0; i < ts->nFinger_support; i++) {
		if (ts->target_report_data->x[i] >= 0 &&
		    ts->target_report_data->x[i] <= ts->pdata->abs_x_max &&
		    ts->target_report_data->y[i] >= 0 &&
		    ts->target_report_data->y[i] <= ts->pdata->abs_y_max)
			valid = true;
		else
			valid = false;
		if (ts->ts_dbg != 0)
			I("valid=%d\n", valid);
		if (valid) {
			if (ts->ts_dbg != 0) {
				I("report_data->x[i]=%d,y[i]=%d,w[i]=%d",
				  ts->target_report_data->x[i], ts->target_report_data->y[i],
				  ts->target_report_data->w[i]);
			}
#if !defined(HX_PROTOCOL_A)
			input_mt_slot(ts->input_dev, i);
#else
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
					 ts->target_report_data->w[i]);
#if !defined(HX_PROTOCOL_A)
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
					 ts->target_report_data->w[i]);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
					 ts->target_report_data->w[i]);
#else
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
#endif
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					 ts->target_report_data->x[i]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					 ts->target_report_data->y[i]);
#if !defined(HX_PROTOCOL_A)
			ts->last_slot = i;
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
			pr_debug("[HXTP]Finger down,report_data->x[i]=%d,y[i]=%d,w[i]=%d",
				 ts->target_report_data->x[i], ts->target_report_data->y[i],
				 ts->target_report_data->w[i]);
			if (ts->touch_num >= 50) {
				I("Finger down,report_data->x[i]=%d,y[i]=%d,w[i]=%d",
				  ts->target_report_data->x[i], ts->target_report_data->y[i],
				  ts->target_report_data->w[i]);
				ts->touch_num = 0;
			}
#else
			input_mt_sync(ts->input_dev);
#endif
		} else {
#if !defined(HX_PROTOCOL_A)
			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#endif
		}
	}
#if !defined(HX_PROTOCOL_A)
	input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
	input_sync(ts->input_dev);

	if (ts->ts_dbg != 0)
		I("%s:end\n", __func__);
}

static void himax_finger_leave(struct himax_ts_data *ts)
{
#if !defined(HX_PROTOCOL_A)
	int32_t loop_i = 0;
#endif

	if (ts->ts_dbg != 0)
		I("%s: start!\n", __func__);

	ts->hx_touch_data->finger_on = 0;
	ts->target_report_data->finger_on = 0;
	ts->target_report_data->finger_num = 0;
	ts->aa_press = 0;

#if defined(HX_PROTOCOL_A)
	input_mt_sync(ts->input_dev);
#endif
#if !defined(HX_PROTOCOL_A)
	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
		input_mt_slot(ts->input_dev, loop_i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	if (ts->pre_finger_mask > 0)
		ts->pre_finger_mask = 0;

	if (ts->first_pressed == 1) {
		ts->first_pressed = 2;
		I("E1@%d, %d\n", ts->pre_finger_data[0][0], ts->pre_finger_data[0][1]);
	}

	/*if (ts->debug_log_level & BIT(1))*/
	/*	himax_log_touch_event(x, y, w, loop_i, ts->en_noisefilter,*/
	/*			HX_FINGER_LEAVE); */

	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);
	ts->touch_num++;

	if (ts->ts_dbg != 0)
		I("%s: end!\n", __func__);
}

static void himax_report_points(struct himax_ts_data *ts)
{
	if (ts->ts_dbg != 0)
		I("%s: start!\n", __func__);

	if (ts->hx_point_num != 0)
		himax_finger_report(ts);
	else
		himax_finger_leave(ts);

	ts->last_en_noisefilter = ts->en_noisefilter;

	if (ts->ts_dbg != 0)
		I("%s: end!\n", __func__);
}
/* end report_points*/

int himax_report_data(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	if (ts->ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
		/* Touch Point information */
		himax_report_points(ts);

	} else {
		E("%s:Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}

	if (ts->ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end report_data */

static int himax_ts_operation(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	uint8_t hw_reset_check[2];
#if defined(FCA_PROTOCOL_EN)
	uint8_t loop_i;
	uint8_t Finger_i = 0;
	uint16_t x_coord[5];
	uint16_t y_coord[5];
	memset(x_coord, 0xFFFF, 5 * sizeof(uint16_t));
	memset(y_coord, 0xFFFF, 5 * sizeof(uint16_t));
#endif
	memset(ts->xfer_buff, 0x00, 128 * sizeof(uint8_t));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	ts_status = himax_touch_get(ts, ts->xfer_buff, ts_path, ts_status);
	if (ts_status == HX_TS_GET_DATA_FAIL)
		goto END_FUNCTION;
#if !defined(FCA_PROTOCOL_EN)
	ts_status = himax_distribute_touch_data(ts, ts->xfer_buff, ts_path, ts_status);
	ts_status = himax_err_ctrl(ts, ts->xfer_buff, ts_path, ts_status);

	if (ts_status == HX_REPORT_DATA || ts_status == HX_TS_NORMAL_END)
		ts_status = himax_parse_report_data(ts, ts_path, ts_status);
	else
		goto END_FUNCTION;

	ts_status = himax_report_data(ts, ts_path, ts_status);

#else
	for (loop_i = 0; loop_i < 25; loop_i++) {
		if (loop_i == 0)
			I("%s: Msg_cnt = 0x%2X\n", __func__, ts->xfer_buff[0]);
		else if ((loop_i % 4) == 0 && loop_i < 24) {
			if (ts->xfer_buff[loop_i] >> 7) {
				x_coord[Finger_i] =
					(((ts->xfer_buff[loop_i] & 0x07) << 8) |
					 ts->xfer_buff[loop_i - 1]); /* (HI & 0x07) >> 0x08 | LO*/
				y_coord[Finger_i++] = (((ts->xfer_buff[loop_i + 2] & 0x07) << 8) |
						       ts->xfer_buff[loop_i + 1]);
			}
		}
	}
	if (!Finger_i) {
		himax_finger_leave(ts);
	} else {
		for (loop_i = 0; loop_i < Finger_i; loop_i++) {
			if (x_coord[loop_i] != 0xFFFF) {
				I("x_coord[%d] = %2d\n", loop_i, x_coord[loop_i]);
				I("y_coord[%d] = %2d\n", loop_i, y_coord[loop_i]);
			}
			input_mt_slot(ts->input_dev,
				      loop_i); //input_mt_slot(input, 0);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 1);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_coord[loop_i]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_coord[loop_i]);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
			input_sync(ts->input_dev);
		}
	}

	goto END_FUNCTION;
#endif

END_FUNCTION:
	return ts_status;
}

void himax_fail_det_work(struct himax_ts_data *ts)
{
	uint8_t data[8] = { 0 };
	/*	uint8_t addr[4] = {0xD4, 0x74, 0x00, 0x10};
	Clear Simulation Register
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
	I("%s Clear: addr[0]=0x%2.2X,addr[1]=0x%2.2X,addr[2]=0x%2.2X, addr[3]=0x%2.2X\n", 
	__func__,addr[0],addr[1],addr[2],addr[3]);
*/
	ts->core_fp.fp_dd_clk_set(ts, true);
	ts->core_fp.fp_dd_reg_en(ts, true);

	ts->core_fp.fp_dd_reg_read(ts, 0xE5, 0, 8, data, 0);
	I("%s E5_Bank0: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
	  __func__, data[0], data[1], data[2], data[3]);
	I("%s E5_Bank0: para[4]=0x%2.2X,para[5]=0x%2.2X,para[6]=0x%2.2X, para[7]=0x%2.2X\n",
	  __func__, data[4], data[5], data[6], data[7]);

	ts->core_fp.fp_dd_reg_read(ts, 0xE5, 0, 8, data, 1);
	I("%s E5_Bank1: para[0]=0x%2.2X,para[1]=0x%2.2X,para[2]=0x%2.2X, para[3]=0x%2.2X\n",
	  __func__, data[0], data[1], data[2], data[3]);
	I("%s E5_Bank1: para[4]=0x%2.2X,para[5]=0x%2.2X,para[6]=0x%2.2X, para[7]=0x%2.2X\n",
	  __func__, data[4], data[5], data[6], data[7]);

	ts->core_fp.fp_dd_clk_set(ts, false);

	/* It depends on customer: */

	goto AP_recovery;

AP_recovery:

	I("%s: Now FAIL_DET tie high means IC need external recovery\n", __func__);

	himax_mcu_tp_lcm_pin_reset(ts);
}

void himax_ts_work(struct himax_ts_data *ts)
{
	int ts_status = HX_TS_NORMAL_END;
	int ts_path = 0;

	if (ts->debug.ops.fp_ts_dbg_func)
		ts->debug.ops.fp_ts_dbg_func(ts, HX_FINGER_ON);

	ts_path = himax_ts_work_status(ts);
	switch (ts_path) {
	case HX_REPORT_COORD:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	case HX_REPORT_COORD_RAWDATA:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	default:
		E("%s:Path Fault! value=%d\n", __func__, ts_path);
		goto END_FUNCTION;
	}

	if (ts_status == HX_TS_GET_DATA_FAIL)
		goto GET_TOUCH_FAIL;
	else
		goto END_FUNCTION;

GET_TOUCH_FAIL:
	I("%s: Now reset the Touch chip.\n", __func__);
#if defined(HX_RST_PIN_FUNC)
	if (HX_SYSTEM_RESET == 0)
		ts->core_fp.fp_ic_reset(ts, false, true);
	else
		ts->core_fp.fp_system_reset(ts);
#endif
END_FUNCTION:
	if (ts->debug.ops.fp_ts_dbg_func)
		ts->debug.ops.fp_ts_dbg_func(ts, HX_FINGER_LEAVE);
}
/*end ts_work*/
enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer)
{
	struct himax_ts_data *ts;

	ts = container_of(timer, struct himax_ts_data, timer);
	queue_work(ts->himax_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

#if defined(HX_BOOT_UPGRADE)
static void himax_boot_upgrade(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work_boot_upgrade.work);
	if (i_get_FW(ts) != 0)
		return;

	ts->core_fp.fp_bin_desc_get(ts, (unsigned char *)ts->hxfw->data, HX1K);

	if (ts->boot_upgrade_flag == true) {
		I("%s: Forced upgrade\n", __func__);
		goto UPDATE_FW;
	}

	if (himax_auto_update_check(ts) != 0)
		goto SKIP_UPDATE_FW;

UPDATE_FW:
	if (i_update_FW(ts) <= 0)
		E("%s: Update FW fail\n", __func__);
	else
		I("%s: Update FW success\n", __func__);

SKIP_UPDATE_FW:
	release_firmware(ts->hxfw);
	ts->hxfw = NULL;
}
#endif

static int hx_chk_flash_sts(struct himax_ts_data *ts)
{
	int rslt = 0;

	I("%s: Entering\n", __func__);

	rslt = (!ts->core_fp.fp_calculateChecksum(ts, false, FW_SIZE_128k));
	/*avoid the FW is full of zero*/
	rslt |= ts->core_fp.fp_flash_lastdata_check(ts, FW_SIZE_128k);

	return rslt;
}

#if defined(HX_CONFIG_FB) || defined(HX_CONFIG_DRM)
static void himax_fb_register(struct work_struct *work)
{
	int ret = 0;

	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work_att.work);

	I("%s in\n", __func__);
#if defined(HX_CONFIG_FB)
	ts->fb_notif.notifier_call = himax_fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
#elif defined(HX_CONFIG_DRM)
	ts->fb_notif.notifier_call = himax_drm_notifier_callback;
	ret = msm_drm_register_client(&ts->fb_notif);
#endif
	if (ret)
		E("Unable to register fb_notifier: %d\n", ret);
}
#endif

#if defined(HX_CONTAINER_SPEED_UP)
static void himax_resume_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, ts_int_work.work);

	himax_chip_common_resume(ts);
}
#endif

int himax_chip_common_init(struct himax_ts_data *ts)
{
	int ret = 0;
	int err = PROBE_FAIL;
	struct himax_i2c_platform_data *pdata;
	struct himax_chip_entry *entry;

	ts->p_point_num = 0xFFFF;

#if defined(__EMBEDDED_FW__)
	g_embedded_fw.size = (size_t)_binary___Himax_firmware_bin_end -
			     (size_t)_binary___Himax_firmware_bin_start;
#endif
	ts->xfer_buff = devm_kzalloc(ts->dev, 128 * sizeof(uint8_t), GFP_KERNEL);
	if (ts->xfer_buff == NULL) {
		err = -ENOMEM;
		goto err_xfer_buff_fail;
	}

	I("PDATA START\n");
	pdata = kzalloc(sizeof(struct himax_i2c_platform_data), GFP_KERNEL);
	if (pdata == NULL) { /*Allocate Platform data space*/
		err = -ENOMEM;
		goto err_dt_platform_data_fail;
	}

	I("ic_data START\n");
	ts->ic_data = kzalloc(sizeof(struct himax_ic_data), GFP_KERNEL);
	if (ts->ic_data == NULL) { /*Allocate IC data space*/
		err = -ENOMEM;
		goto err_dt_ic_data_fail;
	}

	/* allocate report data */
	ts->hx_touch_data = kzalloc(sizeof(struct himax_report_data), GFP_KERNEL);
	if (ts->hx_touch_data == NULL) {
		err = -ENOMEM;
		goto err_alloc_touch_data_failed;
	}

	ts->pdata = pdata;

	if (himax_parse_dt(ts, pdata) < 0) {
		I(" pdata is NULL for DT\n");
		goto err_alloc_dt_pdata_failed;
	}

	ts->lcm_gpio = pdata->lcm_rst;

#if defined(HX_RST_PIN_FUNC)
	ts->rst_gpio = pdata->gpio_reset;
#endif
	himax_gpio_power_config(ts, pdata);

#if !defined(CONFIG_OF)
	if (pdata->power) {
		ret = pdata->power(1);
		if (ret < 0) {
			E("%s: power on failed\n", __func__);
			goto err_power_failed;
		}
	}
#endif

	ts->hx_chip_inited = 0;
	if (list_empty(&ts->chips)) {
		I("%s: No available chip exist!\n", __func__);
		goto error_ic_detect_failed;
	}

	list_for_each_entry (entry, &ts->chips, list) {
		++himax_chip_num;
		if (!entry->ops.detect)
			continue;

		if (entry->ops.detect(ts)) {
			I("%s: chip found! list_num=%d\n", __func__, himax_chip_num);
			goto found_hx_chip;
		}

		I("%s:num=%d,chip NOT found! go Next\n", __func__, himax_chip_num);
	}

	if (entry && list_entry_is_head(entry, &ts->chips, list)) {
		I("%s: No available chip exist!\n", __func__);
		goto error_ic_detect_failed;
	}

found_hx_chip:
	if (ts->core_fp.fp_chip_init != NULL) {
		I("%s:lsy---fp_chip_init\n", __func__);
		ts->core_fp.fp_chip_init(ts);
	} else {
		E("%s: function point of chip_init is NULL!\n", __func__);
		goto error_ic_detect_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;

	if (hx_chk_flash_sts(ts) == 1) {
		E("%s: check flash fail, please upgrade FW\n", __func__);
#if defined(HX_BOOT_UPGRADE)
		ts->boot_upgrade_flag = 1;
#endif
	} else {
		ts->core_fp.fp_reload_disable(ts, 0);
		ts->core_fp.fp_power_on_init(ts);
		ts->core_fp.fp_read_FW_ver(ts);
	}

#if defined(HX_BOOT_UPGRADE)
	ts->himax_boot_upgrade_wq = create_singlethread_workqueue("HX_boot_upgrade");
	if (!ts->himax_boot_upgrade_wq) {
		E("allocate himax_boot_upgrade_wq failed\n");
		err = -ENOMEM;
		goto err_boot_upgrade_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->work_boot_upgrade, himax_boot_upgrade);
	queue_delayed_work(ts->himax_boot_upgrade_wq, &ts->work_boot_upgrade,
			   msecs_to_jiffies(2000));
#endif

#if defined(HX_CONTAINER_SPEED_UP)
	ts->ts_int_workqueue = create_singlethread_workqueue("himax_ts_resume_wq");
	if (!ts->ts_int_workqueue) {
		E("%s: create ts_resume workqueue failed\n", __func__);
		goto err_create_ts_resume_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->ts_int_work, himax_resume_work_func);
#endif

	/*Himax Power On and Load Config*/
	/*	if (himax_loadSensorConfig(pdata)) {
	 *		E("%s: Load Sesnsor configuration failed, unload driver.\n",
	 *				__func__);
	 *		goto err_detect_failed;
	 *	}
	 */
	ts->core_fp.fp_power_on_init(ts);
	calculate_point_number(ts);

#if defined(CONFIG_OF)
	ts->power = pdata->power;
#endif

	/*calculate the i2c data size*/
	calcDataSize(ts);
	I("%s: calcDataSize complete\n", __func__);

#if defined(CONFIG_OF)
	ts->pdata->abs_pressure_min = 0;
	ts->pdata->abs_pressure_max = 200;
	ts->pdata->abs_width_min = 0;
	ts->pdata->abs_width_max = 200;
	pdata->cable_config[0] = 0xF0;
	pdata->cable_config[1] = 0x00;
#endif

	ts->suspended = false;
	ts->touch_num = 50;

#if defined(HX_PROTOCOL_A)
	ts->protocol_type = PROTOCOL_TYPE_A;
#else
	ts->protocol_type = PROTOCOL_TYPE_B;
#endif
	I("%s: Use Protocol Type %c\n", __func__, ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

	ret = himax_input_register(ts);
	if (ret) {
		E("%s: Unable to register %s input device\n", __func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	spin_lock_init(&ts->irq_lock);
	ts->initialized = true;

#if defined(HX_CONFIG_FB) || defined(HX_CONFIG_DRM)
	ts->himax_att_wq = create_singlethread_workqueue("HMX_ATT_request");

	if (!ts->himax_att_wq) {
		E(" allocate himax_att_wq failed\n");
		err = -ENOMEM;
		goto err_get_intr_bit_failed;
	}

	INIT_DELAYED_WORK(&ts->work_att, himax_fb_register);
	queue_delayed_work(ts->himax_att_wq, &ts->work_att, msecs_to_jiffies(15000));
#endif

	/*touch data init*/
	err = himax_report_data_init(ts);
	if (err)
		goto err_report_data_init_failed;
#if defined(HIMAX_I2C_PLATFORM)
	err = himax_sysfs_init(ts);
	if (err)
		goto err_creat_sysfs_file_failed;
#endif

	if (himax_common_proc_init(ts)) {
		E(" %s: himax_common proc_init failed!\n", __func__);
		err = -ENOMEM;
		goto err_creat_proc_file_failed;
	}

	himax_ts_register_interrupt(ts);

	himax_fail_det_register_interrupt(ts);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	if (himax_debug_init(ts))
		E(" %s: debug initial failed!\n", __func__);
#endif

#if defined(HX_BOOT_UPGRADE)
	if (ts->boot_upgrade_flag)
		himax_int_enable(ts, 0);
#endif
	ts->hx_chip_inited = true;
	return 0;

err_creat_proc_file_failed:
#if defined(HIMAX_I2C_PLATFORM)
	himax_sysfs_deinit(ts);
err_creat_sysfs_file_failed:
#endif
	himax_report_data_deinit(ts);
err_report_data_init_failed:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
#if defined(HX_CONFIG_FB) || defined(HX_CONFIG_DRM)
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
err_get_intr_bit_failed:
#endif
err_input_register_device_failed:
	input_free_device(ts->input_dev);
	/*err_detect_failed:*/

#if defined(HX_CONTAINER_SPEED_UP)
	cancel_delayed_work_sync(&ts->ts_int_work);
	destroy_workqueue(ts->ts_int_workqueue);
err_create_ts_resume_wq_failed:
#endif

#if defined(HX_BOOT_UPGRADE)
	cancel_delayed_work_sync(&ts->work_boot_upgrade);
	destroy_workqueue(ts->himax_boot_upgrade_wq);
err_boot_upgrade_wq_failed:
#endif

error_ic_detect_failed:
	himax_gpio_power_deconfig(pdata);
#if !defined(CONFIG_OF)
err_power_failed:
#endif
err_alloc_dt_pdata_failed:
	kfree(ts->hx_touch_data);
	ts->hx_touch_data = NULL;
err_alloc_touch_data_failed:
	kfree(ts->ic_data);
	ts->ic_data = NULL;
err_dt_ic_data_fail:
	kfree(pdata);
	pdata = NULL;
err_dt_platform_data_fail:
	devm_kfree(ts->dev, ts->xfer_buff);
	ts->xfer_buff = NULL;
err_xfer_buff_fail:
	return err;
}

void himax_chip_common_deinit(struct himax_ts_data *ts)
{
	himax_ts_unregister_interrupt(ts);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	himax_inspect_data_clear(ts);
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_debug_remove(ts);
#endif

	himax_common_proc_deinit(ts);
	himax_report_data_deinit(ts);

#if defined(HX_CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		E("Error occurred while unregistering fb_notifier.\n");
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
#elif defined(HX_CONFIG_DRM)

	if (msm_drm_unregister_client(&ts->fb_notif))
		E("Error occurred while unregistering drm_notifier.\n");

	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
#endif
#if defined(HX_CONTAINER_SPEED_UP)
	cancel_delayed_work_sync(&ts->ts_int_work);
	destroy_workqueue(ts->ts_int_workqueue);
#endif
#if defined(HX_BOOT_UPGRADE)
	cancel_delayed_work_sync(&ts->work_boot_upgrade);
	destroy_workqueue(ts->himax_boot_upgrade_wq);
#endif
	himax_gpio_power_deconfig(ts->pdata);
	himax_mcu_in_cmd_struct_free(ts);
	if (ts->input_dev) {
		input_unregister_device(ts->input_dev);
		ts->input_dev = NULL;
	}

#if defined(HIMAX_I2C_PLATFORM)
	himax_sysfs_deinit(ts);
#endif
	kfree(ts->rw_buf);
	ts->rw_buf = NULL;
	kfree(ts->hx_touch_data);
	ts->hx_touch_data = NULL;
	kfree(ts->ic_data);
	ts->ic_data = NULL;
	kfree(ts->pdata->virtual_key);
	ts->pdata->virtual_key = NULL;
	devm_kfree(ts->dev, ts->xfer_buff);
	ts->xfer_buff = NULL;
	kfree(ts->pdata);
	ts->pdata = NULL;
	kfree(ts);
	ts = NULL;

	I("%s: Common section deinited!\n", __func__);
}

int himax_chip_common_suspend(struct himax_ts_data *ts)
{
	if (ts->suspended) {
		I("%s: Already suspended. Skipped.\n", __func__);
		goto END;
	} else {
		ts->suspended = true;
		I("%s: enter\n", __func__);
	}

	if (ts->debug.flash_dump_going == true) {
		I("[himax] %s: Flash dump is going, reject suspend\n", __func__);
		goto END;
	}

	himax_int_enable(ts, 0);
	if (ts->core_fp.fp_suspend_ic_action != NULL)
		ts->core_fp.fp_suspend_ic_action(ts);

	if (!ts->use_irq) {
		int32_t cancel_state;

		cancel_state = cancel_work_sync(&ts->work);
		if (cancel_state)
			himax_int_enable(ts, 1);
	}

	/*ts->first_pressed = 0;*/
	atomic_set(&ts->suspend_mode, 1);
	ts->pre_finger_mask = 0;

	if (ts->pdata)
		if (ts->pdata->powerOff3V3 && ts->pdata->power)
			ts->pdata->power(0);

END:
	if (ts->in_self_test == 1)
		ts->suspend_resume_done = 1;

	I("%s: END\n", __func__);

	return 0;
}

int himax_chip_common_resume(struct himax_ts_data *ts)
{
	I("%s: enter\n", __func__);

	if (ts->suspended == false) {
		I("%s: It had entered resume, skip this step\n", __func__);
		goto END;
	} else {
		ts->suspended = false;
	}

	atomic_set(&ts->suspend_mode, 0);
	ts->diag_cmd = 0;

	if (ts->pdata)
		if (ts->pdata->powerOff3V3 && ts->pdata->power)
			ts->pdata->power(1);

#if defined(HX_RST_PIN_FUNC)
	if (ts->core_fp.fp_ic_reset != NULL)
		ts->core_fp.fp_ic_reset(ts, false, false);
#endif

	himax_report_all_leave_event(ts);

	if (ts->core_fp.fp_sense_on != NULL)
		ts->core_fp.fp_resume_ic_action(ts);

	himax_int_enable(ts, 1);

END:
	if (ts->in_self_test == 1)
		ts->suspend_resume_done = 1;

	I("%s: END\n", __func__);
	return 0;
}
