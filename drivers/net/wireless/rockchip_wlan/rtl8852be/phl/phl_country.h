/******************************************************************************
 *
 * Copyright(c) 2020 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#ifndef _PHL_COUNTRY_H_
#define _PHL_COUNTRY_H_

#define REGULATION_COUNTRY_VERSION 31

#define MAX_COUNTRY_NUM 238
enum TP_OVERWRITE { 
    TPO_CHILE = 0,
    TPO_QATAR = 1,
    TPO_UKRAINE = 2,
    TPO_CN = 3,
    TPO_NA = 4
}; 

#define COUNTRY_CODE_LEN 2
struct country_domain_mapping {
    u8 domain_code;
    u8 domain_code_6g;
    char char2[COUNTRY_CODE_LEN];
    u8 tpo; /* tx power overwrite */

    /*
     * bit0: accept 11bgn
     * bit1: accept 11a
     * bit2: accept 11ac
     * bit3: accept 11ax
     */
    u8 support;
};


#endif /* _PHL_COUNTRY_H_ */
