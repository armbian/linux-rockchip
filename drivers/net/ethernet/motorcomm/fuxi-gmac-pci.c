/*++

Copyright (c) 2021 Motor-comm Corporation. 
Confidential and Proprietary. All rights reserved.

This is Motor-comm Corporation NIC driver relevant files. Please don't copy, modify,
distribute without commercial permission.

--*/


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>

/* for file operation */
#include <linux/fs.h>

#include "fuxi-gmac.h"
#include "fuxi-gmac-reg.h"

#define FXGMAC_DBG         0
/* for power state debug using, 20210629. */
#if FXGMAC_DBG
static struct file *dbg_file = NULL;
#define FXGMAC_DBG_BUF_SIZE     128
static char log_buf[FXGMAC_DBG_BUF_SIZE + 2];
#endif

#if FXGMAC_DBG
static char * file_name = "/home/fxgmac/fxgmac_dbg.log";
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,13,0))
#if FXGMAC_DBG
#define fxgmac_dbg_log(logstr, arg...)  \
        do { \
                static struct kstat dbg_stat; \
                static loff_t pos = 0; \
                if (!(IS_ERR(dbg_file))) { \
                        mm_segment_t previous_fs; /*for address-space switch */ \
                        sprintf (&log_buf[0], (logstr), ##arg); \
                        previous_fs = get_fs(); \
                        set_fs(KERNEL_DS); \
                        vfs_stat(file_name, &dbg_stat); \
                        pos = (loff_t)dbg_stat.size; \
                        kernel_write(dbg_file, (const char*)log_buf, strlen(log_buf) > FXGMAC_DBG_BUF_SIZE ? FXGMAC_DBG_BUF_SIZE : strlen(log_buf), /*&dbg_file->f_pos*/&pos)/**/; \
                        /*DPRINTK("kernel write done, file size=%d, s=%s", (int)dbg_stat.size, log_buf)*/; \
                        set_fs(previous_fs); \
                        /*dbg_file->f_op->flush(dbg_file)*/; \
                        } \
        }while(0)
#else
#define fxgmac_dbg_log(logstr, arg...)  \
        do { \
            /*nothing*/; \
        }while(0)
#endif

#else
#if FXGMAC_DBG
/* in kernel 5.14, get_fs/set_fs are removed. */
#define fxgmac_dbg_log(logstr, arg...)  \
        do { \
                static struct kstat dbg_stat; \
                static loff_t pos = 0; \
                fxgmac_dbg_log_init(); \
                if (!(IS_ERR(dbg_file))) { \
                        sprintf (&log_buf[0], (logstr), ##arg); \
                        vfs_getattr(&(dbg_file->f_path), &dbg_stat, 0, 0); \
                        pos = (loff_t)dbg_stat.size; \
                        kernel_write(dbg_file, (const char*)log_buf, strlen(log_buf) > FXGMAC_DBG_BUF_SIZE ? FXGMAC_DBG_BUF_SIZE : strlen(log_buf), /*&dbg_file->f_pos*/&pos)/**/; \
                        /*DPRINTK("kernel write done, file size=%d, s=%s", (int)dbg_stat.size, log_buf)*/; \
                        /*dbg_file->f_op->flush(dbg_file)*/; \
                        } \
               fxgmac_dbg_log_uninit();\
        }while(0)

#else
#define fxgmac_dbg_log(logstr, arg...)  \
        do { \
            /*nothing*/; \
        }while(0)
#endif
#endif

/* declarations */
static void fxgmac_shutdown(struct pci_dev *pdev);

/*
 * functions definitions
 */
int fxgmac_dbg_log_init( void )
{
#if FXGMAC_DBG
    dbg_file = filp_open(file_name, O_RDWR | O_CREAT, 0644);
    if (IS_ERR(dbg_file))
    {
        DPRINTK("File fxgmac_dbg.log open or created failed.\n");
        return PTR_ERR(dbg_file);
    }
#endif

    return 0;
}

int fxgmac_dbg_log_uninit( void )
{
#if FXGMAC_DBG
    if(IS_ERR(dbg_file)) 
    {
        DPRINTK("Invalid fd for file fxgmac_dbg.log, %p.\n", dbg_file);
        dbg_file= NULL;
        return PTR_ERR(dbg_file);
    }

    filp_close(dbg_file, NULL);
    dbg_file= NULL;
#endif

    return 0;
}

static int fxgmac_probe(struct pci_dev *pcidev, const struct pci_device_id *id)
{
    struct device *dev = &pcidev->dev;
    struct fxgmac_resources res;
    int i, ret;

    ret = pcim_enable_device(pcidev);
    if (ret) {
        dev_err(dev, "ERROR: fxgmac_probe failed to enable device\n");
        return ret;
    }

    for (i = 0; i <= PCI_STD_RESOURCE_END; i++) {
        if (pci_resource_len(pcidev, i) == 0)
            continue;

        ret = pcim_iomap_regions(pcidev, BIT(i), FXGMAC_DRV_NAME);
        if (ret)
        {
            DPRINTK(KERN_INFO "fxgmac_probe pcim_iomap_regions failed\n");
            return ret;
        }
        DPRINTK(KERN_INFO "fxgmac_probe iomap_region i=%#x,ret=%#x\n",(int)i,(int)ret);
        break;
    }

    pci_set_master(pcidev);

    memset(&res, 0, sizeof(res));
    res.irq = pcidev->irq;
    res.addr = pcim_iomap_table(pcidev)[i];

    //fxgmac_dbg_log_init();
    fxgmac_dbg_log("fxpm,_fxgmac_probe\n");

    return fxgmac_drv_probe(&pcidev->dev, &res);
}

static void fxgmac_remove(struct pci_dev *pcidev)
{
    struct net_device *netdev = dev_get_drvdata(&pcidev->dev);
    struct fxgmac_pdata * pdata = netdev_priv(netdev);

#ifdef CONFIG_PCI_MSI
    u32 msix = FXGMAC_GET_REG_BITS(pdata->expansion.int_flags,
                                    FXGMAC_FLAG_MSIX_POS,
                                    FXGMAC_FLAG_MSIX_LEN);
#endif

    fxgmac_drv_remove(&pcidev->dev);
#ifdef CONFIG_PCI_MSI
    if(msix){
        pci_disable_msix(pcidev);
        kfree(pdata->expansion.msix_entries);
        pdata->expansion.msix_entries = NULL;
    }
#endif

    fxgmac_dbg_log("fxpm,_fxgmac_remove\n");

#ifdef HAVE_FXGMAC_DEBUG_FS
    fxgmac_dbg_exit(pdata);
#endif /* HAVE_FXGMAC_DEBUG_FS */

    //fxgmac_dbg_log_uninit();
}

/* for Power management, 20210628 */
static int __fxgmac_shutdown(struct pci_dev *pdev, bool *enable_wake)
{
    struct net_device *netdev = dev_get_drvdata(&pdev->dev);
    struct fxgmac_pdata *pdata = netdev_priv(netdev);
    u32 wufc = pdata->expansion.wol;
#ifdef CONFIG_PM
    int retval = 0;
#endif

    DPRINTK("fxpm,_fxgmac_shutdown, callin\n");
    fxgmac_dbg_log("fxpm,_fxgmac_shutdown, callin.\n");

    rtnl_lock();

    /* for linux shutdown, we just treat it as power off wol can be ignored
     * for suspend, we do need recovery by wol
     */
    fxgmac_net_powerdown(pdata, (unsigned int)!!wufc);
    netif_device_detach(netdev);
    rtnl_unlock();

#ifdef CONFIG_PM
    retval = pci_save_state(pdev);
    if (retval) {
        DPRINTK("fxpm,_fxgmac_shutdown, save pci state failed.\n");
        return retval;
    }
#endif

    DPRINTK("fxpm,_fxgmac_shutdown, save pci state done.\n");
    fxgmac_dbg_log("fxpm,_fxgmac_shutdown, save pci state done.\n");

    pci_wake_from_d3(pdev, !!wufc);
    *enable_wake = !!wufc;

    pci_disable_device(pdev);

    DPRINTK("fxpm,_fxgmac_shutdown callout, enable wake=%d.\n", *enable_wake);
    fxgmac_dbg_log("fxpm,_fxgmac_shutdown callout, enable wake=%d.\n", *enable_wake);

    return 0;
}

static void fxgmac_shutdown(struct pci_dev *pdev)
{
    bool wake;
    struct net_device *netdev = dev_get_drvdata(&pdev->dev);
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    DPRINTK("fxpm, fxgmac_shutdown callin\n");
    fxgmac_dbg_log("fxpm, fxgmac_shutdown callin\n");

    pdata->expansion.current_state = CURRENT_STATE_SHUTDOWN;
    __fxgmac_shutdown(pdev, &wake);

    if (system_state == SYSTEM_POWER_OFF) {
        pci_wake_from_d3(pdev, wake);
        pci_set_power_state(pdev, PCI_D3hot);
    }
    DPRINTK("fxpm, fxgmac_shutdown callout, system power off=%d\n", (system_state == SYSTEM_POWER_OFF)? 1 : 0);
    fxgmac_dbg_log("fxpm, fxgmac_shutdown callout, system power off=%d\n", (system_state == SYSTEM_POWER_OFF)? 1 : 0);
}

#ifdef CONFIG_PM
/* yzhang, 20210628 for PM */
static int fxgmac_suspend(struct pci_dev *pdev,
            pm_message_t __always_unused state)
{
    int retval;
    bool wake;
    struct net_device *netdev = dev_get_drvdata(&pdev->dev);
    struct fxgmac_pdata *pdata = netdev_priv(netdev);

    DPRINTK("fxpm, fxgmac_suspend callin\n");
    fxgmac_dbg_log("fxpm, fxgmac_suspend callin\n");

    pdata->expansion.current_state = CURRENT_STATE_SUSPEND;

    if (netif_running(netdev)) {
        retval = __fxgmac_shutdown(pdev, &wake);
        if (retval)
            return retval;
    } else {
        wake = !!(pdata->expansion.wol);
    }

    if (wake) {
        pci_prepare_to_sleep(pdev);
    } else {
        pci_wake_from_d3(pdev, false);
        pci_set_power_state(pdev, PCI_D3hot);
    }

    DPRINTK("fxpm, fxgmac_suspend callout to %s\n", wake ? "sleep" : "D3hot");
    fxgmac_dbg_log("fxpm, fxgmac_suspend callout to %s\n", wake ? "sleep" : "D3hot");

    return 0;
}

static int fxgmac_resume(struct pci_dev *pdev)
{
    struct fxgmac_pdata *pdata;
    struct net_device *netdev;
    u32 err;

    DPRINTK("fxpm, fxgmac_resume callin\n");
    fxgmac_dbg_log("fxpm, fxgmac_resume callin\n");

    netdev = dev_get_drvdata(&pdev->dev);
    pdata = netdev_priv(netdev);

    pdata->expansion.current_state = CURRENT_STATE_RESUME;

    pci_set_power_state(pdev, PCI_D0);
    pci_restore_state(pdev);
    /*
     * pci_restore_state clears dev->state_saved so call
     * pci_save_state to restore it.
     */
    pci_save_state(pdev);

    err = pci_enable_device_mem(pdev);
    if (err) {
        dev_err(pdata->dev, "fxgmac_resume, failed to enable PCI device from suspend\n");
        return err;
    }
    smp_mb__before_atomic();
    __clear_bit(FXGMAC_POWER_STATE_DOWN, &pdata->expansion.powerstate);
    pci_set_master(pdev);

    pci_wake_from_d3(pdev, false);

    rtnl_lock();
    err = 0;//ixgbe_init_interrupt_scheme(adapter);
    if (!err && netif_running(netdev))
        fxgmac_net_powerup(pdata);

    if (!err)
        netif_device_attach(netdev);

    rtnl_unlock();

    DPRINTK("fxpm, fxgmac_resume callout\n");
    fxgmac_dbg_log("fxpm, fxgmac_resume callout\n");

    return err;
}
#endif

static const struct pci_device_id fxgmac_pci_tbl[] = {
    { PCI_DEVICE(0x1f0a, 0x6801) },
    { 0 }
};
MODULE_DEVICE_TABLE(pci, fxgmac_pci_tbl);

static struct pci_driver fxgmac_pci_driver = {
    .name       = FXGMAC_DRV_NAME,
    .id_table   = fxgmac_pci_tbl,
    .probe      = fxgmac_probe,
    .remove     = fxgmac_remove,
#ifdef CONFIG_PM
    /* currently, we only use USE_LEGACY_PM_SUPPORT */
    .suspend    = fxgmac_suspend,
    .resume     = fxgmac_resume,
#endif
    .shutdown   = fxgmac_shutdown,
};

module_pci_driver(fxgmac_pci_driver);

MODULE_DESCRIPTION(FXGMAC_DRV_DESC);
MODULE_VERSION(FXGMAC_DRV_VERSION);
MODULE_AUTHOR("Frank <Frank.Sae@motor-comm.com>");
MODULE_LICENSE("Dual BSD/GPL");
