// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Wyon Bi <bivvy.bi@rock-chips.com>
 */

#include <linux/debugfs.h>

#include "asm-generic/errno-base.h"
#include "rk628.h"
#include "rk628_cru.h"

#define REFCLK_RATE		24000000UL
#define MIN_FREF_RATE		10000000UL
#define MAX_FREF_RATE		800000000UL
#define MIN_FREFDIV_RATE	1000000UL
#define MAX_FREFDIV_RATE	100000000UL
#define MIN_FVCO_RATE		600000000UL
#define MAX_FVCO_RATE		1600000000UL
#define MIN_FOUTPOSTDIV_RATE	12000000UL
#define MAX_FOUTPOSTDIV_RATE	1600000000UL

static void rational_best_approximation(unsigned long given_numerator,
					unsigned long given_denominator,
					unsigned long max_numerator,
					unsigned long max_denominator,
					unsigned long *best_numerator,
					unsigned long *best_denominator)
{
	unsigned long n, d, n0, d0, n1, d1;

	n = given_numerator;
	d = given_denominator;
	n0 = d1 = 0;
	n1 = d0 = 1;
	for (;;) {
		unsigned long t, a;

		if ((n1 > max_numerator) || (d1 > max_denominator)) {
			n1 = n0;
			d1 = d0;
			break;
		}
		if (d == 0)
			break;
		t = d;
		a = n / d;
		d = n % d;
		n = t;
		t = n0 + a * n1;
		n0 = n1;
		n1 = t;
		t = d0 + a * d1;
		d0 = d1;
		d1 = t;
	}
	*best_numerator = n1;
	*best_denominator = d1;
}

static unsigned long rk628_cru_clk_get_rate_pll(struct rk628 *rk628,
						unsigned int id)
{
	unsigned long parent_rate = REFCLK_RATE;
	u32 postdiv1, fbdiv, dsmpd, postdiv2, refdiv, frac, bypass;
	u32 con0, con1, con2;
	u64 foutvco, foutpostdiv;
	u32 offset, val;

	if (id == CGU_CLK_APLL && rk628->version < RK628F_VERSION)
		return 0;

	rk628_i2c_read(rk628, CRU_MODE_CON00, &val);
	if (id == CGU_CLK_CPLL) {
		val &= CLK_CPLL_MODE_MASK;
		val >>= CLK_CPLL_MODE_SHIFT;
		if (val == CLK_CPLL_MODE_OSC)
			return parent_rate;

		offset = 0x00;
	} else if (id == CGU_CLK_GPLL) {
		val &= CLK_GPLL_MODE_MASK;
		val >>= CLK_GPLL_MODE_SHIFT;
		if (val == CLK_GPLL_MODE_OSC)
			return parent_rate;

		offset = 0x20;
	} else {
		val &= CLK_APLL_MODE_MASK;
		val >>= CLK_APLL_MODE_SHIFT;
		if (val == CLK_APLL_MODE_OSC)
			return parent_rate;

		offset = 0x40;
	}

	rk628_i2c_read(rk628, offset + CRU_CPLL_CON0, &con0);
	rk628_i2c_read(rk628, offset + CRU_CPLL_CON1, &con1);
	rk628_i2c_read(rk628, offset + CRU_CPLL_CON2, &con2);

	bypass = (con0 & PLL_BYPASS_MASK) >> PLL_BYPASS_SHIFT;
	postdiv1 = (con0 & PLL_POSTDIV1_MASK) >> PLL_POSTDIV1_SHIFT;
	fbdiv = (con0 & PLL_FBDIV_MASK) >> PLL_FBDIV_SHIFT;
	dsmpd = (con1 & PLL_DSMPD_MASK) >> PLL_DSMPD_SHIFT;
	postdiv2 = (con1 & PLL_POSTDIV2_MASK) >> PLL_POSTDIV2_SHIFT;
	refdiv = (con1 & PLL_REFDIV_MASK) >> PLL_REFDIV_SHIFT;
	frac = (con2 & PLL_FRAC_MASK) >> PLL_FRAC_SHIFT;

	if (bypass)
		return parent_rate;

	foutvco = parent_rate * fbdiv;
	do_div(foutvco, refdiv);

	if (!dsmpd) {
		u64 frac_rate = (u64)parent_rate * frac;

		do_div(frac_rate, refdiv);
		foutvco += frac_rate >> 24;
	}

	foutpostdiv = foutvco;
	do_div(foutpostdiv, postdiv1);
	do_div(foutpostdiv, postdiv2);

	return foutpostdiv;
}

static unsigned long rk628_cru_clk_set_rate_pll(struct rk628 *rk628,
						unsigned int id,
						unsigned long rate)
{
	unsigned long fin = REFCLK_RATE, fout = rate;
	u8 min_refdiv, max_refdiv, postdiv;
	u8 dsmpd = 1, postdiv1 = 0, postdiv2 = 0, refdiv = 0;
	u16 fbdiv = 0;
	u32 frac = 0;
	u64 foutvco, foutpostdiv, div1, div2;
	u32 offset, val;

	/*
	 * FREF : 10MHz ~ 800MHz
	 * FREFDIV : 1MHz ~ 40MHz
	 * FOUTVCO : 400MHz ~ 1.6GHz
	 * FOUTPOSTDIV : 8MHz ~ 1.6GHz
	 */
	if (fin < MIN_FREF_RATE || fin > MAX_FREF_RATE)
		return 0;

	if (fout < MIN_FOUTPOSTDIV_RATE || fout > MAX_FOUTPOSTDIV_RATE)
		return 0;

	if (id == CGU_CLK_CPLL)
		offset = 0x00;
	else if (id == CGU_CLK_GPLL)
		offset = 0x20;
	else
		offset = 0x40;

	if (id != CGU_CLK_APLL)
		rk628_i2c_write(rk628, offset + CRU_CPLL_CON1, PLL_PD(1));

	if (fin == fout) {
		rk628_i2c_write(rk628, offset + CRU_CPLL_CON0, PLL_BYPASS(1));
		rk628_i2c_write(rk628, offset + CRU_CPLL_CON1, PLL_PD(0));
		while (1) {
			rk628_i2c_read(rk628, offset + CRU_CPLL_CON1, &val);
			if (val & PLL_LOCK)
				break;
		}
		return fin;
	}

	min_refdiv = fin / MAX_FREFDIV_RATE + 1;
	max_refdiv = fin / MIN_FREFDIV_RATE;
	if (max_refdiv > 64)
		max_refdiv = 64;

	if (fout < MIN_FVCO_RATE) {
		div1 = DIV_ROUND_UP(MIN_FVCO_RATE, fout);
		div2 = DIV_ROUND_UP(MAX_FVCO_RATE, fout);
		for (postdiv = div1; postdiv <= div2; postdiv++) {
			/* fix prime number that can not find right div*/
			for (postdiv2 = 1; postdiv2 < 8; postdiv2++) {
				if (postdiv % postdiv2)
					continue;
				postdiv1 = postdiv / postdiv2;
				if (postdiv1 > 0 && postdiv1 < 8)
					break;
			}
			if (postdiv2 > 7)
				continue;
			else
				break;
		}
		if (postdiv > div2)
			return 0;
		fout *= postdiv1 * postdiv2;
	} else {
		postdiv1 = 1;
		postdiv2 = 1;
	}

	for (refdiv = min_refdiv; refdiv <= max_refdiv; refdiv++) {
		u64 tmp, frac_rate;

		if (fin % refdiv)
			continue;

		tmp = (u64)fout * refdiv;
		do_div(tmp, fin);
		fbdiv = tmp;
		if (fbdiv < 10 || fbdiv > 1600)
			continue;

		tmp = (u64)fbdiv * fin;
		do_div(tmp, refdiv);
		if (fout < MIN_FVCO_RATE || fout > MAX_FVCO_RATE)
			continue;

		frac_rate = fout - tmp;

		if (frac_rate) {
			tmp = (u64)frac_rate * refdiv;
			tmp <<= 24;
			do_div(tmp, fin);
			frac = tmp;
			dsmpd = 0;
		}

		break;
	}

	/*
	 * If DSMPD = 1 (DSM is disabled, "integer mode")
	 * FOUTVCO = FREF / REFDIV * FBDIV
	 * FOUTPOSTDIV = FOUTVCO / POSTDIV1 / POSTDIV2
	 *
	 * If DSMPD = 0 (DSM is enabled, "fractional mode")
	 * FOUTVCO = FREF / REFDIV * (FBDIV + FRAC / 2^24)
	 * FOUTPOSTDIV = FOUTVCO / POSTDIV1 / POSTDIV2
	 */
	foutvco = fin * fbdiv;
	do_div(foutvco, refdiv);

	if (!dsmpd) {
		u64 frac_rate = (u64)fin * frac;

		do_div(frac_rate, refdiv);
		foutvco += frac_rate >> 24;
	}

	foutpostdiv = foutvco;
	do_div(foutpostdiv, postdiv1);
	do_div(foutpostdiv, postdiv2);

	rk628_i2c_write(rk628, offset + CRU_CPLL_CON0,
			PLL_BYPASS(0) | PLL_POSTDIV1(postdiv1) |
			PLL_FBDIV(fbdiv));
	rk628_i2c_write(rk628, offset + CRU_CPLL_CON1,
			PLL_DSMPD(dsmpd) | PLL_POSTDIV2(postdiv2) |
			PLL_REFDIV(refdiv));
	rk628_i2c_write(rk628, offset + CRU_CPLL_CON2, PLL_FRAC(frac));

	if (id != CGU_CLK_APLL)
		rk628_i2c_write(rk628, offset + CRU_CPLL_CON1, PLL_PD(0));

	while (1) {
		rk628_i2c_read(rk628, offset + CRU_CPLL_CON1, &val);
		if (val & PLL_LOCK)
			break;
	}

	return (unsigned long)foutpostdiv;
}

static int rk628_cru_clk_get_parent_rate(struct rk628 *rk628, unsigned int id,
					 unsigned int *parent_id,
					 unsigned long *parent_rate)
{
	u32 val;
	int parent = -1;

	switch (id) {
	case CGU_CLK_RX_READ:
		rk628_i2c_read(rk628, CRU_CLKSEL_CON02, &val);
		val &= CLK_RX_READ_SEL_MASK;
		val >>= CLK_RX_READ_SEL_SHIFT;
		parent = val == CLK_RX_READ_SEL_GPLL ? CGU_CLK_GPLL : CGU_CLK_CPLL;
		break;
	case CGU_SCLK_VOP:
		rk628_i2c_read(rk628, CRU_CLKSEL_CON02, &val);
		val &= CLK_UART_SRC_SEL_MASK;
		val >>= SCLK_VOP_SEL_SHIFT;
		parent = val == SCLK_VOP_SEL_GPLL ? CGU_CLK_GPLL : CGU_CLK_CPLL;
		break;
	case CGU_CLK_UART_SRC:
		rk628_i2c_read(rk628, CRU_CLKSEL_CON21, &val);
		val &= SCLK_VOP_SEL_MASK;
		parent = val == CLK_UART_SRC_SEL_GPLL ? CGU_CLK_GPLL : CGU_CLK_CPLL;
		break;
	case CGU_BT1120DEC:
		rk628_i2c_read(rk628, CRU_CLKSEL_CON02, &val);
		val &= CLK_BT1120DEC_SEL_MASK;
		parent = val == CLK_BT1120DEC_SEL_GPLL ? CGU_CLK_GPLL : CGU_CLK_CPLL;
		break;
	case CGU_CLK_HDMIRX_AUD:
		rk628_i2c_read(rk628, CRU_CLKSEL_CON05, &val);
		if (rk628->version >= RK628F_VERSION)
			val = (val & CLK_HDMIRX_AUD_SEL_MASK_V2) >> 14;
		else
			val = (val & CLK_HDMIRX_AUD_SEL_MASK_V1) >> 15;
		switch (val) {
		case 0:
			parent = CGU_CLK_CPLL;
			break;
		case 1:
			parent = CGU_CLK_GPLL;
			break;
		case 2:
			parent = CGU_CLK_APLL;
		}
		break;
	case CGU_CLK_IMODET:
		rk628_i2c_read(rk628, CRU_CLKSEL_CON05, &val);
		val &= CLK_IMODET_SEL_MASK;
		val >>= CLK_IMODET_SEL_SHIFT;
		parent = val == SCLK_VOP_SEL_GPLL ? CGU_CLK_GPLL : CGU_CLK_CPLL;
		break;
	default:
		return -EINVAL;
	}

	if (parent < 0)
		return -EINVAL;

	if (parent_id)
		*parent_id = parent;

	if (parent_rate)
		*parent_rate = rk628_cru_clk_get_rate(rk628, parent);

	return 0;
}

static unsigned long rk628_cru_clk_set_rate_sclk_vop(struct rk628 *rk628,
						     unsigned long rate)
{
	unsigned long m, n, parent_rate;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_SCLK_VOP,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rational_best_approximation(rate, parent_rate,
				    GENMASK(15, 0), GENMASK(15, 0),
				    &m, &n);
	rk628_i2c_write(rk628, CRU_CLKSEL_CON13, m << 16 | n);

	return rate;
}

static unsigned long rk628_cru_clk_get_rate_sclk_vop(struct rk628 *rk628)
{
	unsigned long rate, parent_rate, m, n;
	u32 div;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_SCLK_VOP,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rk628_i2c_read(rk628, CRU_CLKSEL_CON13, &div);
	m = div >> 16 & 0xffff;
	n = div & 0xffff;
	rate = parent_rate * m / n;

	return rate;
}

static unsigned long rk628_cru_clk_get_rate_clk_imodet(struct rk628 *rk628)
{
	unsigned long rate, parent_rate, n;
	u32 div;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_CLK_IMODET,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rk628_i2c_read(rk628, CRU_CLKSEL_CON05, &div);
	n = div & 0x1f;
	rate = parent_rate / (n + 1);

	return rate;
}

static unsigned long rk628_cru_clk_set_rate_rx_read(struct rk628 *rk628,
						    unsigned long rate)
{
	unsigned long m, n, parent_rate;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_CLK_RX_READ,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rational_best_approximation(rate, parent_rate,
				    GENMASK(15, 0), GENMASK(15, 0),
				    &m, &n);
	rk628_i2c_write(rk628, CRU_CLKSEL_CON14, m << 16 | n);

	return rate;
}

static unsigned long rk628_cru_clk_get_rate_rx_read(struct rk628 *rk628)
{
	unsigned long rate, m, n, parent_rate;
	u32 div;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_CLK_RX_READ,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rk628_i2c_read(rk628, CRU_CLKSEL_CON14, &div);
	m = div >> 16 & 0xffff;
	n = div & 0xffff;
	rate = parent_rate * m / n;

	return rate;
}

static unsigned long rk628_cru_clk_get_rate_uart_src(struct rk628 *rk628)
{
	unsigned long rate, parent_rate;
	u32 div;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_CLK_UART_SRC,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rk628_i2c_read(rk628, CRU_CLKSEL_CON21, &div);
	div &= CLK_UART_SRC_DIV_MASK;
	div >>= CLK_UART_SRC_DIV_SHIFT;
	rate = parent_rate / (div + 1);

	return rate;
}

static unsigned long rk628_cru_clk_set_rate_sclk_uart(struct rk628 *rk628,
						      unsigned long rate)
{
	unsigned long m, n, parent_rate;

	parent_rate = rk628_cru_clk_get_rate_uart_src(rk628);

	if (rate == REFCLK_RATE) {
		rk628_i2c_write(rk628, CRU_CLKSEL_CON06,
				SCLK_UART_SEL(SCLK_UART_SEL_OSC));
		return rate;
	} else if (rate == parent_rate) {
		rk628_i2c_write(rk628, CRU_CLKSEL_CON06,
				SCLK_UART_SEL(SCLK_UART_SEL_UART_SRC));
		return rate;
	}

	rk628_i2c_write(rk628, CRU_CLKSEL_CON06,
			SCLK_UART_SEL(SCLK_UART_SEL_UART_FRAC));

	rational_best_approximation(rate, parent_rate,
				    GENMASK(15, 0), GENMASK(15, 0),
				    &m, &n);
	rk628_i2c_write(rk628, CRU_CLKSEL_CON20, m << 16 | n);

	return rate;
}

static unsigned long rk628_cru_clk_set_rate_sclk_hdmirx_aud(struct rk628 *rk628, unsigned long rate)
{
	u64 parent_rate;
	u8 div;

	if (rk628->version >= RK628F_VERSION)
		parent_rate = rk628_cru_clk_set_rate_pll(rk628, CGU_CLK_APLL, rate*4);
	else
		parent_rate = rk628_cru_clk_set_rate_pll(rk628, CGU_CLK_GPLL, rate*4);
	div = DIV_ROUND_CLOSEST_ULL(parent_rate, rate);
	do_div(parent_rate, div);
	rate = parent_rate;
	if (rk628->version >= RK628F_VERSION)
		rk628_i2c_write(rk628, CRU_CLKSEL_CON05, CLK_HDMIRX_AUD_DIV(div - 1) |
				CLK_HDMIRX_AUD_SEL_V2(2));
	else
		rk628_i2c_write(rk628, CRU_CLKSEL_CON05, CLK_HDMIRX_AUD_DIV(div - 1) |
				CLK_HDMIRX_AUD_SEL_V1(1));
	return rate;
}

static unsigned long rk628_cru_clk_get_rate_sclk_hdmirx_aud(struct rk628 *rk628)
{
	unsigned long rate, parent_rate;
	u32 div;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_CLK_HDMIRX_AUD,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rk628_i2c_read(rk628, CRU_CLKSEL_CON05, &div);
	div = ((div & CLK_HDMIRX_AUD_DIV_MASK) >> 6) + 1;
	rate = parent_rate / div;

	return rate;
}

static unsigned long rk628_cru_clk_set_rate_bt1120_dec(struct rk628 *rk628,
						       unsigned long rate)
{
	unsigned long parent_rate;
	u32 div;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_BT1120DEC,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	div = DIV_ROUND_UP(parent_rate, rate);
	rk628_i2c_write(rk628, CRU_CLKSEL_CON02, CLK_BT1120DEC_DIV(div-1));

	return parent_rate / div;
}

static unsigned long rk628_cru_clk_get_rate_bt1120_dec(struct rk628 *rk628)
{
	unsigned long parent_rate;
	u32 div;
	int ret;

	ret = rk628_cru_clk_get_parent_rate(rk628, CGU_BT1120DEC,
					    NULL, &parent_rate);
	if (ret)
		return 0;

	rk628_i2c_read(rk628, CRU_CLKSEL_CON02, &div);
	div = (div & 0x1f) + 1;

	return parent_rate / div;
}

int rk628_cru_clk_set_rate(struct rk628 *rk628, unsigned int id,
			   unsigned long rate)
{
	if (id == CGU_CLK_APLL && rk628->version < RK628F_VERSION)
		return -EINVAL;

	switch (id) {
	case CGU_CLK_APLL:
	case CGU_CLK_CPLL:
	case CGU_CLK_GPLL:
		rk628_cru_clk_set_rate_pll(rk628, id, rate);
		break;
	case CGU_CLK_RX_READ:
		rk628_cru_clk_set_rate_rx_read(rk628, rate);
		break;
	case CGU_SCLK_VOP:
		rk628_cru_clk_set_rate_sclk_vop(rk628, rate);
		break;
	case CGU_SCLK_UART:
		rk628_cru_clk_set_rate_sclk_uart(rk628, rate);
		break;
	case CGU_BT1120DEC:
		rk628_cru_clk_set_rate_bt1120_dec(rk628, rate);
		break;
	case CGU_CLK_HDMIRX_AUD:
		rk628_cru_clk_set_rate_sclk_hdmirx_aud(rk628, rate);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

unsigned long rk628_cru_clk_get_rate(struct rk628 *rk628, unsigned int id)
{
	unsigned long rate;

	if (id == CGU_CLK_APLL && rk628->version < RK628F_VERSION)
		return 0;

	switch (id) {
	case CGU_CLK_APLL:
	case CGU_CLK_CPLL:
	case CGU_CLK_GPLL:
		rate = rk628_cru_clk_get_rate_pll(rk628, id);
		break;
	case CGU_CLK_RX_READ:
		rate = rk628_cru_clk_get_rate_rx_read(rk628);
		break;
	case CGU_SCLK_VOP:
		rate = rk628_cru_clk_get_rate_sclk_vop(rk628);
		break;
	case CGU_CLK_IMODET:
		rate = rk628_cru_clk_get_rate_clk_imodet(rk628);
		break;
	case CGU_CLK_HDMIRX_AUD:
		rate = rk628_cru_clk_get_rate_sclk_hdmirx_aud(rk628);
		break;
	case CGU_BT1120DEC:
		rate = rk628_cru_clk_get_rate_bt1120_dec(rk628);
		break;
	default:
		return 0;
	}

	return rate;
}

static void rk628_cru_show_pll_tree(struct seq_file *s, unsigned int parent_id,
				    const char *parent_name)
{
	struct rk628 *rk628 = s->private;
	unsigned long rate;
	unsigned int parent, i;
	unsigned int id_list[] = {
		CGU_CLK_RX_READ,
		CGU_SCLK_VOP,
		CGU_BT1120DEC,
		CGU_CLK_HDMIRX_AUD,
		CGU_CLK_IMODET
	};
	char const *id_name[] = {
		"clk_rx_read",
		"clk_sclk_vop",
		"clk_bt1120dec",
		"clk_hdmirx_aud",
		"clk_imodet"
	};

	if (rk628->version < RK628F_VERSION && parent_id == CGU_CLK_APLL)
		return;

	rate = rk628_cru_clk_get_rate(rk628, parent_id);
	seq_printf(s, "%-22s %10lu\n", parent_name, rate);

	for (i = 0; i < ARRAY_SIZE(id_list); ++i) {
		rk628_cru_clk_get_parent_rate(rk628, id_list[i], &parent, NULL);
		if (parent != parent_id)
			continue;
		rate = rk628_cru_clk_get_rate(rk628, id_list[i]);
		seq_printf(s, "    %-18s %10lu\n", id_name[i], rate);
	}
}

static int rk628_cru_show_clk_tree(struct seq_file *s, void *data)
{
	unsigned int pll_list[] = {CGU_CLK_CPLL, CGU_CLK_GPLL, CGU_CLK_APLL};
	char const *pll_name[] = {"cpll", "gpll", "apll"};
	unsigned int i;

	seq_printf(s, "%-22s %10s\n", "  clock", "rate  ");
	seq_puts(s, "---------------------------------\n");

	for (i = 0; i < ARRAY_SIZE(pll_list); ++i)
		rk628_cru_show_pll_tree(s, pll_list[i], pll_name[i]);

	return 0;
}

static int rk628_clk_summary_open(struct inode *inode, struct file *file)
{
	struct rk628 *rk628 = inode->i_private;

	return single_open(file, rk628_cru_show_clk_tree, rk628);
}

static const struct file_operations rk628_clk_summary_fops = {
	.owner = THIS_MODULE,
	.open = rk628_clk_summary_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void rk628_cru_create_debugfs_file(struct rk628 *rk628)
{
	debugfs_create_file("clk_summary", 0400, rk628->debug_dir, rk628,
			    &rk628_clk_summary_fops);
}

void rk628_cru_init(struct rk628 *rk628)
{
	u32 val;
	u8 mcu_mode;

	/*
	 * In rk628d application, if you need to dynamically tune the cpll
	 * frequency, you need to mount pclk under gpll, otherwise it will
	 * affect the i2c use. The bt1120rx only supports 5bit integer crossover
	 * frequency, in order to crossover frequency accurately, you need to
	 * adjust the cpll frequency dynamically, so in the scenario of rk628d
	 * bt1120rx, mount the pclk under gpll.
	 */
	rk628_i2c_read(rk628, GRF_SYSTEM_STATUS0, &val);
	mcu_mode = (val & I2C_ONLY_FLAG) ? 0 : 1;
	if (mcu_mode || rk628->version >= RK628F_VERSION) {
		rk628_i2c_write(rk628, CRU_MODE_CON00, HIWORD_UPDATE(1, 4, 4));
		/*
		 * rk628d pclk use cpll by default, and frequency is 99MHz
		 * rk628f pclk use gpll by default, and frequency is 98.304MHz
		 */
		if (rk628_input_is_bt1120(rk628)) {
			/* set pclk use gpll, and set pclk 98.304MHz */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff0089);
		}
	} else {
		/* clock switch and first set gpll almost 99MHz */
		rk628_i2c_write(rk628, CRU_GPLL_CON0, 0xffff701d);
		mdelay(1);
		/* set clk_gpll_mux from gpll */
		rk628_i2c_write(rk628, CRU_MODE_CON00, 0xffff0004);
		mdelay(1);
		rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff0080);
		/* set pclk use gpll, now div is 4 */
		rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff0083);
		/* set cpll almost 400MHz */
		rk628_i2c_write(rk628, CRU_CPLL_CON0, 0xffff3063);
		mdelay(1);
		/* set clk_cpll_mux from clk_cpll */
		rk628_i2c_write(rk628, CRU_MODE_CON00, 0xffff0005);
		mdelay(1);
		if (rk628_input_is_bt1120(rk628)) {
			/* set pclk use cpll, now div is 4 */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff0003);
			/* set pclk use cpll, now div is 10 */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff0009);
			/* set gpll 983.04MHz */
			rk628_i2c_write(rk628, CRU_GPLL_CON0, 0xffff1028);
			mdelay(1);
			/* set pclk use gpll, now div is 10 */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff0089);
			/* set cpll 1188MHz */
			rk628_i2c_write(rk628, CRU_CPLL_CON0, 0xffff1063);
			/* final: cpll 1188MHz, gpll 983.04MHz, pclk (use gpll) 98.304MHz */
		} else {
			/* set pclk use cpll, now div is 4 */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff0003);
			/* set pclk use cpll, now div is 12 */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff000b);
			/* set gpll 983.04MHz */
			rk628_i2c_write(rk628, CRU_GPLL_CON0, 0xffff1028);
			mdelay(1);
			/* set pclk use gpll, now div is 12 */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff008b);
			/* set cpll 1188MHz */
			rk628_i2c_write(rk628, CRU_CPLL_CON0, 0xffff1063);
			mdelay(1);
			/* set pclk use cpll, now div is 12 */
			rk628_i2c_write(rk628, CRU_CLKSEL_CON00, 0x00ff000b);
			/* final: cpll 1188MHz, gpll 983.04MHz, pclk (use cpll) 99MHz */
		}
	}

	/*
	 * The sclk_vop frequency default is 594M, which exceeds the reference
	 * clock frequency acceptable by hdmitx phy. Therefore, in the hdmitx
	 * scenario, we need to set the initial frequency of the sclk_vop to a
	 * lower frequency, which is set to 148.5M.
	 */
	if (rk628_output_is_hdmi(rk628))
		rk628_cru_clk_set_rate(rk628, CGU_SCLK_VOP, 148500000);
}

void rk628_cru_clk_adjust(struct rk628 *rk628)
{
	struct rk628_display_mode *src = &rk628->src_mode;
	const struct rk628_display_mode *dst = &rk628->dst_mode;
	u64 dst_rate, src_rate;
	unsigned long dec_clk_rate;
	u32 val;

	/*
	 * Try to keep cpll frequency close to 1188m (Tested bt1120rx and rk628f
	 * hdmirx scenarios)
	 */
	if ((rk628_input_is_hdmi(rk628) && rk628->version != RK628D_VERSION) ||
	    rk628_input_is_bt1120(rk628)) {
		val = 1188000000UL / (src->clock * 1000);
		if (rk628_input_is_bt1120(rk628) && val > (CLK_BT1120DEC_DIV_MAX + 1))
			val = CLK_BT1120DEC_DIV_MAX + 1;
		val *= src->clock * 1000;
		rk628_cru_clk_set_rate(rk628, CGU_CLK_CPLL, val);
		msleep(50);
		dev_info(rk628->dev, "adjust cpll to %uHz", val);
	}

	/*
	 * BT1120 dec clk is a 5-bit integer division, which is inaccurate in
	 * most resolutions. So if the frequency division is not accurate, apply
	 * for a fault tolerance of up 2% in frequency setting, so that the
	 * obtained frequency is slightly higher than the actual required clk,
	 * so that the deviation between the actual clk and the required clk
	 * frequency is not significant.
	 */
	if (rk628_input_is_bt1120(rk628)) {
		rk628_cru_clk_set_rate(rk628, CGU_BT1120DEC, src->clock * 1000);
		dec_clk_rate = rk628_cru_clk_get_rate(rk628, CGU_BT1120DEC);
		if (dec_clk_rate < src->clock * 1000)
			rk628_cru_clk_set_rate(rk628, CGU_BT1120DEC, src->clock * 1020);
	}

	src_rate = src->clock * 1000;
	dst_rate = src_rate * dst->vtotal * dst->htotal;
	do_div(dst_rate, (src->vtotal * src->htotal));
	do_div(dst_rate, 1000);
	dev_info(rk628->dev, "src %dx%d clock:%d\n",
		 src->hdisplay, src->vdisplay, src->clock);

	dev_info(rk628->dev, "dst %dx%d clock:%llu\n",
		 dst->hdisplay, dst->vdisplay, dst_rate);

	rk628_cru_clk_set_rate(rk628, CGU_CLK_RX_READ, src->clock * 1000);
	rk628_cru_clk_set_rate(rk628, CGU_SCLK_VOP, dst_rate * 1000);
}
