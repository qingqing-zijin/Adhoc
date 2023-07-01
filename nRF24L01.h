/*
 * nRF24L01.h
 *
 *  Created on: Jul 12, 2018
 *      Author: root
 */

#ifndef NRF24L01_H_
#define NRF24L01_H_
#include "MyTypedef.h"
#include "nRF24L01_Reg.h"
#include "gpio.h"

#define E01_MAX_PKT_BYTES	(32)

#define REPEAT_CNT      0  // 0-15, repeat transmit count
#define INIT_ADDR       1, 2, 3, 4, 5

// nRF24L01P相关控制引脚定义
//#define PORT_L01_CSN    GPIOB
//#define PIN_L01_CSN     GPIO_Pin_4
//
//#define PORT_L01_IRQ    GPIOA
//#define PIN_L01_IRQ     GPIO_Pin_3
//
//#define PORT_L01_CE     GPIOA
//#define PIN_L01_CE      GPIO_Pin_2

//#define L01_CSN_LOW()   GPIO_ResetBits(PORT_L01_CSN, PIN_L01_CSN)
//#define L01_CSN_HIGH()  GPIO_SetBits(PORT_L01_CSN, PIN_L01_CSN)
//
//#define L01_CE_LOW()    GPIO_ResetBits(PORT_L01_CE, PIN_L01_CE)
//#define L01_CE_HIGH()   GPIO_SetBits(PORT_L01_CE, PIN_L01_CE)
//

#ifdef L01_CE_USED

#define L01_CSN_LOW()   gpio_write(L01_CSN_PIN_NUM, 0)
#define L01_CSN_HIGH()  gpio_write(L01_CSN_PIN_NUM, 1)
//
#else
#define L01_CSN_LOW()   {}
#define L01_CSN_HIGH()  {}
#endif

#define L01_CE_LOW()    gpio_write(L01_CE_PIN_NUM, 0)
#define L01_CE_HIGH()   gpio_write(L01_CE_PIN_NUM, 1)


/*
 * 	读取中断引脚的电平
 * */
#define L01_IRQ_READ()  gpio_read(L01_IRQ_PIN_NUM)

// nRF24L01P相关函数接口
// 初始化L01
void L01_Init(void);

// 复位TX FIFO指针
void L01_FlushTX(void);

// 复位RX FIFO指针
void L01_FlushRX(void);

// 读取中断
uint8_t L01_ReadIRQSource(void);

// 清除中断
#define IRQ_ALL  ((1<<RX_DR) | (1<<TX_DS) | (1<<MAX_RT))
void L01_ClearIRQ(uint8_t IRQ_Source);

// 读取FIFO数据宽度
uint8_t L01_ReadTopFIFOWidth(void);

// 读取接收到的数据
uint8_t L01_ReadRXPayload(uint8_t *pBuff);

// 设置L01模式
typedef enum{ TX_MODE, RX_MODE } L01MD;
void L01_SetTRMode(L01MD mode);

// 设置L01空速
typedef enum{ SPD_250K, SPD_1M, SPD_2M ,SPD_ERR} L01SPD;
void L01_SetSpeed(L01SPD speed);

// 设置L01功率
typedef enum{ P_F18DBM, P_F12DBM, P_F6DBM, P_0DBM } L01PWR;
void L01_SetPower(L01PWR power);

// 设置L01频率
void L01_WriteFreqPoint(uint8_t FreqPoint);

uint8_t L01_ReadStatusReg(void);

// 写数据到一个寄存器
void L01_WriteSingleReg(uint8_t Addr, uint8_t Value);

// 读取一个寄存器的值
uint8_t L01_ReadSingleReg(uint8_t Addr);

// 读取多个寄存器的值
void L01_ReadMultiReg(uint8_t StartAddr, uint8_t nBytes, uint8_t *pBuff);

// 写数据到多个寄存器
void L01_WriteMultiReg(uint8_t StartAddr, uint8_t *pBuff, uint8_t Length);

// 写数据到TXFIFO(带ACK返回)
void L01_WriteTXPayload_Ack(uint8_t *pBuff, uint8_t nBytes);

// 写数据到TXFIFO(不带ACK返回)
void L01_WriteTXPayload_NoAck(uint8_t *Data, uint8_t Data_Length);

// 设置发送物理地址
void L01_SetTXAddr(uint8_t *pAddr, uint8_t Addr_Length);

// 设置接收物理地址
void L01_SetRXAddr(uint8_t PipeNum, uint8_t *pAddr, uint8_t Addr_Length);

//初始化芯片管教
void L01_Chip_Init(void);

//
int L01_SendPacket(uint8_t* buffer, uint32_t length);

//
int L01_RecvPacket(uint8_t* rx_buffer, uint32_t length);

//
int L01_GPIOs_Init();
#endif /* NRF24L01_H_ */
