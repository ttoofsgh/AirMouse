#include "NRF24L01.h"
#include <SPI.h>

// ========== 引脚定义 ==========
#define PIN_CE   21
#define PIN_CSN  14
#define PIN_SCK  13
#define PIN_MOSI 11
#define PIN_MISO 12

// SPI 实例指针（使用 ESP32 的 HSPI）
SPIClass * spi = nullptr;

// ========== 底层 GPIO 操作 ==========
static void NRF24L01_W_CE(uint8_t level) {
    digitalWrite(PIN_CE, level);
}

static void NRF24L01_W_CSN(uint8_t level) {
    digitalWrite(PIN_CSN, level);
}

// SPI 交换一个字节（硬件 SPI）
static uint8_t NRF24L01_SPI_SwapByte(uint8_t data) {
    return spi->transfer(data);
}

// ========== 全局变量（与 STM32 版本一致） ==========
uint8_t NRF24L01_TxAddress[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
#define NRF24L01_TX_PACKET_WIDTH  4
uint8_t NRF24L01_TxPacket[NRF24L01_TX_PACKET_WIDTH];

uint8_t NRF24L01_RxAddress[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
#define NRF24L01_RX_PACKET_WIDTH  4
uint8_t NRF24L01_RxPacket[NRF24L01_RX_PACKET_WIDTH];

// ========== 初始化 ==========
void NRF24L01_GPIO_Init(void) {
    // 配置引脚模式
    pinMode(PIN_CE, OUTPUT);
    pinMode(PIN_CSN, OUTPUT);
    
    // 初始化硬件 SPI
    spi = new SPIClass(HSPI);
    spi->begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN);
    spi->setFrequency(10000000);      // 10 MHz (NRF24L01 最大支持 10 MHz)
    spi->setDataMode(SPI_MODE0);      // SPI 模式 0 (CPOL=0, CPHA=0)
    
    // 默认电平
    NRF24L01_W_CE(0);
    NRF24L01_W_CSN(1);
}

// ========== 指令实现（几乎不用改，只调整底层 SPI 调用） ==========
uint8_t NRF24L01_ReadReg(uint8_t RegAddress) {
    uint8_t Data;
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER | RegAddress);
    Data = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    NRF24L01_W_CSN(1);
    return Data;
}

void NRF24L01_ReadRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count) {
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER | RegAddress);
    for (uint8_t i = 0; i < Count; i++) {
        DataArray[i] = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    }
    NRF24L01_W_CSN(1);
}

void NRF24L01_WriteReg(uint8_t RegAddress, uint8_t Data) {
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER | RegAddress);
    NRF24L01_SPI_SwapByte(Data);
    NRF24L01_W_CSN(1);
}

void NRF24L01_WriteRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count) {
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER | RegAddress);
    for (uint8_t i = 0; i < Count; i++) {
        NRF24L01_SPI_SwapByte(DataArray[i]);
    }
    NRF24L01_W_CSN(1);
}

void NRF24L01_ReadRxPayload(uint8_t *DataArray, uint8_t Count) {
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < Count; i++) {
        DataArray[i] = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    }
    NRF24L01_W_CSN(1);
}

void NRF24L01_WriteTxPayload(uint8_t *DataArray, uint8_t Count) {
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_W_TX_PAYLOAD);
    for (uint8_t i = 0; i < Count; i++) {
        NRF24L01_SPI_SwapByte(DataArray[i]);
    }
    NRF24L01_W_CSN(1);
}

void NRF24L01_FlushTx(void) {
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_TX);
    NRF24L01_W_CSN(1);
}

void NRF24L01_FlushRx(void) {
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_RX);
    NRF24L01_W_CSN(1);
}

uint8_t NRF24L01_ReadStatus(void) {
    uint8_t Status;
    NRF24L01_W_CSN(0);
    Status = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    NRF24L01_W_CSN(1);
    return Status;
}

// ========== 功能函数 ==========
void NRF24L01_PowerDown(void) {
    uint8_t Config;
    NRF24L01_W_CE(0);
    Config = NRF24L01_ReadReg(NRF24L01_CONFIG);
    if (Config == 0xFF) return;
    Config &= ~0x02;
    NRF24L01_WriteReg(NRF24L01_CONFIG, Config);
}

void NRF24L01_StandbyI(void) {
    uint8_t Config;
    NRF24L01_W_CE(0);
    Config = NRF24L01_ReadReg(NRF24L01_CONFIG);
    if (Config == 0xFF) return;
    Config |= 0x02;
    NRF24L01_WriteReg(NRF24L01_CONFIG, Config);
}

void NRF24L01_Rx(void) {
    uint8_t Config;
    NRF24L01_W_CE(0);
    Config = NRF24L01_ReadReg(NRF24L01_CONFIG);
    if (Config == 0xFF) return;
    Config |= 0x03;
    NRF24L01_WriteReg(NRF24L01_CONFIG, Config);
    NRF24L01_W_CE(1);
}

void NRF24L01_Tx(void) {
    uint8_t Config;
    NRF24L01_W_CE(0);
    Config = NRF24L01_ReadReg(NRF24L01_CONFIG);
    if (Config == 0xFF) return;
    Config |= 0x02;
    Config &= ~0x01;
    NRF24L01_WriteReg(NRF24L01_CONFIG, Config);
    NRF24L01_W_CE(1);
}

void NRF24L01_Init(void) {
    NRF24L01_GPIO_Init();
    
    NRF24L01_WriteReg(NRF24L01_CONFIG, 0x08);
    NRF24L01_WriteReg(NRF24L01_EN_AA, 0x3F);
    NRF24L01_WriteReg(NRF24L01_EN_RXADDR, 0x01);
    NRF24L01_WriteReg(NRF24L01_SETUP_AW, 0x03);
    NRF24L01_WriteReg(NRF24L01_SETUP_RETR, 0x03);
    NRF24L01_WriteReg(NRF24L01_RF_CH, 0x02);
    NRF24L01_WriteReg(NRF24L01_RF_SETUP, 0x0E);
    NRF24L01_WriteReg(NRF24L01_RX_PW_P0, NRF24L01_RX_PACKET_WIDTH);
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
    
    NRF24L01_FlushTx();
    NRF24L01_FlushRx();
    NRF24L01_WriteReg(NRF24L01_STATUS, 0x70);
    NRF24L01_Rx();
}

uint8_t NRF24L01_Send(void) {
    uint8_t Status, SendFlag;
    uint32_t Timeout;
    
    NRF24L01_WriteRegs(NRF24L01_TX_ADDR, NRF24L01_TxAddress, 5);
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_TxAddress, 5);
    NRF24L01_WriteTxPayload(NRF24L01_TxPacket, NRF24L01_TX_PACKET_WIDTH);
    NRF24L01_Tx();
    
    Timeout = 100000;
    while (1) {
        Status = NRF24L01_ReadStatus();
        if (--Timeout == 0) {
            SendFlag = 4;
            // NRF24L01_Init(); //ESP32主频较高加上调度的原因，完全初始化容易导致冲突和总线锁死，故去掉以下所有初始化
            break;
        }
        if ((Status & 0x30) == 0x30) {
            SendFlag = 3;
            // NRF24L01_Init();
            break;
        } else if ((Status & 0x10) == 0x10) {
            SendFlag = 2;
            // NRF24L01_Init();
            break;
        } else if ((Status & 0x20) == 0x20) {
            SendFlag = 1;
            break;
        }
    }
    
    NRF24L01_WriteReg(NRF24L01_STATUS, 0x30);
    NRF24L01_FlushTx();
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
    NRF24L01_Rx();
    return SendFlag;
}

uint8_t NRF24L01_Receive(void) {
    uint8_t Status, Config, ReceiveFlag;
    
    Status = NRF24L01_ReadStatus();
    Config = NRF24L01_ReadReg(NRF24L01_CONFIG);
    
    if ((Config & 0x02) == 0x00) {
        ReceiveFlag = 3;
        // NRF24L01_Init();
    } else if ((Status & 0x30) == 0x30) {
        ReceiveFlag = 2;
        // NRF24L01_Init();
    } else if ((Status & 0x40) == 0x40) {
        ReceiveFlag = 1;
        NRF24L01_ReadRxPayload(NRF24L01_RxPacket, NRF24L01_RX_PACKET_WIDTH);
        NRF24L01_WriteReg(NRF24L01_STATUS, 0x40);
        NRF24L01_FlushRx();
    } else {
        ReceiveFlag = 0;
    }
    return ReceiveFlag;
}

void NRF24L01_UpdateRxAddress(void) {
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
}
