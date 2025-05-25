# SPDX-License-Identifier: GPL-2.0+
obj-$(CONFIG_CRYPTO_DEV_ROCKCHIP) += rk_ce.o

$(obj)/rkce_sm2signature.asn1.o: \
	$(obj)/rkce_sm2signature.asn1.c $(obj)/rkce_sm2signature.asn1.h
$(obj)/rkce_ecdsasignature.asn1.o: \
	$(obj)/rkce_ecdsasignature.asn1.c $(obj)/rkce_ecdsasignature.asn1.h
$(obj)/rkce_akcipher.o: \
	$(obj)/rkce_sm2signature.asn1.h $(obj)/rkce_ecdsasignature.asn1.h

rk_ce-objs := rkce_core.o \
	      rkce_dev.o \
	      rkce_akcipher.o \
	      rkce_skcipher.o \
	      rkce_hash.o \
	      rkce_buf.o \
	      rkce_bignum.o \
	      rkce_pka.o \
	      rkce_ecc.o \
	      rkce_sm2signature.asn1.o \
	      rkce_ecdsasignature.asn1.o \
	      rkce_debug.o \
	      rkce_monitor.o
