#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define OLED_WIDTH  128U
#define OLED_HEIGHT 64U

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_SetCursor(uint8_t x, uint8_t y);
void OLED_Print(const char *text);
void OLED_PrintAt(uint8_t x, uint8_t y, const char *text);
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t on);
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

#endif
