/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2021 Renesas Electronics Corporation
 */

#ifndef __VK_D184280E_H
#define __VK_D184280E_H

#include <asm/arch/renesas.h>
#include "vk_fdt_overlay.h"

#define __p(x)   x

#ifdef CONFIG_TARGET_VK_D184280E_1
#undef CONFIG_SYS_BOARD
#define CONFIG_SYS_BOARD "vk-de184280e_1"
#endif 

#define CONFIG_REMAKE_ELF

#ifdef CONFIG_SPL
#define CONFIG_SPL_TARGET   "spl/u-boot-spl.scif"
#endif

/* boot option */

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

/* Generic Interrupt Controller Definitions */
/* RZ/G2L use GIC-v3 */
#define CONFIG_GICV3
#define GICD_BASE   0x11900000
#define GICR_BASE   0x11960000

/* console */
#define CONFIG_SYS_CBSIZE       2048
#define CONFIG_SYS_BARGSIZE     CONFIG_SYS_CBSIZE
#define CONFIG_SYS_MAXARGS      64
#define CONFIG_SYS_BAUDRATE_TABLE   { 115200, 38400 }

/* PHY needs a longer autoneg timeout */
#define PHY_ANEG_TIMEOUT        20000

/* MEMORY */
#define CONFIG_SYS_INIT_SP_ADDR     CONFIG_SYS_TEXT_BASE

/* SDHI clock freq */
#define CONFIG_SH_SDHI_FREQ     133000000

#define DRAM_RSV_SIZE           0x08000000

#define CONFIG_SYS_SDRAM_BASE       (0x40000000 + DRAM_RSV_SIZE)

#if CONFIG_TARGET_VK_D184280E_1
# define CONFIG_SYS_SDRAM_SIZE       (0x40000000u - DRAM_RSV_SIZE) //total 1GB
#else
# define CONFIG_SYS_SDRAM_SIZE       (0x80000000u - DRAM_RSV_SIZE) //total 2GB
#endif

#define CONFIG_SYS_LOAD_ADDR        0x58000000
#define CONFIG_LOADADDR         CONFIG_SYS_LOAD_ADDR // Default load address for tfpt,bootp...
#define CONFIG_VERY_BIG_RAM

#if CONFIG_TARGET_VK_D184280E_1
# define CONFIG_MAX_MEM_MAPPED       (0x40000000u - DRAM_RSV_SIZE)
#else
# define CONFIG_MAX_MEM_MAPPED       (0x80000000u - DRAM_RSV_SIZE)
#endif

#define CONFIG_SYS_MONITOR_BASE     0x00000000
//#define CONFIG_SYS_MONITOR_LEN      (1 * 1024 * 1024)
//#define CONFIG_SYS_MALLOC_LEN       (64 * 1024 * 1024)
//#define CONFIG_SYS_BOOTM_LEN        (64 << 20)

/* The HF/QSPI layout permits up to 1 MiB large bootloader blob */
#define CONFIG_BOARD_SIZE_LIMIT     1048576

/* I2C configuration */
#define I2C_VERSACLOCK_ADDR  0x68
#define I2C_VERSACLOCK_ADDR_LEN 1
#define I2C_SYS_MODULE    0

#ifndef CONFIG_SYS_I2C_EEPROM_ADDR 
#define CONFIG_SYS_I2C_EEPROM_ADDR 0x50
#endif
#ifndef CONFIG_SYS_I2C_EEPROM_BUS
#define CONFIG_SYS_I2C_EEPROM_BUS 0
#endif

#ifndef CONFIG_SYS_I2C_EEPROM_ADDR_LEN
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN 1
#endif

#define I2C_EEPROM_MAC_ADDR     0x58
#define ETHER_EEPROM_ADDR       0x9A /* MAC address offset in EEPROM */

#ifdef VEKATECH_UENV_FDTO_SUPPORT
#define LOAD_FDT_OVERLAYS \
    OVERLAY_SETTINGS \
    GET_FDT_OVERLAY_mmc \
    GET_FDT_OVERLAY_usb \
    GET_FDT_OVERLAY_net
#else
#define LOAD_FDT_OVERLAYS \
    "loadfdto_mmc=echo overlays are not supported!\0" \
    "loadfdto_usb=echo overlays are not supported!\0" \
    "loadfdto_net=echo overlays are not supported!\0"
#endif

/* Load rzv2l_cm33_rpmsg_demo to run */
#define CM33_BOOT_ARGS \
    "dcache off; " \
    "mmc dev 0; " \
    "fatload mmc 0:1 0x0001FF80 cm33/rzv2l_cm33_rpmsg_demo_secure_vector.bin; " \
    "fatload mmc 0:1 0x42EFF440 cm33/rzv2l_cm33_rpmsg_demo_secure_code.bin; " \
    "fatload mmc 0:1 0x00010000 cm33/rzv2l_cm33_rpmsg_demo_non_secure_vector.bin; " \
    "fatload mmc 0:1 0x40010000 cm33/rzv2l_cm33_rpmsg_demo_non_secure_code.bin; " \
    "cm33 start_debug 0x1001FF80 0x00010000; " \
    "dcache on;"

#define DEFAULT_MMC_UENV_ARGS \
    "boardID="CONFIG_SYS_BOARD"\0" \
    "image=Image\0" \
    "fdtfile="CONFIG_DEFAULT_FDT_FILE"\0" \
    "dtb_addr=0x48000000\0" \
    "image_addr=0x48080000\0" \
    "bootm_size=0x10000000\0" \
    "loadaddr=0x58000000\0" \
    "rd_addr=0x50000000\0" \
    "mmcdev="__stringify(CONFIG_SYS_MMC_DEV)"\0" \
    "mmcpart="__stringify(CONFIG_SYS_MMC_IMG_LOAD_PART)"\0" \
    "tftpdir="CONFIG_SYS_BOARD"\0" \
    "netrootfs=/nfs/"CONFIG_SYS_BOARD"\0" \
    "netmask=255.255.255.0\0" \
    "serverip=192.168.2.91\0" \
    "usbdev="__stringify(CONFIG_SYS_USB_DEV)"\0" \
    "usbpart="__stringify(CONFIG_SYS_USB_IMG_LOAD_PART)"\0" \
    "usbbootdir=boot\0" \
    "usb_pgood_delay=500\0" \
    "restart_on_cold_reset=n\0" \
    "bootenvfile=uEnv.txt\0" \
    "importbootenv=echo Importing environment ...; " \
        "env import -t ${image_addr} ${filesize}\0" \
    "bootimage=booti ${image_addr} - ${dtb_addr}\0" \
    "bootimagerd=booti ${image_addr} ${rd_addr} ${dtb_addr}\0" \
    "rdinitapp=/home/root/emmc_prog\0" \
    "cpiofile=core-image-bsp-"CONFIG_SYS_BOARD"-mod.cpio.gz.u-boot\0"

#define DEFAULT_MMC_CMG2LC_ARGS  DEFAULT_MMC_UENV_ARGS

#define GET_IMAGE_USB \
    "load_usb=echo trying usb...; "          \
          "usb start; "                      \
          "usb reset; "                      \
          "if usb dev ${usbdev}; then "      \
             "if run loadbootenv_usb; then " \
                 "run importbootenv; "       \
             "fi; "                          \
             "if run loadfdt_usb; then "     \
                "run loadfdto_usb; "          \
                "if run loadimage_usb; then " \
                    "run loadcpio_usb; "     \
                "fi; "                       \
             "fi; "                          \
          "fi;\0"

#define GET_IMAGE_MMC \
    "load_mmc=echo trying mmc...; "               \
            "if mmc dev ${mmcdev}; then "         \
              "if mmc rescan; then "              \
                  "if run loadbootenv_mmc; then " \
                      "run importbootenv; "       \
                  "fi; "                          \
                  "if run loadfdt_mmc; then "     \
                      "run loadfdto_mmc; "        \
                      "run loadimage_mmc; "       \
                  "fi; "                          \
              "fi; "                              \
            "fi;\0"

#define GET_IMAGE_NET \
    "load_net=echo trying net...; "         \
            "setenv autoload no; dhcp; "    \
            "if run loadbootenv_net; then " \
                "run importbootenv; "       \
            "fi; "                          \
            "if run loadfdt_net; then "     \
                "run loadfdto_net; "        \
                "run loadimage_net; "       \
            "fi;\0"

#define BOOT_MMC                        \
    "if run load_mmc; then "            \
        "echo Starting from mmc ...; "  \
        "run bootargs_mmc; "            \
        "run bootimage; "               \
    "else "                             \
        "echo Cannot start from mmc!; " \
    "fi; "

#define BOOT_USB \
    "if run load_usb; then "            \
        "echo Starting from usb ...; "  \
        "run bootargs_usb; "            \
        "run bootimagerd; "             \
    "else "                             \
        "echo Cannot start from usb!; " \
    "fi; "

#define BOOT_NET \
    "if run load_net; then "            \
        "echo Starting from net ...; "  \
        "run bootargs_net; "            \
        "run bootimage; "               \
    "else "                             \
        "echo Cannot start from net!; " \
    "fi; "

/* ENV setting */

#define CFG_EXTRA_ENV_SETTINGS \
    DEFAULT_MMC_CMG2LC_ARGS \
    LOAD_FDT_OVERLAYS \
    "bootargs_mmc=setenv bootargs rw rootwait earlycon root=/dev/mmcblk"__stringify(CONFIG_SYS_MMC_DEV)"p2 ${extrabootargs}\0" \
    "bootargs_net=setenv bootargs rw rootwait earlycon root=/dev/nfs ip=dhcp nfsroot=${serverip}:${netrootfs},vers=4,tcp ${extrabootargs}\0" \
    "bootargs_usb=setenv bootargs rw rootwait earlycon root=/dev/ram0 rdinit=${rdinitapp} ${extrabootargs}\0" \
    "loadbootenv_mmc=fatload mmc ${mmcdev} ${image_addr} ${bootenvfile}\0" \
    "loadbootenv_usb=fatload usb ${usbdev} ${image_addr} ${bootenvfile}\0" \
    "loadbootenv_net=tftp ${image_addr} ${serverip}:${tftpdir}/${bootenvfile}\0" \
    "loadimage_mmc=fatload mmc ${mmcdev}:${mmcpart} ${image_addr} ${image}\0" \
    "loadimage_usb=fatload usb ${usbdev}:${usbpart} ${image_addr} ${usbbootdir}/${image}\0" \
    "loadimage_net=tftp ${image_addr} ${serverip}:${tftpdir}/${image}\0" \
    "loadfdt_mmc=echo loading ${fdtfile}; fatload mmc ${mmcdev}:${mmcpart} ${dtb_addr} ${fdtfile}\0" \
    "loadfdt_usb=echo loading ${fdtfile}; fatload usb ${usbdev}:${usbpart} ${dtb_addr} ${usbbootdir}/${fdtfile}\0" \
    "loadfdt_net=echo loading ${fdtfile}; tftp ${dtb_addr} ${serverip}:${tftpdir}/${fdtfile}\0" \
    "loadcpio_usb=echo loading ${cpiofile}; fatload usb ${usbdev}:${usbpart} ${rd_addr} ${usbbootdir}/${cpiofile}\0" \
    GET_IMAGE_MMC \
    GET_IMAGE_USB \
    GET_IMAGE_NET \
    "boot_mmc="BOOT_MMC"\0" \
    "boot_usb="BOOT_USB"\0" \
    "boot_net="BOOT_NET"\0"

#define CONFIG_BOOTCOMMAND \
        BOOT_USB    \
        BOOT_MMC    \
        BOOT_NET

/* For board */
/* Ethernet RAVB */
#define CONFIG_BITBANGMII_MULTI

#endif /* __VK_D184280E_H */
