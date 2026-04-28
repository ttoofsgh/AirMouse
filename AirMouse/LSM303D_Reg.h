#ifndef __LSM303D_REG_H
#define __LSM303D_REG_H

//LSM303D寄存器及其地址，详见手册
//控制寄存器：
#define	LSM303D_CTRL1		  0x20
#define	LSM303D_CTRL2			0x21
#define	LSM303D_CTRL5			0x24
#define	LSM303D_CTRL6			0x25
#define	LSM303D_CTRL7			0x26
//数据寄存器：
#define	LSM303D_ACCEL_XOUT_H	0x29
#define	LSM303D_ACCEL_XOUT_L	0x28
#define	LSM303D_ACCEL_YOUT_H	0x2B
#define	LSM303D_ACCEL_YOUT_L	0x2A
#define	LSM303D_ACCEL_ZOUT_H	0x2D
#define	LSM303D_ACCEL_ZOUT_L	0x2C
#define	LSM303D_MAGN_XOUT_H		0x09
#define	LSM303D_MAGN_XOUT_L		0x08
#define	LSM303D_MAGN_YOUT_H		0x0B
#define	LSM303D_MAGN_YOUT_L		0x0A
#define	LSM303D_MAGN_ZOUT_H		0x0D
#define	LSM303D_MAGN_ZOUT_L		0x0C

#define	LSM303D_WHO_AM_I		0x0F

#endif
