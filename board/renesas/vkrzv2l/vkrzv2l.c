#include <common.h>
#include <cpu_func.h>
#include <image.h>
#include <init.h>
#include <malloc.h>
#include <netdev.h>
#include <dm.h>
#include <dm/platform_data/serial_sh.h>
#include <asm/processor.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/arch/gpio.h>
#include <asm/arch/rmobile.h>
#include <asm/arch/rcar-mstp.h>
#include <asm/arch/sh_sdhi.h>
#include <i2c.h>
#include <mmc.h>
#include <stdio.h>
#include "pfc_regs.h"

DECLARE_GLOBAL_DATA_PTR;

//#define PFC_BASE    0x11030000

#define ETH_CH0     (PFC_BASE + 0x300c)
#define ETH_CH1     (PFC_BASE + 0x3010)
#define I2C_CH1     (PFC_BASE + 0x1870)
#define ETH_PVDD_3300   0x00
#define ETH_PVDD_1800   0x01
#define ETH_PVDD_2500   0x02
#define ETH_MII_RGMII   (PFC_BASE + 0x3018)

/* CPG */
#define CPG_BASE                    0x11010000
#define CPG_CLKON_BASE              (CPG_BASE + 0x500)
#define CPG_RESET_BASE              (CPG_BASE + 0x800)
#define CPG_RESET_ETH               (CPG_RESET_BASE + 0x7C)
#define CPG_RESET_I2C               (CPG_RESET_BASE + 0x80)
#define CPG_PL2_SDHI_DSEL           (CPG_BASE + 0x218)
#define CPG_CLK_STATUS              (CPG_BASE + 0x280)
#define CPG_RST_USB                 (CPG_BASE + 0x878)
#define CPG_CLKON_USB               (CPG_BASE + 0x578)

/* PFC */
//#define PFC_P37                     (PFC_BASE + 0x037)
//#define PFC_PM37                    (PFC_BASE + 0x16E)
//#define PFC_PMC37                   (PFC_BASE + 0x237)
//#define PFC_P18             (PFC_BASE + 0x018)
//#define PFC_PM18                (PFC_BASE + 0x130)
//#define PFC_PMC18               (PFC_BASE + 0x218)
//#define PFC_P21             (PFC_BASE + 0x142)
//#define PFC_PM21                (PFC_BASE + 0x128)
//#define PFC_PMC21               (PFC_BASE + 0x221)
//#define PFC_P23             (PFC_BASE + 0x023)
//#define PFC_PM23                (PFC_BASE + 0x146)
//#define PFC_PMC23               (PFC_BASE + 0x223)
//#define PFC_P14             (PFC_BASE + 0x014)
//#define PFC_PM14                (PFC_BASE + 0x128)
//#define PFC_PMC14               (PFC_BASE + 0x214)
//
#define USBPHY_BASE     0x11c40000
#define USB0_BASE       0x11c50000
#define USB1_BASE       0x11c70000
#define USBF_BASE       0x11c60000
#define USBPHY_RESET        (USBPHY_BASE + 0x000u)
#define COMMCTRL        0x800
#define HcRhDescriptorA     0x048
#define LPSTS           0x102

#define RPC_CMNCR       0x10060000

#define PFC_Pn(n)   (PFC_BASE + 0x0010 + n)      /* Port register R/W */
#define PFC_PMn(n)  (PFC_BASE + 0x0120 + n * 2)  /* Port mode register R/W */
#define PFC_PMCn(n) (PFC_BASE + 0x0210 + n)      /* Port mode control register */
#define PFC_PFCn(n) (PFC_BASE + 0x0440 + n * 4)  /* Port function control register */
#define PFC_PINn(n) (PFC_BASE + 0x0810 + n)      /* Port input register */

#define COMMA   ,
#define P(p, b)     p COMMA b

#define PORT_LED_R          P( 8, 2)
#define PORT_LED_G          P(17, 2)
#define PORT_LED_B          P(19, 1)
#define PORT_LED_Y          P(15, 0)

#define PORT_ET0_RESETn     P(39, 2)     
#define PORT_ET1_RESETn     P( 7, 0)


enum pfc_pin_gpio_mode {GPIO_HiZ=0, GPIO_IN=1, GPIO_OUT=2, GPIO_IO=3};
enum pfc_pin_func_mode {FUNC0=0, FUNC1, FUNC2, FUNC3, FUNC4, FUNC5 };


// assigned-clock-rates = <12288000>, <25000000>,
//                        <25000000>, <12288000>,
//                        <11289600>, <24000000>;
static const u8 ren_5p35023b_settings[] = {
    0x00, 0x00, 0x11, 0x19, 0x00, 0x42, 0xcc, 0x2b, 0x04, 0x32, 0x00, 0x1a, 0x5f, 0x12, 0x90, 0x79,
    0x02, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x24, 0x19, 0xbf, 0x3f, 0x30, 0x90, 0x86,
    0x80, 0xb2, 0x05, 0xc4, 0x9c
};


/* Arguments:
   n = port(1-48)
   b = bit(0-4)
   d = direction(GPIO_HiZ, GPIO_IN, GPIO_OUT, GPIO_IO)
*/
static void pfc_set_gpio(u8 n, u8 b, u8 d)
{
   *(volatile u8 *)PFC_PMCn(n) &= ~(1ul << b); /* 0b: Port Mode (GPIO) */
   *(volatile u16 *)PFC_PMn(n) = (*(volatile u16 *)PFC_PMn(n) & ~(0b11<<(b*2))) | (((u16)d & 0b11) << (b*2)); /* set port mode */
}

/* Arguments:
   n = port(1-11)
   b = bit(0-15)
   v = value (0 or 1)
*/
void gpio_set(u8 n, u8 b, u8 v)
{
   /* The pin should have been configured as GPIO_OUT using pfc_set_gpio */
    if ( v ) 
        *(volatile u8 *)PFC_Pn(n) |= 1UL << b;       // Set pin 
    else
        *(volatile u8 *)PFC_Pn(n) &= ~(1UL << b);    // Clear pin
}

/* Arguments:
   n = port(1-40)
   b = bit(0-4)
   return = current pin level (0 or 1);
*/
static u8 gpio_read(u8 n, u8 b)
{
   /* The pin should have been configured as GPIO_IN using pfc_set_gpio */
   //printf("PINn(%d) %04X\n",n,*(volatile u8 *)PFC_PINn(n));
   return ( *(volatile u8 *)PFC_PINn(n) >> b ) & 0x01;
}


/* Arguments:
    n = port number (P1-P40)
    b = bit number (0-4)
    func = Alternative function ('FUNC0'-'FUNC5')
*/
void pfc_set_pin_function(u16 n, u16 b, u8 func)
{
   *(volatile u8 *)PFC_PMCn(n) |= (1ul << b); /* 1b: Peripheral Function Mode (Peripheral Function) */

   *(volatile u32 *)PFC_PFCn(n) = (*(volatile u32 *)PFC_PFCn(n) & ~(0b111<<(b*4))) | (((u16)func & 0b111) << (b*4)); /* set port mode */
}


void s_init(void)
{
    /* SD power control: P19_0 = 1; */
    *(volatile u32 *)(PFC_PMC23) &= 0xFFFFFFFE; /* Port func mode 0b0 */
    *(volatile u32 *)(PFC_PM23) = (*(volatile u32 *)(PFC_PM23) & 0xFFFFFFFC) | 0x2; /* Port output mode 0b10 */
    *(volatile u32 *)(PFC_P23) = (*(volatile u32 *)(PFC_P23) & 0xFFFFFFFE) | 0x1;   /* Port 19[0] output value 0b1*/
    
    /* can go in board_eht_init() once enabled */
    *(volatile u32 *)(ETH_CH0) = (*(volatile u32 *)(ETH_CH0) & 0xFFFFFFFC) | ETH_PVDD_1800;
    *(volatile u32 *)(ETH_CH1) = (*(volatile u32 *)(ETH_CH1) & 0xFFFFFFFC) | ETH_PVDD_1800;
    /* Enable RGMII for both ETH{0,1} */
    *(volatile u32 *)(ETH_MII_RGMII) = (*(volatile u32 *)(ETH_MII_RGMII) & 0xFFFFFFFC);
    /* ETH CLK */
    *(volatile u32 *)(CPG_RESET_ETH) = 0x30003;
    /* I2C CLK */
    *(volatile u32 *)(CPG_RESET_I2C) = 0xF000F;
    /* I2C pin non GPIO enable */
    *(volatile u32 *)(I2C_CH1) = 0x01010101;
    /* SD CLK */
    *(volatile u32 *)(CPG_PL2_SDHI_DSEL) = 0x00110011;
    while (*(volatile u32 *)(CPG_CLK_STATUS) != 0)
        ;
}


int board_led_init(void)
{
    /* RED LED: P8_2 = 1; */
    pfc_set_gpio(PORT_LED_R, GPIO_OUT); gpio_set(PORT_LED_R, 0);

    /* GREEN LED: P17_2 = 1; */
    pfc_set_gpio(PORT_LED_G, GPIO_OUT); gpio_set(PORT_LED_G, 0);

    /* BLUE LED: P19_1 = 1; */
    pfc_set_gpio(PORT_LED_B, GPIO_OUT); gpio_set(PORT_LED_B, 0);

    /* YELLOW LED: P15_0 = 1; */
    pfc_set_gpio(PORT_LED_Y, GPIO_OUT); gpio_set(PORT_LED_Y, 0);

    return 0;
}

int board_early_init_f(void)
{
    /* LED's */
    board_led_init();

    gpio_set(PORT_LED_R, 1);

    /* Ethernet 0 PHY Reset: P39_2 = 1; */
    pfc_set_gpio(PORT_ET0_RESETn, GPIO_OUT);
    gpio_set(PORT_ET0_RESETn, 1);

    /* Ethernet 1 PHY Reset: P7_0 = 1; */
    pfc_set_gpio(PORT_ET1_RESETn, GPIO_OUT);
    gpio_set(PORT_ET1_RESETn, 1);

    return 0;
}


static void board_usb_init(void)
{
    /*Enable USB*/
    (*(volatile u32 *)CPG_RST_USB) = 0x000f000f;
    (*(volatile u32 *)CPG_CLKON_USB) = 0x000f000f;

    /* Setup  */
    /* Disable GPIO Write Protect */
    (*(volatile u32 *)PFC_PWPR) &= ~(0x1u << 7);    /* PWPR.BOWI = 0 */
    (*(volatile u32 *)PFC_PWPR) |= (0x1u << 6);     /* PWPR.PFCWE = 1 */

    /* set P4_0 as Func.1 for VBUSEN */
    (*(volatile u8 *)PFC_PMC14) |= (0x1u << 0);     /* PMC14.b0 = 1 */
    (*(volatile u8 *)PFC_PFC14) &= ~(0x7u << 0);    /* PFC14.PFC0 = 1 */
    (*(volatile u8 *)PFC_PFC14) |= (0x1u << 0);

    /* set P5_0 as Func.1 for OVERCUR */
    (*(volatile u8 *)PFC_PMC15) |= (0x1u << 0);     /* PMC15.b0 = 1 */
    (*(volatile u8 *)PFC_PFC15) &= ~(0x7u << 0);    /* PFC15.PFC0 = 1 */
    (*(volatile u8 *)PFC_PFC15) |= (0x1u << 0);

    /* set P8_0 as Func.2 for VBUSEN */
    (*(volatile u8 *)PFC_PMC18) |= (0x1u << 0);     /* PMC18.b0 = 1 */
    (*(volatile u8 *)PFC_PFC18) &= ~(0x7u << 0);    /* PFC18.PFC0 = 2 */
    (*(volatile u8 *)PFC_PFC18) |= (0x2u << 0);

    /* set P8_1 as Func.2 for OVERCUR */
    (*(volatile u8 *)PFC_PMC18) |= (0x1u << 1);     /* PMC18.b1 = 1 */
    (*(volatile u8 *)PFC_PFC18) &= ~(0x7u << 4);    /* PFC18.PFC1 = 2 */
    (*(volatile u8 *)PFC_PFC18) |= (0x2u << 4);

    /* Enable write protect */
    (*(volatile u32 *)PFC_PWPR) &= ~(0x1u << 6);    /* PWPR.PFCWE = 0 */
    (*(volatile u32 *)PFC_PWPR) |= (0x1u << 7);     /* PWPR.BOWI = 1 */

    /*Enable 2 USB ports*/
    (*(volatile u32 *)USBPHY_RESET) = 0x00001000u;
    /*USB0 is HOST*/
    (*(volatile u32 *)(USB0_BASE + COMMCTRL)) = 0;
    /*USB1 is HOST*/
    (*(volatile u32 *)(USB1_BASE + COMMCTRL)) = 0;
    /* Set USBPHY normal operation (Function only) */
    (*(volatile u16 *)(USBF_BASE + LPSTS)) |= (0x1u << 14);     /* USBPHY.SUSPM = 1 (func only) */
    /* Overcurrent is not supported */
    (*(volatile u32 *)(USB0_BASE + HcRhDescriptorA)) |= (0x1u << 12);       /* NOCP = 1 */
    (*(volatile u32 *)(USB1_BASE + HcRhDescriptorA)) |= (0x1u << 12);       /* NOCP = 1 */
}


int board_init(void)
{
    /* adress of boot parameters */
    gd->bd->bi_boot_params = CONFIG_SYS_TEXT_BASE + 0x50000;

    board_usb_init();

    gpio_set(PORT_LED_G, 1);

    return 0;
}


int board_late_init(void)
{
    int retval = 0;
    struct udevice *iic_dev;


    /* Set clocks */ 
    retval = i2c_get_chip_for_busnum(CONFIG_I2C_DEFAULT_BUS_NUMBER, I2C_VERSACLOCK_ADDR, 1, &iic_dev);

    if (retval == 0)
        retval = dm_i2c_write(iic_dev, 0u, ren_5p35023b_settings, sizeof(ren_5p35023b_settings));


#ifdef CONFIG_RENESAS_RZG2LWDT
    rzg2l_reinitr_wdt();
#endif // CONFIG_RENESAS_RZG2LWDT

  
    if (retval == 0) {
        gpio_set(PORT_LED_B, 1);
    }
    return 0;
}

int board_eth_init(struct bd_info *bis)
{

    gpio_set(PORT_LED_Y, 1);

    return 0;
}


void reset_cpu(void)
{
#ifdef CONFIG_RENESAS_RZG2LWDT
    struct udevice *wdt_dev;
    if (uclass_get_device(UCLASS_WDT, WDT_INDEX, &wdt_dev) < 0) {
        printf("failed to get wdt device. cannot reset\n");
        return;
    }
    if (wdt_expire_now(wdt_dev, 0) < 0) {
        printf("failed to expire_now wdt\n");
    }
#endif // CONFIG_RENESAS_RZG2LWDT
}
