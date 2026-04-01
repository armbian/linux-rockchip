// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * Rockchip PMIC Type-C Port Controller Driver
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>
#include <linux/string_helpers.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/usb/typec.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/pd.h>
#include <linux/workqueue.h>

#define RK_TCPC_DEVICE_ID			0x00
#define RK_TCPC_DEVICE_VER_ID			GENMASK(7, 4)
#define RK_TCPC_DEVICE_REV_ID			GENMASK(3, 0)

#define RK_TCPC_INT_STS				0x01
#define RK_TCPC_INT_STS_VSAFE0V			BIT(7)
#define RK_TCPC_INT_STS_CC_OV			BIT(6)
#define RK_TCPC_INT_STS_TX_SUCCESS		BIT(5)
#define RK_TCPC_INT_STS_TX_FAILED		BIT(4)
#define RK_TCPC_INT_STS_RX_HARD_RST		BIT(3)
#define RK_TCPC_INT_STS_RX_SUCCESS		BIT(2)
#define RK_TCPC_INT_STS_VBUS			BIT(1)
#define RK_TCPC_INT_STS_CC			BIT(0)
#define RK_TCPC_INT_STS_MASK			GENMASK(7, 0)

#define RK_TCPC_INT				0x02
#define RK_TCPC_INT_VSAFE0V			BIT(7)
#define RK_TCPC_INT_CC_OV			BIT(6)
#define RK_TCPC_INT_TX_SUCCESS			BIT(5)
#define RK_TCPC_INT_TX_FAILED			BIT(4)
#define RK_TCPC_INT_RX_HARD_RST			BIT(3)
#define RK_TCPC_INT_RX_SUCCESS			BIT(2)
#define RK_TCPC_INT_VBUS			BIT(1)
#define RK_TCPC_INT_CC				BIT(0)
#define RK_TCPC_INT_MASK			GENMASK(7, 0)

#define RK_TCPC_CTRL				0x03
#define RK_TCPC_CTRL_CC_POLARITY_CC2		BIT(0)

#define RK_TCPC_CTRL1				0x04
#define RK_TCPC_CTRL1_PD_EN			BIT(0)

#define RK_TCPC_CTRL2				0x05
#define RK_TCPC_CTRL2_TOGGLE			BIT(3)
#define RK_TCPC_CTRL2_CC_RD_EN			BIT(2)
#define RK_TCPC_CTRL2_CC_RP_3_0			(0x3)
#define RK_TCPC_CTRL2_CC_RP_1_5			(0x2)
#define RK_TCPC_CTRL2_CC_RP_DEF			(0x1)
#define RK_TCPC_CTRL2_CC_RP_OFF			(0x0)
#define RK_TCPC_CTRL2_CC_RP_MASK		GENMASK(1, 0)

#define RK_TCPC_CTRL3				0x06
#define RK_TCPC_CTRL3_TDRP_90_MS		(3)
#define RK_TCPC_CTRL3_TDRP_80_MS		(2)
#define RK_TCPC_CTRL3_TDRP_70_MS		(1)
#define RK_TCPC_CTRL3_TDRP_60_MS		(0)
#define RK_TCPC_CTRL3_TDRP			GENMASK(7, 6)
#define RK_TCPC_CTRL3_DCSRCDRP_60		(3)
#define RK_TCPC_CTRL3_DCSRCDRP_50		(2)
#define RK_TCPC_CTRL3_DCSRCDRP_40		(1)
#define RK_TCPC_CTRL3_DCSRCDRP_30		(0)
#define RK_TCPC_CTRL3_DCSRCDRP			GENMASK(5, 4)
#define RK_TCPC_CTRL3_TYPEC_EN			BIT(3)
#define RK_TCPC_CTRL3_VSAFE0V_DET_EN		BIT(2)
#define RK_TCPC_CTRL3_LP_MODE_EN		(BIT(1) | BIT(0))

#define RK_TCPC_STS				0x07
#define RK_TCPC_STS_CC_OV			BIT(6)
#define RK_TCPC_STS_TOGSS_RUNNING		(0x2)
#define RK_TCPC_STS_TOGSS_RP			(0x0)
#define RK_TCPC_STS_TOGSS_RD			(0x1)
#define RK_TCPC_STS_TOGSS			GENMASK(5, 4)
#define RK_TCPC_STS_CC2				GENMASK(3, 2)
#define RK_TCPC_STS_CC1				GENMASK(1, 0)

#define RK_TCPC_STS1				0x08
#define RK_TCPC_STS_ATTACHED_DB_SRC		0x0d
#define RK_TCPC_STS_DETACH_DB_SNK		0x07
#define RK_TCPC_STS_ATTACHED_DB_SNK		0x06
#define RK_TCPC_STS_TYPEC_STATE			GENMASK(7, 3)
#define RK_TCPC_STS_VBUS			BIT(1)
#define RK_TCPC_STS_VSAFE0V			BIT(0)

#define RK_TCPC_RX_DET				0x09
#define RK_TCPC_RX_DET_CABLE_PLUG		BIT(4)
#define RK_TCPC_RX_DET_DATA_ROLE_DFP		BIT(3)
#define RK_TCPC_RX_DET_PD_SPEC_REV		GENMASK(2, 1)
#define RK_TCPC_RX_DET_PWR_ROLE_SRC		BIT(0)

#define RK_TCPC_RX_DET1				0x0a
#define RK_TCPC_RX_DET1_HARD_RST_EN		BIT(6)
#define RK_TCPC_RX_DET1_CABLE_RST_EN		BIT(5)
#define RK_TCPC_RX_DET1_SOP2_EN			BIT(2)
#define RK_TCPC_RX_DET1_SOP1_EN			BIT(1)
#define RK_TCPC_RX_DET1_SOP_EN			BIT(0)

#define RK_TCPC_RX_INFO				0x0b

#define RK_TCPC_RX_CTRL				0x0c
#define RK_TCPC_RX_CTRL_RX_EN			BIT(0)

#define RK_TCPC_TX_CFG				0x0d
#define RK_TCPC_TX_CFG_RETRY_CNT_3		(0x3)
#define RK_TCPC_TX_CFG_RETRY_CNT_2		(0x2)
#define RK_TCPC_TX_CFG_RETRY_CNT_1		(0x1)
#define RK_TCPC_TX_CFG_RETRY_CNT_0		(0x0)
#define RK_TCPC_TX_CFG_RETRY			GENMASK(4, 3)
#define RK_TCPC_TX_CFG_MSG_TYPE			GENMASK(2, 0)

#define RK_TCPC_TX_CFG1				0x0e
#define RK_TCPC_TX_CFG1_TX_LOAD			GENMASK(4, 3)
#define RK_TCPC_TX_CFG1_TX_LOAD_900_OHM		(0x0)
#define RK_TCPC_TX_CFG1_TX_LOAD_800_OHM		(0x1)
#define RK_TCPC_TX_CFG1_TX_LOAD_1000_OHM	(0x2)
#define RK_TCPC_TX_CFG1_TX_LOAD_700_OHM		(0x3)
#define RK_TCPC_TX_CFG1_BIST_TST_MODE		BIT(2)
#define RK_TCPC_TX_CFG1_TX_CARRIER_MODE		BIT(1)
#define RK_TCPC_TX_CFG1_TX_EN			BIT(0)

#define RK_TCPC_TX_CFG2				0x0f
#define RK_TCPC_TX_CFG2_BMC_EN			BIT(0)

#define RK_TCPC_TX_CTRL				0x10
#define RK_TCPC_TX_CTRL_TX_BYTE_CNT		GENMASK(5, 0)

#define RK_TCPC_BMC_STS				0x11

#define RK_TCPC_RESET				0x12
#define RK_TCPC_RESET_TX			BIT(2)
#define RK_TCPC_RESET_RX			BIT(1)
#define RK_TCPC_RESET_PD			BIT(0)

#define RK_TCPC_PD_HEADER			0x15
#define RK_TCPC_PD_DATA				0x17

#define RK_TCPC_DB_CTRL				0x70
#define RK_TCPC_DB_HW_DISABLE			BIT(0)

#define RK_TCPC_REG_OFFSET_MAX			RK_TCPC_DB_CTRL

#define RK_TCPC_PM_DELAY_S			(3 * HZ)

struct rk_tcpc_chip {
	struct device *dev;
	struct regmap *regmap;
	struct tcpm_port *tcpm_port;
	struct tcpc_dev tcpc_dev;
	struct regulator *vbus;
	struct delayed_work pm_work;
	struct iio_channel *vsafe0v_chan;
	int irq;

	/* lock for sharing chip states */
	struct mutex lock;

	/* pd status */
	bool tx_fun_en;
	bool rx_fun_en;

	/* port status */
	bool vbus_on;
	bool charge_on;
	bool vbus_present;
	bool suspended;
	bool vsafe0v_det_en;

	enum typec_cc_status cc1;
	enum typec_cc_status cc2;

#ifdef CONFIG_DEBUG_FS
#define LOG_BUFFER_ENTRIES	1024
#define LOG_BUFFER_ENTRY_SIZE	128

	struct dentry *dentry;
	/* lock for log buffer access */
	struct mutex logbuffer_lock;
	int logbuffer_head;
	int logbuffer_tail;
	u8 *logbuffer[LOG_BUFFER_ENTRIES];
#endif
};

/*
 * Logging
 */

#ifdef CONFIG_DEBUG_FS

static bool rk_tcpc_log_full(struct rk_tcpc_chip *chip)
{
	return chip->logbuffer_tail ==
		(chip->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;
}

__printf(2, 0)
static void _rk_tcpc_log(struct rk_tcpc_chip *chip, const char *fmt,
			 va_list args)
{
	char tmpbuffer[LOG_BUFFER_ENTRY_SIZE];
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec;

	mutex_lock(&chip->logbuffer_lock);

	if (chip->logbuffer_head < 0 ||
	    chip->logbuffer_head >= LOG_BUFFER_ENTRIES) {
		dev_warn(chip->dev,
			 "Bad log buffer index %d\n", chip->logbuffer_head);
		goto abort;
	}

	if (!chip->logbuffer[chip->logbuffer_head]) {
		chip->logbuffer[chip->logbuffer_head] =
				kzalloc(LOG_BUFFER_ENTRY_SIZE, GFP_KERNEL);
		if (!chip->logbuffer[chip->logbuffer_head])
			goto abort;
	}

	vsnprintf(tmpbuffer, sizeof(tmpbuffer), fmt, args);
	dev_dbg(chip->dev, "%s", tmpbuffer);

	if (rk_tcpc_log_full(chip)) {
		chip->logbuffer_head = max(chip->logbuffer_head - 1, 0);
		strscpy(tmpbuffer, "overflow", sizeof(tmpbuffer));
	}

	if (!chip->logbuffer[chip->logbuffer_head]) {
		dev_warn(chip->dev,
			 "Log buffer index %d is NULL\n", chip->logbuffer_head);
		goto abort;
	}

	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(chip->logbuffer[chip->logbuffer_head],
		  LOG_BUFFER_ENTRY_SIZE, "[%5lu.%06lu] %s",
		  (unsigned long)ts_nsec, rem_nsec / 1000,
		  tmpbuffer);
	chip->logbuffer_head = (chip->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;

abort:
	mutex_unlock(&chip->logbuffer_lock);
}

__printf(2, 3)
static void rk_tcpc_log(struct rk_tcpc_chip *chip, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_rk_tcpc_log(chip, fmt, args);
	va_end(args);
}

static int rk_tcpc_debug_show(struct seq_file *s, void *v)
{
	struct rk_tcpc_chip *chip = (struct rk_tcpc_chip *)s->private;
	int tail;

	mutex_lock(&chip->logbuffer_lock);
	tail = chip->logbuffer_tail;
	while (tail != chip->logbuffer_head) {
		seq_printf(s, "%s\n", chip->logbuffer[tail]);
		tail = (tail + 1) % LOG_BUFFER_ENTRIES;
	}

	if (!seq_has_overflowed(s))
		chip->logbuffer_tail = tail;
	mutex_unlock(&chip->logbuffer_lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(rk_tcpc_debug);

static void rk_tcpc_debugfs_init(struct rk_tcpc_chip *chip)
{
	char name[NAME_MAX];

	mutex_init(&chip->logbuffer_lock);
	snprintf(name, NAME_MAX, "rk-tcpc-%s", dev_name(chip->dev));
	chip->dentry = debugfs_create_dir(name, usb_debug_root);
	debugfs_create_file("log", S_IFREG | 0444, chip->dentry, chip,
			    &rk_tcpc_debug_fops);
}

static void rk_tcpc_debugfs_exit(struct rk_tcpc_chip *chip)
{
	int i;

	mutex_lock(&chip->logbuffer_lock);
	for (i = 0; i < LOG_BUFFER_ENTRIES; i++) {
		kfree(chip->logbuffer[i]);
		chip->logbuffer[i] = NULL;
	}
	mutex_unlock(&chip->logbuffer_lock);

	debugfs_remove(chip->dentry);
}

#else

static void rk_tcpc_log(const struct rk_tcpc_chip *chip, const char *fmt, ...) { }
static void rk_tcpc_debugfs_init(const struct rk_tcpc_chip *chip) { }
static void rk_tcpc_debugfs_exit(const struct rk_tcpc_chip *chip) { }

#endif

static const struct regmap_config rk_tcpc_regmap_cfg = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK_TCPC_REG_OFFSET_MAX,
};

static bool rk_tcpc_cc_is_open(enum typec_cc_status cc1, enum typec_cc_status cc2)
{
	return ((cc1 == TYPEC_CC_OPEN && (cc2 == TYPEC_CC_OPEN || cc2 == TYPEC_CC_RA)) ||
		(cc2 == TYPEC_CC_OPEN && (cc1 == TYPEC_CC_OPEN || cc1 == TYPEC_CC_RA)));
}

static int rk_tcpc_write8(struct rk_tcpc_chip *chip, unsigned int reg, u8 val)
{
	int ret = 0;

	ret = regmap_raw_write(chip->regmap, reg, &val, sizeof(u8));
	if (ret < 0)
		rk_tcpc_log(chip, "cannot write 0x%02x to 0x%02x, ret=%d", val, reg, ret);

	return ret;
}

static int rk_tcpc_block_write(struct rk_tcpc_chip *chip, unsigned int reg,
			       u8 *data, u8 len)
{
	int ret = 0;

	ret = regmap_raw_write(chip->regmap, reg, data, len);
	if (ret < 0)
		rk_tcpc_log(chip, "cannot block write 0x%02x, len=%d, ret=%d", reg, len, ret);

	return ret;
}

static int rk_tcpc_read8(struct rk_tcpc_chip *chip, unsigned int reg, u8 *val)
{
	int ret = 0;

	ret = regmap_raw_read(chip->regmap, reg, val, sizeof(u8));
	if (ret < 0)
		rk_tcpc_log(chip, "cannot read 0x%02x, ret=%d", reg, ret);

	return ret;
}

static int rk_tcpc_block_read(struct rk_tcpc_chip *chip, u8 reg,
			      u8 *data, u8 len)
{
	int ret = 0;

	if (len <= 0)
		return ret;

	ret = regmap_raw_read(chip->regmap, reg, data, len);
	if (ret < 0)
		rk_tcpc_log(chip, "cannot block read 0x%02x, len=%d, ret=%d", reg, len, ret);
	return ret;
}

static int rk_tcpc_check_id(struct i2c_client *i2c)
{
	int ret;

	ret = i2c_smbus_read_byte_data(i2c, RK_TCPC_DEVICE_ID);
	if (ret < 0) {
		dev_err(&i2c->dev, "cannot read Device id, ret=%d\n", ret);
		return ret;
	}

	dev_info(&i2c->dev, "Version ID: 0x%02lx, Revision ID: 0x%02lx,\n",
		 FIELD_GET(RK_TCPC_DEVICE_VER_ID, ret), FIELD_GET(RK_TCPC_DEVICE_REV_ID, ret));

	return 0;
}

static int rk_tcpc_sw_reset(struct rk_tcpc_chip *chip)
{
	int ret;

	ret = rk_tcpc_write8(chip, RK_TCPC_RESET, RK_TCPC_RESET_PD);
	if (ret < 0)
		rk_tcpc_log(chip, "cannot sw reset the chip, ret=%d", ret);
	else
		rk_tcpc_log(chip, "sw reset");

	return ret;
}

static int rk_tcpc_enable_tx(struct rk_tcpc_chip *chip, bool enable)
{
	int ret = 0;

	/*
	 * TX module is usually disabled to save power consumption,
	 * enabled it at first PD transmit.
	 */
	if (enable) {
		if (!chip->tx_fun_en) {
			ret = rk_tcpc_write8(chip, RK_TCPC_TX_CFG1, RK_TCPC_TX_CFG1_TX_EN);
			if (ret < 0)
				return ret;

			chip->tx_fun_en = true;
			rk_tcpc_log(chip, "PD tx enabled");
		}
	} else {
		if (chip->tx_fun_en) {
			ret = rk_tcpc_write8(chip, RK_TCPC_TX_CFG1, 0);
			if (ret < 0)
				return ret;

			chip->tx_fun_en = false;
			rk_tcpc_log(chip, "PD tx disabled");
		}
	}

	return 0;
}

static int rk_tcpc_enable_rx(struct rk_tcpc_chip *chip, bool enable)
{
	int ret = 0;

	/*
	 * RX module is usually disabled to save power consumption,
	 * enabled it at first prepare for PD receiving.
	 */
	if (enable) {
		if (!chip->rx_fun_en) {
			ret = rk_tcpc_write8(chip, RK_TCPC_RX_CTRL, RK_TCPC_RX_CTRL_RX_EN);
			if (ret < 0)
				return ret;

			chip->rx_fun_en = true;
			rk_tcpc_log(chip, "PD rx enabled");
		}
	} else {
		if (chip->rx_fun_en) {
			ret = rk_tcpc_write8(chip, RK_TCPC_RX_CTRL, 0);
			if (ret < 0)
				return ret;

			chip->rx_fun_en = false;
			rk_tcpc_log(chip, "PD rx disabled");
		}
	}

	return 0;
}

static int __rk_tcpc_set_lpmode(struct rk_tcpc_chip *chip)
{
	u8 reg = 0;
	int ret;

	/* Set Rp default as 80uA */
	ret = rk_tcpc_read8(chip, RK_TCPC_CTRL2, &reg);
	if (ret < 0)
		return ret;

	reg &= ~RK_TCPC_CTRL2_CC_RP_MASK;
	reg |= RK_TCPC_CTRL2_CC_RP_DEF;

	ret = rk_tcpc_write8(chip, RK_TCPC_CTRL2, reg);
	if (ret < 0)
		return ret;

	/* lp mode enable */
	ret = rk_tcpc_read8(chip, RK_TCPC_CTRL3, &reg);
	if (ret < 0)
		return ret;

	reg |= RK_TCPC_CTRL3_LP_MODE_EN;

	ret = rk_tcpc_write8(chip, RK_TCPC_CTRL3, reg);
	if (ret < 0)
		return ret;

	return 0;
}

static int rk_tcpc_set_lpmode(struct rk_tcpc_chip *chip)
{
	int ret = 0;

	mutex_lock(&chip->lock);

	if (!chip->suspended && rk_tcpc_cc_is_open(chip->cc1, chip->cc2)) {
		ret = __rk_tcpc_set_lpmode(chip);
		if (!ret) {
			chip->suspended = 1;
			rk_tcpc_log(chip, "enter lp mode");
		}
	}

	mutex_unlock(&chip->lock);
	return ret;
}

static enum typec_cc_status rk_tcpc_sts_to_cc(u8 sts, bool sink)
{
	switch (sts) {
	case 0x1:
		return sink ? TYPEC_CC_RP_DEF : TYPEC_CC_RA;
	case 0x2:
		return sink ? TYPEC_CC_RP_1_5 : TYPEC_CC_RD;
	case 0x3:
		if (sink)
			return TYPEC_CC_RP_3_0;
		fallthrough;
	case 0x0:
	default:
		return TYPEC_CC_OPEN;
	}
}

static int rk_tcpc_pd_read_message(struct rk_tcpc_chip *chip,
				   struct pd_message *msg)
{
	u16 header;
	int len;
	int ret;

	ret = rk_tcpc_block_read(chip, RK_TCPC_PD_HEADER, (u8 *)&header, 2);
	if (ret < 0)
		return ret;

	len = pd_header_cnt_le(header) * 4;
	if (len > PD_MAX_PAYLOAD * 4) {
		rk_tcpc_log(chip, "PD message too long %d", len);
		return -EINVAL;
	}

	msg->header = cpu_to_le16(header);

	if (len > 0) {
		ret = rk_tcpc_block_read(chip, RK_TCPC_PD_DATA,
					 (u8 *)&msg->payload, len);
		if (ret < 0)
			return ret;
	}

	rk_tcpc_log(chip, "PD header: 0x%04x, payload len: %d", msg->header, len);

	return 0;
}

static int rk_tcpc_init_interrupt(struct rk_tcpc_chip *chip)
{
	int ret = 0;
	u8 reg;

	/* Clear all events */
	ret = rk_tcpc_write8(chip, RK_TCPC_INT_STS, RK_TCPC_INT_STS_MASK);
	if (ret < 0)
		return ret;

	reg = RK_TCPC_INT_CC | RK_TCPC_INT_VBUS | RK_TCPC_INT_RX_SUCCESS |
	      RK_TCPC_INT_RX_HARD_RST | RK_TCPC_INT_TX_FAILED |
	      RK_TCPC_INT_TX_SUCCESS;

	ret = rk_tcpc_write8(chip, RK_TCPC_INT, reg);
	if (ret < 0)
		return ret;

	return 0;
}

static int tcpm_init(struct tcpc_dev *dev)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg;
	int ret;

	ret = rk_tcpc_sw_reset(chip);
	if (ret < 0)
		return ret;

	/* Clear all events */
	ret = rk_tcpc_write8(chip, RK_TCPC_INT_STS, RK_TCPC_INT_STS_MASK);
	if (ret < 0)
		return ret;

	/* Disable all interrupts before requesting irq */
	ret = rk_tcpc_write8(chip, RK_TCPC_INT, 0);
	if (ret < 0)
		return ret;

	/* Disable HW debounce */
	ret = rk_tcpc_write8(chip, RK_TCPC_DB_CTRL, RK_TCPC_DB_HW_DISABLE);
	if (ret < 0)
		return ret;

	/* tDRP : 80 ms */
	reg = FIELD_PREP(RK_TCPC_CTRL3_TDRP, RK_TCPC_CTRL3_TDRP_80_MS);

	/* dcSRC.DRP : 30%src + 70%snk */
	reg |= FIELD_PREP(RK_TCPC_CTRL3_DCSRCDRP, RK_TCPC_CTRL3_DCSRCDRP_30);

	/* Enable lp_mode_en/cc_ov_det_en/lg_typec_en */
	reg |= RK_TCPC_CTRL3_LP_MODE_EN |
	       RK_TCPC_CTRL3_VSAFE0V_DET_EN |
	       RK_TCPC_CTRL3_TYPEC_EN;

	ret = rk_tcpc_write8(chip, RK_TCPC_CTRL3, reg);
	if (ret < 0)
		return ret;

	chip->suspended = 1;
	rk_tcpc_log(chip, "ctrl3(06h): %02x", reg);

	return 0;
}

static int tcpm_get_vbus(struct tcpc_dev *dev)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg;
	int ret;

	mutex_lock(&chip->lock);

	ret = rk_tcpc_read8(chip, RK_TCPC_STS1, &reg);
	if (ret < 0) {
		mutex_unlock(&chip->lock);
		return 0;
	}

	chip->vbus_present = !!(reg & RK_TCPC_STS_VBUS);
	ret = chip->vbus_present;
	rk_tcpc_log(chip, "sts1(08h) : %02x, vbus present %d", reg, chip->vbus_present);

	mutex_unlock(&chip->lock);
	return ret;
}

static const char * const cc_status_name[] = {
	[TYPEC_CC_OPEN]		= "Open",
	[TYPEC_CC_RA]		= "Ra",
	[TYPEC_CC_RD]		= "Rd",
	[TYPEC_CC_RP_DEF]	= "Rp-def",
	[TYPEC_CC_RP_1_5]	= "Rp-1.5",
	[TYPEC_CC_RP_3_0]	= "Rp-3.0",
};

static int tcpm_set_cc(struct tcpc_dev *dev, enum typec_cc_status cc)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg = 0;
	int ret;

	switch (cc) {
	case TYPEC_CC_OPEN: /* cc_rd_en = 0 && rp_value = 0x0 */
		break;
	case TYPEC_CC_RD:
		ret = rk_tcpc_read8(chip, RK_TCPC_CTRL2, &reg);
		if (ret)
			return ret;
		reg |= RK_TCPC_CTRL2_CC_RD_EN;
		break;
	case TYPEC_CC_RP_DEF:
		reg |= RK_TCPC_CTRL2_CC_RP_DEF;
		break;
	case TYPEC_CC_RP_1_5:
		reg |= RK_TCPC_CTRL2_CC_RP_1_5;
		break;
	case TYPEC_CC_RP_3_0:
		reg |= RK_TCPC_CTRL2_CC_RP_3_0;
		break;
	default:
		rk_tcpc_log(chip, "unsupported cc value %s", cc_status_name[cc]);
		return -EINVAL;
	}

	ret = rk_tcpc_write8(chip, RK_TCPC_CTRL2, reg);
	if (ret)
		return ret;

	rk_tcpc_log(chip, "cc := %s, ctl2(05h) : %02x", cc_status_name[cc], reg);

	return 0;
}

static int tcpm_get_cc(struct tcpc_dev *dev, enum typec_cc_status *cc1,
		       enum typec_cc_status *cc2)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 sts, sts1, togdone, state;
	u16 reg;
	int ret;

	ret = rk_tcpc_block_read(chip, RK_TCPC_STS, (u8 *)&reg, 2);
	if (ret < 0)
		return ret;

	sts = reg & 0x00FF;
	sts1 = reg >> 8;

	rk_tcpc_log(chip, "status(07h) : %02x, status1(08h) : %02x", sts, sts1);

	/* Escape CC Open state during PD transfer */
	state = FIELD_GET(RK_TCPC_STS_TYPEC_STATE, sts1);
	if (((sts & 0xf) == 0) &&
	    (state == RK_TCPC_STS_ATTACHED_DB_SNK || state == RK_TCPC_STS_DETACH_DB_SNK)) {
		*cc1 = chip->cc1;
		*cc2 = chip->cc2;

		rk_tcpc_log(chip, "keep the previous cc value");
		return 0;
	}

	togdone = FIELD_GET(RK_TCPC_STS_TOGSS, sts);
	switch (togdone) {
	case RK_TCPC_STS_TOGSS_RD:
		rk_tcpc_log(chip, "presenting Rd");
		*cc1 = rk_tcpc_sts_to_cc(FIELD_GET(RK_TCPC_STS_CC1, sts), true);
		*cc2 = rk_tcpc_sts_to_cc(FIELD_GET(RK_TCPC_STS_CC2, sts), true);
		break;

	case RK_TCPC_STS_TOGSS_RP:
		rk_tcpc_log(chip, "presenting Rp");
		*cc1 = rk_tcpc_sts_to_cc(FIELD_GET(RK_TCPC_STS_CC1, sts), false);
		*cc2 = rk_tcpc_sts_to_cc(FIELD_GET(RK_TCPC_STS_CC2, sts), false);
		break;

	case RK_TCPC_STS_TOGSS_RUNNING:
	default:
		rk_tcpc_log(chip, "TOGDONE with an invalid state");
		*cc1 = TYPEC_CC_OPEN;
		*cc2 = TYPEC_CC_OPEN;
		break;
	}

	chip->cc1 = *cc1;
	chip->cc2 = *cc2;

	rk_tcpc_log(chip, "detected: cc1=%s, cc2=%s", cc_status_name[*cc1], cc_status_name[*cc2]);
	return 0;
}

static const char * const cc_polarity_name[] = {
	[TYPEC_POLARITY_CC1]	= "Polarity_CC1",
	[TYPEC_POLARITY_CC2]	= "Polarity_CC2",
};

static int tcpm_set_polarity(struct tcpc_dev *dev,
			     enum typec_cc_polarity polarity)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg = 0;
	int ret;

	reg |= polarity == TYPEC_POLARITY_CC2 ? RK_TCPC_CTRL_CC_POLARITY_CC2 : 0;
	ret = rk_tcpc_write8(chip, RK_TCPC_CTRL, reg);
	if (ret < 0)
		return ret;

	rk_tcpc_log(chip, "set %s", cc_polarity_name[polarity]);

	return 0;
}

static int tcpm_set_vconn(struct tcpc_dev *dev, bool on)
{
	/* unsupported */
	return 0;
}

static int tcpm_set_vbus(struct tcpc_dev *dev, bool on, bool charge)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	int ret = 0;

	mutex_lock(&chip->lock);

	if (chip->vbus_on == on) {
		rk_tcpc_log(chip, "vbus is already %s", str_on_off(on));
	} else {
		if (on)
			ret = regulator_enable(chip->vbus);
		else
			ret = regulator_disable(chip->vbus);
		if (ret < 0) {
			rk_tcpc_log(chip, "cannot %s vbus regulator, ret=%d",
				    str_enable_disable(on), ret);
			goto done;
		}
		chip->vbus_on = on;
		rk_tcpc_log(chip, "vbus := %s", str_on_off(on));
	}

	if (chip->charge_on == charge)
		rk_tcpc_log(chip, "charge is already %s",
			    str_on_off(charge));
	else
		chip->charge_on = charge;

done:
	mutex_unlock(&chip->lock);
	return ret;
}

static bool tcpm_is_vbus_vsafe0v(struct tcpc_dev *dev)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	int vol;
	u8 reg;
	int ret;

	if (!chip->vsafe0v_det_en)
		return true;

	/*
	 * [TD.4.7.2.V.11] PUT remains in Attached.SNK
	 * [TD.4.7.2.V.12 ~ V.14] PUT maintain USB communication
	 */
	if (chip->vsafe0v_chan) {
		ret = iio_read_channel_processed(chip->vsafe0v_chan, &vol);
		if (ret < 0)
			return true;

		rk_tcpc_log(chip, "vsafe0v vol: %d", vol);

		return vol < 80; /* the resistance voltage division ratio is 10K / (10K + 100K) */
	}

	ret = rk_tcpc_read8(chip, RK_TCPC_STS1, &reg);
	if (ret < 0)
		return true;

	rk_tcpc_log(chip, "vsafe0v(08h) : 0x%02x", reg);

	return !!(reg & RK_TCPC_STS_VSAFE0V);
}

static const char * const typec_role_name[] = {
	[TYPEC_SINK]		= "Sink",
	[TYPEC_SOURCE]		= "Source",
};

static const char * const typec_data_role_name[] = {
	[TYPEC_DEVICE]		= "Device",
	[TYPEC_HOST]		= "Host",
};

static int tcpm_set_roles(struct tcpc_dev *dev, bool attached,
			  enum typec_role role, enum typec_data_role data)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg;
	int ret;

	reg = FIELD_PREP(RK_TCPC_RX_DET_PD_SPEC_REV, PD_REV20);
	if (role == TYPEC_SOURCE)
		reg |= RK_TCPC_RX_DET_PWR_ROLE_SRC;
	if (data == TYPEC_HOST)
		reg |= RK_TCPC_RX_DET_DATA_ROLE_DFP;

	ret = rk_tcpc_write8(chip, RK_TCPC_RX_DET, reg);
	if (ret < 0) {
		rk_tcpc_log(chip, "cannot set roles %s, %s, ret=%d",
			    typec_role_name[role], typec_data_role_name[data],
			    ret);
		return ret;
	}

	rk_tcpc_log(chip, "set roles := %s, %s", typec_role_name[role],
		    typec_data_role_name[data]);

	return 0;
}

static int tcpm_start_toggling(struct tcpc_dev *dev,
			       enum typec_port_type port_type,
			       enum typec_cc_status cc)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg = 0;
	int ret;

	if (port_type != TYPEC_PORT_DRP)
		return -EOPNOTSUPP;

	rk_tcpc_log(chip, "cc value %s", cc_status_name[cc]);

	switch (cc) {
	case TYPEC_CC_RD:
		reg |= RK_TCPC_CTRL2_CC_RD_EN;
		fallthrough;
	case TYPEC_CC_RP_DEF:
		reg |= RK_TCPC_CTRL2_CC_RP_DEF;
		break;
	case TYPEC_CC_RP_1_5:
		reg |= RK_TCPC_CTRL2_CC_RP_1_5;
		break;
	case TYPEC_CC_RP_3_0:
		reg |= RK_TCPC_CTRL2_CC_RP_3_0;
		break;
	default:
		rk_tcpc_log(chip, "unsupported cc value %s", cc_status_name[cc]);
		return -EINVAL;
	}

	reg |= RK_TCPC_CTRL2_TOGGLE;
	ret = rk_tcpc_write8(chip, RK_TCPC_CTRL2, reg);
	if (ret)
		return ret;

	rk_tcpc_log(chip, "start drp toggling, ctrl2(05h): %02x", reg);

	return 0;
}

static int tcpm_set_pd_rx(struct tcpc_dev *dev, bool on)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg = 0;
	int ret;

	mutex_lock(&chip->lock);

	if (on)
		reg = RK_TCPC_RX_DET1_SOP_EN | RK_TCPC_RX_DET1_HARD_RST_EN;
	ret = rk_tcpc_write8(chip, RK_TCPC_RX_DET1, reg);
	if (ret < 0) {
		rk_tcpc_log(chip, "cannot set rx detect %s", str_on_off(on));
		goto out;
	} else {
		rk_tcpc_log(chip, "set rx detect %s", str_on_off(on));
	}

	ret = rk_tcpc_enable_rx(chip, on);
	if (ret)
		goto out;

	ret = rk_tcpc_enable_tx(chip, on);
	if (ret)
		goto out;

out:
	mutex_unlock(&chip->lock);
	return ret;
}

static const char * const tx_type_name[] = {
	[TCPC_TX_SOP]			= "SOP",
	[TCPC_TX_SOP_PRIME]		= "SOP'",
	[TCPC_TX_SOP_PRIME_PRIME]	= "SOP''",
	[TCPC_TX_SOP_DEBUG_PRIME]	= "DEBUG'",
	[TCPC_TX_SOP_DEBUG_PRIME_PRIME]	= "DEBUG''",
	[TCPC_TX_HARD_RESET]		= "HARD_RESET",
	[TCPC_TX_CABLE_RESET]		= "CABLE_RESET",
	[TCPC_TX_BIST_MODE_2]		= "BIST_MODE_2",
};

static int tcpm_pd_transmit(struct tcpc_dev *dev, enum tcpm_transmit_type type,
			    const struct pd_message *msg, unsigned int negotiated_rev)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u16 header = 0;
	u8 cnt = 0;
	u8 reg;
	int ret = 0;

	mutex_lock(&chip->lock);

	if (type == TCPC_TX_BIST_MODE_2) {
		ret = rk_tcpc_read8(chip, RK_TCPC_TX_CFG1, &reg);
		if (ret < 0)
			goto out;

		reg |= RK_TCPC_TX_CFG1_BIST_TST_MODE;

		ret = rk_tcpc_write8(chip, RK_TCPC_TX_CFG1, reg);
		if (ret < 0)
			goto out;

		rk_tcpc_log(chip, "TX bist mode, tx_cfg1(0eh): %02x", reg);
		goto out;
	}

	if (msg) {
		header = le16_to_cpu(msg->header);
		cnt = pd_header_cnt(header) * 4;

		ret = rk_tcpc_write8(chip, RK_TCPC_TX_CTRL, cnt + 2);
		if (ret < 0)
			goto out;

		ret = rk_tcpc_block_write(chip, RK_TCPC_PD_HEADER, (u8 *)&header, 2);
		if (ret < 0)
			goto out;
	}

	if (cnt > 0) {
		ret = rk_tcpc_block_write(chip, RK_TCPC_PD_DATA, (u8 *)&msg->payload, cnt);
		if (ret < 0)
			goto out;
	}

	/* nRetryCount is 3 in PD2.0 spec where 2 in PD3.0 spec */
	reg = FIELD_PREP(RK_TCPC_TX_CFG_RETRY,
			 (negotiated_rev > PD_REV20 ?
			  RK_TCPC_TX_CFG_RETRY_CNT_2 :
			  RK_TCPC_TX_CFG_RETRY_CNT_3));
	reg |= FIELD_PREP(RK_TCPC_TX_CFG_MSG_TYPE, type);
	ret = rk_tcpc_write8(chip, RK_TCPC_TX_CFG, reg);
	if (ret < 0) {
		rk_tcpc_log(chip, "cannot send PD message type %s , ret=%d",
			    tx_type_name[type], ret);
		goto out;
	}

	/* Enable TX bmc */
	ret = rk_tcpc_write8(chip, RK_TCPC_TX_CFG2, RK_TCPC_TX_CFG2_BMC_EN);
	if (ret < 0)
		goto out;

	if (msg)
		rk_tcpc_log(chip, "sending PD message header: %#x, len: %d", msg->header, cnt);
	else
		rk_tcpc_log(chip, "sending PD message type: %s", tx_type_name[type]);

out:
	mutex_unlock(&chip->lock);
	return ret;
}

static int tcpm_set_bist_data(struct tcpc_dev *dev, bool on)
{
	struct rk_tcpc_chip *chip = container_of(dev, struct rk_tcpc_chip, tcpc_dev);
	u8 reg;
	int ret = 0;

	/* RX Interrupt */
	ret = rk_tcpc_read8(chip, RK_TCPC_INT, &reg);
	if (ret < 0)
		return ret;

	if (on)
		reg &= ~RK_TCPC_INT_RX_SUCCESS;
	else
		reg |= RK_TCPC_INT_RX_SUCCESS;

	ret = rk_tcpc_write8(chip, RK_TCPC_INT, reg);
	if (ret < 0)
		return ret;

	/* HW debounce */
	ret = rk_tcpc_read8(chip, RK_TCPC_DB_CTRL, &reg);
	if (ret < 0)
		return ret;

	if (on)
		reg &= ~RK_TCPC_DB_HW_DISABLE;
	else
		reg |= RK_TCPC_DB_HW_DISABLE;

	ret = rk_tcpc_write8(chip, RK_TCPC_DB_CTRL, reg);
	if (ret < 0)
		return ret;

	return 0;
}

static void rk_tcpc_init_tcpc_dev(struct tcpc_dev *rk_tcpc_dev)
{
	rk_tcpc_dev->init = tcpm_init;
	rk_tcpc_dev->get_vbus = tcpm_get_vbus;
	rk_tcpc_dev->set_cc = tcpm_set_cc;
	rk_tcpc_dev->get_cc = tcpm_get_cc;
	rk_tcpc_dev->set_polarity = tcpm_set_polarity;
	rk_tcpc_dev->set_vconn = tcpm_set_vconn;
	rk_tcpc_dev->set_vbus = tcpm_set_vbus;
	rk_tcpc_dev->is_vbus_vsafe0v = tcpm_is_vbus_vsafe0v;
	rk_tcpc_dev->set_pd_rx = tcpm_set_pd_rx;
	rk_tcpc_dev->set_roles = tcpm_set_roles;
	rk_tcpc_dev->set_bist_data = tcpm_set_bist_data;
	rk_tcpc_dev->start_toggling = tcpm_start_toggling;
	rk_tcpc_dev->pd_transmit = tcpm_pd_transmit;
}

static void rk_tcpc_pm_work(struct work_struct *work)
{
	struct rk_tcpc_chip *chip = container_of(work, struct rk_tcpc_chip, pm_work.work);

	rk_tcpc_log(chip, "pm work: cc1=%s, cc2=%s",
		    cc_status_name[chip->cc1], cc_status_name[chip->cc2]);

	rk_tcpc_set_lpmode(chip);
}

static irqreturn_t rk_tcpc_irq_work(int irq, void *dev_id)
{
	struct rk_tcpc_chip *chip = dev_id;
	u8 int_status, status1;
	bool vbus_present;
	int ret;

	ret = rk_tcpc_read8(chip, RK_TCPC_INT_STS, &int_status);
	if (ret < 0)
		return IRQ_HANDLED;

	if (!(FIELD_GET(RK_TCPC_INT_STS_MASK, int_status)))
		return IRQ_NONE;

	rk_tcpc_log(chip, "IRQ(01h): 0x%02x", int_status);

	/*
	 * Clear alert status for everything except RX_STATUS, which shouldn't
	 * be cleared until we have successfully retrieved message.
	 */
	if (int_status & ~RK_TCPC_INT_STS_RX_SUCCESS)
		rk_tcpc_write8(chip, RK_TCPC_INT_STS,
			       int_status & ~RK_TCPC_INT_STS_RX_SUCCESS);

	if (int_status & RK_TCPC_INT_STS_VBUS) {
		rk_tcpc_read8(chip, RK_TCPC_STS1, &status1);
		vbus_present = !!(status1 & RK_TCPC_STS_VBUS);
		rk_tcpc_log(chip, "IRQ: VBUS %s", str_on_off(vbus_present));

		if (vbus_present != chip->vbus_present) {
			chip->vbus_present = vbus_present;
			tcpm_vbus_change(chip->tcpm_port);
		}
	}

	if (int_status & RK_TCPC_INT_STS_CC) {
		rk_tcpc_log(chip, "IRQ: CC change");
		chip->suspended = 0;
		queue_delayed_work(system_freezable_wq, &chip->pm_work, RK_TCPC_PM_DELAY_S);
		tcpm_cc_change(chip->tcpm_port);
	}

	if (int_status & RK_TCPC_INT_STS_RX_SUCCESS) {
		struct pd_message msg;

		rk_tcpc_log(chip, "IRQ: PD rx success");
		ret = rk_tcpc_pd_read_message(chip, &msg);
		if (ret < 0) {
			rk_tcpc_log(chip, "cannot read PD message, ret=%d", ret);
			return IRQ_HANDLED;
		}

		/* Read complete, clear RX interrupt status bit */
		rk_tcpc_write8(chip, RK_TCPC_INT_STS, RK_TCPC_INT_STS_RX_SUCCESS);

		tcpm_pd_receive(chip->tcpm_port, &msg, TCPC_TX_SOP);
	}

	if (int_status & RK_TCPC_INT_STS_TX_SUCCESS) {
		rk_tcpc_log(chip, "IRQ: PD tx success");
		tcpm_pd_transmit_complete(chip->tcpm_port, TCPC_TX_SUCCESS);
	} else if (int_status & RK_TCPC_INT_STS_TX_FAILED) {
		rk_tcpc_log(chip, "IRQ: PD tx failed");
		tcpm_pd_transmit_complete(chip->tcpm_port, TCPC_TX_FAILED);
	}

	if (int_status & RK_TCPC_INT_STS_RX_HARD_RST) {
		rk_tcpc_log(chip, "IRQ: PD received hardreset");
		tcpm_pd_hard_reset(chip->tcpm_port);
	}

	if (int_status & RK_TCPC_INT_STS_VSAFE0V) {
		rk_tcpc_read8(chip, RK_TCPC_STS1, &status1);
		rk_tcpc_log(chip, "IRQ: VSAFE0V %d", !!(status1 & RK_TCPC_STS_VSAFE0V));
	}

	if (int_status & RK_TCPC_INT_STS_CC_OV) {
		rk_tcpc_log(chip, "IRQ: CC overvoltage");
		WARN(1, "RK TCPC CC Overvoltage");
	}

	return IRQ_HANDLED;
}

static int rk_tcpc_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct rk_tcpc_chip *chip;
	int ret;

	ret = rk_tcpc_check_id(client);
	if (ret < 0)
		return ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->regmap = devm_regmap_init_i2c(client, &rk_tcpc_regmap_cfg);
	if (IS_ERR(chip->regmap))
		return PTR_ERR(chip->regmap);

	i2c_set_clientdata(client, chip);

	chip->dev = &client->dev;
	chip->irq = client->irq;
	mutex_init(&chip->lock);
	INIT_DELAYED_WORK(&chip->pm_work, rk_tcpc_pm_work);

	chip->vsafe0v_det_en = device_property_present(chip->dev, "rockchip,vsafe0v-det");

	if (chip->vsafe0v_det_en && device_property_present(chip->dev, "io-channels")) {
		chip->vsafe0v_chan = devm_iio_channel_get(chip->dev, "vsafe0v-detect");
		if (IS_ERR(chip->vsafe0v_chan))
			return dev_err_probe(chip->dev, PTR_ERR(chip->vsafe0v_chan),
					     "cannot get vsafe0v_chan IIO channel\n");
	}

	chip->vbus = devm_regulator_get_optional(chip->dev, "vbus");
	if (IS_ERR(chip->vbus)) {
		ret = PTR_ERR(chip->vbus);
		if (ret != -ENODEV)
			return ret;
		chip->vbus = NULL;
	}

	rk_tcpc_init_tcpc_dev(&chip->tcpc_dev);
	rk_tcpc_debugfs_init(chip);

	chip->tcpc_dev.fwnode = device_get_named_child_node(dev, "connector");
	if (IS_ERR(chip->tcpc_dev.fwnode)) {
		ret = PTR_ERR(chip->tcpc_dev.fwnode);
		goto debugfs_exit;
	}

	/* Disable chip interrupts before requesting irq */
	ret = rk_tcpc_write8(chip, RK_TCPC_INT, 0);
	if (ret < 0)
		return ret;

	chip->tcpm_port = tcpm_register_port(&client->dev, &chip->tcpc_dev);
	if (IS_ERR(chip->tcpm_port)) {
		ret = dev_err_probe(dev, PTR_ERR(chip->tcpm_port),
				    "cannot register tcpm port\n");
		goto fwnode_put;
	}

	ret = devm_request_threaded_irq(chip->dev, chip->irq,
					NULL, rk_tcpc_irq_work,
					IRQF_SHARED | IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					client->name, chip);
	if (ret < 0) {
		dev_err(dev, "cannot request irq, ret=%d", ret);
		goto tcpm_unregister;
	}

	ret = rk_tcpc_init_interrupt(chip);
	if (ret) {
		dev_err(dev, "cannot init interrupt, ret=%d", ret);
		goto tcpm_unregister;
	}

	return ret;

tcpm_unregister:
	tcpm_unregister_port(chip->tcpm_port);
fwnode_put:
	fwnode_handle_put(chip->tcpc_dev.fwnode);
debugfs_exit:
	rk_tcpc_debugfs_exit(chip);

	return ret;
}

static void rk_tcpc_remove(struct i2c_client *client)
{
	struct rk_tcpc_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->pm_work);
	tcpm_unregister_port(chip->tcpm_port);
	fwnode_handle_put(chip->tcpc_dev.fwnode);
	rk_tcpc_debugfs_exit(chip);
}

static int rk_tcpc_pm_suspend(struct device *dev)
{
	return rk_tcpc_set_lpmode(dev->driver_data);
}

static int rk_tcpc_pm_resume(struct device *dev)
{
	struct rk_tcpc_chip *chip = dev->driver_data;
	u8 reg;
	int ret = 0;

	/*
	 * When the power of TCPC is lost or i2c read failed in PM S/R
	 * process, we must reset the tcpm port first to ensure the devices
	 * can attach again.
	 *
	 * The RK_TCPC_CTRL3_TYPEC_EN we enabled it in tcpm_init, so if TCPC
	 * was powered off in suspend, the value would reset to 0.
	 */
	ret = rk_tcpc_read8(chip, RK_TCPC_CTRL3, &reg);
	if (!(reg & RK_TCPC_CTRL3_TYPEC_EN) || ret < 0)
		tcpm_tcpc_reset(chip->tcpm_port);

	return 0;
}

static const struct of_device_id rk_tcpc_dt_match[] = {
	{.compatible = "rockchip,rk817b2-tcpc"},
	{},
};
MODULE_DEVICE_TABLE(of, rk_tcpc_dt_match);

static const struct i2c_device_id rk_tcpc_i2c_device_id[] = {
	{"rk817b2-tcpc", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rk_tcpc_i2c_device_id);

static DEFINE_SIMPLE_DEV_PM_OPS(rk_tcpc_pm_ops,
	rk_tcpc_pm_suspend, rk_tcpc_pm_resume);

static struct i2c_driver rk_tcpc_driver = {
	.driver = {
		.name = "rockchip,pmic-tcpc",
		.pm = &rk_tcpc_pm_ops,
		.of_match_table = of_match_ptr(rk_tcpc_dt_match),
	},
	.probe = rk_tcpc_probe,
	.remove = rk_tcpc_remove,
	.id_table = rk_tcpc_i2c_device_id,
};
module_i2c_driver(rk_tcpc_driver);

MODULE_AUTHOR("Frank Wang <frank.wang@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip PMIC Type-C Port Controller Driver");
MODULE_LICENSE("GPL");
