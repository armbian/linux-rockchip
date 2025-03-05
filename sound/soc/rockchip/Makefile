# SPDX-License-Identifier: GPL-2.0
# ROCKCHIP Platform Support
snd-soc-rockchip-y := rockchip_utils.o
snd-soc-rockchip-asrc-y := rockchip_asrc.o
snd-soc-rockchip-dlp-y := rockchip_dlp.o
snd-soc-rockchip-dlp-pcm-y := rockchip_dlp_pcm.o
snd-soc-rockchip-dummy-dai-y := rockchip_dummy_dai.o
snd-soc-rockchip-i2s-y := rockchip_i2s.o
snd-soc-rockchip-i2s-tdm-y := rockchip_i2s_tdm.o
snd-soc-rockchip-multi-dais-y := rockchip_multi_dais.o rockchip_multi_dais_pcm.o
snd-soc-rockchip-pdm-y := rockchip_pdm.o
snd-soc-rockchip-pdm-v2-y := rockchip_pdm_v2.o
snd-soc-rockchip-sai-y := rockchip_sai.o
snd-soc-rockchip-spdif-y := rockchip_spdif.o
snd-soc-rockchip-spdifrx-y := rockchip_spdifrx.o
snd-soc-rockchip-trcm-y := rockchip_trcm.o
snd-soc-rockchip-vad-y := rockchip_vad.o
ifdef CONFIG_THUMB2_KERNEL
snd-soc-rockchip-vad-$(CONFIG_THUMB2_KERNEL) += vad_preprocess_thumb.o
else
snd-soc-rockchip-vad-$(CONFIG_ARM64) += vad_preprocess_arm64.o
snd-soc-rockchip-vad-$(CONFIG_ARM) += vad_preprocess_arm.o
endif

obj-$(CONFIG_SND_SOC_ROCKCHIP) += snd-soc-rockchip.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_ASRC) += snd-soc-rockchip-asrc.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_DLP) += snd-soc-rockchip-dlp.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_DLP_PCM) += snd-soc-rockchip-dlp-pcm.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_DUMMY_DAI) += snd-soc-rockchip-dummy-dai.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_I2S) += snd-soc-rockchip-i2s.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_I2S_TDM) += snd-soc-rockchip-i2s-tdm.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_MULTI_DAIS) += snd-soc-rockchip-multi-dais.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_PDM) += snd-soc-rockchip-pdm.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_PDM_V2) += snd-soc-rockchip-pdm-v2.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_SAI) += snd-soc-rockchip-sai.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_SPDIF) += snd-soc-rockchip-spdif.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_SPDIFRX) += snd-soc-rockchip-spdifrx.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_TRCM) += snd-soc-rockchip-trcm.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_VAD) += snd-soc-rockchip-vad.o

snd-soc-rockchip-hdmi-y := rockchip_hdmi.o
snd-soc-rockchip-max98090-y := rockchip_max98090.o
snd-soc-rockchip-multicodecs-y := rockchip_multicodecs.o
snd-soc-rockchip-rt5645-y := rockchip_rt5645.o
snd-soc-rk3288-hdmi-analog-y := rk3288_hdmi_analog.o
snd-soc-rk3399-gru-sound-y := rk3399_gru_sound.o

obj-$(CONFIG_SND_SOC_ROCKCHIP_HDMI) += snd-soc-rockchip-hdmi.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_MAX98090) += snd-soc-rockchip-max98090.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_MULTICODECS) += snd-soc-rockchip-multicodecs.o
obj-$(CONFIG_SND_SOC_ROCKCHIP_RT5645) += snd-soc-rockchip-rt5645.o
obj-$(CONFIG_SND_SOC_RK3288_HDMI_ANALOG) += snd-soc-rk3288-hdmi-analog.o
obj-$(CONFIG_SND_SOC_RK3399_GRU_SOUND) += snd-soc-rk3399-gru-sound.o
