
//////////////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include<sys/types.h>
#include<sys/stat.h>
#include <stdlib.h>

#include "serial.h"
#include "aux_func.h"

#define FALSE 0
#define TRUE 1

/*
 * @finction open_com
 * @description: 打开串口函数
 * @param comNUm: 要打开的串口编号
 * @param return: 返回打开的串口文件描述符
 * */
static int open_com(int comNum) {

	int fd =-1;
	/*
	 * 业务无线模块E32占用COM1，对应/dev/ttymxc1，对应硬件上的UART2
	 * */
	char *dev[] = { "/dev/ttymxc0", "/dev/ttymxc1", "/dev/ttymxc2",
			"/dev/ttymxc3", "/dev/ttymxc4", "/dev/ttymxc5", "/dev/ttymxc6","/dev/ttymxc7" };

	if (comNum > 7 || comNum <0)
	{
		perror("cannot open Serial Port");
		return -1;
	}
	/*
	 * O_NDELAY即非阻塞模式。由于读函数采用select多路IO复用机制，
	 *
	 * */
	fd = open(dev[comNum], O_RDWR | O_NOCTTY /*| O_NDELAY*/);

	if (-1 == fd) {
		perror("open Serial Port failed!\n");
		return -1;
	}

	//阻塞
	if (fcntl(fd, F_SETFL, 0) < 0)
	{
		perror("fcntl failed!\n");
		exit(-1);
	}
	//若非阻塞，则为fcntl(fd, F_SETFL, FNDELAY);


//	if (isatty(fd) == 0)
//		perror("standard input is not a terminal device\n");
//	else
//		perror("isatty success!\n");

	return fd;
}

/*
 *
 *
 *
 */

static int set_opt(int fd, int nSpeed, int nBits, char nEvent, int nStop,int c_min) {

	struct termios newtio, oldtio;

	//save current serial port settings
	if (tcgetattr(fd, &oldtio) != 0) {
		perror("Setup Serial 1\n");
		return (-1);
	}
	bzero(&newtio, sizeof(newtio)); // clear struct for new port settings

	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;

	//Set stopBits
	switch (nBits) {
	case 7:
		newtio.c_cflag |= CS7;
		break;
	case 8:
		newtio.c_cflag |= CS8;
		break;
	default:
		break;
	}
	//Set nEvent
	switch (nEvent)
	{
	case 'O':
		newtio.c_cflag |= PARENB;
		newtio.c_cflag |= PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
		break;
	case 'E':
		newtio.c_cflag |= PARENB;
		newtio.c_cflag &= ~PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
		break;
	case 'N':
		newtio.c_cflag &= ~PARENB;
		break;
	}

	//Baudrate
	switch (nSpeed) {
	case 2400:
		cfsetispeed(&newtio, B2400);
		cfsetospeed(&newtio, B2400);
		break;
	case 4800:
		cfsetispeed(&newtio, B4800);
		cfsetospeed(&newtio, B4800);
		break;
	case 9600:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	case 115200:
		cfsetispeed(&newtio, B115200);
		cfsetospeed(&newtio, B115200);
		break;
	case 460800:
		cfsetispeed(&newtio, B460800);
		cfsetospeed(&newtio, B460800);
		break;
	default:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	}

	if (nStop == 1)
		newtio.c_cflag &= ~CSTOPB;
	else if (nStop == 2)
		newtio.c_cflag |= CSTOPB;

	newtio.c_cc[VINTR] = 0; /* Ctrl-c*/
	newtio.c_cc[VQUIT] = 0; /* Ctrl-\ */
	newtio.c_cc[VERASE] = 0; /* del */
	newtio.c_cc[VKILL] = 0; /* @ */
	newtio.c_cc[VEOF] = 4; /* Ctrl-d */
	newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
	newtio.c_cc[VMIN] = c_min; /* blocking read until 1 character
	 arrives */
	newtio.c_cc[VSWTC] = 0; /* '\0' */
	newtio.c_cc[VSTART] = 0; /* Ctrl-q */
	newtio.c_cc[VSTOP] = 0; /* Ctrl-s */
	newtio.c_cc[VSUSP] = 0; /* Ctrl-z */
	newtio.c_cc[VEOL] = 0; /* '\0' */
	newtio.c_cc[VREPRINT] = 0; /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0; /* Ctrl-u */
	newtio.c_cc[VWERASE] = 0; /* Ctrl-w */
	newtio.c_cc[VLNEXT] = 0; /* Ctrl-v */
	newtio.c_cc[VEOL2] = 0; /* '\0' */

	tcflush(fd, TCIFLUSH);
	if(tcsetattr(fd, TCSANOW, &newtio) !=0)
	{
		perror("SerialCom set Error\n");
		return -1;
	}

	return 0;

}

/**
 *
 *
 */
int uartsendto(int fd, u8* sendBytes, int len)
{
	int ret =0;
	if(fd <0){
		perror(" Serial Port fd error\n");
		return -1;
	}
	if(sendBytes == NULL || len ==0)
		return 0;
	ret = write(fd, sendBytes, len);
	return ret;
}

/**
 *
 *
 */


int uartrecvfrom(int fd, u8* buffer, int len)
{

	int ret =0;
	struct timeval tv;

	fd_set rd;

	tv.tv_sec =4;
	tv.tv_usec =0;

	if(fd <0){
		perror(" Serial Port fd error\n");
		return -1;
	}

	if(buffer == NULL || len ==0)
		return 0;

	FD_ZERO(&rd);
	FD_SET(fd, &rd);

	/*
	 * 使用select函数，设置所关心的文件描述符集、希望等待的时间等;从select函数
	 * 返回时，内核会通知用户已准备好的文件描述符的数量、已准备的条件等。
	 * select函数返回值，成功返回准备好的文件描述符;出错：返回-1.
	 * */

	/*
	 *
	 * select函数的返回值,参考Linux manual :
	 * RETURN VALUE:
	 * 	1> On success, select return the number of file descriptors contained in the
	 * 	three return descriptor sets(that is, the total number of bits that are set in
	 * 	readfds, writefds,exceptfds),
	 * 	2> On timeout expires, which may be zero if the timeout expires before anything interesting happens.
	 * 	3> On error, -1 is returned, and errno is set to indicate the error;
	 * 	the file descriptor sets are unmodified, and timeout becomes undefined.
	 * */

	ret = select(fd+1,&rd, NULL,NULL,&tv );
	if(ret <=0)
	{
//		LOG_MSG(DBG_LEVEL," Select error / timeout\n");
//		LOG_MSG(DBG_LEVEL," Select timeout error\n");
		return ret;
	}
	else
	{
		if(FD_ISSET(fd, &rd))
		{
			ret = read(fd, buffer, len);
		}
	}
	return ret;
}

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
int Uart_Open(int comNum, int nSpeed, int nBits, char nEvent, int nStop, int c_min)
{
	int fd=-1, ret =-1;
	fd = open_com(comNum);
	if(fd <0)
	{
		perror("open Serial Port Failed\n");
		return -1;
	}

	ret =set_opt(fd, nSpeed, nBits, nEvent, nStop, c_min);

	if(ret <0)
	{

		perror("Serial Port Set Failed\n");
		return -1;
	}

	return fd;

}





