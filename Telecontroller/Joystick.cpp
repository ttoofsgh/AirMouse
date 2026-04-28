#include <Arduino.h>
#include "Joystick.h"
#include <esp_adc/adc_continuous.h>
#include <hal/adc_types.h>   // 提供 adc_digi_output_data_t 定义
#include <soc/soc_caps.h>    // 提供 SOC_ADC_DIGI_RESULT_BYTES 定义

#define JOY_X_CHANNEL   ADC_CHANNEL_5  // GPIO6
#define JOY_Y_CHANNEL   ADC_CHANNEL_6  // GPIO7
#define JOY_BTN_PIN     5

static adc_continuous_handle_t adc_handle = NULL;

static volatile uint16_t g_adc_x = 0;
static volatile uint16_t g_adc_y = 0;
// 回调函数：在 DMA 完成一批数据时被调用
static bool IRAM_ATTR adc_callback(adc_continuous_handle_t handle,
                                   const adc_continuous_evt_data_t *edata,
                                   void *user_data)
{
    // 完成转换的一帧数据的起始地址
    uint8_t *buf = edata->conv_frame_buffer;
    size_t len = edata->size;  // 这一帧数据的字节长度
    // 遍历整个数据块，每次移动一个采样点的大小
    for (size_t offset = 0; offset < len; offset += SOC_ADC_DIGI_RESULT_BYTES) {
        // 将原始字节转换为结构体指针
        adc_digi_output_data_t *sample = (adc_digi_output_data_t *)(buf + offset);
        // 获取原始 ADC 值
        uint16_t raw = sample->type2.data;
        // 根据 unit 和 channel 判断是 X 还是 Y
        if (sample->type2.unit == ADC_UNIT_1) {
            if (sample->type2.channel == JOY_X_CHANNEL) {   // X 轴
                g_adc_x = raw;
            } else if (sample->type2.channel == JOY_Y_CHANNEL) {    // Y 轴
                g_adc_y = raw;
            }
        }
    }
    
    return true;
}

static void adc_dma_init(void)
{
    // 每次 DMA 传输 20 个采样点（10 对 XY）
    const size_t frame_size_samples = 20;
    
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,   //缓冲区大小
        // 一帧数据的大小（SOC_ADC_DIGI_RESULT_BYTES：每个采样点占用的字节数）
        .conv_frame_size = frame_size_samples * SOC_ADC_DIGI_RESULT_BYTES,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    //连续转换配置
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,        // 20kHz 总采样率
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,  // ADC1 
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,   // 输出格式为 TYPE1（每个样本2字节，低12位有效）
    };

    //配置通道
    adc_digi_pattern_config_t adc_pattern[2] = {0};
    adc_pattern[0].atten = ADC_ATTEN_DB_11;    // 0~3.3V 量程
    adc_pattern[0].channel = JOY_X_CHANNEL;
    adc_pattern[0].unit = ADC_UNIT_1;
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH; // 12 位
    
    adc_pattern[1].atten = ADC_ATTEN_DB_11;
    adc_pattern[1].channel = JOY_Y_CHANNEL;
    adc_pattern[1].unit = ADC_UNIT_1;
    adc_pattern[1].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    dig_cfg.pattern_num = 2;
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    // 注册回调
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = adc_callback,  // DMA 传输完成时调用
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));

    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

void Joystick_Init(void)
{
    pinMode(JOY_BTN_PIN, INPUT_PULLUP);
    adc_dma_init();
}

uint8_t Joystick_Button_Read(void)
{
    bool JoyBtn = 0;
    if (digitalRead(JOY_BTN_PIN) == LOW) {
      delay(20);
      if (digitalRead(JOY_BTN_PIN) == LOW) {
        JoyBtn = 1;
      }
    }
    return JoyBtn;
}

uint16_t Joystick_GetX(void)
{
    return g_adc_x;
}

uint16_t Joystick_GetY(void)
{
    return g_adc_y;
}