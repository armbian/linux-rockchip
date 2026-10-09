
/*++

Copyright (c) 2021 Motorcomm Corporation. 
Confidential and Proprietary. All rights reserved.

This is Motorcomm Corporation NIC driver relevant files. Please don't copy, modify,
distribute without commercial permission.

--*/

#ifndef __FUXI_EFUSE_H__
#define __FUXI_EFUSE_H__


bool fxgmac_read_patch_from_efuse(struct fxgmac_pdata* pdata, u32 offset, u32* value); /* read patch per register offset. */
bool fxgmac_read_patch_from_efuse_per_index(struct fxgmac_pdata* pdata, u8 index, u32* offset, u32* value); /* read patch per 0-based index. */
bool fxgmac_write_patch_to_efuse(struct fxgmac_pdata* pdata, u32 offset, u32 value);
bool fxgmac_write_patch_to_efuse_per_index(struct fxgmac_pdata* pdata, u8 index, u32 offset, u32 value);
bool fxgmac_read_mac_subsys_from_efuse(struct fxgmac_pdata* pdata, u8* mac_addr, u32* subsys, u32* revid);
bool fxgmac_write_mac_subsys_to_efuse(struct fxgmac_pdata* pdata, u8* mac_addr, u32* subsys, u32* revid);
bool fxgmac_write_mac_addr_to_efuse(struct fxgmac_pdata* pdata, u8* mac_addr);
bool fxgmac_read_subsys_from_efuse(struct fxgmac_pdata* pdata, u32* subsys, u32* revid);
bool fxgmac_write_subsys_to_efuse(struct fxgmac_pdata* pdata, u32* subsys, u32* revid);
bool fxgmac_efuse_load(struct fxgmac_pdata* pdata);
bool fxgmac_efuse_read_data(struct fxgmac_pdata* pdata, u32 offset, u32* value);
bool fxgmac_efuse_write_data(struct fxgmac_pdata* pdata, u32 offset, u32 value);
bool fxgmac_efuse_write_oob(struct fxgmac_pdata* pdata);
bool fxgmac_efuse_write_led(struct fxgmac_pdata* pdata, u32 value);
bool fxgmac_read_led_setting_from_efuse(struct fxgmac_pdata* pdata);
bool fxgmac_write_led_setting_to_efuse(struct fxgmac_pdata* pdata);

#endif // __FUXI_EFUSE_H__