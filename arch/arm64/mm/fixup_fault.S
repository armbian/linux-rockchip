/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

	.arch armv8-a
	.file	"fixup_fault.c"
	.text
	.align	2
	.type	rk_align_rd.part.0, %function
rk_align_rd.part.0:
	stp	x29, x30, [sp, -48]!
	mrs	x3, sp_el0
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x6, sp_el0
// 0 "" 2
#NO_APP
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	w19, w1
	mov	x20, x2
	ldr	x1, [x3, 1456]
	str	x1, [sp, 40]
	mov	x1, 0
	mov	x3, 549755813888
	mov	x1, x0
	mov	x0, 4294963200
	sxtw	x2, w19
	ldr	x4, [x6]
	tst	w4, 4194304
	csel	x0, x0, x3, ne
	cmp	x1, x0
	bcs	.L3
	add	x4, sp, 32
	mov	x7, 549755813887
	mov	w8, 0
	.p2align 3,,7
.L7:
	ldr	w0, [x6, 44]
	tbz	x0, 21, .L23
.L4:
	sbfx	x0, x1, 0, 56
	and	x0, x0, x1
	cmp	x0, x7
	bhi	.L6
.L25:
#APP
// 171 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x3, x1, 36028797018963968

// 0 "" 2
#NO_APP
	mov	w0, w8
#APP
// 25 "arch/arm64/mm/fixup_fault.c" 1
	1:	ldtrb	w5, [x3]
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
.short		((((.L__gpr_num_w0) << 0) | ((.L__gpr_num_w5) << 5)))
.popsection

// 0 "" 2
#NO_APP
	strb	w5, [x4], 1
	cbnz	w0, .L6
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
	bne	.L6
	ldrh	w1, [sp, 32]
	mov	w0, 0
	str	x1, [x20]
	.p2align 3,,7
.L1:
	mrs	x1, sp_el0
	ldr	x3, [sp, 40]
	ldr	x2, [x1, 1456]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L24
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 48
	ret
	.p2align 2,,3
.L23:
	ldr	x3, [x6]
	mov	x0, x1
	tst	w3, 67108864
	bne	.L4
	cmp	x0, x7
	bls	.L25
	.p2align 3,,7
.L6:
	mov	w0, 1
	b	.L1
	.p2align 2,,3
.L10:
	cmp	w19, 8
	bne	.L6
	ldr	x1, [sp, 32]
	mov	w0, 0
	str	x1, [x20]
	b	.L1
	.p2align 2,,3
.L3:
	add	x0, sp, 32
	bl	__memcpy_fromio
	cmp	w19, 4
	bne	.L26
.L9:
	ldr	w1, [sp, 32]
	mov	w0, 0
	str	x1, [x20]
	b	.L1
	.p2align 2,,3
.L11:
	ldrb	w1, [sp, 32]
	mov	w0, 0
	str	x1, [x20]
	b	.L1
.L24:
	bl	__stack_chk_fail
	.size	rk_align_rd.part.0, .-rk_align_rd.part.0
	.align	2
	.type	rk_align_wr, %function
rk_align_wr:
	stp	x29, x30, [sp, -32]!
	mrs	x3, sp_el0
	mov	x29, sp
	ldr	x4, [x3, 1456]
	str	x4, [sp, 24]
	mov	x4, 0
	cmp	w1, 4
	beq	.L28
	bgt	.L29
	cmp	w1, 1
	beq	.L30
	cmp	w1, 2
	bne	.L44
	strh	w2, [sp, 16]
.L34:
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x5, sp_el0
// 0 "" 2
#NO_APP
	ldr	x6, [x5]
	sxtw	x2, w1
	mov	x3, 4294963200
	mov	x4, 549755813888
	tst	w6, 4194304
	csel	x1, x3, x4, ne
	cmp	x0, x1
	bcs	.L35
	add	x6, sp, 16
	mov	x3, x0
	sub	x6, x6, x0
	mov	x7, 549755813887
	mov	w8, 0
	b	.L38
	.p2align 2,,3
.L51:
#APP
// 171 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x4, x3, 36028797018963968

// 0 "" 2
#NO_APP
	ldrb	w1, [x6, x3]
	mov	w0, w8
#APP
// 36 "arch/arm64/mm/fixup_fault.c" 1
	1:	sttrb	w1, [x4]
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
.short		((((.L__gpr_num_w0) << 0) | ((.L__gpr_num_wzr) << 5)))
.popsection

// 0 "" 2
#NO_APP
	cbnz	w0, .L44
	add	x3, x3, 1
	subs	x2, x2, #1
	beq	.L27
.L38:
	ldr	w0, [x5, 44]
	tbz	x0, 21, .L50
.L36:
	sbfx	x0, x3, 0, 56
	and	x0, x0, x3
.L37:
	cmp	x0, x7
	bls	.L51
.L44:
	mov	w0, 1
.L27:
	mrs	x1, sp_el0
	ldr	x3, [sp, 24]
	ldr	x2, [x1, 1456]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L52
	ldp	x29, x30, [sp], 32
	ret
	.p2align 2,,3
.L29:
	cmp	w1, 8
	bne	.L44
	str	x2, [sp, 16]
	b	.L34
	.p2align 2,,3
.L35:
	add	x1, sp, 16
	bl	__memcpy_toio
	mov	w0, 0
	b	.L27
	.p2align 2,,3
.L50:
	ldr	x1, [x5]
	mov	x0, x3
	tst	w1, 67108864
	beq	.L37
	b	.L36
	.p2align 2,,3
.L30:
	strb	w2, [sp, 16]
	b	.L34
	.p2align 2,,3
.L28:
	str	w2, [sp, 16]
	b	.L34
.L52:
	bl	__stack_chk_fail
	.size	rk_align_wr, .-rk_align_wr
	.align	2
	.global	alignment_fixup_helper
	.type	alignment_fixup_helper, %function
alignment_fixup_helper:
	stp	x29, x30, [sp, -160]!
	mrs	x1, sp_el0
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x2
	stp	x21, x22, [sp, 32]
	mov	x21, x0
	ldr	x2, [x1, 1456]
	str	x2, [sp, 152]
	mov	x2, 0
	ldr	x1, [x19, 264]
	tst	x1, 15
	bne	.L54
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
	bcc	.L56
.L60:
	mov	w0, 1
.L53:
	mrs	x1, sp_el0
	ldr	x3, [sp, 152]
	ldr	x2, [x1, 1456]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L329
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x29, x30, [sp], 160
	ret
	.p2align 2,,3
.L56:
	ldr	w3, [x2, 44]
	ldr	x0, [x19, 256]
	mov	x1, x0
	tbz	x3, 21, .L330
.L58:
	sbfx	x2, x0, 0, 56
	and	x0, x0, x2
.L59:
	mov	x2, 549755813884
	cmp	x0, x2
	bhi	.L60
#APP
// 171 "./arch/arm64/include/asm/uaccess.h" 1
		bic	x1, x1, 36028797018963968

// 0 "" 2
#NO_APP
	mov	w0, 0
#APP
// 712 "arch/arm64/mm/fixup_fault.c" 1
	1:	ldtr	w2, [x1]
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
.short		((((.L__gpr_num_w0) << 0) | ((.L__gpr_num_w2) << 5)))
.popsection

// 0 "" 2
#NO_APP
	cbnz	w0, .L60
	mov	w0, w2
	str	w2, [sp, 116]
	bl	aarch64_get_insn_class
	mov	w1, w0
	cmp	w0, 5
	bne	.L331
.L62:
	ldr	w20, [sp, 116]
	uxtw	x0, w20
	ubfx	x21, x20, 28, 2
	ubfx	x22, x20, 26, 1
	cmp	w21, 2
	beq	.L332
	cmp	w21, 3
	bne	.L60
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	ubfx	x25, x0, 10, 2
	tbnz	x0, 24, .L116
	cmp	x25, 2
	ubfx	x0, x0, 16, 6
	cset	w2, ne
	ands	w0, w0, 32
	cset	w1, eq
	ands	w23, w2, w1
	bne	.L116
	cmp	w0, 0
	ccmp	x25, 2, 0, ne
	beq	.L333
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	b	.L60
	.p2align 2,,3
.L54:
	ldr	x0, [x19, 256]
	add	x1, sp, 116
	bl	aarch64_insn_read
	cbnz	w0, .L60
	ldr	w0, [sp, 116]
	bl	aarch64_get_insn_class
	mov	w1, w0
	cmp	w0, 5
	beq	.L62
.L331:
	mov	w0, 1
	cmp	w1, 6
	bne	.L53
	ldr	w1, [sp, 116]
	mov	w2, 29728
	movk	w2, 0xd50b, lsl 16
	and	w1, w1, -32
	cmp	w1, w2
	bne	.L60
#APP
// 121 "arch/arm64/mm/fixup_fault.c" 1
		.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	.equ	.L__gpr_num_x\num, \num
	.equ	.L__gpr_num_w\num, \num
	.endr
	.equ	.L__gpr_num_xzr, 31
	.equ	.L__gpr_num_wzr, 31
	.macro	mrs_s, rt, sreg
.inst (0xd5200000|(\sreg)|(.L__gpr_num_\rt))
		.endm
	mrs_s x1, (((3) << 19) | ((3) << 16) | ((0) << 12) | ((0) << 8) | ((7) << 5))
	.purgem	mrs_s

// 0 "" 2
#NO_APP
	and	w1, w1, 15
	mov	x2, 4294963200
	add	w1, w1, 2
	mov	x3, 549755813888
#APP
// 19 "./arch/arm64/include/asm/current.h" 1
	mrs x4, sp_el0
// 0 "" 2
#NO_APP
	ldr	x4, [x4]
	lsl	w20, w0, w1
	neg	w0, w20
	tst	w4, 4194304
	sxtw	x0, w0
	and	x21, x0, x21
	csel	x2, x2, x3, ne
	cmp	x21, x2
	bcs	.L64
	.p2align 3,,7
.L65:
	mov	x0, x21
	mov	x2, 0
	mov	w1, 1
	bl	rk_align_wr
	cbnz	w0, .L60
	subs	w20, w20, #1
	bne	.L65
.L66:
	ldr	x1, [x19, 256]
	mov	w0, 0
	add	x1, x1, 4
	str	x1, [x19, 256]
	b	.L53
	.p2align 2,,3
.L330:
	ldr	x2, [x2]
	tst	w2, 67108864
	beq	.L59
	b	.L58
	.p2align 2,,3
.L332:
	stp	x23, x24, [sp, 48]
	mov	w1, w20
	lsr	w23, w20, 30
	stp	x25, x26, [sp, 64]
	ubfx	x24, x20, 22, 1
	and	w25, w20, 8388608
	stp	x27, x28, [sp, 80]
	and	w26, w20, 16777216
	mov	w0, 1
	cbnz	x22, .L68
	bl	aarch64_insn_decode_register
	mov	w27, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w28, w0
	mov	w1, w20
	mov	w0, w21
	bl	aarch64_insn_decode_register
	mov	w21, w0
	mov	w1, w20
	mov	w0, 7
	bl	aarch64_insn_decode_immediate
	eor	w1, w24, 1
	and	w5, w23, 1
	mov	w3, w1
	tst	w23, w1
	bne	.L188
	cmp	w23, 3
	beq	.L188
	cbz	w25, .L70
	cmp	w27, w28
	ccmp	w27, w21, 4, ne
	ccmp	w27, 31, 4, eq
	bne	.L188
.L70:
	cmp	w28, w21
	cset	w1, eq
	tst	w1, w24
	bne	.L188
	asr	w4, w23, 1
	mov	w24, 8
	add	w4, w4, 2
	and	w2, w27, 536870911
	lsl	w24, w24, w4
	asr	w1, w24, 3
	cmp	w2, 32
	beq	.L71
	bhi	.L72
	cmp	w2, 30
	bhi	.L334
	ldr	x23, [x19, x2, lsl 3]
.L75:
	sbfx	x20, x0, 0, 7
	cmp	w26, 0
	lsl	x20, x20, x4
	add	x0, x23, x20
	csel	x23, x0, x23, ne
	cbz	w3, .L78
	mov	x2, 0
	cmp	w28, 31
	beq	.L79
	ldr	x2, [x19, w28, sxtw 3]
.L79:
	str	x2, [sp, 136]
	cmp	w21, 31
	beq	.L80
	ldr	x22, [x19, w21, sxtw 3]
.L80:
	mov	x0, x23
	str	w1, [sp, 96]
	str	x22, [sp, 144]
	bl	rk_align_wr
	ldr	w1, [sp, 96]
	cbnz	w0, .L188
	mov	x2, x22
	add	x0, x23, w1, sxtw
	bl	rk_align_wr
	cbnz	w0, .L188
.L82:
	cbz	w25, .L321
	add	x20, x23, x20
	cmp	w26, 0
	csel	x23, x20, x23, eq
	cmp	w27, 31
	beq	.L335
	str	x23, [x19, w27, sxtw 3]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
.L341:
	cbnz	x25, .L188
	tbz	x24, 0, .L321
	.p2align 3,,7
.L188:
	mov	w0, 1
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L53
	.p2align 2,,3
.L68:
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
	beq	.L188
	cmp	w22, w21
	cset	w1, eq
	tst	w1, w24
	bne	.L188
	add	w23, w23, 2
	mov	w28, 8
	and	w1, w27, 536870911
	lsl	w28, w28, w23
	asr	w28, w28, 3
	cmp	w1, 32
	beq	.L92
	bhi	.L93
	cmp	w1, 30
	bhi	.L336
	ldr	x20, [x19, x1, lsl 3]
	.p2align 3,,7
.L96:
	sbfx	x0, x0, 0, 7
	cmp	w26, 0
	lsl	x0, x0, x23
	str	x0, [sp, 96]
	add	x0, x20, x0
	csel	x20, x0, x20, ne
	cbnz	w24, .L99
	mov	x2, 0
	cmp	w22, 31
	bhi	.L100
	mov	w0, w22
	mov	w1, 0
	bl	rk_align_get_data_asm
	mov	x2, x0
.L100:
	str	x2, [sp, 120]
	mov	x24, 0
	cmp	w21, 31
	bhi	.L101
	mov	w0, w21
	mov	w1, 0
	str	x2, [sp, 104]
	bl	rk_align_get_data_asm
	ldr	x2, [sp, 104]
	mov	x24, x0
.L101:
	str	x24, [sp, 136]
	cmp	w23, 4
	bne	.L102
	mov	x23, 0
	cmp	w22, 31
	bhi	.L103
	mov	w0, w22
	mov	w1, 1
	str	x2, [sp, 104]
	bl	rk_align_get_data_asm
	ldr	x2, [sp, 104]
	mov	x23, x0
.L103:
	str	x23, [sp, 128]
	mov	x22, 0
	cmp	w21, 31
	bhi	.L104
	mov	w0, w21
	mov	w1, 1
	str	x2, [sp, 104]
	bl	rk_align_get_data_asm
	ldr	x2, [sp, 104]
	mov	x22, x0
.L104:
	mov	x0, x20
	mov	w1, 8
	str	x22, [sp, 144]
	bl	rk_align_wr
	cbnz	w0, .L188
	mov	x2, x23
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L188
	mov	x2, x24
	add	x0, x20, 16
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L188
	mov	x2, x22
	add	x0, x20, 24
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L188
.L106:
	cbz	w25, .L321
	ldr	x0, [sp, 96]
	cmp	w26, 0
	add	x0, x20, x0
	csel	x20, x0, x20, eq
	cmp	w27, 31
	beq	.L328
	str	x20, [x19, w27, sxtw 3]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
	.p2align 2,,3
.L116:
	and	w23, w20, 33553408
	lsr	w26, w20, 30
	and	w23, w23, -16775169
	lsr	w24, w20, 22
	cbnz	x22, .L118
	eor	x2, x20, 2048
	cmp	w23, 1024
	mov	w1, w20
	mov	w0, 1
	ubfx	x2, x2, 11, 1
	stp	x27, x28, [sp, 80]
	csel	w21, w2, wzr, eq
	bl	aarch64_insn_decode_register
	mov	w27, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w28, w0
	mov	w1, w20
	tbnz	x20, 24, .L120
	mov	w0, 6
	bl	aarch64_insn_decode_immediate
	sbfx	x20, x0, 0, 9
.L121:
	tbnz	x24, 1, .L122
	mvn	w0, w24
	cmp	w26, 3
	mov	w3, 64
	mov	w1, 32
	and	w0, w0, 1
	csel	w3, w3, w1, eq
	mov	w4, 0
.L123:
	cmp	w27, w28
	ccmp	w27, 31, 4, eq
	bne	.L188
	and	w1, w27, 536870911
	cmp	w1, 32
	beq	.L127
	bhi	.L128
	cmp	w1, 30
	bhi	.L337
	ldr	x25, [x19, x1, lsl 3]
.L131:
	mov	w24, 8
	cmp	w21, 0
	add	x2, x25, x20
	lsl	w24, w24, w26
	csel	x25, x2, x25, eq
	asr	w1, w24, 3
	cbz	w0, .L134
	cmp	w28, 31
	beq	.L135
	ldr	x22, [x19, w28, sxtw 3]
.L135:
	mov	x2, x22
	mov	x0, x25
	str	x22, [sp, 144]
	bl	rk_align_wr
	cbnz	w0, .L188
.L136:
	cmp	w23, 1024
	beq	.L338
.L321:
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
.L64:
	sxtw	x2, w20
	mov	x0, x21
	mov	w1, 0
	bl	__memset_io
	b	.L66
.L336:
	cmp	w1, 31
	bne	.L323
	ldr	x20, [x19, 248]
	b	.L96
.L93:
	cmp	w1, 33
	bne	.L323
	ldr	x20, [x19, 264]
	b	.L96
.L118:
	cmp	w23, 1024
	eor	x0, x20, 2048
	lsl	w21, w24, 1
	mov	w1, w20
	ubfx	x0, x0, 11, 1
	and	w21, w21, 4
	csel	w22, w0, wzr, eq
	mov	w0, 1
	bl	aarch64_insn_decode_register
	orr	w21, w21, w26
	mov	w1, w20
	mov	w25, w0
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w26, w0
	cmp	w21, 4
	bhi	.L145
	stp	x27, x28, [sp, 80]
	mov	w1, w20
	tbnz	x20, 24, .L146
	mov	w0, 6
	bl	aarch64_insn_decode_immediate
	sbfx	x27, x0, 0, 9
.L147:
	mov	w1, 8
	and	w0, w25, 536870911
	lsl	w28, w1, w21
	cmp	w0, 32
	beq	.L148
	bhi	.L149
	cmp	w0, 30
	bhi	.L339
	ldr	x20, [x19, x0, lsl 3]
.L152:
	cmp	w22, 0
	add	x0, x20, x27
	csel	x20, x0, x20, eq
	tbnz	x24, 0, .L155
	cmp	w26, 31
	bhi	.L156
	mov	w0, w26
	mov	w1, 0
	bl	rk_align_get_data_asm
	str	x0, [sp, 136]
	mov	x24, x0
	cmp	w21, 4
	bne	.L157
	mov	w0, w26
	mov	w1, 1
	bl	rk_align_get_data_asm
	mov	x21, x0
.L213:
	mov	x2, x24
	mov	x0, x20
	mov	w1, 8
	str	x21, [sp, 144]
	bl	rk_align_wr
	cbnz	w0, .L313
	mov	x2, x21
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_wr
	cbnz	w0, .L313
.L159:
	cmp	w23, 1024
	bne	.L321
	add	x27, x20, x27
	cmp	w22, 0
	csel	x20, x27, x20, ne
	cmp	w25, 31
	beq	.L328
	str	x20, [x19, w25, sxtw 3]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
	.p2align 2,,3
.L333:
	stp	x27, x28, [sp, 80]
	ubfx	x1, x20, 13, 1
	lsr	w28, w20, 30
	lsr	w27, w20, 22
	ubfx	x0, x20, 12, 1
	cmp	w1, 0
	cbnz	x22, .L166
	mov	w1, 64
	mov	w26, 32
	csel	w26, w26, w1, eq
	cmp	w0, 0
	mov	w1, w20
	csel	w25, w28, wzr, ne
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
	tbnz	x27, 1, .L169
	mvn	w2, w27
	and	w23, w2, 1
.L170:
	cmp	w24, w20
	ccmp	w24, 31, 4, eq
	bne	.L188
	mov	x1, 0
	cmp	w0, 31
	beq	.L174
	ldr	x1, [x19, w0, sxtw 3]
.L174:
	cmp	w26, 32
	and	x0, x1, 4294967295
	and	w24, w24, 536870911
	csel	x1, x0, x1, eq
	lsl	x1, x1, x25
	cmp	w24, 32
	beq	.L176
	bhi	.L177
	cmp	w24, 30
	bhi	.L340
	ldr	x0, [x19, x24, lsl 3]
.L180:
	mov	w2, 8
	lsl	w2, w2, w28
	add	x0, x1, x0
	asr	w1, w2, 3
	cbz	w23, .L182
	cmp	w20, 31
	beq	.L183
	ldr	x22, [x19, w20, sxtw 3]
.L183:
	mov	x2, x22
	bl	rk_align_wr
	cbz	w0, .L321
	b	.L188
	.p2align 2,,3
.L72:
	cmp	w2, 33
	bne	.L322
	ldr	x23, [x19, 264]
	b	.L75
.L334:
	cmp	w2, 31
	bne	.L322
	ldr	x23, [x19, 248]
	b	.L75
.L339:
	cmp	w0, 31
	bne	.L325
	ldr	x20, [x19, 248]
	b	.L152
.L149:
	cmp	w0, 33
	bne	.L325
	ldr	x20, [x19, 264]
	b	.L152
.L157:
	mov	x2, x24
	asr	w1, w28, 3
	mov	x0, x20
	bl	rk_align_wr
	cbz	w0, .L159
.L313:
	ldp	x27, x28, [sp, 80]
.L145:
	mov	w0, 1
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	b	.L53
.L323:
	mov	x20, 0
	b	.L96
.L122:
	cmp	w26, 3
	beq	.L341
	and	w24, w24, 1
	cmp	w26, 2
	beq	.L342
	mov	w0, 32
	cmp	w24, 0
	mov	w3, 64
	mov	w4, 1
	csel	w3, w3, w0, eq
	mov	w0, 0
	b	.L123
.L120:
	mov	w0, 5
	bl	aarch64_insn_decode_immediate
	lsl	x20, x0, x26
	b	.L121
.L99:
	cmp	w23, 4
	beq	.L343
	cmp	w28, 4
	beq	.L110
	cmp	w28, 8
	bne	.L188
	add	x2, sp, 120
	mov	w1, w28
	mov	x0, x20
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	add	x0, x20, 8
.L209:
	mov	w1, w28
	add	x2, sp, 136
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	str	xzr, [sp, 128]
	str	xzr, [sp, 144]
.L109:
	cmp	w22, 31
	bhi	.L112
	ldr	x2, [sp, 120]
	mov	w0, w22
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 128]
	mov	w0, w22
	mov	w1, 1
	bl	rk_align_set_data_asm
.L112:
	ldr	x2, [sp, 136]
	cmp	w21, 31
	bhi	.L106
	mov	w0, w21
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 144]
	mov	w0, w21
	mov	w1, 1
	bl	rk_align_set_data_asm
	b	.L106
.L166:
	mov	w1, 64
	lsl	w22, w27, 1
	mov	w24, 32
	csel	w24, w24, w1, eq
	cmp	w0, 0
	and	w22, w22, 4
	orr	w22, w22, w28
	mov	w1, w20
	csel	w26, w22, wzr, ne
	mov	w0, 1
	bl	aarch64_insn_decode_register
	mov	w23, w0
	mov	w1, w20
	mov	w0, 0
	bl	aarch64_insn_decode_register
	mov	w25, w0
	mov	w1, w20
	mov	w0, w21
	bl	aarch64_insn_decode_register
	tbz	x27, 1, .L188
	cmp	w23, w25
	ccmp	w23, 31, 4, eq
	bne	.L188
	mov	x1, 0
	cmp	w0, 31
	beq	.L189
	ldr	x1, [x19, w0, sxtw 3]
.L189:
	cmp	w24, 32
	and	x0, x1, 4294967295
	and	w23, w23, 536870911
	csel	x1, x0, x1, eq
	lsl	x20, x1, x26
	cmp	w23, 32
	beq	.L191
	bhi	.L192
	cmp	w23, 30
	bhi	.L344
	ldr	x0, [x19, x23, lsl 3]
.L195:
	mov	w21, 8
	add	x20, x20, x0
	lsl	w23, w21, w22
	tbnz	x27, 0, .L197
	cmp	w25, 31
	bhi	.L198
	mov	w0, w25
	mov	w1, 0
	bl	rk_align_get_data_asm
	str	x0, [sp, 136]
	mov	x21, x0
	cmp	w22, 4
	bne	.L199
	mov	w0, w25
	mov	w1, 1
	bl	rk_align_get_data_asm
	mov	x22, x0
.L214:
	mov	x2, x21
	mov	x0, x20
	mov	w1, 8
	str	x22, [sp, 144]
	bl	rk_align_wr
	cbnz	w0, .L188
	mov	x2, x22
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_wr
	cbz	w0, .L321
	b	.L188
	.p2align 2,,3
.L322:
	mov	x23, 0
	b	.L75
.L337:
	cmp	w1, 31
	bne	.L324
	ldr	x25, [x19, 248]
	b	.L131
.L128:
	cmp	w1, 33
	bne	.L324
	ldr	x25, [x19, 264]
	b	.L131
.L325:
	mov	x20, 0
	b	.L152
.L155:
	cmp	w21, 4
	beq	.L345
	asr	w1, w28, 3
	mov	x0, 278
	lsr	x0, x0, x1
	tbz	x0, 0, .L313
	add	x2, sp, 136
	mov	x0, x20
	bl	rk_align_rd.part.0
	cbnz	w0, .L313
	str	xzr, [sp, 144]
.L162:
	ldr	x2, [sp, 136]
	cmp	w26, 31
	bhi	.L159
	mov	w0, w26
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 144]
	mov	w0, w26
	mov	w1, 1
	bl	rk_align_set_data_asm
	b	.L159
.L146:
	mov	w0, 5
	bl	aarch64_insn_decode_immediate
	lsl	x27, x0, x21
	b	.L147
.L102:
	mov	w1, w28
	mov	x0, x20
	bl	rk_align_wr
	cbnz	w0, .L188
	mov	x2, x24
	mov	w1, w28
	add	x0, x20, w28, sxtw
	bl	rk_align_wr
	cbz	w0, .L106
	b	.L188
.L78:
	cmp	w1, 4
	beq	.L83
	cmp	w1, 8
	bne	.L188
	add	x2, sp, 136
	mov	x0, x23
	str	w1, [sp, 96]
	str	w5, [sp, 104]
	bl	rk_align_rd.part.0
	ldr	w1, [sp, 96]
	ldr	w5, [sp, 104]
	cbnz	w0, .L188
	add	x0, x23, 8
.L205:
	add	x2, sp, 144
	str	w5, [sp, 96]
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	ldr	w5, [sp, 96]
	cbnz	w5, .L302
	ldr	x0, [sp, 144]
.L85:
	cmp	w28, 31
	beq	.L86
	ldr	x1, [sp, 136]
	str	x1, [x19, w28, sxtw 3]
.L86:
	cmp	w21, 31
	beq	.L82
	str	x0, [x19, w21, sxtw 3]
	b	.L82
.L340:
	cmp	w24, 31
	bne	.L326
	ldr	x0, [x19, 248]
	b	.L180
.L177:
	cmp	w24, 33
	bne	.L326
	ldr	x0, [x19, 264]
	b	.L180
.L169:
	cmp	w28, 3
	beq	.L346
	cmp	w28, 2
	bne	.L170
	tbz	x27, 0, .L170
	b	.L188
	.p2align 2,,3
.L324:
	mov	x25, 0
	b	.L131
.L92:
	ldr	x20, [x19, 256]
	b	.L96
.L344:
	cmp	w23, 31
	bne	.L327
	ldr	x0, [x19, 248]
	b	.L195
.L192:
	cmp	w23, 33
	bne	.L327
	ldr	x0, [x19, 264]
	b	.L195
.L343:
	add	x2, sp, 120
	mov	x0, x20
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	add	x2, sp, 128
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	add	x2, sp, 136
	add	x0, x20, 16
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	add	x2, sp, 144
	add	x0, x20, 24
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbz	w0, .L109
	b	.L188
	.p2align 2,,3
.L156:
	str	xzr, [sp, 136]
	mov	x24, 0
	cmp	w21, 4
	bne	.L157
	mov	x21, 0
	b	.L213
.L71:
	ldr	x23, [x19, 256]
	b	.L75
.L148:
	ldr	x20, [x19, 256]
	b	.L152
.L134:
	mov	x0, 278
	lsr	x0, x0, x1
	tbz	x0, 0, .L188
	add	x2, sp, 144
	mov	x0, x25
	str	w4, [sp, 96]
	str	w3, [sp, 104]
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	ldr	w4, [sp, 96]
	cbz	w4, .L138
	sub	w24, w24, #1
	mov	w4, 31
	ldr	x1, [sp, 144]
	sub	w4, w4, w24, sxtb
	mov	w2, 63
	ldr	w3, [sp, 104]
	sub	w2, w2, w24, sxtb
	lsl	w0, w1, w4
	cmp	w3, 32
	asr	w0, w0, w4
	sxtw	x0, w0
	lsl	x1, x1, x2
	asr	x1, x1, x2
	csel	x0, x0, x1, eq
	str	x0, [sp, 144]
.L138:
	ldr	x0, [sp, 144]
	cmp	w28, 31
	beq	.L136
	str	x0, [x19, w28, sxtw 3]
	b	.L136
.L326:
	mov	x0, 0
	b	.L180
.L345:
	add	x2, sp, 136
	mov	x0, x20
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbnz	w0, .L313
	add	x2, sp, 144
	add	x0, x20, 8
	mov	w1, 8
	bl	rk_align_rd.part.0
	cbz	w0, .L162
	ldp	x27, x28, [sp, 80]
	b	.L145
.L342:
	cbnz	w24, .L188
	mov	w3, 64
	mov	w4, 1
	mov	w0, 0
	b	.L123
.L110:
	add	x2, sp, 120
	mov	w1, w28
	mov	x0, x20
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	add	x0, x20, 4
	b	.L209
.L327:
	mov	x0, 0
	b	.L195
.L197:
	cmp	w22, 4
	beq	.L347
	asr	w1, w23, 3
	sub	w2, w1, #4
	sub	w0, w1, #1
	tst	w2, -5
	ccmp	w0, 1, 0, ne
	bhi	.L188
	mov	x0, x20
	add	x2, sp, 136
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	str	xzr, [sp, 144]
.L203:
	ldr	x2, [sp, 136]
	cmp	w25, 31
	bhi	.L321
	mov	w0, w25
	mov	w1, 0
	bl	rk_align_set_data_asm
	ldr	x2, [sp, 144]
	mov	w0, w25
	mov	w1, 1
	bl	rk_align_set_data_asm
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
.L83:
	add	x2, sp, 136
	mov	x0, x23
	str	w1, [sp, 96]
	str	w5, [sp, 104]
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	ldr	w1, [sp, 96]
	add	x0, x23, 4
	ldr	w5, [sp, 104]
	b	.L205
.L338:
	cbz	w21, .L142
	add	x25, x25, x20
.L142:
	cmp	w27, 31
	beq	.L348
	str	x25, [x19, w27, sxtw 3]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
.L328:
	str	x20, [x19, 248]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
.L127:
	ldr	x25, [x19, 256]
	b	.L131
.L346:
	tbz	x27, 0, .L321
	b	.L188
.L182:
	mov	x2, 278
	lsr	x2, x2, x1
	tbz	x2, 0, .L188
	add	x2, sp, 144
	bl	rk_align_rd.part.0
	cbz	w0, .L321
	b	.L188
.L198:
	str	xzr, [sp, 136]
	mov	x21, 0
	cmp	w22, 4
	bne	.L199
	mov	x22, 0
	b	.L214
.L176:
	ldr	x0, [x19, 256]
	b	.L180
.L199:
	mov	x2, x21
	asr	w1, w23, 3
	mov	x0, x20
	bl	rk_align_wr
	cbz	w0, .L321
	b	.L188
.L191:
	ldr	x0, [x19, 256]
	b	.L195
.L329:
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	bl	__stack_chk_fail
.L335:
	str	x23, [x19, 248]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
.L302:
	ldp	x2, x0, [sp, 136]
	mov	w1, 64
	sub	w1, w1, w24
	lsl	x2, x2, x1
	lsl	x0, x0, x1
	asr	x2, x2, x1
	asr	x0, x0, x1
	stp	x2, x0, [sp, 136]
	b	.L85
.L348:
	str	x25, [x19, 248]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	b	.L66
.L347:
	add	x2, sp, 136
	mov	w1, w21
	mov	x0, x20
	bl	rk_align_rd.part.0
	cbnz	w0, .L188
	mov	w1, w21
	add	x0, x20, 8
	add	x2, sp, 144
	bl	rk_align_rd.part.0
	cbz	w0, .L203
	b	.L188
	.size	alignment_fixup_helper, .-alignment_fixup_helper
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
