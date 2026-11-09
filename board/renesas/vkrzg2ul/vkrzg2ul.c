#include <common.h>
#include <cpu_func.h>
#include <image.h>
#include <init.h>
#include <malloc.h>
#include <netdev.h>
#include <net.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <dm/platform_data/serial_sh.h>
#include <asm/processor.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/arch/gpio.h>
#include <asm/arch/renesas.h>
#include <asm/arch/rcar-mstp.h>
#include <asm/arch/sh_sdhi.h>
#include <i2c.h>
#include <i2c_eeprom.h>
#include <mmc.h>
#include <wdt.h>
#include <rzg2l_wdt.h>
#include <renesas/rzg_pfc.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;


#define ETH_CH0         (PFC_BASE + 0x300c)
#define ETH_CH1		    (PFC_BASE + 0x3010)
#define I2C_CH1         (PFC_BASE + 0x1870)
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

#define USBPHY_BASE     0x11c40000
#define USB0_BASE       0x11c50000
#define USB1_BASE       0x11c70000
#define USBF_BASE       0x11c60000
#define USBPHY_RESET    (USBPHY_BASE + 0x000u)
#define COMMCTRL        0x800
#define HcRhDescriptorA 0x048
#define LPSTS           0x102

#define RPC_CMNCR       0x10060000

#define PFC_Pn(n)   (PFC_BASE + 0x0010ull + (n))      /* Port register R/W */
#define PFC_PMn(n)  (PFC_BASE + 0x0120ull + (n) * 2)  /* Port mode register R/W */
#define PFC_PMCn(n) (PFC_BASE + 0x0210ull + (n))      /* Port mode control register */
#define PFC_PFCn(n) (PFC_BASE + 0x0440ull + (n) * 4)  /* Port function control register */
#define PFC_PINn(n) (PFC_BASE + 0x0810ull + (n))      /* Port input register */

#define COMMA   ,
#define P(p, b)     p COMMA b

#define PORT_LED_R          P(17, 2)

#define PORT_USB0_VBUSEN    P( 5, 0)
#define PORT_USB0_OVRCUR    P( 5, 2)
#define PORT_USB1_VBUSEN    P(18, 4)
#define PORT_USB1_OVRCUR    P(18, 5)

#define PORT_ET0_RESETn     P( 4, 2)     
#define PORT_ET1_RESETn     P(10, 1)

#define PORT_SD_PWR_EN      P( 8, 2)


/* WDT */
#define WDT_INDEX       0

enum pfc_pin_gpio_mode {GPIO_HiZ=0, GPIO_IN=1, GPIO_OUT=2, GPIO_IO=3};
enum pfc_pin_func_mode {FUNC0=0, FUNC1, FUNC2, FUNC3, FUNC4, FUNC5, FUNC6, FUNC7 };


// assigned-clock-rates = <12288000>, <25000000>,
//                        <25000000>, <32768>,
//                        <11289600>, <24000000>;
//static const u8 ren_5p35023b_settings[] = {
//    0x00, 0x00, 0x11, 0x19, 0x00, 0x42, 0xcc, 0x2b, 0x04, 0x32, 0x00, 0x1a, 0x5f, 0x12, 0x90, 0x79,
//    0x02, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x24, 0x19, 0xbf, 0x3f, 0x30, 0x90, 0x86,
//    0x80, 0xb2, 0x05, 0xc4, 0x9c
//};

// assigned-clock-rates = <12288000>, <25000000>,
//                        <25000000>, <OFF>,
//                        <11289600>, <24000000>;
static const u8 ren_5p35023b_settings[] = {
    0x00, 0x00, 0x11, 0x19, 0x00, 0x42, 0xCC, 0x2B, 0x04, 0x32, 0x00, 0x1A, 0x5F, 0x12, 0x90, 0x79,
    0x02, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x24, 0x19, 0xBF, 0x3F, 0x30, 0x80, 0x86,
    0x80, 0xC2, 0x45, 0xC4, 0x9E, 0x6E
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
    func = Alternative function ('FUNC1'-'FUNC7')
*/
void pfc_set_pin_function(u16 n, u16 b, u8 func)
{
   *(volatile u8 *)PFC_PMCn(n) |= (1ul << b); /* 1b: Peripheral Function Mode (Peripheral Function) */

   *(volatile u32 *)PFC_PFCn(n) = (*(volatile u32 *)PFC_PFCn(n) & ~(0b111<<(b*4))) | (((u16)func & 0b111) << (b*4)); /* set port mode */
}


void s_init(void)
{
    pfc_set_gpio(PORT_SD_PWR_EN, GPIO_OUT); 
    gpio_set(PORT_SD_PWR_EN, 1);

    /* can go in board_eht_init() once enabled */
    *(volatile u32 *)(ETH_CH0) = (*(volatile u32 *)(ETH_CH0) & 0xFFFFFFFC) | ETH_PVDD_1800;
	*(volatile u32 *)(ETH_CH1) = (*(volatile u32 *)(ETH_CH1) & 0xFFFFFFFC) | ETH_PVDD_1800;
    /* Enable RGMII for ETH0 */
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

    *(volatile u32 *)(RPC_CMNCR) = 0x01FFF300;
}


static int board_led_init(void)
{
    /* RED LED: P8_2 = 1; */
    pfc_set_gpio(PORT_LED_R, GPIO_OUT); gpio_set(PORT_LED_R, 0);

    return 0;
}


int board_early_init_f(void)
{
    /* LED's */
    board_led_init();

    /* Ethernet 0 PHY Reset:  */
    //pfc_set_gpio(PORT_ET0_RESETn, GPIO_OUT);
    //gpio_set(PORT_ET0_RESETn, 1);

    /* Ethernet 1 PHY Reset:  */
    //pfc_set_gpio(PORT_ET1_RESETn, GPIO_OUT);
    //gpio_set(PORT_ET1_RESETn, 1);


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

    /* set P5_0 as Func.1 for VBUSEN */
    pfc_set_pin_function(PORT_USB0_VBUSEN, FUNC1);

    /* set P5_2 as Func.1 for OVERCUR */
    pfc_set_pin_function(PORT_USB0_OVRCUR, FUNC1);

    /* set P18_4 as Func.4 for VBUSEN */
    pfc_set_pin_function(PORT_USB1_VBUSEN, FUNC4);

    /* set P18_5 as Func.4 for OVERCUR */
    pfc_set_pin_function(PORT_USB1_OVRCUR, FUNC4);

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
    gd->bd->bi_boot_params = CONFIG_TEXT_BASE + 0x50000;

    board_usb_init();

    return 0;
}


int board_late_init(void)
{
    int off, retval = 0;
    struct udevice *iic_dev;

    /* Set clocks */ 
    retval = i2c_get_chip_for_busnum(CONFIG_I2C_DEFAULT_BUS_NUMBER, I2C_VERSACLOCK_ADDR, 1, &iic_dev);

    if (retval == 0)
    {
        retval = dm_i2c_write(iic_dev, 0u, ren_5p35023b_settings, sizeof(ren_5p35023b_settings));
    }
    else
    {
        printf("%s: Couldn't setup system clocks!\n", __func__);
    }

#ifdef CONFIG_RENESAS_RZG2LWDT
    rzg2l_reinitr_wdt();
#endif // CONFIG_RENESAS_RZG2LWDT

    /* Read Mac Address and set*/
    //retval = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "eeprom@50", &iic_dev);
    retval = i2c_get_chip_for_busnum(CONFIG_SYS_I2C_EEPROM_BUS, CONFIG_SYS_I2C_EEPROM_ADDR, CONFIG_SYS_I2C_EEPROM_ADDR_LEN, &iic_dev);
    if (retval == 0)
    {
        const int ETH_ADDR_LEN = 6;
        uint8_t ethaddr[ETH_ADDR_LEN];

        retval = dm_i2c_read(iic_dev, ETHER_EEPROM_ADDR, ethaddr, ETH_ADDR_LEN);
        if (retval == 0 ) 
        {
            if (is_valid_ethaddr(ethaddr))
            {
                eth_env_set_enetaddr("ethaddr", ethaddr);
            }
            else
            {
                printf("Invalid ethaddr format in eeprom!\n");
            }
        }
        else
        {
            printf("Error reading configuration EEPROM!\n");
        }
    }
    else
    {
        printf("%s: Cannot find EEPROM!\n", __func__);
    }
  
    if (retval == 0) {
        gpio_set(PORT_LED_R, 1);
    }
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
