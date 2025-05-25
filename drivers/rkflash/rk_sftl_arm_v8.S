/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2018-2025 Rockchip Electronics Co., Ltd.
 * date: 2025-03-02
 */
	.arch armv8-a
	.file	"rk_sftl.c"
	.text
	.align	2
	.type	access_ok, %function
access_ok:
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x2, sp_el0
// 0 "" 2
#NO_APP
	ldr	w3, [x2, 44]
	tbnz	x3, 21, .L2
	ldr	x2, [x2]
	tst	w2, 67108864
	beq	.L3
.L2:
	sbfx	x2, x0, 0, 56
	and	x0, x0, x2
.L3:
	mov	x2, 549755813888
	sub	x1, x2, x1
	cmp	x0, x1
	cset	w0, ls
	ret
	.size	access_ok, .-access_ok
	.align	2
	.type	_copy_from_user, %function
_copy_from_user:
	stp	x29, x30, [sp, -48]!
	mov	x4, x1
	mov	x1, x2
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x2
	str	x21, [sp, 32]
	mov	x21, x0
	mov	x0, x4
	bl	access_ok
	cbz	w0, .L10
#APP
// 217 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x1, x4, 36028797018963968

// 0 "" 2
#NO_APP
	mov	x2, x19
	mov	x0, x21
	bl	__arch_copy_from_user
	mov	x20, x0
	cbz	x0, .L7
.L8:
	sub	x19, x19, x20
	mov	x2, x20
	add	x0, x21, x19
	mov	w1, 0
	bl	memset
.L7:
	mov	x0, x20
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L10:
	mov	x20, x19
	b	.L8
	.size	_copy_from_user, .-_copy_from_user
	.align	2
	.type	l2p_addr_tran.isra.0, %function
l2p_addr_tran.isra.0:
	adrp	x4, .LANCHOR0
	add	x4, x4, :lo12:.LANCHOR0
	ldrh	w3, [x4, 8]
	ldrh	w5, [x4, 10]
	ldrh	w4, [x4, 14]
	cmp	w4, 4
	bne	.L16
	lsr	w3, w3, 1
	ubfiz	w5, w5, 1, 15
.L16:
	ubfx	x4, x0, 10, 16
	ubfx	x6, x0, 10, 16
	and	w0, w0, 1023
	udiv	w4, w4, w3
	msub	w3, w3, w4, w6
	madd	w3, w3, w5, w0
	str	w3, [x1]
	str	w4, [x2]
	ret
	.size	l2p_addr_tran.isra.0, .-l2p_addr_tran.isra.0
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"SFTL version: 5.0.59 20221121"
.LC1:
	.string	"\n%s\n"
.LC2:
	.string	"act blk: %x %x %x %x %x %x\n"
.LC3:
	.string	"buf blk: %x %x %x %x %x %x\n"
.LC4:
	.string	"tmp blk: %x %x %x %x %x %x\n"
.LC5:
	.string	"gc blk: %x %x %x %x %x %x\n"
.LC6:
	.string	"free blk: %x %x %x\n"
	.text
	.align	2
	.global	ftl_print_sblk_info
	.type	ftl_print_sblk_info, %function
ftl_print_sblk_info:
	stp	x29, x30, [sp, -32]!
	adrp	x1, .LC0
	adrp	x0, .LC1
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	add	x1, x1, :lo12:.LC0
	add	x0, x0, :lo12:.LC1
	bl	sftl_printk
	ldrh	w1, [x19, 24]
	ldr	x0, [x19, 72]
	ldrh	w5, [x19, 28]
	ldrb	w4, [x19, 32]
	ldrh	w6, [x0, x1, lsl 1]
	adrp	x0, .LC2
	ldrb	w3, [x19, 30]
	add	x0, x0, :lo12:.LC2
	ldrh	w2, [x19, 26]
	ldrh	w1, [x19, 24]
	bl	sftl_printk
	ldrh	w1, [x19, 80]
	ldr	x0, [x19, 72]
	ldrh	w5, [x19, 84]
	ldrb	w4, [x19, 88]
	ldrh	w6, [x0, x1, lsl 1]
	adrp	x0, .LC3
	ldrb	w3, [x19, 86]
	add	x0, x0, :lo12:.LC3
	ldrh	w2, [x19, 82]
	ldrh	w1, [x19, 80]
	bl	sftl_printk
	ldrh	w1, [x19, 128]
	ldr	x0, [x19, 72]
	ldrh	w5, [x19, 132]
	ldrb	w4, [x19, 136]
	ldrh	w6, [x0, x1, lsl 1]
	adrp	x0, .LC4
	ldrb	w3, [x19, 134]
	add	x0, x0, :lo12:.LC4
	ldrh	w2, [x19, 130]
	ldrh	w1, [x19, 128]
	bl	sftl_printk
	ldrh	w1, [x19, 176]
	ldr	x0, [x19, 72]
	ldrh	w5, [x19, 180]
	ldrb	w4, [x19, 184]
	ldrh	w6, [x0, x1, lsl 1]
	adrp	x0, .LC5
	ldrb	w3, [x19, 182]
	add	x0, x0, :lo12:.LC5
	ldrh	w2, [x19, 178]
	ldrh	w1, [x19, 176]
	bl	sftl_printk
	ldrh	w3, [x19, 224]
	adrp	x0, .LC6
	ldrh	w2, [x19, 226]
	add	x0, x0, :lo12:.LC6
	ldrh	w1, [x19, 228]
	bl	sftl_printk
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	ftl_print_sblk_info, .-ftl_print_sblk_info
	.align	2
	.global	Ftl_log2
	.type	Ftl_log2, %function
Ftl_log2:
	mov	w2, 1
	mov	w1, 0
.L20:
	cmp	w2, w0
	bls	.L21
	sub	w0, w1, #1
	ret
.L21:
	add	w1, w1, 1
	lsl	w2, w2, 1
	and	w1, w1, 65535
	b	.L20
	.size	Ftl_log2, .-Ftl_log2
	.align	2
	.global	FtlPrintInfo
	.type	FtlPrintInfo, %function
FtlPrintInfo:
	ret
	.size	FtlPrintInfo, .-FtlPrintInfo
	.align	2
	.global	FtlSysBlkNumInit
	.type	FtlSysBlkNumInit, %function
FtlSysBlkNumInit:
	and	w0, w0, 65535
	mov	w1, 24
	cmp	w0, 24
	csel	w0, w0, w1, cs
	and	w1, w0, 65535
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	ldrh	w2, [x0, 236]
	ldrh	w3, [x0, 246]
	str	w1, [x0, 232]
	mul	w2, w2, w1
	sub	w1, w3, w1
	strh	w1, [x0, 244]
	ldr	w1, [x0, 252]
	str	w2, [x0, 240]
	sub	w2, w1, w2
	str	w2, [x0, 248]
	mov	w0, 0
	ret
	.size	FtlSysBlkNumInit, .-FtlSysBlkNumInit
	.align	2
	.global	FtlConstantsInit
	.type	FtlConstantsInit, %function
FtlConstantsInit:
	mov	x6, x0
	stp	x29, x30, [sp, -16]!
	adrp	x4, .LANCHOR0
	add	x4, x4, :lo12:.LANCHOR0
	mov	x29, sp
	ldrh	w1, [x0]
	ldrh	w3, [x0, 2]
	ldrh	w5, [x0, 4]
	ldrh	w2, [x6, 14]
	ldrh	w0, [x0, 6]
	strh	w0, [x4, 246]
	strh	w1, [x4, 256]
	strh	w3, [x4, 258]
	strh	w5, [x4, 260]
	strh	w2, [x4, 262]
	cmp	w2, 4
	bne	.L25
	lsr	w0, w0, 1
	strh	w0, [x4, 246]
	mov	w0, 8
	strh	w0, [x4, 262]
.L26:
	add	x0, x4, 264
	mov	x8, 0
.L27:
	strb	w8, [x8, x0]
	add	x8, x8, 1
	cmp	x8, 32
	bne	.L27
	cmp	w1, 1
	mov	w0, 5
	csel	w0, w0, w1, ne
	strh	w0, [x4, 296]
	mov	w0, 640
	ldrh	w5, [x4, 246]
	strh	w0, [x4, 300]
	ldrh	w0, [x4, 260]
	strh	wzr, [x4, 298]
	mul	w3, w3, w0
	mul	w0, w0, w5
	and	w3, w3, 65535
	strh	w3, [x4, 236]
	and	w0, w0, 65535
	strh	w0, [x4, 302]
	bl	Ftl_log2
	ldrh	w9, [x6, 12]
	ldrh	w7, [x4, 262]
	strh	w0, [x4, 304]
	strh	w9, [x4, 306]
	mul	w0, w3, w9
	strh	w9, [x4, 308]
	strh	w0, [x4, 310]
	mov	w0, w7
	bl	Ftl_log2
	and	w10, w0, 65535
	ubfiz	w1, w7, 9, 7
	strh	w1, [x4, 314]
	and	w1, w7, 127
	mul	w2, w3, w5
	mul	w7, w7, w9
	str	w2, [x4, 252]
	ubfiz	w1, w1, 1, 15
	strh	w1, [x4, 316]
	ldrh	w1, [x6, 20]
	lsl	w5, w5, 6
	strh	w1, [x4, 318]
	mov	w1, 5120
	mul	w2, w2, w7
	strh	w0, [x4, 312]
	sdiv	w1, w1, w7
	asr	w2, w2, 11
	str	w2, [x4, 320]
	mov	w2, 4
	cmp	w1, 4
	csel	w1, w1, w2, gt
	mov	w2, 640
	asr	w2, w2, w0
	add	w0, w10, 9
	add	w2, w2, 2
	cmp	w3, 1
	asr	w0, w5, w0
	and	w5, w0, 65535
	strh	w0, [x4, 328]
	strh	w1, [x4, 324]
	mul	w0, w3, w5
	strh	w2, [x4, 326]
	str	w0, [x4, 332]
	and	w0, w1, 65535
	udiv	w0, w0, w3
	add	w0, w0, w5
	add	w1, w0, 12
	add	w0, w0, 8
	csel	w0, w0, w1, ne
	str	w0, [x4, 232]
	bl	FtlSysBlkNumInit
	strh	w8, [x4, 342]
	ldr	w0, [x4, 232]
	ldr	w1, [x4, 248]
	str	w0, [x4, 336]
	ldrh	w0, [x4, 306]
	str	wzr, [x4, 344]
	mul	w0, w0, w1
	ldrh	w1, [x4, 312]
	add	w1, w1, 9
	lsl	w0, w0, 2
	lsr	w0, w0, w1
	add	w0, w0, 2
	strh	w0, [x4, 340]
	ldrh	w0, [x4, 324]
	add	w0, w0, 3
	strh	w0, [x4, 324]
	ldr	w0, [x4, 332]
	add	w0, w0, 3
	str	w0, [x4, 332]
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
.L25:
	cmp	w2, 8
	bne	.L26
	cmp	w5, 1
	bne	.L26
	lsr	w0, w0, 1
	strh	w0, [x4, 246]
	mov	w0, 2
	strh	w0, [x4, 260]
	b	.L26
	.size	FtlConstantsInit, .-FtlConstantsInit
	.align	2
	.global	IsBlkInVendorPart
	.type	IsBlkInVendorPart, %function
IsBlkInVendorPart:
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	and	w0, w0, 65535
	ldrh	w2, [x1, 348]
	cbz	w2, .L39
	ldr	x2, [x1, 352]
	ldrh	w3, [x1, 324]
	mov	x1, 0
	sub	x2, x2, #2
.L37:
	cmp	w3, w1, uxth
	bhi	.L38
.L39:
	mov	w0, 0
.L35:
	ret
.L38:
	add	x1, x1, 1
	ldrh	w4, [x2, x1, lsl 1]
	cmp	w4, w0
	bne	.L37
	mov	w0, 1
	b	.L35
	.size	IsBlkInVendorPart, .-IsBlkInVendorPart
	.align	2
	.global	FtlCacheWriteBack
	.type	FtlCacheWriteBack, %function
FtlCacheWriteBack:
	mov	w0, 0
	ret
	.size	FtlCacheWriteBack, .-FtlCacheWriteBack
	.align	2
	.global	sftl_get_density
	.type	sftl_get_density, %function
sftl_get_density:
	adrp	x0, .LANCHOR0+344
	ldr	w0, [x0, #:lo12:.LANCHOR0+344]
	ret
	.size	sftl_get_density, .-sftl_get_density
	.section	.rodata.str1.1
.LC7:
	.string	"phyBlk = 0x%x die = %d block_in_die = 0x%x 0x%8x\n"
	.text
	.align	2
	.global	FtlBbmMapBadBlock
	.type	FtlBbmMapBadBlock, %function
FtlBbmMapBadBlock:
	stp	x29, x30, [sp, -32]!
	and	w1, w0, 65535
	mov	w4, 1
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	add	x19, x19, 360
	ldrh	w3, [x19, -58]
	udiv	w2, w1, w3
	msub	w3, w2, w3, w1
	add	x0, x19, w2, uxth 3
	ubfx	x5, x3, 5, 11
	lsl	w4, w4, w3
	ldr	x0, [x0, 32]
	lsl	x5, x5, 2
	and	w3, w3, 65535
	ldr	w6, [x0, x5]
	orr	w4, w4, w6
	str	w4, [x0, x5]
	adrp	x0, .LC7
	add	x0, x0, :lo12:.LC7
	bl	sftl_printk
	ldrh	w0, [x19, 6]
	add	w0, w0, 1
	strh	w0, [x19, 6]
	mov	w0, 0
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlBbmMapBadBlock, .-FtlBbmMapBadBlock
	.align	2
	.global	FtlBbmIsBadBlock
	.type	FtlBbmIsBadBlock, %function
FtlBbmIsBadBlock:
	adrp	x2, .LANCHOR0
	add	x3, x2, :lo12:.LANCHOR0
	and	w0, w0, 65535
	ldrh	w1, [x3, 302]
	udiv	w2, w0, w1
	msub	w1, w2, w1, w0
	add	x2, x3, w2, uxth 3
	ldr	x0, [x2, 392]
	ubfx	x3, x1, 5, 11
	ldr	w0, [x0, x3, lsl 2]
	lsr	w0, w0, w1
	and	w0, w0, 1
	ret
	.size	FtlBbmIsBadBlock, .-FtlBbmIsBadBlock
	.align	2
	.global	FtlBbtInfoPrint
	.type	FtlBbtInfoPrint, %function
FtlBbtInfoPrint:
	ret
	.size	FtlBbtInfoPrint, .-FtlBbtInfoPrint
	.align	2
	.global	FtlBbtCalcTotleCnt
	.type	FtlBbtCalcTotleCnt, %function
FtlBbtCalcTotleCnt:
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	mov	w5, 0
	mov	w4, 0
	ldrh	w6, [x0, 302]
	ldrh	w0, [x0, 258]
	mul	w6, w6, w0
	cmp	w4, w6
	blt	.L57
	mov	w0, w5
	ret
.L57:
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
.L50:
	mov	w0, w4
	bl	FtlBbmIsBadBlock
	cbz	w0, .L49
	add	w5, w5, 1
	and	w5, w5, 65535
.L49:
	add	w4, w4, 1
	and	w4, w4, 65535
	cmp	w4, w6
	blt	.L50
	mov	w0, w5
	ldp	x29, x30, [sp], 16
	ret
	.size	FtlBbtCalcTotleCnt, .-FtlBbtCalcTotleCnt
	.align	2
	.global	V2P_block
	.type	V2P_block, %function
V2P_block:
	adrp	x4, .LANCHOR0
	add	x4, x4, :lo12:.LANCHOR0
	and	w0, w0, 65535
	and	w1, w1, 65535
	ldrh	w2, [x4, 260]
	ldrh	w4, [x4, 302]
	udiv	w3, w0, w2
	msub	w0, w3, w2, w0
	madd	w2, w2, w1, w0
	madd	w0, w3, w4, w2
	ret
	.size	V2P_block, .-V2P_block
	.align	2
	.global	P2V_plane
	.type	P2V_plane, %function
P2V_plane:
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	and	w0, w0, 65535
	ldrh	w1, [x2, 260]
	ldrh	w3, [x2, 302]
	udiv	w2, w0, w1
	udiv	w3, w0, w3
	msub	w0, w2, w1, w0
	madd	w0, w1, w3, w0
	ret
	.size	P2V_plane, .-P2V_plane
	.align	2
	.global	P2V_block_in_plane
	.type	P2V_block_in_plane, %function
P2V_block_in_plane:
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	and	w3, w0, 65535
	ldrh	w0, [x2, 302]
	udiv	w1, w3, w0
	msub	w0, w1, w0, w3
	ldrh	w1, [x2, 260]
	and	w0, w0, 65535
	udiv	w0, w0, w1
	ret
	.size	P2V_block_in_plane, .-P2V_block_in_plane
	.align	2
	.global	ftl_cmp_data_ver
	.type	ftl_cmp_data_ver, %function
ftl_cmp_data_ver:
	cmp	w0, w1
	mov	w2, -2147483648
	bls	.L62
	sub	w1, w0, w1
	cmp	w1, w2
	cset	w0, ls
.L61:
	ret
.L62:
	sub	w1, w1, w0
	cmp	w1, w2
	cset	w0, hi
	b	.L61
	.size	ftl_cmp_data_ver, .-ftl_cmp_data_ver
	.align	2
	.global	FtlFreeSysBlkQueueEmpty
	.type	FtlFreeSysBlkQueueEmpty, %function
FtlFreeSysBlkQueueEmpty:
	adrp	x0, .LANCHOR0+462
	ldrh	w0, [x0, #:lo12:.LANCHOR0+462]
	cmp	w0, 0
	cset	w0, eq
	ret
	.size	FtlFreeSysBlkQueueEmpty, .-FtlFreeSysBlkQueueEmpty
	.align	2
	.global	FtlFreeSysBlkQueueFull
	.type	FtlFreeSysBlkQueueFull, %function
FtlFreeSysBlkQueueFull:
	adrp	x0, .LANCHOR0+462
	ldrh	w0, [x0, #:lo12:.LANCHOR0+462]
	cmp	w0, 1024
	cset	w0, eq
	ret
	.size	FtlFreeSysBlkQueueFull, .-FtlFreeSysBlkQueueFull
	.align	2
	.global	FtlFreeSysBLkSort
	.type	FtlFreeSysBLkSort, %function
FtlFreeSysBLkSort:
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	add	x4, x0, 456
	ldrh	w1, [x0, 462]
	cbz	w1, .L66
	ldrh	w5, [x0, 2540]
	mov	w6, 0
	ldrh	w2, [x4, 2]
	mov	w3, 0
	ldrh	w1, [x4, 4]
	and	w5, w5, 31
.L69:
	cmp	w3, w5
	bne	.L70
	cbz	w6, .L66
	strh	w2, [x0, 458]
	strh	w1, [x0, 460]
.L66:
	ret
.L70:
	add	x6, x4, w2, sxtw 1
	add	w3, w3, 1
	add	w2, w2, 1
	and	w3, w3, 65535
	and	w2, w2, 1023
	ldrh	w7, [x6, 8]
	add	x6, x4, w1, sxtw 1
	add	w1, w1, 1
	and	w1, w1, 1023
	strh	w7, [x6, 8]
	mov	w6, 1
	b	.L69
	.size	FtlFreeSysBLkSort, .-FtlFreeSysBLkSort
	.align	2
	.global	IsInFreeQueue
	.type	IsInFreeQueue, %function
IsInFreeQueue:
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	and	w0, w0, 65535
	add	x4, x1, 456
	ldrh	w3, [x1, 462]
	cmp	w3, 1024
	beq	.L79
	ldrh	w5, [x4, 2]
	mov	w2, 0
.L77:
	cmp	w3, w2
	bhi	.L78
.L79:
	mov	w0, 0
.L75:
	ret
.L78:
	add	w1, w5, w2
	ubfiz	x1, x1, 1, 10
	add	x1, x4, x1
	ldrh	w1, [x1, 8]
	cmp	w1, w0
	beq	.L80
	add	w2, w2, 1
	b	.L77
.L80:
	mov	w0, 1
	b	.L75
	.size	IsInFreeQueue, .-IsInFreeQueue
	.align	2
	.global	insert_data_list
	.type	insert_data_list, %function
insert_data_list:
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	and	w3, w0, 65535
	ldrh	w2, [x1, 244]
	cmp	w2, w0, uxth
	bls	.L83
	mov	w10, 6
	mov	w0, -1
	ldr	x7, [x1, 2560]
	umull	x6, w3, w10
	add	x4, x7, x6
	str	w0, [x7, x6]
	ldr	x2, [x1, 2568]
	cbnz	x2, .L84
.L95:
	str	x4, [x1, 2568]
	b	.L83
.L84:
	ldr	x11, [x1, 72]
	ubfiz	x5, x3, 1, 16
	ldrh	w8, [x4, 4]
	mov	w12, 0
	ldr	x9, [x1, 2560]
	cmp	w8, 0
	ldrh	w5, [x11, x5]
	mov	w15, 65535
	ldrh	w14, [x1, 244]
	mul	w5, w5, w8
	mov	x8, -6148914691236517206
	movk	x8, 0xaaab, lsl 0
	csel	w5, w5, w0, ne
	sub	x0, x2, x9
	asr	x0, x0, 1
	mul	x0, x0, x8
	and	w0, w0, 65535
.L89:
	add	w8, w12, 1
	and	w12, w8, 65535
	cmp	w14, w8, uxth
	bcc	.L83
	cmp	w3, w0
	beq	.L83
	ldrh	w13, [x2, 4]
	cbz	w13, .L87
	ubfiz	x8, x0, 1, 16
	ldrh	w8, [x11, x8]
	mul	w8, w8, w13
	cmp	w8, w5
	bcs	.L87
	ldrh	w8, [x2]
	cmp	w8, w15
	bne	.L88
	strh	w0, [x4, 2]
	strh	w3, [x2]
	str	x4, [x1, 2576]
.L83:
	mov	w0, 0
	ret
.L88:
	umaddl	x2, w8, w10, x9
	mov	w0, w8
	b	.L89
.L87:
	strh	w0, [x7, x6]
	ldrh	w0, [x2, 2]
	strh	w0, [x4, 2]
	ldr	x5, [x1, 2568]
	cmp	x2, x5
	bne	.L90
	strh	w3, [x2, 2]
	b	.L95
.L90:
	mov	w4, 6
	ldr	x1, [x1, 2560]
	umull	x0, w0, w4
	strh	w3, [x1, x0]
	strh	w3, [x2, 2]
	b	.L83
	.size	insert_data_list, .-insert_data_list
	.section	.rodata.str1.1
.LC8:
	.string	"\n!!!!! error @ func:%s - line:%d\n"
	.text
	.align	2
	.global	INSERT_DATA_LIST
	.type	INSERT_DATA_LIST, %function
INSERT_DATA_LIST:
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	bl	insert_data_list
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	ldrh	w0, [x1, 2584]
	add	w0, w0, 1
	and	w0, w0, 65535
	strh	w0, [x1, 2584]
	ldrh	w1, [x1, 244]
	cmp	w1, w0
	bcs	.L96
	adrp	x1, .LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, :lo12:.LANCHOR1
	add	x0, x0, :lo12:.LC8
	mov	w2, 214
	bl	sftl_printk
.L96:
	ldp	x29, x30, [sp], 16
	ret
	.size	INSERT_DATA_LIST, .-INSERT_DATA_LIST
	.align	2
	.global	insert_free_list
	.type	insert_free_list, %function
insert_free_list:
	mov	w11, 65535
	and	w3, w0, 65535
	cmp	w11, w0, uxth
	beq	.L100
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	mov	w9, 6
	mov	w0, -1
	umull	x5, w3, w9
	ldr	x6, [x1, 2560]
	add	x4, x6, x5
	str	w0, [x6, x5]
	ldr	x0, [x1, 2592]
	cbnz	x0, .L101
.L106:
	str	x4, [x1, 2592]
	b	.L100
.L101:
	ldr	x10, [x1, 2600]
	ubfiz	x2, x3, 1, 16
	ldr	x8, [x1, 2560]
	mov	x7, -6148914691236517206
	ldrh	w12, [x10, x2]
	movk	x7, 0xaaab, lsl 0
	sub	x2, x0, x8
	asr	x2, x2, 1
	mul	x2, x2, x7
	and	w2, w2, 65535
.L104:
	ubfiz	x7, x2, 1, 16
	ldrh	w7, [x10, x7]
	cmp	w7, w12
	bcs	.L102
	ldrh	w7, [x0]
	cmp	w7, w11
	bne	.L103
	strh	w2, [x4, 2]
	strh	w3, [x0]
.L100:
	mov	w0, 0
	ret
.L103:
	umaddl	x0, w7, w9, x8
	mov	w2, w7
	b	.L104
.L102:
	ldrh	w7, [x0, 2]
	strh	w7, [x4, 2]
	strh	w2, [x6, x5]
	ldr	x2, [x1, 2592]
	cmp	x2, x0
	bne	.L105
	strh	w3, [x0, 2]
	b	.L106
.L105:
	ldrh	w2, [x0, 2]
	mov	w4, 6
	ldr	x1, [x1, 2560]
	umull	x2, w2, w4
	strh	w3, [x1, x2]
	strh	w3, [x0, 2]
	b	.L100
	.size	insert_free_list, .-insert_free_list
	.align	2
	.global	INSERT_FREE_LIST
	.type	INSERT_FREE_LIST, %function
INSERT_FREE_LIST:
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	bl	insert_free_list
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	ldrh	w0, [x1, 228]
	add	w0, w0, 1
	and	w0, w0, 65535
	strh	w0, [x1, 228]
	ldrh	w1, [x1, 244]
	cmp	w1, w0
	bcs	.L107
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 17
	add	x0, x0, :lo12:.LC8
	mov	w2, 207
	bl	sftl_printk
.L107:
	ldp	x29, x30, [sp], 16
	ret
	.size	INSERT_FREE_LIST, .-INSERT_FREE_LIST
	.align	2
	.global	List_remove_node
	.type	List_remove_node, %function
List_remove_node:
	stp	x29, x30, [sp, -64]!
	and	w1, w1, 65535
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	x21, x0
	mov	w0, 6
	ldr	x22, [x20, 2560]
	umull	x19, w1, w0
	str	x23, [sp, 48]
	mov	w0, 65535
	add	x23, x22, x19
	ldrh	w1, [x23, 2]
	cmp	w1, w0
	bne	.L111
	ldr	x0, [x21]
	cmp	x23, x0
	bne	.L112
.L115:
	ldrh	w0, [x22, x19]
	mov	w1, 65535
	cmp	w0, w1
	bne	.L119
	str	xzr, [x21]
.L117:
	mov	w0, -1
	str	w0, [x22, x19]
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldr	x23, [sp, 48]
	ldp	x29, x30, [sp], 64
	ret
.L112:
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 34
	add	x0, x0, :lo12:.LC8
	mov	w2, 372
	bl	sftl_printk
.L111:
	ldr	x0, [x21]
	ldrh	w1, [x22, x19]
	cmp	x0, x23
	beq	.L115
	ldrh	w0, [x23, 2]
	mov	w2, 65535
	cmp	w1, w2
	bne	.L118
	cmp	w0, w1
	beq	.L117
	mov	w1, 6
	mov	w2, -1
	umull	x0, w0, w1
	ldr	x1, [x20, 2560]
	strh	w2, [x1, x0]
	b	.L117
.L119:
	ldr	x1, [x20, 2560]
	mov	w2, 6
	umaddl	x0, w0, w2, x1
	mov	w1, -1
	str	x0, [x21]
	strh	w1, [x0, 2]
	b	.L117
.L118:
	ldr	x3, [x20, 2560]
	mov	w2, 6
	umaddl	x1, w1, w2, x3
	strh	w0, [x1, 2]
	umull	x0, w0, w2
	ldr	x1, [x20, 2560]
	ldrh	w3, [x22, x19]
	strh	w3, [x1, x0]
	b	.L117
	.size	List_remove_node, .-List_remove_node
	.align	2
	.global	List_pop_index_node
	.type	List_pop_index_node, %function
List_pop_index_node:
	ldr	x2, [x0]
	cbz	x2, .L127
	stp	x29, x30, [sp, -32]!
	adrp	x3, .LANCHOR0+2560
	and	w1, w1, 65535
	mov	x29, sp
	str	x19, [sp, 16]
	mov	w4, 65535
	ldr	x19, [x3, #:lo12:.LANCHOR0+2560]
	mov	w5, 6
.L124:
	cbz	w1, .L125
	ldrh	w3, [x2]
	cmp	w3, w4
	bne	.L126
.L125:
	sub	x19, x2, x19
	mov	x2, -6148914691236517206
	movk	x2, 0xaaab, lsl 0
	asr	x19, x19, 1
	mul	x19, x19, x2
	and	w19, w19, 65535
	mov	w1, w19
	bl	List_remove_node
	mov	w0, w19
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L126:
	sub	w1, w1, #1
	umaddl	x2, w3, w5, x19
	and	w1, w1, 65535
	b	.L124
.L127:
	mov	w0, 65535
	ret
	.size	List_pop_index_node, .-List_pop_index_node
	.align	2
	.global	List_pop_head_node
	.type	List_pop_head_node, %function
List_pop_head_node:
	stp	x29, x30, [sp, -16]!
	mov	w1, 0
	mov	x29, sp
	bl	List_pop_index_node
	ldp	x29, x30, [sp], 16
	ret
	.size	List_pop_head_node, .-List_pop_head_node
	.align	2
	.global	List_get_gc_head_node
	.type	List_get_gc_head_node, %function
List_get_gc_head_node:
	and	w2, w0, 65535
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	ldr	x1, [x0, 2568]
	cbz	x1, .L142
	mov	w4, 6
	ldr	x3, [x0, 2560]
	mov	w0, 65535
.L139:
	cbz	w2, .L140
	ldrh	w1, [x1]
	cmp	w1, w0
	bne	.L141
.L137:
	ret
.L141:
	sub	w2, w2, #1
	umaddl	x1, w1, w4, x3
	and	w2, w2, 65535
	b	.L139
.L142:
	mov	w0, 65535
	b	.L137
.L140:
	sub	x0, x1, x3
	mov	x1, -6148914691236517206
	movk	x1, 0xaaab, lsl 0
	asr	x0, x0, 1
	mul	x0, x0, x1
	and	w0, w0, 65535
	b	.L137
	.size	List_get_gc_head_node, .-List_get_gc_head_node
	.align	2
	.global	List_update_data_list
	.type	List_update_data_list, %function
List_update_data_list:
	stp	x29, x30, [sp, -64]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	ldrh	w1, [x19, 24]
	stp	x23, x24, [sp, 48]
	cmp	w1, w0, uxth
	beq	.L145
	and	w20, w0, 65535
	ldrh	w0, [x19, 80]
	cmp	w0, w20
	beq	.L145
	ldrh	w0, [x19, 128]
	cmp	w0, w20
	beq	.L145
	mov	w22, 6
	ldr	x24, [x19, 2560]
	umull	x22, w20, w22
	ldr	x0, [x19, 2568]
	add	x23, x24, x22
	cmp	x23, x0
	beq	.L145
	ldr	x1, [x19, 72]
	ubfiz	x0, x20, 1, 16
	ldrh	w21, [x1, x0]
	mov	w1, 65535
	ldrh	w0, [x23, 4]
	cmp	w0, 0
	mul	w21, w21, w0
	ldrh	w0, [x23, 2]
	csinv	w21, w21, wzr, ne
	cmp	w0, w1
	bne	.L148
	ldrh	w1, [x24, x22]
	cmp	w1, w0
	bne	.L148
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 51
	add	x0, x0, :lo12:.LC8
	mov	w2, 463
	bl	sftl_printk
.L148:
	ldrh	w0, [x23, 2]
	mov	w1, 65535
	cmp	w0, w1
	bne	.L149
	ldrh	w1, [x24, x22]
	cmp	w1, w0
	beq	.L145
.L149:
	mov	w1, 6
	mov	x2, -6148914691236517206
	movk	x2, 0xaaab, lsl 0
	umull	x0, w0, w1
	asr	x1, x0, 1
	mul	x1, x1, x2
	ldr	x2, [x19, 72]
	ldrh	w1, [x2, x1, lsl 1]
	ldr	x2, [x19, 2560]
	add	x0, x2, x0
	ldrh	w2, [x0, 4]
	cmp	w2, 0
	mul	w0, w1, w2
	csinv	w0, w0, wzr, ne
	cmp	w21, w0
	bcs	.L145
	mov	w1, w20
	add	x0, x19, 2568
	bl	List_remove_node
	ldrh	w0, [x19, 2584]
	cbnz	w0, .L151
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 51
	add	x0, x0, :lo12:.LC8
	mov	w2, 474
	bl	sftl_printk
.L151:
	ldrh	w0, [x19, 2584]
	sub	w0, w0, #1
	strh	w0, [x19, 2584]
	mov	w0, w20
	bl	INSERT_DATA_LIST
.L145:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x29, x30, [sp], 64
	ret
	.size	List_update_data_list, .-List_update_data_list
	.align	2
	.global	select_l2p_ram_region
	.type	select_l2p_ram_region, %function
select_l2p_ram_region:
	stp	x29, x30, [sp, -32]!
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	mov	x29, sp
	str	x19, [sp, 16]
	mov	x3, 0
	mov	w5, 65535
	ldr	x0, [x2, 2608]
	ldrh	w1, [x2, 342]
	sub	x4, x0, #16
.L156:
	and	w19, w3, 65535
	cmp	w1, w3, uxth
	bhi	.L158
	mov	w19, w1
	add	x7, x0, 4
	mov	x3, 0
	mov	w5, -2147483648
.L159:
	and	w6, w3, 65535
	cmp	w1, w3, uxth
	bhi	.L161
	cmp	w1, w19
	bhi	.L157
	mov	w19, w1
	ldrh	w5, [x2, 2616]
	mov	w3, -1
	mov	w2, 0
.L162:
	cmp	w1, w2
	bne	.L164
	cmp	w1, w19
	bhi	.L157
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	add	x1, x1, 73
	mov	w2, 796
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
	b	.L157
.L158:
	add	x3, x3, 1
	lsl	x6, x3, 4
	ldrh	w6, [x4, x6]
	cmp	w6, w5
	bne	.L156
.L157:
	mov	w0, w19
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L161:
	lsl	x4, x3, 4
	ldr	w4, [x7, x4]
	tbnz	w4, #31, .L160
	cmp	w4, w5
	bcs	.L160
	mov	w5, w4
	mov	w19, w6
.L160:
	add	x3, x3, 1
	b	.L159
.L164:
	ldr	w4, [x0, 4]
	cmp	w4, w3
	bcs	.L163
	ldrh	w6, [x0]
	cmp	w6, w5
	csel	w3, w3, w4, eq
	csel	w19, w19, w2, eq
.L163:
	add	w2, w2, 1
	add	x0, x0, 16
	and	w2, w2, 65535
	b	.L162
	.size	select_l2p_ram_region, .-select_l2p_ram_region
	.align	2
	.global	FtlUpdateVaildLpn
	.type	FtlUpdateVaildLpn, %function
FtlUpdateVaildLpn:
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	ldrh	w2, [x1, 2618]
	cmp	w2, 4
	bhi	.L167
	cbnz	w0, .L167
	add	w2, w2, 1
	strh	w2, [x1, 2618]
.L166:
	ret
.L167:
	ldrh	w4, [x1, 244]
	mov	x0, 0
	ldr	x5, [x1, 72]
	mov	w6, 65535
	strh	wzr, [x1, 2618]
	str	wzr, [x1, 2620]
.L168:
	cmp	w4, w0, uxth
	bls	.L166
	ldrh	w3, [x5, x0, lsl 1]
	cmp	w3, w6
	beq	.L169
	ldr	w2, [x1, 2620]
	add	w2, w2, w3
	str	w2, [x1, 2620]
.L169:
	add	x0, x0, 1
	b	.L168
	.size	FtlUpdateVaildLpn, .-FtlUpdateVaildLpn
	.align	2
	.global	ftl_sb_update_avl_pages
	.type	ftl_sb_update_avl_pages, %function
ftl_sb_update_avl_pages:
	adrp	x3, .LANCHOR0
	add	x3, x3, :lo12:.LANCHOR0
	and	w1, w1, 65535
	and	w2, w2, 65535
	mov	w6, 65535
	strh	wzr, [x0, 4]
	ldrh	w5, [x3, 236]
.L173:
	cmp	w5, w2, uxth
	bhi	.L175
	ldrh	w2, [x3, 306]
	add	x4, x0, 16
	mov	w6, 65535
	sub	w2, w2, w1
	mov	x1, 0
	sub	w2, w2, #1
	sxth	w2, w2
.L176:
	cmp	w5, w1, uxth
	bhi	.L178
	ret
.L175:
	add	x4, x0, w2, sxtw 1
	ldrh	w4, [x4, 16]
	cmp	w4, w6
	beq	.L174
	ldrh	w4, [x0, 4]
	add	w4, w4, 1
	strh	w4, [x0, 4]
.L174:
	add	w2, w2, 1
	b	.L173
.L178:
	ldrh	w3, [x4, x1, lsl 1]
	cmp	w3, w6
	beq	.L177
	ldrh	w3, [x0, 4]
	add	w3, w2, w3
	strh	w3, [x0, 4]
.L177:
	add	x1, x1, 1
	b	.L176
	.size	ftl_sb_update_avl_pages, .-ftl_sb_update_avl_pages
	.align	2
	.global	FtlSlcSuperblockCheck
	.type	FtlSlcSuperblockCheck, %function
FtlSlcSuperblockCheck:
	ldrh	w1, [x0, 4]
	cbz	w1, .L179
	ldrh	w1, [x0]
	mov	w2, 65535
	cmp	w1, w2
	beq	.L179
	ldrb	w1, [x0, 6]
	adrp	x3, .LANCHOR0+236
	add	x1, x1, 8
	ldrh	w3, [x3, #:lo12:.LANCHOR0+236]
	ldrh	w1, [x0, x1, lsl 1]
.L182:
	cmp	w1, w2
	beq	.L184
.L179:
	ret
.L184:
	ldrb	w1, [x0, 6]
	add	w1, w1, 1
	and	w1, w1, 255
	strb	w1, [x0, 6]
	cmp	w1, w3
	bne	.L183
	ldrh	w1, [x0, 2]
	strb	wzr, [x0, 6]
	add	w1, w1, 1
	strh	w1, [x0, 2]
.L183:
	ldrb	w1, [x0, 6]
	add	x1, x1, 8
	ldrh	w1, [x0, x1, lsl 1]
	b	.L182
	.size	FtlSlcSuperblockCheck, .-FtlSlcSuperblockCheck
	.align	2
	.global	make_superblock
	.type	make_superblock, %function
make_superblock:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	ldrh	w1, [x0]
	mov	x19, x0
	ldrh	w0, [x20, 244]
	cmp	w1, w0
	bcc	.L186
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 95
	add	x0, x0, :lo12:.LC8
	mov	w2, 2157
	bl	sftl_printk
.L186:
	ldrh	w8, [x20, 236]
	add	x7, x19, 16
	add	x9, x20, 264
	mov	x6, 0
	mov	w10, -1
	strh	wzr, [x19, 4]
	strb	wzr, [x19, 7]
.L187:
	ldrh	w1, [x19]
	ldrb	w5, [x19, 7]
	cmp	w8, w6, uxth
	bhi	.L189
	ldrh	w0, [x20, 306]
	ubfiz	x1, x1, 1, 16
	strb	wzr, [x19, 9]
	mul	w5, w5, w0
	strh	w5, [x19, 4]
	ldr	x0, [x20, 2600]
	ldrh	w1, [x0, x1]
	mov	w0, 10000
	cmp	w1, w0
	cset	w0, hi
	strb	w0, [x19, 9]
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L189:
	ldrb	w0, [x9, x6]
	bl	V2P_block
	strh	w10, [x7]
	mov	w4, w0
	bl	FtlBbmIsBadBlock
	cbnz	w0, .L188
	strh	w4, [x7]
	add	w5, w5, 1
	strb	w5, [x19, 7]
.L188:
	add	x6, x6, 1
	add	x7, x7, 2
	b	.L187
	.size	make_superblock, .-make_superblock
	.align	2
	.global	update_multiplier_value
	.type	update_multiplier_value, %function
update_multiplier_value:
	adrp	x6, .LANCHOR0
	add	x6, x6, :lo12:.LANCHOR0
	mov	x8, 0
	and	w7, w0, 65535
	add	x11, x6, 264
	mov	w5, 0
	ldrh	w9, [x6, 236]
	ldrh	w10, [x6, 306]
	cmp	w9, w8, uxth
	bhi	.L205
	cbz	w5, .L204
	mov	w0, 32768
	udiv	w5, w0, w5
.L204:
	ldr	x0, [x6, 2560]
	mov	w1, 6
	umaddl	x7, w7, w1, x0
	mov	w0, 0
	strh	w5, [x7, 4]
	ret
.L205:
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
.L195:
	ldrb	w0, [x11, x8]
	mov	w1, w7
	bl	V2P_block
	bl	FtlBbmIsBadBlock
	cbnz	w0, .L194
	add	w5, w10, w5
	and	w5, w5, 65535
.L194:
	add	x8, x8, 1
	cmp	w9, w8, uxth
	bhi	.L195
	cbz	w5, .L196
	mov	w0, 32768
	udiv	w5, w0, w5
.L196:
	ldr	x0, [x6, 2560]
	mov	w1, 6
	umaddl	x7, w7, w1, x0
	mov	w0, 0
	strh	w5, [x7, 4]
	ldp	x29, x30, [sp], 16
	ret
	.size	update_multiplier_value, .-update_multiplier_value
	.align	2
	.global	GetFreeBlockMinEraseCount
	.type	GetFreeBlockMinEraseCount, %function
GetFreeBlockMinEraseCount:
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	ldr	x0, [x1, 2592]
	cbz	x0, .L208
	ldr	x2, [x1, 2560]
	ldr	x1, [x1, 2600]
	sub	x0, x0, x2
	mov	x2, -6148914691236517206
	asr	x0, x0, 1
	movk	x2, 0xaaab, lsl 0
	mul	x0, x0, x2
	and	x0, x0, 65535
	ldrh	w0, [x1, x0, lsl 1]
.L207:
	ret
.L208:
	mov	w0, 0
	b	.L207
	.size	GetFreeBlockMinEraseCount, .-GetFreeBlockMinEraseCount
	.align	2
	.global	GetFreeBlockMaxEraseCount
	.type	GetFreeBlockMaxEraseCount, %function
GetFreeBlockMaxEraseCount:
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	and	w0, w0, 65535
	ldr	x1, [x2, 2592]
	cbz	x1, .L215
	ldrh	w3, [x2, 228]
	mov	w4, 7
	mul	w3, w3, w4
	asr	w3, w3, 3
	cmp	w0, w3
	ble	.L211
	and	w0, w3, 65535
.L211:
	ldr	x4, [x2, 2560]
	mov	x3, -6148914691236517206
	movk	x3, 0xaaab, lsl 0
	mov	w6, 6
	sub	x1, x1, x4
	mov	w7, 65535
	asr	x1, x1, 1
	mul	x1, x1, x3
	mov	w3, 0
	and	w1, w1, 65535
.L212:
	cmp	w0, w3
	bne	.L214
.L213:
	ldr	x0, [x2, 2600]
	ubfiz	x1, x1, 1, 16
	ldrh	w0, [x0, x1]
.L210:
	ret
.L214:
	umull	x5, w1, w6
	ldrh	w5, [x4, x5]
	cmp	w5, w7
	beq	.L213
	add	w3, w3, 1
	mov	w1, w5
	and	w3, w3, 65535
	b	.L212
.L215:
	mov	w0, 0
	b	.L210
	.size	GetFreeBlockMaxEraseCount, .-GetFreeBlockMaxEraseCount
	.section	.rodata.str1.1
.LC9:
	.string	"FLASH INFO:\n"
.LC10:
	.string	"Device Capacity: %d MB\n"
.LC11:
	.string	"FTL INFO:\n"
.LC12:
	.string	"g_MaxLpn = 0x%x\n"
.LC13:
	.string	"g_VaildLpn = 0x%x\n"
.LC14:
	.string	"read_page_count = 0x%x\n"
.LC15:
	.string	"discard_page_count = 0x%x\n"
.LC16:
	.string	"write_page_count = 0x%x\n"
.LC17:
	.string	"cache_write_count = 0x%x\n"
.LC18:
	.string	"l2p_write_count = 0x%x\n"
.LC19:
	.string	"gc_page_count = 0x%x\n"
.LC20:
	.string	"totle_write = %d MB\n"
.LC21:
	.string	"totle_read = %d MB\n"
.LC22:
	.string	"GSV = 0x%x\n"
.LC23:
	.string	"GDV = 0x%x\n"
.LC24:
	.string	"bad blk num = %d\n"
.LC25:
	.string	"free_superblocks = 0x%x\n"
.LC26:
	.string	"mlc_EC = 0x%x\n"
.LC27:
	.string	"slc_EC = 0x%x\n"
.LC28:
	.string	"avg_EC = 0x%x\n"
.LC29:
	.string	"sys_EC = 0x%x\n"
.LC30:
	.string	"max_EC = 0x%x\n"
.LC31:
	.string	"min_EC = 0x%x\n"
.LC32:
	.string	"PLT = 0x%x\n"
.LC33:
	.string	"POT = 0x%x\n"
.LC34:
	.string	"MaxSector = 0x%x\n"
.LC35:
	.string	"init_sys_blks_pp = 0x%x\n"
.LC36:
	.string	"sys_blks_pp = 0x%x\n"
.LC37:
	.string	"free sysblock = 0x%x\n"
.LC38:
	.string	"data_blks_pp = 0x%x\n"
.LC39:
	.string	"data_op_blks_pp = 0x%x\n"
.LC40:
	.string	"max_data_blks = 0x%x\n"
.LC41:
	.string	"Sys.id = 0x%x\n"
.LC42:
	.string	"Bbt.id = 0x%x\n"
.LC43:
	.string	"ACT.page = 0x%x\n"
.LC44:
	.string	"ACT.plane = 0x%x\n"
.LC45:
	.string	"ACT.id = 0x%x\n"
.LC46:
	.string	"ACT.mode = 0x%x\n"
.LC47:
	.string	"ACT.a_pages = 0x%x\n"
.LC48:
	.string	"ACT VPC = 0x%x\n"
.LC49:
	.string	"BUF.page = 0x%x\n"
.LC50:
	.string	"BUF.plane = 0x%x\n"
.LC51:
	.string	"BUF.id = 0x%x\n"
.LC52:
	.string	"BUF.mode = 0x%x\n"
.LC53:
	.string	"BUF.a_pages = 0x%x\n"
.LC54:
	.string	"BUF VPC = 0x%x\n"
.LC55:
	.string	"TMP.page = 0x%x\n"
.LC56:
	.string	"TMP.plane = 0x%x\n"
.LC57:
	.string	"TMP.id = 0x%x\n"
.LC58:
	.string	"TMP.mode = 0x%x\n"
.LC59:
	.string	"TMP.a_pages = 0x%x\n"
.LC60:
	.string	"GC.page = 0x%x\n"
.LC61:
	.string	"GC.plane = 0x%x\n"
.LC62:
	.string	"GC.id = 0x%x\n"
.LC63:
	.string	"GC.mode = 0x%x\n"
.LC64:
	.string	"GC.a_pages = 0x%x\n"
.LC65:
	.string	"WR_CHK = %x %x %x\n"
.LC66:
	.string	"Read Err Cnt = 0x%x\n"
.LC67:
	.string	"Prog Err Cnt = 0x%x\n"
.LC68:
	.string	"gc_free_blk_th= 0x%x\n"
.LC69:
	.string	"gc_merge_free_blk_th= 0x%x\n"
.LC70:
	.string	"gc_skip_write_count= 0x%x\n"
.LC71:
	.string	"gc_blk_index= 0x%x\n"
.LC72:
	.string	"free min EC= 0x%x\n"
.LC73:
	.string	"free max EC= 0x%x\n"
.LC74:
	.string	"GC__SB VPC = 0x%x\n"
.LC75:
	.string	"%d. [0x%x]=0x%x 0x%x  0x%x\n"
.LC76:
	.string	"free %d. [0x%x] 0x%x  0x%x\n"
	.text
	.align	2
	.global	FtlPrintInfo2buf
	.type	FtlPrintInfo2buf, %function
FtlPrintInfo2buf:
	stp	x29, x30, [sp, -80]!
	adrp	x1, .LC9
	add	x1, x1, :lo12:.LC9
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	x21, x0
	add	x20, x21, 12
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	bl	strcpy
	ldr	w3, [x19, 320]
	adrp	x2, .LC10
	add	x2, x2, :lo12:.LC10
	mov	x0, x20
	mov	x1, 64
	bl	snprintf
	add	x20, x20, w0, sxtw
	mov	x0, x20
	adrp	x1, .LC11
	add	x1, x1, :lo12:.LC11
	bl	strcpy
	ldr	w3, [x19, 2624]
	add	x20, x20, 10
	mov	x0, x20
	mov	x1, 64
	adrp	x2, .LC12
	add	x2, x2, :lo12:.LC12
	bl	snprintf
	add	x22, x20, w0, sxtw
	ldr	w3, [x19, 2620]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC13
	add	x2, x2, :lo12:.LC13
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2628]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC14
	add	x2, x2, :lo12:.LC14
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2632]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC15
	add	x2, x2, :lo12:.LC15
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2636]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC16
	add	x2, x2, :lo12:.LC16
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2640]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC17
	add	x2, x2, :lo12:.LC17
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2644]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC18
	add	x2, x2, :lo12:.LC18
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2648]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC19
	add	x2, x2, :lo12:.LC19
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2652]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC20
	add	x2, x2, :lo12:.LC20
	lsr	w3, w3, 11
	bl	snprintf
	ldr	w3, [x19, 2656]
	add	x22, x22, w0, sxtw
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC21
	add	x2, x2, :lo12:.LC21
	lsr	w3, w3, 11
	bl	snprintf
	ldr	w3, [x19, 2660]
	add	x22, x22, w0, sxtw
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC22
	add	x2, x2, :lo12:.LC22
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2664]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC23
	add	x2, x2, :lo12:.LC23
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 366]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC24
	add	x2, x2, :lo12:.LC24
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 228]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC25
	add	x2, x2, :lo12:.LC25
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2668]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC26
	add	x2, x2, :lo12:.LC26
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2672]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC27
	add	x2, x2, :lo12:.LC27
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2676]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC28
	add	x2, x2, :lo12:.LC28
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2680]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC29
	add	x2, x2, :lo12:.LC29
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2684]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC30
	add	x2, x2, :lo12:.LC30
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2688]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC31
	add	x2, x2, :lo12:.LC31
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 2542]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC32
	add	x2, x2, :lo12:.LC32
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 2540]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC33
	add	x2, x2, :lo12:.LC33
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 344]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC34
	add	x2, x2, :lo12:.LC34
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 336]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC35
	add	x2, x2, :lo12:.LC35
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 232]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC36
	add	x2, x2, :lo12:.LC36
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 462]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC37
	add	x2, x2, :lo12:.LC37
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 244]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC38
	add	x2, x2, :lo12:.LC38
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 2692]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC39
	add	x2, x2, :lo12:.LC39
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 248]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC40
	add	x2, x2, :lo12:.LC40
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 2696]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC41
	add	x2, x2, :lo12:.LC41
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 360]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC42
	add	x2, x2, :lo12:.LC42
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 26]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC43
	add	x2, x2, :lo12:.LC43
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 30]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC44
	add	x2, x2, :lo12:.LC44
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 24]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC45
	add	x2, x2, :lo12:.LC45
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 32]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC46
	add	x2, x2, :lo12:.LC46
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 28]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC47
	add	x2, x2, :lo12:.LC47
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w1, [x19, 24]
	adrp	x2, .LC48
	ldr	x0, [x19, 72]
	add	x2, x2, :lo12:.LC48
	ldrh	w3, [x0, x1, lsl 1]
	mov	x0, x22
	mov	x1, 64
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 82]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC49
	add	x2, x2, :lo12:.LC49
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 86]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC50
	add	x2, x2, :lo12:.LC50
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 80]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC51
	add	x2, x2, :lo12:.LC51
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 88]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC52
	add	x2, x2, :lo12:.LC52
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 84]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC53
	add	x2, x2, :lo12:.LC53
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w1, [x19, 80]
	adrp	x2, .LC54
	ldr	x0, [x19, 72]
	add	x2, x2, :lo12:.LC54
	ldrh	w3, [x0, x1, lsl 1]
	mov	x0, x22
	mov	x1, 64
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 130]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC55
	add	x2, x2, :lo12:.LC55
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 134]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC56
	add	x2, x2, :lo12:.LC56
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 128]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC57
	add	x2, x2, :lo12:.LC57
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 136]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC58
	add	x2, x2, :lo12:.LC58
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 132]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC59
	add	x2, x2, :lo12:.LC59
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 178]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC60
	add	x2, x2, :lo12:.LC60
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 182]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC61
	add	x2, x2, :lo12:.LC61
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 176]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC62
	add	x2, x2, :lo12:.LC62
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrb	w3, [x19, 184]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC63
	add	x2, x2, :lo12:.LC63
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldrh	w3, [x19, 180]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC64
	add	x2, x2, :lo12:.LC64
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w5, [x19, 2788]
	mov	x0, x22
	ldr	w4, [x19, 2796]
	mov	x1, 64
	ldr	w3, [x19, 2792]
	adrp	x2, .LC65
	add	x2, x2, :lo12:.LC65
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2784]
	mov	x0, x22
	mov	x1, 64
	adrp	x2, .LC66
	add	x2, x2, :lo12:.LC66
	bl	snprintf
	add	x22, x22, w0, sxtw
	ldr	w3, [x19, 2808]
	mov	x1, 64
	mov	x0, x22
	adrp	x2, .LC67
	add	x2, x2, :lo12:.LC67
	bl	snprintf
	add	x20, x22, w0, sxtw
	ldrh	w3, [x19, 3224]
	mov	x0, x20
	mov	x1, 64
	adrp	x2, .LC68
	add	x2, x2, :lo12:.LC68
	bl	snprintf
	add	x20, x20, w0, sxtw
	ldrh	w3, [x19, 3226]
	mov	x0, x20
	mov	x1, 64
	adrp	x2, .LC69
	add	x2, x2, :lo12:.LC69
	bl	snprintf
	add	x20, x20, w0, sxtw
	ldr	w3, [x19, 3228]
	mov	x0, x20
	mov	x1, 64
	adrp	x2, .LC70
	add	x2, x2, :lo12:.LC70
	bl	snprintf
	add	x20, x20, w0, sxtw
	ldrh	w3, [x19, 3232]
	adrp	x2, .LC71
	add	x2, x2, :lo12:.LC71
	mov	x1, 64
	mov	x0, x20
	bl	snprintf
	add	x20, x20, w0, sxtw
	bl	GetFreeBlockMinEraseCount
	and	w3, w0, 65535
	mov	x1, 64
	mov	x0, x20
	adrp	x2, .LC72
	add	x2, x2, :lo12:.LC72
	bl	snprintf
	add	x20, x20, w0, sxtw
	ldrh	w0, [x19, 228]
	bl	GetFreeBlockMaxEraseCount
	and	w3, w0, 65535
	adrp	x2, .LC73
	mov	x0, x20
	add	x2, x2, :lo12:.LC73
	mov	x1, 64
	bl	snprintf
	add	x20, x20, w0, sxtw
	adrp	x0, .LANCHOR2
	ldr	w0, [x0, #:lo12:.LANCHOR2]
	cmp	w0, 1
	beq	.L217
.L222:
	sub	w0, w20, w21
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
.L217:
	ldrh	w0, [x19, 176]
	mov	w1, 65535
	cmp	w0, w1
	beq	.L219
	ldr	x1, [x19, 72]
	ubfiz	x0, x0, 1, 16
	adrp	x2, .LC74
	add	x2, x2, :lo12:.LC74
	ldrh	w3, [x1, x0]
	mov	x0, x20
	mov	x1, 64
	bl	snprintf
	add	x20, x20, w0, sxtw
.L219:
	mov	w0, 0
	adrp	x23, .LC75
	bl	List_get_gc_head_node
	add	x23, x23, :lo12:.LC75
	and	w4, w0, 65535
	mov	w22, 0
	mov	w26, 65535
	mov	w25, 6
.L221:
	ldr	x0, [x19, 2560]
	cmp	w4, w26
	beq	.L220
	umull	x24, w4, w25
	ubfiz	x1, x4, 1, 16
	ldr	x2, [x19, 72]
	add	x0, x0, x24
	ldr	x3, [x19, 2600]
	ldrh	w5, [x2, x1]
	mov	x2, x23
	ldrh	w6, [x0, 4]
	mov	x0, x20
	ldrh	w7, [x3, x1]
	mov	w3, w22
	mov	x1, 64
	add	w22, w22, 1
	bl	snprintf
	add	x20, x20, w0, sxtw
	ldr	x0, [x19, 2560]
	ldrh	w4, [x0, x24]
	cmp	w22, 16
	bne	.L221
.L220:
	ldr	x0, [x19, 2560]
	adrp	x23, .LC76
	ldr	x4, [x19, 2592]
	add	x23, x23, :lo12:.LC76
	mov	w22, 0
	mov	w25, 65535
	sub	x4, x4, x0
	mov	x0, -6148914691236517206
	movk	x0, 0xaaab, lsl 0
	mov	w26, 6
	asr	x4, x4, 1
	mul	x4, x4, x0
	and	w4, w4, 65535
.L223:
	cmp	w4, w25
	beq	.L222
	ldr	x0, [x19, 2560]
	umull	x24, w4, w26
	ldr	x2, [x19, 2600]
	ubfiz	x1, x4, 1, 16
	add	x0, x0, x24
	mov	w3, w22
	add	w22, w22, 1
	ldrh	w6, [x2, x1]
	mov	x2, x23
	ldrh	w5, [x0, 4]
	mov	x1, 64
	mov	x0, x20
	bl	snprintf
	add	x20, x20, w0, sxtw
	cmp	w22, 4
	beq	.L222
	ldr	x0, [x19, 2560]
	ldrh	w4, [x0, x24]
	b	.L223
	.size	FtlPrintInfo2buf, .-FtlPrintInfo2buf
	.section	.rodata.str1.1
.LC77:
	.string	"%s\n"
	.text
	.align	2
	.global	rknand_proc_ftlread
	.type	rknand_proc_ftlread, %function
rknand_proc_ftlread:
	cmp	w0, 2047
	ble	.L231
	stp	x29, x30, [sp, -32]!
	adrp	x3, .LC0
	adrp	x2, .LC77
	mov	x29, sp
	add	x3, x3, :lo12:.LC0
	add	x2, x2, :lo12:.LC77
	stp	x19, x20, [sp, 16]
	mov	x19, x1
	mov	x1, 64
	add	x20, x19, 30
	mov	x0, x19
	bl	snprintf
	mov	x0, x20
	bl	FtlPrintInfo2buf
	add	x0, x20, w0, sxtw
	sub	w0, w0, w19
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L231:
	mov	w0, 0
	ret
	.size	rknand_proc_ftlread, .-rknand_proc_ftlread
	.section	.rodata.str1.1
.LC78:
	.string	"swblk %x ,avg = %x max= %x vpc= %x,ec=%x ,max ec=%x\n"
	.text
	.align	2
	.global	GetSwlReplaceBlock
	.type	GetSwlReplaceBlock, %function
GetSwlReplaceBlock:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldr	w1, [x19, 2676]
	ldr	w0, [x19, 2688]
	cmp	w1, w0
	bcs	.L237
	ldrh	w1, [x19, 244]
	mov	x2, 0
	ldr	x4, [x19, 2600]
	str	wzr, [x19, 2668]
.L238:
	ldr	w0, [x19, 2668]
	cmp	w1, w2
	bhi	.L239
	udiv	w1, w0, w1
	str	w1, [x19, 2676]
	ldr	w1, [x19, 2672]
	sub	w0, w0, w1
	ldrh	w1, [x19, 296]
	udiv	w0, w0, w1
	str	w0, [x19, 2668]
.L240:
	ldr	w8, [x19, 2688]
	ldr	w9, [x19, 2676]
	add	w0, w8, 256
	cmp	w0, w9
	bls	.L245
	ldr	w1, [x19, 2684]
	add	w0, w8, 768
	cmp	w0, w1
	bls	.L245
.L247:
	mov	w20, 65535
.L246:
	mov	w0, w20
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L239:
	ldrh	w3, [x4, x2, lsl 1]
	add	x2, x2, 1
	add	w0, w3, w0
	str	w0, [x19, 2668]
	b	.L238
.L237:
	ldr	w0, [x19, 2684]
	cmp	w1, w0
	bls	.L240
	add	w0, w0, 1
	str	w0, [x19, 2684]
	mov	x0, 0
.L242:
	ldrh	w1, [x19, 244]
	cmp	w1, w0
	bls	.L240
	ldr	x3, [x19, 2600]
	lsl	x2, x0, 1
	add	x0, x0, 1
	ldrh	w1, [x3, x2]
	add	w1, w1, 1
	strh	w1, [x3, x2]
	b	.L242
.L245:
	ldrh	w0, [x19, 228]
	add	w0, w0, w0, lsl 1
	ubfx	x0, x0, 2, 16
	bl	GetFreeBlockMaxEraseCount
	add	w1, w8, 64
	and	w6, w0, 65535
	cmp	w1, w0, uxth
	bhi	.L247
	ldr	x0, [x19, 2568]
	cbz	x0, .L247
	ldrh	w12, [x19, 244]
	mov	w3, 65535
	ldr	x4, [x19, 2560]
	mov	x13, -6148914691236517206
	ldr	x5, [x19, 2600]
	mov	w1, w3
	mov	w11, w3
	mov	w7, 0
	movk	x13, 0xaaab, lsl 0
	mov	w14, 6
.L248:
	ldrh	w2, [x0]
	cmp	w2, w11
	bne	.L251
	mov	w20, w1
.L250:
	mov	w0, 65535
	cmp	w20, w0
	beq	.L247
	ldrh	w4, [x5, w20, uxtw 1]
	ubfiz	x7, x20, 1, 32
	cmp	w8, w4
	bcs	.L252
	bl	GetFreeBlockMinEraseCount
	cmp	w8, w0, uxth
	bcs	.L252
	str	w3, [x19, 2688]
.L252:
	cmp	w9, w4
	bls	.L247
	add	w0, w4, 128
	cmp	w6, w0
	ble	.L247
	add	w0, w4, 256
	ldr	w3, [x19, 2684]
	cmp	w9, w0
	bhi	.L253
	add	w4, w4, 768
	cmp	w4, w3
	bcs	.L247
.L253:
	ldr	x0, [x19, 72]
	mov	w2, w9
	ldrh	w5, [x5, x7]
	mov	w1, w20
	ldrh	w4, [x0, x7]
	adrp	x0, .LC78
	add	x0, x0, :lo12:.LC78
	bl	sftl_printk
	mov	w0, 1
	str	w0, [x19, 3236]
	b	.L246
.L251:
	add	w10, w7, 1
	and	w7, w10, 65535
	cmp	w12, w10, uxth
	bcc	.L247
	ldrh	w10, [x0, 4]
	cbz	w10, .L249
	sub	x0, x0, x4
	asr	x0, x0, 1
	mul	x0, x0, x13
	and	w20, w0, 65535
	and	x0, x0, 65535
	ldrh	w0, [x5, x0, lsl 1]
	cmp	w8, w0
	bcs	.L250
	cmp	w0, w3
	bcs	.L249
	mov	w3, w0
	mov	w1, w20
.L249:
	umaddl	x0, w2, w14, x4
	b	.L248
	.size	GetSwlReplaceBlock, .-GetSwlReplaceBlock
	.align	2
	.global	free_data_superblock
	.type	free_data_superblock, %function
free_data_superblock:
	mov	w2, 65535
	cmp	w2, w0, uxth
	beq	.L264
	stp	x29, x30, [sp, -16]!
	adrp	x2, .LANCHOR0+72
	and	w1, w0, 65535
	mov	x29, sp
	ldr	x2, [x2, #:lo12:.LANCHOR0+72]
	ubfiz	x0, x1, 1, 16
	strh	wzr, [x2, x0]
	mov	w0, w1
	bl	INSERT_FREE_LIST
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
.L264:
	mov	w0, 0
	ret
	.size	free_data_superblock, .-free_data_superblock
	.align	2
	.global	get_new_active_ppa
	.type	get_new_active_ppa, %function
get_new_active_ppa:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	str	x21, [sp, 32]
	ldrh	w1, [x0]
	mov	w0, 65535
	cmp	w1, w0
	bne	.L268
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 111
	add	x0, x0, :lo12:.LC8
	mov	w2, 2792
	bl	sftl_printk
.L268:
	adrp	x21, .LANCHOR0
	add	x21, x21, :lo12:.LANCHOR0
	ldrh	w1, [x19, 2]
	ldrh	w0, [x21, 306]
	cmp	w1, w0
	bne	.L269
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 111
	add	x0, x0, :lo12:.LC8
	mov	w2, 2793
	bl	sftl_printk
.L269:
	ldrh	w0, [x19, 4]
	cbnz	w0, .L270
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 111
	add	x0, x0, :lo12:.LC8
	mov	w2, 2794
	bl	sftl_printk
.L270:
	ldrb	w0, [x19, 6]
	mov	w2, 65535
	strb	wzr, [x19, 10]
	add	x0, x0, 8
	ldrh	w3, [x21, 236]
	ldrh	w0, [x19, x0, lsl 1]
.L271:
	ldrh	w20, [x19, 2]
	cmp	w0, w2
	beq	.L273
	ldrh	w2, [x21, 306]
	cmp	w2, w20
	bls	.L277
	orr	w20, w20, w0, lsl 10
	ldrh	w0, [x19, 4]
	ldrb	w1, [x19, 6]
	mov	w5, 65535
	sub	w0, w0, #1
	and	w0, w0, 65535
	strh	w0, [x19, 4]
.L276:
	add	w4, w1, 1
	and	w1, w4, 255
	cmp	w3, w4, uxtb
	bne	.L275
	ldrh	w1, [x19, 2]
	add	w1, w1, 1
	strh	w1, [x19, 2]
	mov	w1, 0
.L275:
	add	x4, x19, w1, sxtw 1
	ldrh	w4, [x4, 16]
	cmp	w4, w5
	beq	.L276
	strb	w1, [x19, 6]
	ldrh	w1, [x19, 2]
	cmp	w1, w2
	bne	.L267
	cbz	w0, .L267
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 111
	add	x0, x0, :lo12:.LC8
	mov	w2, 2817
	bl	sftl_printk
.L267:
	mov	w0, w20
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L273:
	ldrb	w1, [x19, 6]
	add	w1, w1, 1
	and	w1, w1, 255
	strb	w1, [x19, 6]
	cmp	w1, w3
	bne	.L272
	add	w20, w20, 1
	strh	w20, [x19, 2]
	strb	wzr, [x19, 6]
.L272:
	ldrb	w0, [x19, 6]
	add	x0, x0, 8
	ldrh	w0, [x19, x0, lsl 1]
	b	.L271
.L277:
	mov	w20, 65535
	b	.L267
	.size	get_new_active_ppa, .-get_new_active_ppa
	.align	2
	.global	FtlGcBufInit
	.type	FtlGcBufInit, %function
FtlGcBufInit:
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	mov	w2, 0
	mov	w7, 24
	mov	w8, 1
	mov	w4, 4
	str	wzr, [x0, 3240]
.L284:
	ldrh	w1, [x0, 236]
	cmp	w1, w2
	bhi	.L285
	mov	w6, 24
	mov	w5, 4
.L286:
	ldr	w2, [x0, 3280]
	cmp	w1, w2
	bcc	.L287
	ret
.L285:
	ldr	x5, [x0, 3248]
	umull	x3, w2, w7
	ldr	x6, [x0, 3256]
	add	x1, x5, x3
	str	w8, [x1, 16]
	ldrh	w1, [x0, 314]
	mul	w1, w1, w2
	sdiv	w1, w1, w4
	add	x1, x6, w1, sxtw 2
	str	x1, [x5, x3]
	ldrh	w1, [x0, 316]
	ldr	x6, [x0, 3248]
	ldr	x9, [x0, 3264]
	mul	w1, w1, w2
	add	x5, x6, x3
	sdiv	w1, w1, w4
	add	x1, x9, w1, sxtw 2
	str	x1, [x5, 8]
	ldr	x1, [x0, 3272]
	ubfiz	x9, x2, 5, 16
	ldr	x3, [x6, x3]
	add	x1, x1, x9
	add	w2, w2, 1
	and	w2, w2, 65535
	str	x3, [x1, 8]
	ldr	x3, [x5, 8]
	str	x3, [x1, 16]
	b	.L284
.L287:
	ldr	x3, [x0, 3248]
	umull	x4, w1, w6
	ldr	x7, [x0, 3256]
	add	x2, x3, x4
	str	wzr, [x2, 16]
	ldrh	w2, [x0, 314]
	mul	w2, w2, w1
	sdiv	w2, w2, w5
	add	x2, x7, w2, sxtw 2
	str	x2, [x3, x4]
	ldrh	w2, [x0, 316]
	ldr	x3, [x0, 3248]
	mul	w2, w2, w1
	add	x3, x3, x4
	ldr	x4, [x0, 3264]
	add	w1, w1, 1
	and	w1, w1, 65535
	sdiv	w2, w2, w5
	add	x2, x4, w2, sxtw 2
	str	x2, [x3, 8]
	b	.L286
	.size	FtlGcBufInit, .-FtlGcBufInit
	.align	2
	.global	FtlGcBufFree
	.type	FtlGcBufFree, %function
FtlGcBufFree:
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	mov	w3, 0
	mov	w8, 24
	ldr	w7, [x2, 3280]
	ldr	x5, [x2, 3248]
.L289:
	cmp	w3, w1
	bcs	.L288
	ubfiz	x4, x3, 5, 16
	mov	w2, 0
	add	x4, x0, x4
	b	.L294
.L290:
	add	w2, w2, 1
	and	w2, w2, 65535
.L294:
	cmp	w2, w7
	bcs	.L291
	umull	x6, w2, w8
	add	x9, x5, x6
	ldr	x10, [x5, x6]
	ldr	x6, [x4, 8]
	cmp	x10, x6
	bne	.L290
	str	wzr, [x9, 16]
.L291:
	add	w3, w3, 1
	and	w3, w3, 65535
	b	.L289
.L288:
	ret
	.size	FtlGcBufFree, .-FtlGcBufFree
	.align	2
	.global	FtlGcBufAlloc
	.type	FtlGcBufAlloc, %function
FtlGcBufAlloc:
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	mov	w7, 24
	mov	w8, 1
	ldr	w6, [x2, 3280]
	ldr	x5, [x2, 3248]
	mov	w2, 0
.L296:
	cmp	w2, w1
	bcc	.L301
	ret
.L297:
	add	w3, w3, 1
	and	w3, w3, 65535
.L300:
	cmp	w3, w6
	bcs	.L298
	umaddl	x4, w3, w7, x5
	ldr	w9, [x4, 16]
	cbnz	w9, .L297
	ubfiz	x3, x2, 5, 16
	str	w8, [x4, 16]
	add	x3, x0, x3
	ldr	x9, [x4]
	str	x9, [x3, 8]
	ldr	x4, [x4, 8]
	str	x4, [x3, 16]
.L298:
	add	w2, w2, 1
	and	w2, w2, 65535
	b	.L296
.L301:
	mov	w3, 0
	b	.L300
	.size	FtlGcBufAlloc, .-FtlGcBufAlloc
	.align	2
	.global	IsBlkInGcList
	.type	IsBlkInGcList, %function
IsBlkInGcList:
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	and	w0, w0, 65535
	ldr	x2, [x1, 3288]
	ldrh	w3, [x1, 3284]
	mov	x1, 0
	sub	x2, x2, #2
.L303:
	cmp	w3, w1, uxth
	bhi	.L305
	mov	w0, 0
.L302:
	ret
.L305:
	add	x1, x1, 1
	ldrh	w4, [x2, x1, lsl 1]
	cmp	w4, w0
	bne	.L303
	mov	w0, 1
	b	.L302
	.size	IsBlkInGcList, .-IsBlkInGcList
	.align	2
	.global	FtlGcUpdatePage
	.type	FtlGcUpdatePage, %function
FtlGcUpdatePage:
	stp	x29, x30, [sp, -16]!
	mov	w5, w0
	mov	w7, w1
	mov	x29, sp
	mov	w6, w2
	ubfx	x0, x5, 10, 16
	bl	P2V_block_in_plane
	and	w9, w0, 65535
	adrp	x3, .LANCHOR0
	add	x3, x3, :lo12:.LANCHOR0
	mov	x4, 0
	ldr	x1, [x3, 3288]
	ldrh	w8, [x3, 3284]
	sub	x10, x1, #2
.L308:
	and	w2, w4, 65535
	cmp	w8, w2
	bhi	.L310
	bne	.L309
	strh	w0, [x1, x4, lsl 1]
	ldrh	w0, [x3, 3284]
	add	w0, w0, 1
	strh	w0, [x3, 3284]
	b	.L309
.L310:
	add	x4, x4, 1
	ldrh	w2, [x10, x4, lsl 1]
	cmp	w2, w9
	bne	.L308
.L309:
	ldrh	w4, [x3, 3296]
	mov	w0, 12
	umull	x4, w4, w0
	ldr	x0, [x3, 3304]
	add	x0, x0, x4
	str	w7, [x0, 4]
	ldr	x0, [x3, 3304]
	add	x1, x0, x4
	str	w6, [x1, 8]
	str	w5, [x0, x4]
	ldrh	w0, [x3, 3296]
	add	w0, w0, 1
	strh	w0, [x3, 3296]
	ldp	x29, x30, [sp], 16
	ret
	.size	FtlGcUpdatePage, .-FtlGcUpdatePage
	.section	.rodata.str1.1
.LC79:
	.string	"FtlGcRefreshBlock  0x%x\n"
	.text
	.align	2
	.global	FtlGcRefreshBlock
	.type	FtlGcRefreshBlock, %function
FtlGcRefreshBlock:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	str	x19, [sp, 16]
	and	w19, w0, 65535
	mov	w1, w19
	adrp	x0, .LC79
	add	x0, x0, :lo12:.LC79
	bl	sftl_printk
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	ldrh	w3, [x0, 226]
	cmp	w3, w19
	beq	.L313
	ldrh	w2, [x0, 224]
	cmp	w2, w19
	beq	.L313
	mov	w1, 65535
	cmp	w3, w1
	bne	.L314
	strh	w19, [x0, 226]
.L313:
	mov	w0, 0
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L314:
	cmp	w2, w1
	bne	.L313
	strh	w19, [x0, 224]
	b	.L313
	.size	FtlGcRefreshBlock, .-FtlGcRefreshBlock
	.section	.rodata.str1.1
.LC80:
	.string	"FtlGcMarkBadPhyBlk %d 0x%x\n"
	.text
	.align	2
	.global	FtlGcMarkBadPhyBlk
	.type	FtlGcMarkBadPhyBlk, %function
FtlGcMarkBadPhyBlk:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	and	w20, w0, 65535
	str	x21, [sp, 32]
	mov	w0, w20
	bl	P2V_block_in_plane
	ldrh	w1, [x19, 3312]
	mov	w2, w20
	mov	w21, w0
	adrp	x0, .LC80
	add	x0, x0, :lo12:.LC80
	bl	sftl_printk
	mov	w0, w21
	bl	FtlGcRefreshBlock
	ldrh	w0, [x19, 3312]
	add	x2, x19, 3314
	mov	x1, 0
.L317:
	cmp	w0, w1, uxth
	bhi	.L319
	cmp	w0, 15
	bhi	.L318
	add	w1, w0, 1
	add	x19, x19, 3314
	strh	w1, [x19, -2]
	strh	w20, [x19, w0, sxtw 1]
	b	.L318
.L319:
	add	x1, x1, 1
	add	x3, x2, x1, lsl 1
	ldrh	w3, [x3, -2]
	cmp	w3, w20
	bne	.L317
.L318:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
	.size	FtlGcMarkBadPhyBlk, .-FtlGcMarkBadPhyBlk
	.align	2
	.global	FtlGcReFreshBadBlk
	.type	FtlGcReFreshBadBlk, %function
FtlGcReFreshBadBlk:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldrh	w0, [x19, 3312]
	cbz	w0, .L322
	ldrh	w2, [x19, 226]
	mov	w1, 65535
	cmp	w2, w1
	bne	.L322
	ldrh	w1, [x19, 3348]
	cmp	w1, w0
	bcc	.L323
	strh	wzr, [x19, 3348]
.L323:
	ldrh	w1, [x19, 3348]
	add	x0, x19, 3314
	ldrh	w0, [x0, x1, lsl 1]
	bl	P2V_block_in_plane
	bl	FtlGcRefreshBlock
	ldrh	w0, [x19, 3348]
	add	w0, w0, 1
	strh	w0, [x19, 3348]
.L322:
	mov	w0, 0
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlGcReFreshBadBlk, .-FtlGcReFreshBadBlk
	.align	2
	.global	ftl_malloc
	.type	ftl_malloc, %function
ftl_malloc:
	stp	x29, x30, [sp, -16]!
	mov	w1, 3265
	sxtw	x0, w0
	mov	x29, sp
	bl	__kmalloc
	ldp	x29, x30, [sp], 16
	ret
	.size	ftl_malloc, .-ftl_malloc
	.align	2
	.global	ftl_free
	.type	ftl_free, %function
ftl_free:
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	bl	kfree
	ldp	x29, x30, [sp], 16
	ret
	.size	ftl_free, .-ftl_free
	.section	.rodata.str1.1
.LC81:
	.string	"%s %p + 0x%x:"
.LC82:
	.string	"0x%08x,"
.LC83:
	.string	"0x%04x,"
.LC84:
	.string	"0x%02x,"
.LC85:
	.string	"\n"
	.text
	.align	2
	.global	rknand_print_hex
	.type	rknand_print_hex, %function
rknand_print_hex:
	stp	x29, x30, [sp, -96]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x20, x1
	mov	x19, 0
	stp	x21, x22, [sp, 32]
	mov	w22, w2
	mov	w21, 0
	stp	x23, x24, [sp, 48]
	mov	w23, w3
	mov	w24, 0
	stp	x25, x26, [sp, 64]
	adrp	x26, .LC81
	mov	x25, x0
	add	x26, x26, :lo12:.LC81
	stp	x27, x28, [sp, 80]
	adrp	x27, .LC84
	add	x27, x27, :lo12:.LC84
	adrp	x28, .LC83
.L333:
	cmp	w23, w19
	bhi	.L339
	adrp	x0, .LC85
	add	x0, x0, :lo12:.LC85
	bl	sftl_printk
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 96
	ret
.L339:
	cbnz	w21, .L334
	mov	w3, w24
	mov	x2, x20
	mov	x1, x25
	mov	x0, x26
	bl	sftl_printk
.L334:
	cmp	w22, 4
	bne	.L335
	ldr	w1, [x20, x19, lsl 2]
	adrp	x0, .LC82
	add	x0, x0, :lo12:.LC82
.L341:
	add	w21, w21, 1
	bl	sftl_printk
	cmp	w21, 15
	bls	.L338
	mov	w21, 0
	adrp	x0, .LC85
	add	x0, x0, :lo12:.LC85
	bl	sftl_printk
.L338:
	add	x19, x19, 1
	add	w24, w24, w22
	b	.L333
.L335:
	cmp	w22, 2
	bne	.L337
	ldrh	w1, [x20, x19, lsl 1]
	add	x0, x28, :lo12:.LC83
	b	.L341
.L337:
	ldrb	w1, [x20, x19]
	mov	x0, x27
	b	.L341
	.size	rknand_print_hex, .-rknand_print_hex
	.section	.rodata.str1.1
.LC86:
	.string	"%s: addr: %x is in id block!!!!!!!!!!\n"
.LC87:
	.string	"not free: w: d:"
.LC88:
	.string	"not free: w: s:"
	.text
	.align	2
	.global	FlashEraseBlocks
	.type	FlashEraseBlocks, %function
FlashEraseBlocks:
	stp	x29, x30, [sp, -96]!
	mrs	x1, sp_el0
	mov	x29, sp
	stp	x21, x22, [sp, 32]
	adrp	x21, .LANCHOR0
	add	x21, x21, :lo12:.LANCHOR0
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	mov	w0, 32
	stp	x23, x24, [sp, 48]
	mov	x20, x19
	umaddl	x22, w2, w0, x19
	stp	x25, x26, [sp, 64]
	mov	w24, -1
	ldrh	w23, [x21, 12]
	ldr	x3, [x1, 952]
	str	x3, [sp, 88]
	mov	x3, 0
	lsl	w26, w23, 3
.L343:
	cmp	x20, x22
	beq	.L347
	ldr	w0, [x20, 4]
	add	x2, sp, 80
	add	x1, sp, 84
	bl	l2p_addr_tran.isra.0
	ldr	w0, [sp, 80]
	cbnz	w0, .L344
	ldr	w1, [sp, 84]
	cmp	w26, w1
	bls	.L344
	adrp	x20, .LANCHOR1
	add	x20, x20, :lo12:.LANCHOR1
	adrp	x21, .LC86
	add	x20, x20, 130
	add	x21, x21, :lo12:.LC86
.L345:
	cmp	x19, x22
	bne	.L346
	bl	dump_stack
.L347:
	mrs	x0, sp_el0
	ldr	x1, [sp, 88]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L354
	bl	__stack_chk_fail
.L346:
	ldr	w2, [sp, 84]
	mov	w0, -1
	str	w0, [x19]
	mov	x1, x20
	mov	x0, x21
	add	x19, x19, 32
	bl	sftl_printk
	ldr	x1, [x19, -24]
	mov	w3, 16
	mov	w2, 4
	adrp	x0, .LC87
	add	x0, x0, :lo12:.LC87
	bl	rknand_print_hex
	ldr	x1, [x19, -16]
	mov	w3, 4
	adrp	x0, .LC88
	mov	w2, w3
	add	x0, x0, :lo12:.LC88
	bl	rknand_print_hex
	b	.L345
.L344:
	ldr	w1, [sp, 84]
	ldr	x2, [x21, 3360]
	blr	x2
	cmp	w0, 0
	csel	w0, wzr, w24, eq
	str	w0, [x20]
	ldrh	w0, [x21, 14]
	cmp	w0, 4
	bne	.L351
	ldrb	w0, [sp, 80]
	ldr	x2, [x21, 3360]
	ldr	w1, [sp, 84]
	add	w1, w23, w1
	blr	x2
	cbz	w0, .L351
	str	w24, [x20]
.L351:
	add	x20, x20, 32
	b	.L343
.L354:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x29, x30, [sp], 96
	ret
	.size	FlashEraseBlocks, .-FlashEraseBlocks
	.align	2
	.global	FtlFreeSysBlkQueueIn
	.type	FtlFreeSysBlkQueueIn, %function
FtlFreeSysBlkQueueIn:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	str	x21, [sp, 32]
	and	w21, w0, 65535
	sub	w2, w21, #1
	mov	w0, 65533
	stp	x19, x20, [sp, 16]
	cmp	w0, w2, uxth
	bcc	.L360
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldrh	w0, [x19, 462]
	cmp	w0, 1024
	beq	.L360
	and	w1, w1, 65535
	cbz	w1, .L362
	mov	w0, w21
	bl	P2V_block_in_plane
	and	w20, w0, 65535
	lsl	w1, w21, 10
	ldr	x0, [x19, 3400]
	mov	w2, 1
	str	w1, [x0, 4]
	mov	w1, w2
	ldr	x0, [x19, 3400]
	bl	FlashEraseBlocks
	ldr	x2, [x19, 2600]
	ubfiz	x0, x20, 1, 16
	ldrh	w1, [x2, x0]
	add	w1, w1, 1
	strh	w1, [x2, x0]
	ldr	w0, [x19, 2680]
	add	w0, w0, 1
	str	w0, [x19, 2680]
.L362:
	ldrh	w0, [x19, 462]
	add	x19, x19, 456
	ldrh	w1, [x19, 4]
	add	w0, w0, 1
	strh	w0, [x19, 6]
	ldrh	w0, [x19, 4]
	add	x1, x19, x1, lsl 1
	add	w0, w0, 1
	and	w0, w0, 1023
	strh	w0, [x19, 4]
	strh	w21, [x1, 8]
.L360:
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
	.size	FtlFreeSysBlkQueueIn, .-FtlFreeSysBlkQueueIn
	.section	.rodata.str1.1
.LC89:
	.string	"FtlFreeSysBlkQueueOut = %x, free count = %d, error\n"
	.text
	.align	2
	.global	FtlFreeSysBlkQueueOut
	.type	FtlFreeSysBlkQueueOut, %function
FtlFreeSysBlkQueueOut:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldrh	w2, [x19, 462]
	cbz	w2, .L374
	ldrh	w3, [x19, 458]
	add	x0, x19, 456
	ldrh	w1, [x19, 458]
	sub	w2, w2, #1
	strh	w2, [x19, 462]
	mov	w2, 1
	add	x3, x0, x3, lsl 1
	add	w1, w1, 1
	ldr	x0, [x19, 3400]
	and	w1, w1, 1023
	ldrh	w20, [x3, 8]
	strh	w1, [x19, 458]
	lsl	w1, w20, 10
	str	w1, [x0, 4]
	mov	w1, w2
	ldr	x0, [x19, 3400]
	bl	FlashEraseBlocks
	ldr	w0, [x19, 2680]
	sub	w1, w20, #1
	add	w0, w0, 1
	str	w0, [x19, 2680]
	mov	w0, 65533
	cmp	w0, w1, uxth
	bcs	.L372
.L371:
	ldrh	w2, [x19, 462]
	adrp	x0, .LC89
	mov	w1, w20
	add	x0, x0, :lo12:.LC89
	bl	sftl_printk
.L373:
	b	.L373
.L374:
	mov	w20, 65535
	b	.L371
.L372:
	mov	w0, w20
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlFreeSysBlkQueueOut, .-FtlFreeSysBlkQueueOut
	.section	.rodata.str1.1
.LC90:
	.string	"FtlFreeSysBlkQueueOut = %x, free count = %d\n"
	.text
	.align	2
	.global	ftl_map_blk_alloc_new_blk
	.type	ftl_map_blk_alloc_new_blk, %function
ftl_map_blk_alloc_new_blk:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	mov	w20, 0
	str	x21, [sp, 32]
	ldrh	w1, [x0, 10]
	ldr	x0, [x0, 16]
.L377:
	cmp	w1, w20
	beq	.L383
	mov	x21, x0
	ldrh	w2, [x0], 2
	cbnz	w2, .L378
	bl	FtlFreeSysBlkQueueOut
	and	w1, w0, 65535
	sub	w2, w1, #1
	strh	w0, [x21]
	mov	w0, 65533
	cmp	w0, w2, uxth
	bcs	.L379
	adrp	x0, .LANCHOR0+462
	ldrh	w2, [x0, #:lo12:.LANCHOR0+462]
	adrp	x0, .LC90
	add	x0, x0, :lo12:.LC90
	bl	sftl_printk
.L380:
	b	.L380
.L379:
	ldr	w0, [x19, 48]
	strh	w20, [x19]
	add	w0, w0, 1
	str	w0, [x19, 48]
	ldrh	w0, [x19, 8]
	strh	wzr, [x19, 2]
	add	w0, w0, 1
	strh	w0, [x19, 8]
	ldrh	w0, [x19, 10]
	cmp	w0, w20
	bhi	.L381
.L383:
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 147
	add	x0, x0, :lo12:.LC8
	mov	w2, 578
	bl	sftl_printk
.L381:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L378:
	add	w20, w20, 1
	and	w20, w20, 65535
	b	.L377
	.size	ftl_map_blk_alloc_new_blk, .-ftl_map_blk_alloc_new_blk
	.align	2
	.global	ftl_memset
	.type	ftl_memset, %function
ftl_memset:
	stp	x29, x30, [sp, -16]!
	uxtw	x2, w2
	mov	x29, sp
	bl	memset
	ldp	x29, x30, [sp], 16
	ret
	.size	ftl_memset, .-ftl_memset
	.section	.rodata.str1.1
.LC91:
	.string	"%s error allocating memory. return -1\n"
	.text
	.align	2
	.global	FtlMemInit
	.type	FtlMemInit, %function
FtlMemInit:
	stp	x29, x30, [sp, -48]!
	mov	w0, 65535
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	add	x4, x19, 3416
	add	x3, x19, 2676
	add	x2, x19, 2656
	add	x1, x19, 2636
	str	wzr, [x19, 2688]
	stp	wzr, w0, [x4, -8]
	mov	w0, -1
	strh	w0, [x19, 224]
	strh	w0, [x19, 226]
	mov	w0, 32
	stp	wzr, wzr, [x2, -12]
	stp	wzr, wzr, [x3, -8]
	stp	wzr, wzr, [x1, -8]
	stp	wzr, wzr, [x4]
	stp	wzr, wzr, [x1]
	stp	wzr, wzr, [x3, 4]
	stp	wzr, wzr, [x2, 4]
	strh	w0, [x19, 3224]
	mov	w0, 128
	str	x21, [sp, 32]
	strh	w0, [x19, 3226]
	str	wzr, [x19, 3228]
	strh	wzr, [x19, 3232]
	str	wzr, [x19, 3236]
	strh	wzr, [x19, 3312]
	strh	wzr, [x19, 3348]
	ldrh	w0, [x19, 310]
	lsl	w0, w0, 1
	bl	ftl_malloc
	ldrh	w1, [x19, 310]
	str	x0, [x19, 3288]
	mov	w0, 12
	mul	w0, w1, w0
	bl	ftl_malloc
	ldrh	w20, [x19, 236]
	str	x0, [x19, 3304]
	lsl	w21, w20, 7
	lsl	w20, w20, 5
	mov	w0, w21
	bl	ftl_malloc
	str	x0, [x19, 3424]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3432]
	mov	w0, w21
	bl	ftl_malloc
	str	x0, [x19, 3440]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3400]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3272]
	ldrh	w0, [x19, 236]
	ldrh	w20, [x19, 314]
	lsl	w0, w0, 1
	add	w0, w0, 1
	str	w0, [x19, 3280]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3448]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3456]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3464]
	ldr	w0, [x19, 3280]
	mul	w0, w20, w0
	bl	ftl_malloc
	str	x0, [x19, 3256]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3472]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3480]
	ldr	w1, [x19, 3280]
	mov	w0, 24
	mul	w0, w1, w0
	bl	ftl_malloc
	str	x0, [x19, 3248]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3488]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3496]
	ldrh	w0, [x19, 262]
	lsl	w0, w0, 2
	bl	ftl_malloc
	ldrh	w20, [x19, 316]
	str	x0, [x19, 3504]
	ldrh	w0, [x19, 236]
	mul	w20, w20, w0
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3512]
	lsl	w0, w20, 2
	bl	ftl_malloc
	str	x0, [x19, 3520]
	ldrh	w1, [x19, 316]
	ldr	w0, [x19, 3280]
	mul	w0, w1, w0
	bl	ftl_malloc
	str	x0, [x19, 3264]
	ldrh	w0, [x19, 246]
	ubfiz	w1, w0, 1, 15
	ubfiz	w0, w0, 1, 15
	strh	w1, [x19, 3528]
	bl	ftl_malloc
	str	x0, [x19, 3536]
	ldrh	w0, [x19, 3528]
	add	x0, x0, 547
	lsr	x0, x0, 9
	strh	w0, [x19, 3528]
	lsl	w0, w0, 9
	bl	ftl_malloc
	ldrh	w20, [x19, 246]
	str	x0, [x19, 3544]
	add	x0, x0, 32
	str	x0, [x19, 2600]
	lsl	w20, w20, 1
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3552]
	mov	w0, w20
	bl	ftl_malloc
	ldr	w20, [x19, 332]
	str	x0, [x19, 72]
	lsl	w20, w20, 1
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3560]
	mov	w0, w20
	bl	ftl_malloc
	str	x0, [x19, 3568]
	ldrh	w0, [x19, 324]
	lsl	w0, w0, 1
	bl	ftl_malloc
	str	x0, [x19, 352]
	ldrh	w0, [x19, 324]
	lsl	w0, w0, 1
	bl	ftl_malloc
	str	x0, [x19, 3576]
	ldrh	w0, [x19, 324]
	lsl	w0, w0, 2
	bl	ftl_malloc
	str	x0, [x19, 3584]
	ldrh	w0, [x19, 326]
	lsl	w0, w0, 2
	bl	ftl_malloc
	ldrh	w2, [x19, 326]
	mov	w1, 0
	str	x0, [x19, 3592]
	lsl	w2, w2, 2
	bl	ftl_memset
	ldrh	w0, [x19, 340]
	lsl	w0, w0, 2
	bl	ftl_malloc
	str	x0, [x19, 3600]
	ldr	w0, [x19, 332]
	lsl	w0, w0, 2
	bl	ftl_malloc
	str	x0, [x19, 3608]
	ldrh	w0, [x19, 342]
	lsl	w0, w0, 4
	bl	ftl_malloc
	ldrh	w1, [x19, 342]
	str	x0, [x19, 2608]
	ldrh	w0, [x19, 314]
	mul	w0, w1, w0
	bl	ftl_malloc
	str	x0, [x19, 3616]
	ldrh	w1, [x19, 246]
	mov	w0, 6
	mul	w0, w1, w0
	bl	ftl_malloc
	str	x0, [x19, 2560]
	ldrh	w0, [x19, 302]
	ldrh	w1, [x19, 258]
	add	w0, w0, 31
	asr	w0, w0, 5
	strh	w0, [x19, 3624]
	mul	w0, w1, w0
	lsl	w0, w0, 2
	bl	ftl_malloc
	ldrh	w3, [x19, 3624]
	add	x4, x19, 360
	ldrh	w1, [x19, 258]
	mov	x20, x0
	mov	w2, w3
	str	x0, [x4, 32]
	mov	x0, 0
.L388:
	add	x0, x0, 1
	cmp	w1, w0
	bhi	.L389
	cmp	w1, 0
	mov	w2, 8
	csinc	w1, w1, wzr, ne
	add	x0, x19, 360
	sub	w2, w2, w1
	add	w1, w1, 4
	ubfiz	x3, x1, 3, 17
	lsl	x2, x2, 3
	add	x0, x0, x3
	mov	w1, 0
	bl	memset
	ldr	x0, [x19, 3560]
	cbnz	x0, .L390
.L392:
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	add	x1, x1, 173
	adrp	x0, .LC91
	add	x0, x0, :lo12:.LC91
	bl	sftl_printk
	mov	w0, -1
.L387:
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L389:
	add	x5, x4, x0, lsl 3
	add	x6, x20, w2, uxtw 2
	add	w2, w2, w3
	str	x6, [x5, 32]
	b	.L388
.L390:
	ldr	x0, [x19, 3568]
	cbz	x0, .L392
	ldr	x0, [x19, 3600]
	cbz	x0, .L392
	ldr	x0, [x19, 3608]
	cbz	x0, .L392
	ldr	x0, [x19, 2608]
	cbz	x0, .L392
	ldr	x0, [x19, 3616]
	cbz	x0, .L392
	ldr	x0, [x19, 2560]
	cbz	x0, .L392
	cbz	x20, .L392
	ldr	x0, [x19, 72]
	cbz	x0, .L392
	ldr	x0, [x19, 3288]
	cbz	x0, .L392
	ldr	x0, [x19, 3304]
	cbz	x0, .L392
	ldr	x0, [x19, 3424]
	cbz	x0, .L392
	ldr	x0, [x19, 3440]
	cbz	x0, .L392
	ldr	x0, [x19, 3400]
	cbz	x0, .L392
	ldr	x0, [x19, 3272]
	cbz	x0, .L392
	ldr	x0, [x19, 3432]
	cbz	x0, .L392
	ldr	x0, [x19, 3448]
	cbz	x0, .L392
	ldr	x0, [x19, 3456]
	cbz	x0, .L392
	ldr	x0, [x19, 3464]
	cbz	x0, .L392
	ldr	x0, [x19, 3256]
	cbz	x0, .L392
	ldr	x0, [x19, 3472]
	cbz	x0, .L392
	ldr	x0, [x19, 3480]
	cbz	x0, .L392
	ldr	x0, [x19, 3248]
	cbz	x0, .L392
	ldr	x0, [x19, 3512]
	cbz	x0, .L392
	ldr	x0, [x19, 3520]
	cbz	x0, .L392
	ldr	x0, [x19, 3264]
	cbz	x0, .L392
	ldr	x0, [x19, 2600]
	cbz	x0, .L392
	ldr	x0, [x19, 3536]
	cbz	x0, .L392
	ldr	x0, [x19, 352]
	cbz	x0, .L392
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	ldr	x1, [x0, 3576]
	cbz	x1, .L392
	ldr	x1, [x0, 3584]
	cbz	x1, .L392
	ldr	x0, [x0, 3592]
	cbz	x0, .L392
	mov	w0, 0
	b	.L387
	.size	FtlMemInit, .-FtlMemInit
	.align	2
	.global	FtlBbt2Bitmap
	.type	FtlBbt2Bitmap, %function
FtlBbt2Bitmap:
	stp	x29, x30, [sp, -64]!
	mov	x29, sp
	stp	x23, x24, [sp, 48]
	adrp	x23, .LANCHOR0
	add	x23, x23, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	adrp	x21, .LANCHOR1
	add	x21, x21, :lo12:.LANCHOR1
	ldrh	w2, [x23, 3624]
	adrp	x24, .LC8
	add	x21, x21, 184
	add	x24, x24, :lo12:.LC8
	stp	x19, x20, [sp, 16]
	mov	x19, 0
	mov	x20, x1
	mov	x22, x0
	lsl	w2, w2, 2
	mov	x0, x20
	mov	w1, 0
	bl	ftl_memset
.L490:
	ldrh	w0, [x22, x19, lsl 1]
	mov	w1, 65535
	cmp	w0, w1
	beq	.L487
	ldrh	w1, [x23, 302]
	cmp	w1, w0
	bhi	.L489
	mov	x1, x21
	mov	x0, x24
	mov	w2, 74
	bl	sftl_printk
.L489:
	ldrh	w3, [x22, x19, lsl 1]
	mov	w2, 1
	add	x19, x19, 1
	ubfx	x1, x3, 5, 11
	lsl	w3, w2, w3
	lsl	x1, x1, 2
	ldr	w2, [x20, x1]
	orr	w2, w2, w3
	str	w2, [x20, x1]
	cmp	x19, 512
	bne	.L490
.L487:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x29, x30, [sp], 64
	ret
	.size	FtlBbt2Bitmap, .-FtlBbt2Bitmap
	.align	2
	.global	FtlBbtMemInit
	.type	FtlBbtMemInit, %function
FtlBbtMemInit:
	stp	x29, x30, [sp, -16]!
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	mov	x29, sp
	mov	w1, -1
	mov	w2, 16
	add	x0, x0, 372
	strh	w1, [x0, -12]
	mov	w1, 255
	strh	wzr, [x0, -6]
	bl	ftl_memset
	ldp	x29, x30, [sp], 16
	ret
	.size	FtlBbtMemInit, .-FtlBbtMemInit
	.align	2
	.global	FtlFreeSysBlkQueueInit
	.type	FtlFreeSysBlkQueueInit, %function
FtlFreeSysBlkQueueInit:
	adrp	x3, .LANCHOR0
	add	x3, x3, :lo12:.LANCHOR0
	stp	x29, x30, [sp, -16]!
	add	x1, x3, 456
	mov	w2, 2048
	mov	x29, sp
	str	wzr, [x1, 2]
	mov	w1, 0
	strh	w0, [x3, 456]
	add	x0, x3, 464
	strh	wzr, [x3, 462]
	bl	ftl_memset
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
	.size	FtlFreeSysBlkQueueInit, .-FtlFreeSysBlkQueueInit
	.align	2
	.global	ftl_free_no_use_map_blk
	.type	ftl_free_no_use_map_blk, %function
ftl_free_no_use_map_blk:
	stp	x29, x30, [sp, -80]!
	mov	w1, 0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	stp	x21, x22, [sp, 32]
	stp	x23, x24, [sp, 48]
	str	x25, [sp, 64]
	ldp	x21, x20, [x0, 32]
	ldrh	w2, [x0, 10]
	ldr	x22, [x0, 16]
	lsl	w2, w2, 1
	mov	x0, x21
	bl	ftl_memset
	mov	w0, 0
.L498:
	ldrh	w1, [x19, 6]
	cmp	w1, w0
	bhi	.L502
	adrp	x0, .LANCHOR0+308
	mov	w24, 0
	mov	w20, 0
	ldrh	w1, [x0, #:lo12:.LANCHOR0+308]
	ldrh	w0, [x19]
	strh	w1, [x21, x0, lsl 1]
	ldrh	w25, [x21]
.L503:
	ldrh	w0, [x19, 10]
	cmp	w0, w20
	bhi	.L506
	mov	w0, w24
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
.L502:
	ubfiz	x1, x0, 2, 16
	ldr	w2, [x20, x1]
	mov	w1, 0
	ubfx	x2, x2, 10, 16
.L499:
	ldrh	w3, [x19, 10]
	cmp	w3, w1
	bhi	.L501
	add	w0, w0, 1
	and	w0, w0, 65535
	b	.L498
.L501:
	ubfiz	x3, x1, 1, 16
	ldrh	w4, [x22, x3]
	cmp	w4, w2
	bne	.L500
	cbz	w2, .L500
	ldrh	w4, [x21, x3]
	add	w4, w4, 1
	strh	w4, [x21, x3]
.L500:
	add	w1, w1, 1
	and	w1, w1, 65535
	b	.L499
.L506:
	ubfiz	x23, x20, 1, 16
	ldrh	w0, [x21, x23]
	cmp	w0, w25
	bcs	.L504
	ldrh	w1, [x22, x23]
	cmp	w1, 0
	csel	w24, w24, w20, eq
	csel	w25, w25, w0, eq
.L504:
	cbnz	w0, .L505
	ldrh	w0, [x22, x23]
	cbz	w0, .L505
	mov	w1, 1
	bl	FtlFreeSysBlkQueueIn
	strh	wzr, [x22, x23]
	ldrh	w0, [x19, 8]
	sub	w0, w0, #1
	strh	w0, [x19, 8]
.L505:
	add	w20, w20, 1
	and	w20, w20, 65535
	b	.L503
	.size	ftl_free_no_use_map_blk, .-ftl_free_no_use_map_blk
	.align	2
	.global	FtlL2PDataInit
	.type	FtlL2PDataInit, %function
FtlL2PDataInit:
	stp	x29, x30, [sp, -32]!
	mov	w1, 0
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldr	x0, [x19, 3568]
	ldr	w2, [x19, 332]
	lsl	w2, w2, 1
	bl	ftl_memset
	ldrh	w0, [x19, 342]
	mov	w1, 255
	ldrh	w2, [x19, 314]
	mul	w2, w2, w0
	ldr	x0, [x19, 3616]
	bl	ftl_memset
	mov	w0, 0
	mov	w4, -1
.L517:
	ldrh	w1, [x19, 342]
	cmp	w1, w0
	bhi	.L518
	ldr	w1, [x19, 332]
	strh	w1, [x19, 3642]
	mov	w1, -1
	str	w1, [x19, 3632]
	mov	w1, -3902
	strh	w1, [x19, 3636]
	ldrh	w1, [x19, 3696]
	strh	w1, [x19, 3640]
	ldrh	w1, [x19, 340]
	strh	w1, [x19, 3638]
	ldr	x1, [x19, 3560]
	str	x1, [x19, 3648]
	ldr	x1, [x19, 3608]
	str	x1, [x19, 3656]
	ldr	x1, [x19, 3568]
	str	x1, [x19, 3664]
	ldr	x1, [x19, 3600]
	str	x1, [x19, 3672]
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L518:
	ldr	x2, [x19, 2608]
	ubfiz	x1, x0, 4, 16
	add	x2, x2, x1
	str	wzr, [x2, 4]
	ldr	x2, [x19, 2608]
	strh	w4, [x2, x1]
	ldr	x2, [x19, 2608]
	ldr	x3, [x19, 3616]
	add	x2, x2, x1
	ldrh	w1, [x19, 314]
	mul	w1, w1, w0
	add	w0, w0, 1
	and	w0, w0, 65535
	sxtw	x1, w1
	and	x1, x1, -4
	add	x1, x3, x1
	str	x1, [x2, 8]
	b	.L517
	.size	FtlL2PDataInit, .-FtlL2PDataInit
	.align	2
	.global	FtlVariablesInit
	.type	FtlVariablesInit, %function
FtlVariablesInit:
	stp	x29, x30, [sp, -32]!
	mov	w0, -1
	mov	w1, 0
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	strh	w0, [x19, 3698]
	mov	w0, -1
	str	w0, [x19, 3708]
	ldr	x0, [x19, 352]
	strh	wzr, [x19, 348]
	ldrh	w2, [x19, 324]
	str	wzr, [x19, 3700]
	str	wzr, [x19, 3704]
	lsl	w2, w2, 1
	bl	ftl_memset
	ldr	x0, [x19, 2600]
	mov	w1, 0
	ldrh	w2, [x19, 246]
	lsl	w2, w2, 1
	bl	ftl_memset
	ldr	x0, [x19, 3536]
	mov	w1, 0
	ldrh	w2, [x19, 246]
	lsl	w2, w2, 1
	bl	ftl_memset
	add	x0, x19, 2512
	mov	w2, 48
	mov	w1, 0
	bl	ftl_memset
	mov	w2, 512
	mov	w1, 0
	add	x0, x19, 2712
	bl	ftl_memset
	bl	FtlGcBufInit
	bl	FtlL2PDataInit
	mov	w0, 0
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlVariablesInit, .-FtlVariablesInit
	.align	2
	.global	SupperBlkListInit
	.type	SupperBlkListInit, %function
SupperBlkListInit:
	stp	x29, x30, [sp, -80]!
	mov	w0, 6
	mov	w1, 0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x23, x24, [sp, 48]
	add	x23, x19, 264
	mov	w20, 0
	ldrh	w2, [x19, 246]
	mov	w24, -1
	stp	x21, x22, [sp, 32]
	mov	w22, 0
	mov	w21, 0
	str	x25, [sp, 64]
	mov	w25, 32768
	mul	w2, w2, w0
	ldr	x0, [x19, 2560]
	bl	ftl_memset
	strh	wzr, [x19, 228]
	str	xzr, [x19, 2568]
	str	xzr, [x19, 2576]
	strh	wzr, [x19, 2584]
	str	xzr, [x19, 2592]
.L523:
	ldrh	w0, [x19, 244]
	cmp	w0, w20
	bls	.L530
	ldrh	w8, [x19, 236]
	mov	x6, 0
	ldrh	w7, [x19, 306]
	mov	w5, 0
	b	.L531
.L525:
	ldrb	w0, [x23, x6]
	mov	w1, w20
	bl	V2P_block
	bl	FtlBbmIsBadBlock
	cbnz	w0, .L524
	add	w5, w7, w5
	and	w5, w5, 65535
.L524:
	add	x6, x6, 1
.L531:
	cmp	w8, w6, uxth
	bhi	.L525
	cbz	w5, .L526
	udiv	w5, w25, w5
.L527:
	ldr	x1, [x19, 2560]
	mov	w0, 6
	umaddl	x0, w20, w0, x1
	strh	w5, [x0, 4]
	ldrh	w0, [x19, 24]
	cmp	w0, w20
	beq	.L528
	ldrh	w0, [x19, 80]
	cmp	w0, w20
	beq	.L528
	ldrh	w0, [x19, 128]
	cmp	w0, w20
	beq	.L528
	ldr	x1, [x19, 72]
	ubfiz	x0, x20, 1, 16
	ldrh	w0, [x1, x0]
	cbnz	w0, .L529
	add	w22, w22, 1
	mov	w0, w20
	and	w22, w22, 65535
	bl	INSERT_FREE_LIST
.L528:
	add	w20, w20, 1
	and	w20, w20, 65535
	b	.L523
.L526:
	ldr	x1, [x19, 72]
	ubfiz	x0, x20, 1, 16
	strh	w24, [x1, x0]
	b	.L527
.L529:
	add	w21, w21, 1
	mov	w0, w20
	and	w21, w21, 65535
	bl	INSERT_DATA_LIST
	b	.L528
.L530:
	strh	w22, [x19, 228]
	strh	w21, [x19, 2584]
	add	w21, w21, w22
	cmp	w21, w0
	ble	.L532
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 198
	add	x0, x0, :lo12:.LC8
	mov	w2, 2219
	bl	sftl_printk
.L532:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
	.size	SupperBlkListInit, .-SupperBlkListInit
	.align	2
	.global	FtlGcPageVarInit
	.type	FtlGcPageVarInit, %function
FtlGcPageVarInit:
	stp	x29, x30, [sp, -32]!
	mov	w1, 255
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldr	x0, [x19, 3288]
	strh	wzr, [x19, 3284]
	ldrh	w2, [x19, 310]
	strh	wzr, [x19, 3296]
	lsl	w2, w2, 1
	bl	ftl_memset
	ldrh	w2, [x19, 310]
	mov	w0, 12
	mov	w1, 255
	mul	w2, w2, w0
	ldr	x0, [x19, 3304]
	bl	ftl_memset
	bl	FtlGcBufInit
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlGcPageVarInit, .-FtlGcPageVarInit
	.align	2
	.global	FlashGetBadBlockList
	.type	FlashGetBadBlockList, %function
FlashGetBadBlockList:
	stp	x29, x30, [sp, -48]!
	mov	w2, 256
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	mov	x19, x0
	str	x21, [sp, 32]
	mov	w21, w1
	mov	w1, 255
	bl	ftl_memset
	ldr	x2, [x20, 3352]
	mov	w1, w21
	mov	x0, x19
	blr	x2
	and	w0, w0, 65535
	cmp	w0, 50
	bls	.L537
	mov	x0, x19
	mov	w2, 256
	mov	w1, 255
	bl	ftl_memset
	mov	w0, 0
.L537:
	ldrh	w1, [x20, 14]
	cmp	w1, 4
	bne	.L542
	mov	x1, 0
	b	.L538
.L539:
	ldrh	w2, [x19, x1, lsl 1]
	lsr	w2, w2, 1
	strh	w2, [x19, x1, lsl 1]
	add	x1, x1, 1
.L538:
	cmp	w0, w1, uxth
	bhi	.L539
.L542:
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
	.size	FlashGetBadBlockList, .-FlashGetBadBlockList
	.align	2
	.global	ftl_memcpy
	.type	ftl_memcpy, %function
ftl_memcpy:
	stp	x29, x30, [sp, -16]!
	uxtw	x2, w2
	mov	x29, sp
	bl	memcpy
	ldp	x29, x30, [sp], 16
	ret
	.size	ftl_memcpy, .-ftl_memcpy
	.section	.rodata.str1.1
.LC92:
	.string	"FlashReadPages %x %x error_ecc_bits %d\n"
.LC93:
	.string	"data:"
.LC94:
	.string	"spare:"
	.text
	.align	2
	.global	FlashReadPages
	.type	FlashReadPages, %function
FlashReadPages:
	stp	x29, x30, [sp, -96]!
	mrs	x2, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	mov	w0, 32
	stp	x21, x22, [sp, 32]
	adrp	x22, .LANCHOR1
	add	x22, x22, :lo12:.LANCHOR1
	stp	x23, x24, [sp, 48]
	umaddl	x23, w1, w0, x19
	adrp	x24, .LC8
	stp	x25, x26, [sp, 64]
	add	x22, x22, 216
	ldrh	w25, [x20, 12]
	ldr	x3, [x2, 952]
	str	x3, [sp, 88]
	mov	x3, 0
	add	x24, x24, :lo12:.LC8
.L547:
	cmp	x19, x23
	bne	.L560
	mrs	x0, sp_el0
	ldr	x1, [sp, 88]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L561
	bl	__stack_chk_fail
.L560:
	ldr	x0, [x19, 8]
	cbz	x0, .L548
	ldr	x0, [x19, 16]
	cbnz	x0, .L549
.L548:
	mov	x1, x22
	mov	x0, x24
	mov	w2, 96
	bl	sftl_printk
.L549:
	ldr	w0, [x19, 4]
	add	x2, sp, 80
	add	x1, sp, 84
	bl	l2p_addr_tran.isra.0
	ldr	w0, [sp, 80]
	cmp	w0, 3
	bls	.L550
	mov	w0, -1
	str	w0, [x19]
.L551:
	add	x19, x19, 32
	b	.L547
.L550:
	ldr	x21, [x19, 8]
	tst	x21, 63
	beq	.L552
	ldr	x21, [x20, 3488]
.L552:
	mov	x2, x21
	ldr	w1, [sp, 84]
	ldr	x3, [x19, 16]
	ldr	x4, [x20, 3376]
	blr	x4
	str	w0, [x19]
	ldrh	w0, [x20, 14]
	cmp	w0, 4
	bne	.L554
	ldrb	w0, [sp, 80]
	add	x2, x21, 2048
	ldr	x4, [x20, 3376]
	ldr	x3, [x19, 16]
	ldr	w1, [sp, 84]
	add	x3, x3, 8
	add	w1, w25, w1
	blr	x4
	cmn	w0, #1
	beq	.L555
	ldr	x1, [x19, 16]
	ldr	w2, [x1, 12]
	cmn	w2, #1
	bne	.L556
	ldr	w2, [x1, 8]
	cmn	w2, #1
	bne	.L556
	ldr	w1, [x1]
	cmn	w1, #1
	beq	.L556
.L555:
	mov	w0, -1
.L587:
	str	w0, [x19]
	b	.L557
.L556:
	ldr	w1, [x19]
	cmn	w1, #1
	beq	.L557
	cmp	w0, 256
	beq	.L587
.L557:
	ldr	w3, [x19]
	cmp	w3, 256
	ccmn	w3, #1, 4, ne
	bne	.L554
	ldr	w1, [x19, 4]
	adrp	x0, .LC92
	ldr	w2, [sp, 84]
	add	x0, x0, :lo12:.LC92
	bl	sftl_printk
	ldr	x1, [x19, 8]
	cbz	x1, .L559
	mov	w3, 4
	adrp	x0, .LC93
	mov	w2, w3
	add	x0, x0, :lo12:.LC93
	bl	rknand_print_hex
.L559:
	ldr	x1, [x19, 16]
	cbz	x1, .L554
	mov	w3, 4
	adrp	x0, .LC94
	mov	w2, w3
	add	x0, x0, :lo12:.LC94
	bl	rknand_print_hex
.L554:
	ldr	x0, [x20, 3488]
	cmp	x0, x21
	bne	.L551
	ldr	x0, [x19, 8]
	cmp	x0, x21
	beq	.L551
	ldrh	w2, [x20, 262]
	mov	x1, x21
	lsl	w2, w2, 9
	bl	ftl_memcpy
	b	.L551
.L561:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x29, x30, [sp], 96
	ret
	.size	FlashReadPages, .-FlashReadPages
	.align	2
	.global	FtlLoadFactoryBbt
	.type	FtlLoadFactoryBbt, %function
FtlLoadFactoryBbt:
	stp	x29, x30, [sp, -80]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	add	x21, x20, 3712
	mov	w22, 0
	stp	x23, x24, [sp, 48]
	add	x23, x20, 360
	stp	x25, x26, [sp, 64]
	mov	w25, -1
	mov	w26, 61664
	ldr	x0, [x20, 3448]
	ldr	x24, [x20, 3512]
	stp	x0, x24, [x21, 8]
.L589:
	ldrh	w0, [x20, 258]
	cmp	w0, w22
	bhi	.L594
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
.L594:
	ldrh	w19, [x20, 302]
	strh	w25, [x23, 12]
.L591:
	ldrh	w0, [x20, 302]
	sub	w19, w19, #1
	and	w19, w19, 65535
	sub	w1, w0, #16
	cmp	w19, w1
	ble	.L592
	madd	w0, w0, w22, w19
	mov	w2, 1
	mov	w1, w2
	lsl	w0, w0, 10
	str	w0, [x21, 4]
	mov	x0, x21
	bl	FlashReadPages
	ldr	w0, [x21]
	cmn	w0, #1
	beq	.L591
	ldrh	w0, [x24]
	cmp	w0, w26
	bne	.L591
	strh	w19, [x23, 12]
.L592:
	add	w22, w22, 1
	add	x23, x23, 2
	b	.L589
	.size	FtlLoadFactoryBbt, .-FtlLoadFactoryBbt
	.align	2
	.global	FtlGetLastWrittenPage
	.type	FtlGetLastWrittenPage, %function
FtlGetLastWrittenPage:
	stp	x29, x30, [sp, -176]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	stp	x21, x22, [sp, 32]
	mov	w22, w1
	mrs	x1, sp_el0
	stp	x23, x24, [sp, 48]
	cmp	w22, 1
	ldr	x2, [x1, 952]
	str	x2, [sp, 168]
	mov	x2, 0
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	bne	.L600
	ldrh	w19, [x1, 308]
.L601:
	sub	w19, w19, #1
	lsl	w21, w0, 10
	ldr	x1, [x1, 3496]
	sxth	w19, w19
	orr	w0, w19, w0, lsl 10
	str	x1, [sp, 80]
	add	x1, sp, 104
	mov	w2, w22
	str	w0, [sp, 76]
	add	x0, sp, 72
	str	x1, [sp, 88]
	mov	w1, 1
	bl	FlashReadPages
	ldr	w0, [sp, 104]
	cmn	w0, #1
	bne	.L603
	mov	w23, 0
	mov	w24, 2
	b	.L602
.L600:
	ldrh	w19, [x1, 306]
	b	.L601
.L606:
	add	w0, w23, w19
	mov	w1, 1
	mov	w2, w22
	sdiv	w0, w0, w24
	sxth	w20, w0
	orr	w0, w0, w21
	str	w0, [sp, 76]
	add	x0, sp, 72
	bl	FlashReadPages
	ldp	w0, w1, [sp, 104]
	and	w0, w0, w1
	cmn	w0, #1
	bne	.L604
	ldr	w0, [sp, 72]
	cmn	w0, #1
	beq	.L604
	sub	w19, w20, #1
	sxth	w19, w19
.L602:
	cmp	w23, w19
	ble	.L606
.L603:
	mrs	x0, sp_el0
	ldr	x1, [sp, 168]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L607
	bl	__stack_chk_fail
.L604:
	add	w20, w20, 1
	sxth	w23, w20
	b	.L602
.L607:
	mov	w0, w19
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x29, x30, [sp], 176
	ret
	.size	FtlGetLastWrittenPage, .-FtlGetLastWrittenPage
	.align	2
	.global	FtlScanSysBlk
	.type	FtlScanSysBlk, %function
FtlScanSysBlk:
	stp	x29, x30, [sp, -96]!
	mov	w1, 0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	ldr	x0, [x19, 3608]
	stp	x23, x24, [sp, 48]
	ldr	w2, [x19, 332]
	stp	x25, x26, [sp, 64]
	adrp	x24, .LANCHOR1
	add	x24, x24, :lo12:.LANCHOR1
	lsl	w2, w2, 2
	stp	x27, x28, [sp, 80]
	add	x24, x24, 231
	strh	wzr, [x19, 348]
	strh	wzr, [x19, 3696]
	bl	ftl_memset
	ldr	x0, [x19, 3560]
	mov	w1, 0
	ldr	w2, [x19, 332]
	lsl	w2, w2, 1
	bl	ftl_memset
	ldr	x0, [x19, 3584]
	mov	w1, 0
	ldrh	w2, [x19, 324]
	lsl	w2, w2, 2
	bl	ftl_memset
	ldr	x0, [x19, 352]
	mov	w1, 0
	ldrh	w2, [x19, 324]
	lsl	w2, w2, 1
	bl	ftl_memset
	add	x0, x19, 2696
	mov	w2, 16
	mov	w1, 255
	bl	ftl_memset
	ldrh	w23, [x19, 244]
.L614:
	ldrh	w0, [x19, 246]
	cmp	w0, w23
	bls	.L653
	ldrh	w9, [x19, 236]
	add	x7, x19, 264
	ldrh	w8, [x19, 316]
	mov	x5, 0
	mov	w22, 0
	mov	w6, 4
	b	.L654
.L616:
	ldrb	w0, [x7, x5]
	mov	w1, w23
	bl	V2P_block
	and	w4, w0, 65535
	bl	FtlBbmIsBadBlock
	cbnz	w0, .L615
	ldr	x0, [x19, 3424]
	ubfiz	x2, x22, 5, 16
	lsl	w4, w4, 10
	add	x0, x0, x2
	str	w4, [x0, 4]
	ldr	x1, [x19, 3424]
	ldr	x0, [x19, 3256]
	add	x1, x1, x2
	ldr	x2, [x19, 3264]
	str	x0, [x1, 8]
	mul	w0, w22, w8
	add	w22, w22, 1
	and	w22, w22, 65535
	sdiv	w0, w0, w6
	add	x0, x2, w0, sxtw 2
	str	x0, [x1, 16]
.L615:
	add	x5, x5, 1
.L654:
	cmp	w9, w5, uxth
	bhi	.L616
	cbnz	w22, .L617
.L652:
	add	w23, w23, 1
	and	w23, w23, 65535
	b	.L614
.L617:
	ldr	x0, [x19, 3424]
	adrp	x26, .LC8
	mov	w1, w22
	add	x26, x26, :lo12:.LC8
	mov	w2, 1
	mov	x25, 0
	bl	FlashReadPages
.L651:
	ldr	x0, [x19, 3424]
	lsl	x27, x25, 5
	add	x1, x0, x25, lsl 5
	ldr	w0, [x0, x27]
	ldr	w20, [x1, 4]
	ldr	x21, [x1, 16]
	ubfx	x20, x20, 10, 16
	cmn	w0, #1
	bne	.L620
	mov	w28, 16
.L622:
	ldr	x0, [x19, 3424]
	mov	w2, 1
	add	x0, x0, x27
	ldr	w1, [x0, 4]
	add	w1, w1, 1
	str	w1, [x0, 4]
	mov	w1, w2
	ldr	x0, [x19, 3424]
	add	x0, x0, x27
	bl	FlashReadPages
	ldrh	w0, [x21]
	mov	w3, 65535
	cmp	w0, w3
	ldr	x0, [x19, 3424]
	bne	.L619
	mov	w1, -1
	str	w1, [x0, x27]
	ldr	x0, [x19, 3424]
	ldr	w0, [x0, x27]
	cmp	w0, w1
	beq	.L621
.L620:
	ldr	w1, [x19, 2660]
	ldr	w0, [x21, 4]
	cmn	w1, #1
	beq	.L669
	cmp	w1, w0
	bhi	.L623
.L669:
	cmn	w0, #1
	beq	.L623
	add	w1, w0, 1
	str	w1, [x19, 2660]
.L623:
	ldrh	w1, [x21]
	mov	w2, 61634
	cmp	w1, w2
	beq	.L624
	bhi	.L625
	mov	w2, 61574
	cmp	w1, w2
	beq	.L626
	mov	w2, 61604
	cmp	w1, w2
	beq	.L627
.L628:
	add	x25, x25, 1
	cmp	w22, w25, uxth
	bhi	.L651
	b	.L652
.L619:
	ldr	w0, [x0, x27]
	cmn	w0, #1
	bne	.L620
	sub	w28, w28, #1
	ands	w28, w28, 65535
	bne	.L622
.L621:
	mov	w1, 1
	b	.L693
.L625:
	mov	w0, 65535
	cmp	w1, w0
	bne	.L628
	mov	w1, 0
.L693:
	mov	w0, w20
	bl	FtlFreeSysBlkQueueIn
	b	.L628
.L624:
	ldrh	w1, [x19, 3696]
	ldr	w0, [x19, 332]
	cmp	w1, w0
	bls	.L630
	mov	x1, x24
	mov	x0, x26
	mov	w2, 1232
	bl	sftl_printk
.L630:
	ldr	w4, [x19, 332]
	ldrh	w3, [x19, 3696]
	and	w0, w4, 65535
	sub	w1, w0, #1
	sub	w0, w0, w3
	ldr	x5, [x19, 3608]
	sub	w0, w0, #1
	sxth	w2, w1
	sxth	x1, w1
	sxth	w0, w0
	add	x7, x5, 4
.L631:
	cmp	w0, w2
	bge	.L636
	sub	x6, x1, #1
	ldr	w10, [x21, 4]
	lsl	x8, x1, 2
	ldr	w9, [x7, x6, lsl 2]
	cmp	w10, w9
	bls	.L632
	ldr	w0, [x5]
	cbnz	w0, .L633
	cmp	w4, w3
	beq	.L633
	add	w3, w3, 1
	strh	w3, [x19, 3696]
.L633:
	mov	w0, 0
.L634:
	ldr	x3, [x19, 3608]
	cmp	w0, w2
	bne	.L635
	ldr	w0, [x21, 4]
	str	w0, [x3, x8]
	ldr	x0, [x19, 3560]
	strh	w20, [x0, x1, lsl 1]
.L636:
	tbnz	w2, #31, .L628
	ldrh	w1, [x19, 3696]
	ldr	w0, [x19, 332]
	sub	w0, w0, w1
	sub	w0, w0, #1
	cmp	w2, w0, sxth
	bgt	.L628
	ldr	x0, [x19, 3608]
	add	w1, w1, 1
	strh	w1, [x19, 3696]
	ldr	w1, [x21, 4]
	str	w1, [x0, w2, sxtw 2]
	ldr	x0, [x19, 3560]
.L692:
	strh	w20, [x0, w2, sxtw 1]
	b	.L628
.L635:
	add	w4, w0, 1
	ldr	w5, [x3, w4, sxtw 2]
	str	w5, [x3, w0, sxtw 2]
	ldr	x3, [x19, 3560]
	ldrh	w5, [x3, w4, sxtw 1]
	strh	w5, [x3, w0, sxtw 1]
	sxth	w0, w4
	b	.L634
.L632:
	sub	w2, w2, #1
	mov	x1, x6
	sxth	w2, w2
	b	.L631
.L626:
	ldrh	w1, [x19, 348]
	ldrh	w0, [x19, 324]
	cmp	w1, w0
	bls	.L640
	mov	x1, x24
	mov	x0, x26
	mov	w2, 1273
	bl	sftl_printk
.L640:
	ldrh	w3, [x19, 324]
	ldrh	w0, [x19, 348]
	sub	w1, w3, #1
	ldr	x4, [x19, 3584]
	sxth	w2, w1
	sub	w1, w1, w0
.L641:
	cmp	w2, w1
	ble	.L646
	sbfiz	x5, x2, 2, 32
	ldr	w8, [x21, 4]
	sxtw	x6, w2
	ldr	w7, [x4, x5]
	cmp	w8, w7
	bls	.L642
	ldr	w1, [x4]
	cbnz	w1, .L643
	cmp	w3, w0
	beq	.L643
	add	w0, w0, 1
	strh	w0, [x19, 348]
.L643:
	mov	w0, 0
.L644:
	ldr	x1, [x19, 3584]
	cmp	w0, w2
	bne	.L645
	ldr	w0, [x21, 4]
	str	w0, [x1, x5]
	ldr	x0, [x19, 352]
	strh	w20, [x0, x6, lsl 1]
.L646:
	tbnz	w2, #31, .L628
	ldrh	w0, [x19, 324]
	ldrh	w1, [x19, 348]
	sub	w0, w0, #1
	sub	w0, w0, w1
	cmp	w2, w0, sxth
	bgt	.L628
	ldr	x0, [x19, 3584]
	add	w1, w1, 1
	strh	w1, [x19, 348]
	ldr	w1, [x21, 4]
	str	w1, [x0, w2, sxtw 2]
	ldr	x0, [x19, 352]
	b	.L692
.L645:
	add	w3, w0, 1
	ldr	w4, [x1, w3, sxtw 2]
	str	w4, [x1, w0, sxtw 2]
	ldr	x1, [x19, 352]
	ldrh	w4, [x1, w3, sxtw 1]
	strh	w4, [x1, w0, sxtw 1]
	sxth	w0, w3
	b	.L644
.L642:
	sub	w2, w2, #1
	sxth	w2, w2
	b	.L641
.L627:
	ldrh	w3, [x19, 2696]
	add	x1, x19, 2560
	mov	w2, 65535
	cmp	w3, w2
	bne	.L648
	strh	w20, [x1, 136]
	str	w0, [x1, 144]
	b	.L628
.L648:
	ldrh	w0, [x1, 140]
	cmp	w0, w2
	beq	.L649
	mov	w1, 1
	bl	FtlFreeSysBlkQueueIn
.L649:
	ldr	w1, [x21, 4]
	add	x0, x19, 2560
	ldr	w2, [x0, 144]
	cmp	w2, w1
	bcs	.L650
	ldrh	w1, [x0, 136]
	strh	w1, [x0, 140]
	strh	w20, [x0, 136]
	ldr	w1, [x21, 4]
	str	w1, [x0, 144]
	b	.L628
.L650:
	strh	w20, [x0, 140]
	b	.L628
.L653:
	ldr	x2, [x19, 3560]
	ldrh	w0, [x2]
	cbz	w0, .L655
.L658:
	ldr	x1, [x19, 352]
	ldrh	w0, [x1]
	cbz	w0, .L656
.L657:
	ldrh	w1, [x19, 3696]
	ldr	w0, [x19, 332]
	cmp	w1, w0
	bls	.L688
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 231
	add	x0, x0, :lo12:.LC8
	mov	w2, 1398
	bl	sftl_printk
.L688:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 96
	ret
.L655:
	ldrh	w0, [x19, 3696]
	cbz	w0, .L658
	ldr	w3, [x19, 332]
	mov	w0, 0
.L659:
	cmp	w0, w3
	bcs	.L658
	ldrh	w4, [x2, w0, sxtw 1]
	add	w1, w0, 1
	sxth	w1, w1
	cbz	w4, .L671
	mov	w1, w0
.L660:
	ldr	x2, [x19, 3560]
	sub	w3, w1, w0
	ldrh	w4, [x2, w1, sxtw 1]
	strh	w4, [x2, w3, sxtw 1]
	ldr	x2, [x19, 3608]
	ldr	w4, [x2, w1, sxtw 2]
	str	w4, [x2, w3, sxtw 2]
	ldr	x2, [x19, 3560]
	strh	wzr, [x2, w1, sxtw 1]
	add	w2, w1, 1
	ldr	w3, [x19, 332]
	sxth	w1, w2
	cmp	w3, w2, sxth
	bhi	.L660
	b	.L658
.L671:
	mov	w0, w1
	b	.L659
.L656:
	ldrh	w0, [x19, 348]
	cbz	w0, .L657
	ldrh	w2, [x19, 324]
	mov	w0, 0
.L664:
	mov	w3, w0
	cmp	w0, w2
	bge	.L657
	ldrh	w4, [x1, w0, sxtw 1]
	cbz	w4, .L689
.L665:
	ldrh	w1, [x19, 324]
	cmp	w0, w1
	bge	.L657
	ldr	x1, [x19, 352]
	sub	w2, w0, w3
	ldrh	w4, [x1, w0, sxtw 1]
	strh	w4, [x1, w2, sxtw 1]
	ldr	x1, [x19, 3584]
	ldr	w4, [x1, w0, sxtw 2]
	str	w4, [x1, w2, sxtw 2]
	ldr	x1, [x19, 352]
	strh	wzr, [x1, w0, sxtw 1]
	add	w0, w0, 1
	sxth	w0, w0
	b	.L665
.L689:
	add	w0, w0, 1
	sxth	w0, w0
	b	.L664
	.size	FtlScanSysBlk, .-FtlScanSysBlk
	.align	2
	.global	FtlLoadBbt
	.type	FtlLoadBbt, %function
FtlLoadBbt:
	stp	x29, x30, [sp, -80]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	add	x22, x19, 3712
	ldr	x0, [x19, 3448]
	stp	x23, x24, [sp, 48]
	mov	w23, 61649
	str	x25, [sp, 64]
	ldr	x21, [x19, 3512]
	stp	x0, x21, [x22, 8]
	bl	FtlBbtMemInit
	ldrh	w20, [x19, 302]
	sub	w20, w20, #1
	and	w20, w20, 65535
.L695:
	ldrh	w0, [x19, 302]
	sub	w0, w0, #16
	cmp	w20, w0
	ble	.L700
	lsl	w0, w20, 10
	mov	w2, 1
	mov	w1, w2
	str	w0, [x22, 4]
	mov	x0, x22
	bl	FlashReadPages
	ldr	w0, [x22]
	cmn	w0, #1
	beq	.L696
.L699:
	ldrh	w0, [x21]
	cmp	w0, w23
	bne	.L698
	ldr	w1, [x21, 4]
	str	w1, [x19, 368]
	strh	w20, [x19, 360]
	ldrh	w1, [x21, 8]
	strh	w1, [x19, 364]
.L700:
	ldrh	w1, [x19, 360]
	mov	w0, 65535
	cmp	w1, w0
	beq	.L714
	ldrh	w1, [x19, 364]
	cmp	w1, w0
	beq	.L704
	add	x0, x19, 3712
	lsl	w1, w1, 10
	mov	w2, 1
	str	w1, [x0, 4]
	mov	w1, w2
	bl	FlashReadPages
	ldr	w0, [x19, 3712]
	cmn	w0, #1
	beq	.L704
	ldrh	w1, [x21]
	mov	w0, 61649
	cmp	w1, w0
	bne	.L704
	ldr	w1, [x19, 368]
	ldr	w0, [x21, 4]
	cmp	w0, w1
	bls	.L704
	str	w0, [x19, 368]
	ldrh	w1, [x19, 364]
	ldrh	w0, [x21, 8]
	strh	w1, [x19, 360]
	strh	w0, [x19, 364]
.L704:
	ldrh	w0, [x19, 360]
	mov	w1, 1
	add	x23, x19, 3712
	mov	w20, 0
	mov	w25, 61649
	bl	FtlGetLastWrittenPage
	sxth	w24, w0
	mov	w22, w0
	add	w0, w24, 1
	strh	w0, [x19, 362]
.L706:
	sub	w0, w22, w20
	tbz	x0, 15, .L709
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 245
	add	x0, x0, :lo12:.LC8
	mov	w2, 251
	bl	sftl_printk
.L708:
	ldrh	w0, [x21, 10]
	mov	w1, 65535
	strh	w0, [x19, 366]
	ldrh	w0, [x21, 12]
	cmp	w0, w1
	beq	.L711
	ldr	w2, [x19, 232]
	cmp	w0, w2
	beq	.L711
	ldrh	w1, [x19, 246]
	lsr	w1, w1, 2
	cmp	w2, w1
	bcs	.L711
	cmp	w0, w1
	bcs	.L711
	bl	FtlSysBlkNumInit
.L711:
	add	x21, x19, 360
	mov	x20, 0
.L712:
	ldrh	w0, [x19, 258]
	cmp	w0, w20
	bhi	.L713
	mov	w0, 0
.L694:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
.L696:
	ldr	w0, [x22, 4]
	mov	w2, 1
	mov	w1, w2
	add	w0, w0, 1
	str	w0, [x22, 4]
	mov	x0, x22
	bl	FlashReadPages
	ldr	w0, [x22]
	cmn	w0, #1
	bne	.L699
.L698:
	sub	w20, w20, #1
	and	w20, w20, 65535
	b	.L695
.L709:
	ldrh	w0, [x19, 360]
	sub	w1, w24, w20
	mov	w2, 1
	orr	w0, w1, w0, lsl 10
	str	w0, [x23, 4]
	ldr	x0, [x19, 3448]
	mov	w1, w2
	str	x0, [x23, 8]
	mov	x0, x23
	bl	FlashReadPages
	ldr	w0, [x23]
	cmn	w0, #1
	beq	.L707
	ldrh	w0, [x21]
	cmp	w0, w25
	beq	.L708
.L707:
	add	w20, w20, 1
	b	.L706
.L713:
	ldrh	w2, [x19, 3624]
	add	x0, x21, x20, lsl 3
	ldr	x1, [x19, 3720]
	ldr	x0, [x0, 32]
	mul	w3, w2, w20
	lsl	w2, w2, 2
	add	x20, x20, 1
	add	x1, x1, x3, lsl 2
	bl	ftl_memcpy
	b	.L712
.L714:
	mov	w0, -1
	b	.L694
	.size	FtlLoadBbt, .-FtlLoadBbt
	.section	.rodata.str1.1
.LC95:
	.string	"prog read error: = %x\n"
.LC96:
	.string	"prog read REFRESH: = %x\n"
.LC97:
	.string	"prog read s error: = %x %x %x %x %x\n"
.LC98:
	.string	"prog read d error: = %x %x %x %x %x\n"
	.text
	.align	2
	.global	FlashProgPages
	.type	FlashProgPages, %function
FlashProgPages:
	stp	x29, x30, [sp, -160]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	x19, x0
	mov	w0, 32
	stp	x23, x24, [sp, 48]
	adrp	x23, .LANCHOR1
	umaddl	x22, w1, w0, x19
	stp	x25, x26, [sp, 64]
	add	x23, x23, :lo12:.LANCHOR1
	mov	w25, w2
	stp	x27, x28, [sp, 80]
	mrs	x2, sp_el0
	adrp	x26, .LC8
	ldrh	w27, [x20, 12]
	ldr	x3, [x2, 952]
	str	x3, [sp, 152]
	mov	x3, 0
	mov	x21, x19
	add	x23, x23, 256
	add	x26, x26, :lo12:.LC8
	lsl	w28, w27, 3
.L727:
	cmp	x21, x22
	bne	.L741
	adrp	x21, .LC95
	adrp	x23, .LC96
	add	x21, x21, :lo12:.LC95
	add	x23, x23, :lo12:.LC96
.L742:
	cmp	x19, x22
	beq	.L735
	ldr	x0, [x20, 3488]
	mov	w2, w25
	mov	w1, 1
	str	wzr, [x0]
	ldr	x0, [x20, 3504]
	str	wzr, [x0]
	ldr	w0, [x19, 4]
	str	w0, [sp, 124]
	ldr	x0, [x20, 3488]
	str	x0, [sp, 128]
	ldr	x0, [x20, 3504]
	str	x0, [sp, 136]
	add	x0, sp, 120
	bl	FlashReadPages
	ldr	w24, [sp, 120]
	cmn	w24, #1
	bne	.L743
	ldr	w1, [x19, 4]
	mov	x0, x21
	bl	sftl_printk
	str	w24, [x19]
.L743:
	ldr	w24, [sp, 120]
	cmp	w24, 256
	bne	.L744
	ldr	w1, [x19, 4]
	mov	x0, x23
	bl	sftl_printk
	str	w24, [x19]
.L744:
	ldr	x1, [x19, 16]
	cbz	x1, .L745
	ldr	x0, [x20, 3504]
	ldr	w2, [x1]
	ldr	w4, [x0]
	cmp	w2, w4
	bne	.L746
	ldr	w3, [x0, 8]
	ldr	w5, [x1, 8]
	cmp	w5, w3
	beq	.L745
.L746:
	ldr	w5, [x0, 4]
	adrp	x0, .LC97
	ldr	w3, [x1, 4]
	add	x0, x0, :lo12:.LC97
	ldr	w1, [x19, 4]
	bl	sftl_printk
	mov	w0, -1
	str	w0, [x19]
.L745:
	ldr	x1, [x19, 8]
	cbz	x1, .L747
	ldr	x0, [x20, 3488]
	ldr	w2, [x1]
	ldr	w4, [x0]
	cmp	w2, w4
	bne	.L748
	ldr	w3, [x0, 2048]
	ldr	w5, [x1, 2048]
	cmp	w5, w3
	beq	.L747
.L748:
	ldr	w5, [x0, 4]
	adrp	x0, .LC98
	ldr	w3, [x1, 4]
	add	x0, x0, :lo12:.LC98
	ldr	w1, [x19, 4]
	bl	sftl_printk
	mov	w0, -1
	str	w0, [x19]
.L747:
	add	x19, x19, 32
	b	.L742
.L741:
	ldr	x0, [x21, 8]
	cbz	x0, .L728
	ldr	x0, [x21, 16]
	cbnz	x0, .L729
.L728:
	mov	x1, x23
	mov	x0, x26
	mov	w2, 148
	bl	sftl_printk
.L729:
	ldr	w0, [x21, 4]
	add	x2, sp, 112
	add	x1, sp, 116
	bl	l2p_addr_tran.isra.0
	ldr	w0, [sp, 112]
	cmp	w0, 3
	bls	.L730
	mov	w0, -1
	str	w0, [x21]
.L731:
	add	x21, x21, 32
	b	.L727
.L730:
	cbnz	w0, .L732
	ldr	w0, [sp, 116]
	cmp	w28, w0
	bls	.L732
	adrp	x20, .LANCHOR1
	add	x20, x20, :lo12:.LANCHOR1
	adrp	x21, .LC86
	add	x20, x20, 256
	add	x21, x21, :lo12:.LC86
.L733:
	cmp	x19, x22
	bne	.L734
	bl	dump_stack
.L735:
	mrs	x0, sp_el0
	ldr	x1, [sp, 152]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L750
	bl	__stack_chk_fail
.L734:
	ldr	w2, [x19, 4]
	mov	w0, -1
	str	w0, [x19]
	mov	x1, x20
	mov	x0, x21
	add	x19, x19, 32
	bl	sftl_printk
	ldr	x1, [x19, -24]
	mov	w3, 16
	mov	w2, 4
	adrp	x0, .LC87
	add	x0, x0, :lo12:.LC87
	bl	rknand_print_hex
	ldr	x1, [x19, -16]
	mov	w3, 4
	adrp	x0, .LC88
	mov	w2, w3
	add	x0, x0, :lo12:.LC88
	bl	rknand_print_hex
	b	.L733
.L732:
	ldr	x1, [x21, 8]
	tst	x1, 63
	beq	.L751
	ldr	x24, [x20, 3488]
	cmp	x24, x1
	beq	.L736
	ldrh	w2, [x20, 262]
	mov	x0, x24
	lsl	w2, w2, 9
	bl	ftl_memcpy
.L736:
	ldrb	w0, [sp, 112]
	add	x5, x20, 3352
	ldr	w1, [sp, 116]
	mov	x2, x24
	ldr	x3, [x21, 16]
	str	x5, [sp, 104]
	ldr	x4, [x20, 3368]
	blr	x4
	cmp	w0, 0
	mov	w4, -1
	csel	w0, wzr, w4, eq
	str	w0, [x21]
	ldr	x5, [sp, 104]
	ldrh	w0, [x20, 14]
	cmp	w0, 4
	bne	.L731
	ldrb	w0, [sp, 112]
	add	x2, x24, 2048
	ldr	x5, [x5, 16]
	ldr	x3, [x21, 16]
	ldr	w1, [sp, 116]
	add	x3, x3, 8
	add	w1, w27, w1
	blr	x5
	cbz	w0, .L731
	mov	w4, -1
	str	w4, [x21]
	b	.L731
.L751:
	mov	x24, x1
	b	.L736
.L750:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 160
	ret
	.size	FlashProgPages, .-FlashProgPages
	.align	2
	.global	FtlLowFormatEraseBlock
	.type	FtlLowFormatEraseBlock, %function
FtlLowFormatEraseBlock:
	stp	x29, x30, [sp, -112]!
	mov	w5, 0
	mov	w9, 4
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	add	x8, x19, 264
	stp	x21, x22, [sp, 32]
	and	w22, w1, 255
	mov	w21, 0
	mov	w20, 0
	stp	x23, x24, [sp, 48]
	and	w23, w0, 65535
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	str	w23, [x19, 3420]
.L767:
	ldrh	w0, [x19, 236]
	cmp	w0, w5
	bhi	.L771
	cbz	w21, .L766
	ldr	x0, [x19, 3400]
	mov	w2, w21
	mov	w1, 0
	mov	x24, 0
	bl	FlashEraseBlocks
.L775:
	ldr	x1, [x19, 3400]
	lsl	x0, x24, 5
	ldr	w0, [x1, x0]
	add	x2, x1, x24, lsl 5
	cmn	w0, #1
	bne	.L774
	ldr	w0, [x2, 4]
	add	w20, w20, 1
	and	w20, w20, 65535
	ubfx	x0, x0, 10, 16
	bl	FtlBbmMapBadBlock
.L774:
	add	x24, x24, 1
	cmp	w21, w24, uxth
	bhi	.L775
	cbz	w22, .L788
	ldrh	w26, [x19, 308]
	mov	w25, 1
.L776:
	add	x27, x19, 264
	mov	w24, 0
	mov	w28, 4
.L784:
	mov	w5, 0
	mov	w21, 0
.L777:
	ldrh	w0, [x19, 236]
	cmp	w0, w5
	bhi	.L780
	cbz	w21, .L766
	ldr	x0, [x19, 3400]
	mov	w1, w21
	mov	w2, w25
	mov	w3, 1
	bl	FlashProgPages
	mov	x1, 0
.L783:
	ldr	x2, [x19, 3400]
	lsl	x0, x1, 5
	ldr	w0, [x2, x0]
	add	x3, x2, x1, lsl 5
	cbz	w0, .L782
	ldr	w0, [x3, 4]
	add	w20, w20, 1
	str	x1, [sp, 104]
	and	w20, w20, 65535
	ubfx	x0, x0, 10, 16
	bl	FtlBbmMapBadBlock
	ldr	x1, [sp, 104]
.L782:
	add	x1, x1, 1
	cmp	w21, w1, uxth
	bhi	.L783
	add	w24, w24, 1
	cmp	w26, w24, uxth
	bhi	.L784
	mov	x24, 0
.L786:
	cbz	w22, .L785
	ldr	x1, [x19, 3400]
	lsl	x0, x24, 5
	ldr	w0, [x1, x0]
	add	x2, x1, x24, lsl 5
	cbnz	w0, .L785
	ldr	w0, [x2, 4]
	mov	w1, 1
	ubfx	x0, x0, 10, 16
	bl	FtlFreeSysBlkQueueIn
.L785:
	add	x24, x24, 1
	cmp	w21, w24, uxth
	bhi	.L786
	cmp	w23, 63
	ccmp	w22, 0, 0, hi
	beq	.L766
	ldr	x0, [x19, 3400]
	mov	w2, w21
	mov	w1, w25
	bl	FlashEraseBlocks
.L766:
	mov	w0, w20
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 112
	ret
.L771:
	ldr	x1, [x19, 3400]
	ubfiz	x0, x5, 5, 16
	str	wzr, [x1, x0]
	mov	w1, w23
	ldrb	w0, [x8, w5, sxtw]
	bl	V2P_block
	and	w7, w0, 65535
	mov	w6, w7
	cbz	w22, .L768
	bl	IsBlkInVendorPart
	cbnz	w0, .L769
.L768:
	mov	w0, w6
	bl	FtlBbmIsBadBlock
	cbnz	w0, .L770
	ldr	x0, [x19, 3400]
	ubfiz	x2, x21, 5, 16
	lsl	w7, w7, 10
	add	x0, x0, x2
	str	w7, [x0, 4]
	ldr	x1, [x19, 3400]
	ldr	x0, [x19, 3480]
	add	x1, x1, x2
	ldr	x2, [x19, 3520]
	str	x0, [x1, 8]
	ldrh	w0, [x19, 316]
	mul	w0, w0, w21
	add	w21, w21, 1
	and	w21, w21, 65535
	sdiv	w0, w0, w9
	add	x0, x2, w0, sxtw 2
	str	x0, [x1, 16]
.L769:
	add	w5, w5, 1
	and	w5, w5, 65535
	b	.L767
.L770:
	add	w20, w20, 1
	and	w20, w20, 65535
	b	.L769
.L788:
	mov	w25, 0
	mov	w26, 2
	b	.L776
.L780:
	ldr	x1, [x19, 3400]
	ubfiz	x0, x5, 5, 16
	str	wzr, [x1, x0]
	mov	w1, w23
	ldrb	w0, [x27, w5, sxtw]
	bl	V2P_block
	and	w7, w0, 65535
	mov	w6, w7
	cbz	w22, .L778
	bl	IsBlkInVendorPart
	cbnz	w0, .L779
.L778:
	mov	w0, w6
	bl	FtlBbmIsBadBlock
	cbnz	w0, .L779
	ldr	x0, [x19, 3400]
	ubfiz	x2, x21, 5, 16
	add	w7, w24, w7, lsl 10
	add	x0, x0, x2
	str	w7, [x0, 4]
	ldr	x1, [x19, 3400]
	ldr	x0, [x19, 3472]
	add	x1, x1, x2
	ldr	x2, [x19, 3480]
	str	x0, [x1, 8]
	ldrh	w0, [x19, 316]
	mul	w0, w0, w21
	add	w21, w21, 1
	and	w21, w21, 65535
	sdiv	w0, w0, w28
	add	x0, x2, w0, sxtw 2
	str	x0, [x1, 16]
.L779:
	add	w5, w5, 1
	and	w5, w5, 65535
	b	.L777
	.size	FtlLowFormatEraseBlock, .-FtlLowFormatEraseBlock
	.align	2
	.global	FlashTestBlk
	.type	FlashTestBlk, %function
FlashTestBlk:
	stp	x29, x30, [sp, -144]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	and	w19, w0, 65535
	mrs	x0, sp_el0
	ldr	x1, [x0, 952]
	str	x1, [sp, 136]
	mov	x1, 0
	cmp	w19, 11
	bls	.L813
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	mov	w2, 32
	mov	w1, 165
	lsl	w19, w19, 10
	ldr	x0, [x20, 3496]
	str	x0, [sp, 48]
	add	x0, sp, 72
	str	x0, [sp, 56]
	bl	ftl_memset
	ldr	x0, [x20, 3496]
	mov	w2, 8
	mov	w1, 90
	bl	ftl_memset
	add	x0, sp, 40
	mov	w2, 1
	mov	w1, w2
	str	w19, [sp, 44]
	bl	FlashEraseBlocks
	mov	w3, 1
	add	x0, sp, 40
	mov	w2, w3
	mov	w1, w3
	bl	FlashProgPages
	ldr	w0, [sp, 40]
	cbnz	w0, .L814
	add	w0, w19, 1
	mov	w3, 1
	mov	w2, w3
	mov	w1, w3
	str	w0, [sp, 44]
	add	x0, sp, 40
	bl	FlashProgPages
	ldr	w0, [sp, 40]
	cmp	w0, 0
	csetm	w20, ne
.L811:
	add	x0, sp, 40
	mov	w2, 1
	mov	w1, 0
	str	w19, [sp, 44]
	bl	FlashEraseBlocks
.L809:
	mrs	x0, sp_el0
	ldr	x1, [sp, 136]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L812
	bl	__stack_chk_fail
.L814:
	mov	w20, -1
	b	.L811
.L813:
	mov	w20, 0
	b	.L809
.L812:
	mov	w0, w20
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 144
	ret
	.size	FlashTestBlk, .-FlashTestBlk
	.section	.rodata.str1.1
.LC99:
	.string	"FtlBbmTblFlush id=%x,page=%x,previd=%x cnt=%d\n"
.LC100:
	.string	"FtlBbmTblFlush error:%x\n"
.LC101:
	.string	"FtlBbmTblFlush error = %x error count = %d\n"
	.text
	.align	2
	.global	FtlBbmTblFlush
	.type	FtlBbmTblFlush, %function
FtlBbmTblFlush:
	stp	x29, x30, [sp, -80]!
	mov	w1, 0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	mov	x20, 0
	stp	x21, x22, [sp, 32]
	add	x22, x19, 360
	ldr	x2, [x19, 3512]
	str	x2, [x19, 3728]
	ldrh	w2, [x19, 314]
	ldr	x0, [x19, 3448]
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	str	x0, [x19, 3720]
	bl	ftl_memset
.L817:
	ldrh	w0, [x19, 258]
	add	x1, x19, 3712
	cmp	w0, w20
	bgt	.L818
	ldr	x21, [x1, 16]
	mov	w2, 16
	mov	w1, 255
	adrp	x25, .LC99
	mov	x0, x21
	bl	ftl_memset
	mov	w0, -3887
	strh	w0, [x21]
	adrp	x26, .LC100
	add	x20, x19, 360
	ldr	w0, [x22, 8]
	add	x25, x25, :lo12:.LC99
	str	w0, [x21, 4]
	add	x26, x26, :lo12:.LC100
	ldrh	w0, [x19, 360]
	mov	w24, 0
	strh	w0, [x21, 2]
	ldr	w0, [x22, 4]
	mov	w22, 0
	str	w0, [x21, 8]
	ldr	w0, [x19, 232]
	strh	w0, [x21, 12]
	ldr	w0, [x19, 2676]
	strh	w0, [x21, 14]
.L819:
	ldr	x0, [x19, 3448]
	str	x0, [x19, 3720]
	ldr	x0, [x19, 3512]
	str	x0, [x19, 3728]
	ldrh	w1, [x20]
	ldrh	w2, [x20, 2]
	ldrh	w3, [x20, 4]
	ldrh	w4, [x21, 10]
	orr	w0, w2, w1, lsl 10
	str	wzr, [x19, 3712]
	str	w0, [x19, 3716]
	mov	x0, x25
	bl	sftl_printk
	ldrh	w0, [x19, 308]
	ldrh	w1, [x20, 2]
	sub	w0, w0, #1
	cmp	w1, w0
	blt	.L820
	ldr	w0, [x20, 8]
	mov	w2, 1
	strh	wzr, [x20, 2]
	add	w0, w0, 1
	str	w0, [x20, 8]
	str	w0, [x21, 4]
	ldrh	w0, [x20]
	ldrh	w1, [x20, 4]
	strh	w0, [x20, 4]
	strh	w0, [x21, 8]
	ldr	x0, [x19, 3400]
	strh	w1, [x20]
	lsl	w1, w1, 10
	str	w1, [x19, 3716]
	str	w1, [x0, 4]
	mov	w1, w2
	ldr	x0, [x19, 3400]
	bl	FlashEraseBlocks
.L820:
	add	x23, x19, 3712
	mov	w3, 1
	mov	x0, x23
	mov	w2, w3
	mov	w1, w3
	bl	FlashProgPages
	ldrh	w0, [x20, 2]
	add	w0, w0, 1
	strh	w0, [x20, 2]
	ldr	w0, [x19, 3712]
	cmn	w0, #1
	bne	.L821
	ldr	w1, [x19, 3716]
	add	w22, w22, 1
	and	w22, w22, 65535
	mov	x0, x26
	bl	sftl_printk
	cmp	w22, 3
	bls	.L819
	ldr	w1, [x19, 3716]
	adrp	x0, .LC101
	mov	w2, w22
	add	x0, x0, :lo12:.LC101
	bl	sftl_printk
.L823:
	b	.L823
.L818:
	ldrh	w2, [x19, 3624]
	add	x4, x22, x20, lsl 3
	ldr	x0, [x1, 8]
	ldr	x1, [x4, 32]
	mul	w3, w2, w20
	lsl	w2, w2, 2
	add	x20, x20, 1
	add	x0, x0, w3, sxtw 2
	bl	ftl_memcpy
	b	.L817
.L821:
	add	w24, w24, 1
	cmp	w24, 1
	ble	.L819
	cmp	w0, 256
	beq	.L819
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
	.size	FtlBbmTblFlush, .-FtlBbmTblFlush
	.align	2
	.global	allocate_data_superblock
	.type	allocate_data_superblock, %function
allocate_data_superblock:
	stp	x29, x30, [sp, -112]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	mov	x20, x0
	stp	x23, x24, [sp, 48]
	adrp	x23, .LANCHOR1
	add	x23, x23, :lo12:.LANCHOR1
	stp	x25, x26, [sp, 64]
	add	x25, x19, 128
	stp	x21, x22, [sp, 32]
	stp	x27, x28, [sp, 80]
.L829:
	ldrh	w1, [x19, 2584]
	ldrh	w0, [x19, 228]
	add	w0, w0, w1
	ldrh	w1, [x19, 244]
	cmp	w0, w1
	ble	.L830
	adrp	x0, .LC8
	add	x1, x23, 271
	add	x0, x0, :lo12:.LC8
	mov	w2, 2667
	bl	sftl_printk
.L830:
	cmp	x20, x25
	bne	.L855
	ldrh	w1, [x19, 228]
	mov	w2, 65535
	ldr	w0, [x19, 3236]
	mul	w0, w1, w0
	lsr	w1, w1, 1
	add	w0, w1, w0, lsr 2
	and	w1, w0, 65535
	cmp	w2, w0, uxth
	csel	w1, w1, wzr, ne
.L831:
	add	x0, x19, 2592
	bl	List_pop_index_node
	and	w22, w0, 65535
	ldrh	w0, [x19, 228]
	cbnz	w0, .L832
	adrp	x0, .LC8
	add	x1, x23, 271
	add	x0, x0, :lo12:.LC8
	mov	w2, 2676
	bl	sftl_printk
.L832:
	ldrh	w0, [x19, 228]
	sub	w0, w0, #1
	strh	w0, [x19, 228]
	ldrh	w0, [x19, 244]
	cmp	w0, w22
	bls	.L829
	ldr	x0, [x19, 72]
	ubfiz	x24, x22, 1, 16
	ldrh	w21, [x0, x24]
	cbnz	w21, .L829
	strh	w22, [x20]
	mov	x0, x20
	bl	make_superblock
	ldrb	w0, [x20, 7]
	cbnz	w0, .L834
	ldr	x0, [x19, 72]
	mov	w1, -1
	strh	w1, [x0, x24]
	mov	w0, w22
	bl	INSERT_DATA_LIST
	ldrh	w1, [x19, 2584]
	ldrh	w0, [x19, 228]
	add	w0, w0, w1
	ldrh	w1, [x19, 244]
	cmp	w0, w1
	ble	.L829
	add	x1, x23, 271
	mov	w2, 2690
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
	b	.L829
.L855:
	mov	w1, 0
	b	.L831
.L834:
	ldrh	w1, [x19, 2584]
	ldrh	w0, [x19, 228]
	add	w0, w0, w1
	ldrh	w1, [x19, 244]
	cmp	w0, w1
	ble	.L836
	adrp	x0, .LC8
	add	x1, x23, 271
	add	x0, x0, :lo12:.LC8
	mov	w2, 2693
	bl	sftl_printk
.L836:
	ldrh	w3, [x19, 236]
	add	x4, x20, 16
	mov	x0, 0
	mov	w5, 65535
.L837:
	cmp	w3, w0, uxth
	bhi	.L839
	cbnz	w21, .L840
	adrp	x0, .LC8
	add	x1, x23, 271
	add	x0, x0, :lo12:.LC8
	mov	w2, 2704
	bl	sftl_printk
.L840:
	ldrh	w0, [x19, 176]
	cmp	w0, w22
	bne	.L841
	adrp	x0, .LC8
	add	x1, x23, 271
	add	x0, x0, :lo12:.LC8
	mov	w2, 2706
	bl	sftl_printk
.L841:
	ldrb	w2, [x20, 8]
	ldr	x0, [x19, 2600]
	ldrh	w1, [x0, x24]
	cbnz	w2, .L842
	cbz	w1, .L856
	ldrh	w2, [x19, 296]
	add	w1, w1, w2
	and	w1, w1, 65535
.L843:
	strh	w1, [x0, x24]
	ldr	w0, [x19, 2668]
	add	w0, w0, 1
	str	w0, [x19, 2668]
.L844:
	ldr	x0, [x19, 2600]
	ldr	w1, [x19, 2684]
	ldrh	w0, [x0, x24]
	cmp	w0, w1
	bls	.L845
	str	w0, [x19, 2684]
.L845:
	ldr	w1, [x19, 2672]
	ldrh	w0, [x19, 296]
	ldr	w2, [x19, 2668]
	madd	w0, w0, w2, w1
	ldrh	w1, [x19, 244]
	udiv	w0, w0, w1
	ldr	x1, [x19, 3544]
	str	w0, [x19, 2676]
	ldr	w0, [x1, 16]
	add	w0, w0, 1
	str	w0, [x1, 16]
	mov	x1, 0
.L846:
	ldr	x0, [x19, 3400]
	cmp	w21, w1, uxth
	bhi	.L847
	ldrb	w1, [x20, 8]
	mov	w2, w21
	mov	x26, 0
	mov	w27, 0
	bl	FlashEraseBlocks
	add	x2, x20, 16
.L848:
	cmp	w21, w26, uxth
	bhi	.L850
	cmp	w27, 0
	ble	.L851
	mov	w0, w22
	bl	update_multiplier_value
	bl	FtlBbmTblFlush
.L851:
	ldrb	w0, [x20, 7]
	cbnz	w0, .L852
	ldr	x0, [x19, 72]
	mov	w1, -1
	strh	w1, [x0, x24]
	b	.L829
.L839:
	ldr	x1, [x19, 3400]
	add	x2, x1, x0, lsl 5
	stp	xzr, xzr, [x2, 8]
	ldrh	w2, [x4, x0, lsl 1]
	cmp	w2, w5
	beq	.L838
	ubfiz	x6, x21, 5, 16
	add	w21, w21, 1
	add	x1, x1, x6
	and	w21, w21, 65535
	lsl	w2, w2, 10
	str	w2, [x1, 4]
.L838:
	add	x0, x0, 1
	b	.L837
.L856:
	mov	w1, 2
	b	.L843
.L842:
	add	w1, w1, 1
	strh	w1, [x0, x24]
	ldr	w0, [x19, 2672]
	add	w0, w0, 1
	str	w0, [x19, 2672]
	b	.L844
.L847:
	add	x0, x0, x1, lsl 5
	add	x1, x1, 1
	ldr	w2, [x0, 4]
	and	w2, w2, -1024
	str	w2, [x0, 4]
	b	.L846
.L850:
	ldr	x1, [x19, 3400]
	lsl	x0, x26, 5
	ldr	w28, [x1, x0]
	add	x3, x1, x26, lsl 5
	cmn	w28, #1
	bne	.L849
	ldr	w0, [x3, 4]
	add	w27, w27, 1
	str	x2, [sp, 104]
	ubfx	x0, x0, 10, 16
	bl	FtlBbmMapBadBlock
	ldr	x2, [sp, 104]
	strh	w28, [x2, x26, lsl 1]
	ldrb	w0, [x20, 7]
	sub	w0, w0, #1
	strb	w0, [x20, 7]
.L849:
	add	x26, x26, 1
	b	.L848
.L852:
	ldrh	w1, [x19, 306]
	strh	w22, [x20]
	strh	wzr, [x20, 2]
	strb	wzr, [x20, 6]
	mul	w0, w0, w1
	ldr	w1, [x19, 2660]
	str	w1, [x20, 12]
	and	w0, w0, 65535
	add	w1, w1, 1
	strh	w0, [x20, 4]
	str	w1, [x19, 2660]
	ldr	x1, [x19, 72]
	ldrh	w2, [x20]
	strh	w0, [x1, x2, lsl 1]
	ldrh	w0, [x20, 4]
	cbz	w0, .L853
	ldrb	w0, [x20, 7]
	cbnz	w0, .L854
.L853:
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 271
	add	x0, x0, :lo12:.LC8
	mov	w2, 2759
	bl	sftl_printk
.L854:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 112
	ret
	.size	allocate_data_superblock, .-allocate_data_superblock
	.section	.rodata.str1.1
.LC102:
	.string	"FtlGcFreeBadSuperBlk 0x%x\n"
	.text
	.align	2
	.global	FtlGcFreeBadSuperBlk
	.type	FtlGcFreeBadSuperBlk, %function
FtlGcFreeBadSuperBlk:
	stp	x29, x30, [sp, -96]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x25, x26, [sp, 64]
	and	w25, w0, 65535
	ldrh	w0, [x19, 3312]
	stp	x21, x22, [sp, 32]
	stp	x23, x24, [sp, 48]
	str	x27, [sp, 80]
	cbnz	w0, .L868
.L863:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldr	x27, [sp, 80]
	ldp	x29, x30, [sp], 96
	ret
.L867:
	add	x0, x19, 264
	mov	w1, w25
	mov	w20, 0
	ldrb	w0, [x0, w21, sxtw]
	bl	V2P_block
	and	w22, w0, 65535
.L864:
	ldrh	w0, [x19, 3312]
	cmp	w0, w20
	bhi	.L866
	add	w21, w21, 1
	and	w21, w21, 65535
.L862:
	ldrh	w0, [x19, 236]
	cmp	w0, w21
	bhi	.L867
	bl	FtlGcReFreshBadBlk
	b	.L863
.L866:
	ldrh	w0, [x24, w20, sxtw 1]
	add	w27, w20, 1
	cmp	w0, w22
	bne	.L865
	mov	w1, w22
	mov	x0, x26
	bl	sftl_printk
	mov	w0, w22
	bl	FtlBbmMapBadBlock
	bl	FtlBbmTblFlush
	ldrh	w23, [x19, 3312]
	add	x1, x24, w27, sxtw 1
	add	x0, x24, w20, uxth 1
	cmp	w20, w23
	sub	w2, w23, w20
	sub	w23, w23, #1
	ubfiz	x2, x2, 1, 16
	csel	x2, x2, xzr, ls
	bl	memmove
	strh	w23, [x19, 3312]
.L865:
	and	w20, w27, 65535
	b	.L864
.L868:
	adrp	x26, .LC102
	add	x24, x19, 3314
	add	x26, x26, :lo12:.LC102
	mov	w21, 0
	b	.L862
	.size	FtlGcFreeBadSuperBlk, .-FtlGcFreeBadSuperBlk
	.align	2
	.global	update_vpc_list
	.type	update_vpc_list, %function
update_vpc_list:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	and	w20, w0, 65535
	ubfiz	x0, x0, 1, 16
	ldr	x1, [x19, 72]
	ldrh	w0, [x1, x0]
	cbnz	w0, .L871
	ldrh	w0, [x19, 226]
	cmp	w0, w20
	bne	.L872
	mov	w0, -1
	strh	w0, [x19, 226]
.L872:
	ldrh	w0, [x19, 224]
	cmp	w0, w20
	bne	.L873
	mov	w0, -1
	strh	w0, [x19, 224]
.L873:
	ldrh	w0, [x19, 176]
	cmp	w0, w20
	bne	.L874
	mov	w0, -1
	strh	w0, [x19, 176]
.L875:
	mov	w1, w20
	add	x0, x19, 2568
	bl	List_remove_node
	ldrh	w0, [x19, 2584]
	cbnz	w0, .L877
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 296
	add	x0, x0, :lo12:.LC8
	mov	w2, 2835
	bl	sftl_printk
.L877:
	ldrh	w0, [x19, 2584]
	sub	w0, w0, #1
	strh	w0, [x19, 2584]
	mov	w0, w20
	bl	free_data_superblock
	mov	w0, w20
	bl	FtlGcFreeBadSuperBlk
	ldrh	w1, [x19, 2584]
	ldrh	w0, [x19, 228]
	add	w0, w0, w1
	ldrh	w1, [x19, 244]
	cmp	w0, w1
	ble	.L881
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 296
	add	x0, x0, :lo12:.LC8
	mov	w2, 2838
	bl	sftl_printk
.L881:
	mov	w0, 1
	b	.L870
.L874:
	ldrh	w0, [x19, 24]
	cmp	w0, w20
	beq	.L880
	ldrh	w0, [x19, 80]
	cmp	w0, w20
	beq	.L880
	ldrh	w0, [x19, 128]
	cmp	w0, w20
	bne	.L875
.L880:
	mov	w0, 0
.L870:
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L871:
	mov	w0, w20
	bl	List_update_data_list
	b	.L880
	.size	update_vpc_list, .-update_vpc_list
	.section	.rodata.str1.1
.LC103:
	.string	"decrement_vpc_count %x = %d\n"
	.text
	.align	2
	.global	decrement_vpc_count
	.type	decrement_vpc_count, %function
decrement_vpc_count:
	stp	x29, x30, [sp, -48]!
	mov	w1, 65535
	cmp	w1, w0, uxth
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	and	w20, w0, 65535
	str	x21, [sp, 32]
	add	x19, x19, :lo12:.LANCHOR0
	beq	.L884
	ldr	x1, [x19, 72]
	ubfiz	x21, x20, 1, 16
	ldrh	w0, [x1, x21]
	cbnz	w0, .L885
	mov	w1, w20
	mov	w2, 0
	adrp	x0, .LC103
	add	x0, x0, :lo12:.LC103
	bl	sftl_printk
	ldr	x0, [x19, 72]
	ldrh	w0, [x0, x21]
	cbz	w0, .L886
.L891:
	mov	w21, 0
.L883:
	mov	w0, w21
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L886:
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	add	x1, x1, 312
	mov	w2, 2853
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
	b	.L891
.L885:
	sub	w0, w0, #1
	strh	w0, [x1, x21]
.L884:
	ldrh	w0, [x19, 3698]
	mov	w1, 65535
	cmp	w0, w1
	bne	.L888
	strh	w20, [x19, 3698]
	b	.L891
.L888:
	cmp	w0, w20
	beq	.L891
	bl	update_vpc_list
	cmp	w0, 0
	ldr	x1, [x19, 2560]
	strh	w20, [x19, 3698]
	ldr	x0, [x19, 2568]
	cset	w21, ne
	sub	x0, x0, x1
	mov	x1, -6148914691236517206
	movk	x1, 0xaaab, lsl 0
	asr	x0, x0, 1
	mul	x0, x0, x1
	ldr	x1, [x19, 72]
	and	x2, x0, 65535
	ldrh	w1, [x1, x2, lsl 1]
	cbnz	w1, .L883
	cmp	w20, w0, uxth
	beq	.L883
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	add	x1, x1, 312
	mov	w2, 2869
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
	b	.L883
	.size	decrement_vpc_count, .-decrement_vpc_count
	.align	2
	.global	FtlSuperblockPowerLostFix
	.type	FtlSuperblockPowerLostFix, %function
FtlSuperblockPowerLostFix:
	stp	x29, x30, [sp, -96]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	x19, x0
	mrs	x0, sp_el0
	ldr	x1, [x0, 952]
	str	x1, [sp, 88]
	mov	x1, 0
	mov	w0, -1
	ldr	x22, [x20, 3512]
	str	w0, [sp, 80]
	ldr	x0, [x20, 3448]
	str	x0, [sp, 64]
	mov	x0, -3
	str	x22, [sp, 72]
	movk	x0, 0xfffe, lsl 32
	str	x0, [x22, 8]
	mov	w1, 61589
	ldrh	w0, [x19]
	strh	wzr, [x22]
	strh	w0, [x22, 2]
	ldr	x0, [x20, 3448]
	str	w1, [x0]
	mov	w1, 22136
	movk	w1, 0x1234, lsl 16
	ldr	x0, [x20, 3448]
	str	w1, [x0, 4]
	ldrh	w21, [x19, 4]
	and	w21, w21, 1
	add	w21, w21, 6
.L893:
	ldrh	w0, [x19, 4]
	cbz	w21, .L895
	cbnz	w0, .L894
.L895:
	ldrh	w0, [x19]
	ldr	x2, [x20, 72]
	lsl	x0, x0, 1
	ldrh	w3, [x19, 4]
	ldrh	w1, [x2, x0]
	sub	w1, w1, w3
	strh	w1, [x2, x0]
	strh	wzr, [x19, 4]
	ldrh	w0, [x20, 306]
	strh	w0, [x19, 2]
	mrs	x0, sp_el0
	strb	wzr, [x19, 6]
	ldr	x1, [sp, 88]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L899
	bl	__stack_chk_fail
.L894:
	mov	x0, x19
	bl	get_new_active_ppa
	str	w0, [sp, 60]
	cmn	w0, #1
	beq	.L895
	ldr	w0, [x20, 2664]
	mov	w3, 0
	str	w0, [x22, 4]
	mov	w2, 0
	cmn	w0, #2
	mov	w1, 1
	csinc	w0, wzr, w0, eq
	str	w0, [x20, 2664]
	add	x0, sp, 56
	sub	w21, w21, #1
	bl	FlashProgPages
	ldrh	w0, [x19]
	bl	decrement_vpc_count
	b	.L893
.L899:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x29, x30, [sp], 96
	ret
	.size	FtlSuperblockPowerLostFix, .-FtlSuperblockPowerLostFix
	.align	2
	.global	FtlMakeBbt
	.type	FtlMakeBbt, %function
FtlMakeBbt:
	stp	x29, x30, [sp, -96]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x23, x24, [sp, 48]
	add	x24, x19, 360
	add	x20, x19, 3712
	stp	x25, x26, [sp, 64]
	mov	x25, x24
	mov	w23, 0
	stp	x27, x28, [sp, 80]
	mov	w27, 65535
	mov	w28, -3872
	stp	x21, x22, [sp, 32]
	bl	FtlBbtMemInit
	bl	FtlLoadFactoryBbt
.L906:
	ldrh	w0, [x19, 258]
	cmp	w0, w23
	bhi	.L912
	mov	w20, 0
.L913:
	ldrh	w0, [x19, 318]
	cmp	w0, w20
	bhi	.L914
	ldrh	w20, [x19, 372]
	add	x21, x19, 360
	mov	w22, 65535
	sub	w20, w20, #1
	and	w20, w20, 65535
.L915:
	ldrh	w0, [x21, 12]
	sub	w0, w0, #48
	cmp	w20, w0
	ble	.L919
	mov	w0, w20
	bl	FtlBbmIsBadBlock
	cmp	w0, 1
	beq	.L916
	mov	w0, w20
	bl	FlashTestBlk
	cbz	w0, .L917
	mov	w0, w20
	bl	FtlBbmMapBadBlock
.L916:
	sub	w20, w20, #1
	and	w20, w20, 65535
	b	.L915
.L912:
	ldrh	w1, [x25, 12]
	ldr	x0, [x19, 3448]
	ldr	x26, [x19, 3512]
	stp	x0, x26, [x20, 8]
	cmp	w1, w27
	beq	.L907
	ldrh	w22, [x19, 302]
	mov	w2, 1
	madd	w22, w22, w23, w1
	mov	w1, w2
	lsl	w0, w22, 10
	str	w0, [x20, 4]
	mov	x0, x20
	bl	FlashReadPages
	ldrh	w2, [x19, 302]
	ldr	x1, [x20, 8]
	add	w2, w2, 7
	ldr	x0, [x24, 32]
	lsr	w2, w2, 3
	bl	ftl_memcpy
.L908:
	mov	w0, w22
	add	w23, w23, 1
	add	x24, x24, 8
	add	x25, x25, 2
	bl	FtlBbmMapBadBlock
	b	.L906
.L907:
	mov	w1, w23
	bl	FlashGetBadBlockList
	ldr	x0, [x20, 8]
	ldr	x1, [x24, 32]
	bl	FtlBbt2Bitmap
	ldrh	w21, [x19, 302]
.L910:
	sub	w21, w21, #1
	and	w21, w21, 65535
.L909:
	ldrh	w0, [x19, 302]
	madd	w0, w23, w0, w21
	bl	FtlBbmIsBadBlock
	cmp	w0, 1
	beq	.L910
	ldr	x0, [x19, 3512]
	strh	w21, [x25, 12]
	mov	w2, 16
	mov	w1, 0
	bl	ftl_memset
	ldr	x0, [x19, 3448]
	mov	w2, 4096
	mov	w1, 0
	bl	ftl_memset
	strh	w28, [x26]
	str	wzr, [x26, 4]
	ldrh	w22, [x19, 302]
	ldrh	w0, [x25, 12]
	strh	w0, [x26, 2]
	ldrh	w2, [x19, 3624]
	ldr	x1, [x24, 32]
	madd	w22, w22, w23, w0
	lsl	w2, w2, 2
	lsl	w0, w22, 10
	str	w0, [x20, 4]
	ldr	x0, [x20, 8]
	bl	ftl_memcpy
	mov	w2, 1
	mov	x0, x20
	mov	w1, w2
	bl	FlashEraseBlocks
	mov	w3, 1
	mov	x0, x20
	mov	w1, w3
	mov	w2, w3
	bl	FlashProgPages
	ldr	w1, [x20]
	and	w0, w22, 65535
	cmn	w1, #1
	bne	.L908
	bl	FtlBbmMapBadBlock
	b	.L909
.L914:
	mov	w0, w20
	add	w20, w20, 1
	and	w20, w20, 65535
	bl	FtlBbmMapBadBlock
	b	.L913
.L917:
	ldrh	w0, [x21]
	cmp	w0, w22
	bne	.L918
	strh	w20, [x21]
	b	.L916
.L918:
	strh	w20, [x21, 4]
.L919:
	ldr	x1, [x19, 3400]
	str	wzr, [x19, 368]
	ldrh	w0, [x19, 360]
	mov	w2, 2
	strh	wzr, [x19, 362]
	lsl	w0, w0, 10
	str	w0, [x1, 4]
	ldr	x0, [x19, 3400]
	ldrh	w1, [x19, 364]
	lsl	w1, w1, 10
	str	w1, [x0, 36]
	mov	w1, 1
	bl	FlashEraseBlocks
	ldrh	w0, [x19, 360]
	bl	FtlBbmMapBadBlock
	ldrh	w0, [x19, 364]
	bl	FtlBbmMapBadBlock
	bl	FtlBbmTblFlush
	strh	wzr, [x19, 362]
	ldr	w0, [x19, 368]
	ldrh	w1, [x19, 364]
	add	w0, w0, 1
	str	w0, [x19, 368]
	ldrh	w0, [x19, 360]
	strh	w1, [x19, 360]
	strh	w0, [x19, 364]
	bl	FtlBbmTblFlush
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 96
	ret
	.size	FtlMakeBbt, .-FtlMakeBbt
	.align	2
	.global	ftl_memcmp
	.type	ftl_memcmp, %function
ftl_memcmp:
	stp	x29, x30, [sp, -16]!
	uxtw	x2, w2
	mov	x29, sp
	bl	memcmp
	ldp	x29, x30, [sp], 16
	ret
	.size	ftl_memcmp, .-ftl_memcmp
	.align	2
	.global	js_hash
	.type	js_hash, %function
js_hash:
	mov	x4, x0
	mov	w0, 42982
	mov	x3, 0
	movk	w0, 0x47c6, lsl 16
.L928:
	cmp	w1, w3
	bhi	.L929
	ret
.L929:
	ldrb	w5, [x4, x3]
	lsr	w2, w0, 2
	add	w2, w2, w0, lsl 5
	add	x3, x3, 1
	add	w2, w2, w5
	eor	w0, w0, w2
	b	.L928
	.size	js_hash, .-js_hash
	.section	.rodata.str1.1
.LC104:
	.string	"FtlMapWritePage error = %x \n"
.LC105:
	.string	"FtlMapWritePage error = %x error count = %d\n"
	.text
	.align	2
	.global	FtlMapWritePage
	.type	FtlMapWritePage, %function
FtlMapWritePage:
	stp	x29, x30, [sp, -96]!
	mov	x29, sp
	stp	x23, x24, [sp, 48]
	mov	w23, w1
	adrp	x24, .LANCHOR1
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	mov	x19, x0
	add	x24, x24, :lo12:.LANCHOR1
	stp	x21, x22, [sp, 32]
	add	x21, x20, 3712
	mov	w22, 0
	stp	x25, x26, [sp, 64]
	mov	x25, x2
	ubfiz	x26, x23, 2, 32
	str	x27, [sp, 80]
.L931:
	ldr	w0, [x20, 2644]
	add	w0, w0, 1
	str	w0, [x20, 2644]
	ldrh	w0, [x20, 308]
	ldrh	w1, [x19, 2]
	sub	w0, w0, #1
	cmp	w1, w0
	bge	.L932
	ldrh	w1, [x19]
	mov	w0, 65535
	cmp	w1, w0
	bne	.L933
.L932:
	mov	x0, x19
	bl	Ftl_write_map_blk_to_last_page
.L933:
	ldrh	w1, [x19]
	ldr	x0, [x19, 16]
	ldrh	w0, [x0, x1, lsl 1]
	cbnz	w0, .L934
	adrp	x0, .LC8
	add	x1, x24, 332
	add	x0, x0, :lo12:.LC8
	mov	w2, 700
	bl	sftl_printk
.L934:
	ldrh	w1, [x19]
	ldrh	w0, [x19, 10]
	cmp	w1, w0
	bcc	.L935
	adrp	x0, .LC8
	add	x1, x24, 332
	add	x0, x0, :lo12:.LC8
	mov	w2, 701
	bl	sftl_printk
.L935:
	ldrh	w1, [x19]
	mov	w2, 16
	ldr	x0, [x19, 16]
	ldrh	w27, [x0, x1, lsl 1]
	mov	w1, 0
	ldrh	w0, [x19, 2]
	str	x25, [x21, 8]
	orr	w0, w0, w27, lsl 10
	str	w0, [x21, 4]
	ldr	x0, [x20, 3512]
	str	x0, [x21, 16]
	bl	ftl_memset
	ldr	x6, [x21, 16]
	ldr	w0, [x19, 48]
	str	w0, [x6, 4]
	strh	w23, [x6, 8]
	ldrh	w0, [x19, 4]
	strh	w0, [x6]
	strh	w27, [x6, 2]
	ldrh	w1, [x20, 314]
	ldr	x0, [x21, 8]
	bl	js_hash
	str	w0, [x6, 12]
	mov	w3, 1
	mov	x0, x21
	mov	w1, w3
	mov	w2, w3
	bl	FlashProgPages
	ldrh	w0, [x19, 2]
	add	w0, w0, 1
	strh	w0, [x19, 2]
	ldp	w0, w1, [x21]
	cmn	w0, #1
	bne	.L936
	adrp	x0, .LC104
	add	x0, x0, :lo12:.LC104
	bl	sftl_printk
	add	w22, w22, 1
	ldrh	w0, [x19, 2]
	and	w22, w22, 65535
	cmp	w0, 2
	bhi	.L937
	ldrh	w0, [x20, 308]
	sub	w0, w0, #1
	strh	w0, [x19, 2]
.L937:
	cmp	w22, 3
	bls	.L938
	ldr	w1, [x20, 3716]
	adrp	x0, .LC105
	mov	w2, w22
	add	x0, x0, :lo12:.LC105
	bl	sftl_printk
.L939:
	b	.L939
.L938:
	ldr	w0, [x19, 52]
	cbz	w0, .L931
.L941:
	b	.L941
.L936:
	ldr	x0, [x19, 40]
	str	w1, [x0, x26]
	ldrh	w0, [x19, 2]
	cmp	w0, 1
	beq	.L942
	ldr	w0, [x21]
	cmp	w0, 256
	beq	.L942
	ldr	w1, [x19, 56]
	cbz	w1, .L943
.L942:
	str	wzr, [x19, 56]
	b	.L931
.L943:
	cmn	w0, #1
	bne	.L944
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 332
	add	x0, x0, :lo12:.LC8
	mov	w2, 745
	bl	sftl_printk
.L944:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldr	x27, [sp, 80]
	ldp	x29, x30, [sp], 96
	ret
	.size	FtlMapWritePage, .-FtlMapWritePage
	.section	.rodata.str1.1
.LC106:
	.string	"region_id = %x phyAddr = %x\n"
.LC107:
	.string	"map_ppn:"
.LC108:
	.string	"load_l2p_region refresh = %x phyAddr = %x\n"
	.text
	.align	2
	.global	load_l2p_region
	.type	load_l2p_region, %function
load_l2p_region:
	stp	x29, x30, [sp, -64]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	and	w20, w1, 65535
	stp	x21, x22, [sp, 32]
	and	w21, w0, 65535
	ldrh	w1, [x19, 340]
	stp	x23, x24, [sp, 48]
	cmp	w1, w0, uxth
	bcs	.L956
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 348
	add	x0, x0, :lo12:.LC8
	mov	w2, 485
	bl	sftl_printk
.L956:
	ldr	x1, [x19, 3600]
	ubfiz	x0, x21, 2, 16
	ubfiz	x20, x20, 4, 16
	ldr	w23, [x1, x0]
	ldr	x0, [x19, 2608]
	add	x0, x0, x20
	ldr	x0, [x0, 8]
	cbnz	w23, .L957
	ldrh	w2, [x19, 314]
	mov	w1, 255
	bl	ftl_memset
	ldr	x0, [x19, 2608]
	strh	w21, [x0, x20]
	ldr	x0, [x19, 2608]
	add	x20, x0, x20
	str	wzr, [x20, 4]
.L958:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x29, x30, [sp], 64
	ret
.L957:
	add	x22, x19, 3712
	str	x0, [x19, 3720]
	ldr	x0, [x19, 3512]
	mov	w2, 1
	mov	w1, w2
	str	w23, [x19, 3716]
	str	x0, [x19, 3728]
	mov	x0, x22
	bl	FlashReadPages
	ldr	x24, [x19, 3728]
	ldrh	w0, [x24, 8]
	cmp	w0, w21
	beq	.L959
	mov	w2, w23
	mov	w1, w21
	adrp	x0, .LC106
	add	x0, x0, :lo12:.LC106
	bl	sftl_printk
	ldr	x1, [x19, 3728]
	mov	w3, 4
	adrp	x0, .LC94
	mov	w2, w3
	add	x0, x0, :lo12:.LC94
	bl	rknand_print_hex
	ldrh	w3, [x19, 340]
	adrp	x0, .LC107
	ldr	x1, [x19, 3600]
	add	x0, x0, :lo12:.LC107
	mov	w2, 4
	bl	rknand_print_hex
.L960:
	ldrh	w0, [x24, 8]
	cmp	w0, w21
	beq	.L962
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 348
	add	x0, x0, :lo12:.LC8
	mov	w2, 508
	bl	sftl_printk
.L962:
	ldr	x0, [x19, 2608]
	add	x0, x0, x20
	str	wzr, [x0, 4]
	ldr	x0, [x19, 2608]
	strh	w21, [x0, x20]
	b	.L958
.L959:
	ldr	w0, [x19, 3712]
	cmp	w0, 256
	bne	.L962
	mov	w2, w23
	mov	w1, w21
	adrp	x0, .LC108
	add	x0, x0, :lo12:.LC108
	bl	sftl_printk
	ldr	x0, [x19, 2608]
	mov	w1, w21
	add	x0, x0, x20
	ldr	x2, [x0, 8]
	add	x0, x19, 3632
	bl	FtlMapWritePage
	b	.L960
	.size	load_l2p_region, .-load_l2p_region
	.align	2
	.global	ftl_map_blk_gc
	.type	ftl_map_blk_gc, %function
ftl_map_blk_gc:
	stp	x29, x30, [sp, -96]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	adrp	x20, .LANCHOR0
	stp	x21, x22, [sp, 32]
	add	x20, x20, :lo12:.LANCHOR0
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	str	x27, [sp, 80]
	ldr	x21, [x0, 16]
	ldr	x24, [x0, 40]
	bl	ftl_free_no_use_map_blk
	ldrh	w1, [x19, 10]
	ldrh	w2, [x19, 8]
	sub	w1, w1, #5
	cmp	w2, w1
	blt	.L966
	ubfiz	x0, x0, 1, 16
	ldrh	w25, [x21, x0]
	cbz	w25, .L966
	ldr	w1, [x19, 52]
	cbnz	w1, .L966
	mov	w1, 1
	str	w1, [x19, 52]
	strh	wzr, [x21, x0]
	ldrh	w0, [x19, 8]
	ldrh	w1, [x19, 2]
	sub	w0, w0, #1
	strh	w0, [x19, 8]
	ldrh	w0, [x20, 308]
	cmp	w1, w0
	bcc	.L967
	mov	x0, x19
	bl	ftl_map_blk_alloc_new_blk
.L967:
	adrp	x22, .LANCHOR1
	add	x22, x22, :lo12:.LANCHOR1
	adrp	x26, .LC8
	add	x22, x22, 364
	add	x26, x26, :lo12:.LC8
	mov	w21, 0
.L968:
	ldrh	w0, [x19, 6]
	cmp	w0, w21
	bhi	.L975
	mov	w0, w25
	mov	w1, 1
	bl	FtlFreeSysBlkQueueIn
	str	wzr, [x19, 52]
.L966:
	ldrh	w1, [x19, 2]
	ldrh	w0, [x20, 308]
	cmp	w1, w0
	bcc	.L976
	mov	x0, x19
	bl	ftl_map_blk_alloc_new_blk
.L976:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldr	x27, [sp, 80]
	ldp	x29, x30, [sp], 96
	ret
.L975:
	ubfiz	x1, x21, 2, 16
	add	x27, x24, x1
	ldr	w0, [x24, x1]
	cmp	w25, w0, lsr 10
	bne	.L969
	add	x0, x20, 3712
	ldr	x2, [x20, 3456]
	ldr	x23, [x20, 3512]
	stp	x2, x23, [x0, 8]
	mov	w2, 1
	ldr	w1, [x24, x1]
	str	w1, [x0, 4]
	mov	w1, w2
	bl	FlashReadPages
	ldrh	w0, [x23, 8]
	cmp	w0, w21
	beq	.L970
	mov	x1, x22
	mov	x0, x26
	mov	w2, 611
	bl	sftl_printk
.L970:
	ldr	w1, [x20, 3712]
	add	x0, x20, 3712
	cmn	w1, #1
	bne	.L971
.L973:
	str	wzr, [x27]
.L972:
	b	.L972
.L971:
	ldrh	w1, [x23, 8]
	cmp	w1, w21
	bne	.L973
	ldrh	w2, [x23]
	ldrh	w1, [x19, 4]
	cmp	w2, w1
	bne	.L973
	ldr	x2, [x0, 8]
	mov	w1, w21
	mov	x0, x19
	bl	FtlMapWritePage
.L969:
	add	w21, w21, 1
	and	w21, w21, 65535
	b	.L968
	.size	ftl_map_blk_gc, .-ftl_map_blk_gc
	.align	2
	.global	Ftl_write_map_blk_to_last_page
	.type	Ftl_write_map_blk_to_last_page, %function
Ftl_write_map_blk_to_last_page:
	stp	x29, x30, [sp, -64]!
	mov	w1, 65535
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	stp	x21, x22, [sp, 32]
	str	x23, [sp, 48]
	ldr	x20, [x0, 16]
	ldrh	w0, [x0]
	cmp	w0, w1
	bne	.L983
	ldrh	w0, [x19, 8]
	cbz	w0, .L984
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 379
	add	x0, x0, :lo12:.LC8
	mov	w2, 641
	bl	sftl_printk
.L984:
	ldrh	w0, [x19, 8]
	add	w0, w0, 1
	strh	w0, [x19, 8]
	bl	FtlFreeSysBlkQueueOut
	strh	w0, [x20]
	str	wzr, [x19]
	ldr	w0, [x19, 48]
	add	w0, w0, 1
	str	w0, [x19, 48]
.L985:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldr	x23, [sp, 48]
	ldp	x29, x30, [sp], 64
	ret
.L983:
	ubfiz	x0, x0, 1, 16
	ldrh	w1, [x19, 2]
	ldr	x23, [x19, 40]
	ldrh	w22, [x20, x0]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	ldr	w0, [x19, 48]
	orr	w1, w1, w22, lsl 10
	ldr	x21, [x20, 3512]
	str	w1, [x20, 3716]
	ldr	x1, [x20, 3448]
	str	x1, [x20, 3720]
	str	x21, [x20, 3728]
	mov	w1, 255
	str	w0, [x21, 4]
	mov	w0, -1291
	strh	w0, [x21, 8]
	ldrh	w0, [x19, 4]
	strh	w0, [x21]
	strh	w22, [x21, 2]
	ldrh	w2, [x20, 308]
	ldr	x0, [x20, 3448]
	lsl	w2, w2, 3
	bl	ftl_memset
	mov	w2, 0
	mov	w0, 0
.L986:
	ldrh	w1, [x19, 6]
	cmp	w1, w0
	bhi	.L988
	ldrh	w1, [x20, 314]
	add	x6, x20, 3712
	ldr	x0, [x20, 3720]
	bl	js_hash
	str	w0, [x21, 12]
	mov	w2, 1
	mov	x0, x6
	mov	w1, w2
	mov	w3, 0
	bl	FlashProgPages
	ldrh	w0, [x19, 2]
	add	w0, w0, 1
	strh	w0, [x19, 2]
	mov	x0, x19
	bl	ftl_map_blk_gc
	b	.L985
.L988:
	ubfiz	x3, x0, 2, 16
	ldr	w1, [x23, x3]
	cmp	w22, w1, lsr 10
	bne	.L987
	add	w1, w2, 1
	ldr	x5, [x20, 3448]
	ubfiz	x4, x1, 3, 16
	and	w2, w1, 65535
	ubfiz	x1, x1, 3, 16
	add	x1, x1, 4
	str	w0, [x5, x4]
	ldr	w4, [x23, x3]
	ldr	x3, [x20, 3448]
	str	w4, [x3, x1]
.L987:
	add	w0, w0, 1
	and	w0, w0, 65535
	b	.L986
	.size	Ftl_write_map_blk_to_last_page, .-Ftl_write_map_blk_to_last_page
	.align	2
	.global	flush_l2p_region
	.type	flush_l2p_region, %function
flush_l2p_region:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ubfiz	x20, x0, 4, 16
	ldr	x0, [x19, 2608]
	add	x1, x0, x20
	ldr	x2, [x1, 8]
	ldrh	w1, [x0, x20]
	add	x0, x19, 3632
	bl	FtlMapWritePage
	ldr	x0, [x19, 2608]
	add	x0, x0, x20
	ldr	w1, [x0, 4]
	and	w1, w1, 2147483647
	str	w1, [x0, 4]
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	flush_l2p_region, .-flush_l2p_region
	.align	2
	.global	log2phys
	.type	log2phys, %function
log2phys:
	stp	x29, x30, [sp, -80]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	w21, w0
	mov	x22, x1
	ldrh	w19, [x20, 312]
	stp	x23, x24, [sp, 48]
	mov	w23, w2
	add	w0, w19, 7
	mov	x19, 1
	str	x25, [sp, 64]
	lsr	w24, w21, w0
	and	w24, w24, 65535
	lsl	x19, x19, x0
	ldr	w0, [x20, 2624]
	sub	w19, w19, #1
	and	w19, w19, w21
	and	w19, w19, 65535
	cmp	w0, w21
	bhi	.L996
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	add	x1, x1, 410
	mov	w2, 820
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
	ldr	w0, [x20, 2624]
	cmp	w0, w21
	bhi	.L996
	mov	w0, -1
	cbnz	w23, .L995
	str	w0, [x22]
.L995:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
.L1002:
	add	x0, x0, 1
	lsl	x3, x0, 4
	ldrh	w3, [x1, x3]
	cmp	w3, w24
	bne	.L998
.L999:
	ldr	x0, [x20, 2608]
	ubfiz	x2, x25, 4, 16
	ubfiz	x19, x19, 2, 16
	add	x0, x0, x2
	ldr	x0, [x0, 8]
	cbnz	w23, .L1000
	ldr	w0, [x0, x19]
	str	w0, [x22]
.L1001:
	ldr	x0, [x20, 2608]
	add	x0, x0, x2
	ldr	w1, [x0, 4]
	cmn	w1, #1
	beq	.L1005
	add	w1, w1, 1
	str	w1, [x0, 4]
.L1005:
	mov	w0, 0
	b	.L995
.L1000:
	ldr	w1, [x22]
	str	w1, [x0, x19]
	strh	w24, [x20, 2616]
	ldr	x1, [x20, 2608]
	add	x1, x1, x2
	ldr	w0, [x1, 4]
	orr	w0, w0, -2147483648
	str	w0, [x1, 4]
	b	.L1001
.L996:
	ldr	x1, [x20, 2608]
	mov	x0, 0
	ldrh	w2, [x20, 342]
	sub	x1, x1, #16
.L998:
	and	w25, w0, 65535
	cmp	w2, w0, uxth
	bhi	.L1002
	bl	select_l2p_ram_region
	and	w25, w0, 65535
	mov	w21, w25
	ldr	x2, [x20, 2608]
	ubfiz	x1, x21, 4, 16
	add	x3, x2, x1
	ldrh	w2, [x2, x1]
	mov	w1, 65535
	cmp	w2, w1
	beq	.L1003
	ldr	w1, [x3, 4]
	tbz	w1, #31, .L1003
	bl	flush_l2p_region
.L1003:
	mov	w1, w21
	mov	w0, w24
	bl	load_l2p_region
	b	.L999
	.size	log2phys, .-log2phys
	.align	2
	.global	FtlWriteDump_data
	.type	FtlWriteDump_data, %function
FtlWriteDump_data:
	stp	x29, x30, [sp, -112]!
	mrs	x0, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	str	x23, [sp, 48]
	ldrh	w2, [x19, 28]
	ldr	x1, [x0, 952]
	str	x1, [sp, 104]
	mov	x1, 0
	cbz	w2, .L1009
	ldrb	w1, [x19, 32]
	cbnz	w1, .L1009
	ldrb	w1, [x19, 31]
	ldrh	w3, [x19, 306]
	mul	w1, w1, w3
	cmp	w2, w1
	beq	.L1009
	ldrb	w0, [x19, 34]
	cbnz	w0, .L1008
	ldr	w22, [x19, 2624]
	add	x1, sp, 68
	ldrh	w21, [x19, 236]
	mov	w2, 0
	sub	w22, w22, #1
	mov	w0, w22
	bl	log2phys
	ldr	x20, [x19, 3512]
	str	w22, [sp, 96]
	ldr	w0, [sp, 68]
	ldr	x1, [x19, 3448]
	str	w0, [sp, 76]
	stp	x1, x20, [sp, 80]
	str	wzr, [x20, 4]
	cmn	w0, #1
	beq	.L1011
	add	x0, sp, 72
	mov	w2, 0
	mov	w1, 1
	bl	FlashReadPages
.L1012:
	lsl	w21, w21, 2
	add	x23, x19, 24
	mov	w0, -3947
	strh	w0, [x20]
.L1013:
	cbnz	w21, .L1017
.L1014:
	mov	w0, 1
	strb	w0, [x19, 34]
.L1008:
	mrs	x0, sp_el0
	ldr	x1, [sp, 104]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1019
	bl	__stack_chk_fail
.L1011:
	ldrh	w2, [x19, 314]
	mov	w1, 255
	ldr	x0, [x19, 3448]
	bl	ftl_memset
	b	.L1012
.L1017:
	ldrh	w0, [x23, 4]
	cbz	w0, .L1014
	ldr	w0, [sp, 76]
	sub	w21, w21, #1
	stp	w22, w0, [x20, 8]
	ldrh	w0, [x23]
	strh	w0, [x20, 2]
	mov	x0, x23
	bl	get_new_active_ppa
	str	w0, [sp, 76]
	ldr	w0, [x19, 2664]
	mov	w3, 0
	str	w0, [x20, 4]
	mov	w2, 0
	cmn	w0, #2
	mov	w1, 1
	csinc	w0, wzr, w0, eq
	str	w0, [x19, 2664]
	add	x0, sp, 72
	bl	FlashProgPages
	ldrh	w0, [x23]
	bl	decrement_vpc_count
	b	.L1013
.L1009:
	strb	wzr, [x19, 34]
	b	.L1008
.L1019:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldr	x23, [sp, 48]
	ldp	x29, x30, [sp], 112
	ret
	.size	FtlWriteDump_data, .-FtlWriteDump_data
	.align	2
	.global	FtlReUsePrevPpa
	.type	FtlReUsePrevPpa, %function
FtlReUsePrevPpa:
	stp	x29, x30, [sp, -64]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	w21, w0
	ubfx	x0, x1, 10, 16
	str	w1, [sp, 60]
	bl	P2V_block_in_plane
	and	w4, w0, 65535
	mov	w22, w4
	ldr	x0, [x19, 72]
	ubfiz	x20, x22, 1, 16
	ldrh	w3, [x0, x20]
	cbnz	w3, .L1028
	ldr	x2, [x19, 2592]
	cbz	x2, .L1029
	ldr	x0, [x19, 2560]
	mov	x5, -6148914691236517206
	movk	x5, 0xaaab, lsl 0
	ldrh	w1, [x19, 228]
	sub	x2, x2, x0
	mov	w6, 65535
	asr	x2, x2, 1
	mul	x2, x2, x5
	mov	w5, 6
	and	w2, w2, 65535
.L1030:
	cmp	w3, w1
	beq	.L1029
	cmp	w2, w4
	bne	.L1031
	mov	w1, w22
	add	x0, x19, 2592
	bl	List_remove_node
	ldrh	w0, [x19, 228]
	cbnz	w0, .L1032
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 419
	add	x0, x0, :lo12:.LC8
	mov	w2, 1742
	bl	sftl_printk
.L1032:
	ldrh	w0, [x19, 228]
	sub	w0, w0, #1
	strh	w0, [x19, 228]
	mov	w0, w22
	bl	INSERT_DATA_LIST
	ldr	x1, [x19, 72]
	ldrh	w0, [x1, x20]
	add	w0, w0, 1
	strh	w0, [x1, x20]
.L1029:
	add	x1, sp, 60
	mov	w0, w21
	mov	w2, 1
	bl	log2phys
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x29, x30, [sp], 64
	ret
.L1031:
	umull	x2, w2, w5
	ldrh	w2, [x0, x2]
	cmp	w2, w6
	beq	.L1029
	add	w3, w3, 1
	and	w3, w3, 65535
	b	.L1030
.L1028:
	add	w3, w3, 1
	strh	w3, [x0, x20]
	b	.L1029
	.size	FtlReUsePrevPpa, .-FtlReUsePrevPpa
	.section	.rodata.str1.1
.LC109:
	.string	"data prev_ppa = %x error...................\n"
.LC110:
	.string	"spuer block %x vpn is 0\n "
	.text
	.align	2
	.global	FtlRecoverySuperblock
	.type	FtlRecoverySuperblock, %function
FtlRecoverySuperblock:
	stp	x29, x30, [sp, -176]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x20, x0
	mrs	x0, sp_el0
	stp	x21, x22, [sp, 32]
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	ldr	x1, [x0, 952]
	str	x1, [sp, 168]
	mov	x1, 0
	ldrh	w0, [x20]
	mov	w1, 65535
	cmp	w0, w1
	beq	.L1040
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldrb	w0, [x20, 6]
	ldrh	w24, [x20, 2]
	str	w0, [sp, 120]
	ldrh	w0, [x19, 306]
	cmp	w0, w24
	bne	.L1041
	strh	w0, [x20, 2]
	strh	wzr, [x20, 4]
.L1162:
	strb	wzr, [x20, 6]
.L1040:
	mrs	x0, sp_el0
	ldr	x1, [sp, 168]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1106
	bl	__stack_chk_fail
.L1041:
	ldrh	w0, [x20, 16]
	mov	w21, 0
.L1042:
	cmp	w0, w1
	beq	.L1043
	mov	w1, 1
	bl	FtlGetLastWrittenPage
	mov	w26, w0
	cmn	w0, #1
	beq	.L1044
	ldrh	w6, [x19, 236]
	add	x9, x20, 16
	ldrh	w7, [x19, 314]
	mov	x1, 0
	ldrh	w8, [x19, 316]
	mov	w22, 0
	mov	w10, 65535
	mov	w4, 4
.L1045:
	ldr	x0, [x19, 3424]
	cmp	w6, w1, uxth
	bhi	.L1049
	mov	w1, w22
	mov	w2, 0
	bl	FlashReadPages
	ldr	w21, [x19, 2664]
	ldr	x4, [x19, 3424]
	and	w9, w26, 65535
	sub	w21, w21, #1
	mov	w7, 65535
	mov	x6, x4
	mov	w3, 0
.L1050:
	cmp	w22, w3
	bne	.L1055
	add	w23, w26, 1
	ldr	w0, [x4, 4]
	and	w23, w23, 65535
.L1157:
	ubfx	x0, x0, 10, 16
	bl	P2V_plane
	and	w22, w0, 65535
	ldrh	w0, [x19, 306]
	cmp	w0, w23
	bne	.L1057
	strh	w23, [x20, 2]
	strh	wzr, [x20, 4]
	strb	wzr, [x20, 6]
.L1057:
	ldr	w0, [sp, 120]
	cmp	w23, w24
	ccmp	w22, w0, 0, eq
	bne	.L1058
.L1163:
	mov	w2, w22
	mov	w1, w23
	mov	x0, x20
	bl	ftl_sb_update_avl_pages
	b	.L1040
.L1043:
	add	w0, w21, 1
	and	w21, w0, 65535
	add	x0, x20, w0, uxth 1
	ldrh	w0, [x0, 16]
	b	.L1042
.L1044:
	cbz	w24, .L1046
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 435
	add	x0, x0, :lo12:.LC8
	mov	w2, 1809
	bl	sftl_printk
.L1046:
	ldr	w0, [sp, 120]
	cmp	w0, 0
	ccmp	w21, w0, 4, ne
	beq	.L1047
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 435
	add	x0, x0, :lo12:.LC8
	mov	w2, 1810
	bl	sftl_printk
.L1047:
	strh	wzr, [x20, 2]
	b	.L1162
.L1049:
	ldrh	w3, [x9, x1, lsl 1]
	cmp	w3, w10
	beq	.L1048
	ubfiz	x5, x22, 5, 16
	orr	w3, w26, w3, lsl 10
	add	x0, x0, x5
	str	w3, [x0, 4]
	mul	w0, w22, w7
	ldr	x2, [x19, 3424]
	sdiv	w0, w0, w4
	ldr	x3, [x19, 3256]
	add	x2, x2, x5
	add	x0, x3, w0, sxtw 2
	str	x0, [x2, 8]
	mul	w0, w22, w8
	add	w22, w22, 1
	ldr	x3, [x19, 3264]
	and	w22, w22, 65535
	sdiv	w0, w0, w4
	add	x0, x3, w0, sxtw 2
	str	x0, [x2, 16]
.L1048:
	add	x1, x1, 1
	b	.L1045
.L1055:
	ldr	w0, [x6]
	cbnz	w0, .L1051
	ldr	x8, [x6, 16]
	ldr	w5, [x8, 4]
	cmn	w5, #1
	beq	.L1052
	ldr	w1, [x19, 2664]
	mov	w0, w5
	bl	ftl_cmp_data_ver
	cbz	w0, .L1052
	add	w5, w5, 1
	str	w5, [x19, 2664]
.L1052:
	ldr	w0, [x8]
	cmn	w0, #1
	bne	.L1054
	ubfiz	x3, x3, 5, 16
	and	w23, w26, 65535
	add	x3, x4, x3
	ldr	w0, [x3, 4]
	b	.L1157
.L1051:
	mov	w7, w9
.L1054:
	add	w3, w3, 1
	add	x6, x6, 32
	and	w3, w3, 65535
	b	.L1050
.L1058:
	mov	w0, 65535
	cmp	w7, w0
	bne	.L1059
	ldrb	w0, [x20, 8]
	cbnz	w0, .L1060
.L1059:
	ldr	w0, [x19, 3708]
	and	w27, w26, 65535
	cmn	w0, #1
	bne	.L1061
	str	w21, [x19, 3708]
.L1061:
	add	w0, w24, 7
	sub	w25, w26, #7
	and	w25, w25, 65535
	cmp	w0, w26, uxth
	csel	w25, w25, w24, lt
	add	x4, x20, 16
	mov	w28, 65535
.L1063:
	ldrh	w3, [x19, 236]
	cmp	w25, w27
	bls	.L1108
	mov	w0, -1
	str	w0, [x19, 3708]
.L1060:
	mov	w0, 1
	mov	w27, w24
	strh	w0, [x19, 3744]
	adrp	x0, .LANCHOR1
	add	x0, x0, :lo12:.LANCHOR1
	add	x0, x0, 435
	str	x0, [sp, 128]
.L1071:
	ldrh	w3, [x19, 236]
	add	x4, x20, 16
	mov	x1, 0
	mov	w28, 0
	mov	w5, 65535
.L1072:
	ldr	x0, [x19, 3424]
	cmp	w3, w1, uxth
	bhi	.L1074
	mov	w1, w28
	mov	w2, 0
	bl	FlashReadPages
	str	xzr, [sp, 104]
	adrp	x0, .LC110
	add	x0, x0, :lo12:.LC110
	str	x0, [sp, 136]
.L1075:
	ldrh	w0, [sp, 104]
	cmp	w28, w0, uxth
	bhi	.L1102
	ldrh	w1, [x19, 306]
	add	w0, w27, 1
	and	w27, w0, 65535
	cmp	w1, w0, uxth
	bne	.L1071
	ldrh	w2, [x19, 236]
	add	x3, x20, 14
	mov	x0, 0
	mov	w4, 65535
	strh	w27, [x20, 2]
	strh	wzr, [x20, 4]
.L1103:
	cmp	w2, w0, uxth
	bls	.L1040
	add	x1, x0, 1
	ldrh	w5, [x3, x1, lsl 1]
	cmp	w5, w4
	beq	.L1109
	strb	w0, [x20, 6]
	b	.L1040
.L1065:
	ldrh	w2, [x4, x1, lsl 1]
	cmp	w2, w28
	beq	.L1064
	ubfiz	x5, x26, 5, 16
	add	w26, w26, 1
	add	x0, x0, x5
	and	w26, w26, 65535
	orr	w2, w25, w2, lsl 10
	str	w2, [x0, 4]
.L1064:
	add	x1, x1, 1
.L1070:
	ldr	x0, [x19, 3424]
	cmp	w3, w1, uxth
	bhi	.L1065
	mov	w1, w26
	mov	w2, 0
	str	x4, [sp, 104]
	bl	FlashReadPages
	ldr	x0, [x19, 3424]
	mov	w1, 32
	ldr	x4, [sp, 104]
	umaddl	x1, w1, w26, x0
.L1066:
	cmp	x0, x1
	bne	.L1069
	add	w25, w25, 1
	and	w25, w25, 65535
	b	.L1063
.L1108:
	mov	x1, 0
	mov	w26, 0
	b	.L1070
.L1069:
	ldr	w2, [x0]
	cbnz	w2, .L1060
	ldr	x2, [x0, 16]
	ldrh	w3, [x2]
	cmp	w3, w28
	beq	.L1068
	ldr	w2, [x2, 4]
	cmn	w2, #1
	beq	.L1068
	str	w2, [x19, 3708]
.L1068:
	add	x0, x0, 32
	b	.L1066
.L1074:
	ldrh	w2, [x4, x1, lsl 1]
	cmp	w2, w5
	beq	.L1073
	ubfiz	x6, x28, 5, 16
	add	w28, w28, 1
	add	x0, x0, x6
	and	w28, w28, 65535
	orr	w2, w27, w2, lsl 10
	str	w2, [x0, 4]
.L1073:
	add	x1, x1, 1
	b	.L1072
.L1102:
	ldr	x0, [sp, 104]
	ldr	x6, [x19, 3424]
	lsl	x26, x0, 5
	add	x5, x6, x0, lsl 5
	ldr	w4, [x5, 4]
	str	w4, [sp, 164]
	cmp	w27, w24
	bcc	.L1076
	ubfx	x0, x4, 10, 16
	bl	P2V_plane
	ldr	w1, [sp, 120]
	and	w0, w0, 65535
	cmp	w27, w24
	ccmp	w1, w0, 0, eq
	bhi	.L1076
	cmp	w27, w23
	ccmp	w22, w0, 0, eq
	bne	.L1077
	strh	w23, [x20, 2]
	strb	w22, [x20, 6]
	b	.L1163
.L1077:
	ldr	w0, [x6, x26]
	cmn	w0, #1
	beq	.L1078
	ldr	x3, [x5, 16]
	mov	w0, 61589
	ldrh	w4, [x3]
	cmp	w4, w0
	beq	.L1079
	ldrh	w0, [x20]
.L1159:
	bl	decrement_vpc_count
	b	.L1076
.L1079:
	ldr	w21, [x3, 4]
	cmn	w21, #1
	beq	.L1081
	ldr	w1, [x19, 2664]
	mov	w0, w21
	bl	ftl_cmp_data_ver
	cbz	w0, .L1081
	add	w0, w21, 1
	str	w0, [x19, 2664]
	ldrh	w0, [x3]
	cmp	w0, w4
	beq	.L1081
	ldr	x1, [sp, 128]
	mov	w2, 1961
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	str	x3, [sp, 112]
	bl	sftl_printk
	ldr	x3, [sp, 112]
.L1081:
	add	x1, sp, 160
	ldp	w25, w0, [x3, 8]
	mov	w2, 0
	str	w0, [sp, 156]
	mov	w0, w25
	bl	log2phys
	ldr	w1, [x19, 3708]
	ldr	w3, [sp, 156]
	cmn	w1, #1
	beq	.L1083
	mov	w0, w21
	bl	ftl_cmp_data_ver
	cbz	w0, .L1083
	cmn	w3, #1
	beq	.L1084
	ldr	x0, [x19, 3424]
	mov	w2, 0
	add	x0, x0, x26
	str	w3, [x0, 4]
	ldr	x1, [x0, 16]
	str	x1, [sp, 112]
	ldr	x0, [x19, 3424]
	mov	w1, 1
	add	x0, x0, x26
	bl	FlashReadPages
	ldr	x3, [x19, 3424]
	add	x4, x3, x26
	ldr	w0, [x3, x26]
	cmn	w0, #1
	bne	.L1085
.L1086:
	mov	w0, -1
	str	w0, [sp, 156]
.L1093:
	ldr	w0, [sp, 156]
	cmn	w0, #1
	beq	.L1076
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	and	w26, w0, 65535
	ldrh	w0, [x19, 244]
	mov	w25, w26
	cmp	w0, w26, uxth
	bhi	.L1098
	ldr	x1, [sp, 128]
	adrp	x0, .LC8
	mov	w2, 2066
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
.L1098:
	ldr	x1, [x19, 72]
	ubfiz	x0, x26, 1, 16
	ldrh	w0, [x1, x0]
	cbz	w0, .L1099
	mov	w0, w25
	b	.L1159
.L1084:
	ldp	w1, w0, [sp, 160]
	cmp	w1, w0
	bne	.L1076
	add	x1, sp, 156
	mov	w0, w25
	mov	w2, 1
	bl	log2phys
.L1076:
	ldr	x0, [sp, 104]
	add	x0, x0, 1
	str	x0, [sp, 104]
	b	.L1075
.L1085:
	ldr	x0, [sp, 112]
	ldr	w5, [x0, 8]
	cmp	w5, w25
	bne	.L1086
	ldr	w0, [x0, 4]
	str	w0, [sp, 124]
	uxtw	x1, w0
	ldr	w0, [x19, 3708]
	bl	ftl_cmp_data_ver
	cbz	w0, .L1086
	ldp	w1, w0, [sp, 156]
	ldr	w2, [sp, 164]
	cmp	w0, w2
	bne	.L1088
	mov	w0, w5
.L1158:
	bl	FtlReUsePrevPpa
	b	.L1086
.L1088:
	cmp	w0, w1
	beq	.L1086
	cmn	w0, #1
	beq	.L1089
	str	w0, [x4, 4]
	mov	w2, 0
	ldr	x1, [x4, 16]
	str	x1, [sp, 112]
	ldr	x0, [x19, 3424]
	mov	w1, 1
	add	x0, x0, x26
	bl	FlashReadPages
.L1090:
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	ldr	x1, [x0, 3424]
	ldr	w1, [x1, x26]
	cmn	w1, #1
	beq	.L1091
	ldr	x1, [sp, 112]
	ldr	w0, [x0, 3708]
	ldr	w3, [x1, 4]
	mov	w1, w3
	bl	ftl_cmp_data_ver
	cbz	w0, .L1091
	ldr	w0, [sp, 124]
	mov	w1, w3
	bl	ftl_cmp_data_ver
	cbz	w0, .L1086
.L1091:
	ldr	w1, [sp, 156]
	mov	w0, w25
	b	.L1158
.L1089:
	str	w0, [x3, x26]
	b	.L1090
.L1083:
	ldp	w1, w0, [sp, 160]
	cmp	w1, w0
	beq	.L1093
	cmn	w3, #1
	beq	.L1095
	ldr	w0, [x19, 252]
	cmp	w0, w3, lsr 10
	bhi	.L1095
	adrp	x0, .LC109
	mov	w1, w3
	add	x0, x0, :lo12:.LC109
.L1161:
	bl	sftl_printk
	b	.L1076
.L1095:
	add	x1, sp, 164
	mov	w0, w25
	mov	w2, 1
	bl	log2phys
	ldr	w4, [sp, 160]
	cmn	w4, #1
	beq	.L1093
	ldr	w0, [sp, 156]
	cmp	w4, w0
	beq	.L1093
	ubfx	x0, x4, 10, 16
	bl	P2V_block_in_plane
	ldrh	w2, [x19, 24]
	and	w1, w0, 65535
	cmp	w2, w0, uxth
	beq	.L1097
	ldrh	w0, [x19, 80]
	cmp	w0, w1
	beq	.L1097
	ldrh	w0, [x19, 128]
	cmp	w0, w1
	bne	.L1093
.L1097:
	ldr	x0, [x19, 3424]
	mov	w2, 0
	mov	w1, 1
	str	w4, [x0, 4]
	ldr	x26, [x0, 16]
	ldr	x0, [x19, 3424]
	bl	FlashReadPages
	ldr	x0, [x19, 3424]
	ldr	w0, [x0]
	cmn	w0, #1
	beq	.L1093
	ldr	w1, [x26, 4]
	mov	w0, w21
	bl	ftl_cmp_data_ver
	cbnz	w0, .L1093
	add	x1, sp, 160
	mov	w0, w25
	mov	w2, 1
	bl	log2phys
	b	.L1093
.L1099:
	mov	w1, w26
	ldr	x0, [sp, 136]
	b	.L1161
.L1078:
	ldr	w0, [x19, 3748]
	cmp	w0, 31
	bhi	.L1100
	add	x1, x19, 3752
	str	w4, [x1, w0, uxtw 2]
	add	w0, w0, 1
	str	w0, [x19, 3748]
.L1100:
	ldrh	w0, [x20]
	bl	decrement_vpc_count
	ldr	w0, [x19, 3708]
	cmn	w0, #1
	bne	.L1101
.L1160:
	str	w21, [x19, 3708]
	b	.L1076
.L1101:
	cmp	w0, w21
	bls	.L1076
	b	.L1160
.L1109:
	mov	x0, x1
	b	.L1103
.L1106:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 176
	ret
	.size	FtlRecoverySuperblock, .-FtlRecoverySuperblock
	.section	.rodata.str1.1
.LC111:
	.string	"...%s enter...\n"
.LC112:
	.string	"FtlCheckVpc2 %x = %x  %x\n"
.LC113:
	.string	"free blk vpc error %x = %x  %x\n"
	.text
	.align	2
	.global	ftl_check_vpc
	.type	ftl_check_vpc, %function
ftl_check_vpc:
	stp	x29, x30, [sp, -112]!
	mrs	x0, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	w19, 0
	adrp	x22, .LANCHOR1
	stp	x23, x24, [sp, 48]
	add	x22, x22, :lo12:.LANCHOR1
	adrp	x21, check_vpc_table
	stp	x25, x26, [sp, 64]
	add	x21, x21, :lo12:check_vpc_table
	stp	x27, x28, [sp, 80]
	ldr	x1, [x0, 952]
	str	x1, [sp, 104]
	mov	x1, 0
	adrp	x0, .LC111
	add	x1, x22, 457
	add	x0, x0, :lo12:.LC111
	bl	sftl_printk
	mov	x0, x21
	mov	w2, 8192
	mov	w1, 0
	bl	ftl_memset
.L1165:
	ldr	w0, [x20, 2624]
	cmp	w0, w19
	bhi	.L1167
	adrp	x23, .LC112
	add	x23, x23, :lo12:.LC112
	mov	w24, 0
	mov	w19, 0
	mov	w26, 65535
	mov	w27, 1
.L1168:
	ldrh	w0, [x20, 244]
	cmp	w0, w19
	bhi	.L1170
	ldr	x19, [x20, 2592]
	cbz	x19, .L1171
	ldr	x1, [x20, 2560]
	adrp	x25, .LC113
	ldrh	w26, [x20, 228]
	add	x25, x25, :lo12:.LC113
	sub	x19, x19, x1
	mov	x1, -6148914691236517206
	movk	x1, 0xaaab, lsl 0
	mov	w23, 0
	asr	x19, x19, 1
	mov	w27, 6
	mov	w28, 65535
	mul	x19, x19, x1
	and	w19, w19, 65535
.L1172:
	cmp	w23, w26
	bne	.L1174
.L1171:
	cbz	w24, .L1164
	adrp	x0, .LC8
	add	x1, x22, 457
	add	x0, x0, :lo12:.LC8
	mov	w2, 2394
	bl	sftl_printk
.L1164:
	mrs	x0, sp_el0
	ldr	x1, [sp, 104]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1176
	bl	__stack_chk_fail
.L1167:
	add	x1, sp, 100
	mov	w0, w19
	mov	w2, 0
	bl	log2phys
	ldr	w0, [sp, 100]
	cmn	w0, #1
	beq	.L1166
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	and	x0, x0, 65535
	ldrh	w1, [x21, x0, lsl 1]
	add	w1, w1, 1
	strh	w1, [x21, x0, lsl 1]
.L1166:
	add	w19, w19, 1
	b	.L1165
.L1170:
	ldr	x0, [x20, 72]
	ubfiz	x25, x19, 1, 16
	ldrh	w3, [x21, w19, sxtw 1]
	sxtw	x28, w19
	ldrh	w2, [x0, x25]
	cmp	w2, w3
	beq	.L1169
	mov	x0, x23
	mov	w1, w19
	bl	sftl_printk
	ldr	x0, [x20, 72]
	ldrh	w0, [x0, x25]
	cmp	w0, w26
	beq	.L1169
	ldrh	w1, [x21, x28, lsl 1]
	cmp	w1, w0
	csel	w24, w24, w27, ls
.L1169:
	add	w19, w19, 1
	and	w19, w19, 65535
	b	.L1168
.L1174:
	ldr	x1, [x20, 72]
	ubfiz	x0, x19, 1, 16
	ldrh	w2, [x1, x0]
	cbz	w2, .L1173
	ldrh	w3, [x21, w19, sxtw 1]
	mov	w24, 1
	mov	w1, w19
	mov	x0, x25
	bl	sftl_printk
.L1173:
	umull	x19, w19, w27
	ldr	x0, [x20, 2560]
	ldrh	w19, [x0, x19]
	cmp	w19, w28
	beq	.L1171
	add	w23, w23, 1
	and	w23, w23, 65535
	b	.L1172
.L1176:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 112
	ret
	.size	ftl_check_vpc, .-ftl_check_vpc
	.section	.rodata.str1.1
.LC114:
	.string	"ftl_scan_all_data = %x\n"
.LC115:
	.string	"scan lpa = %x ppa= %x\n"
.LC116:
	.string	"lpa = %x,addr= %x,spare= %x %x %x %x data=%x %x\n"
	.text
	.align	2
	.global	ftl_scan_all_data
	.type	ftl_scan_all_data, %function
ftl_scan_all_data:
	sub	sp, sp, #96
	mrs	x0, sp_el0
	stp	x29, x30, [sp, 16]
	add	x29, sp, 16
	stp	x19, x20, [sp, 32]
	mov	w20, 0
	stp	x21, x22, [sp, 48]
	adrp	x21, .LANCHOR0
	add	x21, x21, :lo12:.LANCHOR0
	str	x23, [sp, 64]
	adrp	x22, .LC115
	add	x19, x21, 3712
	add	x22, x22, :lo12:.LC115
	ldr	x1, [x0, 952]
	str	x1, [sp, 88]
	mov	x1, 0
	adrp	x0, .LC114
	add	x0, x0, :lo12:.LC114
	bl	sftl_printk
.L1191:
	ldr	w0, [x21, 2624]
	cmp	w0, w20
	bhi	.L1197
	mrs	x0, sp_el0
	ldr	x1, [sp, 88]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1198
	bl	__stack_chk_fail
.L1197:
	add	x1, sp, 84
	mov	w0, w20
	mov	w2, 0
	bl	log2phys
	tst	x20, 2047
	bne	.L1192
	ldr	w2, [sp, 84]
	mov	w1, w20
	mov	x0, x22
	bl	sftl_printk
.L1192:
	ldr	w0, [sp, 84]
	cmn	w0, #1
	beq	.L1194
	stp	wzr, w0, [x19]
	mov	w2, 0
	ldr	x0, [x21, 3448]
	mov	w1, 1
	ldr	x23, [x21, 3512]
	str	x0, [x19, 8]
	mov	x0, x19
	str	x23, [x19, 16]
	str	w20, [x19, 24]
	bl	FlashReadPages
	ldr	w0, [x19]
	cmp	w0, 256
	ccmn	w0, #1, 4, ne
	beq	.L1195
	ldr	w0, [x23, 8]
	cmp	w0, w20
	beq	.L1194
.L1195:
	ldp	x0, x1, [x19, 8]
	ldr	w2, [x0, 4]
	str	w2, [sp]
	ldr	w7, [x0]
	adrp	x0, .LC116
	ldp	w3, w4, [x1]
	add	x0, x0, :lo12:.LC116
	ldp	w5, w6, [x1, 8]
	mov	w1, w20
	ldr	w2, [x19, 4]
	bl	sftl_printk
.L1194:
	add	w20, w20, 1
	b	.L1191
.L1198:
	ldp	x29, x30, [sp, 16]
	ldp	x19, x20, [sp, 32]
	ldp	x21, x22, [sp, 48]
	ldr	x23, [sp, 64]
	add	sp, sp, 96
	ret
	.size	ftl_scan_all_data, .-ftl_scan_all_data
	.section	.rodata.str1.1
.LC117:
	.string	"FtlGcScanTempBlk Error ID %x %x!!!!!!! \n"
	.text
	.align	2
	.global	FtlGcScanTempBlk
	.type	FtlGcScanTempBlk, %function
FtlGcScanTempBlk:
	stp	x29, x30, [sp, -160]!
	mov	x29, sp
	stp	x21, x22, [sp, 32]
	adrp	x22, .LANCHOR2
	add	x22, x22, :lo12:.LANCHOR2
	stp	x19, x20, [sp, 16]
	mov	x20, x0
	mrs	x0, sp_el0
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	ldrh	w28, [x22, 4]
	str	w1, [sp, 104]
	ldr	x1, [x0, 952]
	str	x1, [sp, 152]
	mov	x1, 0
	mov	w0, 65535
	cmp	w28, w0
	beq	.L1224
	cbz	w28, .L1202
.L1201:
	adrp	x0, .LANCHOR0+306
	ldr	w1, [sp, 104]
	ldrh	w0, [x0, #:lo12:.LANCHOR0+306]
	cmp	w0, w1
	bne	.L1203
.L1202:
	bl	FtlGcPageVarInit
.L1203:
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	mov	w24, 0
	mov	w26, 65535
.L1219:
	ldrh	w0, [x20]
	strb	wzr, [x20, 8]
	cmp	w0, w26
	beq	.L1204
	add	x0, x20, 16
	str	x0, [sp, 96]
.L1205:
	ldrh	w7, [x19, 236]
	mov	x1, 0
	ldrh	w8, [x19, 314]
	mov	w21, 0
	ldrh	w9, [x19, 316]
	mov	w5, 4
.L1206:
	ldr	x0, [x19, 3424]
	cmp	w7, w1, uxth
	bhi	.L1208
	mov	x23, 0
	mov	w1, w21
	mov	w2, 0
	bl	FlashReadPages
.L1209:
	cmp	w21, w23, uxth
	bhi	.L1220
	ldr	w0, [sp, 104]
	add	w3, w28, 1
	add	w24, w24, 1
	and	w28, w3, 65535
	cmp	w0, w24
	bhi	.L1221
	ldrh	w0, [x22, 4]
	cmp	w0, w26
	beq	.L1221
	add	w0, w0, w24
	strh	w0, [x22, 4]
	ldrh	w0, [x19, 306]
	cmp	w0, w28
	bls	.L1204
.L1222:
	mrs	x0, sp_el0
	ldr	x1, [sp, 152]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1223
	bl	__stack_chk_fail
.L1224:
	mov	w28, 0
	b	.L1201
.L1208:
	ldr	x2, [sp, 96]
	ldrh	w4, [x2, x1, lsl 1]
	cmp	w4, w26
	beq	.L1207
	ubfiz	x6, x21, 5, 16
	orr	w4, w28, w4, lsl 10
	add	x0, x0, x6
	str	w4, [x0, 4]
	mul	w0, w21, w8
	ldr	x2, [x19, 3424]
	sdiv	w0, w0, w5
	ldr	x4, [x19, 3256]
	add	x2, x2, x6
	add	x0, x4, w0, sxtw 2
	str	x0, [x2, 8]
	mul	w0, w21, w9
	add	w21, w21, 1
	ldr	x4, [x19, 3264]
	and	w21, w21, 65535
	sdiv	w0, w0, w5
	add	x0, x4, w0, sxtw 2
	str	x0, [x2, 16]
.L1207:
	add	x1, x1, 1
	b	.L1206
.L1220:
	ldr	x0, [x19, 3424]
	lsl	x27, x23, 5
	add	x1, x0, x23, lsl 5
	ldr	w0, [x0, x27]
	ldr	w5, [x1, 4]
	ldr	x25, [x1, 16]
	cbnz	w0, .L1210
	ldrh	w0, [x25]
	cmp	w0, w26
	beq	.L1234
	ldr	w1, [x19, 2624]
	ldr	w0, [x25, 8]
	cmp	w0, w1
	bls	.L1212
.L1234:
	ldrh	w1, [x20]
	mov	w28, 0
	ldr	x0, [x19, 72]
	strh	wzr, [x0, x1, lsl 1]
	ldrh	w0, [x20]
	bl	INSERT_FREE_LIST
	mov	w0, -1
	strh	w0, [x20]
	strh	w0, [x19, 176]
	bl	FtlGcPageVarInit
	b	.L1219
.L1212:
	add	x1, sp, 116
	mov	w2, 0
	str	w5, [sp, 108]
	bl	log2phys
	ldr	w0, [x25, 12]
	ldr	w1, [sp, 116]
	ldr	w5, [sp, 108]
	cmp	w0, w1
	beq	.L1214
.L1216:
	ldr	w2, [x25, 8]
.L1235:
	ldr	w0, [x25, 12]
	mov	w1, w5
	add	x23, x23, 1
	bl	FtlGcUpdatePage
	b	.L1209
.L1214:
	cmn	w0, #1
	beq	.L1216
	str	w0, [sp, 124]
	mov	w2, 0
	ldr	x0, [x19, 3480]
	str	x0, [sp, 128]
	ldr	x0, [x19, 3520]
	mov	w1, 1
	str	w5, [sp, 108]
	str	x0, [sp, 136]
	add	x0, sp, 120
	bl	FlashReadPages
	ldr	w5, [sp, 108]
	ldrh	w1, [x19, 262]
	ldr	x2, [sp, 128]
	ldr	x0, [x19, 3424]
	lsl	w1, w1, 7
	sub	x2, x2, #4
	add	x27, x0, x27
	mov	x0, 0
.L1217:
	cmp	w1, w0
	bls	.L1216
	ldr	x6, [x27, 8]
	ldr	w7, [x6, x0, lsl 2]
	add	x0, x0, 1
	ldr	w6, [x2, x0, lsl 2]
	cmp	w7, w6
	beq	.L1217
	ldrh	w1, [x20]
	adrp	x0, .LC117
	ldr	w2, [sp, 124]
	add	x0, x0, :lo12:.LC117
	bl	sftl_printk
	b	.L1234
.L1210:
	mov	w2, -1
	b	.L1235
.L1221:
	ldrh	w0, [x19, 306]
	cmp	w0, w28
	bhi	.L1205
.L1204:
	strh	w28, [x20, 2]
	mov	w0, -1
	strb	wzr, [x20, 6]
	mov	w1, w28
	mov	w2, 0
	strh	w0, [x22, 4]
	mov	x0, x20
	bl	ftl_sb_update_avl_pages
	b	.L1222
.L1223:
	mov	w0, -1
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 160
	ret
	.size	FtlGcScanTempBlk, .-FtlGcScanTempBlk
	.align	2
	.global	FtlReadRefresh
	.type	FtlReadRefresh, %function
FtlReadRefresh:
	stp	x29, x30, [sp, -160]!
	mrs	x0, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	str	x21, [sp, 32]
	ldr	x1, [x0, 952]
	str	x1, [sp, 152]
	mov	x1, 0
	ldr	w0, [x19, 2624]
	add	x1, x19, 2712
	ldr	w2, [x1, 80]
	cbz	w2, .L1237
	ldr	w2, [x1, 84]
	cmp	w2, w0
	bcs	.L1238
	mov	w20, 2048
.L1243:
	ldr	w1, [x19, 2624]
	add	x21, x19, 2712
	ldr	w0, [x21, 84]
	cmp	w0, w1
	bcc	.L1239
.L1242:
	mov	w0, -1
.L1236:
	mrs	x1, sp_el0
	ldr	x2, [sp, 152]
	ldr	x3, [x1, 952]
	subs	x2, x2, x3
	mov	x3, 0
	beq	.L1247
	bl	__stack_chk_fail
.L1239:
	add	x1, sp, 52
	mov	w2, 0
	bl	log2phys
	ldr	w1, [sp, 52]
	ldr	w0, [x21, 84]
	add	w0, w0, 1
	str	w0, [x21, 84]
	cmn	w1, #1
	beq	.L1241
	str	w0, [sp, 80]
	mov	w2, 0
	ldr	x0, [x19, 3496]
	str	x0, [sp, 64]
	add	x0, sp, 88
	stp	wzr, w1, [sp, 56]
	mov	w1, 1
	str	x0, [sp, 72]
	add	x0, sp, 56
	bl	FlashReadPages
	ldr	w0, [sp, 56]
	cmp	w0, 256
	bne	.L1242
	ldr	w0, [sp, 52]
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	bl	FtlGcRefreshBlock
	b	.L1242
.L1241:
	subs	w20, w20, #1
	bne	.L1243
	b	.L1242
.L1238:
	ldr	w0, [x19, 2628]
	str	w0, [x1, 76]
	str	xzr, [x1, 80]
.L1250:
	mov	w0, 0
	b	.L1236
.L1237:
	ldr	w2, [x19, 2684]
	mov	w3, 10000
	mov	w5, 31
	ldr	w6, [x1, 76]
	cmp	w2, w3
	mov	w3, 63
	csel	w5, w5, w3, hi
	ldr	w3, [x19, 2628]
	ldrh	w4, [x19, 2540]
	add	w7, w3, 1048576
	cmp	w6, w7
	bhi	.L1246
	mov	w7, 1000
	lsr	w2, w2, 10
	add	w2, w2, 1
	mul	w0, w0, w7
	udiv	w0, w0, w2
	add	w0, w0, w6
	cmp	w3, w0
	bhi	.L1246
	tst	w5, w4
	bne	.L1250
	ldr	w0, [x1, 100]
	cmp	w0, w4
	beq	.L1250
.L1246:
	mov	x0, 1
	str	w3, [x19, 2788]
	str	x0, [x19, 2792]
	str	w4, [x19, 2812]
	b	.L1250
.L1247:
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 160
	ret
	.size	FtlReadRefresh, .-FtlReadRefresh
	.align	2
	.global	l2p_flush
	.type	l2p_flush, %function
l2p_flush:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	mov	w19, 0
	bl	FtlWriteDump_data
.L1254:
	ldrh	w0, [x20, 342]
	cmp	w0, w19
	bhi	.L1256
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L1256:
	ldr	x1, [x20, 2608]
	ubfiz	x0, x19, 4, 16
	add	x0, x1, x0
	ldr	w0, [x0, 4]
	tbz	w0, #31, .L1255
	mov	w0, w19
	bl	flush_l2p_region
.L1255:
	add	w19, w19, 1
	and	w19, w19, 65535
	b	.L1254
	.size	l2p_flush, .-l2p_flush
	.align	2
	.global	FtlVendorPartWrite
	.type	FtlVendorPartWrite, %function
FtlVendorPartWrite:
	stp	x29, x30, [sp, -224]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	mov	w21, w0
	mrs	x0, sp_el0
	stp	x23, x24, [sp, 48]
	mov	w20, w1
	ldrh	w22, [x19, 312]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	lsr	w22, w21, w22
	ldr	x1, [x0, 952]
	str	x1, [sp, 216]
	mov	x1, 0
	ldrh	w0, [x19, 300]
	add	w1, w21, w20
	cmp	w1, w0
	bhi	.L1267
	mov	x24, x2
	add	x27, x19, 3880
	mov	w25, 0
	mov	w28, -1
.L1260:
	cbnz	w20, .L1265
.L1258:
	mrs	x0, sp_el0
	ldr	x1, [sp, 216]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1266
	bl	__stack_chk_fail
.L1265:
	ldrh	w1, [x19, 262]
	ldr	x0, [x19, 3592]
	udiv	w23, w21, w1
	ldr	w2, [x0, w22, uxtw 2]
	msub	w23, w23, w1, w21
	sub	w0, w1, w23
	and	w26, w0, 65535
	cmp	w20, w0, uxth
	bcs	.L1261
	and	w26, w20, 65535
.L1261:
	ldr	x0, [x19, 3464]
	cbz	w2, .L1262
	cmp	w1, w26
	beq	.L1262
	str	x0, [sp, 128]
	add	x0, sp, 152
	str	w2, [sp, 124]
	mov	w2, 1
	mov	w1, w2
	str	x0, [sp, 136]
	add	x0, sp, 120
	bl	FlashReadPages
.L1263:
	lsl	w4, w26, 9
	sbfiz	x23, x23, 9, 23
	ldr	x0, [x19, 3464]
	mov	w2, w4
	mov	x1, x24
	str	w4, [sp, 108]
	add	x0, x0, x23
	bl	ftl_memcpy
	ldr	x2, [x19, 3464]
	mov	w1, w22
	mov	x0, x27
	sub	w20, w20, w26
	add	w21, w21, w26
	add	w22, w22, 1
	bl	FtlMapWritePage
	ldr	w4, [sp, 108]
	cmn	w0, #1
	csel	w25, w25, w28, ne
	add	x24, x24, w4, sxtw
	b	.L1260
.L1262:
	ldrh	w2, [x19, 314]
	mov	w1, 0
	bl	ftl_memset
	b	.L1263
.L1267:
	mov	w25, -1
	b	.L1258
.L1266:
	mov	w0, w25
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 224
	ret
	.size	FtlVendorPartWrite, .-FtlVendorPartWrite
	.align	2
	.global	Ftl_save_ext_data
	.type	Ftl_save_ext_data, %function
Ftl_save_ext_data:
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	mov	w1, 19539
	movk	w1, 0x4654, lsl 16
	ldr	w3, [x0, 2712]
	cmp	w3, w1
	bne	.L1275
	stp	x29, x30, [sp, -16]!
	add	x2, x0, 2712
	mov	w1, 89
	mov	x29, sp
	movk	w1, 0x5000, lsl 16
	str	w1, [x2, 4]
	ldr	w1, [x0, 2652]
	str	w1, [x2, 88]
	ldr	w1, [x0, 2656]
	str	w1, [x2, 92]
	ldr	w1, [x0, 2648]
	str	w1, [x2, 8]
	ldr	w1, [x0, 2636]
	str	w1, [x2, 12]
	ldr	w1, [x0, 2628]
	str	w1, [x2, 16]
	ldr	w1, [x0, 2644]
	str	w1, [x2, 20]
	ldr	w1, [x0, 2672]
	str	w1, [x2, 28]
	ldr	w1, [x0, 2680]
	str	w1, [x2, 32]
	ldr	w1, [x0, 2632]
	str	w1, [x2, 36]
	ldr	w1, [x0, 2640]
	str	w1, [x2, 40]
	ldr	w1, [x0, 2684]
	ldr	w0, [x0, 2688]
	stp	w1, w0, [x2, 44]
	mov	w1, 1
	mov	w0, 0
	bl	FtlVendorPartWrite
	ldp	x29, x30, [sp], 16
	ret
.L1275:
	ret
	.size	Ftl_save_ext_data, .-Ftl_save_ext_data
	.align	2
	.global	FtlEctTblFlush
	.type	FtlEctTblFlush, %function
FtlEctTblFlush:
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	ldrh	w1, [x2, 3944]
	cmp	w1, 31
	bhi	.L1282
	add	w1, w1, 1
	mov	w3, 1
	strh	w1, [x2, 3944]
.L1279:
	ldr	x1, [x2, 3544]
	ldr	w4, [x1, 16]
	cbnz	w0, .L1280
	ldr	w0, [x1, 20]
	add	w3, w3, w0
	cmp	w3, w4
	bhi	.L1284
.L1280:
	stp	x29, x30, [sp, -16]!
	mov	w0, 17221
	movk	w0, 0x4254, lsl 16
	mov	x29, sp
	str	w0, [x1]
	str	w4, [x1, 20]
	ldr	x0, [x2, 3544]
	ldrh	w1, [x2, 3528]
	str	wzr, [x0, 4]
	lsl	w3, w1, 9
	str	w3, [x0, 12]
	ldr	w3, [x0, 8]
	ldr	x2, [x2, 3544]
	add	w3, w3, 1
	str	w3, [x0, 8]
	mov	w0, 64
	bl	FtlVendorPartWrite
	bl	Ftl_save_ext_data
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
.L1282:
	mov	w3, 32
	b	.L1279
.L1284:
	mov	w0, 0
	ret
	.size	FtlEctTblFlush, .-FtlEctTblFlush
	.align	2
	.global	sftl_vendor_write
	.type	sftl_vendor_write, %function
sftl_vendor_write:
	stp	x29, x30, [sp, -16]!
	add	w0, w0, 256
	mov	x29, sp
	bl	FtlVendorPartWrite
	ldp	x29, x30, [sp], 16
	ret
	.size	sftl_vendor_write, .-sftl_vendor_write
	.section	.rodata.str1.1
.LC118:
	.string	"FtlVendorPartRead refresh = %x phyAddr = %x\n"
	.text
	.align	2
	.global	FtlVendorPartRead
	.type	FtlVendorPartRead, %function
FtlVendorPartRead:
	stp	x29, x30, [sp, -224]!
	adrp	x3, .LANCHOR0
	mov	x29, sp
	stp	x27, x28, [sp, 80]
	add	x28, x3, :lo12:.LANCHOR0
	stp	x19, x20, [sp, 16]
	mov	w20, w0
	mrs	x0, sp_el0
	stp	x21, x22, [sp, 32]
	mov	w19, w1
	ldrh	w21, [x28, 312]
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	lsr	w21, w20, w21
	ldr	x1, [x0, 952]
	str	x1, [sp, 216]
	mov	x1, 0
	ldrh	w0, [x28, 300]
	add	w1, w20, w19
	cmp	w1, w0
	bhi	.L1298
	adrp	x26, .LC118
	mov	x22, x2
	add	x26, x26, :lo12:.LC118
	mov	w24, 0
.L1290:
	cbnz	w19, .L1296
.L1288:
	mrs	x0, sp_el0
	ldr	x1, [sp, 216]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1297
	bl	__stack_chk_fail
.L1296:
	ldr	x0, [x28, 3592]
	ldr	w4, [x0, w21, uxtw 2]
	ldrh	w0, [x28, 262]
	udiv	w23, w20, w0
	msub	w23, w23, w0, w20
	sub	w0, w0, w23
	and	w25, w0, 65535
	cmp	w19, w0, uxth
	bcs	.L1291
	and	w25, w19, 65535
.L1291:
	lsl	w27, w25, 9
	cbz	w4, .L1292
	ldr	x0, [x28, 3464]
	str	x0, [sp, 128]
	add	x0, sp, 152
	mov	w2, 1
	mov	w1, w2
	str	w4, [sp, 108]
	str	w4, [sp, 124]
	str	x0, [sp, 136]
	add	x0, sp, 120
	bl	FlashReadPages
	ldr	w0, [sp, 120]
	ldr	w4, [sp, 108]
	cmn	w0, #1
	ldr	w0, [x28, 3712]
	csinv	w24, w24, wzr, ne
	cmp	w0, 256
	bne	.L1294
	mov	w2, w4
	mov	w1, w21
	mov	x0, x26
	bl	sftl_printk
	ldr	x2, [x28, 3464]
	mov	w1, w21
	add	x0, x28, 3880
	bl	FtlMapWritePage
.L1294:
	ldr	x1, [x28, 3464]
	sbfiz	x23, x23, 9, 23
	mov	w2, w27
	mov	x0, x22
	add	x1, x1, x23
	bl	ftl_memcpy
.L1295:
	add	w21, w21, 1
	sub	w19, w19, w25
	add	w20, w20, w25
	add	x22, x22, w27, sxtw
	b	.L1290
.L1292:
	mov	w2, w27
	mov	x0, x22
	mov	w1, 0
	bl	ftl_memset
	b	.L1295
.L1298:
	mov	w24, -1
	b	.L1288
.L1297:
	mov	w0, w24
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 224
	ret
	.size	FtlVendorPartRead, .-FtlVendorPartRead
	.section	.rodata.str1.1
.LC119:
	.string	"no ect"
	.text
	.align	2
	.global	FtlLoadEctTbl
	.type	FtlLoadEctTbl, %function
FtlLoadEctTbl:
	stp	x29, x30, [sp, -32]!
	mov	w0, 64
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldrh	w1, [x19, 3528]
	ldr	x2, [x19, 3544]
	bl	FtlVendorPartRead
	ldr	x0, [x19, 3544]
	ldr	w1, [x0]
	mov	w0, 17221
	movk	w0, 0x4254, lsl 16
	cmp	w1, w0
	beq	.L1301
	adrp	x1, .LC119
	adrp	x0, .LC77
	add	x1, x1, :lo12:.LC119
	add	x0, x0, :lo12:.LC77
	bl	sftl_printk
	ldr	x0, [x19, 3544]
	mov	w1, 0
	ldrh	w2, [x19, 3528]
	lsl	w2, w2, 9
	bl	ftl_memset
.L1301:
	mov	w0, 0
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlLoadEctTbl, .-FtlLoadEctTbl
	.align	2
	.global	Ftl_load_ext_data
	.type	Ftl_load_ext_data, %function
Ftl_load_ext_data:
	stp	x29, x30, [sp, -48]!
	mov	w1, 1
	mov	w0, 0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	str	x21, [sp, 32]
	add	x21, x19, 2712
	mov	x2, x21
	bl	FtlVendorPartRead
	ldr	w0, [x19, 2712]
	mov	w20, 19539
	movk	w20, 0x4654, lsl 16
	cmp	w0, w20
	beq	.L1304
	mov	x0, x21
	mov	w2, 512
	mov	w1, 0
	bl	ftl_memset
	str	w20, [x19, 2712]
.L1304:
	ldr	w1, [x19, 2800]
	str	w1, [x19, 2652]
	ldr	w1, [x19, 2804]
	ldr	w2, [x19, 2744]
	str	w1, [x19, 2656]
	ldr	w1, [x19, 2720]
	str	w1, [x19, 2648]
	ldr	w1, [x19, 2724]
	str	w2, [x19, 2680]
	ldr	w2, [x19, 2748]
	str	w2, [x19, 2632]
	ldr	w2, [x19, 2752]
	str	w1, [x19, 2636]
	ldr	w1, [x19, 2728]
	ldr	w0, [x19, 2760]
	str	w1, [x19, 2628]
	ldr	w1, [x19, 2732]
	str	w2, [x19, 2640]
	ldr	w2, [x19, 2756]
	str	w1, [x19, 2644]
	ldr	w1, [x19, 2740]
	str	w2, [x19, 2684]
	ldr	w2, [x19, 2668]
	str	w0, [x19, 2688]
	ldrh	w0, [x19, 296]
	str	w1, [x19, 2672]
	ldr	x21, [sp, 32]
	madd	w0, w0, w2, w1
	ldrh	w1, [x19, 244]
	udiv	w0, w0, w1
	str	w0, [x19, 2676]
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 48
	ret
	.size	Ftl_load_ext_data, .-Ftl_load_ext_data
	.align	2
	.global	sftl_vendor_read
	.type	sftl_vendor_read, %function
sftl_vendor_read:
	stp	x29, x30, [sp, -16]!
	add	w0, w0, 256
	mov	x29, sp
	bl	FtlVendorPartRead
	ldp	x29, x30, [sp], 16
	ret
	.size	sftl_vendor_read, .-sftl_vendor_read
	.align	2
	.global	FtlMapBlkWriteDump_data
	.type	FtlMapBlkWriteDump_data, %function
FtlMapBlkWriteDump_data:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	str	x21, [sp, 32]
	ldr	w0, [x0, 56]
	cbz	w0, .L1308
	ldrh	w1, [x19, 6]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	str	wzr, [x19, 56]
	sub	w1, w1, #1
	add	x0, x20, 3712
	ldr	x2, [x19, 40]
	and	w21, w1, 65535
	ubfiz	x1, x1, 2, 16
	ldr	x3, [x20, 3456]
	ldr	x4, [x20, 3512]
	stp	x3, x4, [x0, 8]
	ldr	w1, [x2, x1]
	str	w1, [x0, 4]
	cbz	w1, .L1310
	mov	w2, 1
	mov	w1, w2
	bl	FlashReadPages
.L1311:
	ldr	x2, [x20, 3720]
	mov	w1, w21
	mov	x0, x19
	bl	FtlMapWritePage
.L1308:
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L1310:
	ldrh	w2, [x20, 314]
	mov	x0, x3
	mov	w1, 255
	bl	ftl_memset
	b	.L1311
	.size	FtlMapBlkWriteDump_data, .-FtlMapBlkWriteDump_data
	.section	.rodata.str1.1
.LC120:
	.string	"FtlVpcTblFlush error = %x error count = %d\n"
	.text
	.align	2
	.global	FtlVpcTblFlush
	.type	FtlVpcTblFlush, %function
FtlVpcTblFlush:
	stp	x29, x30, [sp, -80]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	add	x21, x19, 2560
	add	x20, x19, 2512
	stp	x23, x24, [sp, 48]
	adrp	x23, .LANCHOR1
	add	x23, x23, :lo12:.LANCHOR1
	str	x25, [sp, 64]
	add	x23, x23, 471
	ldr	x22, [x19, 3512]
	str	x22, [x19, 3728]
	ldr	x0, [x19, 3448]
	str	x0, [x19, 3720]
	ldrh	w0, [x21, 136]
	mov	w24, 65535
	strh	w0, [x22, 2]
	mov	w0, -3932
	strh	w0, [x22]
	ldr	w0, [x21, 144]
	str	w0, [x22, 4]
	mov	x0, 19539
	str	xzr, [x22, 8]
	movk	x0, 0x4654, lsl 16
	ldrb	w1, [x19, 30]
	movk	x0, 0x59, lsl 32
	movk	x0, 0x5000, lsl 48
	str	x0, [x19, 2512]
	ldrh	w0, [x21, 142]
	strh	w0, [x19, 2520]
	ldrh	w0, [x19, 258]
	strb	w0, [x19, 2522]
	ldrh	w0, [x19, 24]
	strh	w0, [x19, 2526]
	ldrh	w0, [x19, 26]
	orr	w0, w1, w0, lsl 6
	strh	w0, [x19, 2528]
	ldrb	w0, [x19, 32]
	strb	w0, [x19, 2523]
	ldrh	w0, [x19, 80]
	ldrb	w1, [x19, 86]
	strh	w0, [x19, 2530]
	ldrh	w0, [x19, 82]
	ldrh	w2, [x19, 314]
	orr	w0, w1, w0, lsl 6
	strh	w0, [x19, 2532]
	ldrb	w0, [x19, 88]
	strb	w0, [x19, 2524]
	ldrh	w0, [x19, 128]
	ldrb	w1, [x19, 134]
	strh	w0, [x19, 2534]
	ldrh	w0, [x19, 130]
	orr	w0, w1, w0, lsl 6
	strh	w0, [x19, 2536]
	ldrb	w0, [x19, 136]
	mov	w1, 255
	strb	w0, [x19, 2525]
	ldr	w0, [x19, 2668]
	str	w0, [x19, 2544]
	ldr	w0, [x19, 2660]
	str	w0, [x19, 2552]
	ldr	w0, [x19, 2664]
	str	w0, [x19, 2548]
	ldr	x0, [x19, 3720]
	bl	ftl_memset
	ldr	x0, [x19, 3720]
	mov	x1, x20
	mov	w2, 48
	mov	w20, 0
	bl	ftl_memcpy
	ldrh	w2, [x19, 244]
	ldr	x1, [x19, 72]
	ldr	x0, [x19, 3720]
	lsl	w2, w2, 1
	add	x0, x0, 48
	bl	ftl_memcpy
	mov	w0, 0
	bl	FtlUpdateVaildLpn
.L1317:
	ldrh	w1, [x21, 136]
	ldrh	w2, [x21, 138]
	ldr	x0, [x19, 3448]
	str	x0, [x19, 3720]
	ldr	x0, [x19, 3512]
	str	x0, [x19, 3728]
	orr	w0, w2, w1, lsl 10
	str	w0, [x19, 3716]
	ldrh	w0, [x19, 308]
	sub	w0, w0, #1
	cmp	w2, w0
	blt	.L1318
	ldrh	w24, [x21, 140]
	strh	wzr, [x21, 138]
	strh	w1, [x21, 140]
	bl	FtlFreeSysBlkQueueOut
	ldr	w1, [x19, 2660]
	str	w1, [x21, 144]
	add	w2, w1, 1
	str	w2, [x19, 2660]
	ubfiz	w2, w0, 10, 16
	str	w2, [x19, 3716]
	strh	w0, [x22, 2]
	str	w1, [x22, 4]
	strh	w0, [x21, 136]
.L1318:
	ldrh	w1, [x19, 314]
	ldr	x0, [x19, 3448]
	bl	js_hash
	str	w0, [x22, 12]
	mov	w3, 1
	add	x0, x19, 3712
	mov	w1, w3
	mov	w2, w3
	bl	FlashProgPages
	ldrh	w0, [x21, 138]
	ldr	w1, [x19, 3712]
	add	w0, w0, 1
	and	w0, w0, 65535
	strh	w0, [x21, 138]
	cmn	w1, #1
	bne	.L1319
	cmp	w0, 1
	bne	.L1321
	mov	x1, x23
	mov	w2, 1145
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
	ldrh	w0, [x21, 138]
	cmp	w0, 1
	bne	.L1321
	ldrh	w0, [x19, 308]
	sub	w0, w0, #1
	strh	w0, [x21, 138]
.L1321:
	add	w20, w20, 1
	and	w20, w20, 65535
	cmp	w20, 3
	bls	.L1317
	ldr	w1, [x19, 3716]
	adrp	x0, .LC120
	mov	w2, w20
	add	x0, x0, :lo12:.LC120
	bl	sftl_printk
.L1324:
	b	.L1324
.L1319:
	cmp	w0, 1
	beq	.L1317
	cmp	w1, 256
	beq	.L1317
	mov	w0, 65535
	cmp	w24, w0
	beq	.L1325
	mov	w0, w24
	mov	w1, 1
	bl	FtlFreeSysBlkQueueIn
.L1325:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
	.size	FtlVpcTblFlush, .-FtlVpcTblFlush
	.align	2
	.global	FtlSysFlush
	.type	FtlSysFlush, %function
FtlSysFlush:
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	bl	l2p_flush
	mov	w0, 1
	bl	FtlEctTblFlush
	bl	FtlVpcTblFlush
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
	.size	FtlSysFlush, .-FtlSysFlush
	.align	2
	.global	sftl_deinit
	.type	sftl_deinit, %function
sftl_deinit:
	adrp	x0, .LANCHOR2
	ldr	w0, [x0, #:lo12:.LANCHOR2]
	cmp	w0, 1
	bne	.L1338
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	bl	FtlSysFlush
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
.L1338:
	mov	w0, 0
	ret
	.size	sftl_deinit, .-sftl_deinit
	.align	2
	.global	sftl_discard
	.type	sftl_discard, %function
sftl_discard:
	stp	x29, x30, [sp, -64]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	str	x21, [sp, 32]
	mov	w20, w1
	mrs	x1, sp_el0
	ldr	x2, [x1, 952]
	str	x2, [sp, 56]
	mov	x2, 0
	ldr	w1, [x19, 344]
	cmp	w1, w0
	bls	.L1352
	cmp	w1, w20
	bcc	.L1352
	add	w2, w0, w20
	cmp	w1, w2
	bcc	.L1352
	cmp	w20, 31
	bhi	.L1343
.L1360:
	mov	w0, 0
.L1341:
	mrs	x1, sp_el0
	ldr	x2, [sp, 56]
	ldr	x3, [x1, 952]
	subs	x2, x2, x3
	mov	x3, 0
	beq	.L1349
	bl	__stack_chk_fail
.L1343:
	ldrh	w1, [x19, 262]
	udiv	w21, w0, w1
	msub	w0, w1, w21, w0
	ands	w0, w0, 65535
	beq	.L1344
	sub	w0, w1, w0
	add	w21, w21, 1
	cmp	w0, w20
	csel	w0, w0, w20, ls
	sub	w20, w20, w0, uxth
.L1344:
	mov	w0, -1
	str	w0, [sp, 52]
.L1345:
	ldrh	w0, [x19, 262]
	cmp	w0, w20
	bls	.L1347
	ldr	w0, [x19, 3948]
	cmp	w0, 32
	bls	.L1360
	str	wzr, [x19, 3948]
	bl	l2p_flush
	bl	FtlVpcTblFlush
	b	.L1360
.L1347:
	add	x1, sp, 48
	mov	w0, w21
	mov	w2, 0
	bl	log2phys
	ldr	w0, [sp, 48]
	cmn	w0, #1
	beq	.L1346
	ldr	w0, [x19, 3948]
	add	x1, sp, 52
	mov	w2, 1
	add	w0, w0, 1
	str	w0, [x19, 3948]
	ldr	w0, [x19, 2632]
	add	w0, w0, 1
	str	w0, [x19, 2632]
	mov	w0, w21
	bl	log2phys
	ldr	w0, [sp, 48]
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	bl	decrement_vpc_count
.L1346:
	ldrh	w0, [x19, 262]
	add	w21, w21, 1
	sub	w20, w20, w0
	b	.L1345
.L1352:
	mov	w0, -1
	b	.L1341
.L1349:
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 64
	ret
	.size	sftl_discard, .-sftl_discard
	.section	.rodata.str1.1
.LC121:
	.string	"FtlCheckVpc %x = %x  %x\n"
	.text
	.align	2
	.global	FtlVpcCheckAndModify
	.type	FtlVpcCheckAndModify, %function
FtlVpcCheckAndModify:
	stp	x29, x30, [sp, -96]!
	mrs	x0, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	adrp	x22, .LANCHOR1
	add	x22, x22, :lo12:.LANCHOR1
	stp	x23, x24, [sp, 48]
	mov	w20, 0
	str	x25, [sp, 64]
	ldr	x1, [x0, 952]
	str	x1, [sp, 88]
	mov	x1, 0
	adrp	x0, .LC111
	add	x1, x22, 486
	add	x0, x0, :lo12:.LC111
	bl	sftl_printk
	ldr	x0, [x19, 3552]
	mov	w1, 0
	ldrh	w2, [x19, 246]
	lsl	w2, w2, 1
	bl	ftl_memset
.L1362:
	ldr	w0, [x19, 2624]
	cmp	w0, w20
	bhi	.L1364
	adrp	x23, .LC121
	adrp	x24, .LC8
	add	x23, x23, :lo12:.LC121
	add	x24, x24, :lo12:.LC8
	mov	w20, 0
	mov	w25, 65535
.L1365:
	ldrh	w0, [x19, 244]
	cmp	w0, w20
	bhi	.L1371
	bl	l2p_flush
	bl	FtlVpcTblFlush
	mrs	x0, sp_el0
	ldr	x1, [sp, 88]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1372
	bl	__stack_chk_fail
.L1364:
	add	x1, sp, 84
	mov	w0, w20
	mov	w2, 0
	bl	log2phys
	ldr	w0, [sp, 84]
	cmn	w0, #1
	beq	.L1363
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	ldr	x2, [x19, 3552]
	ubfiz	x0, x0, 1, 16
	ldrh	w1, [x2, x0]
	add	w1, w1, 1
	strh	w1, [x2, x0]
.L1363:
	add	w20, w20, 1
	b	.L1362
.L1371:
	ldr	x0, [x19, 72]
	ubfiz	x21, x20, 1, 16
	ldrh	w2, [x0, x21]
	ldr	x0, [x19, 3552]
	ldrh	w3, [x0, x21]
	cmp	w2, w3
	beq	.L1367
	cmp	w2, w25
	beq	.L1367
	mov	x0, x23
	mov	w1, w20
	bl	sftl_printk
	ldrh	w0, [x19, 24]
	cmp	w0, w20
	beq	.L1367
	ldrh	w0, [x19, 128]
	cmp	w0, w20
	beq	.L1367
	ldrh	w0, [x19, 80]
	cmp	w0, w20
	beq	.L1367
	ldr	x0, [x19, 72]
	ldr	x1, [x19, 3552]
	ldrh	w2, [x0, x21]
	ldrh	w1, [x1, x21]
	strh	w1, [x0, x21]
	cbnz	w2, .L1369
	mov	w1, w20
	add	x0, x19, 2592
	bl	List_remove_node
	ldrh	w0, [x19, 228]
	cbnz	w0, .L1370
	add	x1, x22, 486
	mov	x0, x24
	mov	w2, 2330
	bl	sftl_printk
.L1370:
	ldrh	w0, [x19, 228]
	sub	w0, w0, #1
	strh	w0, [x19, 228]
	mov	w0, w20
	bl	INSERT_DATA_LIST
.L1367:
	add	w20, w20, 1
	and	w20, w20, 65535
	b	.L1365
.L1369:
	mov	w0, w20
	bl	update_vpc_list
	b	.L1367
.L1372:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 96
	ret
	.size	FtlVpcCheckAndModify, .-FtlVpcCheckAndModify
	.align	2
	.global	allocate_new_data_superblock
	.type	allocate_new_data_superblock, %function
allocate_new_data_superblock:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	str	x21, [sp, 32]
	mov	x21, x0
	ldrh	w19, [x0]
	ldrh	w0, [x20, 244]
	cmp	w0, w19
	bcs	.L1378
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 507
	add	x0, x0, :lo12:.LC8
	mov	w2, 2766
	bl	sftl_printk
.L1378:
	mov	w0, 65535
	cmp	w19, w0
	beq	.L1379
	ldr	x1, [x20, 72]
	ubfiz	x0, x19, 1, 16
	ldrh	w0, [x1, x0]
	cbz	w0, .L1380
	mov	w0, w19
	bl	INSERT_DATA_LIST
.L1379:
	mov	w0, 1
	strb	w0, [x21, 8]
	ldrh	w0, [x20, 3698]
	mov	w1, 65535
	cmp	w0, w1
	beq	.L1381
	cmp	w0, w19
	bne	.L1382
	ldr	x1, [x20, 72]
	ubfiz	x19, x19, 1, 16
	ldrh	w1, [x1, x19]
	cbz	w1, .L1383
.L1382:
	bl	update_vpc_list
.L1383:
	mov	w0, -1
	strh	w0, [x20, 3698]
.L1381:
	mov	x0, x21
	bl	allocate_data_superblock
	bl	l2p_flush
	mov	w0, 0
	bl	FtlEctTblFlush
	bl	FtlVpcTblFlush
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L1380:
	mov	w0, w19
	bl	INSERT_FREE_LIST
	b	.L1379
	.size	allocate_new_data_superblock, .-allocate_new_data_superblock
	.section	.rodata.str1.1
.LC122:
	.string	"FtlProgPages error %x = %d\n"
.LC123:
	.string	"Ftlwrite decrement_vpc_count %x = %d\n"
	.text
	.align	2
	.global	FtlProgPages
	.type	FtlProgPages, %function
FtlProgPages:
	stp	x29, x30, [sp, -96]!
	mrs	x2, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x20, x3
	mov	x19, x0
	stp	x21, x22, [sp, 32]
	mov	w21, w1
	stp	x23, x24, [sp, 48]
	adrp	x24, .LC122
	adrp	x23, .LANCHOR0
	str	x25, [sp, 64]
	add	x24, x24, :lo12:.LC122
	add	x23, x23, :lo12:.LANCHOR0
	mov	w25, 0
	ldr	x3, [x2, 952]
	str	x3, [sp, 88]
	mov	x3, 0
	mov	w2, 0
	ldrb	w3, [x20, 9]
	bl	FlashProgPages
	mov	w1, 32
	umaddl	x21, w21, w1, x19
.L1389:
	cmp	x21, x19
	bne	.L1396
	cbz	w25, .L1397
	bl	FtlSysFlush
.L1397:
	adrp	x0, .LANCHOR0+236
	ldrb	w1, [x20, 6]
	ldrh	w0, [x0, #:lo12:.LANCHOR0+236]
	cmp	w1, w0
	bcc	.L1388
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 536
	add	x0, x0, :lo12:.LC8
	mov	w2, 1005
	bl	sftl_printk
.L1388:
	mrs	x0, sp_el0
	ldr	x1, [sp, 88]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1399
	bl	__stack_chk_fail
.L1391:
	ldr	w1, [x19, 4]
	mov	x0, x24
	bl	sftl_printk
	ldr	w0, [x19, 4]
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	bl	decrement_vpc_count
	ldrh	w0, [x20, 4]
	cbnz	w0, .L1390
	mov	x0, x20
	bl	allocate_new_data_superblock
.L1390:
	mov	x0, x20
	bl	get_new_active_ppa
	str	w0, [x19, 4]
	mov	w2, 0
	mov	w1, 1
	mov	w25, 1
	ldrb	w3, [x20, 9]
	str	w0, [sp, 84]
	mov	x0, x19
	bl	FlashProgPages
.L1396:
	ldr	w2, [x19]
	cmp	w2, 256
	ccmn	w2, #1, 4, ne
	beq	.L1391
	ldrb	w1, [x20, 6]
	ldrh	w0, [x23, 236]
	cmp	w1, w0
	bcc	.L1392
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 536
	add	x0, x0, :lo12:.LC8
	mov	w2, 986
	bl	sftl_printk
.L1392:
	ldr	w0, [x19, 4]
	add	x1, sp, 84
	str	w0, [sp, 84]
	mov	w2, 1
	ldr	w0, [x19, 24]
	bl	log2phys
	ldr	x0, [x19, 16]
	ldr	w0, [x0, 12]
	cmn	w0, #1
	beq	.L1393
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	and	w1, w0, 65535
	mov	w22, w1
	ldr	x2, [x23, 72]
	ubfiz	x0, x22, 1, 16
	ldrh	w0, [x2, x0]
	cbnz	w0, .L1394
	adrp	x0, .LC123
	mov	w2, 0
	add	x0, x0, :lo12:.LC123
	bl	sftl_printk
.L1394:
	mov	w0, w22
	bl	decrement_vpc_count
.L1393:
	add	x19, x19, 32
	b	.L1389
.L1399:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 96
	ret
	.size	FtlProgPages, .-FtlProgPages
	.align	2
	.global	FtlGcFreeTempBlock
	.type	FtlGcFreeTempBlock, %function
FtlGcFreeTempBlock:
	stp	x29, x30, [sp, -80]!
	mrs	x1, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	stp	x23, x24, [sp, 48]
	ldrh	w3, [x19, 128]
	ldr	x2, [x1, 952]
	str	x2, [sp, 72]
	mov	x2, 0
	ldrh	w1, [x19, 306]
	mov	w2, 65535
	cmp	w3, w2
	bne	.L1408
.L1415:
	ldrh	w2, [x19, 128]
	add	x0, x19, 128
	str	wzr, [x19, 3704]
	mov	w1, 65535
	cmp	w2, w1
	bne	.L1429
.L1433:
	mov	w0, 0
.L1407:
	mrs	x1, sp_el0
	ldr	x2, [sp, 72]
	ldr	x3, [x1, 952]
	subs	x2, x2, x3
	mov	x3, 0
	beq	.L1424
	bl	__stack_chk_fail
.L1408:
	adrp	x20, .LANCHOR2
	add	x20, x20, :lo12:.LANCHOR2
	cbz	w0, .L1411
	ldrh	w0, [x20, 4]
	cmp	w0, w2
	beq	.L1412
.L1413:
	mov	w1, 2
.L1411:
	add	x0, x19, 128
	bl	FtlGcScanTempBlk
	ldrh	w1, [x20, 4]
	str	w0, [sp, 68]
	mov	w0, 65535
	cmp	w1, w0
	beq	.L1415
	mov	w0, 1
	b	.L1407
.L1412:
	ldrh	w0, [x19, 228]
	strh	wzr, [x20, 4]
	cmp	w0, 17
	bhi	.L1413
	b	.L1411
.L1429:
	ldrb	w0, [x0, 7]
	ldrh	w2, [x19, 306]
	ldrh	w1, [x19, 3296]
	mul	w0, w0, w2
	cmp	w1, w0
	beq	.L1416
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 549
	add	x0, x0, :lo12:.LC8
	mov	w2, 164
	bl	sftl_printk
.L1416:
	ldrb	w0, [x19, 135]
	mov	w20, 0
	ldrh	w3, [x19, 306]
	mov	w24, 12
	ldrh	w2, [x19, 128]
	ldr	x1, [x19, 72]
	mul	w0, w0, w3
	strh	w0, [x1, x2, lsl 1]
	ldr	w1, [x19, 2648]
	ldrh	w0, [x19, 3296]
	add	w0, w0, w1
	str	w0, [x19, 2648]
.L1417:
	ldrh	w0, [x19, 3296]
	cmp	w0, w20
	bhi	.L1421
	mov	w0, -1
	bl	decrement_vpc_count
	ldrh	w2, [x19, 128]
	ldr	x1, [x19, 72]
	ldrh	w0, [x19, 128]
	ldrh	w1, [x1, x2, lsl 1]
	cbz	w1, .L1422
	bl	INSERT_DATA_LIST
.L1423:
	mov	w20, -1
	strh	w20, [x19, 128]
	strh	wzr, [x19, 3284]
	strh	wzr, [x19, 3296]
	bl	l2p_flush
	bl	FtlVpcTblFlush
	strh	w20, [x19, 176]
	ldrh	w0, [x19, 2692]
	ldrh	w1, [x19, 228]
	add	w0, w0, w0, lsl 1
	cmp	w1, w0, lsr 2
	ble	.L1433
	mov	w0, 20
	strh	w0, [x19, 3224]
	b	.L1433
.L1421:
	ldr	x23, [x19, 3304]
	umull	x22, w20, w24
	ldr	w1, [x19, 2624]
	add	x21, x23, x22
	ldr	w0, [x21, 8]
	cmp	w0, w1
	bcc	.L1418
.L1431:
	ldrh	w0, [x19, 128]
	b	.L1432
.L1418:
	add	x1, sp, 68
	mov	w2, 0
	bl	log2phys
	ldr	w1, [sp, 68]
	ldr	w0, [x23, x22]
	cmp	w0, w1
	bne	.L1420
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	mov	w22, w0
	ldr	w0, [x21, 8]
	add	x1, x21, 4
	mov	w2, 1
	bl	log2phys
	mov	w0, w22
.L1432:
	bl	decrement_vpc_count
	b	.L1419
.L1420:
	ldr	w0, [x21, 4]
	cmp	w1, w0
	bne	.L1431
.L1419:
	add	w20, w20, 1
	and	w20, w20, 65535
	b	.L1417
.L1422:
	bl	INSERT_FREE_LIST
	b	.L1423
.L1424:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x29, x30, [sp], 80
	ret
	.size	FtlGcFreeTempBlock, .-FtlGcFreeTempBlock
	.align	2
	.global	FtlGcPageRecovery
	.type	FtlGcPageRecovery, %function
FtlGcPageRecovery:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	add	x20, x19, 128
	mov	x0, x20
	ldrh	w1, [x19, 306]
	bl	FtlGcScanTempBlk
	ldrh	w1, [x19, 130]
	ldrh	w0, [x19, 306]
	cmp	w1, w0
	bcc	.L1434
	add	x0, x19, 3632
	bl	FtlMapBlkWriteDump_data
	mov	w0, 0
	bl	FtlGcFreeTempBlock
	str	wzr, [x19, 3704]
.L1434:
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlGcPageRecovery, .-FtlGcPageRecovery
	.align	2
	.global	FtlPowerLostRecovery
	.type	FtlPowerLostRecovery, %function
FtlPowerLostRecovery:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	add	x20, x19, 24
	add	x19, x19, 80
	mov	x0, x20
	str	wzr, [x19, 3668]
	bl	FtlRecoverySuperblock
	mov	x0, x20
	bl	FtlSlcSuperblockCheck
	mov	x0, x19
	bl	FtlRecoverySuperblock
	mov	x0, x19
	bl	FtlSlcSuperblockCheck
	bl	FtlGcPageRecovery
	mov	w0, -1
	bl	decrement_vpc_count
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	FtlPowerLostRecovery, .-FtlPowerLostRecovery
	.align	2
	.global	Ftl_gc_temp_data_write_back
	.type	Ftl_gc_temp_data_write_back, %function
Ftl_gc_temp_data_write_back:
	stp	x29, x30, [sp, -32]!
	mov	w3, 0
	mov	w2, 0
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldr	w1, [x19, 3240]
	ldr	x0, [x19, 3432]
	bl	FlashProgPages
	mov	w11, 0
.L1440:
	ldr	w1, [x19, 3240]
	ldr	x2, [x19, 3432]
	cmp	w11, w1
	bcc	.L1443
	mov	x0, x2
	bl	FtlGcBufFree
	ldrh	w0, [x19, 132]
	str	wzr, [x19, 3240]
	cbnz	w0, .L1445
	mov	w0, 1
	bl	FtlGcFreeTempBlock
	mov	w0, 1
.L1439:
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L1443:
	ubfiz	x4, x11, 5, 16
	add	x1, x2, x4
	ldr	w2, [x2, x4]
	ldr	x3, [x1, 16]
	ldr	w1, [x1, 4]
	ldr	w0, [x3, 12]
	cmn	w2, #1
	bne	.L1441
.L1447:
	add	w11, w11, 1
	bl	FtlGcUpdatePage
	and	w11, w11, 65535
	b	.L1440
.L1441:
	ldr	w2, [x3, 8]
	b	.L1447
.L1445:
	mov	w0, 0
	b	.L1439
	.size	Ftl_gc_temp_data_write_back, .-Ftl_gc_temp_data_write_back
	.align	2
	.global	Ftl_get_new_temp_ppa
	.type	Ftl_get_new_temp_ppa, %function
Ftl_get_new_temp_ppa:
	stp	x29, x30, [sp, -32]!
	mov	w1, 65535
	mov	x29, sp
	str	x19, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	ldrh	w2, [x19, 128]
	cmp	w2, w1
	beq	.L1449
	ldrh	w0, [x19, 132]
	cbnz	w0, .L1450
.L1449:
	mov	w0, 0
	bl	FtlGcFreeTempBlock
	add	x0, x19, 128
	strb	wzr, [x0, 8]
	bl	allocate_data_superblock
	strh	wzr, [x19, 3284]
	strh	wzr, [x19, 3296]
	bl	l2p_flush
	mov	w0, 0
	bl	FtlEctTblFlush
	bl	FtlVpcTblFlush
.L1450:
	add	x0, x19, 128
	bl	get_new_active_ppa
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	Ftl_get_new_temp_ppa, .-Ftl_get_new_temp_ppa
	.section	.rodata.str1.1
.LC124:
	.string	"SWL %x, FSB = %x vpc= %x,ec=%x th=%x\n"
	.text
	.align	2
	.global	rk_ftl_garbage_collect
	.type	rk_ftl_garbage_collect, %function
rk_ftl_garbage_collect:
	stp	x29, x30, [sp, -144]!
	adrp	x6, .LANCHOR0
	mov	x29, sp
	stp	x23, x24, [sp, 48]
	add	x23, x6, :lo12:.LANCHOR0
	stp	x19, x20, [sp, 16]
	stp	x21, x22, [sp, 32]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	str	w0, [sp, 108]
	mrs	x0, sp_el0
	ldr	x1, [x0, 952]
	str	x1, [sp, 136]
	mov	x1, 0
	ldr	w0, [x23, 3408]
	cbnz	w0, .L1529
	ldrh	w0, [x23, 2584]
	cmp	w0, 47
	bls	.L1529
	adrp	x0, .LANCHOR2+4
	ldrh	w1, [x0, #:lo12:.LANCHOR2+4]
	mov	w0, 65535
	cmp	w1, w0
	bne	.L1454
.L1457:
	ldrh	w0, [x23, 224]
	mov	w1, 65535
	cmp	w0, w1
	bne	.L1455
.L1456:
	bl	FtlReadRefresh
	ldr	w1, [sp, 108]
	ldr	w0, [x23, 3228]
	mov	w19, 65535
	ldrh	w2, [x23, 128]
	add	w0, w0, 1
	add	w0, w0, w1, lsl 7
	ldrh	w1, [x23, 176]
	str	w0, [x23, 3228]
	and	w1, w1, w2
	ldrh	w2, [x23, 226]
	and	w1, w1, w2
	and	w20, w1, 65535
	cmp	w19, w1, uxth
	bne	.L1531
	ldrh	w1, [x23, 228]
	mov	w2, 1024
	cmp	w1, 24
	mov	w1, 5120
	csel	w1, w1, w2, cc
	cmp	w0, w1
	bls	.L1458
	str	wzr, [x23, 3228]
	strh	wzr, [x23, 3952]
	bl	GetSwlReplaceBlock
	and	w20, w0, 65535
	cmp	w19, w0, uxth
	bne	.L1460
	ldrh	w1, [x23, 228]
	ldrh	w0, [x23, 3226]
	cmp	w1, w0
	bcs	.L1461
	mov	w0, 64
	bl	List_get_gc_head_node
	and	w1, w0, 65535
	cmp	w20, w0, uxth
	beq	.L1463
	ldr	x0, [x23, 72]
	ubfiz	x1, x1, 1, 16
	ldrh	w0, [x0, x1]
	cmp	w0, 7
	bhi	.L1464
	mov	w0, 0
	bl	List_get_gc_head_node
	mov	w2, 128
	strh	w2, [x23, 3226]
	and	w1, w0, 65535
	cmp	w20, w0, uxth
	beq	.L1463
	mov	w20, w1
.L1460:
	ldr	x1, [x23, 72]
	ubfiz	x0, x20, 1, 16
	ldr	x2, [x23, 2600]
	ldrh	w3, [x1, x0]
	mov	w1, w20
	ldrh	w5, [x23, 3224]
	ldrh	w4, [x2, x0]
	adrp	x0, .LC124
	ldrh	w2, [x23, 228]
	add	x0, x0, :lo12:.LC124
	bl	sftl_printk
	b	.L1463
.L1454:
	ldrh	w1, [x23, 128]
	cmp	w1, w0
	beq	.L1457
	mov	w0, 1
	bl	FtlGcFreeTempBlock
	cbz	w0, .L1457
	mov	w0, 1
	b	.L1452
.L1455:
	ldrh	w2, [x23, 226]
	cmp	w2, w1
	bne	.L1456
	strh	w0, [x23, 226]
	mov	w0, -1
	strh	w0, [x23, 224]
	b	.L1456
.L1464:
	mov	w0, 64
.L1559:
	strh	w0, [x23, 3226]
.L1463:
	bl	FtlGcReFreshBadBlk
.L1458:
	ldr	w1, [sp, 108]
	mov	w0, 65535
	cmp	w1, 0
	ccmp	w20, w0, 0, eq
	bne	.L1466
	ldrh	w0, [x23, 228]
	cmp	w0, 24
	bhi	.L1534
	ldrh	w19, [x23, 306]
	cmp	w0, 16
	bls	.L1468
	lsr	w19, w19, 5
.L1467:
	ldrh	w1, [x23, 3224]
	cmp	w1, w0
	bcs	.L1470
	ldrh	w1, [x23, 226]
	ldrh	w0, [x23, 128]
	and	w0, w0, w1
	mov	w1, 65535
	cmp	w1, w0, uxth
	bne	.L1471
	ldrh	w0, [x23, 3952]
	cbnz	w0, .L1472
	ldr	w1, [x23, 2624]
	ldr	w2, [x23, 2620]
	add	w1, w1, w1, lsl 1
	cmp	w2, w1, lsr 2
	bcs	.L1535
.L1472:
	ldrh	w1, [x23, 2692]
	add	w1, w1, w1, lsl 1
	lsr	w1, w1, 2
.L1473:
	strh	w1, [x23, 3224]
	str	wzr, [x23, 3236]
.L1452:
	mrs	x1, sp_el0
	ldr	x2, [sp, 136]
	ldr	x3, [x1, 952]
	subs	x2, x2, x3
	mov	x3, 0
	beq	.L1527
	bl	__stack_chk_fail
.L1461:
	mov	w0, 80
	b	.L1559
.L1531:
	mov	w20, w19
	b	.L1458
.L1468:
	cmp	w0, 12
	bls	.L1469
	lsr	w19, w19, 4
	b	.L1467
.L1469:
	cmp	w0, 8
	bls	.L1467
	lsr	w19, w19, 2
	b	.L1467
.L1534:
	mov	w19, 1
	b	.L1467
.L1535:
	mov	w1, 18
	b	.L1473
.L1471:
	ldrh	w0, [x23, 2692]
	add	w0, w0, w0, lsl 1
	asr	w0, w0, 2
	strh	w0, [x23, 3224]
.L1470:
	ldrh	w0, [x23, 3312]
	cbz	w0, .L1536
	add	w19, w19, 32
	and	w19, w19, 65535
.L1536:
	mov	w20, 65535
.L1475:
	ldrh	w0, [x23, 176]
	mov	w1, 65535
	cmp	w0, w1
	bne	.L1483
	cmp	w20, w0
	beq	.L1484
	strh	w20, [x23, 176]
.L1485:
	ldrh	w0, [x23, 176]
	add	x21, x23, 176
	strb	wzr, [x21, 8]
	mov	w1, 65535
	cmp	w0, w1
	beq	.L1483
	bl	IsBlkInGcList
	cbz	w0, .L1488
	mov	w0, -1
	strh	w0, [x23, 176]
.L1483:
	ldrh	w0, [x23, 176]
	ldrh	w1, [x23, 24]
	cmp	w1, w0
	beq	.L1489
	ldrh	w1, [x23, 80]
	cmp	w1, w0
	beq	.L1489
	ldrh	w1, [x23, 128]
	cmp	w1, w0
	bne	.L1490
.L1489:
	mov	w0, -1
	strh	w0, [x23, 176]
.L1490:
	adrp	x21, .LANCHOR1
	add	x21, x21, :lo12:.LANCHOR1
	add	x21, x21, 568
.L1525:
	ldrh	w22, [x23, 176]
	mov	w0, 65535
	cmp	w22, w0
	bne	.L1491
	mov	w24, 2
	str	wzr, [x23, 3236]
.L1492:
	ldrh	w5, [x23, 3232]
	mov	w0, w5
	bl	List_get_gc_head_node
	and	w7, w0, 65535
	strh	w7, [x23, 176]
	cmp	w7, w22
	bne	.L1493
	mov	w0, 8
	strh	wzr, [x23, 3232]
	b	.L1452
.L1466:
	ldrh	w19, [x23, 128]
	ldrh	w1, [x23, 226]
	and	w19, w19, w1
	and	w19, w20, w19
	cmp	w19, w0
	bne	.L1537
	ldrh	w0, [x23, 176]
	cmp	w0, w19
	beq	.L1476
.L1480:
	mov	w20, w19
.L1537:
	mov	w19, 1
	b	.L1475
.L1476:
	ldrh	w1, [x23, 228]
	ldrh	w0, [x23, 3224]
	str	wzr, [x23, 3236]
	cmp	w1, w0
	bls	.L1477
	ldrh	w0, [x23, 3952]
	cbnz	w0, .L1478
	ldr	w0, [x23, 2624]
	ldr	w1, [x23, 2620]
	add	w0, w0, w0, lsl 1
	cmp	w1, w0, lsr 2
	bcs	.L1538
.L1478:
	ldrh	w0, [x23, 2692]
	add	w0, w0, w0, lsl 1
	lsr	w0, w0, 2
.L1479:
	strh	w0, [x23, 3224]
	bl	FtlReadRefresh
	mov	w0, 0
	bl	List_get_gc_head_node
	ldr	x1, [x23, 72]
	ubfiz	x0, x0, 1, 16
	ldrh	w0, [x1, x0]
	cmp	w0, 4
	bls	.L1477
.L1560:
	ldrh	w0, [x23, 3952]
	b	.L1452
.L1538:
	mov	w0, 18
	b	.L1479
.L1477:
	ldrh	w0, [x23, 3952]
	cbnz	w0, .L1480
	ldrh	w5, [x23, 2692]
	add	w0, w5, w5, lsl 1
	asr	w0, w0, 2
	strh	w0, [x23, 3224]
	mov	w0, 0
	bl	List_get_gc_head_node
	ldr	x1, [x23, 72]
	ubfiz	x0, x0, 1, 16
	ldrh	w2, [x23, 236]
	ldrh	w1, [x1, x0]
	ldrh	w0, [x23, 308]
	mul	w0, w0, w2
	mov	w2, 2
	sdiv	w0, w0, w2
	cmp	w1, w0
	ble	.L1481
	ldrh	w0, [x23, 228]
	sub	w5, w5, #1
	cmp	w0, w5
	blt	.L1481
	bl	FtlReadRefresh
	b	.L1560
.L1481:
	cbnz	w1, .L1480
	mov	w0, -1
	bl	decrement_vpc_count
	ldrh	w0, [x23, 228]
	add	w0, w0, 1
	b	.L1452
.L1484:
	ldrh	w0, [x23, 226]
	cmp	w0, w20
	beq	.L1485
	ldr	x1, [x23, 72]
	ubfiz	x0, x0, 1, 16
	ldrh	w0, [x1, x0]
	cbnz	w0, .L1486
	mov	w0, -1
	strh	w0, [x23, 226]
.L1486:
	ldrh	w0, [x23, 226]
	strh	w0, [x23, 176]
	mov	w0, -1
	strh	w0, [x23, 226]
	b	.L1485
.L1493:
	mov	w0, w7
	bl	IsBlkInGcList
	add	w1, w5, 1
	and	w1, w1, 65535
	strh	w1, [x23, 3232]
	cbnz	w0, .L1492
	ldrh	w5, [x23, 236]
	ubfiz	x2, x7, 1, 16
	ldrh	w0, [x23, 306]
	ldr	x3, [x23, 72]
	mul	w0, w0, w5
	ldrh	w4, [x3, x2]
	sdiv	w5, w0, w24
	cmp	w4, w5
	bgt	.L1496
	cmp	w1, 48
	bls	.L1497
	cmp	w4, 8
	bls	.L1497
	ldrh	w1, [x23, 3284]
	cmp	w1, 35
	bhi	.L1497
.L1496:
	strh	wzr, [x23, 3232]
.L1497:
	ldrh	w1, [x3, x2]
	cmp	w0, w1
	bgt	.L1498
	cmp	w20, w22
	bne	.L1498
	ldrh	w0, [x23, 3232]
	cmp	w0, 3
	bhi	.L1498
	mov	w0, -1
	strh	w0, [x23, 176]
	strh	wzr, [x23, 3232]
	b	.L1560
.L1498:
	cbnz	w1, .L1499
	mov	w0, -1
	bl	decrement_vpc_count
	ldrh	w0, [x23, 3232]
	add	w0, w0, 1
	strh	w0, [x23, 3232]
	b	.L1492
.L1499:
	ldrh	w0, [x23, 24]
	strb	wzr, [x23, 184]
	cmp	w0, w7
	bne	.L1500
	adrp	x0, .LC8
	mov	x1, x21
	add	x0, x0, :lo12:.LC8
	mov	w2, 717
	bl	sftl_printk
.L1500:
	ldrh	w1, [x23, 176]
	ldrh	w0, [x23, 80]
	cmp	w1, w0
	bne	.L1501
	adrp	x0, .LC8
	mov	x1, x21
	add	x0, x0, :lo12:.LC8
	mov	w2, 718
	bl	sftl_printk
.L1501:
	ldrh	w1, [x23, 176]
	ldrh	w0, [x23, 128]
	cmp	w1, w0
	bne	.L1502
	adrp	x0, .LC8
	mov	x1, x21
	add	x0, x0, :lo12:.LC8
	mov	w2, 719
	bl	sftl_printk
.L1502:
	add	x22, x23, 176
	mov	x0, x22
	bl	make_superblock
	ldrh	w1, [x23, 176]
	ldr	x0, [x23, 72]
	strh	wzr, [x23, 3954]
	ldrh	w0, [x0, x1, lsl 1]
	strh	wzr, [x23, 178]
	strb	wzr, [x23, 182]
	strh	w0, [x23, 3956]
.L1491:
	mov	w0, 1
	str	w0, [x23, 3408]
	ldrh	w0, [x23, 306]
	str	w0, [sp, 104]
	ldr	w0, [sp, 108]
	cbz	w0, .L1503
	ldr	w1, [sp, 104]
	ldrh	w0, [x23, 236]
	ldrh	w2, [x23, 176]
	mul	w0, w0, w1
	ldr	x1, [x23, 72]
	ldrh	w1, [x1, x2, lsl 1]
	sub	w0, w0, w1
	mov	w1, 4
	sdiv	w0, w0, w1
	add	w19, w0, w19
	and	w19, w19, 65535
.L1503:
	ldrh	w0, [x23, 178]
	ldr	w2, [sp, 104]
	add	w1, w0, w19
	cmp	w1, w2
	ble	.L1504
	sub	w0, w2, w0
	and	w19, w0, 65535
.L1504:
	add	x26, x23, 176
	mov	w27, 0
.L1505:
	ldrh	w0, [x26, 2]
	cmp	w19, w27, uxth
	bls	.L1513
	ldrh	w7, [x23, 236]
	add	w1, w0, w27
	mov	x4, x26
	mov	w22, 0
	mov	w2, 0
	mov	w5, 65535
	b	.L1514
.L1507:
	ldrh	w3, [x4, 16]
	cmp	w3, w5
	beq	.L1506
	ubfiz	x8, x22, 5, 16
	add	w22, w22, 1
	add	x0, x0, x8
	and	w22, w22, 65535
	orr	w3, w1, w3, lsl 10
	str	w3, [x0, 4]
.L1506:
	add	w2, w2, 1
	add	x4, x4, 2
	and	w2, w2, 65535
.L1514:
	ldr	x0, [x23, 3272]
	cmp	w2, w7
	bne	.L1507
	ldrb	w2, [x26, 8]
	mov	w1, w22
	mov	x24, 0
	bl	FlashReadPages
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	str	x0, [sp, 112]
.L1508:
	cmp	w22, w24, uxth
	bhi	.L1512
	add	w27, w27, 1
	b	.L1505
.L1512:
	ldr	x0, [x23, 3272]
	lsl	x28, x24, 5
	add	x1, x0, x24, lsl 5
	ldr	w0, [x0, x28]
	cmn	w0, #1
	beq	.L1509
	ldr	x25, [x1, 16]
	mov	w0, 61589
	ldrh	w1, [x25]
	cmp	w1, w0
	bne	.L1509
	ldr	w4, [x25, 8]
	cmn	w4, #1
	bne	.L1510
	ldr	x0, [sp, 112]
	mov	x1, x21
	mov	w2, 753
	str	w4, [sp, 120]
	bl	sftl_printk
	ldr	w4, [sp, 120]
.L1510:
	add	x1, sp, 128
	mov	w2, 0
	mov	w0, w4
	bl	log2phys
	ldr	x0, [x23, 3272]
	ldr	w1, [sp, 128]
	add	x0, x0, x28
	ldr	w2, [x0, 4]
	cmp	w2, w1
	bne	.L1509
	ldrh	w1, [x23, 3954]
	ldr	x2, [x23, 3432]
	add	w1, w1, 1
	strh	w1, [x23, 3954]
	ldr	w1, [x23, 3240]
	ldr	w0, [x0, 24]
	add	x1, x2, x1, lsl 5
	str	x1, [sp, 120]
	str	w0, [x1, 24]
	bl	Ftl_get_new_temp_ppa
	ldr	x1, [sp, 120]
	str	w0, [x1, 4]
	ldr	w0, [x23, 3240]
	ldr	x1, [x23, 3272]
	ldr	x2, [x23, 3432]
	add	x1, x1, x28
	add	x0, x2, x0, lsl 5
	ldr	x2, [x1, 8]
	str	x2, [x0, 8]
	ldr	x1, [x1, 16]
	str	x1, [x0, 16]
	ldr	w0, [sp, 128]
	mov	w1, 1
	str	w0, [x25, 12]
	ldrh	w0, [x23, 128]
	strh	w0, [x25, 2]
	ldr	w0, [x23, 2664]
	str	w0, [x25, 4]
	ldr	w0, [x23, 3240]
	add	w0, w0, 1
	str	w0, [x23, 3240]
	ldr	x0, [x23, 3272]
	add	x0, x0, x28
	bl	FtlGcBufAlloc
	ldrb	w1, [x23, 135]
	ldr	w0, [x23, 3240]
	cmp	w1, w0
	beq	.L1511
	ldrh	w0, [x23, 132]
	cbnz	w0, .L1509
.L1511:
	bl	Ftl_gc_temp_data_write_back
	cbz	w0, .L1509
	mov	w0, 65535
	str	w0, [x23, 176]
	str	wzr, [x23, 3408]
	b	.L1560
.L1509:
	add	x24, x24, 1
	b	.L1508
.L1513:
	add	w19, w0, w19
	ldr	w0, [sp, 104]
	and	w19, w19, 65535
	strh	w19, [x26, 2]
	cmp	w0, w19
	bhi	.L1515
	ldr	w0, [x23, 3240]
	cbz	w0, .L1516
	bl	Ftl_gc_temp_data_write_back
	cbz	w0, .L1516
	str	wzr, [x23, 3408]
	b	.L1560
.L1516:
	ldrh	w0, [x23, 3954]
	cbnz	w0, .L1517
	ldrh	w1, [x23, 176]
	ldr	x0, [x23, 72]
	ldrh	w0, [x0, x1, lsl 1]
	cbz	w0, .L1517
	mov	w19, 0
	b	.L1518
.L1521:
	add	x1, sp, 132
	mov	w0, w19
	mov	w2, 0
	bl	log2phys
	ldr	w0, [sp, 132]
	cmn	w0, #1
	beq	.L1519
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	ldrh	w1, [x23, 176]
	cmp	w1, w0, uxth
	beq	.L1520
.L1519:
	add	w19, w19, 1
.L1518:
	ldr	w0, [x23, 2624]
	cmp	w19, w0
	bcc	.L1521
.L1523:
	ldrh	w1, [x23, 176]
	ldr	x0, [x23, 72]
	strh	wzr, [x0, x1, lsl 1]
	ldrh	w0, [x23, 176]
	bl	update_vpc_list
	bl	l2p_flush
	bl	FtlVpcTblFlush
.L1517:
	mov	w0, -1
	strh	w0, [x23, 176]
.L1515:
	ldrh	w0, [x23, 228]
	str	wzr, [x23, 3408]
	cmp	w0, 2
	bhi	.L1524
	ldrh	w19, [x23, 306]
	b	.L1525
.L1520:
	ldr	w0, [x23, 2624]
	cmp	w19, w0
	bcc	.L1517
	b	.L1523
.L1524:
	ldrh	w1, [x23, 3952]
	cmp	w1, 0
	csinc	w0, w1, w0, ne
	b	.L1452
.L1529:
	mov	w0, 0
	b	.L1452
.L1488:
	mov	x0, x21
	bl	make_superblock
	ldrh	w1, [x23, 176]
	ldr	x0, [x23, 72]
	strh	wzr, [x21, 2]
	strb	wzr, [x21, 6]
	strh	wzr, [x23, 3954]
	ldrh	w0, [x0, x1, lsl 1]
	strh	w0, [x23, 3956]
	b	.L1483
.L1527:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 144
	ret
	.size	rk_ftl_garbage_collect, .-rk_ftl_garbage_collect
	.align	2
	.global	FtlRead
	.type	FtlRead, %function
FtlRead:
	stp	x29, x30, [sp, -160]!
	and	w0, w0, 255
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	stp	x21, x22, [sp, 32]
	mov	x21, x3
	stp	x23, x24, [sp, 48]
	mov	w23, w2
	stp	x25, x26, [sp, 64]
	mov	w26, w1
	mrs	x1, sp_el0
	stp	x27, x28, [sp, 80]
	ldr	x2, [x1, 952]
	str	x2, [sp, 152]
	mov	x2, 0
	cmp	w0, 16
	bne	.L1562
	mov	x2, x3
	mov	w1, w23
	add	w0, w26, 256
	bl	FtlVendorPartRead
	mov	w19, w0
.L1561:
	mrs	x0, sp_el0
	ldr	x1, [sp, 152]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1584
	bl	__stack_chk_fail
.L1562:
	adrp	x3, .LANCHOR0
	add	x28, x3, :lo12:.LANCHOR0
	ldr	w0, [x28, 344]
	cmp	w0, w26
	bls	.L1587
	cmp	w0, w23
	bcc	.L1587
	add	w1, w26, w23
	str	w1, [sp, 116]
	cmp	w0, w1
	bcc	.L1587
	adrp	x0, .LANCHOR2
	ldr	w19, [x0, #:lo12:.LANCHOR2]
	cmn	w19, #1
	beq	.L1561
	ldrh	w0, [x28, 262]
	mov	w20, 0
	mov	w19, 0
	stp	wzr, wzr, [sp, 108]
	udiv	w1, w26, w0
	stp	w1, wzr, [sp, 100]
	add	w1, w26, w23
	sub	w25, w1, #1
	ldr	w22, [sp, 100]
	udiv	w25, w25, w0
	ldr	w0, [sp, 100]
	sub	w24, w25, w0
	ldr	w0, [x28, 2656]
	add	w24, w24, 1
	add	w0, w0, w23
	str	w0, [x28, 2656]
	ldr	w0, [x28, 2628]
	add	w0, w0, w24
	str	w0, [x28, 2628]
.L1564:
	cbnz	w24, .L1582
	ldrh	w0, [x28, 3312]
	cbnz	w0, .L1583
	ldrh	w0, [x28, 228]
	cmp	w0, 31
	bhi	.L1561
.L1583:
	mov	w1, 1
	mov	w0, 0
	bl	rk_ftl_garbage_collect
	b	.L1561
.L1582:
	add	x1, sp, 148
	mov	w0, w22
	mov	w2, 0
	bl	log2phys
	ldr	w1, [sp, 148]
	cmn	w1, #1
	bne	.L1597
	mov	w27, 0
	b	.L1565
.L1568:
	madd	w0, w0, w22, w27
	cmp	w26, w0
	bhi	.L1567
	ldr	w1, [sp, 116]
	cmp	w1, w0
	bls	.L1567
	sub	w0, w0, w26
	mov	w2, 512
	mov	w1, 0
	lsl	w0, w0, 9
	add	x0, x21, x0
	bl	ftl_memset
.L1567:
	add	w27, w27, 1
.L1565:
	ldrh	w0, [x28, 262]
	cmp	w0, w27
	bhi	.L1568
.L1569:
	add	w22, w22, 1
	subs	w24, w24, #1
	beq	.L1574
	ldrh	w0, [x28, 236]
	cmp	w20, w0, lsl 2
	bne	.L1564
.L1574:
	cbz	w20, .L1564
	ldr	x0, [x28, 3424]
	mov	w1, w20
	mov	w2, 0
	mov	x27, 0
	bl	FlashReadPages
	ldr	w0, [sp, 112]
	lsl	w0, w0, 9
	str	w0, [sp, 132]
	ldr	w0, [sp, 104]
	lsl	w0, w0, 9
	str	x0, [sp, 120]
	ldr	w0, [sp, 108]
	lsl	w0, w0, 9
	str	w0, [sp, 128]
.L1581:
	ldr	x0, [x28, 3424]
	lsl	x6, x27, 5
	ldr	w2, [sp, 100]
	add	x0, x0, x27, lsl 5
	ldr	w1, [x0, 24]
	cmp	w1, w2
	bne	.L1576
	ldr	x1, [x0, 8]
	ldr	x0, [x28, 3472]
	cmp	x1, x0
	bne	.L1577
	ldr	x0, [sp, 120]
	str	x6, [sp, 136]
	ldr	w2, [sp, 128]
	add	x1, x1, x0
	mov	x0, x21
.L1600:
	bl	ftl_memcpy
	ldr	x6, [sp, 136]
.L1577:
	ldr	x1, [x28, 3424]
	add	x0, x1, x6
	ldr	x2, [x0, 16]
	ldr	w7, [x0, 24]
	ldr	w2, [x2, 8]
	cmp	w7, w2
	beq	.L1578
	ldr	w2, [x28, 2784]
	add	w2, w2, 1
	str	w2, [x28, 2784]
.L1578:
	ldr	w7, [x1, x6]
	cmn	w7, #1
	bne	.L1579
	ldr	w2, [x28, 2784]
	mov	w19, w7
	add	w2, w2, 1
	str	w2, [x28, 2784]
.L1579:
	ldr	w1, [x1, x6]
	cmp	w1, 256
	bne	.L1580
	ldr	w0, [x0, 4]
	ubfx	x0, x0, 10, 16
	bl	P2V_block_in_plane
	bl	FtlGcRefreshBlock
.L1580:
	add	x27, x27, 1
	cmp	w20, w27
	bhi	.L1581
	mov	w20, 0
	b	.L1564
.L1597:
	ldr	x0, [x28, 3424]
	ubfiz	x5, x20, 5, 32
	add	x0, x0, x5
	str	w1, [x0, 4]
	ldr	w0, [sp, 100]
	ldrh	w1, [x28, 262]
	cmp	w22, w0
	bne	.L1570
	udiv	w6, w26, w1
	ldr	x0, [x28, 3424]
	ldr	x2, [x28, 3472]
	msub	w3, w6, w1, w26
	add	x0, x0, x5
	str	w3, [sp, 104]
	sub	w6, w1, w3
	cmp	w23, w6
	csel	w3, w23, w6, ls
	str	w3, [sp, 108]
	cmp	w1, w3
	csel	x1, x2, x21, ne
	str	x1, [x0, 8]
.L1571:
	ldrh	w0, [x28, 316]
	ldr	x1, [x28, 3424]
	ldr	x2, [x28, 3520]
	mul	w0, w0, w20
	add	x1, x1, x5
	add	w20, w20, 1
	and	x0, x0, 4294967292
	add	x0, x2, x0
	str	x0, [x1, 16]
	str	w22, [x1, 24]
	b	.L1569
.L1570:
	ldr	x2, [x28, 3424]
	mul	w0, w22, w1
	add	x2, x2, x5
	cmp	w22, w25
	bne	.L1572
	ldr	w3, [sp, 116]
	sub	w3, w3, w0
	str	w3, [sp, 112]
	cmp	w3, w1
	beq	.L1572
	ldr	x0, [x28, 3480]
.L1599:
	str	x0, [x2, 8]
	b	.L1571
.L1572:
	sub	w0, w0, w26
	lsl	w0, w0, 9
	add	x0, x21, x0
	b	.L1599
.L1576:
	cmp	w1, w25
	bne	.L1577
	ldr	x1, [x0, 8]
	ldr	x0, [x28, 3480]
	cmp	x1, x0
	bne	.L1577
	ldrh	w0, [x28, 262]
	ldr	w2, [sp, 132]
	str	x6, [sp, 136]
	mul	w0, w0, w25
	sub	w0, w0, w26
	lsl	w0, w0, 9
	add	x0, x21, x0
	b	.L1600
.L1587:
	mov	w19, -1
	b	.L1561
.L1584:
	mov	w0, w19
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 160
	ret
	.size	FtlRead, .-FtlRead
	.align	2
	.global	sftl_read
	.type	sftl_read, %function
sftl_read:
	stp	x29, x30, [sp, -16]!
	mov	x3, x2
	mov	w2, w1
	mov	x29, sp
	mov	w1, w0
	mov	w0, 0
	bl	FtlRead
	ldp	x29, x30, [sp], 16
	ret
	.size	sftl_read, .-sftl_read
	.section	.rodata.str1.1
.LC125:
	.string	"FtlWrite: ecc error:%x %x %x\n"
.LC126:
	.string	"FtlWrite: lpa error:%x %x\n"
	.text
	.align	2
	.global	FtlWrite
	.type	FtlWrite, %function
FtlWrite:
	stp	x29, x30, [sp, -224]!
	and	w0, w0, 255
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	stp	x21, x22, [sp, 32]
	mov	w21, w1
	mrs	x1, sp_el0
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	mov	x25, x3
	stp	x27, x28, [sp, 80]
	str	w2, [sp, 120]
	ldr	x2, [x1, 952]
	str	x2, [sp, 216]
	mov	x2, 0
	cmp	w0, 16
	bne	.L1604
	ldr	w1, [sp, 120]
	mov	x2, x3
	add	w0, w21, 256
	bl	FtlVendorPartWrite
.L1603:
	mrs	x1, sp_el0
	ldr	x2, [sp, 216]
	ldr	x3, [x1, 952]
	subs	x2, x2, x3
	mov	x3, 0
	beq	.L1641
	bl	__stack_chk_fail
.L1604:
	adrp	x4, .LANCHOR0
	add	x24, x4, :lo12:.LANCHOR0
	ldr	w0, [x24, 344]
	cmp	w0, w21
	bls	.L1644
	ldr	w1, [sp, 120]
	cmp	w0, w1
	bcc	.L1644
	add	w1, w21, w1
	str	w1, [sp, 124]
	cmp	w0, w1
	bcc	.L1644
	adrp	x0, .LANCHOR2
	ldr	w0, [x0, #:lo12:.LANCHOR2]
	cmn	w0, #1
	beq	.L1603
	mov	w0, 2048
	str	w0, [x24, 3960]
	ldrh	w0, [x24, 262]
	sub	w1, w1, #1
	add	x20, x24, 24
	udiv	w27, w21, w0
	udiv	w0, w1, w0
	ldr	w1, [sp, 120]
	str	w0, [sp, 160]
	sub	w0, w0, w27
	add	w26, w0, 1
	str	w0, [sp, 136]
	ldr	w0, [x24, 2652]
	add	w0, w0, w1
	str	w0, [x24, 2652]
	ldr	w0, [x24, 2636]
	add	w0, w0, w26
	str	w0, [x24, 2636]
	adrp	x0, .LANCHOR2
	add	x0, x0, :lo12:.LANCHOR2
	ldr	w0, [x0, 8]
	cbz	w0, .L1606
	ldrh	w1, [x20, 4]
	add	x0, x24, 80
	cmp	w1, 0
	csel	x20, x0, x20, eq
.L1606:
	adrp	x23, .LANCHOR1
	add	x23, x23, :lo12:.LANCHOR1
	adrp	x28, .LC8
	mov	w19, w27
	add	x23, x23, 591
	add	x28, x28, :lo12:.LC8
.L1607:
	cbnz	w26, .L1637
	ldr	w1, [sp, 136]
	mov	w0, 0
	bl	rk_ftl_garbage_collect
	ldrh	w0, [x24, 228]
	cmp	w0, 5
	bls	.L1649
.L1640:
	mov	w0, 0
	b	.L1603
.L1637:
	ldrb	w1, [x20, 6]
	ldrh	w0, [x24, 236]
	cmp	w1, w0
	bcc	.L1608
	mov	x1, x23
	mov	x0, x28
	mov	w2, 1046
	bl	sftl_printk
.L1608:
	ldrh	w0, [x20, 4]
	cbnz	w0, .L1609
	add	x0, x24, 24
	cmp	x20, x0
	bne	.L1610
	ldrh	w1, [x24, 84]
	add	x0, x24, 80
	cbnz	w1, .L1611
	bl	allocate_new_data_superblock
	adrp	x0, .LANCHOR2
	add	x0, x0, :lo12:.LANCHOR2
	str	wzr, [x0, 8]
.L1611:
	add	x0, x24, 24
	bl	allocate_new_data_superblock
	adrp	x0, .LANCHOR2
	add	x0, x0, :lo12:.LANCHOR2
	ldr	w0, [x0, 8]
	cbnz	w0, .L1648
.L1613:
	add	x20, x24, 24
.L1612:
	ldrh	w0, [x20, 4]
	cbnz	w0, .L1609
	mov	x0, x20
	bl	allocate_new_data_superblock
.L1609:
	ldrh	w0, [x20, 4]
	ldrb	w1, [x20, 7]
	cmp	w0, w26
	csel	w0, w0, w26, ls
	lsl	w1, w1, 2
	cmp	w1, w0
	csel	w0, w1, w0, ls
	ldrb	w1, [x20, 6]
	str	w0, [sp, 140]
	ldrh	w0, [x24, 236]
	cmp	w1, w0
	bcc	.L1614
	mov	x1, x23
	mov	x0, x28
	mov	w2, 1079
	bl	sftl_printk
.L1614:
	ldr	w0, [sp, 140]
	str	xzr, [sp, 104]
	add	w0, w0, w19
	str	w0, [sp, 164]
	adrp	x0, .LC126
	add	x0, x0, :lo12:.LC126
	str	x0, [sp, 168]
.L1615:
	ldr	w0, [sp, 164]
	ldr	w22, [sp, 104]
	cmp	w19, w0
	bne	.L1635
	ldr	w22, [sp, 140]
.L1616:
	ldr	x0, [x24, 3440]
	mov	x3, x20
	mov	w1, w22
	mov	w2, 0
	bl	FtlProgPages
	cmp	w26, w22
	bcs	.L1636
	mov	x1, x23
	mov	x0, x28
	mov	w2, 1157
	bl	sftl_printk
.L1636:
	sub	w26, w26, w22
	b	.L1607
.L1610:
	adrp	x1, .LANCHOR2
	add	x1, x1, :lo12:.LANCHOR2
	ldrh	w0, [x0, 4]
	str	wzr, [x1, 8]
	cbnz	w0, .L1613
	mov	x0, x20
	bl	allocate_new_data_superblock
	b	.L1612
.L1648:
	add	x20, x24, 80
	b	.L1612
.L1635:
	ldrh	w0, [x20, 4]
	cbz	w0, .L1616
	add	x1, sp, 180
	mov	w2, 0
	mov	w0, w19
	bl	log2phys
	mov	x0, x20
	bl	get_new_active_ppa
	ldr	x1, [sp, 104]
	ldr	x2, [sp, 104]
	lsl	x1, x1, 5
	str	x1, [sp, 112]
	ldr	x1, [x24, 3440]
	add	x1, x1, x2, lsl 5
	ldrh	w2, [x24, 316]
	str	w0, [x1, 4]
	ldr	x1, [sp, 104]
	mul	w22, w2, w22
	ldr	x0, [x24, 3440]
	add	x0, x0, x1, lsl 5
	and	x1, x22, 4294967292
	str	x1, [sp, 144]
	ldr	x1, [x24, 3520]
	str	w19, [x0, 24]
	ldr	x3, [sp, 144]
	str	x1, [sp, 152]
	add	x22, x1, x3
	str	x22, [x0, 16]
	mov	w1, 0
	mov	x0, x22
	bl	ftl_memset
	ldr	w0, [sp, 160]
	cmp	w19, w27
	ldrh	w1, [x24, 262]
	ccmp	w19, w0, 4, ne
	bne	.L1617
	cmp	w19, w27
	bne	.L1618
	udiv	w0, w21, w1
	msub	w0, w0, w1, w21
	str	w0, [sp, 128]
	sub	w2, w1, w0
	ldr	w0, [sp, 120]
	cmp	w2, w0
	csel	w0, w2, w0, ls
	str	w0, [sp, 132]
.L1619:
	ldr	x0, [sp, 112]
	ldr	x3, [x24, 3440]
	add	x3, x3, x0
	ldr	w0, [sp, 132]
	cmp	w1, w0
	bne	.L1620
	mul	w0, w1, w19
	cmp	w19, w27
	sub	w0, w0, w21
	lsl	w0, w0, 9
	add	x0, x25, x0
	csel	x0, x25, x0, eq
	str	x0, [x3, 8]
.L1622:
	ldrb	w1, [x20, 6]
	ldrh	w0, [x24, 236]
	cmp	w1, w0
	bcc	.L1632
	mov	x1, x23
	mov	x0, x28
	mov	w2, 1148
	bl	sftl_printk
.L1632:
	ldp	x2, x1, [sp, 144]
	mov	w0, -3947
	strh	w0, [x1, x2]
	ldr	w0, [x24, 2664]
	stp	w0, w19, [x22, 4]
	add	w19, w19, 1
	cmn	w0, #2
	csinc	w0, wzr, w0, eq
	str	w0, [x24, 2664]
	ldr	w0, [sp, 180]
	str	w0, [x22, 12]
	ldrh	w0, [x20]
	strh	w0, [x22, 2]
	ldr	x0, [sp, 104]
	add	x0, x0, 1
	str	x0, [sp, 104]
	b	.L1615
.L1618:
	ldr	w0, [sp, 124]
	msub	w2, w1, w19, w0
	and	w0, w2, 65535
	stp	wzr, w0, [sp, 128]
	b	.L1619
.L1620:
	cmp	w19, w27
	bne	.L1623
	ldr	x0, [x24, 3472]
.L1655:
	str	x0, [x3, 8]
	ldr	w0, [sp, 180]
	cmn	w0, #1
	beq	.L1625
	str	w19, [sp, 208]
	mov	w2, 0
	str	w0, [sp, 188]
	mov	w1, 1
	ldr	x0, [x3, 8]
	str	x0, [sp, 192]
	ldr	x0, [x3, 16]
	str	x0, [sp, 200]
	add	x0, sp, 184
	bl	FlashReadPages
	ldr	w3, [sp, 184]
	cmn	w3, #1
	bne	.L1626
	ldr	w0, [x24, 2784]
	mov	w2, w19
	add	w0, w0, 1
	str	w0, [x24, 2784]
	adrp	x0, .LC125
	add	x0, x0, :lo12:.LC125
	ldr	w1, [x22, 8]
	bl	sftl_printk
.L1630:
	ldr	w0, [sp, 132]
	cmp	w19, w27
	lsl	w2, w0, 9
	bne	.L1631
	ldr	x0, [sp, 112]
	ldr	x1, [x24, 3440]
	add	x1, x1, x0
	ldr	w0, [sp, 128]
	ldr	x3, [x1, 8]
	lsl	w0, w0, 9
	mov	x1, x25
	add	x0, x3, x0
.L1656:
	bl	ftl_memcpy
	b	.L1622
.L1623:
	ldr	x0, [x24, 3480]
	b	.L1655
.L1626:
	ldr	w0, [x22, 8]
	cmp	w0, w19
	beq	.L1630
	ldr	w0, [x24, 2784]
	mov	w2, w19
	add	w0, w0, 1
	str	w0, [x24, 2784]
	ldr	x0, [sp, 168]
	ldr	w1, [x22, 8]
	bl	sftl_printk
	ldr	w0, [x22, 8]
	cmp	w0, w19
	beq	.L1630
	mov	x1, x23
	mov	x0, x28
	mov	w2, 1133
	bl	sftl_printk
	b	.L1630
.L1625:
	ldrh	w2, [x24, 314]
	mov	w1, 0
	ldr	x0, [x3, 8]
	bl	ftl_memset
	b	.L1630
.L1631:
	ldrh	w1, [x24, 262]
	ldr	x3, [sp, 112]
	ldr	x0, [x24, 3440]
	mul	w1, w1, w19
	add	x0, x0, x3
	sub	w1, w1, w21
	lsl	w1, w1, 9
	add	x1, x25, x1
	ldr	x0, [x0, 8]
	b	.L1656
.L1617:
	ldr	x2, [sp, 112]
	mul	w1, w1, w19
	ldr	x0, [x24, 3440]
	sub	w1, w1, w21
	add	x0, x0, x2
	lsl	w1, w1, 9
	add	x1, x25, x1
	str	x1, [x0, 8]
	b	.L1622
.L1649:
	mov	w19, 256
	mov	w21, 65535
	mov	w20, 128
.L1638:
	ldrh	w0, [x24, 176]
	ldrh	w1, [x24, 226]
	and	w0, w0, w1
	cmp	w21, w0, uxth
	bne	.L1639
	mov	w0, 0
	bl	List_get_gc_head_node
	bl	FtlGcRefreshBlock
.L1639:
	mov	w1, 1
	mov	w0, w1
	strh	w20, [x24, 3224]
	strh	w20, [x24, 3226]
	bl	rk_ftl_garbage_collect
	mov	w1, 1
	mov	w0, 0
	bl	rk_ftl_garbage_collect
	ldrh	w0, [x24, 228]
	cmp	w0, 2
	bhi	.L1640
	subs	w19, w19, #1
	bne	.L1638
	b	.L1640
.L1644:
	mov	w0, -1
	b	.L1603
.L1641:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 224
	ret
	.size	FtlWrite, .-FtlWrite
	.align	2
	.global	sftl_gc
	.type	sftl_gc, %function
sftl_gc:
	stp	x29, x30, [sp, -16]!
	mov	w1, 1
	mov	w0, w1
	mov	x29, sp
	bl	rk_ftl_garbage_collect
	ldp	x29, x30, [sp], 16
	ret
	.size	sftl_gc, .-sftl_gc
	.section	.rodata.str1.1
.LC127:
	.string	"%s hash error this.id =%x page =%x pre_id =%x hash =%x hash_r =%x\n"
	.text
	.align	2
	.global	FtlLoadSysInfo
	.type	FtlLoadSysInfo, %function
FtlLoadSysInfo:
	stp	x29, x30, [sp, -80]!
	mov	w1, 0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x23, x24, [sp, 48]
	add	x24, x19, 3712
	stp	x21, x22, [sp, 32]
	ldrh	w2, [x19, 244]
	str	x25, [sp, 64]
	ldr	x0, [x19, 3448]
	str	x0, [x24, 8]
	ldr	x0, [x19, 3512]
	str	x0, [x24, 16]
	ldr	x0, [x19, 72]
	lsl	w2, w2, 1
	bl	ftl_memset
	ldrh	w0, [x19, 2696]
	mov	w1, 65535
	cmp	w0, w1
	bne	.L1660
.L1672:
	mov	w0, -1
.L1659:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldr	x25, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
.L1660:
	add	x21, x19, 2560
	mov	w1, 1
	bl	FtlGetLastWrittenPage
	sxth	w20, w0
	add	w0, w20, 1
	mov	w23, 19539
	ldrsh	w22, [x21, 136]
	movk	w23, 0x4654, lsl 16
	strh	w0, [x21, 138]
	adrp	x21, .LANCHOR1
	add	x21, x21, :lo12:.LANCHOR1
	add	x21, x21, 600
.L1662:
	tbz	w20, #31, .L1669
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	add	x1, x1, 600
	mov	w2, 1474
	adrp	x0, .LC8
	add	x0, x0, :lo12:.LC8
	bl	sftl_printk
	b	.L1668
.L1669:
	orr	w0, w20, w22, lsl 10
	str	w0, [x24, 4]
	ldr	x0, [x19, 3448]
	mov	w2, 1
	mov	w1, w2
	str	x0, [x24, 8]
	mov	x0, x24
	bl	FlashReadPages
	ldr	x0, [x24, 16]
	ldr	w7, [x0, 12]
	ldr	w0, [x24]
	cmn	w0, #1
	cbz	w7, .L1663
	beq	.L1664
	ldrh	w1, [x19, 314]
	ldr	x0, [x24, 8]
	bl	js_hash
	mov	w6, w0
	cmp	w7, w0
	beq	.L1665
	ldrh	w4, [x19, 2700]
	adrp	x0, .LC127
	mov	w5, w7
	mov	w3, w20
	mov	w2, w22
	mov	x1, x21
	add	x0, x0, :lo12:.LC127
	bl	sftl_printk
	cbnz	w20, .L1666
	ldrh	w0, [x19, 2700]
	cmp	w22, w0
	beq	.L1666
	ldrh	w20, [x19, 308]
	sxth	w22, w0
.L1664:
	sub	w20, w20, #1
	sxth	w20, w20
	b	.L1662
.L1666:
	mov	w0, -1
	str	w0, [x24]
	b	.L1664
.L1663:
	beq	.L1664
.L1665:
	ldr	x0, [x19, 3448]
	ldr	w0, [x0]
	cmp	w0, w23
	bne	.L1664
	ldr	x0, [x19, 3512]
	ldrh	w1, [x0]
	mov	w0, 61604
	cmp	w1, w0
	bne	.L1664
.L1668:
	ldrh	w0, [x19, 244]
	ldrh	w1, [x19, 314]
	add	x0, x0, 24
	cmp	x1, x0, lsl 1
	bcs	.L1671
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 600
	add	x0, x0, :lo12:.LC8
	mov	w2, 1476
	bl	sftl_printk
.L1671:
	ldr	x1, [x19, 3720]
	add	x21, x19, 2512
	mov	w2, 48
	mov	x0, x21
	bl	ftl_memcpy
	ldr	x0, [x19, 72]
	ldrh	w2, [x19, 244]
	ldr	x1, [x19, 3720]
	lsl	w2, w2, 1
	add	x1, x1, 48
	bl	ftl_memcpy
	ldr	w1, [x19, 2512]
	mov	w0, 19539
	movk	w0, 0x4654, lsl 16
	cmp	w1, w0
	bne	.L1672
	ldrh	w1, [x19, 2520]
	ldrb	w2, [x19, 2522]
	ldrh	w0, [x19, 258]
	strh	w1, [x19, 2702]
	cmp	w2, w0
	bne	.L1672
	ldrh	w0, [x19, 306]
	ldrh	w2, [x19, 262]
	ldrh	w3, [x19, 236]
	str	w1, [x19, 3964]
	mul	w0, w0, w1
	str	w0, [x19, 2624]
	mul	w0, w2, w0
	ldr	w2, [x19, 248]
	str	w0, [x19, 344]
	ldrh	w0, [x19, 366]
	add	w0, w0, w1
	sub	w0, w2, w0
	udiv	w0, w0, w3
	strh	w0, [x19, 2692]
	cmp	w1, w2
	bls	.L1673
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC8
	add	x1, x1, 600
	add	x0, x0, :lo12:.LC8
	mov	w2, 1498
	bl	sftl_printk
.L1673:
	ldrh	w2, [x19, 2528]
	add	x0, x19, 2660
	ldr	w1, [x19, 2544]
	ldrh	w3, [x19, 2526]
	lsr	w4, w2, 6
	and	w2, w2, 63
	strb	w2, [x19, 30]
	ldrb	w2, [x19, 2523]
	strb	w2, [x19, 32]
	mov	w2, 65535
	str	w2, [x19, 176]
	ldrh	w2, [x19, 2530]
	strh	w2, [x19, 80]
	ldrh	w2, [x19, 2532]
	strh	w4, [x19, 26]
	stp	wzr, wzr, [x0, -16]
	lsr	w4, w2, 6
	and	w2, w2, 63
	strb	w2, [x19, 86]
	ldrb	w2, [x19, 2524]
	strb	w2, [x19, 88]
	ldrh	w2, [x19, 2534]
	strh	w2, [x19, 128]
	ldrh	w2, [x19, 2536]
	strh	w4, [x19, 82]
	stp	w1, wzr, [x0, 8]
	lsr	w4, w2, 6
	and	w2, w2, 63
	strb	w2, [x19, 134]
	ldrb	w2, [x19, 2525]
	strh	w3, [x19, 24]
	strh	w4, [x19, 130]
	strb	w2, [x19, 136]
	strb	wzr, [x19, 182]
	strb	wzr, [x19, 184]
	str	wzr, [x19, 2628]
	str	wzr, [x19, 2636]
	str	wzr, [x19, 2684]
	str	wzr, [x19, 2640]
	ldr	w0, [x19, 2552]
	ldr	w1, [x19, 2660]
	cmp	w0, w1
	bls	.L1674
	str	w0, [x19, 2660]
.L1674:
	ldr	w0, [x19, 2548]
	ldr	w1, [x19, 2664]
	cmp	w0, w1
	bls	.L1675
	str	w0, [x19, 2664]
.L1675:
	mov	w0, 65535
	cmp	w3, w0
	beq	.L1676
	add	x0, x19, 24
	bl	make_superblock
.L1676:
	ldrh	w2, [x19, 80]
	add	x0, x19, 80
	mov	w1, 65535
	cmp	w2, w1
	beq	.L1677
	bl	make_superblock
.L1677:
	ldrh	w2, [x19, 128]
	add	x0, x19, 128
	mov	w1, 65535
	cmp	w2, w1
	beq	.L1678
	bl	make_superblock
.L1678:
	ldrh	w2, [x19, 176]
	add	x0, x19, 176
	mov	w1, 65535
	cmp	w2, w1
	beq	.L1679
	bl	make_superblock
.L1679:
	mov	w0, 0
	b	.L1659
	.size	FtlLoadSysInfo, .-FtlLoadSysInfo
	.section	.rodata.str1.1
.LC128:
	.string	"%s last blk_id =%x page =%x hash error hash =%x hash_r =%x\n"
.LC129:
	.string	"%s scan blk_id =%x page =%x hash error hash =%x hash_r =%x\n"
	.text
	.align	2
	.global	FtlMapTblRecovery
	.type	FtlMapTblRecovery, %function
FtlMapTblRecovery:
	stp	x29, x30, [sp, -144]!
	mov	w1, 0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	mov	w20, 0
	stp	x21, x22, [sp, 32]
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	ldrh	w23, [x0, 6]
	ldr	x22, [x0, 40]
	ldp	x27, x28, [x0, 16]
	lsl	w2, w23, 2
	ldrh	w0, [x0, 8]
	str	w0, [sp, 112]
	mov	x0, x22
	bl	ftl_memset
	str	xzr, [x19, 48]
	adrp	x6, .LANCHOR0
	add	x24, x6, :lo12:.LANCHOR0
	mov	w0, -1
	ldr	x1, [x24, 3448]
	str	x1, [x24, 3720]
	ldr	x21, [x24, 3512]
	str	x21, [x24, 3728]
	str	w0, [x19]
	mov	w0, 1
	str	w0, [x19, 56]
	ldr	w0, [sp, 112]
	sub	w0, w0, #1
	str	w0, [sp, 124]
	adrp	x0, .LANCHOR1
	add	x0, x0, :lo12:.LANCHOR1
	add	x0, x0, 615
	str	x0, [sp, 128]
.L1689:
	ldr	w0, [sp, 112]
	cmp	w20, w0
	bge	.L1709
	add	x0, x27, w20, sxtw 1
	str	x0, [sp, 104]
	ldr	w0, [sp, 124]
	sxtw	x26, w20
	sbfiz	x1, x20, 1, 32
	cmp	w0, w20
	bne	.L1690
	ldr	x0, [sp, 104]
	mov	w1, 1
	adrp	x27, .LC128
	add	x27, x27, :lo12:.LC128
	ldrh	w0, [x0]
	bl	FtlGetLastWrittenPage
	sxth	w25, w0
	add	w25, w25, 1
	strh	w20, [x19]
	strh	w25, [x19, 2]
	ldr	w0, [x28, x26, lsl 2]
	adrp	x26, .LANCHOR1
	add	x26, x26, :lo12:.LANCHOR1
	mov	w28, 0
	add	x26, x26, 615
	str	w0, [x19, 48]
.L1691:
	cmp	w28, w25
	blt	.L1695
.L1709:
	mov	x0, x19
	bl	ftl_free_no_use_map_blk
	ldrh	w1, [x19, 2]
	ldrh	w0, [x24, 308]
	cmp	w1, w0
	bne	.L1697
	mov	x0, x19
	bl	ftl_map_blk_alloc_new_blk
.L1697:
	mov	x0, x19
	bl	ftl_map_blk_gc
	mov	x0, x19
	bl	ftl_map_blk_gc
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 144
	ret
.L1695:
	ldr	x0, [sp, 104]
	add	x3, x24, 3712
	mov	w2, 1
	mov	w1, w2
	str	x3, [sp, 112]
	ldrh	w0, [x0]
	orr	w0, w28, w0, lsl 10
	str	w0, [x24, 3716]
	mov	x0, x3
	bl	FlashReadPages
	ldr	x3, [sp, 112]
	ldr	x0, [x3, 16]
	ldr	w7, [x0, 12]
	ldr	w0, [x24, 3712]
	cmn	w0, #1
	cbz	w7, .L1692
	beq	.L1710
	ldrh	w1, [x24, 314]
	ldr	x0, [x3, 8]
	bl	js_hash
	mov	w5, w0
	cmp	w7, w0
	beq	.L1694
	mov	x0, x27
	mov	w4, w7
	mov	w3, w28
	mov	w2, w20
	mov	x1, x26
	bl	sftl_printk
	mov	w0, -1
	str	w0, [x24, 3712]
.L1710:
	add	w28, w28, 1
	sxth	w28, w28
	b	.L1691
.L1692:
	beq	.L1710
.L1694:
	ldrh	w0, [x21, 8]
	cmp	w0, w23
	bcs	.L1710
	ldrh	w2, [x21]
	ldrh	w1, [x19, 4]
	cmp	w2, w1
	bne	.L1710
	ubfiz	x0, x0, 2, 16
	ldr	w1, [x24, 3716]
	str	w1, [x22, x0]
	b	.L1710
.L1690:
	add	x0, x24, 3712
	ldr	x2, [x24, 3448]
	str	x2, [x0, 8]
	ldrh	w2, [x24, 308]
	ldrh	w1, [x27, x1]
	sub	w2, w2, #1
	orr	w1, w2, w1, lsl 10
	mov	w2, 1
	str	w1, [x0, 4]
	mov	w1, w2
	bl	FlashReadPages
	ldr	w0, [x24, 3712]
	cmn	w0, #1
	beq	.L1713
	ldrh	w1, [x21]
	ldrh	w0, [x19, 4]
	cmp	w1, w0
	bne	.L1713
	ldrh	w1, [x21, 8]
	mov	w0, 64245
	cmp	w1, w0
	beq	.L1714
.L1713:
	adrp	x25, .LC129
	mov	w26, 0
	add	x25, x25, :lo12:.LC129
	b	.L1705
.L1701:
	ldr	x3, [x24, 3448]
	lsl	w1, w0, 1
	ldr	w4, [x3, w1, sxtw 2]
	and	w2, w4, 65535
	cmp	w23, w4, uxth
	bls	.L1700
	add	w1, w1, 1
	ldr	w3, [x3, w1, sxtw 2]
	ubfiz	x1, x2, 2, 16
	str	w3, [x22, x1]
.L1700:
	add	w0, w0, 1
	sxth	w0, w0
.L1699:
	ldrh	w1, [x24, 308]
	sub	w1, w1, #1
	cmp	w0, w1
	blt	.L1701
.L1707:
	add	w20, w20, 1
	sxth	w20, w20
	b	.L1689
.L1714:
	mov	w0, 0
	b	.L1699
.L1706:
	ldr	x0, [sp, 104]
	add	x5, x24, 3712
	mov	w2, 1
	mov	w1, w2
	str	x5, [sp, 136]
	ldrh	w0, [x0]
	orr	w0, w26, w0, lsl 10
	str	w0, [x24, 3716]
	mov	x0, x5
	bl	FlashReadPages
	ldr	x5, [sp, 136]
	ldr	x0, [x5, 16]
	ldr	w7, [x0, 12]
	ldr	w0, [x24, 3712]
	cmn	w0, #1
	cbz	w7, .L1702
	beq	.L1711
	ldrh	w1, [x24, 314]
	ldr	x0, [x5, 8]
	bl	js_hash
	mov	w5, w0
	cmp	w7, w0
	beq	.L1704
	ldr	x1, [sp, 128]
	mov	x0, x25
	mov	w4, w7
	mov	w3, w26
	mov	w2, w20
	bl	sftl_printk
	mov	w0, -1
	str	w0, [x24, 3712]
.L1711:
	add	w3, w26, 1
	sxth	w26, w3
.L1705:
	ldrh	w0, [x24, 308]
	cmp	w26, w0
	blt	.L1706
	b	.L1707
.L1702:
	beq	.L1711
.L1704:
	ldrh	w0, [x21, 8]
	cmp	w0, w23
	bcs	.L1711
	ldrh	w2, [x21]
	ldrh	w1, [x19, 4]
	cmp	w2, w1
	bne	.L1711
	ubfiz	x0, x0, 2, 16
	ldr	w1, [x24, 3716]
	str	w1, [x22, x0]
	b	.L1711
	.size	FtlMapTblRecovery, .-FtlMapTblRecovery
	.align	2
	.global	FtlLoadVonderInfo
	.type	FtlLoadVonderInfo, %function
FtlLoadVonderInfo:
	adrp	x1, .LANCHOR0
	add	x1, x1, :lo12:.LANCHOR0
	stp	x29, x30, [sp, -16]!
	add	x0, x1, 3880
	mov	x29, sp
	ldrh	w2, [x1, 324]
	strh	w2, [x0, 10]
	mov	w2, -3962
	strh	w2, [x0, 4]
	ldrh	w2, [x1, 348]
	strh	w2, [x0, 8]
	ldrh	w2, [x1, 326]
	strh	w2, [x0, 6]
	ldr	x2, [x1, 352]
	str	x2, [x0, 16]
	ldr	x2, [x1, 3584]
	str	x2, [x0, 24]
	ldr	x2, [x1, 3576]
	ldr	x1, [x1, 3592]
	stp	x2, x1, [x0, 32]
	bl	FtlMapTblRecovery
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
	.size	FtlLoadVonderInfo, .-FtlLoadVonderInfo
	.align	2
	.global	FtlLoadMapInfo
	.type	FtlLoadMapInfo, %function
FtlLoadMapInfo:
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	bl	FtlL2PDataInit
	adrp	x0, .LANCHOR0
	add	x0, x0, :lo12:.LANCHOR0
	add	x0, x0, 3632
	bl	FtlMapTblRecovery
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
	.size	FtlLoadMapInfo, .-FtlLoadMapInfo
	.align	2
	.global	FtlSysBlkInit
	.type	FtlSysBlkInit, %function
FtlSysBlkInit:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	ldrh	w0, [x19, 240]
	strh	wzr, [x19, 3744]
	bl	FtlFreeSysBlkQueueInit
	bl	FtlScanSysBlk
	ldrh	w1, [x19, 2696]
	mov	w0, 65535
	cmp	w1, w0
	bne	.L1733
.L1735:
	mov	w20, -1
.L1732:
	mov	w0, w20
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L1733:
	bl	FtlLoadSysInfo
	mov	w20, w0
	cbnz	w0, .L1735
	bl	FtlLoadMapInfo
	bl	FtlLoadVonderInfo
	bl	Ftl_load_ext_data
	bl	FtlLoadEctTbl
	bl	FtlFreeSysBLkSort
	bl	SupperBlkListInit
	bl	FtlPowerLostRecovery
	mov	w0, 1
	bl	FtlUpdateVaildLpn
	ldr	x1, [x19, 2608]
	mov	x0, 0
	ldrh	w2, [x19, 342]
	add	x1, x1, 4
.L1736:
	mov	w3, w0
	cmp	w2, w0
	ble	.L1740
	lsl	x4, x0, 4
	add	x0, x0, 1
	ldr	w4, [x1, x4]
	tbz	w4, #31, .L1736
.L1740:
	ldrh	w0, [x19, 2540]
	add	w0, w0, 1
	strh	w0, [x19, 2540]
	cmp	w3, w2
	bge	.L1747
.L1737:
	add	x22, x19, 24
	add	x21, x19, 80
	mov	x0, x22
	bl	FtlSuperblockPowerLostFix
	mov	x0, x21
	bl	FtlSuperblockPowerLostFix
	ldrh	w0, [x19, 24]
	ldr	x2, [x19, 72]
	lsl	x0, x0, 1
	ldrh	w3, [x19, 28]
	ldrh	w1, [x2, x0]
	sub	w1, w1, w3
	strh	w1, [x2, x0]
	strh	wzr, [x19, 28]
	ldrh	w0, [x19, 306]
	strh	w0, [x19, 26]
	ldrh	w0, [x19, 80]
	ldr	x2, [x19, 72]
	strb	wzr, [x19, 30]
	lsl	x0, x0, 1
	ldrh	w3, [x19, 84]
	ldrh	w1, [x2, x0]
	sub	w1, w1, w3
	strh	w1, [x2, x0]
	strh	wzr, [x19, 84]
	ldrh	w0, [x19, 306]
	strh	w0, [x19, 82]
	ldrh	w0, [x19, 2542]
	strb	wzr, [x19, 86]
	add	w0, w0, 1
	strh	w0, [x19, 2542]
	bl	l2p_flush
	bl	FtlVpcTblFlush
	bl	FtlVpcTblFlush
	b	.L1741
.L1747:
	ldrh	w0, [x19, 3744]
	cbnz	w0, .L1737
.L1741:
	ldrh	w0, [x19, 24]
	add	x22, x19, 24
	mov	w1, 65535
	cmp	w0, w1
	beq	.L1742
	ldrh	w1, [x19, 28]
	add	x21, x19, 80
	ldrh	w2, [x19, 84]
	orr	w1, w1, w2
	tst	w1, 65535
	bne	.L1742
	bl	FtlGcRefreshBlock
	ldrh	w0, [x19, 80]
	bl	FtlGcRefreshBlock
	bl	FtlVpcTblFlush
	mov	x0, x22
	bl	allocate_new_data_superblock
	mov	x0, x21
	bl	allocate_new_data_superblock
.L1742:
	bl	FtlVpcCheckAndModify
	b	.L1732
	.size	FtlSysBlkInit, .-FtlSysBlkInit
	.align	2
	.global	ftl_low_format
	.type	ftl_low_format, %function
ftl_low_format:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	str	x21, [sp, 32]
	ldrh	w0, [x19, 240]
	str	wzr, [x19, 2660]
	str	wzr, [x19, 2664]
	str	wzr, [x19, 2676]
	bl	FtlFreeSysBlkQueueInit
	bl	FtlLoadBbt
	cbz	w0, .L1750
	bl	FtlMakeBbt
.L1750:
	mov	w3, 23752
	mov	w0, 0
	movk	w3, 0xa0f, lsl 16
.L1751:
	ldrh	w1, [x19, 262]
	cmp	w0, w1, lsl 7
	blt	.L1752
	ldrh	w21, [x19, 244]
	mov	w20, 0
.L1753:
	ldrh	w0, [x19, 246]
	cmp	w0, w21
	bhi	.L1754
	ldrh	w0, [x19, 236]
	sub	w1, w20, #3
	cmp	w1, w0, lsl 1
	bge	.L1755
.L1759:
	mov	w20, 0
	mov	w21, 0
.L1756:
	ldrh	w0, [x19, 244]
	cmp	w0, w21
	bhi	.L1760
	ldrh	w0, [x19, 246]
	ldrh	w2, [x19, 236]
	str	w0, [x19, 3420]
	ldr	w0, [x19, 248]
	udiv	w4, w0, w2
	ubfx	x3, x4, 5, 16
	str	w4, [x19, 2624]
	add	w1, w3, 36
	strh	w1, [x19, 2692]
	mov	w1, 24
	mul	w1, w2, w1
	cmp	w20, w1
	ble	.L1761
	sub	w0, w0, w20
	udiv	w0, w0, w2
	str	w0, [x19, 2624]
	lsr	w0, w0, 5
	add	w0, w0, 24
	strh	w0, [x19, 2692]
.L1761:
	ldrh	w1, [x19, 298]
	cbz	w1, .L1763
	ldrh	w0, [x19, 2692]
	add	w0, w0, w1, lsr 1
	strh	w0, [x19, 2692]
	mul	w0, w1, w2
	cmp	w20, w0
	bge	.L1763
	add	w1, w1, 32
	str	w4, [x19, 2624]
	add	w1, w3, w1
	strh	w1, [x19, 2692]
.L1763:
	ldrh	w1, [x19, 2692]
	add	x21, x19, 24
	ldr	w0, [x19, 2624]
	mov	w20, -1
	sub	w0, w0, w1
	ldrh	w1, [x19, 306]
	mul	w0, w0, w2
	str	w0, [x19, 3964]
	mul	w0, w1, w0
	ldrh	w1, [x19, 262]
	str	w0, [x19, 2624]
	mul	w0, w1, w0
	str	w0, [x19, 344]
	bl	FtlBbmTblFlush
	ldr	x0, [x19, 72]
	mov	w1, 0
	ldrh	w2, [x19, 246]
	lsl	w2, w2, 1
	bl	ftl_memset
	mov	w1, 65535
	mov	w0, 1
	strb	wzr, [x21, 6]
	strb	w0, [x21, 8]
	str	wzr, [x19, 24]
	str	w1, [x19, 176]
	strb	wzr, [x19, 182]
	strb	wzr, [x19, 184]
	str	wzr, [x19, 2620]
.L1765:
	mov	x0, x21
	bl	make_superblock
	ldrb	w1, [x21, 7]
	ldrh	w0, [x21]
	cbnz	w1, .L1766
	ldr	x1, [x19, 72]
	ubfiz	x0, x0, 1, 16
	strh	w20, [x1, x0]
	ldrh	w0, [x21]
	add	w0, w0, 1
	strh	w0, [x21]
	b	.L1765
.L1752:
	ldr	x4, [x19, 3472]
	ubfiz	x2, x0, 2, 16
	mvn	w1, w0
	orr	w1, w0, w1, lsl 16
	str	w1, [x4, x2]
	add	w0, w0, 1
	ldr	x1, [x19, 3480]
	and	w0, w0, 65535
	str	w3, [x1, x2]
	b	.L1751
.L1754:
	mov	w0, w21
	mov	w1, 1
	add	w21, w21, 1
	bl	FtlLowFormatEraseBlock
	add	w20, w0, w20
	and	w21, w21, 65535
	and	w20, w20, 65535
	b	.L1753
.L1755:
	udiv	w20, w20, w0
	ldr	w0, [x19, 336]
	add	w0, w20, w0
	bl	FtlSysBlkNumInit
	ldrh	w0, [x19, 240]
	bl	FtlFreeSysBlkQueueInit
	ldrh	w20, [x19, 244]
.L1757:
	ldrh	w0, [x19, 246]
	cmp	w0, w20
	bls	.L1759
	mov	w0, w20
	add	w20, w20, 1
	mov	w1, 1
	and	w20, w20, 65535
	bl	FtlLowFormatEraseBlock
	b	.L1757
.L1760:
	mov	w0, w21
	mov	w1, 0
	add	w21, w21, 1
	bl	FtlLowFormatEraseBlock
	add	w20, w0, w20
	and	w21, w21, 65535
	and	w20, w20, 65535
	b	.L1756
.L1766:
	ldr	w1, [x19, 2660]
	ubfiz	x0, x0, 1, 16
	str	w1, [x21, 12]
	add	x20, x19, 80
	add	w1, w1, 1
	str	w1, [x19, 2660]
	ldr	x1, [x19, 72]
	ldrh	w2, [x21, 4]
	strh	w2, [x1, x0]
	strh	wzr, [x20, 2]
	ldrh	w0, [x21]
	mov	w21, -1
	strb	wzr, [x20, 6]
	add	w0, w0, 1
	strh	w0, [x19, 80]
	mov	w0, 1
	strb	w0, [x20, 8]
.L1767:
	mov	x0, x20
	bl	make_superblock
	ldrb	w1, [x20, 7]
	ldrh	w0, [x20]
	cbnz	w1, .L1768
	ldr	x1, [x19, 72]
	ubfiz	x0, x0, 1, 16
	strh	w21, [x1, x0]
	ldrh	w0, [x20]
	add	w0, w0, 1
	strh	w0, [x20]
	b	.L1767
.L1768:
	ldr	w1, [x19, 2660]
	ubfiz	x0, x0, 1, 16
	str	w1, [x20, 12]
	add	w1, w1, 1
	str	w1, [x19, 2660]
	ldr	x1, [x19, 72]
	ldrh	w2, [x20, 4]
	strh	w2, [x1, x0]
	mov	w0, -1
	strh	w0, [x19, 128]
	bl	FtlFreeSysBlkQueueOut
	strh	w0, [x19, 2696]
	add	x1, x19, 2560
	mov	w0, -65536
	str	w0, [x1, 138]
	ldr	w0, [x19, 3964]
	strh	w0, [x19, 2702]
	ldr	w0, [x19, 2660]
	str	w0, [x19, 2704]
	add	w0, w0, 1
	str	w0, [x19, 2660]
	bl	FtlVpcTblFlush
	bl	FtlSysBlkInit
	cbnz	w0, .L1769
	adrp	x0, .LANCHOR2
	mov	w1, 1
	str	w1, [x0, #:lo12:.LANCHOR2]
.L1769:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
	.size	ftl_low_format, .-ftl_low_format
	.align	2
	.global	sftl_init
	.type	sftl_init, %function
sftl_init:
	stp	x29, x30, [sp, -32]!
	adrp	x1, .LC0
	add	x1, x1, :lo12:.LC0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR2
	mov	w0, -1
	adrp	x19, .LANCHOR0
	add	x19, x19, :lo12:.LANCHOR0
	str	w0, [x20, #:lo12:.LANCHOR2]
	adrp	x0, .LC77
	add	x0, x0, :lo12:.LC77
	bl	sftl_printk
	mov	x0, x19
	bl	FtlConstantsInit
	bl	FtlMemInit
	bl	FtlVariablesInit
	ldrh	w0, [x19, 240]
	bl	FtlFreeSysBlkQueueInit
	bl	FtlLoadBbt
	cbnz	w0, .L1775
	bl	FtlSysBlkInit
	cbnz	w0, .L1775
	mov	w0, 1
	str	w0, [x20, #:lo12:.LANCHOR2]
	ldrh	w0, [x19, 228]
	cmp	w0, 15
	bhi	.L1775
	mov	w19, 8129
.L1776:
	mov	w1, 1
	mov	w0, 0
	bl	rk_ftl_garbage_collect
	subs	w19, w19, #1
	bne	.L1776
.L1775:
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	sftl_init, .-sftl_init
	.section	.rodata.str1.1
.LC130:
	.string	"write_idblock %x %x\n"
.LC131:
	.string	"write_idblock fail! %x %x %x %x\n"
.LC132:
	.string	"%s idb buffer alloc fail\n"
	.text
	.align	2
	.global	FtlWriteToIDB
	.type	FtlWriteToIDB, %function
FtlWriteToIDB:
	stp	x29, x30, [sp, -208]!
	adrp	x5, .LANCHOR0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	w20, w0
	mrs	x0, sp_el0
	stp	x21, x22, [sp, 32]
	mov	w19, w1
	add	w22, w19, w20
	stp	x23, x24, [sp, 48]
	sub	w23, w22, #1
	cmp	w23, 63
	stp	x25, x26, [sp, 64]
	add	x25, x5, :lo12:.LANCHOR0
	stp	x27, x28, [sp, 80]
	ldr	x1, [x0, 952]
	str	x1, [sp, 200]
	mov	x1, 0
	bls	.L1780
	cmp	w20, 575
	bls	.L1781
.L1780:
	ldr	w0, [x25, 3968]
	cbnz	w0, .L1782
.L1844:
	mov	w19, 0
	b	.L1779
.L1782:
	ldr	x22, [x25, 3976]
	mov	w0, 35899
	movk	w0, 0xfcdc, lsl 16
	ldr	w1, [x22]
	cmp	w1, w0
	bne	.L1784
	add	x1, x22, 258048
	ldrh	w2, [x25, 10]
	add	x1, x1, 2044
	mov	w0, 0
	mov	w19, 65023
.L1788:
	ldr	w3, [x1]
	cbnz	w3, .L1785
	ldr	w3, [x22, w0, uxtw 2]
	add	w0, w0, 1
	str	w3, [x1, 2048]
	cmp	w0, 4096
	bls	.L1786
	mov	w0, 0
.L1786:
	sub	w19, w19, #1
	sub	x1, x1, #4
	cmp	w19, 4096
	bne	.L1788
	mov	w19, 512
	b	.L1787
.L1785:
	add	w19, w19, 127
	lsr	w19, w19, 7
.L1787:
	ubfiz	w0, w2, 2, 14
	add	w20, w19, 4
	mov	w1, w19
	mov	w23, 0
	mov	w21, 0
	udiv	w20, w20, w0
	adrp	x0, .LC130
	add	x0, x0, :lo12:.LC130
	add	w20, w20, 1
	mov	w2, w20
	bl	sftl_printk
	lsl	w0, w19, 7
	str	w0, [sp, 100]
.L1789:
	add	w0, w20, w21
	str	w0, [sp, 104]
	cmp	w0, 8
	bls	.L1811
	cbnz	w23, .L1810
.L1784:
	mov	w19, -1
	b	.L1810
.L1811:
	ldr	x0, [x25, 3984]
	mov	w1, 0
	mov	x2, 512
	bl	memset
	ldrh	w27, [x25, 10]
	ldr	x1, [x25, 3384]
	mul	w0, w21, w27
	str	w0, [sp, 96]
	cbz	x1, .L1820
	ldr	x0, [x25, 3392]
	mov	w24, 6
	cmp	x0, 0
	mov	w0, 9
	csel	w24, w24, w0, eq
.L1790:
	mul	w28, w21, w27
	mov	w26, 0
.L1791:
	ldr	x3, [x25, 3360]
	mov	w1, w28
	add	w26, w26, 1
	add	w28, w28, w27
	mov	w0, 0
	blr	x3
	cmp	w20, w26
	bhi	.L1791
	cmp	w24, 9
	bne	.L1822
	ldr	x28, [x25, 3984]
	mov	w2, 1024
	mov	w1, 0
	mov	x0, x28
	bl	ftl_memset
	mov	x0, 18766
	str	wzr, [x28, 12]
	movk	x0, 0x464e, lsl 16
	mov	w1, 12
	movk	x0, 0xc, lsl 32
	str	x0, [x28]
	mov	w0, 1024
	strh	w0, [x28, 16]
	ldrh	w0, [x25, 10]
	strh	w0, [x28, 18]
	mov	w0, 4096
	str	w0, [x28, 20]
	add	x0, x28, 12
	bl	js_hash
	str	w0, [x28, 8]
.L1792:
	mul	w0, w20, w27
	mov	x27, x22
	mov	w26, 0
	str	w0, [sp, 108]
.L1793:
	ldr	w0, [sp, 108]
	cmp	w26, w0
	beq	.L1800
	cmp	w24, 9
	lsl	w0, w26, 2
	csinc	w0, w0, w26, ne
	str	w0, [sp, 136]
	mov	w0, 61424
	str	w0, [sp, 140]
	cmp	w26, 0
	ccmp	w24, 9, 0, eq
	bne	.L1796
	ldr	x1, [x25, 3384]
	add	x8, x25, 3352
	mov	w0, 70
	str	x8, [sp, 112]
	blr	x1
	ldr	w1, [sp, 96]
	ldr	x8, [sp, 112]
	add	x3, sp, 136
	mov	x2, x28
	mov	w0, 0
	str	x8, [sp, 120]
	ldr	x9, [x8, 16]
	blr	x9
	mov	w1, w0
	ldr	x8, [sp, 120]
	str	w1, [sp, 112]
	ldrb	w0, [x25, 22]
	ldr	x2, [x8, 32]
	blr	x2
	ldr	w1, [sp, 112]
	cmn	w1, #1
	bne	.L1797
.L1800:
	ldrb	w0, [x25, 14]
	add	x1, x25, 3352
	str	w0, [sp, 108]
	ldrh	w0, [x25, 10]
	ldr	x3, [x1, 32]
	ldr	x26, [x25, 3984]
	mul	w27, w21, w0
	cbnz	x3, .L1798
	mov	w1, 6
.L1843:
	mul	w0, w20, w0
	mov	w28, 0
	str	w1, [sp, 96]
	str	w0, [sp, 112]
.L1802:
	ldr	w0, [sp, 112]
	cmp	w28, w0
	beq	.L1806
	ldr	w0, [sp, 96]
	cmp	w28, 0
	ccmp	w0, 9, 0, eq
	bne	.L1803
	ldr	x1, [x25, 3384]
	mov	w0, 70
	blr	x1
	ldr	x1, [x25, 3392]
	mov	w0, 2
	blr	x1
	add	x3, sp, 136
	ldr	x7, [x25, 3376]
	mov	x2, x26
	mov	w1, w27
	mov	w0, 0
	blr	x7
	ldrb	w0, [sp, 108]
	ldr	x1, [x25, 3392]
	blr	x1
	ldrb	w0, [x25, 22]
	ldr	x1, [x25, 3384]
	blr	x1
	ldr	w0, [x26]
	mov	w1, 18766
	movk	w1, 0x464e, lsl 16
	cmp	w0, w1
	beq	.L1804
.L1806:
	ldr	x0, [x25, 3984]
	mov	x24, 0
.L1805:
	ldr	w2, [x0, x24, lsl 2]
	add	x27, x0, x24, lsl 2
	ldr	w1, [x22, x24, lsl 2]
	add	x28, x22, x24, lsl 2
	cmp	w2, w1
	beq	.L1808
	mov	x2, 512
	mov	w1, 0
	bl	memset
	ldr	w2, [x27]
	mov	w1, w21
	ldr	w3, [x28]
	mov	w4, w24
	adrp	x0, .LC131
	add	x0, x0, :lo12:.LC131
	bl	sftl_printk
	ldrh	w1, [x25, 10]
	mov	w0, 0
	ldr	x2, [x25, 3360]
	mul	w1, w1, w21
	blr	x2
.L1809:
	ldr	w21, [sp, 104]
	b	.L1789
.L1820:
	mov	w24, 6
	b	.L1790
.L1822:
	mov	x28, 0
	b	.L1792
.L1796:
	ldr	x8, [x25, 3368]
	add	x3, sp, 136
	ldr	w0, [sp, 96]
	mov	x2, x27
	add	w1, w0, w26
	mov	w0, 0
	blr	x8
	cmn	w0, #1
	beq	.L1800
	add	x27, x27, 2048
.L1797:
	add	w26, w26, 1
	b	.L1793
.L1798:
	ldr	x1, [x1, 40]
	mov	w24, 6
	cmp	x1, 0
	mov	w1, 9
	csel	w1, w24, w1, eq
	b	.L1843
.L1803:
	ldr	x6, [x25, 3376]
	add	x3, sp, 136
	mov	x2, x26
	add	w1, w27, w28
	mov	w0, 0
	blr	x6
	cmn	w0, #1
	beq	.L1806
	ldr	w0, [sp, 140]
	mov	w1, 61424
	cmp	w0, w1
	bne	.L1806
	add	x26, x26, 2048
.L1804:
	add	w28, w28, 1
	b	.L1802
.L1808:
	ldr	w1, [sp, 100]
	add	x24, x24, 1
	cmp	w1, w24
	bhi	.L1805
	add	w23, w23, 1
	cmp	w23, 5
	bls	.L1809
.L1810:
	ldr	x0, [x25, 3976]
	str	wzr, [x25, 3968]
	bl	kfree
	ldr	x0, [x25, 3984]
	bl	kfree
.L1779:
	mrs	x0, sp_el0
	ldr	x1, [sp, 200]
	ldr	x2, [x0, 952]
	subs	x1, x1, x2
	mov	x2, 0
	beq	.L1819
	bl	__stack_chk_fail
.L1781:
	mov	x21, x2
	cmp	w20, 64
	bne	.L1812
	mov	w0, 262144
	bl	ftl_malloc
	str	x0, [x25, 3976]
	mov	w0, 262144
	bl	ftl_malloc
	mov	x1, x0
	ldr	x0, [x25, 3976]
	str	x1, [x25, 3984]
	cbz	x0, .L1813
	cbz	x1, .L1813
	mov	w1, 1
	mov	w2, 262144
	str	w1, [x25, 3968]
	mov	w1, 0
	bl	ftl_memset
.L1814:
	sub	w1, w20, #64
	ldr	x0, [x25, 3976]
	cmp	w20, 63
	bhi	.L1816
	add	w19, w19, w1
	mov	w1, 64
	sub	w20, w1, w20
	ubfiz	x20, x20, 9, 25
	add	x21, x21, x20
.L1817:
	cmp	w23, 575
	bls	.L1818
	sub	w19, w19, w22
	add	w19, w19, 576
.L1818:
	lsl	w2, w19, 9
	mov	x1, x21
	bl	ftl_memcpy
	b	.L1844
.L1813:
	adrp	x1, .LANCHOR1
	add	x1, x1, :lo12:.LANCHOR1
	adrp	x0, .LC132
	add	x1, x1, 633
	add	x0, x0, :lo12:.LC132
	bl	sftl_printk
.L1812:
	ldr	w0, [x25, 3968]
	cbnz	w0, .L1814
	b	.L1844
.L1816:
	ubfiz	x1, x1, 9, 25
	add	x0, x0, x1
	b	.L1817
.L1819:
	mov	w0, w19
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 208
	ret
	.size	FtlWriteToIDB, .-FtlWriteToIDB
	.align	2
	.global	sftl_write
	.type	sftl_write, %function
sftl_write:
	stp	x29, x30, [sp, -48]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	w20, w1
	mov	w19, w0
	str	x21, [sp, 32]
	mov	x21, x2
	bl	FtlWriteToIDB
	mov	x3, x21
	mov	w2, w20
	mov	w1, w19
	mov	w0, 0
	bl	FtlWrite
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
	.size	sftl_write, .-sftl_write
	.align	2
	.global	rk_sftl_vendor_dev_ops_register
	.type	rk_sftl_vendor_dev_ops_register, %function
rk_sftl_vendor_dev_ops_register:
	adrp	x2, .LANCHOR0
	add	x2, x2, :lo12:.LANCHOR0
	ldr	x3, [x2, 3992]
	cbnz	x3, .L1849
	str	x0, [x2, 3992]
	mov	w0, 0
	str	x1, [x2, 4000]
.L1847:
	ret
.L1849:
	mov	w0, -1
	b	.L1847
	.size	rk_sftl_vendor_dev_ops_register, .-rk_sftl_vendor_dev_ops_register
	.align	2
	.global	rk_sftl_vendor_storage_init
	.type	rk_sftl_vendor_storage_init, %function
rk_sftl_vendor_storage_init:
	stp	x29, x30, [sp, -64]!
	mov	w0, 65536
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	adrp	x20, .LANCHOR0
	add	x20, x20, :lo12:.LANCHOR0
	stp	x21, x22, [sp, 32]
	stp	x23, x24, [sp, 48]
	bl	ftl_malloc
	str	x0, [x20, 4008]
	cbz	x0, .L1856
	mov	w24, 22084
	mov	w23, 0
	mov	w22, 0
	mov	w21, 0
	movk	w24, 0x524b, lsl 16
.L1854:
	ldr	x3, [x20, 3992]
	lsl	w0, w21, 7
	ldr	x2, [x20, 4008]
	mov	w1, 128
	blr	x3
	mov	w19, w0
	cbnz	w0, .L1852
	ldr	x3, [x20, 4008]
	ldr	w0, [x3]
	cmp	w0, w24
	bne	.L1853
	add	x0, x3, 49152
	ldr	w1, [x3, 4]
	ldr	w0, [x0, 16380]
	cmp	w0, w1
	bne	.L1853
	cmp	w0, w22
	bls	.L1853
	mov	w23, w21
	mov	w22, w0
.L1853:
	cbz	w21, .L1857
	cbz	w22, .L1855
	ldr	x4, [x20, 3992]
	mov	x2, x3
	lsl	w0, w23, 7
	mov	w1, 128
	blr	x4
	mov	w19, w0
	cbz	w0, .L1850
.L1852:
	ldr	x0, [x20, 4008]
	mov	w19, -1
	bl	kfree
	str	xzr, [x20, 4008]
	b	.L1850
.L1857:
	mov	w21, 1
	b	.L1854
.L1855:
	mov	x0, x3
	mov	x2, 65536
	mov	w1, 0
	bl	memset
	mov	x3, x0
	mov	x0, 22084
	movk	x0, 0x524b, lsl 16
	movk	x0, 0x1, lsl 32
	str	x0, [x3]
	add	x0, x3, 49152
	str	w21, [x0, 16380]
	mov	w0, -1032
	strh	w0, [x3, 14]
.L1850:
	mov	w0, w19
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x29, x30, [sp], 64
	ret
.L1856:
	mov	w19, -12
	b	.L1850
	.size	rk_sftl_vendor_storage_init, .-rk_sftl_vendor_storage_init
	.align	2
	.global	rk_sftl_vendor_read
	.type	rk_sftl_vendor_read, %function
rk_sftl_vendor_read:
	mov	w5, w0
	mov	x0, x1
	adrp	x1, .LANCHOR0+4008
	ldr	x4, [x1, #:lo12:.LANCHOR0+4008]
	cbz	x4, .L1863
	ldrh	w1, [x4, 10]
	add	x6, x4, 16
	mov	x3, 0
.L1861:
	cmp	w1, w3
	bhi	.L1862
.L1863:
	mov	w0, -1
	ret
.L1862:
	add	x7, x3, 1
	lsl	x3, x3, 3
	ldrh	w8, [x6, x3]
	cmp	w8, w5
	bne	.L1864
	stp	x29, x30, [sp, -32]!
	add	x3, x4, x3
	mov	x29, sp
	str	x19, [sp, 16]
	ldrh	w5, [x3, 20]
	ldrh	w1, [x3, 18]
	cmp	w5, w2
	add	x1, x1, 1024
	csel	w19, w5, w2, ls
	add	x1, x4, x1
	csel	w2, w5, w2, ls
	bl	memcpy
	mov	w0, w19
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
.L1864:
	mov	x3, x7
	b	.L1861
	.size	rk_sftl_vendor_read, .-rk_sftl_vendor_read
	.align	2
	.global	rk_sftl_vendor_write
	.type	rk_sftl_vendor_write, %function
rk_sftl_vendor_write:
	stp	x29, x30, [sp, -128]!
	mov	x29, sp
	stp	x21, x22, [sp, 32]
	adrp	x22, .LANCHOR0
	add	x22, x22, :lo12:.LANCHOR0
	stp	x19, x20, [sp, 16]
	ldr	x19, [x22, 4008]
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	cbz	x19, .L1884
	add	w21, w2, 63
	ldrh	w3, [x19, 10]
	ldrh	w23, [x19, 8]
	mov	w7, w0
	mov	x4, x1
	mov	w27, w2
	and	w21, w21, -64
	add	x0, x19, 16
	mov	x20, 0
.L1871:
	mov	w25, w20
	cmp	w3, w20
	bhi	.L1878
	ldrh	w1, [x19, 14]
	cmp	w1, w21
	bcc	.L1884
	add	x3, x19, w3, uxth 3
	and	w21, w21, 65535
	strh	w7, [x3, 16]
	ldrh	w0, [x19, 12]
	strh	w0, [x3, 18]
	strh	w27, [x3, 20]
	add	w2, w0, w21
	sub	w21, w1, w21
	and	x0, x0, 65535
	strh	w2, [x19, 12]
	add	x0, x0, 1024
	strh	w21, [x19, 14]
	uxtw	x2, w27
	mov	x1, x4
	add	x0, x19, x0
	bl	memcpy
	ldrh	w0, [x19, 10]
	add	w0, w0, 1
	strh	w0, [x19, 10]
	b	.L1886
.L1878:
	add	x1, x20, 1
	lsl	x20, x20, 3
	ldrh	w2, [x0, x20]
	cmp	w2, w7
	bne	.L1882
	add	x20, x19, x20
	add	x5, x19, 1024
	ldrh	w26, [x20, 20]
	add	w26, w26, 63
	and	w26, w26, -64
	cmp	w27, w26
	bls	.L1872
	ldrh	w0, [x19, 14]
	cmp	w0, w21
	bcc	.L1884
	ldrh	w6, [x20, 18]
	sub	w24, w3, #1
.L1873:
	and	w28, w6, 65535
	add	x20, x20, 8
	cmp	w24, w25
	bhi	.L1874
	add	x25, x19, w25, uxtw 3
	add	x0, x5, w28, uxth
	uxtw	x2, w27
	mov	x1, x4
	and	w21, w21, 65535
	strh	w7, [x25, 16]
	strh	w28, [x25, 18]
	add	w28, w21, w28
	strh	w27, [x25, 20]
	bl	memcpy
	strh	w28, [x19, 12]
	ldrh	w0, [x19, 14]
	sub	w21, w0, w21
	add	w26, w21, w26
	strh	w26, [x19, 14]
.L1886:
	ldr	w0, [x19, 4]
	add	x1, x19, 49152
	ldr	x3, [x22, 4000]
	add	w0, w0, 1
	str	w0, [x19, 4]
	mov	x2, x19
	str	w0, [x1, 16380]
	mov	w1, 128
	ldrh	w0, [x19, 8]
	add	w0, w0, 1
	and	w0, w0, 65535
	cmp	w0, 1
	csel	w0, w0, wzr, ls
	strh	w0, [x19, 8]
	lsl	w0, w23, 7
	blr	x3
	mov	w0, 0
.L1869:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 128
	ret
.L1874:
	add	w25, w25, 1
	str	x5, [sp, 104]
	stp	w6, w7, [sp, 112]
	add	x1, x19, w25, uxtw 3
	str	x4, [sp, 120]
	ldrh	w0, [x1, 16]
	strh	w0, [x20, 8]
	ldrh	w0, [x1, 20]
	strh	w28, [x20, 10]
	strh	w0, [x20, 12]
	add	x0, x5, w6, uxtw
	ldrh	w2, [x1, 20]
	ldrh	w1, [x1, 18]
	add	w2, w2, 63
	and	w28, w2, -64
	add	x1, x5, x1
	and	x2, x2, 131008
	bl	memcpy
	ldp	w6, w7, [sp, 112]
	ldr	x5, [sp, 104]
	add	w6, w6, w28
	ldr	x4, [sp, 120]
	b	.L1873
.L1872:
	ldrh	w0, [x20, 18]
	uxtw	x2, w27
	mov	x1, x4
	add	x0, x5, x0
	bl	memcpy
	strh	w27, [x20, 20]
	b	.L1886
.L1882:
	mov	x20, x1
	b	.L1871
.L1884:
	mov	w0, -1
	b	.L1869
	.size	rk_sftl_vendor_write, .-rk_sftl_vendor_write
	.section	.rodata.str1.1
.LC133:
	.string	"copy_from_user error %d %p %p\n"
	.text
	.align	2
	.global	rk_sftl_vendor_storage_ioctl
	.type	rk_sftl_vendor_storage_ioctl, %function
rk_sftl_vendor_storage_ioctl:
	stp	x29, x30, [sp, -48]!
	mov	w0, 4096
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x20, x2
	str	x21, [sp, 32]
	mov	w21, w1
	bl	ftl_malloc
	cbz	x0, .L1898
	mov	w1, 30209
	mov	x19, x0
	movk	w1, 0x4004, lsl 16
	cmp	w21, w1
	beq	.L1889
	add	w1, w1, 1
	cmp	w21, w1
	beq	.L1890
.L1908:
	mov	x20, -14
.L1891:
	mov	x0, x19
	bl	kfree
.L1887:
	mov	x0, x20
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	ldp	x29, x30, [sp], 48
	ret
.L1889:
	mov	x1, x20
	mov	x2, 8
	bl	_copy_from_user
	cbz	x0, .L1892
	mov	x2, x20
	mov	w1, 256
.L1909:
	adrp	x0, .LC133
	add	x0, x0, :lo12:.LC133
	bl	sftl_printk
	b	.L1908
.L1892:
	ldr	w1, [x19]
	mov	w0, 17745
	movk	w0, 0x5652, lsl 16
	cmp	w1, w0
	beq	.L1893
.L1894:
	mov	x20, -1
	b	.L1891
.L1893:
	ldrh	w2, [x19, 6]
	add	x1, x19, 8
	ldrh	w0, [x19, 4]
	bl	rk_sftl_vendor_read
	cmn	w0, #1
	beq	.L1894
	strh	w0, [x19, 6]
	and	x0, x0, 65535
	add	x4, x0, 8
	mov	x0, x20
	mov	x1, x4
	bl	access_ok
	cbz	w0, .L1908
#APP
// 217 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x0, x20, 36028797018963968

// 0 "" 2
#NO_APP
	mov	x20, 0
	mov	x2, x4
	mov	x1, x19
	bl	__arch_copy_to_user
	cbz	x0, .L1891
	b	.L1908
.L1890:
	mov	x1, x20
	mov	x2, 8
	bl	_copy_from_user
	cbz	x0, .L1896
	mov	x2, x20
	mov	w1, 276
	b	.L1909
.L1896:
	ldr	w1, [x19]
	mov	w0, 17745
	movk	w0, 0x5652, lsl 16
	cmp	w1, w0
	bne	.L1894
	ldrh	w2, [x19, 6]
	cmp	w2, 4087
	bhi	.L1894
	add	w2, w2, 8
	mov	x1, x20
	and	x2, x2, 65535
	mov	x0, x19
	bl	_copy_from_user
	cbz	x0, .L1897
	mov	x2, x20
	mov	w1, 283
	b	.L1909
.L1897:
	ldrh	w2, [x19, 6]
	add	x1, x19, 8
	ldrh	w0, [x19, 4]
	bl	rk_sftl_vendor_write
	sxtw	x20, w0
	b	.L1891
.L1898:
	mov	x20, -1
	b	.L1887
	.size	rk_sftl_vendor_storage_ioctl, .-rk_sftl_vendor_storage_ioctl
	.align	2
	.global	rk_sftl_vendor_register
	.type	rk_sftl_vendor_register, %function
rk_sftl_vendor_register:
	stp	x29, x30, [sp, -16]!
	adrp	x0, .LANCHOR2
	add	x0, x0, :lo12:.LANCHOR2
	mov	x29, sp
	add	x0, x0, 16
	bl	misc_register
	ldp	x29, x30, [sp], 16
	ret
	.size	rk_sftl_vendor_register, .-rk_sftl_vendor_register
	.section	.rodata.str1.1
.LC134:
	.string	"vendor_storage"
	.global	g_nand_ops
	.global	g_nand_phy_info
	.global	gc_ink_free_return_value
	.global	check_vpc_table
	.global	FtlUpdateVaildLpnCount
	.global	g_ect_tbl_power_up_flush
	.global	power_up_flag
	.global	gFtlInitStatus
	.global	DeviceCapacity
	.global	g_power_lost_recovery_flag
	.global	c_mlc_erase_count_value
	.global	g_recovery_ppa_tbl
	.global	g_recovery_page_min_ver
	.global	g_recovery_page_num
	.global	sftl_nand_check_spare_buf
	.global	sftl_temp_buf
	.global	sftl_nand_check_buf
	.global	g_cur_erase_blk
	.global	g_gc_skip_write_count
	.global	g_gc_head_data_block_count
	.global	g_gc_head_data_block
	.global	g_ftl_nand_free_count
	.global	g_in_swl_replace
	.global	g_in_gc_progress
	.global	g_max_erase_count
	.global	g_totle_sys_slc_erase_count
	.global	g_totle_slc_erase_count
	.global	g_min_erase_count
	.global	g_totle_avg_erase_count
	.global	g_totle_mlc_erase_count
	.global	g_totle_l2p_write_count
	.global	g_totle_cache_write_count
	.global	g_tmp_data_superblock_id
	.global	g_totle_read_page_count
	.global	g_totle_discard_page_count
	.global	g_totle_read_sector
	.global	g_totle_write_sector
	.global	g_totle_write_page_count
	.global	g_totle_gc_page_count
	.global	g_gc_blk_index
	.global	g_gc_merge_free_blk_threshold
	.global	g_gc_free_blk_threshold
	.global	g_gc_bad_block_temp_tbl
	.global	g_gc_bad_block_gc_index
	.global	g_gc_bad_block_temp_num
	.global	g_gc_next_blk_1
	.global	g_gc_next_blk
	.global	g_gc_cur_blk_max_valid_pages
	.global	g_gc_cur_blk_valid_pages
	.global	g_gc_page_offset
	.global	g_gc_blk_num
	.global	p_gc_blk_tbl
	.global	p_gc_page_info
	.global	g_sys_ext_data
	.global	g_sys_save_data
	.global	gp_last_act_superblock
	.global	g_gc_superblock
	.global	g_gc_temp_superblock
	.global	g_buffer_superblock
	.global	g_active_superblock
	.global	g_num_data_superblocks
	.global	g_num_free_superblocks
	.global	p_data_block_list_tail
	.global	p_data_block_list_head
	.global	p_free_data_block_list_head
	.global	p_data_block_list_table
	.global	g_l2p_last_update_region_id
	.global	p_l2p_map_buf
	.global	p_l2p_ram_map
	.global	g_totle_vendor_block
	.global	p_vendor_region_ppn_table
	.global	p_vendor_block_ver_table
	.global	p_vendor_block_valid_page_count
	.global	p_vendor_block_table
	.global	g_totle_map_block
	.global	p_map_region_ppn_table
	.global	p_map_block_ver_table
	.global	p_map_block_valid_page_count
	.global	p_map_block_table
	.global	p_valid_page_count_check_table
	.global	p_valid_page_count_table
	.global	g_totle_swl_count
	.global	p_swl_mul_table
	.global	p_erase_count_table
	.global	g_ect_tbl_info_size
	.global	gp_ect_tbl_info
	.global	g_gc_num_req
	.global	c_gc_page_buf_num
	.global	gp_gc_page_buf_info
	.global	p_gc_data_buf
	.global	p_gc_spare_buf
	.global	p_io_spare_buf
	.global	p_io_data_buf_1
	.global	p_io_data_buf_0
	.global	p_sys_spare_buf
	.global	p_vendor_data_buf
	.global	p_sys_data_buf_1
	.global	p_sys_data_buf
	.global	p_plane_order_table
	.global	req_gc_dst
	.global	req_gc
	.global	req_erase
	.global	req_prgm
	.global	req_read
	.global	req_sys
	.global	gVendorBlkInfo
	.global	gL2pMapInfo
	.global	gSysFreeQueue
	.global	gSysInfo
	.global	gBbtInfo
	.global	g_MaxLbn
	.global	g_VaildLpn
	.global	g_MaxLpn
	.global	g_MaxLbaSector
	.global	g_GlobalDataVersion
	.global	g_GlobalSysVersion
	.global	ftl_gc_temp_power_lost_recovery_flag
	.global	c_ftl_nand_max_data_blks
	.global	c_ftl_nand_data_op_blks_per_plane
	.global	c_ftl_nand_data_blks_per_plane
	.global	c_ftl_nand_max_sys_blks
	.global	c_ftl_nand_init_sys_blks_per_plane
	.global	c_ftl_nand_sys_blks_per_plane
	.global	c_ftl_vendor_part_size
	.global	c_ftl_nand_max_vendor_blks
	.global	c_ftl_nand_max_map_blks
	.global	c_ftl_nand_map_blks_per_plane
	.global	c_ftl_nand_vendor_region_num
	.global	c_ftl_nand_l2pmap_ram_region_num
	.global	c_ftl_nand_map_region_num
	.global	c_ftl_nand_totle_phy_blks
	.global	c_ftl_nand_reserved_blks
	.global	c_ftl_nand_byte_pre_oob
	.global	c_ftl_nand_byte_pre_page
	.global	c_ftl_nand_sec_pre_page_shift
	.global	c_ftl_nand_sec_pre_page
	.global	c_ftl_nand_page_pre_super_blk
	.global	c_ftl_nand_page_pre_slc_blk
	.global	c_ftl_nand_page_pre_blk
	.global	c_ftl_nand_bbm_buf_size
	.global	c_ftl_nand_ext_blk_pre_plane
	.global	c_ftl_nand_blk_pre_plane
	.global	c_ftl_nand_planes_num
	.global	c_ftl_nand_blks_per_die_shift
	.global	c_ftl_nand_blks_per_die
	.global	c_ftl_nand_planes_per_die
	.global	c_ftl_nand_die_num
	.global	c_ftl_nand_type
	.section	.rodata
	.align	3
	.set	.LANCHOR1,. + 0
	.type	__func__.30, %object
	.size	__func__.30, 17
__func__.30:
	.string	"INSERT_DATA_LIST"
	.type	__func__.31, %object
	.size	__func__.31, 17
__func__.31:
	.string	"INSERT_FREE_LIST"
	.type	__func__.29, %object
	.size	__func__.29, 17
__func__.29:
	.string	"List_remove_node"
	.type	__func__.28, %object
	.size	__func__.28, 22
__func__.28:
	.string	"List_update_data_list"
	.type	__func__.22, %object
	.size	__func__.22, 22
__func__.22:
	.string	"select_l2p_ram_region"
	.type	__func__.14, %object
	.size	__func__.14, 16
__func__.14:
	.string	"make_superblock"
	.type	__func__.8, %object
	.size	__func__.8, 19
__func__.8:
	.string	"get_new_active_ppa"
	.type	__func__.1, %object
	.size	__func__.1, 17
__func__.1:
	.string	"FlashEraseBlocks"
	.type	__func__.26, %object
	.size	__func__.26, 26
__func__.26:
	.string	"ftl_map_blk_alloc_new_blk"
	.type	__func__.37, %object
	.size	__func__.37, 11
__func__.37:
	.string	"FtlMemInit"
	.type	__func__.34, %object
	.size	__func__.34, 14
__func__.34:
	.string	"FtlBbt2Bitmap"
	.type	__func__.13, %object
	.size	__func__.13, 18
__func__.13:
	.string	"SupperBlkListInit"
	.type	__func__.3, %object
	.size	__func__.3, 15
__func__.3:
	.string	"FlashReadPages"
	.type	__func__.19, %object
	.size	__func__.19, 14
__func__.19:
	.string	"FtlScanSysBlk"
	.type	__func__.32, %object
	.size	__func__.32, 11
__func__.32:
	.string	"FtlLoadBbt"
	.type	__func__.2, %object
	.size	__func__.2, 15
__func__.2:
	.string	"FlashProgPages"
	.type	__func__.10, %object
	.size	__func__.10, 25
__func__.10:
	.string	"allocate_data_superblock"
	.type	__func__.7, %object
	.size	__func__.7, 16
__func__.7:
	.string	"update_vpc_list"
	.type	__func__.6, %object
	.size	__func__.6, 20
__func__.6:
	.string	"decrement_vpc_count"
	.type	__func__.23, %object
	.size	__func__.23, 16
__func__.23:
	.string	"FtlMapWritePage"
	.type	__func__.27, %object
	.size	__func__.27, 16
__func__.27:
	.string	"load_l2p_region"
	.type	__func__.25, %object
	.size	__func__.25, 15
__func__.25:
	.string	"ftl_map_blk_gc"
	.type	__func__.24, %object
	.size	__func__.24, 31
__func__.24:
	.string	"Ftl_write_map_blk_to_last_page"
	.type	__func__.21, %object
	.size	__func__.21, 9
__func__.21:
	.string	"log2phys"
	.type	__func__.16, %object
	.size	__func__.16, 16
__func__.16:
	.string	"FtlReUsePrevPpa"
	.type	__func__.15, %object
	.size	__func__.15, 22
__func__.15:
	.string	"FtlRecoverySuperblock"
	.type	__func__.11, %object
	.size	__func__.11, 14
__func__.11:
	.string	"ftl_check_vpc"
	.type	__func__.20, %object
	.size	__func__.20, 15
__func__.20:
	.string	"FtlVpcTblFlush"
	.type	__func__.12, %object
	.size	__func__.12, 21
__func__.12:
	.string	"FtlVpcCheckAndModify"
	.type	__func__.9, %object
	.size	__func__.9, 29
__func__.9:
	.string	"allocate_new_data_superblock"
	.type	__func__.36, %object
	.size	__func__.36, 13
__func__.36:
	.string	"FtlProgPages"
	.type	__func__.5, %object
	.size	__func__.5, 19
__func__.5:
	.string	"FtlGcFreeTempBlock"
	.type	__func__.4, %object
	.size	__func__.4, 23
__func__.4:
	.string	"rk_ftl_garbage_collect"
	.type	__func__.35, %object
	.size	__func__.35, 9
__func__.35:
	.string	"FtlWrite"
	.type	__func__.18, %object
	.size	__func__.18, 15
__func__.18:
	.string	"FtlLoadSysInfo"
	.type	__func__.17, %object
	.size	__func__.17, 18
__func__.17:
	.string	"FtlMapTblRecovery"
	.type	__func__.0, %object
	.size	__func__.0, 14
__func__.0:
	.string	"FtlWriteToIDB"
	.zero	1
	.type	rk_sftl_vendor_storage_fops, %object
	.size	rk_sftl_vendor_storage_fops, 280
rk_sftl_vendor_storage_fops:
	.zero	80
	.xword	rk_sftl_vendor_storage_ioctl
	.xword	rk_sftl_vendor_storage_ioctl
	.zero	184
	.data
	.align	3
	.set	.LANCHOR2,. + 0
	.type	gFtlInitStatus, %object
	.size	gFtlInitStatus, 4
gFtlInitStatus:
	.word	-1
	.type	ftl_gc_temp_block_bops_scan_page_addr, %object
	.size	ftl_gc_temp_block_bops_scan_page_addr, 2
ftl_gc_temp_block_bops_scan_page_addr:
	.hword	-1
	.zero	2
	.type	power_up_flag, %object
	.size	power_up_flag, 4
power_up_flag:
	.word	1
	.zero	4
	.type	rkflash_vender_storage_dev, %object
	.size	rkflash_vender_storage_dev, 80
rkflash_vender_storage_dev:
	.word	255
	.zero	4
	.xword	.LC134
	.xword	rk_sftl_vendor_storage_fops
	.zero	56
	.bss
	.align	3
	.set	.LANCHOR0,. + 0
	.type	g_nand_phy_info, %object
	.size	g_nand_phy_info, 24
g_nand_phy_info:
	.zero	24
	.type	g_active_superblock, %object
	.size	g_active_superblock, 48
g_active_superblock:
	.zero	48
	.type	p_valid_page_count_table, %object
	.size	p_valid_page_count_table, 8
p_valid_page_count_table:
	.zero	8
	.type	g_buffer_superblock, %object
	.size	g_buffer_superblock, 48
g_buffer_superblock:
	.zero	48
	.type	g_gc_temp_superblock, %object
	.size	g_gc_temp_superblock, 48
g_gc_temp_superblock:
	.zero	48
	.type	g_gc_superblock, %object
	.size	g_gc_superblock, 48
g_gc_superblock:
	.zero	48
	.type	g_gc_next_blk_1, %object
	.size	g_gc_next_blk_1, 2
g_gc_next_blk_1:
	.zero	2
	.type	g_gc_next_blk, %object
	.size	g_gc_next_blk, 2
g_gc_next_blk:
	.zero	2
	.type	g_num_free_superblocks, %object
	.size	g_num_free_superblocks, 2
g_num_free_superblocks:
	.zero	2
	.zero	2
	.type	c_ftl_nand_sys_blks_per_plane, %object
	.size	c_ftl_nand_sys_blks_per_plane, 4
c_ftl_nand_sys_blks_per_plane:
	.zero	4
	.type	c_ftl_nand_planes_num, %object
	.size	c_ftl_nand_planes_num, 2
c_ftl_nand_planes_num:
	.zero	2
	.zero	2
	.type	c_ftl_nand_max_sys_blks, %object
	.size	c_ftl_nand_max_sys_blks, 4
c_ftl_nand_max_sys_blks:
	.zero	4
	.type	c_ftl_nand_data_blks_per_plane, %object
	.size	c_ftl_nand_data_blks_per_plane, 2
c_ftl_nand_data_blks_per_plane:
	.zero	2
	.type	c_ftl_nand_blk_pre_plane, %object
	.size	c_ftl_nand_blk_pre_plane, 2
c_ftl_nand_blk_pre_plane:
	.zero	2
	.type	c_ftl_nand_max_data_blks, %object
	.size	c_ftl_nand_max_data_blks, 4
c_ftl_nand_max_data_blks:
	.zero	4
	.type	c_ftl_nand_totle_phy_blks, %object
	.size	c_ftl_nand_totle_phy_blks, 4
c_ftl_nand_totle_phy_blks:
	.zero	4
	.type	c_ftl_nand_type, %object
	.size	c_ftl_nand_type, 2
c_ftl_nand_type:
	.zero	2
	.type	c_ftl_nand_die_num, %object
	.size	c_ftl_nand_die_num, 2
c_ftl_nand_die_num:
	.zero	2
	.type	c_ftl_nand_planes_per_die, %object
	.size	c_ftl_nand_planes_per_die, 2
c_ftl_nand_planes_per_die:
	.zero	2
	.type	c_ftl_nand_sec_pre_page, %object
	.size	c_ftl_nand_sec_pre_page, 2
c_ftl_nand_sec_pre_page:
	.zero	2
	.type	p_plane_order_table, %object
	.size	p_plane_order_table, 32
p_plane_order_table:
	.zero	32
	.type	c_mlc_erase_count_value, %object
	.size	c_mlc_erase_count_value, 2
c_mlc_erase_count_value:
	.zero	2
	.type	c_ftl_nand_ext_blk_pre_plane, %object
	.size	c_ftl_nand_ext_blk_pre_plane, 2
c_ftl_nand_ext_blk_pre_plane:
	.zero	2
	.type	c_ftl_vendor_part_size, %object
	.size	c_ftl_vendor_part_size, 2
c_ftl_vendor_part_size:
	.zero	2
	.type	c_ftl_nand_blks_per_die, %object
	.size	c_ftl_nand_blks_per_die, 2
c_ftl_nand_blks_per_die:
	.zero	2
	.type	c_ftl_nand_blks_per_die_shift, %object
	.size	c_ftl_nand_blks_per_die_shift, 2
c_ftl_nand_blks_per_die_shift:
	.zero	2
	.type	c_ftl_nand_page_pre_blk, %object
	.size	c_ftl_nand_page_pre_blk, 2
c_ftl_nand_page_pre_blk:
	.zero	2
	.type	c_ftl_nand_page_pre_slc_blk, %object
	.size	c_ftl_nand_page_pre_slc_blk, 2
c_ftl_nand_page_pre_slc_blk:
	.zero	2
	.type	c_ftl_nand_page_pre_super_blk, %object
	.size	c_ftl_nand_page_pre_super_blk, 2
c_ftl_nand_page_pre_super_blk:
	.zero	2
	.type	c_ftl_nand_sec_pre_page_shift, %object
	.size	c_ftl_nand_sec_pre_page_shift, 2
c_ftl_nand_sec_pre_page_shift:
	.zero	2
	.type	c_ftl_nand_byte_pre_page, %object
	.size	c_ftl_nand_byte_pre_page, 2
c_ftl_nand_byte_pre_page:
	.zero	2
	.type	c_ftl_nand_byte_pre_oob, %object
	.size	c_ftl_nand_byte_pre_oob, 2
c_ftl_nand_byte_pre_oob:
	.zero	2
	.type	c_ftl_nand_reserved_blks, %object
	.size	c_ftl_nand_reserved_blks, 2
c_ftl_nand_reserved_blks:
	.zero	2
	.type	DeviceCapacity, %object
	.size	DeviceCapacity, 4
DeviceCapacity:
	.zero	4
	.type	c_ftl_nand_max_vendor_blks, %object
	.size	c_ftl_nand_max_vendor_blks, 2
c_ftl_nand_max_vendor_blks:
	.zero	2
	.type	c_ftl_nand_vendor_region_num, %object
	.size	c_ftl_nand_vendor_region_num, 2
c_ftl_nand_vendor_region_num:
	.zero	2
	.type	c_ftl_nand_map_blks_per_plane, %object
	.size	c_ftl_nand_map_blks_per_plane, 2
c_ftl_nand_map_blks_per_plane:
	.zero	2
	.zero	2
	.type	c_ftl_nand_max_map_blks, %object
	.size	c_ftl_nand_max_map_blks, 4
c_ftl_nand_max_map_blks:
	.zero	4
	.type	c_ftl_nand_init_sys_blks_per_plane, %object
	.size	c_ftl_nand_init_sys_blks_per_plane, 4
c_ftl_nand_init_sys_blks_per_plane:
	.zero	4
	.type	c_ftl_nand_map_region_num, %object
	.size	c_ftl_nand_map_region_num, 2
c_ftl_nand_map_region_num:
	.zero	2
	.type	c_ftl_nand_l2pmap_ram_region_num, %object
	.size	c_ftl_nand_l2pmap_ram_region_num, 2
c_ftl_nand_l2pmap_ram_region_num:
	.zero	2
	.type	g_MaxLbaSector, %object
	.size	g_MaxLbaSector, 4
g_MaxLbaSector:
	.zero	4
	.type	g_totle_vendor_block, %object
	.size	g_totle_vendor_block, 2
g_totle_vendor_block:
	.zero	2
	.zero	2
	.type	p_vendor_block_table, %object
	.size	p_vendor_block_table, 8
p_vendor_block_table:
	.zero	8
	.type	gBbtInfo, %object
	.size	gBbtInfo, 96
gBbtInfo:
	.zero	96
	.type	gSysFreeQueue, %object
	.size	gSysFreeQueue, 2056
gSysFreeQueue:
	.zero	2056
	.type	g_sys_save_data, %object
	.size	g_sys_save_data, 48
g_sys_save_data:
	.zero	48
	.type	p_data_block_list_table, %object
	.size	p_data_block_list_table, 8
p_data_block_list_table:
	.zero	8
	.type	p_data_block_list_head, %object
	.size	p_data_block_list_head, 8
p_data_block_list_head:
	.zero	8
	.type	p_data_block_list_tail, %object
	.size	p_data_block_list_tail, 8
p_data_block_list_tail:
	.zero	8
	.type	g_num_data_superblocks, %object
	.size	g_num_data_superblocks, 2
g_num_data_superblocks:
	.zero	2
	.zero	6
	.type	p_free_data_block_list_head, %object
	.size	p_free_data_block_list_head, 8
p_free_data_block_list_head:
	.zero	8
	.type	p_erase_count_table, %object
	.size	p_erase_count_table, 8
p_erase_count_table:
	.zero	8
	.type	p_l2p_ram_map, %object
	.size	p_l2p_ram_map, 8
p_l2p_ram_map:
	.zero	8
	.type	g_l2p_last_update_region_id, %object
	.size	g_l2p_last_update_region_id, 2
g_l2p_last_update_region_id:
	.zero	2
	.type	FtlUpdateVaildLpnCount, %object
	.size	FtlUpdateVaildLpnCount, 2
FtlUpdateVaildLpnCount:
	.zero	2
	.type	g_VaildLpn, %object
	.size	g_VaildLpn, 4
g_VaildLpn:
	.zero	4
	.type	g_MaxLpn, %object
	.size	g_MaxLpn, 4
g_MaxLpn:
	.zero	4
	.type	g_totle_read_page_count, %object
	.size	g_totle_read_page_count, 4
g_totle_read_page_count:
	.zero	4
	.type	g_totle_discard_page_count, %object
	.size	g_totle_discard_page_count, 4
g_totle_discard_page_count:
	.zero	4
	.type	g_totle_write_page_count, %object
	.size	g_totle_write_page_count, 4
g_totle_write_page_count:
	.zero	4
	.type	g_totle_cache_write_count, %object
	.size	g_totle_cache_write_count, 4
g_totle_cache_write_count:
	.zero	4
	.type	g_totle_l2p_write_count, %object
	.size	g_totle_l2p_write_count, 4
g_totle_l2p_write_count:
	.zero	4
	.type	g_totle_gc_page_count, %object
	.size	g_totle_gc_page_count, 4
g_totle_gc_page_count:
	.zero	4
	.type	g_totle_write_sector, %object
	.size	g_totle_write_sector, 4
g_totle_write_sector:
	.zero	4
	.type	g_totle_read_sector, %object
	.size	g_totle_read_sector, 4
g_totle_read_sector:
	.zero	4
	.type	g_GlobalSysVersion, %object
	.size	g_GlobalSysVersion, 4
g_GlobalSysVersion:
	.zero	4
	.type	g_GlobalDataVersion, %object
	.size	g_GlobalDataVersion, 4
g_GlobalDataVersion:
	.zero	4
	.type	g_totle_mlc_erase_count, %object
	.size	g_totle_mlc_erase_count, 4
g_totle_mlc_erase_count:
	.zero	4
	.type	g_totle_slc_erase_count, %object
	.size	g_totle_slc_erase_count, 4
g_totle_slc_erase_count:
	.zero	4
	.type	g_totle_avg_erase_count, %object
	.size	g_totle_avg_erase_count, 4
g_totle_avg_erase_count:
	.zero	4
	.type	g_totle_sys_slc_erase_count, %object
	.size	g_totle_sys_slc_erase_count, 4
g_totle_sys_slc_erase_count:
	.zero	4
	.type	g_max_erase_count, %object
	.size	g_max_erase_count, 4
g_max_erase_count:
	.zero	4
	.type	g_min_erase_count, %object
	.size	g_min_erase_count, 4
g_min_erase_count:
	.zero	4
	.type	c_ftl_nand_data_op_blks_per_plane, %object
	.size	c_ftl_nand_data_op_blks_per_plane, 2
c_ftl_nand_data_op_blks_per_plane:
	.zero	2
	.zero	2
	.type	gSysInfo, %object
	.size	gSysInfo, 16
gSysInfo:
	.zero	16
	.type	g_sys_ext_data, %object
	.size	g_sys_ext_data, 512
g_sys_ext_data:
	.zero	512
	.type	g_gc_free_blk_threshold, %object
	.size	g_gc_free_blk_threshold, 2
g_gc_free_blk_threshold:
	.zero	2
	.type	g_gc_merge_free_blk_threshold, %object
	.size	g_gc_merge_free_blk_threshold, 2
g_gc_merge_free_blk_threshold:
	.zero	2
	.type	g_gc_skip_write_count, %object
	.size	g_gc_skip_write_count, 4
g_gc_skip_write_count:
	.zero	4
	.type	g_gc_blk_index, %object
	.size	g_gc_blk_index, 2
g_gc_blk_index:
	.zero	2
	.zero	2
	.type	g_in_swl_replace, %object
	.size	g_in_swl_replace, 4
g_in_swl_replace:
	.zero	4
	.type	g_gc_num_req, %object
	.size	g_gc_num_req, 4
g_gc_num_req:
	.zero	4
	.zero	4
	.type	gp_gc_page_buf_info, %object
	.size	gp_gc_page_buf_info, 8
gp_gc_page_buf_info:
	.zero	8
	.type	p_gc_data_buf, %object
	.size	p_gc_data_buf, 8
p_gc_data_buf:
	.zero	8
	.type	p_gc_spare_buf, %object
	.size	p_gc_spare_buf, 8
p_gc_spare_buf:
	.zero	8
	.type	req_gc, %object
	.size	req_gc, 8
req_gc:
	.zero	8
	.type	c_gc_page_buf_num, %object
	.size	c_gc_page_buf_num, 4
c_gc_page_buf_num:
	.zero	4
	.type	g_gc_blk_num, %object
	.size	g_gc_blk_num, 2
g_gc_blk_num:
	.zero	2
	.zero	2
	.type	p_gc_blk_tbl, %object
	.size	p_gc_blk_tbl, 8
p_gc_blk_tbl:
	.zero	8
	.type	g_gc_page_offset, %object
	.size	g_gc_page_offset, 2
g_gc_page_offset:
	.zero	2
	.zero	6
	.type	p_gc_page_info, %object
	.size	p_gc_page_info, 8
p_gc_page_info:
	.zero	8
	.type	g_gc_bad_block_temp_num, %object
	.size	g_gc_bad_block_temp_num, 2
g_gc_bad_block_temp_num:
	.zero	2
	.type	g_gc_bad_block_temp_tbl, %object
	.size	g_gc_bad_block_temp_tbl, 34
g_gc_bad_block_temp_tbl:
	.zero	34
	.type	g_gc_bad_block_gc_index, %object
	.size	g_gc_bad_block_gc_index, 2
g_gc_bad_block_gc_index:
	.zero	2
	.zero	2
	.type	g_nand_ops, %object
	.size	g_nand_ops, 48
g_nand_ops:
	.zero	48
	.type	req_erase, %object
	.size	req_erase, 8
req_erase:
	.zero	8
	.type	g_in_gc_progress, %object
	.size	g_in_gc_progress, 4
g_in_gc_progress:
	.zero	4
	.type	g_gc_head_data_block, %object
	.size	g_gc_head_data_block, 4
g_gc_head_data_block:
	.zero	4
	.type	g_gc_head_data_block_count, %object
	.size	g_gc_head_data_block_count, 4
g_gc_head_data_block_count:
	.zero	4
	.type	g_cur_erase_blk, %object
	.size	g_cur_erase_blk, 4
g_cur_erase_blk:
	.zero	4
	.type	req_read, %object
	.size	req_read, 8
req_read:
	.zero	8
	.type	req_gc_dst, %object
	.size	req_gc_dst, 8
req_gc_dst:
	.zero	8
	.type	req_prgm, %object
	.size	req_prgm, 8
req_prgm:
	.zero	8
	.type	p_sys_data_buf, %object
	.size	p_sys_data_buf, 8
p_sys_data_buf:
	.zero	8
	.type	p_sys_data_buf_1, %object
	.size	p_sys_data_buf_1, 8
p_sys_data_buf_1:
	.zero	8
	.type	p_vendor_data_buf, %object
	.size	p_vendor_data_buf, 8
p_vendor_data_buf:
	.zero	8
	.type	p_io_data_buf_0, %object
	.size	p_io_data_buf_0, 8
p_io_data_buf_0:
	.zero	8
	.type	p_io_data_buf_1, %object
	.size	p_io_data_buf_1, 8
p_io_data_buf_1:
	.zero	8
	.type	sftl_nand_check_buf, %object
	.size	sftl_nand_check_buf, 8
sftl_nand_check_buf:
	.zero	8
	.type	sftl_temp_buf, %object
	.size	sftl_temp_buf, 8
sftl_temp_buf:
	.zero	8
	.type	sftl_nand_check_spare_buf, %object
	.size	sftl_nand_check_spare_buf, 8
sftl_nand_check_spare_buf:
	.zero	8
	.type	p_sys_spare_buf, %object
	.size	p_sys_spare_buf, 8
p_sys_spare_buf:
	.zero	8
	.type	p_io_spare_buf, %object
	.size	p_io_spare_buf, 8
p_io_spare_buf:
	.zero	8
	.type	g_ect_tbl_info_size, %object
	.size	g_ect_tbl_info_size, 2
g_ect_tbl_info_size:
	.zero	2
	.zero	6
	.type	p_swl_mul_table, %object
	.size	p_swl_mul_table, 8
p_swl_mul_table:
	.zero	8
	.type	gp_ect_tbl_info, %object
	.size	gp_ect_tbl_info, 8
gp_ect_tbl_info:
	.zero	8
	.type	p_valid_page_count_check_table, %object
	.size	p_valid_page_count_check_table, 8
p_valid_page_count_check_table:
	.zero	8
	.type	p_map_block_table, %object
	.size	p_map_block_table, 8
p_map_block_table:
	.zero	8
	.type	p_map_block_valid_page_count, %object
	.size	p_map_block_valid_page_count, 8
p_map_block_valid_page_count:
	.zero	8
	.type	p_vendor_block_valid_page_count, %object
	.size	p_vendor_block_valid_page_count, 8
p_vendor_block_valid_page_count:
	.zero	8
	.type	p_vendor_block_ver_table, %object
	.size	p_vendor_block_ver_table, 8
p_vendor_block_ver_table:
	.zero	8
	.type	p_vendor_region_ppn_table, %object
	.size	p_vendor_region_ppn_table, 8
p_vendor_region_ppn_table:
	.zero	8
	.type	p_map_region_ppn_table, %object
	.size	p_map_region_ppn_table, 8
p_map_region_ppn_table:
	.zero	8
	.type	p_map_block_ver_table, %object
	.size	p_map_block_ver_table, 8
p_map_block_ver_table:
	.zero	8
	.type	p_l2p_map_buf, %object
	.size	p_l2p_map_buf, 8
p_l2p_map_buf:
	.zero	8
	.type	c_ftl_nand_bbm_buf_size, %object
	.size	c_ftl_nand_bbm_buf_size, 2
c_ftl_nand_bbm_buf_size:
	.zero	2
	.zero	6
	.type	gL2pMapInfo, %object
	.size	gL2pMapInfo, 64
gL2pMapInfo:
	.zero	64
	.type	g_totle_map_block, %object
	.size	g_totle_map_block, 2
g_totle_map_block:
	.zero	2
	.type	g_tmp_data_superblock_id, %object
	.size	g_tmp_data_superblock_id, 2
g_tmp_data_superblock_id:
	.zero	2
	.type	g_totle_swl_count, %object
	.size	g_totle_swl_count, 4
g_totle_swl_count:
	.zero	4
	.type	ftl_gc_temp_power_lost_recovery_flag, %object
	.size	ftl_gc_temp_power_lost_recovery_flag, 4
ftl_gc_temp_power_lost_recovery_flag:
	.zero	4
	.type	g_recovery_page_min_ver, %object
	.size	g_recovery_page_min_ver, 4
g_recovery_page_min_ver:
	.zero	4
	.type	req_sys, %object
	.size	req_sys, 32
req_sys:
	.zero	32
	.type	g_power_lost_recovery_flag, %object
	.size	g_power_lost_recovery_flag, 2
g_power_lost_recovery_flag:
	.zero	2
	.zero	2
	.type	g_recovery_page_num, %object
	.size	g_recovery_page_num, 4
g_recovery_page_num:
	.zero	4
	.type	g_recovery_ppa_tbl, %object
	.size	g_recovery_ppa_tbl, 128
g_recovery_ppa_tbl:
	.zero	128
	.type	gVendorBlkInfo, %object
	.size	gVendorBlkInfo, 64
gVendorBlkInfo:
	.zero	64
	.type	g_ect_tbl_power_up_flush, %object
	.size	g_ect_tbl_power_up_flush, 2
g_ect_tbl_power_up_flush:
	.zero	2
	.zero	2
	.type	gc_discard_updated, %object
	.size	gc_discard_updated, 4
gc_discard_updated:
	.zero	4
	.type	gc_ink_free_return_value, %object
	.size	gc_ink_free_return_value, 2
gc_ink_free_return_value:
	.zero	2
	.type	g_gc_cur_blk_valid_pages, %object
	.size	g_gc_cur_blk_valid_pages, 2
g_gc_cur_blk_valid_pages:
	.zero	2
	.type	g_gc_cur_blk_max_valid_pages, %object
	.size	g_gc_cur_blk_max_valid_pages, 2
g_gc_cur_blk_max_valid_pages:
	.zero	2
	.zero	2
	.type	g_ftl_nand_free_count, %object
	.size	g_ftl_nand_free_count, 4
g_ftl_nand_free_count:
	.zero	4
	.type	g_MaxLbn, %object
	.size	g_MaxLbn, 4
g_MaxLbn:
	.zero	4
	.type	idb_need_write_back, %object
	.size	idb_need_write_back, 4
idb_need_write_back:
	.zero	4
	.zero	4
	.type	idb_buf, %object
	.size	idb_buf, 8
idb_buf:
	.zero	8
	.type	gp_flash_check_buf, %object
	.size	gp_flash_check_buf, 8
gp_flash_check_buf:
	.zero	8
	.type	_flash_read, %object
	.size	_flash_read, 8
_flash_read:
	.zero	8
	.type	_flash_write, %object
	.size	_flash_write, 8
_flash_write:
	.zero	8
	.type	g_vendor, %object
	.size	g_vendor, 8
g_vendor:
	.zero	8
	.type	check_vpc_table, %object
	.size	check_vpc_table, 16384
check_vpc_table:
	.zero	16384
	.type	gp_last_act_superblock, %object
	.size	gp_last_act_superblock, 8
gp_last_act_superblock:
	.zero	8
	.ident	"GCC: (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 10.3.1 20210621"
	.section	.note.GNU-stack,"",@progbits
