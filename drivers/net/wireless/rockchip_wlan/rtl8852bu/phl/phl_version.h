/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
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
#ifndef _PHL_VERSION_H_
#define _PHL_VERSION_H_

/*major-minor-hotfix-[branchid + hotfix]*/
#define PHL_VERSION(a, b, c, d) (((a) << 48) + ((b) << 32) + ((c) << 16) + (d))

#define PHL_MAJOR_VER	0001
#define PHL_MINOR_VER	0015
#define PHL_HOTFIX_VER	0034
#define PHL_BRANCH_IDX	0100

#define PHL_VER_CODE	PHL_VERSION(PHL_MAJOR_VER, PHL_MINOR_VER, PHL_HOTFIX_VER, PHL_BRANCH_IDX)

#endif /*_PHL_VERSION_H_*/
