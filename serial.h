/*
 * serial.h
 *
 *  Created on: Mar 9, 2018
 *      Author: root
 */

#ifndef SERIAL_H_
#define SERIAL_H_

/**
 *宏开关 ：采用UART转无线透明传输模块
 */



typedef enum
{
        COM0,
        COM1, //E32,实测linux下/dev/ttymxc1，对应UART2，用COM1表示
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
 * @Desc 	 打开指定comNum 串口，并指定串口波特率、数据位长度、校验位、停止位和接收时最小字符数返回
 * @param1 comNum 	串口id
 * @param2 nSpeed 	波特率
 * @param3 nBits	数据位长度
 * @param4 nEvents 	奇偶校验位
 * @param5 nStop	停止位
 * @param6 c_min	最小接收字符个数
 * @Return 返回打开的串口-文件描述符fd
 * */
int Uart_Open(int comNum, int nSpeed, int nBits, char nEvent, int nStop, int c_min);

#endif /* SERIAL_H_ */
