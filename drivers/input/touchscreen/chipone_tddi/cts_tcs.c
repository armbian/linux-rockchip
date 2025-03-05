// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/kernel.h>

#define LOG_TAG                    "TCS"

#include "cts_config.h"
#include "cts_firmware.h"
#include "cts_platform.h"
#include "cts_tcs.h"

#define TCS_WR_ADDR                         (0xF0)
#define TCS_RD_ADDR                         (0xF1)

#define TCS_READ_BIT                        (14)
#define TCS_WRITE_BIT                       (13)
#define TCS_HEADER_SIZE                     sizeof(STRUCT_TCS_TX_HEAD)
#define TCS_TAIL_SIZE                       sizeof(STRUCT_TCS_RX_TAIL)

/* raw touch info without data */
#define TOUCH_INFO_SIZE                     sizeof(struct cts_device_touch_info)
#define INT_DATA_VALID_INDEX                (174)
#define INT_DATA_HEADER_SIZE                (180)

#define CTS_I2C_MAX_TRANS_SIZE              (48)

#pragma pack(1)
#ifdef CONFIG_CTS_I2C_HOST
typedef struct {
    u16 cmd;
    u16 datlen;
    u8 check_l;
    u8 check_h;
} STRUCT_TCS_TX_HEAD;

typedef struct {
    u8 ecode;
    u16 cmd;
    u8 check_l;
    u8 check_h;
} STRUCT_TCS_RX_TAIL;
#else /* CONFIG_CTS_I2C_HOST */
typedef struct {
    u8 addr;
    u16 cmd;
    u16 datlen;
    u16 crc16;
} STRUCT_TCS_TX_HEAD;
typedef struct {
    u8 ecode;
    u16 cmd;
    u16 crc16;
} STRUCT_TCS_RX_TAIL;
#endif /* CONFIG_CTS_I2C_HOST */
#pragma pack()

#define CMD_INFO_CHIP_HW_ID_RO                     (0x4002)
#define CMD_INFO_CHIP_FW_ID_RO                     (0x4003)
#define CMD_INFO_FW_VER_RO                         (0x4005)
#define CMD_INFO_TOUCH_XY_INFO_RO                  (0x4007)
#define CMD_INFO_IC_INFO_RO                        (0x4009)
#define CMD_INFO_PAD_PANEL_INFO_RO                 (0x400a)
#define CMD_INFO_MODULE_ID_RO                      (0x4011)
#define CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW         (0x6101)
#define CMD_TP_DATA_READ_START_RO                  (0x4102)
#define CMD_TP_DATA_COORDINATES_RO                 (0x4103)
#define CMD_TP_DATA_RAW_RO                         (0x4104)
#define CMD_TP_DATA_DIFF_RO                        (0x4105)
#define CMD_TP_DATA_BASE_RO                        (0x4106)
#define CMD_TP_DATA_CNEG_RO                        (0x410a)

#define CMD_TP_DATA_WR_REG_RAM_SEQUENCE_WO         (0x2114)
#define CMD_TP_DATA_WR_REG_RAM_BATCH_WO            (0x2115)
#define CMD_TP_DATA_WR_DDI_REG_SEQUENCE_WO         (0x2116)
#define CMD_TP_STD_CMD_SET_KRANG_STOP              (0x224a)
#define CMD_GET_DATA_BY_POLLING_RO                 (0x4123)
#define CMD_TP_DATA_STATUS_RO                      (0x4125)
#define CMD_SYS_STS_READ_RO                        (0x4200)
#define CMD_SYS_STS_WORK_MODE_RW                   (0x6201)
#define CMD_SYS_STS_DAT_RDY_FLAG_RW                (0x6203)
#define CMD_SYS_STS_PWR_STATE_RW                   (0x6204)
#define CMD_SYS_STS_CHARGER_PLUGIN_RW              (0x6205)
#define CMD_SYS_STS_DDI_CODE_VER_RO                (0x4206)
#define CMD_SYS_STS_DAT_TRANS_IN_NORMAL_RW         (0x6207)
#define CMD_SYS_STS_VSTIM_LVL_RW                   (0x6208)
#define CMD_SYS_STS_CNEG_RDY_FLAG_RW               (0x6211)
#define CMD_SYS_STS_EP_PLUGIN_RW                   (0x6213)
#define CMD_SYS_STS_RESET_WO                       (0x2216)
#define CMD_SYS_STS_INT_TEST_EN_RW                 (0x6217)
#define CMD_SYS_STS_SET_INT_PIN_RW                 (0x6218)
#define CMD_SYS_STS_CNEG_RD_EN_RW                  (0x6219)
#define CMD_SYS_STS_HI_SENSE_EN_RW                 (0x621a)
#define CMD_SYS_STS_INT_MODE_RW                    (0x6223)
#define CMD_SYS_STS_INT_KEEP_TIME_RW               (0x6224)
#define CMD_SYS_STS_CURRENT_WORKMODE_RO            (0x4233)
#define CMD_SYS_STS_DATA_CAPTURE_SUPPORT_RO        (0x423f)
#define CMD_SYS_STS_DATA_CAPTURE_EN_RW             (0x6240)
#define CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW       (0x6241)
#define CMD_SYS_STS_PANEL_DIRECTION_RW             (0x6242)
#define CMD_SYS_STS_KRANG_WORK_MODE_RW             (0x6243)
#define CMD_SYS_STS_KRANG_CURRENT_WORKMODE_RO      (0x4244)
#define CMD_SYS_STS_GAME_MODE_RW                   (0x624e)
#define CMD_SYS_STS_POCKET_MODE_EN_RW              (0x6250)
#define CMD_SYS_STS_PRODUCTION_TEST_EN_RW          (0x6252)
#define CMD_SYS_STS_KRANG_WORK_STS_RO              (0x4259)

#define CMD_GSTR_WAKEUP_EN_RW                      (0x6301)
#define CMD_GSTR_DAT_RDY_FLAG_GSTR_RW              (0x631e)
#define CMD_GSTR_ENTER_MAP_RW                      (0x6328)
#define CMD_MNT_EN_RW                              (0x6401)
#define CMD_MNT_FORCE_EXIT_MNT_WO                  (0x2403)
#define CMD_DDI_ESD_EN_RW                          (0x6501)
#define CMD_DDI_ESD_OPTIONS_RW                     (0x6502)
#define CMD_CNEG_EN_RW                             (0x6601)
#define CMD_CNEG_OPTIONS_RW                        (0x6602)
#define CMD_COORD_FLIP_X_EN_RW                     (0x6702)
#define CMD_COORD_FLIP_Y_EN_RW                     (0x6703)
#define CMD_COORD_SWAP_AXES_EN_RW                  (0x6704)
#define CMD_PARA_PROXI_EN_RW                       (0x692a)
#define CMD_PARA_KUNCKLE_RW                        (0x694b)
#define CMD_OPENSHORT_EN_RW                        (0x6b01)
#define CMD_OPENSHORT_MODE_SEL_RW                  (0x6b02)
#define CMD_OPENSHORT_SHORT_SEL_RW                 (0x6b03)
#define CMD_OPENSHORT_SHORT_DISP_ON_EN_RW          (0x6b04)

#if 0
static u8 str[1024 * 4];

void dump_spi(const char *prefix, u8 *data, size_t datalen)
{

    int offset = 0;
    int i;

    offset += snprintf(str + offset, sizeof(str) - offset, "%s", prefix);
    for (i = 0; i < datalen; i++) {
        offset += snprintf(str + offset, sizeof(str) - offset, " %02x", data[i]);
    }
    cts_err("%s", str);
}
#endif

#ifdef CONFIG_CTS_I2C_HOST
static int cts_tcs_tail_check(u8 *buf, u16 cmd, int len)
{
    STRUCT_TCS_RX_TAIL *tail;
    u8 check_l = 0;
    u8 check_h = 0x01;
    int check_len = len - TCS_TAIL_SIZE;
    int i;

    tail = (STRUCT_TCS_RX_TAIL *)(buf + check_len);
    if (tail->ecode != 0) {
        cts_err("error code:0x%02x", tail->ecode);
        return -EIO;
    }

    if (tail->cmd != cmd) {
        cts_err("cmd error: recv %04x != %04x send", tail->cmd, cmd);
        return -EIO;
    }

    for (i = 0; i < check_len + 3; i++) {
        check_l += buf[i];
    }
    check_l = ~check_l;
    if (tail->check_h != check_h || tail->check_l != check_l) {
        cts_err("crc error: recv %02x%02x != %02x%02x calc",
                tail->check_h, tail->check_l, check_h, check_l);
        return -EIO;
    }

    return 0;
}

static int cts_tcs_i2c_trans(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int i, retries = 0;
    int ret;

    struct i2c_msg msgs[2] = {
        {
            .addr = i2c_addr,
            .flags = 0,
            .buf = (u8 *) wbuf,
            .len = wlen,
        },
        {
            .addr = i2c_addr,
            .flags = I2C_M_RD,
            .buf = (u8 *) rbuf,
            .len = rlen,
        }
    };

    for (i = 0; i < 2; i++) {
        for (retries = 0; retries < retry; retries++) {
            ret = i2c_transfer(pdata->i2c_client->adapter, &msgs[i], 1);
            if (ret == 1) {
                ret = 0;

                if (delay > 0)
                    mdelay(delay);
                else
                    udelay(300);

                break;
            } else {
                mdelay(10);
            }
        }

        if (retries >= retry) {
            return -EIO;
        }
    }

    ret = cts_tcs_tail_check(rbuf, get_unaligned_le16(wbuf), rlen);

    return ret;
}

static int cts_tcs_i2c_trans_1_time(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int retries = 0;
    int ret;

    struct i2c_msg msgs[2] = {
        {
            .addr = i2c_addr,
            .flags = 0,
            .buf = (u8 *) wbuf,
            .len = wlen,
        },
        {
            .addr = i2c_addr,
            .flags = I2C_M_RD,
            .buf = (u8 *) rbuf,
            .len = rlen,
        }
    };

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter, msgs, 2);

        if (ret != 2) {
            if (ret >= 0) {
                ret = -EIO;
            }

            udelay(delay);

            continue;
        } else {
            return 0;
        }
    } while (++retries < retry);

    ret = cts_tcs_tail_check(rbuf, get_unaligned_le16(wbuf), rlen);

    return ret;
}

static int cts_tcs_i2c_read_pack(u8 *tx, u16 cmd, u16 rdatalen)
{
    STRUCT_TCS_TX_HEAD *txhdr = (STRUCT_TCS_TX_HEAD *) tx;
    u16 is_read;
    int packlen = 0;

    is_read = cmd & BIT(TCS_READ_BIT);
    if (0 == is_read) {
        return packlen;
    }
    txhdr->cmd = (cmd & ~BIT(TCS_WRITE_BIT));
    txhdr->datlen = rdatalen;
    txhdr->check_l = ~((txhdr->cmd & 0xff)
            + ((txhdr->cmd >> 8) & 0xff)
            + (rdatalen & 0xff)
            + ((rdatalen >> 8) & 0xff));
    txhdr->check_h = 1;
    packlen = TCS_HEADER_SIZE;

    return packlen;
}

static int cts_tcs_i2c_write_pack(u8 *tx, u16 cmd, u8 *wdata, u16 wlen)
{
    STRUCT_TCS_TX_HEAD *txhdr = (STRUCT_TCS_TX_HEAD *) tx;
    int packlen = 0;
    u8 check_l = 0;
    u16 is_write;
    int i;

    is_write = cmd & BIT(TCS_WRITE_BIT);
    if (0 == is_write) {
        return packlen;
    }
    txhdr->cmd = (cmd & ~BIT(TCS_READ_BIT));
    txhdr->datlen = wlen;
    txhdr->check_l = ~((txhdr->cmd & 0xff)
            + ((txhdr->cmd >> 8) & 0xff)
            + (wlen & 0xff)
            + ((wlen >> 8) & 0xff));
    txhdr->check_h = 1;
    packlen += TCS_HEADER_SIZE;

    if (wlen > 0) {
        memcpy(tx + TCS_HEADER_SIZE, wdata, wlen);
        for (i = 0; i < wlen; i++) {
            check_l += wdata[i];
        }
        *(tx + TCS_HEADER_SIZE + wlen) = ~check_l;
        *(tx + TCS_HEADER_SIZE + wlen + 1) = 1;
        packlen += wlen + 2;
    }

    return packlen;
}

static int cts_tcs_i2c_read(struct cts_device *cts_dev, u16 cmd,
        u8 *buf, size_t len)
{
    int txlen;
    int size;
    int ret;

    size = len + TCS_TAIL_SIZE;

    txlen = cts_tcs_i2c_read_pack(cts_dev->pdata->i2c_fifo_buf, cmd, len);

    ret = cts_tcs_i2c_trans(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR,
        cts_dev->pdata->i2c_fifo_buf, txlen, cts_dev->pdata->i2c_rbuf, size,
        3, 0);

    if (ret == 0) {
        memcpy(buf, cts_dev->pdata->i2c_rbuf, len);
    }

    return ret;
}

static int cts_tcs_i2c_write(struct cts_device *cts_dev,
        u16 cmd, u8 *wbuf, size_t wlen)
{
    int txlen = cts_tcs_i2c_write_pack(cts_dev->pdata->i2c_fifo_buf, cmd, wbuf, wlen);
    return cts_tcs_i2c_trans(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR,
        cts_dev->pdata->i2c_fifo_buf, txlen, cts_dev->pdata->i2c_rbuf, TCS_TAIL_SIZE,
        3, 0);
}

static int cts_tcs_i2c_set_krang_stop(struct cts_device *cts_dev,
        u16 cmd, u8 *wbuf, size_t wlen)
{
    int txlen = cts_tcs_i2c_write_pack(cts_dev->pdata->i2c_fifo_buf, cmd, wbuf, wlen);
    return cts_tcs_i2c_trans(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR,
        cts_dev->pdata->i2c_fifo_buf, txlen, cts_dev->pdata->i2c_rbuf, TCS_TAIL_SIZE,
        3, 50);
}

static int cts_tcs_i2c_read_touch(struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len)
{
    int txlen;
    int rxlen_without_tail = len - TCS_TAIL_SIZE;
    int ret;

    txlen = cts_tcs_i2c_read_pack(cts_dev->pdata->i2c_fifo_buf,
            cmd, len - TCS_TAIL_SIZE);
    ret = cts_tcs_i2c_trans_1_time(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR,
        cts_dev->pdata->i2c_fifo_buf, txlen, cts_dev->pdata->i2c_rbuf, len, 3, 10);

    if (ret == 0) {
        memcpy(buf, cts_dev->pdata->i2c_rbuf, rxlen_without_tail);
    }

    return ret;
}
#else
static int cts_tcs_spi_xtrans(struct cts_device *cts_dev, u8 *tx,
        size_t txlen, u8 *rx, size_t rxlen, int delay)
{
    struct chipone_ts_data *cts_data = container_of(cts_dev,
        struct chipone_ts_data, cts_dev);
    struct spi_transfer xfer[2];
    struct spi_message msg;
    u16 crc16_recv, crc16_calc;
    u16 cmd_recv, cmd_send;
    int ret;

    memset(&xfer[0], 0, sizeof(struct spi_transfer));
    xfer[0].delay_usecs = 0;
    xfer[0].speed_hz = cts_dev->pdata->spi_speed * 1000u;
    xfer[0].tx_buf = tx;
    xfer[0].rx_buf = NULL;
    xfer[0].len = txlen;

    spi_message_init(&msg);
    spi_message_add_tail(&xfer[0], &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 0);
#endif
    ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 1);
#endif
    if (ret < 0) {
        cts_err("spi_sync xfer[0] failed: %d", ret);
        return ret;
    }

    if (delay > 0)
        mdelay(delay);
    else
        udelay(500);

    memset(&xfer[1], 0, sizeof(struct spi_transfer));
    xfer[1].delay_usecs = 0;
    xfer[1].speed_hz = cts_dev->pdata->spi_speed * 1000u;
    xfer[1].tx_buf = NULL;
    xfer[1].rx_buf = rx;
    xfer[1].len = rxlen;

    spi_message_init(&msg);
    spi_message_add_tail(&xfer[1], &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 0);
#endif
    ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 1);
#endif
    if (ret < 0) {
        cts_err("spi_sync xfer[1] failed: %d", ret);
        return ret;
    }

    cmd_recv = get_unaligned_le16(rx +rxlen - 4);
    cmd_send = get_unaligned_le16(tx + 1);
    if (cmd_recv != cmd_send) {
        cts_dbg("cmd check error, send %04x != %04x recv", cmd_send, cmd_recv);
        // return -EIO;
    }

    crc16_recv = get_unaligned_le16(rx + rxlen - 2);
    crc16_calc = cts_crc16(rx, rxlen - 2);
    if (crc16_recv != crc16_calc) {
        cts_err("crc error: recv %04x != %04x calc", crc16_recv, crc16_calc);
        return -EIO;
    }
    udelay(100);

    return 0;
}

static int cts_tcs_spi_xtrans_1_cs(struct cts_device *cts_dev, u8 *tx,
        size_t txlen, u8 *rx, size_t rxlen)
{
    struct chipone_ts_data *cts_data = container_of(cts_dev,
        struct chipone_ts_data, cts_dev);
    struct spi_transfer xfer[1];
    struct spi_message msg;
    u16 crc16_recv, crc16_calc;
    u16 cmd_recv, cmd_send;
    int ret;

    memset(&xfer[0], 0, sizeof(struct spi_transfer));
    xfer[0].delay_usecs = 0;
    xfer[0].speed_hz = cts_dev->pdata->spi_speed * 1000u;
    xfer[0].tx_buf = tx;
    xfer[0].rx_buf = rx;
    xfer[0].len = txlen > rxlen ? txlen : rxlen;

    spi_message_init(&msg);
    spi_message_add_tail(&xfer[0], &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 0);
#endif
    ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(cts_dev->pdata, 1);
#endif
    if (ret < 0) {
        cts_err("spi_sync xfer[0] failed: %d", ret);
        return ret;
    }

    cmd_recv = get_unaligned_le16(rx + rxlen - 4);
    cmd_send = get_unaligned_le16(tx + 1);
    if (cmd_recv != cmd_send) {
        cts_dbg("cmd check error, send %04x != %04x recv", cmd_send, cmd_recv);
        // return -EIO;
    }

    crc16_recv = get_unaligned_le16(rx + rxlen - 2);
    crc16_calc = cts_crc16(rx, rxlen - 2);
    if (crc16_recv != crc16_calc) {
        cts_err("1cs crc error: recv %04x != %04x calc", crc16_recv, crc16_calc);
        return -EIO;
    }

    return 0;
}

static int cts_tcs_spi_read_pack(u8 *tx, u16 cmd, u16 rdatalen)
{
    STRUCT_TCS_TX_HEAD *txhdr = (STRUCT_TCS_TX_HEAD *) tx;
    u16 is_read;
    int packlen = 0;
    u16 crc16;

    is_read = cmd & BIT(TCS_READ_BIT);
    if (0 == is_read) {
        return packlen;
    }
    txhdr->addr = TCS_RD_ADDR;
    txhdr->cmd = (cmd & ~BIT(TCS_WRITE_BIT));
    txhdr->datlen = rdatalen;
    crc16 = cts_crc16((const u8 *)txhdr, offsetof(STRUCT_TCS_TX_HEAD, crc16));
    txhdr->crc16 = crc16;
    packlen += TCS_HEADER_SIZE;

    return packlen;
}

static int cts_tcs_spi_write_pack(u8 *tx, u16 cmd, u8 *wdata, u16 wdatalen)
{
    STRUCT_TCS_TX_HEAD *txhdr = (STRUCT_TCS_TX_HEAD *) tx;
    u16 is_write;
    int packlen = 0;
    u16 crc16;

    is_write = cmd & BIT(TCS_WRITE_BIT);
    if (0 == is_write) {
        return packlen;
    }
    txhdr->addr = TCS_WR_ADDR;
    txhdr->cmd = (cmd & ~BIT(TCS_READ_BIT));
    txhdr->datlen = wdatalen;
    crc16 = cts_crc16((const u8 *)txhdr, offsetof(STRUCT_TCS_TX_HEAD, crc16));
    txhdr->crc16 = crc16;
    packlen += TCS_HEADER_SIZE;

    if (wdatalen > 0) {
        memcpy(tx + TCS_HEADER_SIZE, wdata, wdatalen);
        crc16 = cts_crc16(wdata, wdatalen);
        *(tx + TCS_HEADER_SIZE + wdatalen) = ((crc16 >> 0) & 0xFF);
        *(tx + TCS_HEADER_SIZE + wdatalen + 1) = ((crc16 >> 8) & 0xFF);
        packlen += wdatalen + sizeof(crc16);
    }

    return packlen;
}

static int cts_tcs_spi_read(struct cts_device *cts_dev,
        u16 cmd, u8 *rdata, size_t rdatalen)
{
    int txlen;
    int ret;

    txlen = cts_tcs_spi_read_pack(cts_dev->pdata->spi_tx_buf, cmd, rdatalen);
    if (0 == txlen) {
        cts_err("spi read pack len: %d", txlen);
        return -EINVAL;
    }
    ret = cts_tcs_spi_xtrans(cts_dev, cts_dev->pdata->spi_tx_buf, txlen,
            cts_dev->pdata->spi_rx_buf, rdatalen + TCS_TAIL_SIZE, 0);
    if (ret) {
        return ret;
    }

    memcpy(rdata, cts_dev->pdata->spi_rx_buf, rdatalen);

    return ret;
}

static int cts_tcs_spi_read_1_cs(struct cts_device *cts_dev,
        u16 cmd, u8 *rdata, size_t rdatalen)
{
    int txlen;
    int ret;

    txlen = cts_tcs_spi_read_pack(cts_dev->pdata->spi_tx_buf, cmd, rdatalen);
    if (0 == txlen) {
        cts_err("spi read pack len: %d", txlen);
        return -EINVAL;
    }
    ret = cts_tcs_spi_xtrans_1_cs(cts_dev, cts_dev->pdata->spi_tx_buf, txlen,
            cts_dev->pdata->spi_rx_buf, rdatalen);
    if (ret) {
        return ret;
    }

    memcpy(rdata, cts_dev->pdata->spi_rx_buf, rdatalen);

    return ret;
}

static int cts_tcs_spi_write(struct cts_device *cts_dev,
        u16 cmd, u8 *data, size_t wlen)
{
    int txlen;
    int ret;

    txlen = cts_tcs_spi_write_pack(cts_dev->pdata->spi_tx_buf, cmd, data, wlen);
    if (0 == txlen) {
        cts_err("spi write pack len: %d", txlen);
        return -EINVAL;
    }
    ret = cts_tcs_spi_xtrans(cts_dev, cts_dev->pdata->spi_tx_buf, txlen,
            cts_dev->pdata->spi_rx_buf, TCS_TAIL_SIZE, 0);
    return ret;
}

static int cts_tcs_spi_set_krang_stop(struct cts_device *cts_dev,
        u16 cmd, u8 *data, size_t wlen)
{
    int txlen;
    int ret;

    txlen = cts_tcs_spi_write_pack(cts_dev->pdata->spi_tx_buf, cmd, data, wlen);
    if (0 == txlen) {
        cts_err("spi write pack len: %d", txlen);
        return -EINVAL;
    }
    ret = cts_tcs_spi_xtrans(cts_dev, cts_dev->pdata->spi_tx_buf, txlen,
            cts_dev->pdata->spi_rx_buf, TCS_TAIL_SIZE, 50);
    return ret;
}
#endif

int cts_tcs_tool_xtrans(struct cts_device *cts_dev, u8 *tx, size_t txlen,
        u8 *rx, size_t rxlen)
{
#ifdef CONFIG_CTS_I2C_HOST
    int ret;
    memcpy(cts_dev->pdata->i2c_fifo_buf, tx, txlen);

    ret = cts_tcs_i2c_trans(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR,
        cts_dev->pdata->i2c_fifo_buf, txlen, cts_dev->pdata->i2c_rbuf, rxlen,
        3, 1);
    if (ret == 0)
        memcpy(rx, cts_dev->pdata->i2c_rbuf, rxlen);

    return ret;
#else
    return cts_tcs_spi_xtrans(cts_dev, tx, txlen, rx, rxlen, 1);
#endif
}

int cts_tcs_read(struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_tcs_i2c_read(cts_dev, cmd, buf, len);
#else
    return cts_tcs_spi_read(cts_dev, cmd, buf, len);
#endif
}
int cts_tcs_write(struct cts_device *cts_dev,
        u16 cmd, u8 *buf, size_t len)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_tcs_i2c_write(cts_dev, cmd, buf, len);
#else
    return cts_tcs_spi_write(cts_dev, cmd, buf, len);
#endif
}

static int cts_tcs_set_krang_stop(struct cts_device *cts_dev)
{
    uint8_t stop = 1;
    int ret;

#ifdef CONFIG_CTS_I2C_HOST
    ret = cts_tcs_i2c_set_krang_stop(cts_dev, CMD_TP_STD_CMD_SET_KRANG_STOP,
            &stop, sizeof(stop));
#else
    ret = cts_tcs_spi_set_krang_stop(cts_dev, CMD_TP_STD_CMD_SET_KRANG_STOP,
            &stop, sizeof(stop));
#endif
    if (ret < 0) {
        cts_err("Set krang stop failed!");
    }

    return ret;
}

int cts_tcs_get_hwid_info(struct cts_device *cts_dev, u8 *info)
{
    u8 buf[12] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_CHIP_HW_ID_RO, buf, sizeof(buf));
    if (ret == 0) {
        memcpy(info, buf, sizeof(buf));
    }
    return ret;
}

int cts_tcs_get_fw_ver(struct cts_device *cts_dev, u16 *fwver)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_FW_VER_RO, buf, sizeof(buf));
    if (ret == 0) {
        *fwver = buf[0] | (buf[1] << 8);
    }
    return ret;
}

int cts_tcs_get_lib_ver(struct cts_device *cts_dev, u16 *libver)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_FW_VER_RO, buf, sizeof(buf));
    if (ret == 0) {
        *libver = buf[2] | (buf[3] << 8);
    }
    return ret;
}

int cts_tcs_get_fw_id(struct cts_device *cts_dev, u16 *fwid)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_CHIP_FW_ID_RO, buf, sizeof(buf));
    if (ret == 0) {
        *fwid = buf[0] | (buf[1] << 8);
    }

    return ret;
}

int cts_tcs_get_ddi_ver(struct cts_device *cts_dev, u8 *ddiver)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DDI_CODE_VER_RO, buf, sizeof(buf));
    if (ret == 0) {
        *ddiver = buf[0];
    }
    return ret;
}

int cts_tcs_get_res_x(struct cts_device *cts_dev, u16 *res_x)
{
    u8 buf[10] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_TOUCH_XY_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        *res_x = buf[0] | (buf[1] << 8);
    }
    return ret;
}

int cts_tcs_get_res_y(struct cts_device *cts_dev, u16 *res_y)
{
    u8 buf[10] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_TOUCH_XY_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        *res_y = buf[2] | (buf[3] << 8);
    }
    return ret;
}

int cts_tcs_get_rows(struct cts_device *cts_dev, u8 *rows)
{
    u8 buf[10] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_TOUCH_XY_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        *rows = buf[5];
    }
    return ret;
}

int cts_tcs_get_cols(struct cts_device *cts_dev, u8 *cols)
{
    u8 buf[10] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_TOUCH_XY_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        *cols = buf[4];
    }
    return ret;
}

int cts_tcs_get_rx_tx_info(struct cts_device *cts_dev, u8 *info)
{
    u8 buf[8] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_IC_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        memcpy(info, buf, sizeof(buf));
    }
    return ret;
}

int cts_tcs_get_panel_rx_tx_info(struct cts_device *cts_dev, u8 *info)
{
    u8 buf[15] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_PAD_PANEL_INFO_RO, buf, sizeof(buf));
    if (ret == 0) {
        memcpy(info, buf, sizeof(buf));
    }
    return ret;
}

int cts_tcs_get_flip_x(struct cts_device *cts_dev, bool *flip_x)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_COORD_FLIP_X_EN_RW, buf, sizeof(buf));
    if (ret == 0) {
        *flip_x = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_flip_y(struct cts_device *cts_dev, bool *flip_y)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_COORD_FLIP_Y_EN_RW, buf, sizeof(buf));
    if (ret == 0) {
        *flip_y = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_swap_axes(struct cts_device *cts_dev, bool *swap_axes)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_COORD_SWAP_AXES_EN_RW, buf, sizeof(buf));
    if (ret == 0) {
        *swap_axes = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_int_mode(struct cts_device *cts_dev, u8 *int_mode)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_INT_MODE_RW, buf, sizeof(buf));
    if (ret == 0) {
        *int_mode = buf[0];
    }
    return ret;
}

int cts_tcs_get_int_keep_time(struct cts_device *cts_dev,
        u16 *int_keep_time)
{
    u8 buf[2] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_INT_KEEP_TIME_RW, buf, sizeof(buf));
    if (ret == 0) {
        *int_keep_time = (buf[0] | (buf[1] << 8));
    }
    return ret;

}

int cts_tcs_get_rawdata_target(struct cts_device *cts_dev,
        u16 *rawdata_target)
{
    u8 buf[2] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_CNEG_OPTIONS_RW, buf, sizeof(buf));
    if (ret == 0) {
        *rawdata_target = (buf[0] | (buf[1] << 8));
    }
    return ret;

}

int cts_tcs_get_esd_method(struct cts_device *cts_dev, u8 *esd_method)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_DDI_ESD_OPTIONS_RW, buf, sizeof(buf));
    if (ret == 0) {
        *esd_method = buf[0];
    }
    return ret;
}

int cts_tcs_get_esd_protection(struct cts_device *cts_dev,
        u8 *esd_protection)
{
    u8 buf[4] = { 0 };
    int ret;

    buf[0] = 0x01;
    buf[1] = 0x56;
    buf[2] = 0x81;
    buf[3] = 0x00;

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
            buf, sizeof(buf));
    if (ret != 0)
        return ret;

    ret = cts_tcs_read(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
            esd_protection, sizeof(u8));

    return ret;
}

static int cts_tcs_get_data_ready_flag(struct cts_device *cts_dev, u8 *ready)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DAT_RDY_FLAG_RW, buf, sizeof(buf));
    if (ret == 0) {
        *ready = buf[0];
    }
    return ret;
}

int cts_tcs_clr_gstr_ready_flag(struct cts_device *cts_dev)
{
    u8 ready = 0;

    return cts_tcs_write(cts_dev, CMD_GSTR_DAT_RDY_FLAG_GSTR_RW,
            &ready, sizeof(ready));
}

static int cts_tcs_clr_data_ready_flag(struct cts_device *cts_dev)
{
    u8 ready = 0;

    return cts_tcs_write(cts_dev, CMD_SYS_STS_DAT_RDY_FLAG_RW,
            &ready, sizeof(ready));
}

int cts_tcs_read_hw_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size)
{
    u8 buf[4] = { 0 };
    int ret;

    buf[0] = 1;
    buf[1] = ((addr >> 0) & 0xFF);
    buf[2] = ((addr >> 8) & 0xFF);
    buf[3] = ((addr >> 16) & 0xFF);

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
            buf, sizeof(buf));
    if (ret != 0)
        return ret;

    ret = cts_tcs_read(cts_dev, CMD_TP_DATA_READ_START_RO, regbuf, size);

    return ret;
}

int cts_tcs_write_hw_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size)
{
    u8 *buf;
    int ret;

    buf = kmalloc(size + 6, GFP_KERNEL);
    if (buf == NULL)
        return -ENOMEM;

    buf[0] = ((size >> 0) & 0xFF);
    buf[1] = ((size >> 8) & 0xFF);
    buf[2] = ((addr >> 0) & 0xFF);
    buf[3] = ((addr >> 8) & 0xFF);
    buf[4] = ((addr >> 16) & 0xFF);
    buf[5] = 0x00;
    memcpy(buf + 6, regbuf, size);

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_WR_REG_RAM_SEQUENCE_WO,
            buf, size + 6);
    if (ret != 0) {
        kfree(buf);
        return ret;
    }

    kfree(buf);

    return ret;
}

int cts_tcs_read_ddi_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size)
{
    u8 buf[2] = { 0 };
    int ret;

    buf[0] = 2;
    buf[1] = addr;

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
        buf, sizeof(buf));
    if (ret != 0)
        return ret;

    ret = cts_tcs_read(cts_dev, CMD_TP_DATA_READ_START_RO, regbuf, size);
    if (ret != 0)
        return ret;

    return 0;
}

int cts_tcs_write_ddi_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size)
{
    u8 *buf;
    int ret;

    buf = kmalloc(size + 6, GFP_KERNEL);
    if (buf == NULL)
        return -ENOMEM;

    buf[0] = ((size >> 0) & 0xFF);
    buf[1] = ((size >> 8) & 0xFF);
    buf[2] = addr;
    buf[3] = 0;
    buf[4] = 0;
    buf[5] = 0;
    memcpy(buf + 6, regbuf, size);

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_WR_DDI_REG_SEQUENCE_WO,
            buf, size + 6);
    if (ret != 0) {
        kfree(buf);
        return ret;
    }

    kfree(buf);

    return ret;

}

int cts_tcs_read_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size)
{
    return cts_tcs_read(cts_dev, CMD_SYS_STS_READ_RO, regbuf, size);
}

int cts_tcs_write_reg(struct cts_device *cts_dev, u32 addr,
        u8 *regbuf, size_t size)
{
    u8 buf[4] = { 0 };
    int ret;

    buf[0] = 0x01;
    buf[1] = ((addr >> 0) & 0xFF);
    buf[2] = ((addr >> 8) & 0xFF);
    buf[3] = ((addr >> 16) & 0xFF);

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
        buf, sizeof(buf));
    if (ret != 0)
        return ret;

    ret = cts_tcs_write(cts_dev, CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
        regbuf, sizeof(buf));
    if (ret != 0)
        return ret;

    return ret;
}

int cts_tcs_calc_int_data_size(struct cts_device *cts_dev)
{
#define INT_DATA_TYPE_U8_SIZ        \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * sizeof(u8))
#define INT_DATA_TYPE_U16_SIZ        \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * sizeof(u16))

    u16 data_types = cts_dev->fwdata.int_data_types;
    bool is_stylus = !!(cts_dev->fwdata.int_data_types & BIT(14));
    u8 data_method = cts_dev->fwdata.int_data_method;

    u8 pen_freq_num = cts_dev->fwdata.pen_freq_num;
    u8 cascade_num = cts_dev->fwdata.cascade_num;
    u8 pc_cols = cts_dev->fwdata.pc_cols;
    u8 pc_rows = cts_dev->fwdata.pc_rows;
    u8 pr_cols = cts_dev->fwdata.pr_cols;
    u8 pr_rows = cts_dev->fwdata.pr_rows;

    cts_dev->fwdata.int_data_size = (TOUCH_INFO_SIZE + TCS_TAIL_SIZE);

    if (data_method == INT_DATA_METHOD_NONE) {
        return 0;
    } else if (data_method == INT_DATA_METHOD_DEBUG) {
        cts_dev->fwdata.int_data_size += INT_DATA_TYPE_U16_SIZ;
        return 0;
    }

    if (data_types != INT_DATA_TYPE_NONE) {
        cts_dev->fwdata.int_data_size = INT_DATA_HEADER_SIZE;
        if (is_stylus) {
            cts_dev->fwdata.int_data_size += (pr_rows * pr_cols + pc_rows * pc_cols)
                * pen_freq_num * sizeof(u16) * cascade_num;
        } else {
            if ((data_types & INT_DATA_TYPE_CNEGDATA)) {
                cts_dev->fwdata.int_data_size += INT_DATA_TYPE_U8_SIZ;
            } else {
                cts_dev->fwdata.int_data_size += INT_DATA_TYPE_U16_SIZ;
            }
        }

        cts_dev->fwdata.int_data_size += TCS_TAIL_SIZE;
    }

    cts_info("data_method:%d, data_type:%04x", data_method, data_types);
    cts_info("data_size:%d", cts_dev->fwdata.int_data_size);
    return 0;
}

int cts_tcs_polling_data(struct cts_device *cts_dev,
        u8 *buf, size_t size)
{
    int retries = 100;
    u8 ready = 0;
    int ret;

    size_t data_size = cts_dev->fwdata.int_data_size;

    do {
        ret = cts_tcs_get_data_ready_flag(cts_dev, &ready);
        if (!ret && ready)
            break;
        mdelay(10);
    } while (!ready && --retries);
    cts_info("get data rdy, retries left %d", retries);

    if (!ready) {
        cts_err("time out wait for data rdy");
        return -EIO;
    }

    retries = 3;
    do {
#ifdef CONFIG_CTS_I2C_HOST
        ret = cts_tcs_i2c_read_touch(cts_dev, CMD_GET_DATA_BY_POLLING_RO,
            cts_dev->int_data, data_size);
#else
        ret = cts_tcs_spi_read_1_cs(cts_dev, CMD_GET_DATA_BY_POLLING_RO,
            cts_dev->int_data, data_size);
#endif
        mdelay(1);
    } while (ret && --retries);

    if (cts_tcs_clr_data_ready_flag(cts_dev))
        cts_err("Clear data ready flag failed");

    return ret;
}

static void cts_rotate_data(struct cts_device *cts_dev,
        u8 *dst, u8 *src, enum int_data_type type)
{
    int rows = cts_dev->fwdata.rows;
    int cols = cts_dev->fwdata.cols;
    u16 *u16dst = (u16 *)dst;
    u16 *u16src = (u16 *)src;
    int i, j;

    if ((type & 0x3f) ==INT_DATA_TYPE_CNEGDATA) {
        for (i = 0; i < cols; i++) {
            for (j = 0; j < rows; j++) {
                *dst++ = src[j * cols + i];
            }
        }
    } else {
        for (i = 0; i < cols; i++) {
            for (j = 0; j < rows; j++) {
                *u16dst++ = u16src[j * cols + i];
            }
        }
    }
}

static int tool_polling_data(struct cts_device *cts_dev, u8 *buf,
        enum int_data_type type)
{
    u8 old_int_data_method;
    u16 old_int_data_types;
    int retries = 5;
    int ret;

    old_int_data_types = cts_dev->fwdata.int_data_types;
    old_int_data_method = cts_dev->fwdata.int_data_method;

    cts_set_int_data_types(cts_dev, type);
    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);

    while (retries--) {
        ret = cts_tcs_polling_data(cts_dev, buf, 0);
        if (!ret) {
            memcpy(buf, cts_dev->int_data, cts_dev->fwdata.int_data_size);
            break;
        }
    }

    cts_set_int_data_types(cts_dev, old_int_data_types);
    cts_set_int_data_method(cts_dev, old_int_data_method);

    return ret;
}

static int polling_data(struct cts_device *cts_dev, u8 *buf, size_t size,
        enum int_data_type type)
{
    u8 old_int_data_method;
    u16 old_int_data_types;
    int offset = INT_DATA_HEADER_SIZE;
    int retries = 5;
    int ret;

    old_int_data_types = cts_dev->fwdata.int_data_types;
    old_int_data_method = cts_dev->fwdata.int_data_method;

    cts_set_int_data_types(cts_dev, type);
    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);

    while (retries--) {
        ret = cts_tcs_polling_data(cts_dev, buf, size);
        if (!ret) {
            if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9951R) {
                cts_rotate_data(cts_dev, buf, cts_dev->int_data + offset, type);
            } else {
                memcpy(buf, cts_dev->int_data + offset, size);
            }
            break;
        }
    }

    cts_set_int_data_types(cts_dev, old_int_data_types);
    cts_set_int_data_method(cts_dev, old_int_data_method);

    return ret;
}


int cts_polling_test_data(struct cts_device *cts_dev,
        u8 *buf, size_t size, enum int_data_type type)
{
    int offset = INT_DATA_HEADER_SIZE;
    int retries = 5;
    int ret;

    while (retries--) {
        ret = cts_tcs_polling_data(cts_dev, buf, size);
        if (!ret) {
            if (cts_dev->int_data[INT_DATA_VALID_INDEX]) {
                memcpy(buf, cts_dev->int_data + offset, size);
                break;
            }
        }
    }

    return ret;
}

int cts_tcs_tool_get_rawdata(struct cts_device *cts_dev, u8 *buf, u16 data_source)
{
    return tool_polling_data(cts_dev, buf, data_source | INT_DATA_TYPE_RAWDATA);
}

int cts_tcs_tool_get_manual_diff(struct cts_device *cts_dev, u8 *buf, u16 data_source)
{
    return tool_polling_data(cts_dev, buf, data_source | INT_DATA_TYPE_MANUAL_DIFF);
}

int cts_tcs_tool_get_real_diff(struct cts_device *cts_dev, u8 *buf, u16 data_source)
{
    return tool_polling_data(cts_dev, buf, data_source | INT_DATA_TYPE_REAL_DIFF);
}

int cts_tcs_tool_get_basedata(struct cts_device *cts_dev, u8 *buf, u16 data_source)
{
    return tool_polling_data(cts_dev, buf, data_source | INT_DATA_TYPE_BASEDATA);
}


int cts_tcs_top_get_rawdata(struct cts_device *cts_dev, u8 *buf, size_t size,
    u16 data_source)
{
    return polling_data(cts_dev, buf, size, data_source | INT_DATA_TYPE_RAWDATA);
}

int cts_tcs_top_get_manual_diff(struct cts_device *cts_dev, u8 *buf, size_t size,
    u16 data_source)
{
    return polling_data(cts_dev, buf, size, data_source | INT_DATA_TYPE_MANUAL_DIFF);
}

int cts_tcs_top_get_real_diff(struct cts_device *cts_dev, u8 *buf, size_t size,
    u16 data_source)
{
    return polling_data(cts_dev, buf, size, data_source | INT_DATA_TYPE_REAL_DIFF);
}

int cts_tcs_top_get_noise_diff(struct cts_device *cts_dev, u8 *buf, size_t size,
    u16 data_source)
{
    return polling_data(cts_dev, buf, size, data_source | INT_DATA_TYPE_NOISE_DIFF);
}

int cts_tcs_top_get_basedata(struct cts_device *cts_dev, u8 *buf, size_t size,
    u16 data_source)
{
    return polling_data(cts_dev, buf, size, data_source | INT_DATA_TYPE_BASEDATA);
}

int cts_tcs_top_get_cnegdata(struct cts_device *cts_dev, u8 *buf, size_t size,
    u16 data_source)
{
    return polling_data(cts_dev, buf, size, data_source | INT_DATA_TYPE_CNEGDATA);
}

int cts_tcs_reset_device(struct cts_device *cts_dev)
{
#ifdef CONFIG_CTS_ICTYPE_ICNL9922
    u8 buf[2] = { 0x01, 0xfe };
    int ret;

    cts_info("ICNL9922 use software reset");
    /* normal */
    cts_info("tp reset in normal mode");
    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_RESET_WO,
            buf, sizeof(buf));
    if (!ret) {
        mdelay(40);
        return 0;
    }
    /* program */
    cts_info("tp reset in program mode");
    ret = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_RESET_CONFIG, 0xfd);
    if (!ret) {
        mdelay(40);
        return 0;
    }
    return ret;
#else
    return cts_plat_reset_device(cts_dev->pdata);
#endif
}

int cts_tcs_set_int_test(struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_INT_TEST_EN_RW, &enable,
            sizeof(enable));
}

int cts_tcs_set_int_pin(struct cts_device *cts_dev, u8 high)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_SET_INT_PIN_RW, &high,
            sizeof(high));
}

int cts_tcs_get_module_id(struct cts_device *cts_dev, u32 *modId)
{
    u8 buf[4] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_INFO_MODULE_ID_RO, buf, sizeof(buf));
    if (ret == 0) {
        *modId = *(u32 *)buf;
    }
    return ret;
}


int cts_tcs_get_gestureinfo(struct cts_device *cts_dev,
        struct cts_device_gesture_info *gesture_info)
{
    size_t size = sizeof(*gesture_info) + TCS_TAIL_SIZE;
    int ret;

#ifdef CONFIG_CTS_I2C_HOST
    ret = cts_tcs_i2c_read_touch(cts_dev, CMD_TP_DATA_COORDINATES_RO,
            cts_dev->int_data, size);
#else
    ret = cts_tcs_spi_read_1_cs(cts_dev, CMD_TP_DATA_COORDINATES_RO,
            cts_dev->int_data, size);
#endif
    if (cts_tcs_clr_gstr_ready_flag(cts_dev)) {
        cts_err("Clear gesture ready flag failed");
    }
    if (ret < 0) {
        cts_err("Get gesture info failed: ret=%d", ret);
        return ret;
    }

    memcpy(gesture_info, cts_dev->int_data, sizeof(*gesture_info));

    return ret;
}

int cts_tcs_get_touchinfo(struct cts_device *cts_dev,
        struct cts_device_touch_info *touch_info)
{
    size_t size = cts_dev->fwdata.int_data_size;
    int ret;

    memset(touch_info, 0, sizeof(*touch_info));

#ifdef CONFIG_CTS_I2C_HOST
    ret = cts_tcs_i2c_read_touch(cts_dev, CMD_TP_DATA_COORDINATES_RO,
            cts_dev->int_data, size);
    if (unlikely(ret != 0)) {
        cts_err("cts_tcs_i2c_read_touch failed");
        return ret;
    }
#else
    ret = cts_tcs_spi_read_1_cs(cts_dev, CMD_TP_DATA_COORDINATES_RO,
            cts_dev->int_data, size);
    if (unlikely(ret != 0)) {
        cts_err("tcs_spi_read_1_cs failed");
        return ret;
    }
#endif
    memcpy(touch_info, cts_dev->int_data, sizeof(*touch_info));

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (unlikely((touch_info->debug_msg.reset_flag & 0xFFFFFF) != 0xFFFFFF
    || (touch_info->debug_msg.reset_flag & 0xFFFF) != 0xFFFF)) {
        cts_err("reset flag:0x%08x error", touch_info->debug_msg.reset_flag);
        cts_show_touch_debug_msg(&touch_info->debug_msg);
    }
#endif

    return ret;
}

int cts_tcs_get_touch_status(struct cts_device *cts_dev)
{
    return  cts_tcs_read(cts_dev, CMD_TP_DATA_STATUS_RO,
            cts_dev->int_data, TOUCH_INFO_SIZE);
}

int cts_tcs_get_workmode(struct cts_device *cts_dev, u8 *workmode)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_CURRENT_WORKMODE_RO,
            &buf, sizeof(buf));
    if (ret == 0) {
        *workmode = buf;
    }

    return ret;
}

int cts_tcs_set_workmode(struct cts_device *cts_dev, u8 workmode)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_WORK_MODE_RW,
            &workmode, sizeof(workmode));
}

int cts_tcs_set_openshort_mode(struct cts_device *cts_dev, u8 mode)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_MODE_SEL_RW, &mode,
            sizeof(mode));
}

int cts_tcs_get_curr_mode(struct cts_device *cts_dev, u8 *currmode)
{
    u8 buf = 0;
    int ret;
    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_CURRENT_WORKMODE_RO,
        &buf, sizeof(buf));
    if (ret == 0) {
        *currmode = buf;
    }

    return ret;
}

int cts_tcs_get_krang_current_workmode(struct cts_device *cts_dev, u8 *workmode)
{
    u8 buf = 0;
    int ret = 0;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_KRANG_CURRENT_WORKMODE_RO,
            &buf, sizeof(buf));
    if (ret == 0) {
        *workmode = buf;
    }

    return ret;
}

int cts_tcs_set_krang_workmode(struct cts_device *cts_dev, u8 workmode)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_KRANG_WORK_MODE_RW, &workmode,
            sizeof(workmode));
}

/*Tab A9 code for AX6739A-953 by suyurui at 20230609 end*/
int cts_tcs_set_tx_vol(struct cts_device *cts_dev, u8 txvol)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_VSTIM_LVL_RW, &txvol,
            sizeof(txvol));
}

int cts_tcs_set_short_test_type(struct cts_device *cts_dev,
        u8 short_type)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_SHORT_SEL_RW,
            &short_type, sizeof(short_type));
}

int cts_tcs_is_openshort_enabled(struct cts_device *cts_dev, u8 *enabled)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_write(cts_dev, CMD_OPENSHORT_EN_RW, &buf, sizeof(buf));
    if (ret == 0) {
        *enabled = buf;
    }

    return ret;
}

int cts_tcs_set_openshort_enable(struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_set_esd_enable(struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_DDI_ESD_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_is_cneg_enabled(struct cts_device *cts_dev, u8 *enabled)
{
    return cts_tcs_read(cts_dev, CMD_CNEG_EN_RW, enabled, sizeof(*enabled));
}

int cts_tcs_is_mnt_enabled(struct cts_device *cts_dev, u8 *enabled)
{
    return cts_tcs_read(cts_dev, CMD_MNT_EN_RW, enabled, sizeof(*enabled));
}

int cts_tcs_set_cneg_enable(struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_CNEG_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_set_mnt_enable(struct cts_device *cts_dev, u8 enable)
{
    return cts_tcs_write(cts_dev, CMD_MNT_EN_RW, &enable, sizeof(enable));
}

int cts_tcs_is_display_on(struct cts_device *cts_dev, u8 *display_on)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_OPENSHORT_SHORT_DISP_ON_EN_RW,
            &buf, sizeof(buf));
    if (ret == 0) {
        *display_on = buf;
    }

    return ret;
}

int cts_tcs_set_pwr_mode(struct cts_device *cts_dev, u8 pwr_mode)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_PWR_STATE_RW,
            &pwr_mode, sizeof(pwr_mode));
}

int cts_tcs_set_display_on(struct cts_device *cts_dev, u8 display_on)
{
    return cts_tcs_write(cts_dev, CMD_OPENSHORT_SHORT_DISP_ON_EN_RW,
            &display_on, sizeof(display_on));
}


int cts_tcs_set_charger_plug(struct cts_device *cts_dev, u8 set)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set charger enable:%d", set);
    status->charger = set;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_CHARGER_PLUGIN_RW, &set, 1);
    if (ret < 0) {
        cts_info("Set charger failed!");
    }

    return ret;
}

int cts_tcs_get_charger_plug(struct cts_device *cts_dev, u8 *isset)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_CHARGER_PLUGIN_RW,&buf, sizeof(buf));
    if (ret == 0) {
        *isset = buf;
    }

    return ret;
}

int cts_tcs_set_earjack_plug(struct cts_device *cts_dev, u8 set)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set earjack enable:%d", set);

    status->earjack = set;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_EP_PLUGIN_RW, &set, 1);
    if (ret) {
        cts_info("Set earjack failed!");
    }

    return ret;
}

int cts_tcs_get_earjack_plug(struct cts_device *cts_dev, u8 *isset)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_EP_PLUGIN_RW, &buf, sizeof(buf));
    if (ret == 0) {
        *isset = buf;
    }

    return ret;
}

int cts_tcs_set_panel_direction(struct cts_device *cts_dev, u8 direction)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_PANEL_DIRECTION_RW,
            &direction, sizeof(direction));
}

int cts_tcs_get_panel_direction(struct cts_device *cts_dev, u8 *direction)
{
    u8 buf = 0;
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_PANEL_DIRECTION_RW,
            &buf, sizeof(buf));
    if (ret == 0) {
        *direction = buf;
    }

    return ret;
}

int cts_tcs_set_game_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set game enable:%d", enable);

    status->game = enable;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_GAME_MODE_RW,
            &enable, sizeof(enable));
    if (ret) {
        cts_err("Set game failed!");
    }

    return ret;
}

int cts_tcs_get_game_mode(struct cts_device *cts_dev, u8 *enabled)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_GAME_MODE_RW, buf, sizeof(buf));
    if (ret == 0) {
        *enabled = buf[0];
    }

    return ret;
}

int cts_tcs_get_has_int_data(struct cts_device *cts_dev, bool *has)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DATA_CAPTURE_SUPPORT_RO,
            buf, sizeof(buf));
    if (ret == 0) {
        *has = !!buf[0];
    }
    return ret;
}

int cts_tcs_get_int_data_types(struct cts_device *cts_dev, u16 *type)
{
    u8 buf[2] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW,
            buf, sizeof(buf));
    if (ret == 0) {
        *type = buf[0] | (buf[1] << 8);
    }
    return ret;
}

int cts_tcs_set_int_data_types(struct cts_device *cts_dev, u16 type)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW,
            (u8 *) &type, sizeof(type));
}

int cts_tcs_get_int_data_method(struct cts_device *cts_dev, u8 *method)
{
    u8 buf[1] = { 0 };
    int ret;

    ret = cts_tcs_read(cts_dev, CMD_SYS_STS_DATA_CAPTURE_EN_RW,
            buf, sizeof(buf));
    if (ret == 0) {
        *method = buf[0];
    }
    return ret;
}

int cts_tcs_set_int_data_method(struct cts_device *cts_dev, u8 method)
{
    return cts_tcs_write(cts_dev, CMD_SYS_STS_DATA_CAPTURE_EN_RW,
            &method, sizeof(method));
}

int cts_tcs_set_proximity_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set proximity enable:%d", enable);
    status->proximity = enable;

    ret = cts_tcs_write(cts_dev, CMD_PARA_PROXI_EN_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set proximity failed!");
    }

    return ret;
}

static int cts_tcs_set_knuckle_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set knuckle enable:%d", enable);
    status->knuckle = enable;

    ret = cts_tcs_write(cts_dev, CMD_PARA_KUNCKLE_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set knuckle failed!");
    }

    return ret;
}

int cts_tcs_set_glove_mode(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set glove enable:%d", enable);
    status->glove = enable;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_HI_SENSE_EN_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set glove failed!");
    }

    return ret;
}

static int cts_tcs_set_pocket_enable(struct cts_device *cts_dev, u8 enable)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    int ret;

    cts_info("Set pocket enable:%d", enable);
    status->pocket = enable;

    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_POCKET_MODE_EN_RW, &enable, 1);
    if (ret != 0) {
        cts_err("Set pocket failed!");
    }

    return ret;
}

void cts_tcs_reinit_fw_status(struct cts_device *cts_dev)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    cts_tcs_set_charger_plug(cts_dev, status->charger);
    cts_tcs_set_proximity_mode(cts_dev, status->proximity);
    cts_tcs_set_earjack_plug(cts_dev, status->earjack);
    cts_tcs_set_knuckle_mode(cts_dev, status->knuckle);
    cts_tcs_set_glove_mode(cts_dev, status->glove);
    cts_tcs_set_pocket_enable(cts_dev, status->pocket);
    cts_tcs_set_game_mode(cts_dev, status->game);
}


int cts_tcs_set_product_en(struct cts_device *cts_dev, u8 enable)
{
    int ret;
    ret = cts_tcs_write(cts_dev, CMD_SYS_STS_PRODUCTION_TEST_EN_RW,
            &enable, sizeof(enable));
    if (ret) {
        cts_err("Set product_en failed!");
    }

    return ret;
}

int cts_tcs_wait_krang_stop(struct cts_device *cts_dev)
{
    uint8_t status = 1;
    int i;
    int ret;

    for (i = 0; i < 3; i++) {
        ret = cts_tcs_set_krang_stop(cts_dev);
        if (ret < 0) {
            cts_err("Set krang stop failed!");
            mdelay(1);
            continue;
        }

        ret = cts_tcs_read(cts_dev, CMD_SYS_STS_KRANG_WORK_STS_RO, &status, sizeof(status));
        if (ret < 0) {
            cts_err("Read krang stop status failed!");
            mdelay(1);
            continue;
        }

        if (0 == status) {
            cts_info("krang flag was already stopped!");
            return 0;
        } else {
            cts_err("Read krang stop status:%d != 0", status);
            mdelay(1);
            continue;
        }
    }

    return -EBUSY;
}
