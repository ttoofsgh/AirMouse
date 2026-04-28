#ifndef __JOYSTICK_H
#define __JOYSTICK_H

#include <stdint.h>

// 初始化摇杆（ADC DMA 和按键）
void Joystick_Init(void);

// 获取最新 X、Y 值（范围 0~4095）
uint16_t Joystick_GetX(void);
uint16_t Joystick_GetY(void);

// 获取按键状态（按下返回 0，释放返回 1）
uint8_t Joystick_Button_Read(void);

#endif
