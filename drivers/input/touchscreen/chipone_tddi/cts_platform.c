// SPDX-License-Identifier: GPL-2.0-or-later

#define LOG_TAG         "Plat"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_firmware.h"
#include "cts_sysfs.h"
#include "cts_tcs.h"

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
int tpd_rst_gpio_index = 0;
int tpd_int_gpio_index = 1;
static int tpd_history_x, tpd_history_y;
#endif

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_plat_set_reset(struct cts_platform_data *pdata, int val)
{
    cts_info("Set Reset to %s", val ? "HIGH" : "LOW");
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    if (val)
        tpd_gpio_output(tpd_rst_gpio_index, 1);
    else
        tpd_gpio_output(tpd_rst_gpio_index, 0);
#else
    if (val)
        gpio_set_value(pdata->rst_gpio, 1);
    else
        gpio_set_value(pdata->rst_gpio, 0);

#endif
    return 0;
}

int cts_plat_reset_device(struct cts_platform_data *pdata)
{
    /* !!!can not be modified */
    /* !!!can not be modified */
    /* !!!can not be modified */
    cts_plat_set_reset(pdata, 1);
    mdelay(1);
    cts_plat_set_reset(pdata, 0);
    mdelay(5);/* 1ms */
    cts_plat_set_reset(pdata, 1);
    /* under normal mode, delay over 50ms */
#ifdef CONFIG_CTS_I2C_HOST
    mdelay(120);
#else
    mdelay(70);
#endif
    return 0;
}
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
int cts_plat_set_cs(struct cts_platform_data *pdata, int val)
{
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    if (val)
        pinctrl_select_state(pdata->pinctrl1, pdata->spi_cs_high);
    else
        pinctrl_select_state(pdata->pinctrl1, pdata->spi_cs_low);
#else
    if (val)
        gpio_set_value(pdata->cs_gpio, 1);
    else
        gpio_set_value(pdata->cs_gpio, 0);
#endif
    return 0;
}
#endif /* CFG_CTS_MANUAL_CS */


#ifdef CONFIG_CTS_I2C_HOST
size_t cts_plat_get_max_i2c_xfer_size(struct cts_platform_data *pdata)
{
#ifdef TPD_SUPPORT_I2C_DMA
    if (pdata->i2c_dma_available) {
        return CFG_CTS_MAX_I2C_XFER_SIZE;
    } else {
        return CFG_CTS_MAX_I2C_FIFO_XFER_SIZE;
    }
#else
    return CFG_CTS_MAX_I2C_XFER_SIZE;
#endif
}

u8 *cts_plat_get_i2c_xfer_buf(struct cts_platform_data *pdata, size_t xfer_size)
{
#ifdef TPD_SUPPORT_I2C_DMA
    if (pdata->i2c_dma_available && xfer_size > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
        return pdata->i2c_dma_buff_va;
    } else
#endif /* TPD_SUPPORT_I2C_DMA */
        return pdata->i2c_fifo_buf;
}

int cts_plat_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
        const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;

#ifdef TPD_SUPPORT_I2C_DMA
    struct i2c_msg msg = {
        .addr    = i2c_addr,
        .flags    = !I2C_M_RD,
        .len    = len,
        .timing = 300,
    };

    if (pdata->i2c_dma_available && len > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
        msg.ext_flag = (pdata->i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
        msg.buf = (u8 *)pdata->i2c_dma_buff_pa;
        memcpy(pdata->i2c_dma_buff_va, src, len);
    } else {
        msg.buf = (u8 *)src;
    }
    msg.len  = len;
#else /* TPD_SUPPORT_I2C_DMA */
    struct i2c_msg msg = {
        .flags = !I2C_M_RD,
        .addr = i2c_addr,
        .buf = (u8 *) src,
        .len = len,
    };
#endif /* TPD_SUPPORT_I2C_DMA */

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter, &msg, 1);
        if (ret != 1) {
            if (ret >= 0) {
                ret = -EIO;
            }

            if (delay) {
                mdelay(delay);
            }
            continue;
        } else {
            return 0;
        }
    } while (++retries < retry);

    return ret;
}

int cts_plat_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int num_msg, ret = 0, retries = 0;

#ifdef TPD_SUPPORT_I2C_DMA
    struct i2c_msg msgs[2] = {
        {
            .addr    = i2c_addr,
            .flags    = !I2C_M_RD,
            .len    = wlen,
            .buf    = (u8 *)wbuf,
            .timing = 300,
        },
        {
            .addr      = i2c_addr,
            .flags      = I2C_M_RD,
            .len      = rlen,
            .timing   = 300,
        },
    };

    if (pdata->i2c_dma_available && rlen > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
        msgs[1].ext_flag = (pdata->i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
        msgs[1].buf  = (u8 *)pdata->i2c_dma_buff_pa;
    } else {
        msgs[1].buf = (u8 *)rbuf;
    }
#else /* TPD_SUPPORT_I2C_DMA */
    struct i2c_msg msgs[2] = {
        {
         .addr = i2c_addr,
         .flags = !I2C_M_RD,
         .buf = (u8 *) wbuf,
         .len = wlen },
        {
         .addr = i2c_addr,
         .flags = I2C_M_RD,
         .buf = (u8 *) rbuf,
         .len = rlen }
    };
#endif /* TPD_SUPPORT_I2C_DMA */

    if (wbuf == NULL || wlen == 0)
        num_msg = 1;
    else
        num_msg = 2;

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter,
                msgs + ARRAY_SIZE(msgs) - num_msg, num_msg);

        if (ret != num_msg) {
            if (ret >= 0)
                ret = -EIO;

            if (delay)
                mdelay(delay);
            continue;
        } else {
#ifdef TPD_SUPPORT_I2C_DMA
        if (pdata->i2c_dma_available && rlen > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
            memcpy(rbuf, pdata->i2c_dma_buff_va, rlen);
        }
#endif /* TPD_SUPPORT_I2C_DMA */

            return 0;
        }
    } while (++retries < retry);

    return ret;
}

#else /*CONFIG_CTS_I2C_HOST*/

#ifdef CFG_MTK_LEGEND_PLATFORM
struct mt_chip_conf cts_spi_conf_mt65xx = {
    .setuptime = 15,
    .holdtime = 15,
    .high_time = 21, //for mt6582, 104000khz/(4+4) = 130000khz
    .low_time = 21,
    .cs_idletime = 20,
    .ulthgh_thrsh = 0,

    .cpol = 0,
    .cpha = 0,

    .rx_mlsb = 1,
    .tx_mlsb = 1,

    .tx_endian = 0,
    .rx_endian = 0,

    .com_mod = FIFO_TRANSFER,
    .pause = 1,
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};

typedef enum {
    SPEED_500KHZ = 500,
    SPEED_1MHZ = 1000,
    SPEED_2MHZ = 2000,
    SPEED_3MHZ = 3000,
    SPEED_4MHZ = 4000,
    SPEED_6MHZ = 6000,
    SPEED_8MHZ = 8000,
    SPEED_KEEP,
    SPEED_UNSUPPORTED
} SPI_SPEED;

static int cts_plat_spi_set_mode(struct spi_device *spi, SPI_SPEED speed, int flag)
{
    struct mt_chip_conf *mcc = &cts_spi_conf_mt65xx;
    int ret;

    if (flag == 0) {
        mcc->com_mod = FIFO_TRANSFER;
    } else {
        mcc->com_mod = DMA_TRANSFER;
    }

    switch (speed) {
    case SPEED_500KHZ:
        mcc->high_time = 120;
        mcc->low_time = 120;
        break;
    case SPEED_1MHZ:
        mcc->high_time = 60;
        mcc->low_time = 60;
        break;
    case SPEED_2MHZ:
        mcc->high_time = 30;
        mcc->low_time = 30;
        break;
    case SPEED_3MHZ:
        mcc->high_time = 20;
        mcc->low_time = 20;
        break;
    case SPEED_4MHZ:
        mcc->high_time = 15;
        mcc->low_time = 15;
        break;
    case SPEED_6MHZ:
        mcc->high_time = 10;
        mcc->low_time = 10;
        break;
    case SPEED_8MHZ:
        mcc->high_time = 8;
        mcc->low_time = 8;
        break;
    case SPEED_KEEP:
    case SPEED_UNSUPPORTED:
        break;
    }

    ret = spi_setup(spi);
    if (ret) {
        cts_err("Spi setup failed %d(%s)", ret, cts_strerror(ret));
    }
    return ret;
}
#endif /* CFG_MTK_LEGEND_PLATFORM */

int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
    int ret;

    pdata->spi_client->chip_select = 0;
    pdata->spi_client->mode = SPI_MODE_0;
    pdata->spi_client->bits_per_word = 8;

    cts_info(" - chip_select  :%d", pdata->spi_client->chip_select);
    cts_info(" - spi_mode     :%d", pdata->spi_client->mode);
    cts_info(" - bits_per_word:%d", pdata->spi_client->bits_per_word);

#ifdef CFG_MTK_LEGEND_PLATFORM
    pdata->spi_client->controller_data = (void *)&cts_spi_conf_mt65xx;
    ret = spi_setup(pdata->spi_client);
    cts_plat_spi_set_mode(pdata->spi_client, pdata->spi_speed, 0);
#else /* CFG_MTK_LEGEND_PLATFORM */
    ret = spi_setup(pdata->spi_client);
#endif /* CFG_MTK_LEGEND_PLATFORM */
    if (ret) {
        cts_err("spi_setup err!");
    }
    return ret;
}

int cts_spi_send_recv(struct cts_platform_data *pdata, size_t len,
        u8 *tx_buffer, u8 *rx_buffer)
{
    struct chipone_ts_data *cts_data;
    struct spi_message msg;
    struct spi_transfer cmd = {
        .delay_usecs = 0,
        .speed_hz = pdata->spi_speed * 1000u,
        .tx_buf = tx_buffer,
        .rx_buf = rx_buffer,
        .len = len,
        /* .tx_dma = 0, */
        /* .rx_dma = 0, */
    };
    int ret = 0;

    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(pdata, 0);
#endif
    spi_message_init(&msg);
    spi_message_add_tail(&cmd, &msg);
    ret = spi_sync(cts_data->spi_client, &msg);
    if (ret)
        cts_err("spi sync failed %d", ret);

#ifdef CFG_CTS_MANUAL_CS
    udelay(100);
    cts_plat_set_cs(pdata, 1);
#endif
    return ret;
}

size_t cts_plat_get_max_spi_xfer_size(struct cts_platform_data *pdata)
{
    return CFG_CTS_MAX_SPI_XFER_SIZE;
}

u8 *cts_plat_get_spi_xfer_buf(struct cts_platform_data *pdata, size_t xfer_size)
{
    return pdata->spi_cache_buf;
}

int cts_plat_spi_write(struct cts_platform_data *pdata, u8 dev_addr,
        const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;
    u16 crc16_calc;
    size_t data_len;

    if (len > CFG_CTS_MAX_SPI_XFER_SIZE) {
        cts_err("write too much data:wlen=%zu", len);
        return -EIO;
    }

    if (pdata->cts_dev->rtdata.program_mode) {
        pdata->spi_tx_buf[0] = dev_addr;
        memcpy(&pdata->spi_tx_buf[1], src, len);
        do {
            ret = cts_spi_send_recv(pdata, len + 1, pdata->spi_tx_buf,
                    pdata->spi_rx_buf);
            if (ret) {
                cts_err("SPI write failed %d", ret);
                if (delay)
                    mdelay(delay);
            } else
                return 0;
        } while (++retries < retry);
    } else {
        data_len = len - 2;
        pdata->spi_tx_buf[0] = dev_addr;
        pdata->spi_tx_buf[1] = *((u8 *) src + 1);
        pdata->spi_tx_buf[2] = *((u8 *) src);
        put_unaligned_le16(data_len, &pdata->spi_tx_buf[3]);
        crc16_calc = (u16) cts_crc32(pdata->spi_tx_buf, 5);
        put_unaligned_le16(crc16_calc, &pdata->spi_tx_buf[5]);
        memcpy(&pdata->spi_tx_buf[7], (char *)src + 2, data_len);
        crc16_calc = (u16) cts_crc32((char *)src + 2, data_len);
        put_unaligned_le16(crc16_calc, &pdata->spi_tx_buf[7 + data_len]);
        do {
            ret = cts_spi_send_recv(pdata, len + 7, pdata->spi_tx_buf,
                    pdata->spi_rx_buf);
            udelay(10 * data_len);
            if (ret) {
                cts_err("SPI write failed %d", ret);
                if (delay)
                    mdelay(delay);
            } else
                return 0;
        } while (++retries < retry);
    }
    return ret;
}

int cts_plat_spi_read(struct cts_platform_data *pdata, u8 dev_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int ret = 0, retries = 0;
    u16 crc16_calc, crc16_recv;

    if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE
    || rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
        cts_err("write/read too much data:wlen=%zd, rlen=%zd", wlen, rlen);
        return -EIO;
    }

    if (pdata->cts_dev->rtdata.program_mode) {
        pdata->spi_tx_buf[0] = dev_addr | 0x01;
        memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
        do {
            ret = cts_spi_send_recv(pdata, rlen + 5, pdata->spi_tx_buf,
                    pdata->spi_rx_buf);
            if (ret) {
                cts_err("SPI read failed %d", ret);
                if (delay)
                    mdelay(delay);
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf + 5, rlen);
            return 0;
        } while (++retries < retry);
    } else {
        do {
            if (wlen != 0) {
                pdata->spi_tx_buf[0] = dev_addr | 0x01;
                pdata->spi_tx_buf[1] = wbuf[1];
                pdata->spi_tx_buf[2] = wbuf[0];
                put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
                crc16_calc = (u16) cts_crc32(pdata->spi_tx_buf, 5);
                put_unaligned_le16(crc16_calc, &pdata->spi_tx_buf[5]);
                ret = cts_spi_send_recv(pdata, 7, pdata->spi_tx_buf,
                        pdata->spi_rx_buf);
                if (ret) {
                    cts_err("SPI read failed %d", ret);
                    if (delay)
                        mdelay(delay);
                    continue;
                }
            }
            memset(pdata->spi_tx_buf, 0, 7);
            pdata->spi_tx_buf[0] = dev_addr | 0x01;
            udelay(100);
            ret = cts_spi_send_recv(pdata, rlen + 2,
                    pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                cts_err("SPI read failed %d", ret);
                if (delay)
                    mdelay(delay);
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf, rlen);
            crc16_calc = (u16) cts_crc32(pdata->spi_rx_buf, rlen);
            crc16_recv = get_unaligned_le16(&pdata->spi_rx_buf[rlen]);
            if (crc16_recv != crc16_calc) {
                cts_err("SPI RX CRC error: rx_crc %04x != %04x",
                        crc16_recv, crc16_calc);
                continue;
            }
            return 0;
        } while (++retries < retry);
    }
    if (retries >= retry)
        cts_err("SPI read too much retry");

    return -EIO;
}

int cts_plat_spi_read_delay_idle(struct cts_platform_data *pdata, u8 dev_addr,
        const u8 *wbuf, size_t wlen, void *rbuf,
        size_t rlen, int retry, int delay, int idle)
{
    int ret = 0, retries = 0;
    u16 crc;

    if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE
    || rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
        cts_err("write/read too much data:wlen=%zu, rlen=%zu", wlen, rlen);
        return -E2BIG;
    }

    if (pdata->cts_dev->rtdata.program_mode) {
        pdata->spi_tx_buf[0] = dev_addr | 0x01;
        memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
        do {
            ret = cts_spi_send_recv(pdata, rlen + 5, pdata->spi_tx_buf,
                    pdata->spi_rx_buf);
            if (ret) {
                cts_err("SPI read failed %d", ret);
                if (delay)
                    mdelay(delay);
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf + 5, rlen);
            return 0;
        } while (++retries < retry);
    } else {
        do {
            if (wlen != 0) {
                pdata->spi_tx_buf[0] = dev_addr | 0x01;
                pdata->spi_tx_buf[1] = wbuf[1];
                pdata->spi_tx_buf[2] = wbuf[0];
                put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
                crc = (u16) cts_crc32(pdata->spi_tx_buf, 5);
                put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
                ret = cts_spi_send_recv(pdata, 7, pdata->spi_tx_buf,
                        pdata->spi_rx_buf);
                if (ret) {
                    cts_err("SPI read failed %d", ret);
                    if (delay)
                        mdelay(delay);
                    continue;
                }
            }
            memset(pdata->spi_tx_buf, 0, 7);
            pdata->spi_tx_buf[0] = dev_addr | 0x01;
            udelay(idle);
            ret = cts_spi_send_recv(pdata, rlen + 2,
                pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                if (delay)
                    mdelay(delay);
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf, rlen);
            crc = (u16) cts_crc32(pdata->spi_rx_buf, rlen);
            if (get_unaligned_le16(&pdata->spi_rx_buf[rlen]) != crc)
                continue;
            return 0;
        } while (++retries < retry);
    }
    if (retries >= retry)
        cts_err("cts_plat_spi_read error");

    return -EIO;
}
#endif /*CONFIG_CTS_I2C_HOST*/

int cts_plat_is_normal_mode(struct cts_platform_data *pdata)
{
    struct chipone_ts_data *cts_data;
    u16 fwid;
    int ret;

    cts_set_normal_addr(pdata->cts_dev);
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    ret = cts_tcs_get_fw_id(pdata->cts_dev, &fwid);
    if (ret || !cts_is_fwid_valid(fwid))
        return false;

    return true;
}

static void cts_plat_handle_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_dbg("Handle IRQ");

    cts_lock_device(pdata->cts_dev);
    ret = cts_irq_handler(pdata->cts_dev);
    if (ret)
        cts_err("Device handle IRQ failed %d", ret);
    cts_unlock_device(pdata->cts_dev);
}

static irqreturn_t cts_plat_irq_handler(int irq, void *dev_id)
{
    struct cts_platform_data *pdata;
#ifndef CONFIG_GENERIC_HARDIRQS
    struct chipone_ts_data *cts_data;
#endif

    cts_dbg("IRQ handler");

    pdata = (struct cts_platform_data *)dev_id;
    if (pdata == NULL) {
        cts_err("IRQ handler with NULL dev_id");
        return IRQ_NONE;
    }
#ifdef CONFIG_GENERIC_HARDIRQS
    cts_plat_handle_irq(pdata);
#else
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (queue_work(cts_data->workqueue, &pdata->ts_irq_work)) {
        cts_dbg("IRQ queue work");
        cts_plat_disable_irq(pdata);
    } else
        cts_warn("IRQ handler queue work failed as already on the queue");
#endif /* CONFIG_GENERIC_HARDIRQS */

    return IRQ_HANDLED;
}

#ifndef CONFIG_GENERIC_HARDIRQS
static void cts_plat_touch_dev_irq_work(struct work_struct *work)
{
    struct cts_platform_data *pdata =
        container_of(work, struct cts_platform_data, ts_irq_work);

    cts_dbg("IRQ work");

    cts_plat_handle_irq(pdata);

    cts_plat_enable_irq(pdata);
}
#endif /* CONFIG_GENERIC_HARDIRQS */


#ifdef CFG_CTS_FORCE_UP
static void cts_plat_touch_timeout_work(struct work_struct *work)
{
    struct cts_platform_data *pdata = container_of(work,
            struct cts_platform_data, touch_timeout.work);

    cts_warn("Touch event timeout work");

    cts_plat_release_all_touch(pdata);
}
#endif


#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_CTS_OF
static int cts_plat_parse_dt(struct cts_platform_data *pdata,
        struct device_node *dev_node)
{
    int ret = 0;

    cts_info("Parse device tree");

    pdata->int_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_INT_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->int_gpio)) {
        cts_err("Parse INT GPIO from dt failed %d", pdata->int_gpio);
        pdata->int_gpio = -1;
    }
    cts_info("  %-12s: %d", "int gpio", pdata->int_gpio);

    pdata->irq = gpio_to_irq(pdata->int_gpio);
    if (pdata->irq < 0) {
        cts_err("Parse irq failed %d", ret);
        return pdata->irq;
    }
    cts_info("  %-12s: %d", "irq num", pdata->irq);

#ifdef CFG_CTS_HAS_RESET_PIN
    pdata->rst_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_RST_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->rst_gpio)) {
        cts_err("Parse RST GPIO from dt failed %d", pdata->rst_gpio);
        pdata->rst_gpio = -1;
    }
    cts_info("  %-12s: %d", "rst gpio", pdata->rst_gpio);
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
    pdata->cs_gpio = of_get_named_gpio(dev_node, CFG_CTS_OF_CS_GPIO_NAME, 0);
    if (!gpio_is_valid(pdata->cs_gpio)) {
        cts_err("Parse CS GPIO from dt failed %d", pdata->cs_gpio);
        pdata->cs_gpio = -1;
    }
    cts_info("  %-12s: %d", "cs gpio", pdata->cs_gpio);
#endif

    ret = of_property_read_u32(dev_node, CFG_CTS_OF_X_RESOLUTION_NAME,
            &pdata->res_x);
    if (ret)
        cts_warn("Parse X resolution from dt failed %d", ret);

    cts_info("  %-12s: %d", "X resolution", pdata->res_x);

    ret = of_property_read_u32(dev_node, CFG_CTS_OF_Y_RESOLUTION_NAME,
            &pdata->res_y);
    if (ret)
        cts_warn("Parse Y resolution from dt failed %d", ret);

    cts_info("  %-12s: %d", "Y resolution", pdata->res_y);

    if (of_property_read_u32(dev_node, "chipone,def-build-id", &pdata->build_id)) {
        pdata->build_id = 0;
        cts_info("chipone,build_id undefined.");
    } else
        cts_info("chipone,build_id=0x%04X", pdata->build_id);

    if (of_property_read_u32(dev_node, "chipone,def-config-id", &pdata->config_id)) {
        pdata->config_id = 0;
        cts_info("chipone,config_id undefined.");
    } else
        cts_info("chipone,config_id=0x%04X", pdata->config_id);

#ifdef CFG_CTS_FW_UPDATE_SYS
    ret = of_property_read_string(dev_node, CFG_CTS_OF_PANEL_SUPPLIER,
            &pdata->panel_supplier);
    if (ret) {
        pdata->panel_supplier = NULL;
        cts_warn("read panel supplier failed, ret=%d", ret);
    } else
        cts_info("panel supplier=%s", (char *)pdata->panel_supplier);
#endif

    return 0;
}
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct i2c_client *i2c_client)
#else
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct spi_device *spi)
#endif
{
    struct input_dev *input_dev;
    struct input_dev *pen_dev;
    int ret = 0;

    cts_info("cts_init_platform_data Init");

#ifdef CONFIG_CTS_OF
    {
        struct device *dev;

#ifdef CONFIG_CTS_I2C_HOST
        dev = &i2c_client->dev;
#else
        dev = &spi->dev;
#endif /* CONFIG_CTS_I2C_HOST */
        ret = cts_plat_parse_dt(pdata, dev->of_node);
        if (ret) {
            cts_err("Parse dt failed %d", ret);
            return ret;
        }
    }
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
    pdata->i2c_client = i2c_client;
    pdata->i2c_client->irq = pdata->irq;
#else
    pdata->spi_client = spi;
    pdata->spi_client->irq = pdata->irq;
#endif /* CONFIG_CTS_I2C_HOST */

    mutex_init(&pdata->dev_lock);
    spin_lock_init(&pdata->irq_lock);

    input_dev = input_allocate_device();
    if (input_dev == NULL) {
        cts_err("Failed to allocate input device.");
        return -ENOMEM;
    }

    /** - Init input device */
    input_dev->name = CFG_CTS_DEVICE_NAME;
    input_dev->name = CFG_CTS_DEVICE_NAME;
#ifdef CONFIG_CTS_I2C_HOST
    input_dev->id.bustype = BUS_I2C;
    input_dev->dev.parent = &pdata->i2c_client->dev;
#else
    input_dev->id.bustype = BUS_SPI;
    input_dev->dev.parent = &pdata->spi_client->dev;
#endif /* CONFIG_CTS_I2C_HOST */
    input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#ifdef CFG_CTS_PALM_DETECT
    set_bit(CFG_CTS_PALM_EVENT, input_dev->keybit);
#endif

#ifdef CFG_CTS_SWAP_XY
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_x, 0, 0);
#else /* CFG_CTS_SWAP_XY */
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_x, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_y, 0, 0);
#endif /* CFG_CTS_SWAP_XY */

    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
            CFG_CTS_MAX_TOUCH_NUM * 2, 0, 0);

    input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

#ifdef CONFIG_CTS_SLOTPROTOCOL
    input_mt_init_slots(input_dev, CFG_CTS_MAX_TOUCH_NUM, 0);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
    __set_bit(EV_ABS, input_dev->evbit);
    input_set_drvdata(input_dev, pdata);
    ret = input_register_device(input_dev);
    if (ret) {
        cts_err("Failed to register input device");
        return ret;
    }
    pdata->ts_input_dev = input_dev;


    pen_dev = input_allocate_device();
    if (pen_dev == NULL) {
        cts_err("Failed to allocate pen device.");
        return -ENOMEM;
    }

#ifdef CONFIG_CTS_I2C_HOST
    pen_dev->id.bustype = BUS_I2C;
    pen_dev->dev.parent = &pdata->i2c_client->dev;
#else
    pen_dev->id.bustype = BUS_SPI;
    pen_dev->dev.parent = &pdata->spi_client->dev;
#endif /* CONFIG_CTS_I2C_HOST */
    pen_dev->name = "chipone-tddi,pen";
    pen_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    __set_bit(ABS_X, pen_dev->absbit);
    __set_bit(ABS_Y, pen_dev->absbit);
    __set_bit(BTN_STYLUS, pen_dev->keybit);
    __set_bit(BTN_STYLUS2, pen_dev->keybit);
    __set_bit(BTN_TOUCH, pen_dev->keybit);
    __set_bit(BTN_TOOL_PEN, pen_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
    input_set_abs_params(pen_dev, ABS_X, 0, pdata->res_x, 0, 0);
    input_set_abs_params(pen_dev, ABS_Y, 0, pdata->res_y, 0, 0);
    input_set_abs_params(pen_dev, ABS_PRESSURE, 0, 4096, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_X, -9000, 9000, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_Y, -9000, 9000, 0, 0);
    input_set_abs_params(pen_dev, ABS_Z, 0, 36000, 0, 0);

    ret = input_register_device(pen_dev);
    if (ret) {
        cts_err("Input pen device registration failed");
        input_free_device(pen_dev);
        pen_dev = NULL;
        return ret;
    }
    pdata->pen_input_dev = pen_dev;

#if !defined(CONFIG_GENERIC_HARDIRQS)
    INIT_WORK(&pdata->ts_irq_work, cts_plat_touch_dev_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_VIRTUALKEY
    {
        u8 vkey_keymap[CFG_CTS_NUM_VKEY] = CFG_CTS_VKEY_KEYCODES;

        memcpy(pdata->vkey_keycodes, vkey_keymap, sizeof(vkey_keymap));
        pdata->vkey_num = CFG_CTS_NUM_VKEY;
    }
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
    {
        u8 gesture_keymap[CFG_CTS_NUM_GESTURE][2] = CFG_CTS_GESTURE_KEYMAP;

        memcpy(pdata->gesture_keymap, gesture_keymap, sizeof(gesture_keymap));
        pdata->gesture_num = CFG_CTS_NUM_GESTURE;
    }
#endif /* CFG_CTS_GESTURE */

#ifdef CFG_CTS_FORCE_UP
    INIT_DELAYED_WORK(&pdata->touch_timeout, cts_plat_touch_timeout_work);
#endif

#ifndef CONFIG_CTS_I2C_HOST
    pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
    cts_plat_spi_setup(pdata);
#endif
    return 0;
}

int cts_deinit_platform_data(struct cts_platform_data *pdata)
{
    cts_info("De-Init platform_data");
    input_unregister_device(pdata->ts_input_dev);
    return 0;
}

int cts_plat_request_resource(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request resource");

    ret = gpio_request_one(pdata->int_gpio, GPIOF_IN,
            CFG_CTS_DEVICE_NAME "-int");
    if (ret) {
        cts_err("Request INT gpio (%d) failed %d", pdata->int_gpio, ret);
        goto err_out;
    }
#ifdef CFG_CTS_HAS_RESET_PIN
    ret = gpio_request_one(pdata->rst_gpio, GPIOF_OUT_INIT_HIGH,
            CFG_CTS_DEVICE_NAME "-rst");
    if (ret) {
        cts_err("Request RST gpio (%d) failed %d", pdata->rst_gpio, ret);
        goto err_free_int;
    }
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
    ret = gpio_request_one(pdata->cs_gpio, GPIOF_OUT_INIT_HIGH,
            CFG_CTS_DEVICE_NAME "-cs");
    if (ret) {
        cts_err("Request CS gpio (%d) failed %d", pdata->cs_gpio, ret);
        goto err_request_cs_gpio;
    }
#endif

    return 0;

#ifdef CFG_CTS_MANUAL_CS
err_request_cs_gpio:
#endif

#ifdef CONFIG_CTS_REGULATOR
err_free_rst:
#endif /* CONFIG_CTS_REGULATOR */
#ifdef CFG_CTS_HAS_RESET_PIN
    gpio_free(pdata->rst_gpio);
err_free_int:
#endif /* CFG_CTS_HAS_RESET_PIN */
    gpio_free(pdata->int_gpio);
err_out:
    return ret;
}

void cts_plat_free_resource(struct cts_platform_data *pdata)
{
    cts_info("Free resource");

    if (gpio_is_valid(pdata->int_gpio))
        gpio_free(pdata->int_gpio);

#ifdef CFG_CTS_HAS_RESET_PIN
    if (gpio_is_valid(pdata->rst_gpio))
        gpio_free(pdata->rst_gpio);

#endif /* CFG_CTS_HAS_RESET_PIN */
#ifdef CFG_CTS_MANUAL_CS
    if (gpio_is_valid(pdata->cs_gpio))
        gpio_free(pdata->cs_gpio);

#endif
}

#else /*CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED*/

#ifndef CONFIG_CTS_I2C_HOST
static int cts_plat_init_dts(struct cts_platform_data *pdata, struct device *device)
{
#ifdef CFG_CTS_MANUAL_CS
    struct device_node *node;

    pdata->pinctrl1 = devm_pinctrl_get(device);
    node = device->of_node;
    if (node) {
        pdata->spi_cs_low = pinctrl_lookup_state(pdata->pinctrl1, "spi_cs_low");
        if (IS_ERR(pdata->spi_cs_low)) {
            cts_err("Cannot find pinctrl spi cs high!\n");
            return -ENOENT;
        }
        pdata->spi_cs_high = pinctrl_lookup_state(pdata->pinctrl1, "spi_cs_high");
        if (IS_ERR(pdata->spi_cs_high)) {
            return -ENOENT;
        }
        return 0;
    }
    return -ENOENT;
#else
    return 0;
#endif
}
#endif /* CONFIG_CTS_I2C_HOST */

#ifdef CONFIG_CTS_I2C_HOST
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct i2c_client *i2c_client)
#else
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct spi_device *spi)
#endif
{
    struct device_node *node = NULL;
    u32 ints[2] = { 0, 0 };

    cts_info("cts_init_platform_data Init");

#ifdef CONFIG_CTS_OF
    {
        struct device *dev;

#ifdef CONFIG_CTS_I2C_HOST
        dev = &i2c_client->dev;
#else
        dev = &spi->dev;
#endif /* CONFIG_CTS_I2C_HOST */
    }
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
    pdata->i2c_client = i2c_client;
#else
    pdata->spi_client = spi;
#endif /* CONFIG_CTS_I2C_HOST */

    pdata->ts_input_dev = tpd->dev;

    spin_lock_init(&pdata->irq_lock);
    mutex_init(&pdata->dev_lock);

#if !defined(CONFIG_GENERIC_HARDIRQS)
    INIT_WORK(&pdata->ts_irq_work, cts_plat_touch_dev_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

    if ((node = of_find_matching_node(node, touch_of_match)) == NULL) {
        cts_err("Find touch eint node failed");
        return -ENODATA;
    }
    if (of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints)) == 0) {
        gpio_set_debounce(ints[0], ints[1]);
    } else {
        cts_info("Debounce time not found");
    }
    pdata->irq = irq_of_parse_and_map(node, 0);
    if (pdata->irq == 0) {
        cts_err("Parse irq in dts failed");
        return -ENODEV;
    }

#ifdef CONFIG_CTS_VIRTUALKEY
    pdata->vkey_num = tpd_dts_data.tpd_keycnt;
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
    {
        u8 gesture_keymap[CFG_CTS_NUM_GESTURE][2] = CFG_CTS_GESTURE_KEYMAP;
        memcpy(pdata->gesture_keymap, gesture_keymap, sizeof(gesture_keymap));
        pdata->gesture_num = CFG_CTS_NUM_GESTURE;
    }
#endif /* CFG_CTS_GESTURE */

#ifdef TPD_SUPPORT_I2C_DMA
        tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
        pdata->i2c_dma_buff_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev,
                CFG_CTS_MAX_I2C_XFER_SIZE, &pdata->i2c_dma_buff_pa, GFP_KERNEL);
        if (pdata->i2c_dma_buff_va == NULL) {
            cts_err("Allocate I2C DMA Buffer failed!");
            //return -ENOMEM;
        } else {
            pdata->i2c_dma_available = true;
        }
#endif /* TPD_SUPPORT_I2C_DMA */

#ifdef CFG_CTS_FORCE_UP
    INIT_DELAYED_WORK(&pdata->touch_timeout, cts_plat_touch_timeout_work);
#endif

#ifndef CONFIG_CTS_I2C_HOST
    cts_plat_init_dts(pdata, &spi->dev);
    pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
    cts_plat_spi_setup(pdata);
#endif
    return 0;
}

int cts_plat_request_resource(struct cts_platform_data *pdata)
{
    cts_info("Request resource");

    tpd_gpio_as_int(tpd_int_gpio_index);
    tpd_gpio_output(tpd_rst_gpio_index, 1);

    return 0;
}

void cts_plat_free_resource(struct cts_platform_data *pdata)
{
    cts_info("Free resource");

    /**
     * Note:
     *    If resource request without managed, should free all resource
     *    requested in cts_plat_request_resource().
     */
#ifdef TPD_SUPPORT_I2C_DMA
    if (pdata->i2c_dma_buff_va) {
        dma_free_coherent(&tpd->dev->dev, CFG_CTS_MAX_I2C_XFER_SIZE,
        pdata->i2c_dma_buff_va, pdata->i2c_dma_buff_pa);
        pdata->i2c_dma_buff_va = NULL;
        pdata->i2c_dma_buff_pa = 0;
    }
#endif /* TPD_SUPPORT_I2C_DMA */
}
#endif /*CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED*/


int cts_plat_request_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Request IRQ");

#ifdef CONFIG_GENERIC_HARDIRQS
    ret = request_threaded_irq(pdata->irq, NULL, cts_plat_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
    ret = request_irq(pdata->irq, cts_plat_irq_handler,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, CFG_CTS_DRIVER_NAME, pdata);
#endif /* CONFIG_GENERIC_HARDIRQS */
    if (ret) {
        cts_err("Request IRQ failed %d", ret);
        return ret;
    }

    cts_plat_disable_irq(pdata);

    return 0;
}

void cts_plat_free_irq(struct cts_platform_data *pdata)
{
    free_irq(pdata->irq, pdata);
}

int cts_plat_enable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Enable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (pdata->irq_is_disable) {/* && !cts_is_device_suspended(pdata->chip)) */
            cts_dbg("Real enable IRQ");
            enable_irq(pdata->irq);
            pdata->irq_is_disable = false;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

int cts_plat_disable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Disable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (!pdata->irq_is_disable) {
            cts_dbg("Real disable IRQ");
            disable_irq_nosync(pdata->irq);
            pdata->irq_is_disable = true;
        }
        spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

        return 0;
    }

    return -ENODEV;
}

int cts_plat_get_int_pin(struct cts_platform_data *pdata)
{
#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    return gpio_get_value(pdata->int_gpio);
#else
    cts_err("MTK platform can not get INT pin value");
    return -ENOTSUPP;
#endif
}

int cts_plat_init_touch_device(struct cts_platform_data *pdata)
{
    cts_info("Init touch device");

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    return input_mt_init_slots(pdata->ts_input_dev,
        tpd_dts_data.touch_max_num, INPUT_MT_DIRECT);
#endif
    return 0;
}

void cts_plat_deinit_touch_device(struct cts_platform_data *pdata)
{
    cts_info("De-init touch device");

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&pdata->ts_irq_work)) {
        cancel_work_sync(&pdata->ts_irq_work);
    }
#endif /* CONFIG_GENERIC_HARDIRQS */
}

#ifdef CFG_CTS_PALM_DETECT
void cts_report_palm_event(struct cts_platform_data *pdata)
{
    input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 1);
    input_sync(pdata->ts_input_dev);
    msleep(100);
    input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 0);
    input_sync(pdata->ts_input_dev);
}
#endif

int cts_plat_process_touch_msg(struct cts_platform_data *pdata,
        struct cts_device_touch_msg *msgs, int num)
{
    struct chipone_ts_data *cts_data;
    struct input_dev *input_dev = pdata->ts_input_dev;
    int i;
    int contact = 0;
#ifdef CONFIG_CTS_SLOTPROTOCOL
    static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
    unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
#endif

    cts_dbg("Process touch %d msgs", num);

    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (num == 0 || num > CFG_CTS_MAX_TOUCH_NUM)
        return 0;

    for (i = 0; i < num; i++) {
        u16 x, y;

        x = (msgs[i].xl) | (msgs[i].xh << 8);
        y = (msgs[i].yl) | (msgs[i].yh << 8);

        cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
            i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);
        if (msgs[i].event == CTS_DEVICE_TOUCH_EVENT_DOWN
        || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_MOVE
        || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_STAY) {
            if (msgs[i].id < CFG_CTS_MAX_TOUCH_NUM)
                finger_current[msgs[i].id] = 1;
        }
#ifdef CONFIG_CTS_SLOTPROTOCOL
        /* input_mt_slot(input_dev, msgs[i].id); */
        switch (msgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(x, y, x, y, msgs[i].id, 1);
            tpd_history_x = x;
            tpd_history_y = y;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(x, y, 1);
            }
#endif /* CONFIG_MTK_BOOT */
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_mt_slot(input_dev, msgs[i].id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_POSITION_X,x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y,y);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, msgs[i].id, 0);
            tpd_history_x = 0;
            tpd_history_y = 0;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(0, 0, 0);
            }
#endif /* CONFIG_MTK_BOOT */
            //input_report_key(input_dev, BTN_TOUCH, 0);
            //input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
            break;

        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
            break;
        }
#else /* CONFIG_CTS_SLOTPROTOCOL */
    /**
    * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
    * in addition to the ABS_MT events, the last SYN_MT_REPORT event
    * may be omitted. Otherwise, the last SYN_REPORT will be dropped
    * by the input core, resulting in no zero-contact event
    * reaching userland.
    */
        switch (msgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_key(input_dev, BTN_TOUCH, 1);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_mt_sync(input_dev);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
            break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
        if (finger_last[i] != 0 && finger_current[i] == 0) {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        }
        finger_last[i] = finger_current[i];
    }
    input_report_key(input_dev, BTN_TOUCH, contact > 0);
#else /* CONFIG_CTS_SLOTPROTOCOL */
    if (contact == 0) {
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    input_sync(input_dev);

#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_timeout)) {
            mod_delayed_work(cts_data->workqueue,
                    &pdata->touch_timeout, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                    &pdata->touch_timeout, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_timeout);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}

int cts_plat_process_stylus_msg(struct cts_platform_data *pdata,
        struct cts_device_stylus_msg *msgs, int num)
{
    struct chipone_ts_data *cts_data;
    struct input_dev *pen_dev = pdata->pen_input_dev;
    int i;
    int contact = 0;

    cts_dbg("Process stylus %d msgs", num);

    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (num == 0 || num > CFG_CTS_MAX_STYLUS_NUM)
        return 0;

    for (i = 0; i < num; i++) {
        u16 x, y, p;

        x = (msgs[i].tip_xl) | (msgs[i].tip_xh << 8);
        y = (msgs[i].tip_yl) | (msgs[i].tip_yh << 8);
        p = msgs[i].pressure_l | (msgs[i].pressure_h << 8);

        cts_dbg("  Process stylus msg[%d]: id[%u] ev=%u x=%u y=%u p=%u"
            " tx=%d ty=%d btn0=%d btn1=%d btn2=%d",
            i, msgs[i].id, msgs[i].event, x, y, p, msgs[i].tiltx, msgs[i].tilty,
            msgs[i].btn0, msgs[i].btn1, msgs[i].btn2);

        /* Report stylus button */
        input_report_key(pen_dev, BTN_STYLUS, msgs[i].btn0);
        input_report_key(pen_dev, BTN_STYLUS2, msgs[i].btn1);

        switch (msgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(pen_dev, ABS_X, x);
            input_report_abs(pen_dev, ABS_Y, y);
            input_report_abs(pen_dev, ABS_PRESSURE, p);
            input_report_abs(pen_dev, ABS_TILT_X, msgs[i].tiltx);
            input_report_abs(pen_dev, ABS_TILT_Y, msgs[i].tilty);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process stylus msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
            break;
        }
    }

    input_report_key(pen_dev, BTN_TOUCH, contact > 0);
    input_report_key(pen_dev, BTN_TOOL_PEN, contact > 0);
    input_sync(pen_dev);

#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_timeout)) {
            mod_delayed_work(cts_data->workqueue,
                    &pdata->touch_timeout, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                    &pdata->touch_timeout, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_timeout);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}

#ifdef CFG_CTS_FINGER_STYLUS_SUPPORTED
int cts_plat_process_touch_stylus(struct cts_platform_data *pdata,
        struct cts_device_touch_info *touch_info)
{
    struct chipone_ts_data *cts_data;
    struct input_dev *input_dev = pdata->ts_input_dev;
    struct input_dev *pen_dev = pdata->pen_input_dev;
    struct cts_device_touch_msg *msgs = touch_info->msgs;
    int touch_num = touch_info->num_msg;
    struct cts_device_stylus_msg *smsgs = touch_info->smsgs;
    int stylus_num = touch_info->stylus_num;
    int i;
    int contact = 0;
#ifdef CONFIG_CTS_SLOTPROTOCOL
    static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
    unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
#endif

    cts_dbg("Process touch %d msgs, stylus %d msgs", touch_num, stylus_num);

    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    if (touch_num == 0 || touch_num > CFG_CTS_MAX_TOUCH_NUM) {
        goto process_stylus;
    }

    for (i = 0; i < touch_num; i++) {
        u16 x, y;

        x = (msgs[i].xl) | (msgs[i].xh << 8);
        y = (msgs[i].yl) | (msgs[i].yh << 8);

        cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
            i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);
        if (msgs[i].event == CTS_DEVICE_TOUCH_EVENT_DOWN
        || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_MOVE
        || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_STAY) {
            if (msgs[i].id < CFG_CTS_MAX_TOUCH_NUM)
                finger_current[msgs[i].id] = 1;
        }
#ifdef CONFIG_CTS_SLOTPROTOCOL
        /* input_mt_slot(input_dev, msgs[i].id); */
        switch (msgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(x, y, x, y, msgs[i].id, 1);
            tpd_history_x = x;
            tpd_history_y = y;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(x, y, 1);
            }
#endif /* CONFIG_MTK_BOOT */
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_mt_slot(input_dev, msgs[i].id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
            TPD_DEBUG_SET_TIME;
            TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, msgs[i].id, 0);
            tpd_history_x = 0;
            tpd_history_y = 0;
#ifdef CONFIG_MTK_BOOT
            if (tpd_dts_data.use_tpd_button) {
                if (FACTORY_BOOT == get_boot_mode() ||
                    RECOVERY_BOOT == get_boot_mode())
                    tpd_button(0, 0, 0);
            }
#endif /* CONFIG_MTK_BOOT */
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
            break;
        }
#else /* CONFIG_CTS_SLOTPROTOCOL */
    /**
    * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
    * in addition to the ABS_MT events, the last SYN_MT_REPORT event
    * may be omitted. Otherwise, the last SYN_REPORT will be dropped
    * by the input core, resulting in no zero-contact event
    * reaching userland.
    */
        switch (msgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
            input_report_key(input_dev, BTN_TOUCH, 1);
            input_report_abs(input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
            input_mt_sync(input_dev);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
            break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
        if (finger_last[i] != 0 && finger_current[i] == 0) {
            input_mt_slot(input_dev, i);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        }
        finger_last[i] = finger_current[i];
    }
    input_report_key(input_dev, BTN_TOUCH, contact > 0);
#else
    if (contact == 0) {
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }
#endif
    input_sync(input_dev);

process_stylus:
    if (stylus_num == 0 || stylus_num > CFG_CTS_MAX_STYLUS_NUM) {
        return 0;
    }

    for (i = 0; i < stylus_num; i++) {
        u16 x, y, p;

        x = (smsgs[i].tip_xl) | (smsgs[i].tip_xh << 8);
        y = (smsgs[i].tip_yl) | (smsgs[i].tip_yh << 8);
        p = smsgs[i].pressure_l | (smsgs[i].pressure_h << 8);

        cts_dbg("  Process stylus msg[%d]: id[%u] ev=%u x=%u y=%u p=%u"
            " tx=%d ty=%d btn0=%d btn1=%d btn2=%d",
            i, smsgs[i].id, smsgs[i].event, x, y, p, smsgs[i].tiltx, smsgs[i].tilty,
            smsgs[i].btn0, smsgs[i].btn1, smsgs[i].btn2);

        /* Report stylus button */
        input_report_key(pen_dev, BTN_STYLUS, smsgs[i].btn0);
        input_report_key(pen_dev, BTN_STYLUS2, smsgs[i].btn1);

        switch (smsgs[i].event) {
        case CTS_DEVICE_TOUCH_EVENT_DOWN:
        case CTS_DEVICE_TOUCH_EVENT_MOVE:
        case CTS_DEVICE_TOUCH_EVENT_STAY:
            contact++;
            input_report_abs(pen_dev, ABS_X, x);
            input_report_abs(pen_dev, ABS_Y, y);
            input_report_abs(pen_dev, ABS_PRESSURE, p);
            input_report_abs(pen_dev, ABS_TILT_X, smsgs[i].tiltx);
            input_report_abs(pen_dev, ABS_TILT_Y, smsgs[i].tilty);
            break;

        case CTS_DEVICE_TOUCH_EVENT_UP:
            break;
        default:
            cts_warn("Process stylus msg with unknwon event %u id %u",
                    smsgs[i].event, smsgs[i].id);
            break;
        }
    }

    input_report_key(pen_dev, BTN_TOUCH, contact > 0);
    input_report_key(pen_dev, BTN_TOOL_PEN, contact > 0);
    input_sync(pen_dev);


#ifdef CFG_CTS_FORCE_UP
    if (contact) {
        if (delayed_work_pending(&pdata->touch_timeout)) {
            mod_delayed_work(cts_data->workqueue,
                    &pdata->touch_timeout, msecs_to_jiffies(100));
        } else {
            queue_delayed_work(cts_data->workqueue,
                    &pdata->touch_timeout, msecs_to_jiffies(100));
        }
    } else {
        cancel_delayed_work_sync(&pdata->touch_timeout);
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    if (contact) {
        if (delayed_work_pending(&cts_data->heart_work)) {
            mod_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        } else {
            queue_delayed_work(cts_data->heart_workqueue,
                    &cts_data->heart_work, msecs_to_jiffies(2000));
        }
    }
#endif

    return 0;
}
#endif

int cts_plat_release_all_touch(struct cts_platform_data *pdata)
{
    struct input_dev *input_dev = pdata->ts_input_dev;
    struct input_dev *pen_dev = pdata->pen_input_dev;

#if defined(CONFIG_CTS_SLOTPROTOCOL)
    int id;
#endif /* CONFIG_CTS_SLOTPROTOCOL */

    cts_info("Release all touch");

#ifdef CONFIG_CTS_SLOTPROTOCOL
    for (id = 0; id < CFG_CTS_MAX_TOUCH_NUM; id++) {
        input_mt_slot(input_dev, id);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
    input_report_key(input_dev, BTN_TOUCH, 0);
#else
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_mt_sync(input_dev);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    input_sync(input_dev);

    input_report_key(pen_dev, BTN_TOUCH,  0);
    input_report_key(pen_dev, BTN_TOOL_PEN, 0);
    input_sync(pen_dev);

    return 0;
}

#ifdef CONFIG_CTS_VIRTUALKEY
int cts_plat_init_vkey_device(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init VKey");
    pdata->vkey_state = 0;

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    for (i = 0; i < pdata->vkey_num; i++) {
        input_set_capability(pdata->ts_input_dev,
                EV_KEY, pdata->vkey_keycodes[i]);
    }
#else
    if (tpd_dts_data.use_tpd_button) {
        cts_info("Init vkey");

        pdata->vkey_state = 0;
        tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
                tpd_dts_data.tpd_key_dim_local);
    }
#endif
    return 0;
}

void cts_plat_deinit_vkey_device(struct cts_platform_data *pdata)
{
    cts_info("De-init VKey");

    pdata->vkey_state = 0;
}

int cts_plat_process_vkey(struct cts_platform_data *pdata, u8 vkey_state)
{
    u8 event;
    int i;

    event = pdata->vkey_state ^ vkey_state;

    cts_dbg("Process vkey state=0x%02x, event=0x%02x", vkey_state, event);

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    for (i = 0; i < pdata->vkey_num; i++) {
        input_report_key(pdata->ts_input_dev, pdata->vkey_keycodes[i],
                vkey_state & BIT(i) ? 1 : 0);
    }
#else
    for (i = 0; i < pdata->vkey_num; i++) {
        if (event & BIT(i)) {
            tpd_button(x, y, vkey_state & BIT(i));

            /* MTK fobidon more than one key pressed in the same time */
            break;
        }
    }
#endif
    pdata->vkey_state = vkey_state;

    return 0;
}

int cts_plat_release_all_vkey(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Release all vkeys");

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    for (i = 0; i < pdata->vkey_num; i++) {
        if (pdata->vkey_state & BIT(i)) {
            input_report_key(pdata->ts_input_dev, pdata->vkey_keycodes[i], 0);
        }
    }
#else
    for (i = 0; i < pdata->vkey_num; i++) {
        if (pdata->vkey_state & BIT(i)) {
            tpd_button(x, y, 0);
        }
    }
#endif
    pdata->vkey_state = 0;

    return 0;
}
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
int cts_plat_enable_irq_wake(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Enable IRQ wake");

    if (pdata->irq > 0) {
        ret = enable_irq_wake(pdata->irq);
        if (ret < 0) {
            cts_err("Enable irq wake failed");
            return -EINVAL;
        }
        pdata->irq_wake_enabled = true;
        return 0;
    }

    cts_warn("Enable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_disable_irq_wake(struct cts_platform_data *pdata)
{
    int ret;

    cts_info("Disable IRQ wake");

    if (pdata->irq > 0) {
        ret = disable_irq_wake(pdata->irq);
        if (ret < 0) {
            cts_warn("Disable irq wake while already disabled");
            return -EINVAL;
        }
        pdata->irq_wake_enabled = false;
        return 0;
    }

    cts_warn("Disable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_init_gesture(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init gesture");

    /* TODO: If system will issure enable/disable command, comment following line. */
    /* cts_enable_gesture_wakeup(pdata->cts_dev); */

    for (i = 0; i < pdata->gesture_num; i++) {
        input_set_capability(pdata->ts_input_dev, EV_KEY,
                pdata->gesture_keymap[i][1]);
    }

    return 0;
}

void cts_plat_deinit_gesture(struct cts_platform_data *pdata)
{
    cts_info("De-init gesture");
}

int cts_plat_process_gesture_info(struct cts_platform_data *pdata,
        struct cts_device_gesture_info *gesture_info)
{
    int i;

    cts_info("Process gesture, id=0x%02x", gesture_info->gesture_id);

#if defined(CFG_CTS_GESTURE_REPORT_KEY)
    for (i = 0; i < CFG_CTS_NUM_GESTURE; i++) {
        if (gesture_info->gesture_id == pdata->gesture_keymap[i][0]) {
            if (gesture_info->gesture_id == GESTURE_D_TAP) {
                if (!pdata->cts_dev->rtdata.gesture_d_tap_enabled) {
                    cts_info("Ingore double tap");
                    return 0;
                }
            }

            cts_info("Report key[%u]", pdata->gesture_keymap[i][1]);
            input_report_key(pdata->ts_input_dev, pdata->gesture_keymap[i][1], 1);
            input_sync(pdata->ts_input_dev);

            input_report_key(pdata->ts_input_dev, pdata->gesture_keymap[i][1], 0);
            input_sync(pdata->ts_input_dev);

            return 0;
        }
    }
#endif /* CFG_CTS_GESTURE_REPORT_KEY */

    cts_warn("Process unrecognized gesture id=%u", gesture_info->gesture_id);

    return -EINVAL;
}

#endif /* CFG_CTS_GESTURE */
