#ifndef __BLE_MOUSE_H
#define __BLE_MOUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化BLE鼠标
void BLEMouse_Init(void);

// 检查是否已连接到主机
bool BLEMouse_IsConnected(void);

// 当前是否处于灵敏度调节模式
bool BLEMouse_IsSensitivityMode(void);

//  获取当前灵敏度（挡位1~5）
uint8_t BLEMouse_GetSensitivityLevel(void);

// 发送鼠标移动（相对位移，-127 ~ 127）
void BLEMouse_Move(int8_t x, int8_t y);

// 发送鼠标按键（button: MOUSE_LEFT, MOUSE_RIGHT）
void BLEMouse_Click(uint8_t button);

// 发送滚轮滚动（-127 ~ 127）
void BLEMouse_Scroll(int8_t scroll);

// 在主循环中调用，处理内部状态
void BLEMouse_Update(void);

#ifdef __cplusplus
}
#endif

#endif
