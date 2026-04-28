#include <Arduino.h>
#include <HijelHID_BLEMouse.h>
#include "BLE_Mouse.h"
#include "Joystick.h"
#include "LSM303D.h"
#include "OLED.h"

#ifdef __cplusplus
extern "C" {
#endif

//  引脚配置
#define BTN_LEFT_PIN        35  // 鼠标左键（灵敏度减）
#define BTN_RIGHT_PIN       36  // 鼠标右键（灵敏度加）
#define BTN_ENABLE_PIN      37  // 发送使能按键（按下时才发送光标移动）
#define BTN_JOY_PIN         5   // 摇杆按键（灵敏度调节模式）

//  灵敏度配置 
#define SENSITIVITY_LEVELS  5     // 灵敏度挡位数量
#define SENSITIVITY_BASE    1     // 基础灵敏度系数（挡位1的系数）
#define SENSITIVITY_STEP    2     // 每挡增加的系数
#define ANGLE_DEADZONE      30    // X轴死区（偏航角）
#define Y_DEADZONE          600   // 死区阈值，小于此值的增量忽略

//  低通滤波变量
static float filtX = 0, filtY = 0;

//  状态变量 
static bool isConnected = false;
static uint8_t sensitivityLevel = 3;    // 当前灵敏度挡位（1~5）
static bool isSensitivityMode = false;  // 是否处于灵敏度调节模式

//  传感器数据缓存 
static Axis_Data lastSensorData;
static bool firstRead = true;

//  创建BLE鼠标对象 
static HijelBLEMouse* bleMouse = NULL;

//  按钮消抖
typedef struct {
    uint32_t lastDebounceTime;
    bool lastReading;
    bool currentState;
    uint8_t pin;
} ButtonDebouncer;

static ButtonDebouncer btnLeft;
static ButtonDebouncer btnRight;
static ButtonDebouncer btnEnable;
static ButtonDebouncer btnJoy;

// 初始化消抖器
static void button_init(ButtonDebouncer* btn, uint8_t pin) {
    btn->pin = pin;
    btn->lastDebounceTime = 0;
    btn->lastReading = HIGH;
    btn->currentState = HIGH;
}

// 更新消抖器，返回 true 表示状态发生变化
static bool button_update(ButtonDebouncer* btn) {
    bool reading = digitalRead(btn->pin);
    if (reading != btn->lastReading) {
        btn->lastDebounceTime = millis();
        btn->lastReading = reading;
    }
    if ((millis() - btn->lastDebounceTime) > 10) {
        if (reading != btn->currentState) {
            btn->currentState = reading;
            return true;
        }
    }
    return false;
}

// 获取按钮是否按下
static bool button_isPressed(ButtonDebouncer* btn) {
    return btn->currentState == LOW;
}

//  应用灵敏度
static uint8_t getSensitivityFactor(void) {
    return SENSITIVITY_BASE + (sensitivityLevel - 1) * SENSITIVITY_STEP;
}

//  初始化 
void BLEMouse_Init(void) {
    // 初始化 GPIO
    pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_ENABLE_PIN, INPUT_PULLUP);
    pinMode(BTN_JOY_PIN, INPUT_PULLUP);
    
    // 初始化消抖器
    button_init(&btnLeft, BTN_LEFT_PIN);
    button_init(&btnRight, BTN_RIGHT_PIN);
    button_init(&btnEnable, BTN_ENABLE_PIN);
    button_init(&btnJoy, BTN_JOY_PIN);
    
    // 初始化 BLE 鼠标
    bleMouse = new HijelBLEMouse("ESP32 AirMouse");
    bleMouse->begin();
    bleMouse->setLogLevel(HIDLogLevel::Normal);
    
    // 首次读取传感器数据
    LSM303D_GetData(&lastSensorData);
    firstRead = true;
    
    // Serial.println("BLE鼠标已启动，等待连接...");
    // Serial.println("按键功能：");
    // Serial.println("  GPIO37(按下): 允许发送光标移动");
    // Serial.println("  GPIO5: 进入灵敏度调节模式");
    // Serial.println("  灵敏度调节模式下: GPIO35减挡位, GPIO36加挡位");
    // Serial.println("  灵敏度调节模式下: 再次按GPIO5退出");
}

//  连接状态 
bool BLEMouse_IsConnected(void) {
    if (bleMouse == NULL) return false;
    return bleMouse->isConnected();
}

// 当前是否处于灵敏度调节模式
bool BLEMouse_IsSensitivityMode(void) {
    return isSensitivityMode;
}

//  获取当前灵敏度
uint8_t BLEMouse_GetSensitivityLevel(void) {
    return sensitivityLevel;
}

//  发送鼠标移动 
void BLEMouse_Move(int8_t x, int8_t y) {
    if (bleMouse != NULL && bleMouse->isConnected()) {
        bleMouse->move(x, y);
    }
}

//  发送鼠标按键 
void BLEMouse_Click(uint8_t button) {
    if (bleMouse != NULL && bleMouse->isConnected()) {
        bleMouse->click((MouseButton)button);
    }
}

//  发送滚轮滚动 
void BLEMouse_Scroll(int8_t scroll) {
    if (bleMouse != NULL && bleMouse->isConnected()) {
        bleMouse->addScroll(scroll);
    }
}

//  更新 
void BLEMouse_Update(void) {
    if (bleMouse == NULL) return;
    
    bool currentConnected = bleMouse->isConnected();
    if (currentConnected != isConnected) {
        isConnected = currentConnected;
        if (isConnected) {
            // Serial.println("BLE鼠标已连接！");
        } else {
            // Serial.println("BLE鼠标已断开！");
        }
    }
    
    if (!isConnected) return;
    
    // 更新按钮状态
    bool leftChanged = button_update(&btnLeft);
    bool rightChanged = button_update(&btnRight);
    bool enableChanged = button_update(&btnEnable);
    bool JoyChanged = button_update(&btnJoy);
    bool JoyPressed = button_isPressed(&btnJoy);
    bool enablePressed = button_isPressed(&btnEnable);
    bool leftPressed = button_isPressed(&btnLeft);
    bool rightPressed = button_isPressed(&btnRight);
    
    //  灵敏度调节模式切换 
    // 按摇杆键进入灵敏度调节模式
    if (JoyChanged && JoyPressed) {
      if (isSensitivityMode) {
          isSensitivityMode = false;
          Serial.print("退出灵敏度调节模式，当前挡位: ");
          Serial.println(sensitivityLevel);
      } else {
          isSensitivityMode = true;
          Serial.print("进入灵敏度调节模式，当前挡位: ");
          Serial.println(sensitivityLevel);
      }
    }
    
    //  灵敏度调节模式下的按键处理 
    if (isSensitivityMode) {
        // 左键减挡位
        if (leftChanged && leftPressed) {
            if (sensitivityLevel > 1) {
                sensitivityLevel--;
                Serial.print("灵敏度挡位: ");
                Serial.println(sensitivityLevel);
            }
        }
        // 右键加挡位
        if (rightChanged && rightPressed) {
            if (sensitivityLevel < SENSITIVITY_LEVELS) {
                sensitivityLevel++;
                Serial.print("灵敏度挡位: ");
                Serial.println(sensitivityLevel);
            }
        }
        return; // 灵敏度调节模式下不发送任何鼠标数据
    }
    
    //  正常模式：发送鼠标数据 
    // 读取当前传感器数据
    Axis_Data currentData;
    LSM303D_GetData(&currentData);
    
    if (firstRead) {
        lastSensorData = currentData;
        firstRead = false;
        return;
    }
    
    // 计算增量
    int16_t deltaAccX = currentData.AccX - lastSensorData.AccX;
    int16_t yawDelta = LSM303D_GetYawDelta();  // 单位：0.01 度

    // 应用死区
    if (abs(yawDelta) < ANGLE_DEADZONE) yawDelta = 0;
    if (abs(deltaAccX) < Y_DEADZONE) deltaAccX = 0;
    
    // Serial.print(yawDelta);
    // Serial.print(",");
    // Serial.println(deltaAccX);

    /* 调试 */
    // static uint8_t i = 5;
    // if (leftChanged && leftPressed) {
    //   i -= 1;
    // }
    // if (rightChanged && rightPressed) {
    //   i += 1;
    // }
    // OLED_ShowNum(4, 1, i, 4);

    // 应用灵敏度系数
    int16_t mouseX = -(yawDelta * getSensitivityFactor()) / 5;
    int16_t mouseY = -(deltaAccX * getSensitivityFactor()) / 175;

    // 低通滤波
    filtX = 0.5 * mouseX + (1.0f - 0.5) * filtX;
    filtY = 0.6 * mouseY + (1.0f - 0.6) * filtY;

    // 限幅到[-127, 127]
    int8_t moveX = constrain((int16_t)filtX, -127, 127);
    int8_t moveY = constrain((int16_t)filtY, -127, 127);
    
    // 只有发送使能键按下时才发送光标移动数据
    if (enablePressed) {
        if (moveX != 0 || moveY != 0) {
            bleMouse->move(moveX, moveY);
            // Serial.print(moveX);
            // Serial.print(",");
            // Serial.println(moveY);
        }
        // Serial.println("发送使能键按下");
    }

    //鼠标左右按键
    if (leftChanged && leftPressed) {
      bleMouse->click((MouseButton)0x01);
      // Serial.println("左键按下");
    }

    if (rightChanged && rightPressed) {
      bleMouse->click((MouseButton)0x02);
      // Serial.println("右键按下");
    }

    // 摇杆Y轴作为滚轮
    uint16_t joyY = Joystick_GetY();
    int8_t scroll = 0;
    if (joyY < 1830) {
      scroll = -map(joyY, 0, 1830, -5, 0);
    } else if (joyY > 1850) {
      scroll = -map(joyY, 1850, 4095, 0, 5);
    }
    
    if (abs(scroll) > 1) {
        bleMouse->addScroll(scroll);
    }
    
    // 保存本次数据供下次计算增量
    lastSensorData = currentData;
}

#ifdef __cplusplus
}
#endif