#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2025 Rockchip Electronics Co., Ltd.
# ==============================================================================
# Script Name: dts_xlate_rv1126_to_rv1126b.sh
# Version: v2.3
# Description: RV1126 to RV1126B Device Tree Translation Tool
# Features:
#   1. Debug mode support for viewing preprocessed mappings (-d/--debug)
#   2. Auto-skip processed files and base definition files
#   3. Full hardware definition mapping (GPIO/controllers/pinctrl)
# Usage:
#   Normal mode: ./dts_xlate_rv1126_to_rv1126b.sh [file list]
#   Debug mode:  ./dts_xlate_rv1126_to_rv1126b.sh -d
# ==============================================================================

# ------------------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------------------

# File processing markers
declare -r MARKER="DTS-XLATE-RV1126B"
declare -A COMMENT_MARKERS=(
    [".dts"]="/* ${MARKER} */"
    [".dtsi"]="/* ${MARKER} */"
)

# Intermediate replacement placeholder (prevent chained replacement conflicts)
declare -r PLACEHOLDER="@#@"

# Debug mode flag
declare DEBUG_MODE=false

# ------------------------------------------------------------------------------
# Hardware Definition Mapping (RV1126 -> RV1126B)
# ------------------------------------------------------------------------------
declare -A MAPPING=(
    # ----- GPIO Pin Definitions -----
    ["0 RK_PA3"]="0 RK_PA5"
    ["0 RK_PA4"]="0 RK_PA6"
    ["0 RK_PA5"]="0 RK_PA7"
    ["0 RK_PA6"]="0 RK_PB0"
    ["0 RK_PA7"]="0 RK_PB1"
    ["0 RK_PB0"]="0 RK_PB2"
    ["0 RK_PB1"]="0 RK_PC0"
    ["0 RK_PB2"]="0 RK_PC1"
    ["0 RK_PB4"]="0 RK_PC2"
    ["0 RK_PB5"]="0 RK_PC3"
    ["0 RK_PB6"]="0 RK_PC4"
    ["0 RK_PB7"]="0 RK_PC5"
    ["0 RK_PC0"]="0 RK_PC6"
    ["0 RK_PC1"]="0 RK_PC7"
    ["0 RK_PC2"]="0 RK_PD0"
    ["0 RK_PC3"]="0 RK_PD1"
    ["0 RK_PC4"]="1 RK_PA0"
    ["0 RK_PC5"]="1 RK_PA1"
    ["0 RK_PC6"]="1 RK_PA2"
    ["0 RK_PC7"]="1 RK_PA3"
    ["0 RK_PD0"]="1 RK_PA4"
    ["0 RK_PD1"]="1 RK_PA5"
    ["0 RK_PD2"]="1 RK_PA6"
    ["0 RK_PD3"]="1 RK_PA7"
    ["0 RK_PD4"]="1 RK_PB0"
    ["0 RK_PD5"]="1 RK_PB1"
    ["0 RK_PD6"]="1 RK_PB2"
    ["0 RK_PD7"]="1 RK_PB3"

    # ------------------ gpio1 ------------------
    ["1 RK_PA0"]="1 RK_PB4"
    ["1 RK_PA1"]="1 RK_PB5"
    ["1 RK_PA2"]="1 RK_PB6"
    ["1 RK_PA3"]="1 RK_PB7"
    ["1 RK_PA4"]="2 RK_PA0"
    ["1 RK_PA5"]="2 RK_PA1"
    ["1 RK_PA6"]="2 RK_PA2"
    ["1 RK_PA7"]="2 RK_PA3"
    ["1 RK_PB0"]="2 RK_PA4"
    ["1 RK_PB1"]="2 RK_PA5"
    ["1 RK_PB2"]="3 RK_PA0"
    ["1 RK_PB3"]="3 RK_PA1"
    ["1 RK_PB4"]="3 RK_PA2"
    ["1 RK_PB5"]="3 RK_PA3"
    ["1 RK_PB6"]="3 RK_PA4"
    ["1 RK_PB7"]="3 RK_PA5"
    ["1 RK_PC0"]="3 RK_PA6"
    ["1 RK_PC1"]="3 RK_PA7"
    ["1 RK_PC2"]="3 RK_PB0"
    ["1 RK_PC3"]="3 RK_PB1"
    ["1 RK_PC4"]="3 RK_PB2"
    ["1 RK_PC5"]="3 RK_PB3"
    ["1 RK_PC6"]="3 RK_PB4"
    ["1 RK_PC7"]="3 RK_PB5"
    ["1 RK_PD0"]="3 RK_PB6"
    ["1 RK_PD1"]="3 RK_PB7"
    ["1 RK_PD2"]="4 RK_PA0"
    ["1 RK_PD3"]="4 RK_PA1"
    ["1 RK_PD4"]="4 RK_PA2"
    ["1 RK_PD5"]="4 RK_PA3"
    ["1 RK_PD6"]="4 RK_PA4"
    ["1 RK_PD7"]="4 RK_PA5"

    # ------------------ gpio2 ------------------
    ["2 RK_PA0"]="4 RK_PA6"
    ["2 RK_PA1"]="4 RK_PA7"
    ["2 RK_PA2"]="4 RK_PB0"
    ["2 RK_PA3"]="4 RK_PB1"
    ["2 RK_PA4"]="5 RK_PA0"
    ["2 RK_PA5"]="5 RK_PA1"
    ["2 RK_PA6"]="5 RK_PA2"
    ["2 RK_PA7"]="5 RK_PA3"
    ["2 RK_PB0"]="5 RK_PA4"
    ["2 RK_PB1"]="5 RK_PA5"
    ["2 RK_PB2"]="5 RK_PA6"
    ["2 RK_PB3"]="5 RK_PA7"
    ["2 RK_PB4"]="5 RK_PB0"
    ["2 RK_PB5"]="5 RK_PB1"
    ["2 RK_PB6"]="5 RK_PB2"
    ["2 RK_PB7"]="5 RK_PB3"
    ["2 RK_PC0"]="5 RK_PB4"
    ["2 RK_PC1"]="5 RK_PB5"
    ["2 RK_PC2"]="5 RK_PB6"
    ["2 RK_PC3"]="5 RK_PB7"
    ["2 RK_PC4"]="5 RK_PC0"
    ["2 RK_PC5"]="5 RK_PC1"
    ["2 RK_PC6"]="5 RK_PC2"
    ["2 RK_PC7"]="5 RK_PC3"
    ["2 RK_PD0"]="5 RK_PC4"
    ["2 RK_PD1"]="5 RK_PC5"
    ["2 RK_PD2"]="5 RK_PC6"
    ["2 RK_PD3"]="5 RK_PC7"
    ["2 RK_PD4"]="5 RK_PD0"
    ["2 RK_PD5"]="5 RK_PD1"
    ["2 RK_PD6"]="5 RK_PD2"
    ["2 RK_PD7"]="5 RK_PD3"

    # ------------------ gpio3 ------------------
    ["3 RK_PA0"]="5 RK_PD4"
    ["3 RK_PA1"]="5 RK_PD5"
    ["3 RK_PA2"]="5 RK_PD6"
    ["3 RK_PA3"]="5 RK_PD7"
    ["3 RK_PA4"]="6 RK_PA0"
    ["3 RK_PA5"]="6 RK_PA1"
    ["3 RK_PA6"]="6 RK_PA2"
    ["3 RK_PA7"]="6 RK_PA3"
    ["3 RK_PB0"]="6 RK_PA4"
    ["3 RK_PB1"]="6 RK_PA5"
    ["3 RK_PB2"]="6 RK_PA6"
    ["3 RK_PB3"]="6 RK_PA7"
    ["3 RK_PB4"]="6 RK_PB0"
    ["3 RK_PB5"]="6 RK_PB1"
    ["3 RK_PB6"]="6 RK_PB2"
    ["3 RK_PB7"]="6 RK_PB3"
    ["3 RK_PC0"]="6 RK_PB4"
    ["3 RK_PC1"]="6 RK_PB5"
    ["3 RK_PC2"]="6 RK_PB6"
    ["3 RK_PC3"]="6 RK_PB7"
    ["3 RK_PC4"]="6 RK_PC0"
    ["3 RK_PC5"]="6 RK_PC1"
    ["3 RK_PC6"]="6 RK_PC2"
    ["3 RK_PC7"]="6 RK_PC3"
    ["3 RK_PD0"]="7 RK_PA0"
    ["3 RK_PD1"]="7 RK_PA1"
    ["3 RK_PD2"]="7 RK_PA2"
    ["3 RK_PD3"]="7 RK_PA3"
    ["3 RK_PD4"]="7 RK_PA4"
    ["3 RK_PD5"]="7 RK_PA5"
    ["3 RK_PD6"]="7 RK_PA6"
    ["3 RK_PD7"]="7 RK_PA7"

    # ------------------ gpio4 ------------------
    ["4 RK_PA0"]="7 RK_PB0"
    ["4 RK_PA1"]="7 RK_PB1"

    # ----- Controller Identifiers -----
    ["pwm0 "]="pwm0_8ch_0 "        # Trailing space prevents partial matches
    ["pwm1 "]="pwm0_8ch_1 "
    ["pwm2 "]="pwm0_8ch_2 "
    ["pwm3 "]="pwm0_8ch_3 "
    ["pwm4 "]="pwm0_8ch_4 "
    ["pwm5 "]="pwm0_8ch_5 "
    ["pwm6 "]="pwm0_8ch_6 "
    ["pwm7 "]="pwm0_8ch_7 "
    ["pwm8 "]="pwm1_4ch_0 "
    ["pwm9 "]="pwm1_4ch_1 "
    ["pwm10 "]="pwm1_4ch_2 "
    ["pwm11 "]="pwm1_4ch_3 "
    ["i2s0_8ch "]="sai0 "
    ["i2s1_2ch "]="sai1 "
    ["i2s2_2ch "]="sai2 "
    ["uart0 "]="uart2 "
    ["uart2 "]="uart0 "
    ["sfc "]="fspi0 "

    # ----- pinctrl Nodes -----
    ["pwm0m0_pins_pull_down"]="pwm0m0_ch0_pins"
    ["pwm1m0_pins_pull_down"]="pwm0m0_ch1_pins"
    ["pwm2m0_pins_pull_down"]="pwm0m0_ch2_pins"
    ["pwm3m0_pins_pull_down"]="pwm0m0_ch3_pins"
    ["pwm4m0_pins_pull_down"]="pwm0m0_ch4_pins"
    ["pwm5m0_pins_pull_down"]="pwm0m0_ch5_pins"
    ["pwm6m0_pins_pull_down"]="pwm0m0_ch6_pins"
    ["pwm7m0_pins_pull_down"]="pwm0m0_ch7_pins"
    ["pwm8m0_pins_pull_down"]="pwm1m2_ch0_pins"
    ["pwm9m0_pins_pull_down"]="pwm1m2_ch1_pins"
    ["pwm10m0_pins_pull_down"]="pwm1m2_ch2_pins"
    ["pwm11m0_pins_pull_down"]="pwm1m2_ch3_pins"
    ["pwm0m1_pins_pull_down"]="pwm0m1_ch0_pins"
    ["pwm1m1_pins_pull_down"]="pwm0m1_ch1_pins"
    ["pwm2m1_pins_pull_down"]="pwm0m1_ch2_pins"
    ["pwm3m1_pins_pull_down"]="pwm0m1_ch3_pins"
    ["pwm4m1_pins_pull_down"]="pwm0m2_ch4_pins"
    ["pwm5m1_pins_pull_down"]="pwm0m2_ch5_pins"
    ["pwm6m1_pins_pull_down"]="pwm0m2_ch6_pins"
    ["pwm7m1_pins_pull_down"]="pwm0m2_ch7_pins"
    ["pwm8m1_pins_pull_down"]="pwm1m1_ch0_pins"
    ["pwm9m1_pins_pull_down"]="pwm1m1_ch1_pins"
    ["pwm10m1_pins_pull_down"]="pwm1m1_ch2_pins"
    ["pwm11m1_pins_pull_down"]="pwm1m1_ch3_pins"
    ["pwm0m0_pins"]="pwm0m0_ch0_pins"
    ["pwm1m0_pins"]="pwm0m0_ch1_pins"
    ["pwm2m0_pins"]="pwm0m0_ch2_pins"
    ["pwm3m0_pins"]="pwm0m0_ch3_pins"
    ["pwm4m0_pins"]="pwm0m0_ch4_pins"
    ["pwm5m0_pins"]="pwm0m0_ch5_pins"
    ["pwm6m0_pins"]="pwm0m0_ch6_pins"
    ["pwm7m0_pins"]="pwm0m0_ch7_pins"
    ["pwm8m0_pins"]="pwm1m2_ch0_pins"
    ["pwm9m0_pins"]="pwm1m2_ch1_pins"
    ["pwm10m0_pins"]="pwm1m2_ch2_pins"
    ["pwm11m0_pins"]="pwm1m2_ch3_pins"
    ["pwm0m1_pins"]="pwm0m1_ch0_pins"
    ["pwm1m1_pins"]="pwm0m1_ch1_pins"
    ["pwm2m1_pins"]="pwm0m1_ch2_pins"
    ["pwm3m1_pins"]="pwm0m1_ch3_pins"
    ["pwm4m1_pins"]="pwm0m2_ch4_pins"
    ["pwm5m1_pins"]="pwm0m2_ch5_pins"
    ["pwm6m1_pins"]="pwm0m2_ch6_pins"
    ["pwm7m1_pins"]="pwm0m2_ch7_pins"
    ["pwm8m1_pins"]="pwm1m1_ch0_pins"
    ["pwm9m1_pins"]="pwm1m1_ch1_pins"
    ["pwm10m1_pins"]="pwm1m1_ch2_pins"
    ["pwm11m1_pins"]="pwm1m1_ch3_pins"
    ["i2s0m0_lrck_tx"]="sai0m0_lrck_pins"
    ["i2s0m0_mclk"]="sai0m0_mclk_pins"
    ["i2s0m0_sclk_tx"]="sai0m0_sclk_pins"
    ["i2s0m0_sdi0"]="sai0m0_sdi0_pins"
    ["i2s0m0_sdo0"]="sai0m0_sdo0_pins"
    ["i2s0m1_lrck_tx"]="sai0m1_lrck_pins"
    ["i2s0m1_mclk"]="sai0m1_mclk_pins"
    ["i2s0m1_sclk_tx"]="sai0m1_sclk_pins"
    ["i2s0m1_sdi0"]="sai0m1_sdi0_pins"
    ["i2s0m1_sdo0"]="sai0m1_sdo0_pins"
    ["i2s1m0_lrck_tx"]="sai1m0_lrck_pins"
    ["i2s1m0_mclk"]="sai1m0_mclk_pins"
    ["i2s1m0_sclk_tx"]="sai1m0_sclk_pins"
    ["i2s1m0_sdi0"]="sai1m0_sdi0_pins"
    ["i2s1m0_sdo0"]="sai1m0_sdo0_pins"
    ["i2s1m1_lrck_tx"]="sai1m1_lrck_pins"
    ["i2s1m1_mclk"]="sai1m1_mclk_pins"
    ["i2s1m1_sclk_tx"]="sai1m1_sclk_pins"
    ["i2s1m1_sdi0"]="sai1m1_sdi0_pins"
    ["i2s1m1_sdo0"]="sai1m1_sdo0_pins"
    ["i2s2m0_lrck_tx"]="sai2m0_lrck_pins"
    ["i2s2m0_mclk"]="sai2m0_mclk_pins"
    ["i2s2m0_sclk_tx"]="sai2m0_sclk_pins"
    ["i2s2m0_sdi0"]="sai2m0_sdi0_pins"
    ["i2s2m0_sdo0"]="sai2m0_sdo0_pins"
    ["i2s2m1_lrck_tx"]="sai2m1_lrck_pins"
    ["i2s2m1_mclk"]="sai2m1_mclk_pins"
    ["i2s2m1_sclk_tx"]="sai2m1_sclk_pins"
    ["i2s2m1_sdi0"]="sai2m1_sdi0_pins"
    ["i2s2m1_sdo0"]="sai2m1_sdo0_pins"
    ["uart0_xfer"]="uart2m0_xfer_pins"
    ["uart0_ctsn"]="uart2m0_ctsn_pins"
    ["uart0_rtsn"]="uart2m0_rtsn_pins"
    ["uart1m0_xfer"]="uart1m0_xfer_pins"
    ["uart1m0_ctsn"]="uart1m0_ctsn_pins"
    ["uart1m0_rtsn"]="uart1m0_rtsn_pins"
    ["uart1m1_xfer"]="uart1m1_xfer_pins"
    ["uart1m1_ctsn"]="uart1m1_ctsn_pins"
    ["uart1m1_rtsn"]="uart1m1_rtsn_pins"
    ["uart2m0_xfer"]="uart0m0_xfer_pins"
    ["uart2m0_ctsn"]="uart0m0_ctsn_pins"
    ["uart2m0_rtsn"]="uart0m0_rtsn_pins"
    ["uart2m1_xfer"]="uart0m1_xfer_pins"
    ["uart2m1_ctsn"]="uart0m1_ctsn_pins"
    ["uart2m1_rtsn"]="uart0m1_rtsn_pins"
    ["uart3m0_xfer"]="uart3m2_xfer_pins"
    ["uart3m0_ctsn"]="uart3m2_ctsn_pins"
    ["uart3m0_rtsn"]="uart3m2_rtsn_pins"
    ["uart3m1_xfer"]="uart3m0_xfer_pins"
    ["uart3m1_ctsn"]="uart3m0_ctsn_pins"
    ["uart3m1_rtsn"]="uart3m0_rtsn_pins"
    ["uart3m2_xfer"]="uart3m1_xfer_pins"
    ["uart3m2_ctsn"]="uart3m1_ctsn_pins"
    ["uart3m2_rtsn"]="uart3m1_rtsn_pins"
    ["uart4m0_xfer"]="uart4m2_xfer_pins"
    ["uart4m0_ctsn"]="uart4m2_ctsn_pins"
    ["uart4m0_rtsn"]="uart4m2_rtsn_pins"
    ["uart4m1_xfer"]="uart4m1_xfer_pins"
    ["uart4m1_ctsn"]="uart4m1_ctsn_pins"
    ["uart4m1_rtsn"]="uart4m1_rtsn_pins"
    ["uart4m2_xfer"]="uart4m0_xfer_pins"
    ["uart4m2_ctsn"]="uart4m0_ctsn_pins"
    ["uart4m2_rtsn"]="uart4m0_rtsn_pins"
    ["uart5m0_xfer"]="uart5m2_xfer_pins"
    ["uart5m0_ctsn"]="uart5m2_ctsn_pins"
    ["uart5m0_rtsn"]="uart5m2_rtsn_pins"
    ["uart5m1_xfer"]="uart5m1_xfer_pins"
    ["uart5m1_ctsn"]="uart5m1_ctsn_pins"
    ["uart5m1_rtsn"]="uart5m1_rtsn_pins"
    ["uart5m2_xfer"]="uart5m0_xfer_pins"
    ["uart5m2_ctsn"]="uart5m0_ctsn_pins"
    ["uart5m2_rtsn"]="uart5m0_rtsn_pins"
    ["spi0m0_pins"]="spi0m0_clk_pins"
    ["spi0m0_cs0"]="spi0m0_csn0_pins"
    ["spi0m0_cs1"]="spi0m0_csn1_pins"
    ["spi0m1_pins"]="spi0m1_clk_pins"
    ["spi0m1_cs0"]="spi0m1_csn0_pins"
    ["spi0m1_cs1"]="spi0m1_csn1_pins"
    ["spi0m2_pins"]="spi0m2_clk_pins"
    ["spi0m2_cs0"]="spi0m2_csn0_pins"
    ["spi0m2_cs1"]="spi0m2_csn1_pins"
    ["spi1m0_pins"]="spi1m0_clk_pins"
    ["spi1m0_cs0"]="spi1m0_csn0_pins"
    ["spi1m0_cs1"]="spi1m0_csn1_pins"
    ["spi1m1_pins"]="spi1m1_clk_pins"
    ["spi1m1_cs0"]="spi1m1_csn0_pins"
    ["spi1m1_cs1"]="spi1m1_csn1_pins"
    ["spi1m2_pins"]="spi1m2_clk_pins"
    ["spi1m2_cs0"]="spi1m2_csn0_pins"
    ["spi1m2_cs1"]="spi1m2_csn1_pins"
    ["i2c0_xfer"]="i2c0m0_pins"
    ["i2c1_xfer"]="i2c1m2_pins"
    ["i2c2_xfer"]="i2c2m0_pins"
    ["i2c3m0_xfer"]="i2c3m3_pins"
    ["i2c3m1_xfer"]="i2c3m2_pins"
    ["i2c3m2_xfer"]="i2c3m1_pins"
    ["i2c4m0_xfer"]="i2c2m1_pins"
    ["i2c4m1_xfer"]="i2c1m3_pins"
    ["i2c5m0_xfer"]="i2c5m2_pins"
    ["i2c5m1_xfer"]="i2c5m3_pins"
    ["i2c5m2_xfer"]="i2c5m1_pins"
    ["pdmm0_clk"]="pdmm0_clk0_pins"
    ["pdmm0_clk1"]="pdmm0_clk1_pins"
    ["pdmm0_sdi0"]="pdmm0_sdi0_pins"
    ["pdmm0_sdi1"]="pdmm0_sdi1_pins"
    ["pdmm0_sdi2"]="pdmm0_sdi2_pins"
    ["pdmm0_sdi3"]="pdmm0_sdi3_pins"
    ["pdmm1_clk"]="pdmm1_clk0_pins"
    ["pdmm1_clk1"]="pdmm1_clk1_pins"
    ["pdmm1_sdi0"]="pdmm1_sdi0_pins"
    ["pdmm1_sdi1"]="pdmm1_sdi1_pins"
    ["pdmm1_sdi2"]="pdmm1_sdi2_pins"
    ["pdmm1_sdi3"]="pdmm1_sdi3_pins"
)

# ------------------------------------------------------------------------------
# Function: Display Usage Information
# ------------------------------------------------------------------------------
show_usage() {
    echo "Usage:"
    echo "  $0 [-d|--debug] [file list...]"
    echo "Options:"
    echo "  -d, --debug  Display preprocessed mapping table and exit"
    exit 0
}

# ------------------------------------------------------------------------------
# Preprocess Mapping Table (Insert Collision Prevention Placeholder)
# ------------------------------------------------------------------------------
preprocess_mapping() {
    declare -Ag MODIFIED_MAPPING
    for key in "${!MAPPING[@]}"; do
        value="${MAPPING[$key]}"
        MODIFIED_MAPPING[$key]="${value:0:1}${PLACEHOLDER}${value:1}"
    done
}

# ------------------------------------------------------------------------------
# Debug Mode: Print Sorted Mapping Table
# ------------------------------------------------------------------------------
debug_print() {
    echo "===== Sorted Mapping Table (Debug Mode) ====="
    echo "Note: Keys sorted by descending length for replacement accuracy"
    printf "%-35s -> %s\n" "Original Identifier (Length)" "Processed Identifier"
    echo "-----------------------------------------------"

    # Get keys sorted by descending length
    sorted_keys=$(printf "%s\n" "${!MODIFIED_MAPPING[@]}" | \
        awk '{ print length($0), $0 }' | \
        sort -nr -k1,1 | \
        cut -d' ' -f2-)

    # Print sorted table
    while IFS= read -r key; do
        if [[ -n "$key" ]]; then
            key_length=${#key}
            printf "%-35s (%2d) -> %s\n" \
                   "$key" \
                   "$key_length" \
                   "${MODIFIED_MAPPING[$key]}"
        fi
    done <<< "$sorted_keys"

    echo "==============================================="
    echo "Total mappings: ${#MODIFIED_MAPPING[@]} entries"
}

# ------------------------------------------------------------------------------
# Main Processing Workflow
# ------------------------------------------------------------------------------
process_files() {
    declare processed_count=0
    for file in "$@"; do
        # File existence check
        [[ ! -f "$file" ]] && { echo "[WARN] File not found: $file"; continue; }

        # Exclude base definition files
        if [[ "$file" =~ rv1126b?\.dtsi$ ]]; then
            echo "[SKIP] Base definition file: $file"
            continue
        fi

        # Check processing marker
        if head -n 1 "$file" | grep -q "${MARKER}"; then
            echo "[SKIP] Already processed: $file"
            continue
        fi

        # Perform replacements
        content=$(<"$file")

        # Get keys sorted by descending length
        sorted_keys=$(printf "%s\n" "${!MODIFIED_MAPPING[@]}" | awk '{ print length($0), $0 }' | sort -nr | cut -d' ' -f2-)

        # Execute replacements in order
        while IFS= read -r key; do
            if [[ -n "$key" ]]; then
                content=${content//"$key"/"${MODIFIED_MAPPING[$key]}"}
            fi
        done <<< "$sorted_keys"

        content=${content//"${PLACEHOLDER}"/}

        # Special case handling
        if [[ "$content" == *"i2c2m1_pins"* ]]; then
            content=${content//"i2c4 "/"i2c2 "}
        fi
        if [[ "$content" == *"i2c1m3_pins"* ]]; then
            content=${content//"i2c4 "/"i2c1 "}
        fi

        # Write processing marker
        ext=".${file##*.}"
        printf "%s\n%s" "${COMMENT_MARKERS[$ext]}" "$content" > "$file"
        echo "[OK] Processed: $file"
        ((processed_count++))
    done

    echo "Processing complete. Converted ${processed_count} files"
}

# ------------------------------------------------------------------------------
# Entry Point
# ------------------------------------------------------------------------------
main() {
    # Argument parsing
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -d|--debug) DEBUG_MODE=true; shift ;;
            -h|--help)  show_usage ;;
            *)          break ;;
        esac
    done

    # Preprocess mapping table
    preprocess_mapping

    # Debug mode handling
    if [[ "$DEBUG_MODE" == true ]]; then
        debug_print
        exit 0
    fi

    # File input validation
    [[ $# -eq 0 ]] && { echo "Error: No input files specified"; exit 1; }

    # Execute main process
    process_files "$@"
}

# ------------------------------------------------------------------------------
# Execute Main Function
# ------------------------------------------------------------------------------
main "$@"
