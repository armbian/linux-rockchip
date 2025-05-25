/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * u64 rk_align_get_data_asm(int n, int t);
 * x0: n - vector register number
 * w1: t - index selector
 * returns: selected 64-bit data in x0
 */
SYM_FUNC_START(rk_align_get_data_asm)
	cmp	w0, #31
	b.gt	.Lget_default

	// Create an index for the jump table
	adr	x3, .Lget_jumptable
	add	x3, x3, w0, uxtw #3  // Each entry is 8 bytes
	br	x3

.Lget_jumptable:
	b	.Lget0
	b	.Lget1
	b	.Lget2
	b	.Lget3
	b	.Lget4
	b	.Lget5
	b	.Lget6
	b	.Lget7
	b	.Lget8
	b	.Lget9
	b	.Lget10
	b	.Lget11
	b	.Lget12
	b	.Lget13
	b	.Lget14
	b	.Lget15
	b	.Lget16
	b	.Lget17
	b	.Lget18
	b	.Lget19
	b	.Lget20
	b	.Lget21
	b	.Lget22
	b	.Lget23
	b	.Lget24
	b	.Lget25
	b	.Lget26
	b	.Lget27
	b	.Lget28
	b	.Lget29
	b	.Lget30
	b	.Lget31

.Lget0:
	cbnz	w1, 1f
	mov	x0, v0.d[0]
	b	.Lget_end
1:	mov	x0, v0.d[1]
	b	.Lget_end

.Lget1:
	cbnz	w1, 1f
	mov	x0, v1.d[0]
	b	.Lget_end
1:	mov	x0, v1.d[1]
	b	.Lget_end

.Lget2:
	cbnz	w1, 1f
	mov	x0, v2.d[0]
	b	.Lget_end
1:	mov	x0, v2.d[1]
	b	.Lget_end

.Lget3:
	cbnz	w1, 1f
	mov	x0, v3.d[0]
	b	.Lget_end
1:	mov	x0, v3.d[1]
	b	.Lget_end

.Lget4:
	cbnz	w1, 1f
	mov	x0, v4.d[0]
	b	.Lget_end
1:	mov	x0, v4.d[1]
	b	.Lget_end

.Lget5:
	cbnz	w1, 1f
	mov	x0, v5.d[0]
	b	.Lget_end
1:	mov	x0, v5.d[1]
	b	.Lget_end

.Lget6:
	cbnz	w1, 1f
	mov	x0, v6.d[0]
	b	.Lget_end
1:	mov	x0, v6.d[1]
	b	.Lget_end

.Lget7:
	cbnz	w1, 1f
	mov	x0, v7.d[0]
	b	.Lget_end
1:	mov	x0, v7.d[1]
	b	.Lget_end

.Lget8:
	cbnz	w1, 1f
	mov	x0, v8.d[0]
	b	.Lget_end
1:	mov	x0, v8.d[1]
	b	.Lget_end

.Lget9:
	cbnz	w1, 1f
	mov	x0, v9.d[0]
	b	.Lget_end
1:	mov	x0, v9.d[1]
	b	.Lget_end

.Lget10:
	cbnz	w1, 1f
	mov	x0, v10.d[0]
	b	.Lget_end
1:	mov	x0, v10.d[1]
	b	.Lget_end

.Lget11:
	cbnz	w1, 1f
	mov	x0, v11.d[0]
	b	.Lget_end
1:	mov	x0, v11.d[1]
	b	.Lget_end

.Lget12:
	cbnz	w1, 1f
	mov	x0, v12.d[0]
	b	.Lget_end
1:	mov	x0, v12.d[1]
	b	.Lget_end

.Lget13:
	cbnz	w1, 1f
	mov	x0, v13.d[0]
	b	.Lget_end
1:	mov	x0, v13.d[1]
	b	.Lget_end

.Lget14:
	cbnz	w1, 1f
	mov	x0, v14.d[0]
	b	.Lget_end
1:	mov	x0, v14.d[1]
	b	.Lget_end

.Lget15:
	cbnz	w1, 1f
	mov	x0, v15.d[0]
	b	.Lget_end
1:	mov	x0, v15.d[1]
	b	.Lget_end

.Lget16:
	cbnz	w1, 1f
	mov	x0, v16.d[0]
	b	.Lget_end
1:	mov	x0, v16.d[1]
	b	.Lget_end

.Lget17:
	cbnz	w1, 1f
	mov	x0, v17.d[0]
	b	.Lget_end
1:	mov	x0, v17.d[1]
	b	.Lget_end

.Lget18:
	cbnz	w1, 1f
	mov	x0, v18.d[0]
	b	.Lget_end
1:	mov	x0, v18.d[1]
	b	.Lget_end

.Lget19:
	cbnz	w1, 1f
	mov	x0, v19.d[0]
	b	.Lget_end
1:	mov	x0, v19.d[1]
	b	.Lget_end

.Lget20:
	cbnz	w1, 1f
	mov	x0, v20.d[0]
	b	.Lget_end
1:	mov	x0, v20.d[1]
	b	.Lget_end

.Lget21:
	cbnz	w1, 1f
	mov	x0, v21.d[0]
	b	.Lget_end
1:	mov	x0, v21.d[1]
	b	.Lget_end

.Lget22:
	cbnz	w1, 1f
	mov	x0, v22.d[0]
	b	.Lget_end
1:	mov	x0, v22.d[1]
	b	.Lget_end

.Lget23:
	cbnz	w1, 1f
	mov	x0, v23.d[0]
	b	.Lget_end
1:	mov	x0, v23.d[1]
	b	.Lget_end

.Lget24:
	cbnz	w1, 1f
	mov	x0, v24.d[0]
	b	.Lget_end
1:	mov	x0, v24.d[1]
	b	.Lget_end

.Lget25:
	cbnz	w1, 1f
	mov	x0, v25.d[0]
	b	.Lget_end
1:	mov	x0, v25.d[1]
	b	.Lget_end

.Lget26:
	cbnz	w1, 1f
	mov	x0, v26.d[0]
	b	.Lget_end
1:	mov	x0, v26.d[1]
	b	.Lget_end

.Lget27:
	cbnz	w1, 1f
	mov	x0, v27.d[0]
	b	.Lget_end
1:	mov	x0, v27.d[1]
	b	.Lget_end

.Lget28:
	cbnz	w1, 1f
	mov	x0, v28.d[0]
	b	.Lget_end
1:	mov	x0, v28.d[1]
	b	.Lget_end

.Lget29:
	cbnz	w1, 1f
	mov	x0, v29.d[0]
	b	.Lget_end
1:	mov	x0, v29.d[1]
	b	.Lget_end

.Lget30:
	cbnz	w1, 1f
	mov	x0, v30.d[0]
	b	.Lget_end
1:	mov	x0, v30.d[1]
	b	.Lget_end

.Lget31:
	cbnz	w1, 1f
	mov	x0, v31.d[0]
	b	.Lget_end
1:	mov	x0, v31.d[1]
	b	.Lget_end

.Lget_default:
	mov	x0, #0

.Lget_end:
	ret
SYM_FUNC_END(rk_align_get_data_asm)

/*
 * void rk_align_set_data_asm(int n, int t, u64 val);
 * x0: n - vector register number
 * w1: t - index selector
 * x2: val - 64-bit value to set
 */
SYM_FUNC_START(rk_align_set_data_asm)
	cmp	w0, #31
	b.gt	.Lset_end

	// Create an index for the jump table
	adr	x3, .Lset_jumptable
	add	x3, x3, w0, uxtw #3  // Each entry is 8 bytes
	br	x3

.Lset_jumptable:
	b	.Lset0
	b	.Lset1
	b	.Lset2
	b	.Lset3
	b	.Lset4
	b	.Lset5
	b	.Lset6
	b	.Lset7
	b	.Lset8
	b	.Lset9
	b	.Lset10
	b	.Lset11
	b	.Lset12
	b	.Lset13
	b	.Lset14
	b	.Lset15
	b	.Lset16
	b	.Lset17
	b	.Lset18
	b	.Lset19
	b	.Lset20
	b	.Lset21
	b	.Lset22
	b	.Lset23
	b	.Lset24
	b	.Lset25
	b	.Lset26
	b	.Lset27
	b	.Lset28
	b	.Lset29
	b	.Lset30
	b	.Lset31

.Lset0:
	cbnz	w1, 1f
	mov	v0.d[0], x2
	b	.Lset_end
1:	mov	v0.d[1], x2
	b	.Lset_end

.Lset1:
	cbnz	w1, 1f
	mov	v1.d[0], x2
	b	.Lset_end
1:	mov	v1.d[1], x2
	b	.Lset_end

.Lset2:
	cbnz	w1, 1f
	mov	v2.d[0], x2
	b	.Lset_end
1:	mov	v2.d[1], x2
	b	.Lset_end

.Lset3:
	cbnz	w1, 1f
	mov	v3.d[0], x2
	b	.Lset_end
1:	mov	v3.d[1], x2
	b	.Lset_end

.Lset4:
	cbnz	w1, 1f
	mov	v4.d[0], x2
	b	.Lset_end
1:	mov	v4.d[1], x2
	b	.Lset_end

.Lset5:
	cbnz	w1, 1f
	mov	v5.d[0], x2
	b	.Lset_end
1:	mov	v5.d[1], x2
	b	.Lset_end

.Lset6:
	cbnz	w1, 1f
	mov	v6.d[0], x2
	b	.Lset_end
1:	mov	v6.d[1], x2
	b	.Lset_end

.Lset7:
	cbnz	w1, 1f
	mov	v7.d[0], x2
	b	.Lset_end
1:	mov	v7.d[1], x2
	b	.Lset_end

.Lset8:
	cbnz	w1, 1f
	mov	v8.d[0], x2
	b	.Lset_end
1:	mov	v8.d[1], x2
	b	.Lset_end

.Lset9:
	cbnz	w1, 1f
	mov	v9.d[0], x2
	b	.Lset_end
1:	mov	v9.d[1], x2
	b	.Lset_end

.Lset10:
	cbnz	w1, 1f
	mov	v10.d[0], x2
	b	.Lset_end
1:	mov	v10.d[1], x2
	b	.Lset_end

.Lset11:
	cbnz	w1, 1f
	mov	v11.d[0], x2
	b	.Lset_end
1:	mov	v11.d[1], x2
	b	.Lset_end

.Lset12:
	cbnz	w1, 1f
	mov	v12.d[0], x2
	b	.Lset_end
1:	mov	v12.d[1], x2
	b	.Lset_end

.Lset13:
	cbnz	w1, 1f
	mov	v13.d[0], x2
	b	.Lset_end
1:	mov	v13.d[1], x2
	b	.Lset_end

.Lset14:
	cbnz	w1, 1f
	mov	v14.d[0], x2
	b	.Lset_end
1:	mov	v14.d[1], x2
	b	.Lset_end

.Lset15:
	cbnz	w1, 1f
	mov	v15.d[0], x2
	b	.Lset_end
1:	mov	v15.d[1], x2
	b	.Lset_end

.Lset16:
	cbnz	w1, 1f
	mov	v16.d[0], x2
	b	.Lset_end
1:	mov	v16.d[1], x2
	b	.Lset_end

.Lset17:
	cbnz	w1, 1f
	mov	v17.d[0], x2
	b	.Lset_end
1:	mov	v17.d[1], x2
	b	.Lset_end

.Lset18:
	cbnz	w1, 1f
	mov	v18.d[0], x2
	b	.Lset_end
1:	mov	v18.d[1], x2
	b	.Lset_end

.Lset19:
	cbnz	w1, 1f
	mov	v19.d[0], x2
	b	.Lset_end
1:	mov	v19.d[1], x2
	b	.Lset_end

.Lset20:
	cbnz	w1, 1f
	mov	v20.d[0], x2
	b	.Lset_end
1:	mov	v20.d[1], x2
	b	.Lset_end

.Lset21:
	cbnz	w1, 1f
	mov	v21.d[0], x2
	b	.Lset_end
1:	mov	v21.d[1], x2
	b	.Lset_end

.Lset22:
	cbnz	w1, 1f
	mov	v22.d[0], x2
	b	.Lset_end
1:	mov	v22.d[1], x2
	b	.Lset_end

.Lset23:
	cbnz	w1, 1f
	mov	v23.d[0], x2
	b	.Lset_end
1:	mov	v23.d[1], x2
	b	.Lset_end

.Lset24:
	cbnz	w1, 1f
	mov	v24.d[0], x2
	b	.Lset_end
1:	mov	v24.d[1], x2
	b	.Lset_end

.Lset25:
	cbnz	w1, 1f
	mov	v25.d[0], x2
	b	.Lset_end
1:	mov	v25.d[1], x2
	b	.Lset_end

.Lset26:
	cbnz	w1, 1f
	mov	v26.d[0], x2
	b	.Lset_end
1:	mov	v26.d[1], x2
	b	.Lset_end

.Lset27:
	cbnz	w1, 1f
	mov	v27.d[0], x2
	b	.Lset_end
1:	mov	v27.d[1], x2
	b	.Lset_end

.Lset28:
	cbnz	w1, 1f
	mov	v28.d[0], x2
	b	.Lset_end
1:	mov	v28.d[1], x2
	b	.Lset_end

.Lset29:
	cbnz	w1, 1f
	mov	v29.d[0], x2
	b	.Lset_end
1:	mov	v29.d[1], x2
	b	.Lset_end

.Lset30:
	cbnz	w1, 1f
	mov	v30.d[0], x2
	b	.Lset_end
1:	mov	v30.d[1], x2
	b	.Lset_end

.Lset31:
	cbnz	w1, 1f
	mov	v31.d[0], x2
	b	.Lset_end
1:	mov	v31.d[1], x2

.Lset_end:
	ret
SYM_FUNC_END(rk_align_set_data_asm)
