// SPDX-License-Identifier: GPL-2.0-or-later

static u8 icnl9951_driver_builtin_firmware[] = {
};

static u8 icnl9951r_driver_builtin_firmware[] = {
};

static struct cts_firmware cts_driver_builtin_firmwares[] = {
    {
        .name = "OEM-Project",    /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9951,
        .fwid = CTS_DEV_FWID_ICNL9951,
        .data = icnl9951_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9951_driver_builtin_firmware),
    },
    {
        .name = "OEM-Project",    /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9951R,
        .fwid = CTS_DEV_FWID_ICNL9951R,
        .data = icnl9951r_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9951r_driver_builtin_firmware),
    },

};
