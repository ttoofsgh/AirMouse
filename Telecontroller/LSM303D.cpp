#include <Arduino.h>
#include <Wire.h>
#include "LSM303D_Reg.h"
#include "LSM303D.h"
#include <math.h>

// 与STM32不同，此处的I2C地址应为7位
#define LSM303D_ADDRESS  0x1D	//SA0接VCC

// 全局变量保存上一周期的航向角和初始标志
static float yaw_previous = 0.0f;
static bool yaw_initialized = false;

// 校准参数（可根据实际传感器偏差调整）
static int16_t mag_offset_x = 0;
static int16_t mag_offset_y = 0;
static int16_t mag_offset_z = 0;

// 将加速度计原始值转换为 g 值（±2g 量程，16 位有符号，1g = 16384 LSB）
#define ACCEL_SCALE (16384.0f)

// 磁力计比例因子（±2 gauss 量程，16 位有符号，1 gauss = 1024 LSB？LSM303D 磁力计 ±2 gauss 对应 0.080 mG/LSB，即 1 gauss = 1000 mG = 12500 LSB，但为简化直接用原始值）
// 这里不进行单位转换，直接使用原始值参与反正切计算，角度结果正确

// 低通滤波系数（用于平滑加速度计计算的角度）
#define ALPHA 0.2f

//指定地址写（可改为连续写）
void LSM303D_WriteReg(uint8_t RegAddress, uint8_t Data)
{
  Wire.beginTransmission(LSM303D_ADDRESS);
  Wire.write(RegAddress);
  Wire.write(Data);
  Wire.endTransmission();
}

//指定地址读（可改为连续读）
uint8_t LSM303D_ReadReg(uint8_t RegAddress)
{
  Wire.beginTransmission(LSM303D_ADDRESS);
  Wire.write(RegAddress);
  Wire.endTransmission(false); // 发送重新起始信号，不产生 STOP

  Wire.requestFrom(LSM303D_ADDRESS, (uint8_t)1); // 请求 1 字节数据
  if (Wire.available()) {
    return Wire.read();
  }
  return 0; // 读取失败返回 0
}

void LSM303D_Init(void)
{
  // 初始化I2C总线，可利用GPIO交换矩阵来任意指定引脚
  Wire.begin(1, 2); // (SDA, SCL)
  
  // 可选：设置 I2C 时钟频率为 400kHz (高速模式)
  Wire.setClock(400000);

  //验证通信
  uint8_t whoami = LSM303D_GetID();
  if (whoami != 0x49) {
    Serial.print("LSM303D 初始化失败! WHO_AM_I = 0x");
    Serial.println(whoami, HEX);
  } else {
    Serial.println("LSM303D 初始化成功！");
  }
	
	//配置寄存器，详见手册
	LSM303D_WriteReg(LSM303D_CTRL1, 0x6F);	//使能加速度计并设定数据速率（100Hz）
	LSM303D_WriteReg(LSM303D_CTRL2, 0x00);	//加速度计滤波，量程设定（±2g）
	LSM303D_WriteReg(LSM303D_CTRL5, 0x74);	//磁力计分辨率，数据速率设定（100Hz）
	LSM303D_WriteReg(LSM303D_CTRL6, 0x00);	//磁力计量程设定（±2gauss）
	LSM303D_WriteReg(LSM303D_CTRL7, 0x00);  //加速度数据选择，磁力计模式选择（连续转换）
}

//获取ID
uint8_t LSM303D_GetID(void)
{
	return LSM303D_ReadReg(LSM303D_WHO_AM_I);
}

//加工数据
void LSM303D_GetData(Axis_Data *AxisData)
{
	uint16_t DataH = 0;
	uint16_t DataL = 0;
	
	DataH = LSM303D_ReadReg(LSM303D_ACCEL_XOUT_H);
	DataL = LSM303D_ReadReg(LSM303D_ACCEL_XOUT_L);
	AxisData ->AccX = (DataH << 8) | (DataL);
		
	DataH = LSM303D_ReadReg(LSM303D_ACCEL_YOUT_H);
	DataL = LSM303D_ReadReg(LSM303D_ACCEL_YOUT_L);
	AxisData ->AccY = (DataH << 8) | (DataL);
		
	DataH = LSM303D_ReadReg(LSM303D_ACCEL_ZOUT_H);
	DataL = LSM303D_ReadReg(LSM303D_ACCEL_ZOUT_L);
	AxisData ->AccZ = (DataH << 8) | (DataL);
		
	DataH = LSM303D_ReadReg(LSM303D_MAGN_XOUT_H);
	DataL = LSM303D_ReadReg(LSM303D_MAGN_XOUT_L);
	AxisData ->MagnX = (DataH << 8) | (DataL);
		
	DataH = LSM303D_ReadReg(LSM303D_MAGN_YOUT_H);
	DataL = LSM303D_ReadReg(LSM303D_MAGN_YOUT_L);
	AxisData ->MagnY = (DataH << 8) | (DataL);
		
	DataH = LSM303D_ReadReg(LSM303D_MAGN_ZOUT_H);
	DataL = LSM303D_ReadReg(LSM303D_MAGN_ZOUT_L);
	AxisData ->MagnZ = (DataH << 8) | (DataL);
}

// 初始化姿态解算（可选，预留）
void LSM303D_AttitudeInit(void) {
    yaw_initialized = false;
}

// 计算横滚角（roll）和俯仰角（pitch）弧度值
static void computeRollPitch(int16_t acc_x, int16_t acc_y, int16_t acc_z,
                             float *roll, float *pitch) {
    // 归一化加速度向量
    float ax = acc_x / ACCEL_SCALE;
    float ay = acc_y / ACCEL_SCALE;
    float az = acc_z / ACCEL_SCALE;

    // 计算俯仰角 (pitch) 和横滚角 (roll)
    *pitch = atan2(-ax, sqrt(ay * ay + az * az));
    *roll  = atan2(ay, az);
}

// 对磁力计读数进行倾斜补偿，计算航向角（弧度）
static float computeYaw(int16_t mag_x, int16_t mag_y, int16_t mag_z,
                        float roll, float pitch) {
    // 去除硬铁偏移（可事先校准）
    float mx = mag_x - mag_offset_x;
    float my = mag_y - mag_offset_y;
    float mz = mag_z - mag_offset_z;

    // 倾斜补偿公式（将磁力计投影到水平面）
    float cos_roll  = cos(roll);
    float sin_roll  = sin(roll);
    float cos_pitch = cos(pitch);
    float sin_pitch = sin(pitch);

    float mag_x_hor = mx * cos_pitch + my * sin_roll * sin_pitch + mz * cos_roll * sin_pitch;
    float mag_y_hor = my * cos_roll - mz * sin_roll;

    // 计算航向角（从磁北顺时针旋转，弧度）
    float yaw = atan2(-mag_y_hor, mag_x_hor);  // 注意负号，根据坐标定义调整

    return yaw;
}

// 获取当前偏航角（单位：0.01 度）
int16_t LSM303D_GetYaw(void) {
    Axis_Data data;
    LSM303D_GetData(&data);

    float roll, pitch;
    computeRollPitch(data.AccX, data.AccY, data.AccZ, &roll, &pitch);
    float yaw = computeYaw(data.MagnX, data.MagnY, data.MagnZ, roll, pitch);

    // 转换为 0.01 度单位
    return (int16_t)(yaw * 1800.0f / M_PI);  // 180/π * 100
}

// 获取偏航角变化量（每次调用返回增量，并更新内部状态）
int16_t LSM303D_GetYawDelta(void) {
    Axis_Data data;
    LSM303D_GetData(&data);

    float roll, pitch;
    computeRollPitch(data.AccX, data.AccY, data.AccZ, &roll, &pitch);
    float yaw = computeYaw(data.MagnX, data.MagnY, data.MagnZ, roll, pitch);

    if (!yaw_initialized) {
        yaw_previous = yaw;
        yaw_initialized = true;
        return 0;
    }

    // 计算角度差，并处理环绕（-π 到 π）
    float delta = yaw - yaw_previous;
    if (delta > M_PI) delta -= 2.0f * M_PI;
    if (delta < -M_PI) delta += 2.0f * M_PI;

    yaw_previous = yaw;

    // 转换为 0.01 度单位
    return (int16_t)(delta * 1800.0f / M_PI);
}
