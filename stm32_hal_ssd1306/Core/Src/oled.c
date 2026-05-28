/**
 * @file oled.c
 * @brief 波特律动OLED驱动(SSD1306) - 已修复未定义引用
 */
#include "oled.h"
#include "i2c.h"
#include "font.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 引用外部信号发生器的变量 (需确保在 main.c 中定义)
extern uint32_t current_freq;
extern uint8_t wave_mode; 

// OLED器件地址
#define OLED_ADDRESS 0x78

// OLED参数
#define OLED_PAGE 8            // OLED页数
#define OLED_ROW 8 * OLED_PAGE // OLED行数
#define OLED_COLUMN 128        // OLED列数

// 显存
uint8_t OLED_GRAM[OLED_PAGE][OLED_COLUMN];

// ========================== 底层通信函数 ==========================

/**
 * @brief 向OLED发送数据的函数
 * 注意：如果你的I2C接口是hi2c1，请将下面的&hi2c2改为&hi2c1
 */
void OLED_Send(uint8_t *data, uint8_t len)
{
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, data, len, HAL_MAX_DELAY);
}

/**
 * @brief 向OLED发送指令
 */
void OLED_SendCmd(uint8_t cmd)
{
    static uint8_t sendBuffer[2] = {0};
    sendBuffer[0] = 0x00; // 命令模式
    sendBuffer[1] = cmd;
    OLED_Send(sendBuffer, 2);
}

// ========================== 业务逻辑函数 ==========================

/**
 * @brief 实时刷新显示当前的波形和频率
 */

// ========================== OLED驱动函数 ==========================

void OLED_Init()
{
    HAL_Delay(100); // 等待OLED上电稳定
    
    OLED_SendCmd(0xAE); /* 关闭显示 */
    OLED_SendCmd(0x20); /* 设置内存寻址模式 */
    OLED_SendCmd(0x10); /* 行寻址 */
    OLED_SendCmd(0xB0); /* 设置页地址 */
    OLED_SendCmd(0xC8); /* 反向扫描 */
    OLED_SendCmd(0x00); /* 设置低列地址 */
    OLED_SendCmd(0x10); /* 设置高列地址 */
    OLED_SendCmd(0x40); /* 设置起始行地址 */
    OLED_SendCmd(0x81); /* 设置对比度控制 */
    OLED_SendCmd(0xDF); 
    OLED_SendCmd(0xA1); /* 设置段重映射 */
    OLED_SendCmd(0xA6); /* 正常显示 */
    OLED_SendCmd(0xA8); /* 设置多路复用率 */
    OLED_SendCmd(0x3F);
    OLED_SendCmd(0xA4); /* 全屏点亮恢复 */
    OLED_SendCmd(0xD3); /* 设置显示偏移 */
    OLED_SendCmd(0x00);
    OLED_SendCmd(0xD5); /* 设置显示时钟分频 */
    OLED_SendCmd(0xF0);
    OLED_SendCmd(0xD9); /* 设置预充电周期 */
    OLED_SendCmd(0x22);
    OLED_SendCmd(0xDA); /* 设置COM硬件引脚配置 */
    OLED_SendCmd(0x12);
    OLED_SendCmd(0xDB); /* 设置VCOMH */
    OLED_SendCmd(0x20);
    OLED_SendCmd(0x8D); /* 电荷泵设置 */
    OLED_SendCmd(0x14);

    OLED_NewFrame();
    OLED_ShowFrame();
    OLED_SendCmd(0xAF); /* 开启显示 */
}

void OLED_DisPlay_On()
{
    OLED_SendCmd(0x8D); OLED_SendCmd(0x14); OLED_SendCmd(0xAF);
}

void OLED_DisPlay_Off()
{
    OLED_SendCmd(0x8D); OLED_SendCmd(0x10); OLED_SendCmd(0xAE);
}

void OLED_SetColorMode(OLED_ColorMode mode)
{
    if (mode == OLED_COLOR_NORMAL) OLED_SendCmd(0xA6);
    else OLED_SendCmd(0xA7);
}

// ========================== 显存操作函数 ==========================

void OLED_NewFrame()
{
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
}

void OLED_ShowFrame()
{
    static uint8_t sendBuffer[OLED_COLUMN + 1];
    sendBuffer[0] = 0x40; // 数据模式
    for (uint8_t i = 0; i < OLED_PAGE; i++)
    {
        OLED_SendCmd(0xB0 + i); 
        OLED_SendCmd(0x00);     
        OLED_SendCmd(0x10);     
        memcpy(sendBuffer + 1, OLED_GRAM[i], OLED_COLUMN);
        OLED_Send(sendBuffer, OLED_COLUMN + 1);
    }
}

void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color)
{
    if (x >= OLED_COLUMN || y >= OLED_ROW) return;
    if (color == OLED_COLOR_NORMAL) OLED_GRAM[y / 8][x] |= 1 << (y % 8);
    else OLED_GRAM[y / 8][x] &= ~(1 << (y % 8));
}

void OLED_SetByte_Fine(uint8_t page, uint8_t column, uint8_t data, uint8_t start, uint8_t end, OLED_ColorMode color)
{
    uint8_t temp;
    if (page >= OLED_PAGE || column >= OLED_COLUMN) return;
    if (color == OLED_COLOR_REVERSED) data = ~data;
    temp = data | (0xff << (end + 1)) | (0xff >> (8 - start));
    OLED_GRAM[page][column] &= temp;
    temp = data & ~(0xff << (end + 1)) & ~(0xff >> (8 - start));
    OLED_GRAM[page][column] |= temp;
}

void OLED_SetBits_Fine(uint8_t x, uint8_t y, uint8_t data, uint8_t len, OLED_ColorMode color)
{
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    if (bit + len > 8)
    {
        OLED_SetByte_Fine(page, x, data << bit, bit, 7, color);
        OLED_SetByte_Fine(page + 1, x, data >> (8 - bit), 0, len + bit - 9, color);
    }
    else
    {
        OLED_SetByte_Fine(page, x, data << bit, bit, bit + len - 1, color);
    }
}

void OLED_SetBits(uint8_t x, uint8_t y, uint8_t data, OLED_ColorMode color)
{
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    OLED_SetByte_Fine(page, x, data << bit, bit, 7, color);
    if (bit) OLED_SetByte_Fine(page + 1, x, data >> (8 - bit), 0, bit - 1, color);
}

void OLED_SetBlock(uint8_t x, uint8_t y, const uint8_t *data, uint8_t w, uint8_t h, OLED_ColorMode color)
{
    uint8_t fullRow = h / 8;
    uint8_t partBit = h % 8;
    for (uint8_t i = 0; i < w; i++)
    {
        for (uint8_t j = 0; j < fullRow; j++)
        {
            OLED_SetBits(x + i, y + j * 8, data[i + j * w], color);
        }
    }
    if (partBit)
    {
        uint16_t fullNum = w * fullRow;
        for (uint8_t i = 0; i < w; i++)
        {
            OLED_SetBits_Fine(x + i, y + (fullRow * 8), data[fullNum + i], partBit, color);
        }
    }
}

// ========================== 图形与文字绘制 ==========================

void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color)
{
    OLED_SetBlock(x, y, font->chars + (ch - ' ') * (((font->h + 7) / 8) * font->w), font->w, font->h, color);
}

void OLED_PrintASCIIString(uint8_t x, uint8_t y, char *str, const ASCIIFont *font, OLED_ColorMode color)
{
    while (*str)
    {
        OLED_PrintASCIIChar(x, y, *str, font, color);
        x += font->w;
        str++;
    }
}

/**
 * @brief 绘制一条线段 (Bresenham算法)
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color)
{
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (1)
    {
        OLED_SetPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// (此处根据需要可继续添加 DrawCircle 等函数，为节省空间暂略)