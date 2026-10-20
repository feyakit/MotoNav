#pragma once

// 微雪 ESP32-S3-Touch-AMOLED-2.16 引脚定义。
// 来源：微雪官方文档 https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-2.16/ 与官方 Arduino 示例。
// 与 1.75 的主要区别：面板同为 CO5300/QSPI 且数据引脚相同，但分辨率为 480x480、
// 构造偏移为 0（1.75 为 6）；GPIO16 是 SYS_OUT、GPIO17 是 QMI8658 中断，
// 不是背光/TE 引脚（AMOLED 无背光引脚，本板未引出 TE），切勿占用。

#define XPOWERS_CHIP_AXP2101

// LCD（CO5300，QSPI，480x480）
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define PANEL_WIDTH 480
#define PANEL_HEIGHT 480
#define LCD_WIDTH 480
#define LCD_HEIGHT 480
// 预留：与 185b 构建流程共用同一 PanelVariant 机制；当前固件未使用此值，
// 2.16 面板初始化走 GFX 库自带 CO5300 序列（与 1.75 相同）。
#define LCD_INIT_VARIANT 0

// TOUCH（CST9220，与 CST9217 同属 CST92xx 系列，SensorLib TouchDrvCST92xx 通用）
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40
#define BOARD_TOUCH_FLIP_X 0
#define BOARD_TOUCH_FLIP_Y 0

// ES8311（播放）/ ES7210（录音），官方音频引脚表：
// DSDIN GPIO8 = ESP 数据出（到 ES8311）；ASDOUT GPIO10 = ESP 数据入（来自 ES7210）
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8

#define MCLKPIN             42
#define BCLKPIN              9
#define WSPIN               45
#define DOPIN               10
#define DIPIN                8
#define PA                  46

// SD（SPI：CMD/CLK/D0/CS）
const int SDMMC_CLK = 2;
const int SDMMC_CMD = 1;
const int SDMMC_DATA = 3;
const int SDMMC_CS = 41;
