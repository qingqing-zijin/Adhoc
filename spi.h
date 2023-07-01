/*
 * spi.h
 *
 *  Created on: Jul 12, 2018
 *      Author: root
 */

#ifndef SPI_H_
#define SPI_H_

#include "MyTypedef.h"


/**
 * 功 能：同步数据传输
 * 入口参数 ：
 * TxBuf -> 发送数据首地址
 * len -> 交换数据的长度
 * 出口参数：
 * RxBuf -> 接收数据缓冲区
 * 返回值：0 成功
 * 开发人员：Lzy 2013－5－22
 */
int SPI_Transfer(const uint8_t *TxBuf, uint8_t *RxBuf, int len);



/**
 * 功 能：发送数据
 * 入口参数 ：
 * TxBuf -> 发送数据首地址
 ＊len -> 发送与长度
 ＊返回值：0 成功
 * 开发人员：Lzy 2013－5－22
 */
int SPI_Write(uint8_t *TxBuf, int len);

/**
 * 功 能：接收数据
 * 出口参数：
 * RxBuf -> 接收数据缓冲区
 * rtn -> 接收到的长度
 * 返回值：>=0 成功
 * 开发人员：Lzy 2013－5－22
 */
int SPI_Read(uint8_t *RxBuf, int len) ;


/**
 * 功 能：打开设备 并初始化设备
 * 入口参数 ：
 * 出口参数：
 * 返回值：0 表示已打开 0XF1 表示SPI已打开 其它出错
 * 开发人员：Lzy 2013－5－22
 */
int SPI_Open(void);


/**
 * 功 能：关闭SPI模块
 */
int SPI_Close(void) ;



/**
 * 功 能：自发自收测试程序
 * 接收到的数据与发送的数据如果不一样 ，则失败
 * 说明：
 * 在硬件上需要把输入与输出引脚短跑
 * 开发人员：Lzy 2013－5－22
 */
int SPI_LookBackTest(void) ;


//void LOG_MSG(int level, char *format, ...);


uint8_t SPI_ExchangeByte(uint8_t TxByte);

//
uint8_t SPI_WriteTxFIFO(uint8_t w_tx_payload_cmd,uint8_t *pBuff,uint8_t len);
//
uint8_t SPI_ReadRxFIFO(uint8_t r_rx_payload_cmd,uint8_t *pBuff,uint8_t len);
//
uint8_t SPI_ReadSingleByte(uint8_t Addr);
//
uint8_t SPI_WriteMultiBytes(uint8_t StartAddr, uint8_t *pBuff, uint8_t len);
//
uint8_t SPI_WriteSingleByte(uint8_t Addr, uint8_t val);

//
uint8_t SPI_WriteCommand(uint8_t cmd);

//
uint8_t SPI_WriteCommandWithAck(uint8_t cmd, uint8_t tx_dump);

#endif /* SPI_H_ */
