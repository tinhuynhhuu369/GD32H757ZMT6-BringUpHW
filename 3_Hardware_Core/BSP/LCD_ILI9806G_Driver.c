/*!
    \file    LCD_ILI9806G_Driver.c
    \brief   ILI9806G 16-bit 8080 LCD driver over EXMC (FSMC) - GD32H757ZMT6

    Ported from the multi-platform driver (original kept in BSP/original/).
    Only the GD32H7 path is kept. Init sequences are unchanged.
*/

#include "LCD_ILI9806G_Driver.h"
#include "systick.h"

/* ---------------------------------------------------------------------------
 * EXMC timing (CK_EXMC = CK_AHB = 300 MHz -> 3.33 ns / cycle)
 *
 * ILI9806G (datasheet, 8080-I):
 *   - Address setup time (R/W)  10/0 ns
 *   - Address hold time (R/W)   2 ns
 *   - Data setup time           15 ns
 *   - Data hold time            10 ns
 *   - Read cycle (FM)           450 ns, RDX low 355 ns
 *
 * Bring-up values are intentionally slow. Tune later (plan.md Phase 3).
 * ------------------------------------------------------------------------- */
#define LCD_WR_ADDSET           15U     /* 0..15   */
#define LCD_WR_ADDHLD           1U      /* 1..15 (unused in mode A) */
#define LCD_WR_DATAST           15U     /* 1..255  -> NWE low ~50 ns */
#define LCD_WR_BUSLAT           1U      /* 0..15   */

#define LCD_RD_ADDSET           15U
#define LCD_RD_ADDHLD           1U
#define LCD_RD_DATAST           120U    /* NOE low ~400 ns */
#define LCD_RD_BUSLAT           2U

#define LCD_GPIO_SPEED          GPIO_OSPEED_85MHZ

volatile uint16_t g_lcd_id[4] = {0x00};
volatile bool     g_lcd_detected = false;

/* ---------------------------------------------------------------------------
 * Backlight (GPIO, LCD_BACKLIGHT_ON_LEVEL = level that turns it on)
 * ------------------------------------------------------------------------- */
static void ILI9806G_SetBacklight(bool on)
{
    if (on == (LCD_BACKLIGHT_ON_LEVEL != 0)) {
        gpio_bit_set(LCD_BACKLIGHT_PORT, LCD_BACKLIGHT_PIN);
    } else {
        gpio_bit_reset(LCD_BACKLIGHT_PORT, LCD_BACKLIGHT_PIN);
    }
}

void ILI9806G_LCD_BacklightInit(uint8_t bStep)
{
    rcu_periph_clock_enable(LCD_BACKLIGHT_CLK);

    /* set the level before switching to output to avoid a glitch */
    ILI9806G_SetBacklight(bStep != 0U);
    gpio_mode_set(LCD_BACKLIGHT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LCD_BACKLIGHT_PIN);
    gpio_output_options_set(LCD_BACKLIGHT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, LCD_BACKLIGHT_PIN);
}

void ILI9806G_LCD_SetBrightness(uint8_t bStep)
{
    /* no PWM yet: any non-zero step = on */
    ILI9806G_SetBacklight(bStep != 0U);
}

void ILI9806G_LCD_WriteRAM(uint16_t RGB_Code)
{
    NW_LCD_Write_Data(RGB_Code);
}

/* ---------------------------------------------------------------------------
 * Hardware reset: RES low 10 ms, high, wait 120 ms
 * ------------------------------------------------------------------------- */
static void ILI9806G_Reset(void)
{
    rcu_periph_clock_enable(LCDRES_CLK);

    gpio_bit_set(LCDRES_PORT, LCDRES_PIN);
    gpio_mode_set(LCDRES_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LCDRES_PIN);
    gpio_output_options_set(LCDRES_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, LCDRES_PIN);

    gpio_bit_reset(LCDRES_PORT, LCDRES_PIN);
    delay_1ms(10);
    gpio_bit_set(LCDRES_PORT, LCDRES_PIN);
    delay_1ms(120);
}

/* ---------------------------------------------------------------------------
 * EXMC pins: D0..D15, A0, NE2, NWE, NOE -> AF12
 * ------------------------------------------------------------------------- */
static void LCD_GPIO_AF12_Config(uint32_t port, uint32_t pins)
{
    gpio_af_set(port, GPIO_AF_12, pins);
    gpio_mode_set(port, GPIO_MODE_AF, GPIO_PUPD_NONE, pins);
    gpio_output_options_set(port, GPIO_OTYPE_PP, LCD_GPIO_SPEED, pins);
}

static void LCD_CtrlLinesConfig(void)
{
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOF);
    rcu_periph_clock_enable(RCU_GPIOG);

    /* EXMC_D[0-3, 13-15] + NOE (PD4) + NWE (PD5) */
    LCD_GPIO_AF12_Config(GPIOD, LCD_DATA_GPIOD_PINS | LCDRD_PIN | LCD_WRX_PIN);
    /* EXMC_D[4-12] */
    LCD_GPIO_AF12_Config(GPIOE, LCD_DATA_GPIOE_PINS);
    /* EXMC_A0 */
    LCD_GPIO_AF12_Config(LCD_DCX_PORT, LCD_DCX_PIN);
    /* EXMC_NE2 */
    LCD_GPIO_AF12_Config(LCDCS_PORT, LCDCS_PIN);
}

/* ---------------------------------------------------------------------------
 * EXMC bank0 region2: SRAM, 16 bit, async mode A, extended (separate R/W timing)
 * ------------------------------------------------------------------------- */
static void LCD_ILI9806G_FSMCConfig(void)
{
    exmc_norsram_parameter_struct        nor_init_struct;
    exmc_norsram_timing_parameter_struct read_timing;
    exmc_norsram_timing_parameter_struct write_timing;

    /* EXMC clock enable (source: CK_AHB by default) */
    rcu_periph_clock_enable(RCU_EXMC);

    exmc_norsram_struct_para_init(&nor_init_struct);

    /* read timing (also used as "read_write_timing" in extended mode) */
    read_timing.asyn_access_mode       = EXMC_ACCESS_MODE_A;
    read_timing.asyn_address_setuptime = LCD_RD_ADDSET;
    read_timing.asyn_address_holdtime  = LCD_RD_ADDHLD;
    read_timing.asyn_data_setuptime    = LCD_RD_DATAST;
    read_timing.bus_latency            = LCD_RD_BUSLAT;
    read_timing.syn_clk_division       = EXMC_SYN_CLOCK_RATIO_DISABLE;
    read_timing.syn_data_latency       = EXMC_DATALAT_2_CLK;

    /* write timing */
    write_timing.asyn_access_mode       = EXMC_ACCESS_MODE_A;
    write_timing.asyn_address_setuptime = LCD_WR_ADDSET;
    write_timing.asyn_address_holdtime  = LCD_WR_ADDHLD;
    write_timing.asyn_data_setuptime    = LCD_WR_DATAST;
    write_timing.bus_latency            = LCD_WR_BUSLAT;
    write_timing.syn_clk_division       = EXMC_SYN_CLOCK_RATIO_DISABLE;
    write_timing.syn_data_latency       = EXMC_DATALAT_2_CLK;

    nor_init_struct.norsram_region    = LCD_EXMC_REGION;
    nor_init_struct.address_data_mux  = DISABLE;
    nor_init_struct.memory_type       = EXMC_MEMORY_TYPE_SRAM;
    nor_init_struct.databus_width     = EXMC_NOR_DATABUS_WIDTH_16B;
    nor_init_struct.burst_mode        = DISABLE;
    nor_init_struct.nwait_polarity    = EXMC_NWAIT_POLARITY_LOW;
    nor_init_struct.nwait_config      = EXMC_NWAIT_CONFIG_BEFORE;
    nor_init_struct.memory_write      = ENABLE;
    nor_init_struct.nwait_signal      = DISABLE;
    nor_init_struct.extended_mode     = ENABLE;
    nor_init_struct.asyn_wait         = DISABLE;
    nor_init_struct.cram_page_size    = EXMC_CRAM_AUTO_SPLIT;
    nor_init_struct.write_mode        = EXMC_ASYN_WRITE;
    nor_init_struct.read_write_timing = &read_timing;
    nor_init_struct.write_timing      = &write_timing;
    exmc_norsram_init(&nor_init_struct);

    exmc_norsram_enable(LCD_EXMC_REGION);
}

/* ---------------------------------------------------------------------------
 * Init sequences (unchanged from original driver)
 * ------------------------------------------------------------------------- */
#if (USING_OLD_ILI9806G_LCD)
static void ILI9806G_LCD_Config(void)
{
#if USE_NEW_LCD_POWER7
    //************* Start Initial Sequence **********//    
    NW_LCD_Write_Cmd(0x00FF); // EXTC Command Set enable register
    NW_LCD_Write_Data(0x00FF);
    NW_LCD_Write_Data(0x0098);
    NW_LCD_Write_Data(0x0006);

//    NW_LCD_Write_Cmd(0x00BA); // SPI Interface Setting 
//    NW_LCD_Write_Data(0x0060); 

    NW_LCD_Write_Cmd(0x00BC); // GIP 1
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x000F);
    NW_LCD_Write_Data(0x0061);
    NW_LCD_Write_Data(0x00FF);
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x000B);
    NW_LCD_Write_Data(0x0010);
    NW_LCD_Write_Data(0x0037);
    NW_LCD_Write_Data(0x0063);
    NW_LCD_Write_Data(0x00FF);
    NW_LCD_Write_Data(0x00FF);
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x00FF);
    NW_LCD_Write_Data(0x0052);
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0040);

    NW_LCD_Write_Cmd(0x00BD); // GIP 2
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x0023);
    NW_LCD_Write_Data(0x0045);
    NW_LCD_Write_Data(0x0067);
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x0023);
    NW_LCD_Write_Data(0x0045);
    NW_LCD_Write_Data(0x0067);

    NW_LCD_Write_Cmd(0x00BE); // GIP 3
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0001);
    NW_LCD_Write_Data(0x00AB);
    NW_LCD_Write_Data(0x0060);
    NW_LCD_Write_Data(0x0022);
    NW_LCD_Write_Data(0x0022);
    NW_LCD_Write_Data(0x0022);
    NW_LCD_Write_Data(0x0022);
    NW_LCD_Write_Data(0x0022);

    NW_LCD_Write_Cmd(0x00C7); // VCOM Control
    NW_LCD_Write_Data(0x0036);

    NW_LCD_Write_Cmd(0x00ED); // EN_volt_reg     VGMP / VGMN /VGSP / VGSN voltage to output
    NW_LCD_Write_Data(0x007F);
    NW_LCD_Write_Data(0x000F);

    NW_LCD_Write_Cmd(0x00C0); // Power Control 1    Setting AVDD / AVEE / VGH / VGL
    NW_LCD_Write_Data(0x000F);
    NW_LCD_Write_Data(0x000B);
    NW_LCD_Write_Data(0x000A);//VGH 15V,VGLO-10V

    NW_LCD_Write_Cmd(0x00FC); //AVDD / AVEE generated by internal pumping. 
    NW_LCD_Write_Data(0x0008);

    NW_LCD_Write_Cmd(0x00DF); // Engineering Setting 
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0020);

    NW_LCD_Write_Cmd(0x00F3);//DVDD Voltage Setting 
    NW_LCD_Write_Data(0x0074);

    NW_LCD_Write_Cmd(0x00B4); // Display Inversion Control 
    NW_LCD_Write_Data(0x0002); 
    NW_LCD_Write_Data(0x0002); 
    NW_LCD_Write_Data(0x0002); 

    NW_LCD_Write_Cmd(0x00F7); // Resolution Control
    NW_LCD_Write_Data(0x0082);//480*800

    NW_LCD_Write_Cmd(0x0036);
    NW_LCD_Write_Data(ILI9806G_ROTATION);

    NW_LCD_Write_Cmd(0x00B1); // Frame Rate Control 
    NW_LCD_Write_Data(0x0000);
    NW_LCD_Write_Data(0x0013);
    NW_LCD_Write_Data(0x0013); 

    NW_LCD_Write_Cmd(0x00F2); //Panel Timing Control 
    NW_LCD_Write_Data(0x00C0); 
    NW_LCD_Write_Data(0x0004);
    NW_LCD_Write_Data(0x0040);
    NW_LCD_Write_Data(0x0028);

    NW_LCD_Write_Cmd(0x00C1); // Power Control 2     SD OP Bias_VRH1_VRH2_EXT_CPCK_SEL
    NW_LCD_Write_Data(0x0017);
    NW_LCD_Write_Data(0x0088);//VGMP
    NW_LCD_Write_Data(0x0088);//VGMN
    NW_LCD_Write_Data(0x0020);

    NW_LCD_Write_Cmd(0x00E0);   //Positive Gamma Control
    NW_LCD_Write_Data(0x0000); //P1
    NW_LCD_Write_Data(0x000A); //P2
    NW_LCD_Write_Data(0x0012); //P3
    NW_LCD_Write_Data(0x0010); //P4
    NW_LCD_Write_Data(0x000E); //P5
    NW_LCD_Write_Data(0x0020); //P6
    NW_LCD_Write_Data(0x00CC); //P7
    NW_LCD_Write_Data(0x0007); //P8
    NW_LCD_Write_Data(0x0006); //P9
    NW_LCD_Write_Data(0x000B); //P10
    NW_LCD_Write_Data(0x000E); //P11
    NW_LCD_Write_Data(0x000F); //P12
    NW_LCD_Write_Data(0x000D); //P13
    NW_LCD_Write_Data(0x0015); //P14
    NW_LCD_Write_Data(0x0010); //P15
    NW_LCD_Write_Data(0x0000); //P16

    NW_LCD_Write_Cmd(0x00E1); //Negative Gamma Correction
    NW_LCD_Write_Data(0x0000); //P1
    NW_LCD_Write_Data(0x000B); //P2
    NW_LCD_Write_Data(0x0013); //P3
    NW_LCD_Write_Data(0x000D); //P4
    NW_LCD_Write_Data(0x000E); //P5
    NW_LCD_Write_Data(0x001B); //P6
    NW_LCD_Write_Data(0x0071); //P7
    NW_LCD_Write_Data(0x0006); //P8
    NW_LCD_Write_Data(0x0006); //P9
    NW_LCD_Write_Data(0x000A); //P10
    NW_LCD_Write_Data(0x000F); //P11
    NW_LCD_Write_Data(0x000E); //P12
    NW_LCD_Write_Data(0x000F); //P13
    NW_LCD_Write_Data(0x0015); //P14
    NW_LCD_Write_Data(0x000C); //P15
    NW_LCD_Write_Data(0x0000); //P16

    NW_LCD_Write_Cmd(0x003A); // Pixel Format
    NW_LCD_Write_Data(0x0055);

    NW_LCD_Write_Cmd(0x0011); //Exit Sleep 
    delay_1ms(120); 

    ILI9806G_LCD_Fill_Screen(ILI9806G_WHITE);
    NW_LCD_Write_Cmd(0x0029); // Display On
    
#else
    //************* Start Initial Sequence **********//
    NW_LCD_Write_Cmd(0x00FF); // EXTC Command Set enable register 
    NW_LCD_Write_Data(0x00FF); 
    NW_LCD_Write_Data(0x0098); 
    NW_LCD_Write_Data(0x0006); 
	
    NW_LCD_Write_Cmd(0x00BA); // SPI Interface Setting 
    NW_LCD_Write_Data(0x0060); 
	
    NW_LCD_Write_Cmd(0x00BC); // GIP 1 
    NW_LCD_Write_Data(0x0003); 
    NW_LCD_Write_Data(0x000E); 
    NW_LCD_Write_Data(0x0003); 
    NW_LCD_Write_Data(0x0063); 
    NW_LCD_Write_Data(0x0001); 
    NW_LCD_Write_Data(0x0001); 
    NW_LCD_Write_Data(0x001B); 
    NW_LCD_Write_Data(0x0012); 
    NW_LCD_Write_Data(0x0039); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0001); 
    NW_LCD_Write_Data(0x0001); 
    NW_LCD_Write_Data(0x0002);
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x00FF); 
    NW_LCD_Write_Data(0x00F2); 
    NW_LCD_Write_Data(0x0001); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x00C0); 
    
    NW_LCD_Write_Cmd(0x00BD); // GIP 2 
    NW_LCD_Write_Data(0x0002); 
    NW_LCD_Write_Data(0x0013); 
    NW_LCD_Write_Data(0x0045); 
    NW_LCD_Write_Data(0x0067); 
    NW_LCD_Write_Data(0x0045); 
    NW_LCD_Write_Data(0x0067); 
    NW_LCD_Write_Data(0x0001); 
    NW_LCD_Write_Data(0x0023);
    
    NW_LCD_Write_Cmd(0x00BE); // GIP 3 
    NW_LCD_Write_Data(0x0001); 
    NW_LCD_Write_Data(0x0022); 
    NW_LCD_Write_Data(0x0022); 
    NW_LCD_Write_Data(0x00DC); 
    NW_LCD_Write_Data(0x00BA); 
    NW_LCD_Write_Data(0x0067); 
    NW_LCD_Write_Data(0x0022); 
    NW_LCD_Write_Data(0x0022); 
    NW_LCD_Write_Data(0x0022); 
    
    NW_LCD_Write_Cmd(0x00C7); // VCOM Control 
    NW_LCD_Write_Data(0x0069); 
    
    NW_LCD_Write_Cmd(0x00ED); // EN_volt_reg 
    NW_LCD_Write_Data(0x007F); 
    NW_LCD_Write_Data(0x000F); 
    NW_LCD_Write_Data(0x0000); 
    
    NW_LCD_Write_Cmd(0x00C0); // Power Control 1
    NW_LCD_Write_Data(0x0003); 
    NW_LCD_Write_Data(0x000B);
    NW_LCD_Write_Data(0x0000); 
    
    NW_LCD_Write_Cmd(0x00FC); // LVGL 
    NW_LCD_Write_Data(0x0008); 
    
    NW_LCD_Write_Cmd(0x00DF); // Engineering Setting 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0020); 
    
    NW_LCD_Write_Cmd(0x00F3); // DVDD Voltage Setting 
    NW_LCD_Write_Data(0x0074); 
    
    NW_LCD_Write_Cmd(0x00B4); // Display Inversion Control 
    NW_LCD_Write_Data(0x0002); 
    NW_LCD_Write_Data(0x0002); 
    NW_LCD_Write_Data(0x0002); 
    
    NW_LCD_Write_Cmd(0x00F7); // 480x800
    NW_LCD_Write_Data(0x0082);

    NW_LCD_Write_Cmd(0x0036);
    NW_LCD_Write_Data(ILI9806G_ROTATION);
    
    NW_LCD_Write_Cmd(0x00B1); // Frame Rate Control 
    NW_LCD_Write_Data(0x0000); 
    NW_LCD_Write_Data(0x0013); 
    NW_LCD_Write_Data(0x0013); 
    
    NW_LCD_Write_Cmd(0x00F2); //Panel Timing Control 
    NW_LCD_Write_Data(0x00C0); 
    NW_LCD_Write_Data(0x0004); 
    NW_LCD_Write_Data(0x0040); 
    NW_LCD_Write_Data(0x0028); 
    
    NW_LCD_Write_Cmd(0x00C1); // Power Control 2 
    NW_LCD_Write_Data(0x0017); 
    NW_LCD_Write_Data(0x0075); 
    NW_LCD_Write_Data(0x0075);
    NW_LCD_Write_Data(0x0020); 
    
    NW_LCD_Write_Cmd(0x00E0); 
    NW_LCD_Write_Data(0x0000); //P1 
    NW_LCD_Write_Data(0x0006); //P2 
    NW_LCD_Write_Data(0x000F); //P3 
    NW_LCD_Write_Data(0x0010); //P4 
    NW_LCD_Write_Data(0x0013); //P5 
    NW_LCD_Write_Data(0x0019); //P6 
    NW_LCD_Write_Data(0x000C); //P7 
    NW_LCD_Write_Data(0x000A); //P8 
    NW_LCD_Write_Data(0x0003); //P9 
    NW_LCD_Write_Data(0x0008); //P10 
    NW_LCD_Write_Data(0x0004); //P11 
    NW_LCD_Write_Data(0x000D); //P12 
    NW_LCD_Write_Data(0x0012); //P13 
    NW_LCD_Write_Data(0x002F); //P14 
    NW_LCD_Write_Data(0x002C); //P15 
    NW_LCD_Write_Data(0x0000); //P16
    
    NW_LCD_Write_Cmd(0x00E1); 
    NW_LCD_Write_Data(0x0000); //P1 
    NW_LCD_Write_Data(0x0002); //P2 
    NW_LCD_Write_Data(0x000A); //P3 
    NW_LCD_Write_Data(0x000F); //P4 
    NW_LCD_Write_Data(0x0011); //P5 
    NW_LCD_Write_Data(0x0017); //P6 
    NW_LCD_Write_Data(0x0008); //P7 
    NW_LCD_Write_Data(0x0009); //P8 
    NW_LCD_Write_Data(0x0003); //P9 
    NW_LCD_Write_Data(0x0009); //P10 
    NW_LCD_Write_Data(0x0006); //P11 
    NW_LCD_Write_Data(0x000B); //P12 
    NW_LCD_Write_Data(0x000A); //P13 
    NW_LCD_Write_Data(0x002C); //P14 
    NW_LCD_Write_Data(0x0026); //P15 
    NW_LCD_Write_Data(0x0000); //P16 
    
    NW_LCD_Write_Cmd(0x003A);
    NW_LCD_Write_Data(0x0055); 
    
    NW_LCD_Write_Cmd(0x0035); //Tearing Effect ON
    NW_LCD_Write_Data(0x0000); 
    
    NW_LCD_Write_Cmd(0x0011); //Exit Sleep 
    delay_1ms(120); 

    ILI9806G_LCD_Fill_Screen(ILI9806G_WHITE);
                                             
    NW_LCD_Write_Cmd(0x0029); // Display On
//    NW_LCD_Write_Cmd(0x0030); // Partial Mode On 
#endif
}
#endif /* USING_OLD_ILI9806G_LCD */

#if (!USING_OLD_ILI9806G_LCD)
static void ILI9806G_LCD_115FPC_B_Config(void)
{
	//************* Start Initial Sequence **********//
	NW_LCD_Write_Cmd(0x00FF); // EXTC Command Set enable register 
	NW_LCD_Write_Data(0x00FF); 
	NW_LCD_Write_Data(0x0098); 
	NW_LCD_Write_Data(0x0006); 
	
    NW_LCD_Write_Cmd(0x00BA); // SPI Interface Setting 
    NW_LCD_Write_Data(0x0060); 
	
	NW_LCD_Write_Cmd(0x00BC); // GIP 1 
	NW_LCD_Write_Data(0x0001); 
	NW_LCD_Write_Data(0x000E); 
	NW_LCD_Write_Data(0x0061); 
	NW_LCD_Write_Data(0x00FB); 
	NW_LCD_Write_Data(0x0010); 
	NW_LCD_Write_Data(0x0010); 
	NW_LCD_Write_Data(0x000B); 
	NW_LCD_Write_Data(0x000F); 
	NW_LCD_Write_Data(0x002E); 
	NW_LCD_Write_Data(0x0073); 
	NW_LCD_Write_Data(0x00FF); 
	NW_LCD_Write_Data(0x00FF); 
	NW_LCD_Write_Data(0x000E); 
	NW_LCD_Write_Data(0x000E); 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0003); 
	NW_LCD_Write_Data(0x0066); 
	NW_LCD_Write_Data(0x0063); 
	NW_LCD_Write_Data(0x0001); 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0000); 
	
	NW_LCD_Write_Cmd(0x00BD); // GIP 2 
	NW_LCD_Write_Data(0x0001); 
	NW_LCD_Write_Data(0x0023); 
	NW_LCD_Write_Data(0x0045); 
	NW_LCD_Write_Data(0x0067); 
	NW_LCD_Write_Data(0x0001); 
	NW_LCD_Write_Data(0x0023); 
	NW_LCD_Write_Data(0x0045); 
	NW_LCD_Write_Data(0x0067); 
	
	NW_LCD_Write_Cmd(0x00BE); // GIP 3 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0021); 
	NW_LCD_Write_Data(0x00AB); 
	NW_LCD_Write_Data(0x0060); 
	NW_LCD_Write_Data(0x0022); 
	NW_LCD_Write_Data(0x0022); 
	NW_LCD_Write_Data(0x0022); 
	NW_LCD_Write_Data(0x0022); 
	NW_LCD_Write_Data(0x0022); 
	
	NW_LCD_Write_Cmd(0x00C7); // Vcom 
	NW_LCD_Write_Data(0x006F); 
	
	NW_LCD_Write_Cmd(0x00ED); // EN_volt_reg 
	NW_LCD_Write_Data(0x007F); 
	NW_LCD_Write_Data(0x000F); 
	NW_LCD_Write_Data(0x0000); 
	
	NW_LCD_Write_Cmd(0x00C0); // Power Control 1
	NW_LCD_Write_Data(0x0037); 
	NW_LCD_Write_Data(0x000B); 
	NW_LCD_Write_Data(0x000A); 
	
	NW_LCD_Write_Cmd(0x00FC); // LVGL 
	NW_LCD_Write_Data(0x000A); 
	
	NW_LCD_Write_Cmd(0x00DF); // Engineering Setting 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0020); 
	
	NW_LCD_Write_Cmd(0x00F3); // DVDD Voltage Setting 
	NW_LCD_Write_Data(0x0074); 
	
	NW_LCD_Write_Cmd(0x00B4); // Display Inversion Control 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0000); 
	
	NW_LCD_Write_Cmd(0x00F7); // 480x800
	NW_LCD_Write_Data(0x0082);
	
	NW_LCD_Write_Cmd(0x0036);
	NW_LCD_Write_Data(ILI9806G_ROTATION);
	
	NW_LCD_Write_Cmd(0x00B1); // Frame Rate 
	NW_LCD_Write_Data(0x0000); 
	NW_LCD_Write_Data(0x0012); 
	NW_LCD_Write_Data(0x0013); 
	
	NW_LCD_Write_Cmd(0x00F2); //Panel Timing Control 
	NW_LCD_Write_Data(0x0080); 
	NW_LCD_Write_Data(0x005B); 
	NW_LCD_Write_Data(0x0040); 
	NW_LCD_Write_Data(0x0028); 
	
	NW_LCD_Write_Cmd(0x00C1); // Power Control 2 
	NW_LCD_Write_Data(0x0017); 
	NW_LCD_Write_Data(0x007D); 
	NW_LCD_Write_Data(0x007A); 
	NW_LCD_Write_Data(0x0020); 
	
	NW_LCD_Write_Cmd(0x00E0); 
	NW_LCD_Write_Data(0x0000); //P1 
	NW_LCD_Write_Data(0x0011); //P2 
	NW_LCD_Write_Data(0x001C); //P3 
	NW_LCD_Write_Data(0x000E); //P4 
	NW_LCD_Write_Data(0x000F); //P5 
	NW_LCD_Write_Data(0x000C); //P6 
	NW_LCD_Write_Data(0x00C7); //P7 
	NW_LCD_Write_Data(0x0006); //P8 
	NW_LCD_Write_Data(0x0006); //P9 
	NW_LCD_Write_Data(0x000A); //P10 
	NW_LCD_Write_Data(0x0010); //P11 
	NW_LCD_Write_Data(0x0012); //P12 
	NW_LCD_Write_Data(0x000A); //P13 
	NW_LCD_Write_Data(0x0010); //P14 
	NW_LCD_Write_Data(0x0002); //P15 
	NW_LCD_Write_Data(0x0000); //P16 
	
	NW_LCD_Write_Cmd(0x00E1); 
	NW_LCD_Write_Data(0x0000); //P1 
	NW_LCD_Write_Data(0x0012); //P2 
	NW_LCD_Write_Data(0x0018); //P3 
	NW_LCD_Write_Data(0x000C); //P4 
	NW_LCD_Write_Data(0x000F); //P5 
	NW_LCD_Write_Data(0x000A); //P6 
	NW_LCD_Write_Data(0x0077); //P7 
	NW_LCD_Write_Data(0x0006); //P8 
	NW_LCD_Write_Data(0x0007); //P9 
	NW_LCD_Write_Data(0x000A); //P10 
	NW_LCD_Write_Data(0x000E); //P11 
	NW_LCD_Write_Data(0x000B); //P12 
	NW_LCD_Write_Data(0x0010); //P13 
	NW_LCD_Write_Data(0x001D); //P14 
	NW_LCD_Write_Data(0x0017); //P15 
	NW_LCD_Write_Data(0x0000); //P16 

    NW_LCD_Write_Cmd(0x003A);
    NW_LCD_Write_Data(0x0055);
	
	NW_LCD_Write_Cmd(0x0035); //Tearing Effect ON 
	NW_LCD_Write_Data(0x0000); 
	
	NW_LCD_Write_Cmd(0x0011); //Exit Sleep 
	delay_1ms(500); 
	
	ILI9806G_LCD_Fill_Screen(ILI9806G_WHITE);
	
	NW_LCD_Write_Cmd(0x0029); // Display On
	//    NW_LCD_Write_Cmd(0x0030); // Partial Mode On
}
#endif /* !USING_OLD_ILI9806G_LCD */

/* ---------------------------------------------------------------------------
 * ID read: RDDID (0x04) -> dummy, ID1, ID2, ID3 (on D[7:0])
 * ------------------------------------------------------------------------- */
const uint16_t* ILI9806G_ReadJDECID(void)
{
    NW_LCD_Write_Cmd(0x0004);
    g_lcd_id[0] = NW_LCD_Read_Data();   /* dummy */
    g_lcd_id[1] = NW_LCD_Read_Data();
    g_lcd_id[2] = NW_LCD_Read_Data();
    g_lcd_id[3] = NW_LCD_Read_Data();

    return (const uint16_t *)g_lcd_id;
}

bool ILI9806G_LCD_IsDetected(void)
{
    /* Default Value
       ID0 - dummy data
       ID1 ID2 ID3
       0x00 0x80 0x00 */
    const uint16_t *id = ILI9806G_ReadJDECID();

    return ((id[1] & 0xFFU) == ILI9806G_ID1) &&
           ((id[2] & 0xFFU) == ILI9806G_ID2) &&
           ((id[3] & 0xFFU) == ILI9806G_ID3);
}

void ILI9806G_LCD_Function_Test(void)
{
    ILI9806G_LCD_Fill_Screen(ILI9806G_BLUE);
    delay_1ms(500);

    ILI9806G_LCD_Fill_Screen(ILI9806G_RED);
    delay_1ms(500);

    ILI9806G_LCD_Fill_Screen(ILI9806G_GREEN);
    delay_1ms(500);

    ILI9806G_LCD_Fill_Screen(ILI9806G_MAGENTA);
    delay_1ms(500);

    /* orientation check: red square top-left, green bottom-right, frame in yellow */
    ILI9806G_LCD_Fill_Screen(ILI9806G_WHITE);
    ILI9806G_LCD_Fill_Rect(0, 0, 49, 49, ILI9806G_RED);
    ILI9806G_LCD_Fill_Rect(ILI9806G_WIDTH - 50, ILI9806G_HEIGHT - 50,
                           ILI9806G_WIDTH - 1, ILI9806G_HEIGHT - 1, ILI9806G_GREEN);
    ILI9806G_LCD_DrawHLine(0, 0, ILI9806G_WIDTH, ILI9806G_YELLOW);
    ILI9806G_LCD_DrawHLine(0, ILI9806G_HEIGHT - 1, ILI9806G_WIDTH, ILI9806G_YELLOW);
    ILI9806G_LCD_DrawVLine(0, 0, ILI9806G_HEIGHT, ILI9806G_YELLOW);
    ILI9806G_LCD_DrawVLine(ILI9806G_WIDTH - 1, 0, ILI9806G_HEIGHT, ILI9806G_YELLOW);
    delay_1ms(1000);
}

bool ILI9806G_LCD_Init(void)
{
    /* keep backlight off while the panel is being initialised */
    ILI9806G_LCD_BacklightInit(0);

    /* Config Pins for LCD */
    LCD_CtrlLinesConfig();

    /* Config FSMC Bus */
    LCD_ILI9806G_FSMCConfig();

    /* Reset LCD */
    ILI9806G_Reset();

    /* bus sanity check (result kept in g_lcd_detected / g_lcd_id) */
    g_lcd_detected = ILI9806G_LCD_IsDetected();

    /* LCD Config */
#if (USING_OLD_ILI9806G_LCD)
    ILI9806G_LCD_Config();
#else   /* Use new LCD 115FPC-B */
    ILI9806G_LCD_115FPC_B_Config();
#endif

    return g_lcd_detected;
}

void ILI9806G_LCD_Addr_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* set column address */
    NW_LCD_Write_Cmd(0x2a);
    NW_LCD_Write_Data(x1 >> 8);
    NW_LCD_Write_Data(x1 & 0x00FF);
    NW_LCD_Write_Data(x2 >> 8);
    NW_LCD_Write_Data(x2 & 0x00FF);

    /* set page address */
    NW_LCD_Write_Cmd(0x2b);
    NW_LCD_Write_Data(y1 >> 8);
    NW_LCD_Write_Data(y1 & 0x00FF);
    NW_LCD_Write_Data(y2 >> 8);
    NW_LCD_Write_Data(y2 & 0x00FF);

    /* Memory Write */
    NW_LCD_Write_Cmd(0x002C);
}

void ILI9806G_LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    ILI9806G_LCD_Addr_Set(x, y, x, y);
    ILI9806G_LCD_WriteRAM(color);
}

void ILI9806G_LCD_Fill_Rect(uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1,
                            uint16_t color)
{
    uint32_t count = (uint32_t)(x1 - x0 + 1U) * (uint32_t)(y1 - y0 + 1U);

    ILI9806G_LCD_Addr_Set(x0, y0, x1, y1);
    while (count--) {
        ILI9806G_LCD_WriteRAM(color);
    }
}

void ILI9806G_LCD_Fill_Screen(uint16_t color)
{
    ILI9806G_LCD_Fill_Rect(0, 0, ILI9806G_WIDTH - 1, ILI9806G_HEIGHT - 1, color);
}

void ILI9806G_LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t Length, uint32_t Color)
{
    ILI9806G_LCD_Addr_Set(x, y, x + Length - 1, y);
    for (uint16_t i = 0; i < Length; i++)
        ILI9806G_LCD_WriteRAM((uint16_t)Color);
}

void ILI9806G_LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t Length, uint32_t Color)
{
    ILI9806G_LCD_Addr_Set(x, y, x, y + Length - 1);
    for (uint16_t i = 0; i < Length; i++)
        ILI9806G_LCD_WriteRAM((uint16_t)Color);
}

void ILI9806G_LCD_DrawBitMap(uint32_t x, uint32_t y, uint8_t *pbmp)
{
    uint16_t i, j;
    uint32_t iImageSize;
    uint32_t iIdx;

    uint16_t sMaxX;
    uint16_t sMaxY;

    /*Read Bmp size*/
    iImageSize = *(pbmp + 1);
    iImageSize |= *(pbmp + 2) << 16;

    /*Read Data Idx*/
    iIdx = *(pbmp + 5);

    /*Read width*/
    sMaxX = *(pbmp + 9);

    /*Read Height*/
    sMaxY = *(pbmp + 11);

    iImageSize = (iImageSize - iIdx) / 2;
    (void)iImageSize;
    pbmp += (iIdx / 2);

    for(i = y + sMaxY; i > y; i--)
    {
        for(j = x; j < x + sMaxX; j++)
        {
            ILI9806G_LCD_DrawPixel(j, i - 1, (*pbmp));

            pbmp++;
            /*End of each row is 0x0000: seperate 2 rows -> ignore this pixel*/
            if(j == (x + sMaxX - 1))
            {
                pbmp++;
            }
        }
    }
}
