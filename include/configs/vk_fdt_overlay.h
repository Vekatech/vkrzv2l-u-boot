/*
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __VK_FDT_OVERLAY_H__
#define __VK_FDT_OVERLAY_H__

#define OVERLAY_SETTINGS \
	"dtbo_addr=0x48010000\0" \
	"overlaydir=overlays\0"

#define GET_OVERLAY_APPLY_mmc(dtbo_name)          \
	"echo Applying DT overlay: " dtbo_name " ;"  \
	"fatload mmc ${mmcdev}:${mmcpart} ${dtbo_addr} ${overlaydir}/" dtbo_name "; " \
	"fdt addr ${dtb_addr}; " \
	"fdt apply ${dtbo_addr}; "

#define GET_OVERLAY_APPLY_net(dtbo_name)          \
	"echo Applying DT overlay: " dtbo_name "; "  \
	"tftp ${dtbo_addr} ${serverip}:${tftpdir}/${overlaydir}/" dtbo_name "; " \
	"fdt addr ${dtb_addr}; " \
	"fdt apply ${dtbo_addr}; "

#define GET_OVERLAY_APPLY_usb(dtbo_name)          \
	"echo Applying DT overlay: " dtbo_name "; "  \
	"fatload usb ${usbdev}:${usbpart} ${dtbo_addr} ${overlaydir}/" dtbo_name "; " \
	"fdt addr ${dtb_addr}; " \
	"fdt apply ${dtbo_addr}; "

#define GET_FDT_OVERLAY_mmc       \
	"loadfdto_mmc=echo Loading overlays from mmc ...; "     \
	"fdt addr ${dtb_addr}; "      \
	"fdt resize 0x10000; "        \
	"if env exists fdt_extra_overlays && test -n ${fdt_extra_overlays}; then " \
		"for dtbo_file in ${fdt_extra_overlays};  do "  \
			GET_OVERLAY_APPLY_mmc( "${dtbo_file}" )         \
		"done; "                  \
	"fi;\0"

#define GET_FDT_OVERLAY_net       \
	"loadfdto_net=echo Loading overlays from net ...; "     \
	"fdt addr ${dtb_addr}; "      \
	"fdt resize 0x10000; "        \
	"if env exists fdt_extra_overlays && test -n ${fdt_extra_overlays}; then " \
		"for dtbo_file in ${fdt_extra_overlays}; do " \
			GET_OVERLAY_APPLY_net( "${dtbo_file}" )         \
		"done; "                  \
	"fi;\0"

#define GET_FDT_OVERLAY_usb       \
	"loadfdto_usb=echo Loading overlays from usb ...; "     \
	"fdt addr ${dtb_addr}; "      \
 	"fdt resize 0x10000; "        \
	"if env exists fdt_extra_overlays && test -n ${fdt_extra_overlays}; then " \
		"for dtbo_file in ${fdt_extra_overlays};  do "  \
			GET_OVERLAY_APPLY_usb( "${dtbo_file}" )     \
		"done; "                  \
	"fi;\0"

#define   VEKATECH_UENV_FDTO_SUPPORT

#endif  /* __VK_FDT_OVERLAY_H__ */
