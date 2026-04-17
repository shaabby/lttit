以下是从图片中提取并整理的**探索者V3 IO资源分配表**，包含所有引脚的详细信息：


| 引脚编号 | GPIO  | 连接资源1          | 连接资源2      | 完全独立 | 连接关系说明                                                                 |
|----------|-------|--------------------|----------------|----------|------------------------------------------------------------------------------|
| 34       | PA0   | WK_UP              |                | Y        | 1，按键 KEY_UP；2，可以做待机唤醒脚 (WKUP)                                   |
| 35       | PA1   | RMII_REF_CLK       |                | N        | 接 YT8512C 的 TXC 脚                                                         |
| 36       | PA2   | USART2_TX/RS485_RX | ETH_MDIO       | N        | 1，RS232 串口 2(COM2) RX 脚(P4 设置)；2，RS485 RX 脚(P4 设置)；3，YT8512C 的 MDIO 脚 |
| 37       | PA3   | USART2_RX/RS485_RX | PWM_DAC        | N        | 1，RS232 串口 2(COM2) TX 脚(P4 设置)；2，RS485 TX 脚(P4 设置)；3，PWM_DAC 输出脚 |
| 40       | PA4   | STM_DAC            | DCMI_HREF      | Y        | 1，DAC_OUT1 输出脚；2，OLED/CAMERA 接口的 HREF 脚                             |
| 41       | PA5   | STM_ADC            |                | Y        | ADC 输入引脚，同时做 TPAD 检测脚                                             |
| 42       | PA6   | DCMI_PCLK          |                | Y        | OLED/CAMERA 接口的 PCLK 脚                                                   |
| 43       | PA7   | RMII_CRS_DV        |                | N        | 接 YT8512C 的 CRS_DV 脚                                                      |
| 100      | PA8   | DCMI_XCLK          | REMOTE_IN      | N        | 1，OLED/CAMERA 接口的 XCLK 脚；2，接 LF0038 红外接收头                       |
| 101      | PA9   | USART1_TX          |                | Y        | 串口 1 TX 脚，默认连接 CH340 的 RX(P10 设置)                                 |
| 102      | PA10  | USART1_RX          |                | Y        | 串口 1 RX 脚，默认连接 CH340 的 TX(P10 设置)                                 |
| 103      | PA11  | USB_D-             | CAN_RX         | Y        | 1，USB D-引脚(P5 设置)；2，CAN_RX 引脚(P5 设置)                               |
| 104      | PA12  | USB_D+             | CAN_TX         | Y        | 1，USB D+引脚(P5 设置)；2，CAN_TX 引脚(P5 设置)                               |
| 105      | PA13  | JTMS               | SWDIO          | N        | JTAG/SWD 仿真接口,没接任何外设；注意：如要做普通 IO，需先禁止 JTAG&SWD         |
| 109      | PA14  | JTCK               | SWDCLK         | N        | JTAG/SWD 仿真接口,没接任何外设；注意：如要做普通 IO，需先禁止 JTAG&SWD         |
| 110      | PA15  | JTDI               | USB_PWR        | N        | 1，JTAG 仿真口(JTDI)；2，USB_HOST 接口供电控制脚(使用时，需先禁止 JTAG，才可以当普通 IO 使用) |
| 46       | PB0   | T_SCK              |                | Y        | TFTLCD 接口触摸屏 SCK 信号                                                   |
| 47       | PB1   | T_PEN              |                | Y        | TFTLCD 接口触摸屏 PEN 信号                                                   |
| 48       | PB2   | BOOT1              | T_MISO         | N        | 1，BOOT1,启动选择配置引脚(仅上电时用)；2，TFTLCD 接口触摸屏 MISO 信号         |
| 133      | PB3   | JTD0               | SPI1_SCK       | N        | 1，JTAG 仿真口(JTDO)；2，25Q128 和 WIRELESS 接口的 SCK 信号(使用时，需先禁止 JTAG，才可以当普通 IO 使用) |
| 134      | PB4   | JTRST              | SPI1_MISO      | N        | 1，JTAG 仿真口(JTRST)；2，25Q128 和 WIRELESS 接口的 MISO 信号(使用时，需先禁止 JTAG，才可以当普通 IO 使用) |
| 135      | PB5   |                    | SPI1_MOSI      | N        | 25Q128 和 WIRELESS 接口的 MOSI 信号                                          |
| 136      | PB6   | DCMI_D5            |                | Y        | OLED/CAMERA 接口的 D5 脚                                                     |
| 137      | PB7   | DCMI_VSYNC         |                | Y        | OLED/CAMERA 接口的 VSYNC 脚                                                  |
| 139      | PB8   | IIC_SCL            |                | N        | 接 24C02&ST480MC&ES8388 的 SCL                                               |
| 140      | PB9   | IIC_SDA            |                | N        | 接 24C02&ST480MC&ES8388 的 SDA                                               |
| 69       | PB10  | USART3_TX          |                | Y        | 1，RS232 串口 3(COM3) RX 脚(P2 设置)；2，ATK-MODULE 接口的 RXD 脚(P2 设置)    |
| 70       | PB11  | USART3_RX          |                | Y        | 1，RS232 串口 3(COM3) TX 脚(P2 设置)；2，ATK-MODULE 接口的 TXD 脚(P2 设置)    |
| 73       | PB12  | I2S_LRCK           |                | N        | ES8388 的 LRCK 信号                                                          |
| 74       | PB13  | I2S_SCLK           |                | N        | ES8388 的 SCLK 信号                                                          |
| 75       | PB14  | F_CS               |                | N        | 25Q128 的片选信号                                                            |
| 76       | PB15  | LCD_BL             |                | Y        | TFTLCD 接口背光控制脚                                                        |
| 26       | PC0   | GBC_LED            |                | N        | ATK-MODU接口的 LED 引LE 脚                                                   |
| 27       | PC1   | ETH_MDC            |                | N        | 接 YT8512C 的 MDC 脚                                                         |
| 28       | PC2   | I2S_SDOUT          |                | N        | ES8388 的 SDOUT 信号                                                         |
| 29       | PC3   | I2S_SDIN           |                | N        | ES8388 的 SDIN 信号                                                          |
| 44       | PC4   | RMII_RXD0          |                | N        | 接 YT8512C 的 RXD0 脚                                                        |
| 45       | PC5   | RMII_RXD1          |                | N        | 接 YT8512C 的 RXD1 脚                                                        |
| 96       | PC6   | I2S_MCLK           | DCMI_D0        | N        | 1，ES8388 的 MCLK 信号；2，OLED/CAMERA 接口的 D0 脚                           |
| 97       | PC7   |                    | DCMI_D1        | Y        | OLED/CAMERA 接口的 D1 脚                                                     |
| 98       | PC8   | SDIO_D0            | DCMI_D2        | N        | 1，SD 卡接口的 D0；2，OLED/CAMERA 接口的 D2 脚                               |
| 99       | PC9   | SDIO_D1            | DCMI_D3        | N        | 1，SD 卡接口的 D1；2，OLED/CAMERA 接口的 D3 脚                               |
| 111      | PC10  | SDIO_D2            |                | N        | SD 卡接口的 D2                                                               |
| 112      | PC11  | SDIO_D3            |                | N        | SD 卡接口的 D3                                                               |
| 113      | PC12  | SDIO_SCK           | DCMI_D4        | Y        | 1，SD 卡接口的 SCK；2，OLED/CAMERA 接口的 D4 脚                               |
| 7        | PC13  | T_CS               |                | Y        | TFTLCD 接口触摸屏 CS 信号                                                    |
| 8        | PC14  |                    | RTC 晶振       | N        | 接 32.768K 晶振，不可用做 IO                                                 |
| 9        | PC15  |                    | RTC 晶振       | N        | 接 32.768K 晶振，不可用做 IO                                                 |
| 114      | PD0   | FSMC_D2            |                | N        | FSMC 总线数据线 D2(LCD/SRAM 共用)                                            |
| 115      | PD1   | FSMC_D3            |                | N        | FSMC 总线数据线 D3(LCD/SRAM 共用)                                            |
| 116      | PD2   | SDIO_CMD           |                | N        | SD 卡接口的 CMD                                                              |
| 117      | PD3   | ETH_RESET          |                | N        | YT8512C 的复位脚                                                             |
| 118      | PD4   | FSMC_NOE           |                | N        | FSMC 总线 NOE(RD)(LCD/SRAM 共用)                                             |
| 119      | PD5   | FSMC_NWE           |                | N        | FSMC 总线 NWE(WR)(LCD/SRAM 共用)                                             |
| 122      | PD6   | DCMI_SCL           |                | Y        | OLED/CAMERA 接口的 SCL 脚                                                    |
| 123      | PD7   | DCMI_SDA           |                | Y        | OLED/CAMERA 接口的 SDA 脚                                                    |
| 77       | PD8   | FSMC_D13           |                | N        | FSMC 总线数据线 D13(LCD/SRAM 共用)                                           |
| 78       | PD9   | FSMC_D14           |                | N        | FSMC 总线数据线 D14(LCD/SRAM 共用)                                           |
| 79       | PD10  | FSMC_D15           |                | N        | FSMC 总线数据线 D15(LCD/SRAM 共用)                                           |
| 80       | PD11  | FSMC_A16           |                | N        | FSMC 总线地址线 A17(SRAM 专用)                                               |
| 81       | PD12  | FSMC_A17           |                | N        | FSMC 总线地址线 A18(SRAM 专用)                                               |
| 82       | PD13  | FSMC_A18           |                | N        | FSMC 总线地址线 A19(SRAM 专用)                                               |
| 85       | PD14  | FSMC_D0            |                | N        | FSMC 总线数据线 D0(LCD/SRAM 共用)                                            |
| 86       | PD15  | FSMC_D1            |                | N        | FSMC 总线数据线 D1(LCD/SRAM 共用)                                            |
| 141      | PE0   | FSMC_NBL0          |                | N        | FSMC 总线 NBL0(SRAM 专用)                                                    |
| 142      | PE1   | FSMC_NBL1          |                | N        | FSMC 总线 NBL1(SRAM 专用)                                                    |
| 1        | PE2   | KEY2               |                | Y        | 接按键 KEY2                                                                  |
| 2        | PE3   | KEY1               |                | Y        | 接按键 KEY1                                                                  |
| 3        | PE4   | KEY0               |                | Y        | 接按键 KEY0                                                                  |
| 4        | PE5   | DCMI_D6            |                | Y        | OLED/CAMERA 接口的 D6 脚                                                     |
| 5        | PE6   | DCMI_D7            |                | Y        | OLED/CAMERA 接口的 D7 脚                                                     |
| 58       | PE7   | FSMC_D4            |                | N        | FSMC 总线数据线 D4(LCD/SRAM 共用)                                            |
| 59       | PE8   | FSMC_D5            |                | N        | FSMC 总线数据线 D5(LCD/SRAM 共用)                                            |
| 60       | PE9   | FSMC_D6            |                | N        | FSMC 总线数据线 D6(LCD/SRAM 共用)                                            |
| 63       | PE10  | FSMC_D7            |                | N        | FSMC 总线数据线 D7(LCD/SRAM 共用)                                            |
| 64       | PE11  | FSMC_D8            |                | N        | FSMC 总线数据线 D8(LCD/SRAM 共用)                                            |
| 65       | PE12  | FSMC_D9            |                | N        | FSMC 总线数据线 D9(LCD/SRAM 共用)                                            |
| 66       | PE13  | FSMC_D10           |                | N        | FSMC 总线数据线 D10(LCD/SRAM 共用)                                           |
| 67       | PE14  | FSMC_D11           |                | N        | FSMC 总线数据线 D11(LCD/SRAM 共用)                                           |
| 68       | PE15  | FSMC_D12           |                | N        | FSMC 总线数据线 D12(LCD/SRAM 共用)                                           |
| 10       | PF0   | FSMC_A0            |                | N        | FSMC 总线地址线 A0(SRAM 专用)                                                |
| 11       | PF1   | FSMC_A1            |                | N        | FSMC 总线地址线 A1(SRAM 专用)                                                |
| 12       | PF2   | FSMC_A2            |                | N        | FSMC 总线地址线 A2(SRAM 专用)                                                |
| 13       | PF3   | FSMC_A3            |                | N        | FSMC 总线地址线 A3(SRAM 专用)                                                |
| 14       | PF4   | FSMC_A4            |                | N        | FSMC 总线地址线 A4(SRAM 专用)                                                |
| 15       | PF5   | FSMC_A5            |                | N        | FSMC 总线地址线 A5(SRAM 专用)                                                |
| 18       | PF6   | GBC_KEY            |                | Y        | 接 ATK-MODULE 接口的 KEY 脚                                                  |
| 19       | PF7   | LIGHT_SENSOR       |                | N        | 接光敏传感器(LS1)                                                            |
| 20       | PF8   | BEEP               |                | N        | 接蜂鸣器(BEEP)                                                               |
| 21       | PF9   | LED0               |                | N        | 接 DS0 LED 灯(红色)                                                          |
| 22       | PF10  | LED1               |                | N        | 接 DS1 LED 灯(绿色)                                                          |
| 49       | PF11  | T_MOSI             |                | Y        | TFTLCD 接口触摸屏 MOSI 信号                                                  |
| 50       | PF12  | FSMC_A6            |                | N        | FSMC 总线地址线 A10(SRAM/LCD 共用)                                           |
| 53       | PF13  | FSMC_A7            |                | N        | FSMC 总线地址线 A7(SRAM 专用)                                                |
| 54       | PF14  | FSMC_A8            |                | N        | FSMC 总线地址线 A8(SRAM 专用)                                                |
| 55       | PF15  | FSMC_A9            |                | N        | FSMC 总线地址线 A9(SRAM 专用)                                                |
| 56       | PG0   | FSMC_A10           |                | N        | FSMC 总线地址线 A10(SRAM 专用)                                               |
| 57       | PG1   | FSMC_A11           |                | N        | FSMC 总线地址线 A11(SRAM 专用)                                               |
| 87       | PG2   | FSMC_A12           |                | N        | FSMC 总线地址线 A12(SRAM 专用)                                               |
| 88       | PG3   | FSMC_A13           |                | N        | FSMC 总线地址线 A13(SRAM 专用)                                               |
| 89       | PG4   | FSMC_A14           |                | N        | FSMC 总线地址线 A14(SRAM 专用)                                               |
| 90       | PG5   | FSMC_A15           |                | N        | FSMC 总线地址线 A15(SRAM 专用)                                               |
| 91       | PG6   | NRF_CE             |                | Y        | WIRELESS 接口的 CE 信号                                                      |
| 92       | PG7   | NRF_CS             |                | Y        | WIRELESS 接口的 CS 信号                                                      |
| 93       | PG8   | NRF_IRQ            | RS485_RE       | N        | 1，WIRELESS 接口 IRQ 信号；2，RS485 RE 引脚                                  |
| 124      | PG9   | DCMI_PWDN          | 1WIRE_DQ       | N        | 1，OLED/CAMERA 接口的 PWDN 脚；2，接单总线接口(U19),即 DHT11/DS18B20          |
| 125      | PG10  | FSMC_NE3           |                | N        | FSMC 总线的片选信号 3，为外部 SRAM 片选信号                                   |
| 126      | PG11  | RMII_TX_EN         |                | N        | 接 YT8512C 的 TXEN 脚                                                        |
| 127      | PG12  | FSMC_NE4           |                | Y        | FSMC 总线的片选信号 4，为 LCD 片选信号                                        |
| 128      | PG13  | RMII_TXD0          |                | N        | 接 YT8512C 的 TXD0 脚                                                        |
| 129      | PG14  | RMII_TXD1          |                | N        | 接 YT8512C 的 TXD1 脚                                                        |
| 132      | PG15  | DCMI_RESET         |                | Y        | OLED/CAMERA 接口的 RESET 脚                                                  |


### 说明：
- **连接资源1/2**：表示该 GPIO 可复用的功能（如串口、SPI、I2C、FSMC 等）。
- **完全独立**：`Y` 表示该 GPIO 无复用冲突，可独立作为普通 IO 使用；`N` 表示存在复用功能，需根据需求配置（部分需关闭 JTAG/SWD 等调试接口）。
- **连接关系说明**：详细描述该引脚的实际硬件连接（如外设、接口、特殊功能等）。

可根据此表快速查询探索者 V3 开发板的 IO 资源分配及复用功能。