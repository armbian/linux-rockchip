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


#ifdef CONFIG_PCI_MSI
u32 pcidev_int_mode; // for msix

//for MSIx, 20210526
struct msix_entry *msix_entries = NULL;
int req_vectors = 0;
#endif

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

/*
 * since kernel does not clear the MSI mask bits and 
 * this function clear MSI mask bits when MSI is enabled.
 */
int fxgmac_pci_config_modify(struct pci_dev *pdev)
{
    //struct net_device *netdev = dev_get_drvdata(&pdev->dev);
    //struct fxgmac_pdata *pdata = netdev_priv(netdev);
    u16 pcie_cap_offset;
    u32 pcie_msi_mask_bits;
    int ret;

    pcie_cap_offset = pci_find_capability(pdev, 0x05 /*MSI Capability ID*/);
    if (pcie_cap_offset) {
        ret = pci_read_config_dword(pdev,
                                  pcie_cap_offset + 0x10 /* MSI mask bits */,
                                  &pcie_msi_mask_bits);
        if (ret)
        {
            DPRINTK(KERN_ERR "fxgmac_pci_config_modify read pci config space MSI cap. failed, %d\n", ret);
            return -EFAULT;
        }
    }

    ret = pci_write_config_word(pdev, pcie_cap_offset + 0xc /* MSI data */, 0x1234/* random val */);
    ret = pci_write_config_dword(pdev, pcie_cap_offset + 0x10 /* MSI mask bits */, (pcie_msi_mask_bits & 0xffff0000) /*clear mask bits*/);
    if (ret)
    {
        DPRINTK(KERN_ERR "fxgmac_pci_config_modify write pci config space MSI mask failed, %d\n", ret);
    }

    return ret;
}

static int fxgmac_probe(struct pci_dev *pcidev, const struct pci_device_id *id)
{
    struct device *dev = &pcidev->dev;
    struct fxgmac_resources res;
    int i, ret;
#ifdef CONFIG_PCI_MSI
    //for MSIx, 20210526
    int vectors, rc;
#endif

    ret = pcim_enable_device(pcidev);
    if (ret) {
        dev_err(dev, "ERROR: fxgmac_probe failed to enable device\n");
        return ret;
    }

    for (i = 0; i <= PCI_STD_RESOURCE_END; i++) {
        if (pci_resource_len(pcidev, i) == 0)
            continue;
        /*if (0x100 != pci_resource_len(pcidev, i))
            continue;
        */

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
    DPRINTK(KERN_INFO "fxgmac_probe res.addr=%p,val=%#x\n",res.addr,/*(int)(*((int*)(res.addr)))*/0);

#ifdef CONFIG_PCI_MSI
    /* added for MSIx feature, 20210526 */
    pcidev_int_mode = 0; //init to legacy interrupt, and MSIx has the first priority

    /* check cpu core number.
     * since we have 4 channels, we must ensure the number of cpu core > 4
     * otherwise, just roll back to legacy
     */
    vectors = num_online_cpus();
    DPRINTK("fxgmac_probe, num of cpu=%d\n", vectors);
    if(vectors >= FXGMAC_MAX_DMA_CHANNELS) {
#if FXGMAC_TX_INTERRUPT_EN
        req_vectors = FXGMAC_MAX_DMA_CHANNELS_PLUS_1TX;
#else
        req_vectors = FXGMAC_MAX_DMA_CHANNELS;
#endif
        msix_entries = kcalloc(/*vectors 20220211*/req_vectors,
                        sizeof(struct msix_entry),
                        GFP_KERNEL);
        if (!msix_entries) {
            DPRINTK("fxgmac_probe, MSIx, kcalloc err for msix entries, rollback to MSI..\n");
            goto enable_msi_interrupt;
        }else {
            for (i = 0; i < /*vectors 20220211*/req_vectors; i++)
                msix_entries[i].entry = i;
#if 0
            /* follow the requirement from pci_enable_msix():
             *
             * Setup the MSI-X capability structure of device function with the number
             * of requested irqs upon its software driver call to request for
             * MSI-X mode enabled on its hardware device function. A return of zero
             * indicates the successful configuration of MSI-X capability structure
             * with new allocated MSI-X irqs. A return of < 0 indicates a failure.
             * Or a return of > 0 indicates that driver request is exceeding the number
             * of irqs or MSI-X vectors available. Driver should use the returned value to
             * re-send its request.
             */
            req_vectors = vectors;
                do {
                        rc = pci_enable_msix(pcidev, msix_entries, req_vectors);
                        if (rc < 0) {
                    //failure
                    //DPRINTK("fxgmac_probe, enable MSIx failed,%d.\n", rc);
                    rc = 0; //stop the loop
                    req_vectors = 0; //indicate failure
                        } else if (rc > 0) {
                            //re-request with new number
                            req_vectors = rc;
                        }
                } while (rc);
#else
            rc = pci_enable_msix_range(pcidev, msix_entries, req_vectors, req_vectors);
            if (rc < 0) {
                    //failure
                    DPRINTK("fxgmac_probe, enable MSIx failed,%d.\n", rc);
                    req_vectors = 0; //indicate failure
            } else {
                    req_vectors = rc;
            }
#endif

#if FXGMAC_TX_INTERRUPT_EN
            if(req_vectors >= FXGMAC_MAX_DMA_CHANNELS_PLUS_1TX) {
#else
            if(req_vectors >= FXGMAC_MAX_DMA_CHANNELS) {
#endif
                DPRINTK("fxgmac_probe, enable MSIx ok, cpu=%d,vectors=%d.\n", vectors, req_vectors);
                pcidev_int_mode = FXGMAC_FLAG_MSIX_CAPABLE | FXGMAC_FLAG_MSIX_ENABLED;
                goto drv_probe;
            }else if (req_vectors){
                DPRINTK("fxgmac_probe, enable MSIx with only %d vector, while we need %d, rollback to MSI.\n", req_vectors, vectors);
                //roll back to msi
                pci_disable_msix(pcidev);
                kfree(msix_entries);
                msix_entries = NULL;
            }else {
                DPRINTK("fxgmac_probe, enable MSIx failure and clear msix entries.\n");
                //roll back to msi
                kfree(msix_entries);
                msix_entries = NULL;
            }
        }
    }

enable_msi_interrupt:
    rc = pci_enable_msi(pcidev);
    if (rc < 0)
        DPRINTK("fxgmac_probe, enable MSI failure, rollback to LEGACY.\n");
    else {
        pcidev_int_mode = FXGMAC_FLAG_MSI_CAPABLE | FXGMAC_FLAG_MSI_ENABLED;
        DPRINTK("fxgmac_probe, enable MSI ok, irq=%d.\n", pcidev->irq);
    }

drv_probe:
#endif

    //fxgmac_dbg_log_init();
    fxgmac_dbg_log("fxpm,_fxgmac_probe\n");

    return fxgmac_drv_probe(&pcidev->dev, &res);
}

static void fxgmac_remove(struct pci_dev *pcidev)
{
    struct net_device *netdev = dev_get_drvdata(&pcidev->dev);
    struct fxgmac_pdata * pdata = netdev_priv(netdev);

    fxgmac_drv_remove(&pcidev->dev);
#ifdef CONFIG_PCI_MSI
    //20210526
    if(pcidev_int_mode & (FXGMAC_FLAG_MSIX_CAPABLE | FXGMAC_FLAG_MSIX_ENABLED)){
        pci_disable_msix(pcidev);
        kfree(msix_entries);
        msix_entries = NULL;
        req_vectors = 0;
    }
    pcidev_int_mode = 0;
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
    u32 wufc = pdata->wol;
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

    pdata->current_state = CURRENT_STATE_SHUTDOWN;
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

    pdata->current_state = CURRENT_STATE_SUSPEND;
    retval = __fxgmac_shutdown(pdev, &wake);
    if (retval)
        return retval;

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

    pdata->current_state = CURRENT_STATE_RESUME;

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
    __clear_bit(FXGMAC_POWER_STATE_DOWN, &pdata->powerstate);
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
