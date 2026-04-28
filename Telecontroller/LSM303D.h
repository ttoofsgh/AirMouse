#ifndef __LSM303D_H
#define __LSM303D_H

#include <stdint.h>

typedef struct{
	int16_t AccX; int16_t AccY; int16_t AccZ;
	int16_t MagnX; int16_t MagnY; int16_t MagnZ;
}Axis_Data;

//指定地址写
void LSM303D_WriteReg(uint8_t RegAddress, uint8_t Data);
//指定地址读
uint8_t LSM303D_ReadReg(uint8_t RegAddress);
//初始化
void LSM303D_Init(void);
//获取ID
uint8_t LSM303D_GetID(void);
//获取数据
void LSM303D_GetData(Axis_Data *AxisData);
// 初始化姿态解算（在 LSM303D_Init 中调用）
void LSM303D_AttitudeInit(void);
// 获取当前偏航角（单位：0.01 度，便于整数运算）
int16_t LSM303D_GetYaw(void);
// 获取偏航角变化量（自上次调用后的增量）
int16_t LSM303D_GetYawDelta(void);

#endif
