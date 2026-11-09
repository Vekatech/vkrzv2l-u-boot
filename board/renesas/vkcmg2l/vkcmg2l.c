#include <common.h>
#include <cpu_func.h>
#include <image.h>
#include <init.h>
#include <hang.h>
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
#include <linux/delay.h>
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
#include <stdio.h>
#include <renesas/rzg_pfc.h>

DECLARE_GLOBAL_DATA_PTR;

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

#define USBPHY_BASE     0x11c40000
#define USB0_BASE       0x11c50000
#define USB1_BASE       0x11c70000
#define USBF_BASE       0x11c60000
#define USBPHY_RESET    (USBPHY_BASE + 0x000u)
#define COMMCTRL        0x800
#define HcRhDescriptorA 0x048
#define LPSTS           0x102

#define RPC_CMNCR       0x10060000

#define RESTART_ON_COLD_RESET

#define PFC_Pn(n)   (PFC_BASE + 0x0010ull + (n))      /* Port register R/W */
#define PFC_PMn(n)  (PFC_BASE + 0x0120ull + (n) * 2)  /* Port mode register R/W */
#define PFC_PMCn(n) (PFC_BASE + 0x0210ull + (n))      /* Port mode control register */
#define PFC_PFCn(n) (PFC_BASE + 0x0440ull + (n) * 4)  /* Port function control register */
#define PFC_PINn(n) (PFC_BASE + 0x0810ull + (n))      /* Port input register */

#define COMMA   ,
#define P(p, b)     p COMMA b

#define PORT_LED_R          P( 6, 1)
#define PORT_LED_G          P(17, 0)
#define PORT_LED_B          P(15, 1)
#define PORT_LED_Y          P(16, 0)

#define PORT_ET0_RESETn     P(22, 1)     
#define PORT_ET1_RESETn     P(33, 0)

#define PORT_USB0_VBUSEN    P( 4, 0)
#define PORT_USB0_OVRCUR    P( 5, 0)
#define PORT_USB1_VBUSEN    P( 8, 0)
#define PORT_USB1_OVRCUR    P( 8, 1)

#define PORT_SD_PWREN       P(39, 2)


/* WDT */
#define WDT_INDEX		0

enum pfc_pin_gpio_mode {GPIO_HiZ=0, GPIO_IN=1, GPIO_OUT=2, GPIO_IO=3};
enum pfc_pin_func_mode {FUNC0=0, FUNC1, FUNC2, FUNC3, FUNC4, FUNC5 };


// assigned-clock-rates = <12288000>, <25000000>,
//                           <32768>, <12288000>,
//                        <11289600>, <24000000>;
static const u8 ren_5p35023b_settings[] = {
    0x00, 0x00, 0x11, 0x19, 0x00, 0x42, 0xCC, 0x2B, 0x04, 0x32, 0x00, 0x1A, 0x5F, 0x12, 0x90, 0x79,
    0x02, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x24, 0x19, 0xBF, 0x3F, 0x30, 0x90, 0x86,
    0x80, 0x42, 0x45, 0xC4, 0x9C, 0x6E, 0x68, 0xD8, 0x9A, 0x7F, 0x00
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
__attribute__((weak)) u8 gpio_read(u8 n, u8 b)
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
    /* SD power control: = 1; */
    //pfc_set_gpio(PORT_SD_PWREN, GPIO_OUT); gpio_set(PORT_SD_PWREN, 1);

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
    *(volatile u32 *)(RPC_CMNCR) = 0x01FFF300;
}

#define WARM_RESET_PATTERN "This is an warm reset!"
#define RST_PATTERN_LEN (size_t)(sizeof(WARM_RESET_PATTERN)-1)
#define PATTERN_LOC 0x0002e000

#define I2C_PMIC_ADDR               0x12
#define I2C_PMIC_ADDR_LEN           1
#define I2C_PMIC_REG_SOFTRST        0x6D
#define I2C_PMIC_SOFTRST_COLDRST    (1<<0)
#define I2C_PMIC_SOFTRST_WARMRST    (1<<1)

void restart_on_cold_reset(void)
{
    if ( strncmp((const char *)PATTERN_LOC, WARM_RESET_PATTERN, RST_PATTERN_LEN) != 0 ) 
    { 
        strncpy((char *)PATTERN_LOC, WARM_RESET_PATTERN, RST_PATTERN_LEN);

        gpio_set(PORT_LED_Y, 1);

        printf("%s: Cold RESET detected!\n", __func__);

        mdelay(100);
       
#if 1 
        struct udevice *wdt_dev;
        if (uclass_get_device(UCLASS_WDT, WDT_INDEX, &wdt_dev) < 0) {
            printf("failed to get wdt device. cannot reset\n");
            return;
        }
        if (wdt_expire_now(wdt_dev, 1) < 0) {
            printf("failed to expire_now wdt\n");
        }
#else 
        struct udevice *iic_dev;

        if (i2c_get_chip_for_busnum(CONFIG_I2C_DEFAULT_BUS_NUMBER, I2C_PMIC_ADDR, I2C_PMIC_ADDR_LEN, &iic_dev) != 0 ) 
        {
            log_err("failed to get PMIC device. cannot reset\n");
            return;
        }

        if ( dm_i2c_reg_write(iic_dev, I2C_PMIC_REG_SOFTRST, (unsigned char)I2C_PMIC_SOFTRST_WARMRST) < 0)
        {
            log_err("failed to force PMIC software reset\n");
        }
        else
        {
            log_debug("Waiting for RESET!\n");
            hang();
        }
#endif
    }
    else
    {
        printf("%s: Warm RESET detected!\n", __func__);
    }
}

static int board_led_init(void)
{
    /* RED LED: = 0; */
    pfc_set_gpio(PORT_LED_R, GPIO_OUT); gpio_set(PORT_LED_R, 0);

    /* GREEN LED: = 0; */
    pfc_set_gpio(PORT_LED_G, GPIO_OUT); gpio_set(PORT_LED_G, 0);

    /* BLUE LED: = 0; */
    pfc_set_gpio(PORT_LED_B, GPIO_OUT); gpio_set(PORT_LED_B, 0);

    /* YELLOW LED: = 0; */
    pfc_set_gpio(PORT_LED_Y, GPIO_OUT); gpio_set(PORT_LED_Y, 0);

    return 0;
}

int board_early_init_f(void)
{
    /* LED's */
    board_led_init();

    gpio_set(PORT_LED_R, 1);

    /* Ethernet 0 PHY Reset:  = 1; */
    pfc_set_gpio(PORT_ET0_RESETn, GPIO_OUT);
    gpio_set(PORT_ET0_RESETn, 1);

    /* Ethernet 1 PHY Reset: = 1; */
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
    pfc_set_pin_function(PORT_USB0_VBUSEN, FUNC1);

    /* set P5_0 as Func.1 for OVERCUR */
    pfc_set_pin_function(PORT_USB0_OVRCUR, FUNC1);

    /* set P8_0 as Func.2 for VBUSEN */
    pfc_set_pin_function(PORT_USB1_VBUSEN, FUNC2);

    /* set P8_1 as Func.2 for OVERCUR */
    pfc_set_pin_function(PORT_USB1_OVRCUR, FUNC2);

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

    gpio_set(PORT_LED_G, 1);

    return 0;
}


int board_late_init(void)
{
    int retval = 0;
    struct udevice *iic_dev;

    /* Set clocks */ 
    retval = i2c_get_chip_for_busnum(CONFIG_I2C_DEFAULT_BUS_NUMBER, I2C_VERSACLOCK_ADDR, I2C_VERSACLOCK_ADDR_LEN, &iic_dev);

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

    if (env_get_yesno("restart_on_cold_reset")==1)
    {
        restart_on_cold_reset();
    }

    /* Read Mac Address and set*/
    //retval = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "eeprom@50", &iic_dev);
    retval = i2c_get_chip_for_busnum(CONFIG_SYS_I2C_EEPROM_BUS, I2C_EEPROM_MAC_ADDR, CONFIG_SYS_I2C_EEPROM_ADDR_LEN, &iic_dev);
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
        gpio_set(PORT_LED_B, 1);
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
