#include "hw_lcd.h"
#include "lcdfont.h"

#define 	delay_ms(X) 	delay_cycles( ( (CPUCLK_FREQ/1000) * (X) ) )

/* ---------------------------------------------------------------------------
 * 软件 SPI 时序控制（LCD_SPI_DELAY_NOP 在 hw_lcd.h 里，默认 2）
 * 80MHz 下 2 条 NOP 约 25ns，SCL 半周期约 100~150ns ⇒ 3~5MHz，
 * 比 ST7789 的写周期上限(约 15MHz)低得多，给数据建立/保持留足余量，
 * 杜邦线或排线长一点也不容易丢位（原来是“零延时”，数据建立时间只有十几 ns，偏紧）。
 * 花屏/闪烁就把 LCD_SPI_DELAY_NOP 加到 4/8；想刷得更快就写 0/1。
 * -------------------------------------------------------------------------*/
#define TFT_NOP1()          __asm volatile ("nop")
#if (LCD_SPI_DELAY_NOP > 0)
#define TFT_SPI_NOP()       do { unsigned char _n = (LCD_SPI_DELAY_NOP); while (_n--) { TFT_NOP1(); } } while (0)
#else
#define TFT_SPI_NOP()       do { } while (0)
#endif

/* GPIO 驱动能力：SPI 线用【高驱动】，边沿更陡，长线/杜邦线更稳 */
#define TFT_IO_DRIVE        DL_GPIO_DRIVE_STRENGTH_HIGH
#define TFT_IO_HIZ          DL_GPIO_HIZ_DISABLE

/* ===========================================================================
 * 显示屏引脚初始化（本板：软件 SPI + RES/DC/CS 三根控制线）
 * ---------------------------------------------------------------------------
 * 为什么是软件 SPI：
 *   本板屏的 SCL/SDA 接在 PA31(物理39脚)/PA22(物理18脚)。查 TI 官方 IOMUX 表，
 *   这两脚【没有任何 SPI 复用功能】；与它们并联的 PA4(44脚)/PA3(43脚) 虽然有
 *   SPI0 功能，但分别是 SPI0_POCI(MISO) 与 SPI0_CS1_POCI1，同样不是 MOSI，
 *   所以硬件 SPI 在本板上无法驱动这块屏，只能用 GPIO 翻转模拟。
 *   （06_ui_display 例程用的是 SPI1 = PB8/PB9 + PB10/11/14/26 控制脚，
 *     那是 LaunchPad 的接法，与本板布线不同，故此处不能用 SPI_LCD_INST。）
 * =========================================================================*/
void LCD_GPIO_Init(void)
{
    /* 1) 5 根信号线：推挽输出（高驱动能力，长线/杜邦线边沿更干净） */
    DL_GPIO_initDigitalOutputFeatures(TFT_SCK_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, TFT_IO_DRIVE, TFT_IO_HIZ);
    DL_GPIO_initDigitalOutputFeatures(TFT_SDA_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, TFT_IO_DRIVE, TFT_IO_HIZ);
    DL_GPIO_initDigitalOutputFeatures(TFT_RES_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, TFT_IO_DRIVE, TFT_IO_HIZ);
    DL_GPIO_initDigitalOutputFeatures(TFT_DC_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, TFT_IO_DRIVE, TFT_IO_HIZ);
    DL_GPIO_initDigitalOutputFeatures(TFT_CS_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, TFT_IO_DRIVE, TFT_IO_HIZ);

    DL_GPIO_enableOutput(TFT_SCK_PORT, TFT_SCK_PIN);
    DL_GPIO_enableOutput(TFT_SDA_PORT, TFT_SDA_PIN);
    DL_GPIO_enableOutput(TFT_RES_PORT, TFT_RES_PIN);
    DL_GPIO_enableOutput(TFT_DC_PORT,  TFT_DC_PIN);
    DL_GPIO_enableOutput(TFT_CS_PORT,  TFT_CS_PIN);

    /* 2) 空闲电平：CS 拉高(未选中)、SCL 拉低、DC 拉低、RES 先拉低（随后 lcd_init 复位） */
    LCD_CS_Set();
    TFT_SCK_Clr();
    LCD_DC_Clr();
    LCD_RES_Clr();

    /* 3) 【重要】引脚重合保护：
     *    PA4(44脚) 与 PA31 并联在 C0-SCL；PA3(43脚) 与 PA22 并联在 C0-SDA。
     *    这里把它们显式配置成“无上下拉的高阻输入”，不输出任何电平，
     *    避免两个推挽输出对顶短路。对应的驱动代码见下方注释块。 */
    DL_GPIO_initDigitalInputFeatures(TFT_ALT_SCK_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(TFT_ALT_SDA_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    /* --- 如确需改用 PA4/PA3 那一组驱动（此时必须把上面的 PA31/PA22 同样改成高阻
     *     输入，否则依旧对顶），再把下面 6 行取消注释、并把 board_pins.h 中
     *     TFT_SCK_x 与 TFT_SDA_x 改成 TFT_ALT_x 即可 —— 默认注释掉：
     *
     * DL_GPIO_initDigitalOutput(TFT_ALT_SCK_IOMUX);
     * DL_GPIO_initDigitalOutput(TFT_ALT_SDA_IOMUX);
     * DL_GPIO_enableOutput(TFT_ALT_SCK_PORT, TFT_ALT_SCK_PIN);
     * DL_GPIO_enableOutput(TFT_ALT_SDA_PORT, TFT_ALT_SDA_PIN);
     * DL_GPIO_setPins(TFT_ALT_SCK_PORT, TFT_ALT_SCK_PIN);
     * DL_GPIO_setPins(TFT_ALT_SDA_PORT, TFT_ALT_SDA_PIN);
     */

    /* 4) ★背光 BLK：板上的显示屏座没有这个脚，所以这里主动点亮它。
     *    ① BLK 接到 PB9（U4 第 61 脚）时 —— 由这里点亮；
     *    ② BLK 直接接 3.3V 时 —— 这里输出高电平也无害（PB9 空着）。
     *    无论哪种接法，少了这一步，一块完好的屏也会是全黑。 */
#ifdef TFT_BLK_ENABLE
    DL_GPIO_initDigitalOutputFeatures(TFT_BLK_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, TFT_IO_DRIVE, TFT_IO_HIZ);
    DL_GPIO_enableOutput(TFT_BLK_PORT, TFT_BLK_PIN);
    DL_GPIO_setPins(TFT_BLK_PORT, TFT_BLK_PIN);      /* 立刻点亮背光 */
#endif
    return;
}

/* ===========================================================================
 * 底层：软件 4 线 SPI 发送一个字节（MSB 先出，SCL 上升沿被 ST7789 采样）
 * 时序：SCL 拉低 → 摆好 SDA → 等 TFT_SPI_NOP() → SCL 拉高（采样）→ 等一拍
 * 约 3~5MHz，全屏 320x170 刷一次约 0.3 秒，静态显示完全够用。
 * =========================================================================*/
void spi_write_bus(unsigned char dat)
{
    unsigned char i;
    for (i = 0; i < 8; i++)
    {
        TFT_SCK_Clr();                       /* 时钟先拉低，数据才允许变化 */
        if (dat & 0x80) { TFT_SDA_Set(); }
        else            { TFT_SDA_Clr(); }
        TFT_SPI_NOP();                       /* 数据建立时间 tDS */
        TFT_SCK_Set();                       /* 上升沿：ST7789 采样 SDA */
        TFT_SPI_NOP();                       /* 高电平保持 tCHW/tSHW */
        dat <<= 1;
    }
    TFT_SCK_Clr();
}

void LCD_Writ_Bus(unsigned char dat)
{
	LCD_CS_Clr();
	spi_write_bus(dat);
  LCD_CS_Set();
}
void LCD_WR_DATA8(unsigned char dat)
{
	LCD_Writ_Bus(dat);
}
void LCD_WR_DATA(unsigned int dat)
{
	LCD_Writ_Bus(dat>>8);
	LCD_Writ_Bus(dat);
}
void LCD_WR_REG(unsigned char dat)
{
	LCD_DC_Clr();//写命令
	LCD_Writ_Bus(dat);
	LCD_DC_Set();//写数据
}
void LCD_Address_Set(unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2)
{
	LCD_WR_REG(0x2a);//列地址设置
	LCD_WR_DATA(x1+LCD_X_OFFSET);
	LCD_WR_DATA(x2+LCD_X_OFFSET);
	LCD_WR_REG(0x2b);//行地址设置
	LCD_WR_DATA(y1+LCD_Y_OFFSET);
	LCD_WR_DATA(y2+LCD_Y_OFFSET);
	LCD_WR_REG(0x2c);//储存器写
}
void lcd_init(void)
{
	/* 0. 显示屏 GPIO 初始化（软件 SPI 引脚 + RES/DC/CS + 背光，见 board_pins.h）
	 *    背光在这一步就点亮了：先亮背光，再初始化 —— 这样即使后面初始化有问题，
	 *    也能立刻区分“背光没亮(硬件/接线)”和“初始化没成功(SPI/参数)”。 */
	LCD_GPIO_Init();
	delay_ms(10);

	/* 1. 硬件复位：RES 低 ≥10ms（数据手册要求），拉高后必须等 ≥120ms 才能发命令 */
	LCD_RES_Clr();
	delay_ms(20);
	LCD_RES_Set();
	delay_ms(120);

	/* 2. 软件复位 + 退出睡眠
	 *    ⚠ ST7789 数据手册规定：0x11(SLPOUT) 之后必须等 120ms 才能发下一条命令，
	 *    之前这版代码在 0x11 后面没有等待就直接灌了上百条参数，
	 *    这些参数有可能被芯片忽略（表现就是黑屏/花屏）。现在补上等待。 */
	LCD_WR_REG(0x01);          /* SWRESET 软复位 */
	delay_ms(150);
	LCD_WR_REG(0x11);          /* SLPOUT  退出睡眠 */
	delay_ms(150);

	/* 3. 关键参数（数值与 TI 06_ui_display 例程、你的 02_screen_driver 完全一致） */
	LCD_WR_REG(0x36);          /* MADCTL 扫描方向 */
	LCD_WR_DATA8(LCD_MADCTL);

	LCD_WR_REG(0x3A);          /* COLMOD 像素格式：16bit RGB565 */
	LCD_WR_DATA8(0x05);

	LCD_WR_REG(0xB2);          /* PORCTRL 前后沿 */
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x33);

	LCD_WR_REG(0xB7);          /* GCTRL 门控 */
	LCD_WR_DATA8(0x35);

	LCD_WR_REG(0xBB);          /* VCOMS */
	LCD_WR_DATA8(0x1A);

	LCD_WR_REG(0xC0);          /* LCMCTRL */
	LCD_WR_DATA8(0x2C);

	LCD_WR_REG(0xC2);          /* VDVVRHEN */
	LCD_WR_DATA8(0x01);

	LCD_WR_REG(0xC3);          /* VRHS */
	LCD_WR_DATA8(0x0B);

	LCD_WR_REG(0xC4);          /* VDVS */
	LCD_WR_DATA8(0x20);

	LCD_WR_REG(0xC6);          /* FRCTRL2 */
	LCD_WR_DATA8(0x0F);

	LCD_WR_REG(0xD0);          /* PWCTRL1 */
	LCD_WR_DATA8(0xA4);
	LCD_WR_DATA8(0xA1);

	LCD_WR_REG(0x21);          /* INVON 反显（IPS 面板必须开，否则颜色发白/反相） */

	LCD_WR_REG(0xE0);          /* PVGAMCTRL 正极性 Gamma */
	LCD_WR_DATA8(0xF0);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x04);
	LCD_WR_DATA8(0x04);
	LCD_WR_DATA8(0x04);
	LCD_WR_DATA8(0x05);
	LCD_WR_DATA8(0x29);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x3E);
	LCD_WR_DATA8(0x38);
	LCD_WR_DATA8(0x12);
	LCD_WR_DATA8(0x12);
	LCD_WR_DATA8(0x28);
	LCD_WR_DATA8(0x30);

	LCD_WR_REG(0xE1);          /* NVGAMCTRL 负极性 Gamma */
	LCD_WR_DATA8(0xF0);
	LCD_WR_DATA8(0x07);
	LCD_WR_DATA8(0x0A);
	LCD_WR_DATA8(0x0D);
	LCD_WR_DATA8(0x0B);
	LCD_WR_DATA8(0x07);
	LCD_WR_DATA8(0x28);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x3E);
	LCD_WR_DATA8(0x36);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x29);
	LCD_WR_DATA8(0x32);

	LCD_WR_REG(0x11);          /* SLPOUT（例程的写法，重复一次无害，保持与例程一致） */
	delay_ms(120);

	LCD_WR_REG(0x29);          /* DISPON 开显示 */
	delay_ms(20);

	/* 4. 背光再确认一次（万一前面被别的代码改过） */
	LCD_BLK_Set();
}

/* ===========================================================================
 * 背光控制：BLK 接在 PB9（U4 第 61 脚）时有效；BLK 直接接 3.3V 时本函数无效
 * =========================================================================*/
void LCD_Backlight(unsigned char on)
{
#ifdef TFT_BLK_ENABLE
    if (on) { DL_GPIO_setPins(TFT_BLK_PORT, TFT_BLK_PIN); }
    else    { DL_GPIO_clearPins(TFT_BLK_PORT, TFT_BLK_PIN); }
#else
    (void) on;
#endif
}

/* ===========================================================================
 * 上电自检图形
 * ---------------------------------------------------------------------------
 * 顺序：红 → 绿 → 蓝 → 白 → 黑（各 300ms），最后停在白底 + 黑框 + “TFT OK”。
 * 用它一眼判断黑屏到底是哪一层的问题：
 *   ① 连红/绿/蓝都没有（整屏黑的，背光也不亮）→ 背光或供电问题（见 board_pins.h 第二节）
 *   ② 有背光但一直黑，最后也没有 “TFT OK”   → SPI 接线(RES/DC/CS/SCL/SDA)或初始化问题
 *   ③ 红绿蓝的顺序/颜色不对（红显示成蓝）    → 面板需要 BGR，MADCTL 的 bit3 改成 1
 *   ④ 画面整体偏移/上下反了                  → 改 LCD_MADCTL / LCD_X_OFFSET / LCD_Y_OFFSET
 *   ⑤ 能正常显示 “TFT OK”                    → 硬件全对，接着看 1/2/3/4 那一屏
 * =========================================================================*/
void LCD_TestPattern(void)
{
    LCD_Fill(0, 0, LCD_W, LCD_H, RED);
    delay_ms(300);
    LCD_Fill(0, 0, LCD_W, LCD_H, GREEN);
    delay_ms(300);
    LCD_Fill(0, 0, LCD_W, LCD_H, BLUE);
    delay_ms(300);
    LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
    delay_ms(300);
    LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
    delay_ms(300);

    /* 结束画面：白底黑框 + 居中的 “TFT OK”（320x170 定位对不对也一并看出来了） */
    LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
    LCD_DrawRectangle(1, 1, LCD_W - 2, LCD_H - 2, BLACK);
    LCD_ShowString(112, 69, (const unsigned char *)"TFT OK", RED, WHITE, 32, 0);
}
/*******************************************************************************
 以下为基础UI部分
*******************************************************************************/

/******************************************************************************
      函数说明：在指定区域填充颜色
      入口数据：xsta,ysta   起始坐标
                xend,yend   终止坐标
								color       要填充的颜色
      返回值：  无
******************************************************************************/
void LCD_Fill(unsigned int xsta,unsigned int ysta,unsigned int xend,unsigned int yend,unsigned int color)
{
	unsigned int i,j;
	LCD_Address_Set(xsta,ysta,xend-1,yend-1);//设置显示范围
	for(i=ysta;i<yend;i++)
	{
		for(j=xsta;j<xend;j++)
		{
			LCD_WR_DATA(color);
		}
	}
}

/******************************************************************************
      函数说明：在指定位置画点
      入口数据：x,y 画点坐标
                color 点的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawPoint(unsigned int x,unsigned int y,unsigned int color)
{
	LCD_Address_Set(x,y,x,y);//设置光标位置
	LCD_WR_DATA(color);
}


/******************************************************************************
      函数说明：画线
      入口数据：x1,y1   起始坐标
                x2,y2   终止坐标
                color   线的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawLine(unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned int color)
{
	unsigned int t;
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //计算坐标增量
	delta_y=y2-y1;
	uRow=x1;//画线起点坐标
	uCol=y1;
	if(delta_x>0)incx=1; //设置单步方向
	else if (delta_x==0)incx=0;//垂直线
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if (delta_y==0)incy=0;//水平线
	else {incy=-1;delta_y=-delta_y;}
	if(delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴
	else distance=delta_y;
	for(t=0;t<distance+1;t++)
	{
		LCD_DrawPoint(uRow,uCol,color);//画点
		xerr+=delta_x;
		yerr+=delta_y;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}

//绘制垂直线
void LCD_DrawVerrticalLine(int x, int y1, int y2, unsigned int color)
{
	int i;
	LCD_Address_Set(x, y1, x, y1 + y2);
	for(i=y1; i <= y1 + y2; i++)
	{
		LCD_WR_DATA(color);
	}

}
/******************************************************************************
      函数说明：画矩形
      入口数据：x1,y1   起始坐标
                x2,y2   终止坐标
                color   矩形的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawRectangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2,unsigned int color)
{
	LCD_DrawLine(x1,y1,x2,y1,color);
	LCD_DrawLine(x1,y1,x1,y2,color);
	LCD_DrawLine(x1,y2,x2,y2,color);
	LCD_DrawLine(x2,y1,x2,y2,color);
}


/******************************************************************************
      函数说明：画圆
      入口数据：x0,y0   圆心坐标
                r       半径
                color   圆的颜色
      返回值：  无
******************************************************************************/
void Draw_Circle(unsigned int x0,unsigned int y0,unsigned char r,unsigned int color)
{
	int a,b;
	a=0;b=r;
	while(a<=b)
	{
		LCD_DrawPoint(x0-b,y0-a,color);             //3
		LCD_DrawPoint(x0+b,y0-a,color);             //0
		LCD_DrawPoint(x0-a,y0+b,color);             //1
		LCD_DrawPoint(x0-a,y0-b,color);             //2
		LCD_DrawPoint(x0+b,y0+a,color);             //4
		LCD_DrawPoint(x0+a,y0-b,color);             //5
		LCD_DrawPoint(x0+a,y0+b,color);             //6
		LCD_DrawPoint(x0-b,y0+a,color);             //7
		a++;
		if((a*a+b*b)>(r*r))//判断要画的点是否过远
		{
			b--;
		}
	}
}

void Drawarc(int x,int y,int a,int b,unsigned int r,unsigned int c)
{
    int x_tp, y_tp;
    int d; // Decision variable

    // Start with the top and bottom rows of the circle
    for (x_tp = 0; x_tp <= r; x_tp++) {
        // Calculate the corresponding y values
        y_tp = (int)sqrt(r * r - x_tp * x_tp);

        // Draw the horizontal lines from the circle edge to the center
        for (int i = x_tp; i >= -x_tp; i--) {
            LCD_DrawPoint(x + i, y - y_tp, c);
            LCD_DrawPoint(x + i, y + y_tp, c);
        }
    }

    // Now fill the rest of the circle
    d = 3 - 2 * r;
    while (x_tp > y_tp) {
        if (d < 0) {
            d += 4 * x_tp + 6;
        } else {
            d += 4 * (x_tp - y_tp) + 10;
            y_tp++;
        }
        x_tp--;

        // Draw the horizontal lines from the circle edge to the center
        for (int i = -x_tp; i <= x_tp; i++) {
            LCD_DrawPoint(x + i, y - y_tp, c);
            LCD_DrawPoint(x + i, y + y_tp, c);
        }

        // Draw the vertical lines from the circle edge to the center
        for (int i = -y_tp; i <= y_tp; i++) {
            LCD_DrawPoint(x + x_tp, y + i, c);
            LCD_DrawPoint(x - x_tp, y + i, c);
        }
    }
}

void LCD_ArcRect(unsigned int xsta,unsigned int ysta,unsigned int xend,unsigned int yend,unsigned int color)
{

	int r = 4;
	//画四条边
	LCD_DrawLine(xsta+r,ysta,xend-r,ysta,color);
	LCD_DrawLine(xsta,ysta+r,xsta,yend-r,color);
	LCD_DrawLine(xsta+r,yend,xend-r,yend,color);
	LCD_DrawLine(xend,ysta+r,xend,yend-r,color);

	//再画四个圆角
	Drawarc(xsta+r,ysta+r,90,180,r,color);//左上
	Drawarc(xend-r,ysta+r,0,90,r,color);//右上
	Drawarc(xsta+r,yend-r,180,270,r,color);//左下
	Drawarc(xend-r,yend-r,270,360,r,color);//右下

	//画五个实心矩形
	LCD_Fill(xsta+r,ysta,xend-r,ysta+r,color);//上
	LCD_Fill(xend-r,ysta+r,xend,yend-r,color);//右
	LCD_Fill(xsta+r,yend-r,xend-r,yend,color);//下
	LCD_Fill(xsta,ysta+r,xsta+r,yend-r,color);//左
	LCD_Fill(xsta+r,ysta+r,xend-r,yend-r,color);//中

}


/******************************************************************************
      函数说明：显示中英文字符串
      入口数据：x,y显示坐标
                *s 要显示的汉字串
                fc 字的颜色
                bc 字的背景色
                sizey 字号 可选 16 24 32
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode)
{
	while(*s!=0)
	{
		if(*s >= 128)
		{
			if(sizey==12) LCD_ShowChinese12x12(x,y,s,fc,bc,sizey,mode);
			else if(sizey==16) LCD_ShowChinese16x16(x,y,s,fc,bc,sizey,mode);
			else if(sizey==24) LCD_ShowChinese24x24(x,y,s,fc,bc,sizey,mode);
			else if(sizey==32) LCD_ShowChinese32x32(x,y,s,fc,bc,sizey,mode);
			else return;
			s+=2;
			x+=sizey;
		}
		else
		{
			LCD_ShowChar(x,y,*s,fc,bc,sizey,mode);
			s+=1;
			x+=(sizey/2);
		}
	}
}

/******************************************************************************
      函数说明：显示单个12x12汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese12x12(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode)
{
	unsigned char i,j,m=0;
	unsigned int k;
	unsigned int HZnum;//汉字数目
	unsigned int TypefaceNum;//一个字符所占字节大小
	unsigned int x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;

	HZnum=sizeof(tfont12)/sizeof(typFNT_GB12);	//统计汉字数目
	for(k=0;k<HZnum;k++)
	{
		if((tfont12[k].Index[0]==*(s))&&(tfont12[k].Index[1]==*(s+1)))
		{
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{
					if(!mode)//非叠加方式
					{
						if(tfont12[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont12[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}

/******************************************************************************
      函数说明：显示单个16x16汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese16x16(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode)
{
	unsigned char i,j,m=0;
	unsigned int k;
	unsigned int HZnum;//汉字数目
	unsigned int TypefaceNum;//一个字符所占字节大小
	unsigned int x0=x;
  TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;//32
	HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);	//统计汉字数目
	for(k=0;k<HZnum;k++)
	{
		if ((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)) )
		{
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{
					if(!mode)//非叠加方式
					{
						if(tfont16[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont16[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}


/******************************************************************************
      函数说明：显示单个24x24汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese24x24(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode)
{
	unsigned char i,j,m=0;
	unsigned int k;
	unsigned int HZnum;//汉字数目
	unsigned int TypefaceNum;//一个字符所占字节大小
	unsigned int x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);	//统计汉字数目
	for(k=0;k<HZnum;k++)
	{
		if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
		{
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{
					if(!mode)//非叠加方式
					{
						if(tfont24[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont24[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}

/******************************************************************************
      函数说明：显示单个32x32汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChinese32x32(unsigned int x,unsigned int y,unsigned char *s,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode)
{
	unsigned char i,j,m=0;
	unsigned int k;
	unsigned int HZnum;//汉字数目
	unsigned int TypefaceNum;//一个字符所占字节大小
	unsigned int x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);	//统计汉字数目
	for(k=0;k<HZnum;k++)
	{
		if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
		{
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{
					if(!mode)//非叠加方式
					{
						if(tfont32[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//叠加方式
					{
						if(tfont32[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//画一个点
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}


/******************************************************************************
      函数说明：显示单个字符
      入口数据：x,y显示坐标
                num 要显示的字符
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChar(unsigned int x,unsigned int y,unsigned char num,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode)
{
	unsigned char temp,sizex,t,m=0;
	unsigned int i,TypefaceNum;//一个字符所占字节大小
	unsigned int x0=x;
	sizex=sizey/2;
	TypefaceNum=(sizex/8+((sizex%8)?1:0))*sizey;
	num=num-' ';    //得到偏移后的值
	LCD_Address_Set(x,y,x+sizex-1,y+sizey-1);  //设置光标位置
	for(i=0;i<TypefaceNum;i++)
	{
		if(sizey==12)temp=ascii_1206[num][i];		       //调用6x12字体
		else if(sizey==16)temp=ascii_1608[num][i];		 //调用8x16字体
		else if(sizey==24)temp=ascii_2412[num][i];		 //调用12x24字体
		else if(sizey==32)temp=ascii_3216[num][i];		 //调用16x32字体
		else return;
		for(t=0;t<8;t++)
		{
			if(!mode)//非叠加模式
			{
				if(temp&(0x01<<t))LCD_WR_DATA(fc);
				else LCD_WR_DATA(bc);
				m++;
				if(m%sizex==0)
				{
					m=0;
					break;
				}
			}
			else//叠加模式
			{
				if(temp&(0x01<<t))LCD_DrawPoint(x,y,fc);//画一个点
				x++;
				if((x-x0)==sizex)
				{
					x=x0;
					y++;
					break;
				}
			}
		}
	}
}


/******************************************************************************
      函数说明：显示字符串
      入口数据：x,y显示坐标
                *p 要显示的字符串
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowString(unsigned int x,unsigned int y,const unsigned char *p,unsigned int fc,unsigned int bc,unsigned char sizey,unsigned char mode)
{
	while(*p!='\0')
	{
		LCD_ShowChar(x,y,*p,fc,bc,sizey,mode);
		x+=sizey/2;
		p++;
	}
}


/******************************************************************************
      函数说明：显示数字
      入口数据：m底数，n指数
      返回值：  无
******************************************************************************/
unsigned int mypow(unsigned char m,unsigned char n)
{
	unsigned int result=1;
	while(n--)result*=m;
	return result;
}


/******************************************************************************
      函数说明：显示整数变量
      入口数据：x,y显示坐标
                num 要显示整数变量
                len 要显示的位数
                fc 字的颜色
                bc 字的背景色
                sizey 字号
      返回值：  无
******************************************************************************/
void LCD_ShowIntNum(unsigned int x,unsigned int y,unsigned int num,unsigned char len,unsigned int fc,unsigned int bc,unsigned char sizey)
{
	unsigned char t,temp;
	unsigned char enshow=0;
	unsigned char sizex=sizey/2;
	for(t=0;t<len;t++)
	{
		temp=(num/mypow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+t*sizex,y,' ',fc,bc,sizey,0);
				continue;
			}else enshow=1;

		}
	 	LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
	}
}


/******************************************************************************
      函数说明：显示两位小数变量
      入口数据：x,y显示坐标
                num 要显示小数变量
                len 要显示的位数
                fc 字的颜色
                bc 字的背景色
                sizey 字号
      返回值：  无
******************************************************************************/
void LCD_ShowFloatNum1(unsigned int x,unsigned int y,float num,unsigned char len,unsigned int fc,unsigned int bc,unsigned char sizey)
{
	unsigned char t,temp,sizex;
	unsigned int num1;
	sizex=sizey/2;
	num1=num*100;
	for(t=0;t<len;t++)
	{
		temp=(num1/mypow(10,len-t-1))%10;
		if(t==(len-2))
		{
			LCD_ShowChar(x+(len-2)*sizex,y,'.',fc,bc,sizey,0);
			t++;
			len+=1;
		}
	 	LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
	}
}


/******************************************************************************
      函数说明：显示图片
      入口数据：x,y起点坐标
                length 图片长度
                width  图片宽度
                pic[]  图片数组
      返回值：  无
******************************************************************************/
void LCD_ShowPicture(unsigned int x,unsigned int y,unsigned int length,unsigned int width,const unsigned char pic[])
{
	unsigned int i,j;
	uint32_t k=0;
	LCD_Address_Set(x,y,x+length-1,y+width-1);
	for(i=0;i<length;i++)
	{
		for(j=0;j<width;j++)
		{
			LCD_WR_DATA8(pic[k*2]);
			LCD_WR_DATA8(pic[k*2+1]);
			k++;
		}
	}
}
