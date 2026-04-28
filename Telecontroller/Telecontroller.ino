#include "OLED.h"
#include "LSM303D.h"
#include "Joystick.h"
#include "BLE_Mouse.h"
#include <HijelHID_BLEMouse.h>
#include "NRF24L01.h"

#define AIRMOUSE_MODE     0
#define TELECONTROL_MODE  1

//Air Mouse Mode
bool Mode = 0;
//Telecontrol Mode
uint8_t SendFlag, ReceiveFlag;	//发送接收标志位

void setup() {
  //切换开关引脚
  pinMode(17, INPUT_PULLDOWN);

  delay(10);

  if (digitalRead(17) == AIRMOUSE_MODE) {
    Mode = AIRMOUSE_MODE;
  } else {
    Mode = TELECONTROL_MODE;
  }

  // Serial.begin(115200);
  OLED_Init();
  Joystick_Init();

  OLED_Clear();

  if (Mode == AIRMOUSE_MODE) {
    LSM303D_Init();
    BLEMouse_Init();

    OLED_ShowString(1, 1, "ESP32 AirMouse");
    OLED_ShowString(2, 12, "--BLE");
  } else {
    NRF24L01_Init();
    
    OLED_ShowString(1, 1, "Telecontroller");
    OLED_ShowString(2, 11, "--2.4G");
    OLED_ShowString(3, 1, "SendF:  ReceF:  ");
    OLED_ShowString(4, 1, "M1:      M2:    ");

    // 读取 CONFIG 寄存器验证通信
    uint8_t config = NRF24L01_ReadReg(NRF24L01_CONFIG);
    
    // Serial.print("NRF24L01 CONFIG = 0x");
    // Serial.println(config, HEX);
    
    if (config != 0x08 && config != 0x0B) {
        // Serial.println("错误：SPI通信失败或模块未响应！请检查接线和电源。");
        OLED_ShowString(2, 1, "Error");
        while (1);
    } else {
        // Serial.println("SPI通信正常，模块响应正确。");
        OLED_ShowString(2, 1, "Ready");
    }
    
    /*初始化发送数据*/
    NRF24L01_TxPacket[0] = 0x00;
    NRF24L01_TxPacket[1] = 0x00;
    NRF24L01_TxPacket[2] = 0x00;
    NRF24L01_TxPacket[3] = 0x00;
  }
}

void loop() {
  if (Mode == AIRMOUSE_MODE) {
    static unsigned long lastUpdate, lastDisplay = 0;
    
    if (millis() - lastUpdate > 40) {
      BLEMouse_Update();
      lastUpdate = millis();
    }
    
    if (millis() - lastDisplay > 200) {
      if (BLEMouse_IsConnected()) {
          OLED_ShowString(2, 1, "Connected  ");
      } else {
          OLED_ShowString(2, 1, "Waiting... ");
      }
      if (BLEMouse_IsSensitivityMode()) {
          OLED_ShowString(3, 1, "Sens Mode: ON ");
      } else {
          OLED_ShowString(3, 1, "Normal Mode   ");
      }
      OLED_ShowString(4, 1, "Sens Level:   /5");
      OLED_ShowNum(4, 13, BLEMouse_GetSensitivityLevel(), 1);

      lastDisplay = millis();
    }
  } else {
    
    static unsigned long lastSend = 0;

    // 每隔20ms发送一次
    if (millis() - lastSend > 20) {
      int16_t JoyX = Joystick_GetX();
      int16_t JoyY = Joystick_GetY();

      // Serial.print(JoyX);
      // Serial.print(",");
      // Serial.println(JoyY);

      if (JoyX < 1910) {
        JoyX = map(JoyX, 0, 1910, -100, 0);
      } else if (JoyX > 1925) {
        JoyX = map(JoyX, 1925, 4095, 0, 100);
      } else {
        JoyX = 0;
      }
      if (JoyY < 1835) {
        JoyY = - map(JoyY, 0, 1835, -100, 0);
      } else if (JoyY > 1855) {
        JoyY = - map(JoyY, 1855, 4095, 0, 100);
      } else {
        JoyY = 0;
      }

      int16_t LPWM_raw = JoyY + JoyX;
      int16_t RPWM_raw = JoyY - JoyX;

      //等比例限幅
      int16_t LPWM, RPWM;
      int16_t max_abs = max(abs(LPWM_raw), abs(RPWM_raw));

      if (max_abs > 100) {
        float scale = 100.0f / max_abs;
        LPWM = (int16_t)(LPWM_raw * scale);
        RPWM = (int16_t)(RPWM_raw * scale);
      } else {
        LPWM = LPWM_raw;
        RPWM = RPWM_raw;
      }

      // NRF24L01_TxPacket[0] ++;
      // NRF24L01_TxPacket[1] ++;
      NRF24L01_TxPacket[2] = LPWM;
      NRF24L01_TxPacket[3] = RPWM;
      
        
      /*调用NRF24L01_Send函数，发送数据，同时返回发送标志位，方便用户了解发送状态*/
      /*发送标志位与发送状态的对应关系，可以转到此函数定义上方查看*/
      SendFlag = NRF24L01_Send();
      OLED_ShowNum(3, 7, SendFlag, 1);
      OLED_ShowSignedNum(4, 4, LPWM, 3);
      OLED_ShowSignedNum(4, 13, RPWM, 3);

      lastSend = millis();
    }
    
    /*主循环内循环执行NRF24L01_Receive函数，接收数据，同时返回接收标志位，方便用户了解接收状态*/
    /*接收标志位与接收状态的对应关系，可以转到此函数定义上方查看*/
    
    ReceiveFlag = NRF24L01_Receive();

    // 坦克模式判断
    if ((NRF24L01_RxPacket[0] >> 6) == 0x01) {   //
      OLED_Clear();
      OLED_ShowString(1, 1, "Tracking...");
      OLED_ShowString(2, 1, "Trac_sign:");
      OLED_ShowString(3, 1, "Obst_dist:");
      OLED_ShowString(4, 1, "M1:      M2:    ");

      uint16_t Num = 0;
      switch(NRF24L01_RxPacket[0] & 0x0F)
      {
        case 0x00: Num = 0;    break; // 0000
        case 0x0F: Num = 1111; break; // 1111
        case 0x02: Num = 10;   break; // 0010
        case 0x03: Num = 11;   break; // 0011
        case 0x01: Num = 1;    break; // 0001
        case 0x04: Num = 100 ; break; // 0100
        case 0x0C: Num = 1100; break; // 1100
        case 0x08: Num = 1000; break; // 1000
        default: break;  // 可选：未定义状态的处理
      }
      
      OLED_ShowNum(2, 12, Num, 4);
      OLED_ShowNum(3, 12, NRF24L01_RxPacket[1], 3);
      OLED_ShowSignedNum(4, 4, (int8_t)NRF24L01_RxPacket[2], 3);
      OLED_ShowSignedNum(4, 13, (int8_t)NRF24L01_RxPacket[3], 3);
    }
    
    /*显示接收数据*/
    // Serial.println("接收的数据：");
    // Serial.print(NRF24L01_RxPacket[0]);
    // Serial.print(NRF24L01_RxPacket[1]);
    // Serial.print(NRF24L01_RxPacket[2]);
    // Serial.println(NRF24L01_RxPacket[3]);
  }

  if (digitalRead(17) != Mode) {
    delay(20);
    if (digitalRead(17) != Mode) {
      OLED_Clear();
      OLED_ShowString(2, 1, "Switching...");
      if (Mode == TELECONTROL_MODE) {
        //关停电机
        NRF24L01_TxPacket[0] = 0x00;
        NRF24L01_TxPacket[1] = 0x00;
        NRF24L01_Send(); 
      }
      ESP.restart();
    }
  }
}
