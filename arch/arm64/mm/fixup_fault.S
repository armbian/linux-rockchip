/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 */

	.arch armv8-a
	.file	"fixup_fault.c"
	.text
	.align	2
	.p2align 3,,7
	.type	rk_align_rd.part.0, %function
rk_align_rd.part.0:
	sub	sp, sp, #48
	mrs	x3, sp_el0
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x5, sp_el0
// 0 "" 2
#NO_APP
	stp	x29, x30, [sp, 16]
	add	x29, sp, 16
	stp	x19, x20, [sp, 32]
	mov	w19, w1
	mov	x20, x2
	ldr	x1, [x3, 1384]
	str	x1, [sp, 8]
	mov	x1, 0
	mov	x3, 549755813888
	mov	x1, x0
	mov	x0, 4294963200
	sxtw	x2, w19
	ldr	x4, [x5]
	tst	w4, 4194304
	csel	x0, x0, x3, ne
	cmp	x1, x0
	bcs	.L3
	mov	x4, sp
	mov	x6, 549755813887
	mov	w7, 0
	.p2align 3,,7
.L7:
	ldr	w0, [x5, 44]
	tbz	x0, 21, .L23
.L4:
	sbfx	x0, x1, 0, 56
	and	x0, x0, x1
.L5:
	cmp	x0, x6
	bls	.L24
.L18:
	mov	w0, 1
.L1:
	mrs	x1, sp_el0
	ldr	x3, [sp, 8]
	ldr	x2, [x1, 1384]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L25
	ldp	x29, x30, [sp, 16]
	ldp	x19, x20, [sp, 32]
	add	sp, sp, 48
	ret
	.p2align 2,,3
.L24:
#APP
// 169 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x3, x1, 36028797018963968

// 0 "" 2
#NO_APP
	mov	w0, w7
#APP
// 26 "arch/arm64/mm/fixup_fault.c" 1
	1:	ldtrb	w3, [x3]
2:
	.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	.equ	.L__gpr_num_x\num, \num
	.equ	.L__gpr_num_w\num, \num
	.endr
	.equ	.L__gpr_num_xzr, 31
	.equ	.L__gpr_num_wzr, 31
.pushsection	__ex_table, "a"
.align		2
.long		((1b) - .)
.long		((2b) - .)
.short		(2)
.short		((((.L__gpr_num_w0) << 0) | ((.L__gpr_num_w3) << 5)))
.popsection

// 0 "" 2
#NO_APP
	cbnz	w0, .L18
	strb	w3, [x4], 1
	add	x1, x1, 1
	subs	x2, x2, #1
	bne	.L7
	cmp	w19, 4
	beq	.L9
.L26:
	bgt	.L10
	cmp	w19, 1
	beq	.L11
	cmp	w19, 2
	bne	.L18
	ldrh	w1, [sp]
	mov	w0, 0
	str	x1, [x20]
	b	.L1
	.p2align 2,,3
.L23:
	ldr	x3, [x5]
	mov	x0, x1
	tst	w3, 67108864
	beq	.L5
	b	.L4
	.p2align 2,,3
.L10:
	cmp	w19, 8
	bne	.L18
	ldr	x1, [sp]
	mov	w0, 0
	str	x1, [x20]
	b	.L1
	.p2align 2,,3
.L3:
	mov	x0, sp
	bl	__memcpy_fromio
	cmp	w19, 4
	bne	.L26
.L9:
	ldr	w1, [sp]
	mov	w0, 0
	str	x1, [x20]
	b	.L1
	.p2align 2,,3
.L11:
	ldrb	w1, [sp]
	mov	w0, 0
	str	x1, [x20]
	b	.L1
.L25:
	bl	__stack_chk_fail
	.size	rk_align_rd.part.0, .-rk_align_rd.part.0
	.align	2
	.p2align 3,,7
	.type	rk_align_wr, %function
rk_align_wr:
	sub	sp, sp, #32
	mrs	x3, sp_el0
	stp	x29, x30, [sp, 16]
	add	x29, sp, 16
	ldr	x4, [x3, 1384]
	str	x4, [sp, 8]
	mov	x4, 0
	cmp	w1, 4
	beq	.L28
	bgt	.L29
	cmp	w1, 1
	beq	.L30
	cmp	w1, 2
	bne	.L44
	strh	w2, [sp]
.L34:
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x4, sp_el0
// 0 "" 2
#NO_APP
	ldr	x6, [x4]
	sxtw	x2, w1
	mov	x3, 4294963200
	mov	x5, 549755813888
	tst	w6, 4194304
	csel	x1, x3, x5, ne
	cmp	x0, x1
	bcs	.L35
	mov	x5, sp
	mov	x6, 549755813887
	sub	x5, x5, x0
	b	.L38
	.p2align 2,,3
.L52:
#APP
// 169 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x3, x0, 36028797018963968

// 0 "" 2
#NO_APP
	ldrb	w1, [x5, x0]
#APP
// 37 "arch/arm64/mm/fixup_fault.c" 1
	1:	sttrb	w1, [x3]
2:
	.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	.equ	.L__gpr_num_x\num, \num
	.equ	.L__gpr_num_w\num, \num
	.endr
	.equ	.L__gpr_num_xzr, 31
	.equ	.L__gpr_num_wzr, 31
.pushsection	__ex_table, "a"
.align		2
.long		((1b) - .)
.long		((.L44) - .)
.short		(2)
.short		((((.L__gpr_num_wzr) << 0) | ((.L__gpr_num_wzr) << 5)))
.popsection

// 0 "" 2
#NO_APP
	add	x0, x0, 1
	subs	x2, x2, #1
	beq	.L50
.L38:
	ldr	w1, [x4, 44]
	tbz	x1, 21, .L51
.L36:
	sbfx	x1, x0, 0, 56
	and	x1, x1, x0
.L37:
	cmp	x1, x6
	bls	.L52
.L44:
.L32:
	mov	w0, 1
.L27:
	mrs	x1, sp_el0
	ldr	x3, [sp, 8]
	ldr	x2, [x1, 1384]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L53
	ldp	x29, x30, [sp, 16]
	add	sp, sp, 32
	ret
	.p2align 2,,3
.L29:
	cmp	w1, 8
	bne	.L44
	str	x2, [sp]
	b	.L34
	.p2align 2,,3
.L35:
	mov	x1, sp
	bl	__memcpy_toio
.L50:
	mov	w0, 0
	b	.L27
	.p2align 2,,3
.L51:
	ldr	x3, [x4]
	mov	x1, x0
	tst	w3, 67108864
	beq	.L37
	b	.L36
	.p2align 2,,3
.L30:
	strb	w2, [sp]
	b	.L34
	.p2align 2,,3
.L28:
	str	w2, [sp]
	b	.L34
.L53:
	bl	__stack_chk_fail
	.size	rk_align_wr, .-rk_align_wr
	.align	2
	.p2align 3,,7
	.global	alignment_fixup_helper
	.type	alignment_fixup_helper, %function
alignment_fixup_helper:
	sub	sp, sp, #160
	mrs	x1, sp_el0
	stp	x29, x30, [sp, 64]
	add	x29, sp, 64
	stp	x19, x20, [sp, 80]
	mov	x19, x2
	stp	x21, x22, [sp, 96]
	mov	x21, x0
	ldr	x2, [x1, 1384]
	str	x2, [sp, 56]
	mov	x2, 0
	ldr	x1, [x19, 264]
	tst	x1, 15
	bne	.L55
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x2, sp_el0
// 0 "" 2
#NO_APP
	ldr	x3, [x2]
	mov	x1, 4294963200
	mov	x0, 549755813888
	tst	w3, 4194304
	csel	x1, x1, x0, ne
	cmp	x21, x1
	bcc	.L57
.L61:
	mov	w0, 1
.L54:
	mrs	x1, sp_el0
	ldr	x3, [sp, 56]
	ldr	x2, [x1, 1384]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L326
	ldp	x29, x30, [sp, 64]
	ldp	x19, x20, [sp, 80]
	ldp	x21, x22, [sp, 96]
	add	sp, sp, 160
	ret
	.p2align 2,,3
.L57:
	ldr	w3, [x2, 44]
	ldr	x0, [x19, 256]
	mov	x1, x0
	tbz	x3, 21, .L327
.L59:
	sbfx	x2, x0, 0, 56
	and	x0, x0, x2
.L60:
	mov	x2, 549755813884
	cmp	x0, x2
	bhi	.L61
#APP
// 169 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x0, x1, 36028797018963968

// 0 "" 2
#NO_APP
	mov	w1, 0
#APP
// 713 "arch/arm64/mm/fixup_fault.c" 1
	1:	ldtr	w0, [x0]
2:
	.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	.equ	.L__gpr_num_x\num, \num
	.equ	.L__gpr_num_w\num, \num
	.endr
	.equ	.L__gpr_num_xzr, 31
	.equ	.L__gpr_num_wzr, 31
.pushsection	__ex_table, "a"
.align		2
.long		((1b) - .)
.long		((2b) - .)
.short		(2)
.short		((((.L__gpr_num_w1) << 0) | ((.L__gpr_num_w0) << 5)))
.popsection

// 0 "" 2
#NO_APP
	cbnz	w1, .L61
	mov	w20, w0
	str	w0, [sp, 20]
	and	w1, w20, 469762048
	mov	w0, 335544320
	cmp	w1, w0
	bne	.L63
.L328:
	mov	w0, 29728
	and	w20, w20, -32
	movk	w0, 0xd50b, lsl 16
	cmp	w20, w0
	bne	.L61
#APP
// 122 "arch/arm64/mm/fixup_fault.c" 1
		.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	.equ	.L__gpr_num_x\num, \num
	.equ	.L__gpr_num_w\num, \num
	.endr
	.equ	.L__gpr_num_xzr, 31
	.equ	.L__gpr_num_wzr, 31
	.macro	mrs_s, rt, sreg
.inst (0xd5200000|(\sreg)|(.L__gpr_num_\rt))
		.endm
	mrs_s x0, (((3) << 19) | ((3) << 16) | ((0) << 12) | ((0) << 8) | ((7) << 5))
	.purgem	mrs_s

// 0 "" 2
#NO_APP
	and	w0, w0, 15
	mov	w20, 1
	add	w0, w0, 2
	mov	x1, 4294963200
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x2, sp_el0
// 0 "" 2
#NO_APP
	ldr	x3, [x2]
	lsl	w20, w20, w0
	neg	w0, w20
	mov	x2, 549755813888
	sxtw	x0, w0
	tst	w3, 4194304
	and	x21, x0, x21
	csel	x0, x1, x2, ne
	cmp	x21, x0
	bcs	.L65
	.p2align 3,,7
.L66:
	mov	x0, x21
	mov	x2, 0
	mov	w1, 1
	bl	rk_align_wr
	cbnz	w0, .L61
	subs	w20, w20, #1
	bne	.L66
.L67:
.L58:
	ldr	x1, [x19, 256]
	mov	w0, 0
	add	x1, x1, 4
	str	x1, [x19, 256]
	b	.L54
	.p2align 2,,3
.L55:
	ldr	x0, [x19, 256]
	add	x1, sp, 20
	bl	aarch64_insn_read
	cbnz	w0, .L61
	ldr	w20, [sp, 20]
	mov	w0, 335544320
	and	w1, w20, 469762048
	cmp	w1, w0
	beq	.L328
.L63:
	mov	w0, 167772160
	and	w0, w20, w0
	mov	w1, 134217728
	cmp	w0, w1
	bne	.L61
	ubfx	x21, x20, 28, 2
	uxtw	x0, w20
	ubfx	x22, x20, 26, 1
	cmp	w21, 2
	beq	.L329
	cmp	w21, 3
	bne	.L61
	stp	x23, x24, [sp, 112]
	stp	x25, x26, [sp, 128]
	ubfx	x25, x0, 10, 2
	tbnz	x0, 24, .L117
	cmp	x25, 2
	ubfx	x0, x0, 16, 6
	cset	w2, ne
	ands	w0, w0, 32
	cset	w1, eq
	ands	w23, w2, w1
	bne	.L117
	cmp	w0, 0
	ccmp	x25, 2, 0, ne
	beq	.L330
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	b	.L61
	.p2align 2,,3
.L327:
	ldr	x2, [x2]
	tst	w2, 67108864
	beq	.L60
	b	.L59
	.p2align 2,,3
.L329:
	stp	x23, x24, [sp, 112]
	mov	w1, w20
	lsr	w23, w20, 30
	stp	x25, x26, [sp, 128]
	ubfx	x24, x20, 22, 1
	and	w26, w20, 8388608
	stp	x27, x28, [sp, 144]
	and	w25, w20, 16777216
	mov	w0, 1
	cbnz	x22, .L69
	bl	aarch64_insn_decode_register
	mov	w28, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w27, w0
	mov	w1, w20
	mov	w0, w21
	bl	aarch64_insn_decode_register
	mov	w21, w0
	mov	w1, w20
	mov	w0, 7
	bl	aarch64_insn_decode_immediate
	eor	w1, w24, 1
	and	w4, w23, 1
	mov	w3, w1
	tst	w23, w1
	bne	.L189
	cmp	w23, 3
	beq	.L189
	cbz	w26, .L71
	cmp	w28, w27
	ccmp	w28, w21, 4, ne
	ccmp	w28, 31, 4, eq
	bne	.L189
.L71:
	cmp	w27, w21
	cset	w1, eq
	tst	w1, w24
	bne	.L189
	asr	w5, w23, 1
	mov	w23, 8
	add	w5, w5, 2
	and	w2, w28, 536870911
	lsl	w23, w23, w5
	asr	w20, w23, 3
	cmp	w2, 32
	beq	.L72
	bhi	.L73
	cmp	w2, 30
	bhi	.L331
	ldr	x24, [x19, x2, lsl 3]
.L76:
	sbfx	x0, x0, 0, 7
	cmp	w25, 0
	lsl	x0, x0, x5
	str	x0, [sp]
	add	x0, x24, x0
	csel	x24, x0, x24, ne
	cbz	w3, .L79
	mov	x2, 0
	cmp	w27, 31
	beq	.L80
	ldr	x2, [x19, w27, sxtw 3]
.L80:
	str	x2, [sp, 40]
	cmp	w21, 31
	beq	.L81
	ldr	x22, [x19, w21, sxtw 3]
.L81:
	mov	w1, w20
	mov	x0, x24
	str	x22, [sp, 48]
	bl	rk_align_wr
	cbnz	w0, .L189
	mov	x2, x22
	mov	w1, w20
	add	x0, x24, w20, sxtw
	bl	rk_align_wr
	cbnz	w0, .L189
.L83:
	cbz	w26, .L317
	cbnz	w25, .L90
	ldr	x0, [sp]
	add	x24, x24, x0
.L90:
	cmp	w28, 31
	beq	.L332
	str	x24, [x19, w28, sxtw 3]
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
.L340:
	cbnz	x25, .L189
	tbz	x24, 0, .L317
	.p2align 3,,7
.L189:
	mov	w0, 1
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L54
	.p2align 2,,3
.L65:
	sxtw	x2, w20
	mov	x0, x21
	mov	w1, 0
	bl	__memset_io
	b	.L67
	.p2align 2,,3
.L69:
	bl	aarch64_insn_decode_register
	mov	w27, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w22, w0
	mov	w1, w20
	mov	w0, w21
	bl	aarch64_insn_decode_register
	mov	w21, w0
	mov	w1, w20
	mov	w0, 7
	bl	aarch64_insn_decode_immediate
	cmp	w23, 3
	beq	.L189
	cmp	w22, w21
	cset	w1, eq
	tst	w1, w24
	bne	.L189
	add	w23, w23, 2
	mov	w3, 8
	and	w1, w27, 536870911
	lsl	w3, w3, w23
	asr	w20, w3, 3
	cmp	w1, 32
	beq	.L93
	bhi	.L94
	cmp	w1, 30
	bhi	.L333
	ldr	x28, [x19, x1, lsl 3]
.L97:
	sbfx	x0, x0, 0, 7
	cmp	w25, 0
	lsl	x0, x0, x23
	str	x0, [sp]
	add	x0, x28, x0
	csel	x28, x0, x28, ne
	cbnz	w24, .L100
	mov	x2, 0
	cmp	w22, 31
	bhi	.L101
	mov	w0, w22
	mov	w1, 0
	bl	rk_align_get_data_asm
	mov	x2, x0
.L101:
	str	x2, [sp, 24]
	mov	x24, 0
	cmp	w21, 31
	bhi	.L102
	mov	w0, w21
	mov	w1, 0
	str	x2, [sp, 8]
	bl	rk_align_get_data_asm
	ldr	x2, [sp, 8]
	mov	x24, x0
.L102:
	str	x24, [sp, 40]
	cmp	w23, 4
	bne	.L103
	mov	x23, 0
	cmp	w22, 31
	bhi	.L104
	mov	w0, w22
	mov	w1, 1
	str	x2, [sp, 8]
	bl	rk_align_get_data_asm
	ldr	x2, [sp, 8]
	mov	x23, x0
.L104:
	str	x23, [sp, 32]
	mov	x22, 0
	cmp	w21, 31
	bhi	.L105
	mov	w0, w21
	mov	w1, 1
	str	x2, [sp, 8]
	bl	rk_align_get_data_asm
	ldr	x2, [sp, 8]
	mov	x22, x0
.L105:
	mov	x0, x28
	mov	w1, 8
	str	x22, [sp, 48]
	bl	rk_align_wr
	cbnz	w0, .L189
	mov	x2, x23
	add	x0, x28, 8
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L189
	mov	x2, x24
	add	x0, x28, 16
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L189
	mov	x2, x22
	add	x0, x28, 24
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L189
.L107:
	cbz	w26, .L317
	cbnz	w25, .L115
	ldr	x0, [sp]
	add	x28, x28, x0
.L115:
	cmp	w27, 31
	beq	.L334
	str	x28, [x19, w27, sxtw 3]
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
	.p2align 2,,3
.L117:
	and	w23, w20, 33553408
	stp	x27, x28, [sp, 144]
	and	w23, w23, -16775169
	lsr	w27, w20, 30
	lsr	w24, w20, 22
	cmp	w23, 1024
	cbnz	x22, .L119
	eor	x2, x20, 2048
	mov	w1, w20
	mov	w0, 1
	ubfx	x2, x2, 11, 1
	csel	w21, w2, wzr, eq
	bl	aarch64_insn_decode_register
	mov	w26, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w28, w0
	mov	w1, w20
	tbnz	x20, 24, .L121
	mov	w0, 6
	bl	aarch64_insn_decode_immediate
	sbfx	x3, x0, 0, 9
.L122:
	tbnz	x24, 1, .L123
	mvn	w0, w24
	cmp	w27, 3
	mov	w24, 64
	mov	w1, 32
	and	w0, w0, 1
	csel	w24, w24, w1, eq
	mov	w4, 0
.L124:
	cmp	w26, w28
	ccmp	w26, 31, 4, eq
	bne	.L189
	and	w1, w26, 536870911
	cmp	w1, 32
	beq	.L128
	bhi	.L129
	cmp	w1, 30
	bhi	.L335
	ldr	x20, [x19, x1, lsl 3]
.L132:
	mov	w25, 8
	cmp	w21, 0
	add	x2, x20, x3
	lsl	w25, w25, w27
	csel	x20, x2, x20, eq
	asr	w1, w25, 3
	cbz	w0, .L135
	cmp	w28, 31
	beq	.L136
	ldr	x22, [x19, w28, sxtw 3]
.L136:
	mov	x2, x22
	mov	x0, x20
	str	x3, [sp]
	str	x22, [sp, 48]
	bl	rk_align_wr
	ldr	x3, [sp]
	cbnz	w0, .L189
.L137:
	cmp	w23, 1024
	beq	.L336
.L317:
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
.L119:
	eor	x0, x20, 2048
	lsl	w21, w24, 1
	mov	w1, w20
	and	w21, w21, 4
	ubfx	x0, x0, 11, 1
	orr	w21, w21, w27
	csel	w22, w0, wzr, eq
	mov	w0, 1
	bl	aarch64_insn_decode_register
	mov	w26, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w25, w0
	cmp	w21, 4
	bhi	.L189
	mov	w1, w20
	tbnz	x20, 24, .L147
	mov	w0, 6
	bl	aarch64_insn_decode_immediate
	sbfx	x27, x0, 0, 9
.L148:
	mov	w28, 8
	and	w0, w26, 536870911
	lsl	w28, w28, w21
	cmp	w0, 32
	beq	.L149
	bhi	.L150
	cmp	w0, 30
	bhi	.L337
	ldr	x20, [x19, x0, lsl 3]
.L153:
	cmp	w22, 0
	add	x0, x20, x27
	csel	x20, x0, x20, eq
	tbnz	x24, 0, .L156
	cmp	w25, 31
	bhi	.L157
	mov	w0, w25
	mov	w1, 0
	bl	rk_align_get_data_asm
	str	x0, [sp, 40]
	mov	x24, x0
	cmp	w21, 4
	bne	.L158
	mov	w0, w25
	mov	w1, 1
	bl	rk_align_get_data_asm
	mov	x21, x0
.L214:
	mov	x2, x24
	mov	x0, x20
	mov	w1, 8
	str	x21, [sp, 48]
	bl	rk_align_wr
	cbnz	w0, .L189
	mov	x2, x21
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L189
.L160:
	cmp	w23, 1024
	bne	.L317
	cbz	w22, .L165
	add	x20, x20, x27
.L165:
	cmp	w26, 31
	beq	.L338
	str	x20, [x19, w26, sxtw 3]
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
.L333:
	cmp	w1, 31
	bne	.L319
	ldr	x28, [x19, 248]
	b	.L97
.L94:
	cmp	w1, 33
	bne	.L319
	ldr	x28, [x19, 264]
	b	.L97
.L330:
	stp	x27, x28, [sp, 144]
	ubfx	x1, x20, 13, 1
	cmp	w1, 0
	lsr	w26, w20, 30
	lsr	w25, w20, 22
	ubfx	x0, x20, 12, 1
	mov	w27, 32
	mov	w1, 64
	cbnz	x22, .L167
	csel	w27, w27, w1, eq
	cmp	w0, 0
	mov	w1, w20
	csel	w28, w26, wzr, ne
	mov	w0, 1
	bl	aarch64_insn_decode_register
	mov	w24, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w1, w20
	mov	w20, w0
	mov	w0, w21
	bl	aarch64_insn_decode_register
	tbnz	x25, 1, .L170
	mvn	w2, w25
	and	w23, w2, 1
.L171:
	cmp	w24, w20
	ccmp	w24, 31, 4, eq
	bne	.L189
	cmp	w0, 31
	beq	.L230
	ldr	x1, [x19, w0, sxtw 3]
.L175:
	cmp	w27, 32
	bne	.L176
	and	x1, x1, 4294967295
.L176:
	and	w0, w24, 536870911
	lsl	x1, x1, x28
	cmp	w0, 32
	beq	.L177
	bhi	.L178
	cmp	w0, 30
	bhi	.L339
	ldr	x0, [x19, x0, lsl 3]
.L181:
	mov	w2, 8
	lsl	w2, w2, w26
	add	x0, x1, x0
	asr	w1, w2, 3
	cbz	w23, .L183
	cmp	w20, 31
	beq	.L184
	ldr	x22, [x19, w20, sxtw 3]
.L184:
	mov	x2, x22
	bl	rk_align_wr
	cbz	w0, .L317
	b	.L189
	.p2align 2,,3
.L331:
	cmp	w2, 31
	bne	.L318
	ldr	x24, [x19, 248]
	b	.L76
.L73:
	cmp	w2, 33
	bne	.L318
	ldr	x24, [x19, 264]
	b	.L76
.L337:
	cmp	w0, 31
	bne	.L321
	ldr	x20, [x19, 248]
	b	.L153
.L150:
	cmp	w0, 33
	bne	.L321
	ldr	x20, [x19, 264]
	b	.L153
.L319:
	mov	x28, 0
	b	.L97
.L123:
	cmp	w27, 3
	beq	.L340
	and	w24, w24, 1
	cmp	w27, 2
	beq	.L341
	cmp	w24, 0
	mov	w0, 32
	mov	w24, 64
	mov	w4, 1
	csel	w24, w24, w0, eq
	mov	w0, 0
	b	.L124
.L121:
	mov	w0, 5
	bl	aarch64_insn_decode_immediate
	lsl	x3, x0, x27
	b	.L122
.L100:
	cmp	w23, 4
	beq	.L342
	cmp	w20, 4
	beq	.L111
	cmp	w20, 8
	bne	.L189
	add	x2, sp, 24
	mov	w1, w20
	mov	x0, x28
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	add	x0, x28, 8
.L210:
	mov	w1, w20
	add	x2, sp, 40
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	str	xzr, [sp, 32]
	str	xzr, [sp, 48]
.L110:
	cmp	w22, 31
	bhi	.L113
	ldr	x2, [sp, 24]
	mov	w0, w22
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 32]
	mov	w0, w22
	mov	w1, 1
	bl	rk_align_set_data_asm
.L113:
	ldr	x2, [sp, 40]
	cmp	w21, 31
	bhi	.L107
	mov	w0, w21
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 48]
	mov	w0, w21
	mov	w1, 1
	bl	rk_align_set_data_asm
	b	.L107
.L167:
	csel	w27, w27, w1, eq
	lsl	w24, w25, 1
	cmp	w0, 0
	and	w24, w24, 4
	orr	w24, w24, w26
	mov	w1, w20
	csel	w26, w24, wzr, ne
	mov	w0, 1
	bl	aarch64_insn_decode_register
	mov	w22, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w23, w0
	mov	w1, w20
	mov	w0, w21
	bl	aarch64_insn_decode_register
	tbz	x25, 1, .L189
	cmp	w22, w23
	ccmp	w22, 31, 4, eq
	bne	.L189
	cmp	w0, 31
	beq	.L233
	ldr	x0, [x19, w0, sxtw 3]
.L190:
	cmp	w27, 32
	bne	.L191
	and	x0, x0, 4294967295
.L191:
	and	w1, w22, 536870911
	lsl	x0, x0, x26
	cmp	w1, 32
	beq	.L192
	bhi	.L193
	cmp	w1, 30
	bhi	.L343
	ldr	x2, [x19, x1, lsl 3]
.L196:
	mov	w21, 8
	add	x20, x0, x2
	lsl	w22, w21, w24
	tbnz	x25, 0, .L198
	cmp	w23, 31
	bhi	.L199
	mov	w0, w23
	mov	w1, 0
	bl	rk_align_get_data_asm
	str	x0, [sp, 40]
	mov	x21, x0
	cmp	w24, 4
	bne	.L200
	mov	w0, w23
	mov	w1, 1
	bl	rk_align_get_data_asm
	mov	x22, x0
.L215:
	mov	x2, x21
	mov	x0, x20
	mov	w1, 8
	str	x22, [sp, 48]
	bl	rk_align_wr
	cbnz	w0, .L189
	mov	x2, x22
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_wr
	cbz	w0, .L317
	b	.L189
	.p2align 2,,3
.L318:
	mov	x24, 0
	b	.L76
.L335:
	cmp	w1, 31
	bne	.L320
	ldr	x20, [x19, 248]
	b	.L132
.L129:
	cmp	w1, 33
	bne	.L320
	ldr	x20, [x19, 264]
	b	.L132
.L321:
	mov	x20, 0
	b	.L153
.L147:
	mov	w0, 5
	bl	aarch64_insn_decode_immediate
	lsl	x27, x0, x21
	b	.L148
.L156:
	cmp	w21, 4
	beq	.L344
	asr	w1, w28, 3
	mov	x0, 278
	lsr	x0, x0, x1
	tbz	x0, 0, .L189
	add	x2, sp, 40
	mov	x0, x20
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	str	xzr, [sp, 48]
.L163:
	ldr	x2, [sp, 40]
	cmp	w25, 31
	bhi	.L160
	mov	w0, w25
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 48]
	mov	w0, w25
	mov	w1, 1
	bl	rk_align_set_data_asm
	b	.L160
.L103:
	mov	w1, w20
	mov	x0, x28
	bl	rk_align_wr
	cbnz	w0, .L189
	mov	x2, x24
	mov	w1, w20
	add	x0, x28, w20, sxtw
	bl	rk_align_wr
	cbz	w0, .L107
	b	.L189
.L79:
	cmp	w20, 4
	beq	.L84
	cmp	w20, 8
	bne	.L189
	add	x2, sp, 40
	mov	w1, w20
	mov	x0, x24
	str	w4, [sp, 8]
	bl	rk_align_rd.part.0
	ldr	w4, [sp, 8]
	cbnz	w0, .L189
	add	x0, x24, 8
.L206:
	mov	w1, w20
	add	x2, sp, 48
	str	w4, [sp, 8]
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	ldr	w4, [sp, 8]
	cbnz	w4, .L301
	ldr	x0, [sp, 48]
.L86:
	cmp	w27, 31
	beq	.L87
	ldr	x1, [sp, 40]
	str	x1, [x19, w27, sxtw 3]
.L87:
	cmp	w21, 31
	beq	.L83
	str	x0, [x19, w21, sxtw 3]
	b	.L83
.L170:
	cmp	w26, 3
	beq	.L345
	cmp	w26, 2
	bne	.L171
	tbz	x25, 0, .L171
	b	.L189
.L339:
	cmp	w0, 31
	bne	.L322
	ldr	x0, [x19, 248]
	b	.L181
.L178:
	cmp	w0, 33
	bne	.L322
	ldr	x0, [x19, 264]
	b	.L181
.L320:
	mov	x20, 0
	b	.L132
.L93:
	ldr	x28, [x19, 256]
	b	.L97
.L343:
	cmp	w1, 31
	bne	.L323
	ldr	x2, [x19, 248]
	b	.L196
.L193:
	cmp	w1, 33
	bne	.L323
	ldr	x2, [x19, 264]
	b	.L196
.L342:
	add	x2, sp, 24
	mov	x0, x28
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	add	x2, sp, 32
	add	x0, x28, 8
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	add	x2, sp, 40
	add	x0, x28, 16
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	add	x2, sp, 48
	add	x0, x28, 24
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbz	w0, .L110
	b	.L189
.L157:
	str	xzr, [sp, 40]
	mov	x24, 0
	cmp	w21, 4
	bne	.L158
	mov	x21, 0
	b	.L214
.L72:
	ldr	x24, [x19, 256]
	b	.L76
.L158:
	mov	x2, x24
	asr	w1, w28, 3
	mov	x0, x20
	bl	rk_align_wr
	cbz	w0, .L160
	b	.L189
.L135:
	mov	x0, 278
	lsr	x0, x0, x1
	tbz	x0, 0, .L189
	add	x2, sp, 48
	mov	x0, x20
	str	x3, [sp]
	str	w4, [sp, 8]
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	ldr	w4, [sp, 8]
	ldr	x3, [sp]
	cbz	w4, .L139
	sub	w25, w25, #1
	mov	w4, 31
	ldr	x1, [sp, 48]
	sub	w4, w4, w25, sxtb
	mov	w2, 63
	cmp	w24, 32
	sub	w2, w2, w25, sxtb
	lsl	w0, w1, w4
	asr	w0, w0, w4
	sxtw	x0, w0
	lsl	x1, x1, x2
	asr	x1, x1, x2
	csel	x0, x0, x1, eq
	str	x0, [sp, 48]
.L139:
	ldr	x0, [sp, 48]
	cmp	w28, 31
	beq	.L137
	str	x0, [x19, w28, sxtw 3]
	b	.L137
.L149:
	ldr	x20, [x19, 256]
	b	.L153
.L322:
	mov	x0, 0
	b	.L181
.L198:
	cmp	w24, 4
	beq	.L346
	asr	w1, w22, 3
	sub	w2, w1, #4
	sub	w0, w1, #1
	tst	w2, -5
	ccmp	w0, 1, 0, ne
	bhi	.L189
	mov	x0, x20
	add	x2, sp, 40
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	str	xzr, [sp, 48]
.L204:
	ldr	x2, [sp, 40]
	cmp	w23, 31
	bhi	.L317
	mov	w0, w23
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 48]
	mov	w0, w23
	mov	w1, 1
	bl	rk_align_set_data_asm
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
.L341:
	cbnz	w24, .L189
	mov	w24, 64
	mov	w4, 1
	mov	w0, 0
	b	.L124
.L344:
	add	x2, sp, 40
	mov	x0, x20
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	add	x2, sp, 48
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbz	w0, .L163
	b	.L189
.L111:
	add	x2, sp, 24
	mov	w1, w20
	mov	x0, x28
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	add	x0, x28, 4
	b	.L210
.L326:
	stp	x23, x24, [sp, 112]
	stp	x25, x26, [sp, 128]
	stp	x27, x28, [sp, 144]
	bl	__stack_chk_fail
.L323:
	mov	x2, 0
	b	.L196
.L338:
	str	x20, [x19, 248]
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
.L84:
	add	x2, sp, 40
	mov	w1, w20
	mov	x0, x24
	str	w4, [sp, 8]
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	ldr	w4, [sp, 8]
	add	x0, x24, 4
	b	.L206
.L233:
	mov	x0, 0
	b	.L190
.L183:
	mov	x2, 278
	lsr	x2, x2, x1
	tbz	x2, 0, .L189
	add	x2, sp, 48
	bl	rk_align_rd.part.0
	cbz	w0, .L317
	b	.L189
.L128:
	ldr	x20, [x19, 256]
	b	.L132
.L336:
	cbz	w21, .L165
	add	x20, x20, x3
	b	.L165
.L345:
	tbz	x25, 0, .L317
	b	.L189
.L230:
	mov	x1, 0
	b	.L175
.L177:
	ldr	x0, [x19, 256]
	b	.L181
.L332:
	str	x24, [x19, 248]
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
.L192:
	ldr	x2, [x19, 256]
	b	.L196
.L301:
	ldp	x2, x0, [sp, 40]
	mov	w1, 64
	sub	w1, w1, w23
	lsl	x2, x2, x1
	lsl	x0, x0, x1
	asr	x2, x2, x1
	asr	x0, x0, x1
	stp	x2, x0, [sp, 40]
	b	.L86
.L200:
	mov	x2, x21
	asr	w1, w22, 3
	mov	x0, x20
	bl	rk_align_wr
	cbz	w0, .L317
	b	.L189
.L199:
	str	xzr, [sp, 40]
	mov	x21, 0
	cmp	w24, 4
	bne	.L200
	mov	x22, 0
	b	.L215
.L346:
	add	x2, sp, 40
	mov	w1, w21
	mov	x0, x20
	bl	rk_align_rd.part.0
	cbnz	w0, .L189
	mov	w1, w21
	add	x0, x20, 8
	add	x2, sp, 48
	bl	rk_align_rd.part.0
	cbz	w0, .L204
	b	.L189
.L334:
	str	x28, [x19, 248]
	ldp	x23, x24, [sp, 112]
	ldp	x25, x26, [sp, 128]
	ldp	x27, x28, [sp, 144]
	b	.L67
	.size	alignment_fixup_helper, .-alignment_fixup_helper
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
