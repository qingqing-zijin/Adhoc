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
 * �� �ܣ�ͬ�����ݴ���
 * ��ڲ��� ��
 * TxBuf -> ���������׵�ַ
 * len -> �������ݵĳ���
 * ���ڲ�����
 * RxBuf -> �������ݻ�����
 * ����ֵ��0 �ɹ�
 * ������Ա��Lzy 2013��5��22
 */
int SPI_Transfer(const uint8_t *TxBuf, uint8_t *RxBuf, int len);



/**
 * �� �ܣ���������
 * ��ڲ��� ��
 * TxBuf -> ���������׵�ַ
 ��len -> �����볤��
 ������ֵ��0 �ɹ�
 * ������Ա��Lzy 2013��5��22
 */
int SPI_Write(uint8_t *TxBuf, int len);

/**
 * �� �ܣ���������
 * ���ڲ�����
 * RxBuf -> �������ݻ�����
 * rtn -> ���յ��ĳ���
 * ����ֵ��>=0 �ɹ�
 * ������Ա��Lzy 2013��5��22
 */
int SPI_Read(uint8_t *RxBuf, int len) ;


/**
 * �� �ܣ����豸 ����ʼ���豸
 * ��ڲ��� ��
 * ���ڲ�����
 * ����ֵ��0 ��ʾ�Ѵ� 0XF1 ��ʾSPI�Ѵ� ��������
 * ������Ա��Lzy 2013��5��22
 */
int SPI_Open(void);


/**
 * �� �ܣ��ر�SPIģ��
 */
int SPI_Close(void) ;



/**
 * �� �ܣ��Է����ղ��Գ���
 * ���յ��������뷢�͵����������һ�� ����ʧ��
 * ˵����
 * ��Ӳ������Ҫ��������������Ŷ���
 * ������Ա��Lzy 2013��5��22
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
