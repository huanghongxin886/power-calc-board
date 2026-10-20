#ifndef __HW_LCD_H
#define __HW_LCD_H

#include "ti_msp_dl_config.h"
#include "board_pins.h"

#define LCD_W 320
#define LCD_H 170

/* ---------------------------------------------------------------------------
 * ST7789 面板参数（换屏/换方向只改这 4 行，驱动 .c 不用动）
 * ---------------------------------------------------------------------------
 * LCD_MADCTL 位定义: D7=MY D6=MX D5=MV D4=ML D3=RGB/BGR D2=MH
 *     0x70 = MX|MV|ML  → 横屏 320x170（★本工程默认，与 TI 06_ui_display
 *                        例程、以及你 LaunchPad 上那份 02_screen_driver 完全一致）
 *     0x60 = MX|MV     → 横屏但上下镜像（画面上下反了时改这个）
 *     0x00 / 0xA0      → 竖屏 170x320（此时 LCD_W/LCD_H 与下面两个偏移要跟着换）
 * LCD_X_OFFSET / LCD_Y_OFFSET：GRAM 是 240x320，1.9" 面板可视区 170x320，
 *     横屏时列不用偏移、行偏移 (240-170)/2 = 35。
 * -------------------------------------------------------------------------*/
#define LCD_MADCTL        0x70
#define LCD_X_OFFSET      0
#define LCD_Y_OFFSET      35

/* 软件 SPI 每一位插入的空指令条数：0=最快，2≈3~5MHz（默认，最稳），
 * 4~8=更慢更抗干扰（杜邦线长/飞线乱时用），想刷屏更快就写 0 或 1 */
#define LCD_SPI_DELAY_NOP 2

#define screen_center_x (LCD_W/2)//屏幕中心X = 屏幕x/2
#define screen_center_y (LCD_H/2)//屏幕中心Y = 屏幕y/2

//-----------------LCD端口定义（本板引脚，详见 board_pins.h）----------------

/* 软件 SPI 位操作：ST7789 在 SCL 上升沿采样 SDA，MSB 先出 */
#define TFT_SCK_Clr()  DL_GPIO_clearPins(TFT_SCK_PORT, TFT_SCK_PIN)//SCL/SCK
#define TFT_SCK_Set()  DL_GPIO_setPins(TFT_SCK_PORT, TFT_SCK_PIN)
#define TFT_SDA_Clr()  DL_GPIO_clearPins(TFT_SDA_PORT, TFT_SDA_PIN)//SDA/MOSI
#define TFT_SDA_Set()  DL_GPIO_setPins(TFT_SDA_PORT, TFT_SDA_PIN)

#define LCD_RES_Clr()  DL_GPIO_clearPins(TFT_RES_PORT,TFT_RES_PIN)//RES
#define LCD_RES_Set()  DL_GPIO_setPins(TFT_RES_PORT,TFT_RES_PIN)

#define LCD_DC_Clr()   DL_GPIO_clearPins(TFT_DC_PORT,TFT_DC_PIN)//DC
#define LCD_DC_Set()   DL_GPIO_setPins(TFT_DC_PORT,TFT_DC_PIN)

#define LCD_CS_Clr()   DL_GPIO_clearPins(TFT_CS_PORT,TFT_CS_PIN)//CS
#define LCD_CS_Set()   DL_GPIO_setPins(TFT_CS_PORT,TFT_CS_PIN)

/* 背光 BLK：本板显示屏座上【没有】BLK 脚，所以：
 * A) 模块 BLK 直接接 3.3V（背光常亮）-> board_pins.h 里的 TFT_BLK_ENABLE 可关掉；
 * B) BLK 接到空闲脚 PB9 / U4 第 61 脚 -> 保持 TFT_BLK_ENABLE 打开（默认），
 *    驱动会在初始化时立刻把它拉高点亮背光。
 * ⚠ 如果 BLK 既没接 3.3V 也没接 PB9（就插在 7 脚座上），背光不亮 ⇒ 上电全黑。 */
#ifdef TFT_BLK_ENABLE
#define LCD_BLK_Clr()  DL_GPIO_clearPins(TFT_BLK_PORT,TFT_BLK_PIN)//BLK 低=灭
#define LCD_BLK_Set()  DL_GPIO_setPins(TFT_BLK_PORT,TFT_BLK_PIN)//BLK 高=亮
#else
#define LCD_BLK_Clr()  do {} while (0)
#define LCD_BLK_Set()  do {} while (0)
#endif

void LCD_GPIO_Init(void);
void spi_write_bus(unsigned char dat);
void LCD_Writ_Bus(unsigned char dat);
void LCD_WR_DATA8(unsigned char dat);
void LCD_WR_DATA(unsigned int dat);
void LCD_WR_REG(unsigned char dat);
void lcd_init(void);
void LCD_Backlight(unsigned char on);//背光开/关（TFT_BLK_ENABLE 打开时有效）
void LCD_TestPattern(void);//上电自检：红/绿/蓝/白/黑 → “TFT OK”
void LCD_Address_Set(unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2);
void LCD_Fill(unsigned int xsta,unsigned int ysta,unsigned int xend,unsigned int yend,unsigned int color);//指定区域填充颜色
void LCD_DrawPoint(unsigned int x,unsigned int y,unsigned int color);//在指定位置画一个点
void LCD_DrawLine(unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned int color);//在指定位置画一条线
void LCD_DrawRectangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2,unsigned int color);//在指定位置画一个矩形
void Draw_Circle(unsigned int x0,unsigned int y0,unsigned char r,unsigned int color);//在指定位置画一个圆

void LCD_ShowChinese(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode);//显示汉字串
void LCD_ShowChinese12x12(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode);//显示单个12x12汉字
void LCD_ShowChinese16x16(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode);//显示单个16x16汉字
void LCD_ShowChinese24x24(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode);//显示单个24x24汉字
void LCD_ShowChinese32x32(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode);//显示单个32x32汉字

void LCD_ShowChar(unsigned int x,unsigned int y,unsigned char num,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode);//显示一个字符
void LCD_ShowString(unsigned int x,unsigned int y,const unsigned char *p,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode);//显示字符串
unsigned int mypow(unsigned char m,unsigned char n);//求幂
void LCD_ShowIntNum(unsigned int x,unsigned int y,unsigned int num,unsigned char len,unsigned int fc,unsigned int bc,unsigned char sizey);//显示整数变量
void LCD_ShowFloatNum1(unsigned int x,unsigned int y,float num,unsigned char len,unsigned int fc,unsigned int bc,unsigned char sizey);//显示两位小数变量

void LCD_ShowPicture(unsigned int x,unsigned int y,unsigned int length,unsigned int width,const unsigned char pic[]);//显示图片

void LCD_ArcRect(unsigned int xsta,unsigned int ysta,unsigned int xend,unsigned int yend,unsigned int color);//画圆角矩形
void LCD_DrawVerrticalLine(int x, int y1, int y2, unsigned int color);
//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000
#define BLUE           	 0x001F
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //棕色
#define BRRED 			 0XFC07 //棕红色
#define GRAY  			 0X8430 //灰色
#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色
#define GRAYBLUE       	 0X5458 //灰蓝色
#define LIGHTGREEN     	 0X841F //浅绿色
#define LGRAY 			 0XC618 //浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

#endif
