/*
 * serial.h
 *
 *  Created on: Mar 9, 2018
 *      Author: root
 */

#ifndef SERIAL_H_
#define SERIAL_H_

/**
 *�꿪�� ������UARTת����͸������ģ��
 */



typedef enum
{
        COM0,
        COM1, //E32,ʵ��linux��/dev/ttymxc1����ӦUART2����COM1��ʾ
        COM2,	//E43
        COM3,
        COM4,
        COM5,
        COM6,
        COM7,
        COM_NULL,
}RADIO_COM_t;

typedef unsigned char u8;

int uartsendto(int fd, u8* sendBytes, int len);

int uartrecvfrom(int fd, u8* buffer, int len);
/*
 * @function Uart_Open
 * @Desc 	 ��ָ��comNum ���ڣ���ָ�����ڲ����ʡ�����λ���ȡ�У��λ��ֹͣλ�ͽ���ʱ��С�ַ�������
 * @param1 comNum 	����id
 * @param2 nSpeed 	������
 * @param3 nBits	����λ����
 * @param4 nEvents 	��żУ��λ
 * @param5 nStop	ֹͣλ
 * @param6 c_min	��С�����ַ�����
 * @Return ���ش򿪵Ĵ���-�ļ�������fd
 * */
int Uart_Open(int comNum, int nSpeed, int nBits, char nEvent, int nStop, int c_min);

#endif /* SERIAL_H_ */
