/*
 * main.c
 *
 *  Created on: Jul 12, 2018
 *      Author: root
 */
/*
 * 说明：SPI通讯实现
 * 方式一： 同时发送与接收实现函数： SPI_Transfer()
 * 方式二：发送与接收分开来实现
 * SPI_Write() 只发送
 * SPI_Read() 只接收
 * 两种方式不同之处：方式一，在发的过程中也在接收，第二种方式，收与发单独进行
 * Created on: 2013-5-28
 * Author: lzy
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include "aux_func.h"
#include "nRF24L01.h"

static const char *L01_spidev_path = "/dev/spidev0.0";
static uint8_t 	mode = 0; 			/* SPI通信使用全双工，设置CPOL＝0，CPHA＝0。 */
static uint8_t 	bits = 8; 			/* 8ｂiｔｓ读写，MSB first。*/

/*
 * 芯片手册Page53页，Fsclk最高10MHz
 * */
static uint32_t speed = 8 * 1000 * 1000;/* 设置12M传输速度 */
//static uint32_t speed = 16 * 1000 * 1000;/* 设置16M传输速度 */

static int g_SPI_Fd = 0;


////void LOG_MSG(0,int level, char *format, ...)
//void LOG_MSG(int level, char *format, ...)
//{
//	va_list	args;
//	time_t curtime;
//	unsigned long long cur_time;
//	va_start(args, format);
//	cur_time =time(&curtime);
//	printf("%lld:%lld:%lld.%03lld  ",cur_time/1000/3600, (cur_time/1000/60)%60, (cur_time/1000)%60, cur_time%1000);
//	vprintf(format, args);
//	va_end(args);
//}


#define SPI_DEBUG 0

static void pabort(const char *s) {
	perror(s);
	abort();
}

/*
 *
 * */
uint8_t SPI_WriteTxFIFO(uint8_t w_tx_payload_cmd,uint8_t *pBuff,uint8_t len)
{
	int ret;
	int fd = g_SPI_Fd;

	uint8_t status;
	uint8_t rx_dump=0xff;

	uint8_t i=0;

	struct spi_ioc_transfer *xfers;

	if(pBuff == NULL || len ==0)
		return -1;

	xfers = (struct spi_ioc_transfer *)malloc(sizeof(struct spi_ioc_transfer )*(len+1));

	if(xfers== NULL)
	{
		LOG_MSG(ERR_LEVEL, "SPI_WriteTxFIFO malloc failed\n");
		exit(-1);
	}

	memset(xfers,0, sizeof(struct spi_ioc_transfer)*(len+1));

	xfers[0].tx_buf= (unsigned long) (&w_tx_payload_cmd);
	xfers[0].rx_buf= (unsigned long) (&status);
	xfers[0].len = 1;
	xfers[0].speed_hz =8000000;
	xfers[0].delay_usecs =0;
//	xfers[0].cs_change = 0;


	for(i=1; i<= len; i++)
	{
		xfers[i].tx_buf= (unsigned long) (&pBuff[i-1]);
		xfers[i].rx_buf= (unsigned long) (&rx_dump);
		xfers[i].speed_hz =8000000;
		xfers[i].delay_usecs =0;
		xfers[i].len = 1;
//		xfers[i].cs_change = 0;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(len+1), xfers);
	if (ret < 1)
		LOG_MSG(ERR_LEVEL,"SPI_ReadRxFIFO can't send spi message\n");

	free(xfers);
	return status;
}


/*
 *
 * */
uint8_t SPI_ReadRxFIFO(uint8_t r_rx_payload_cmd,uint8_t *pBuff,uint8_t len)
{
	int ret;
	int fd = g_SPI_Fd;

	uint8_t i=0;
	uint8_t status;
	uint8_t tx_dump=0xff;

	struct spi_ioc_transfer *xfers;

	if(pBuff == NULL || len ==0)
		return -1;

	xfers = (struct spi_ioc_transfer *)malloc(sizeof(struct spi_ioc_transfer )*(len+1));
	if(xfers== NULL)
	{
		LOG_MSG(ERR_LEVEL, "SPI_ReadRxFIFO malloc failed\n");
		exit(-1);
	}

	memset(xfers,0, sizeof(struct spi_ioc_transfer)*(len+1));

	xfers[0].tx_buf= (unsigned long) (&r_rx_payload_cmd);
	xfers[0].rx_buf= (unsigned long) (&status);
	xfers[0].len = 1;
	xfers[0].speed_hz =8000000;
	xfers[0].delay_usecs =0;
//	xfers[0].cs_change = 0;


	for(i=1; i<= len; i++)
	{
		xfers[i].tx_buf= (unsigned long) (&tx_dump);
		xfers[i].rx_buf= (unsigned long) (&pBuff[i-1]);
		xfers[i].speed_hz =8000000;
		xfers[i].delay_usecs =0;
		xfers[i].len = 1;
//		xfers[i].cs_change = 0;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(len+1), xfers);
	if (ret < 1)
		LOG_MSG(ERR_LEVEL,"SPI_ReadRxFIFO can't send spi message\n");

	free(xfers);
	return status;
}

/*
 *
 * */
uint8_t SPI_ReadSingleByte(uint8_t Addr)
{
	int ret;
	int fd = g_SPI_Fd;

	uint8_t rx=0;
	uint8_t rx_dum=0xff;
	uint8_t tx_dum=0xff;
	struct spi_ioc_transfer xfers[2];
	memset(xfers,0, sizeof(xfers));

	/*
	 * 第一步设置读命令+读寄存器地址
	 * */
	uint8_t cmd_addr = R_REGISTER | Addr;

	xfers[0].tx_buf= (unsigned long) (&cmd_addr);
	xfers[0].rx_buf= (unsigned long) (&rx_dum);
	xfers[0].speed_hz =8000000;
	xfers[0].delay_usecs =0;
	xfers[0].len = 1;
//	xfer[0].cs_change = 0;

	xfers[1].tx_buf= (unsigned long) (&tx_dum);
	xfers[1].rx_buf= (unsigned long) (&rx);
	xfers[1].len = 1;
	xfers[1].speed_hz =8000000;
	xfers[1].delay_usecs =0;
//	xfer[1].cs_change = 1;
	ret = ioctl(fd, SPI_IOC_MESSAGE(2), &xfers);
	if (ret < 1)
		LOG_MSG(ERR_LEVEL,"SPI_ReadSingleByte can't send spi message\n");
	return rx;
}


/*
 *
 * */
uint8_t SPI_WriteMultiBytes(uint8_t StartAddr, uint8_t *pBuff, uint8_t len)
{
	int ret;
	int fd = g_SPI_Fd;

	uint8_t i=0;
	uint8_t status;
	uint8_t rx_dump=0xff;
	struct spi_ioc_transfer *xfers;

	if(len <=0 || pBuff== NULL)
		return -1;

	xfers = (struct spi_ioc_transfer*) malloc(sizeof(struct spi_ioc_transfer)*(len+1));
	if(xfers== NULL)
	{
		LOG_MSG(ERR_LEVEL, "SPI_WriteMultiBytes malloc failed\n");
		exit(-1);
	}
	memset(xfers,0, sizeof(struct spi_ioc_transfer)*(len+1));

	/*
	 * 第一步设置读命令+读寄存器地址
	 * */
	uint8_t cmd_addr = W_REGISTER | StartAddr;

	xfers[0].tx_buf= (unsigned long) (&cmd_addr);
	xfers[0].rx_buf= (unsigned long) (&status);
	xfers[0].len = 1;
	xfers[0].speed_hz =8000000;
	xfers[0].delay_usecs =0;
//	xfer[0].cs_change = 0;
	for(i=1; i<= len; i++)
	{
		xfers[i].tx_buf= (unsigned long) (&pBuff[i-1]);
		xfers[i].rx_buf= (unsigned long) (&rx_dump);
		xfers[i].speed_hz =8000000;
		xfers[i].delay_usecs =0;
		xfers[i].len = 1;
//		xfers[i].cs_change = 0;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(len+1), xfers);
	if (ret < 1)
		LOG_MSG(ERR_LEVEL,"SPI_WriteMultiBytes can't send spi message\n");

	free(xfers);

	return status;
}

/*
 *
 * */
uint8_t SPI_WriteSingleByte(uint8_t Addr, uint8_t val)
{
	int ret;
	int fd = g_SPI_Fd;

	uint8_t status;
	uint8_t rx_dump=0xff;

	struct spi_ioc_transfer xfers[2];
	memset(xfers,0, sizeof(xfers));

	/*
	 * 第一步设置读命令+读寄存器地址
	 * */
	uint8_t cmd_addr = W_REGISTER | Addr;

	xfers[0].tx_buf= (unsigned long) (&cmd_addr);
	xfers[0].rx_buf= (unsigned long) (&status);
	xfers[0].len = 1;
	xfers[0].speed_hz =8000000;
	xfers[0].delay_usecs =0;
//	xfer[0].cs_change = 0;

	xfers[1].tx_buf= (unsigned long) (&val);
	xfers[1].rx_buf= (unsigned long) (&rx_dump);
	xfers[1].speed_hz =8000000;
	xfers[1].delay_usecs =0;
	xfers[1].len = 1;
//	xfer[1].cs_change = 1;

	ret = ioctl(fd, SPI_IOC_MESSAGE(2), &xfers);
	if (ret < 1)
		LOG_MSG(ERR_LEVEL,"SPI_WriteSingleByte can't send spi message\n");
	return status;
}


/*
 *
 * */
uint8_t SPI_WriteCommand(uint8_t cmd)
{
	int ret;
	int fd = g_SPI_Fd;
	uint8_t status;

	struct spi_ioc_transfer xfer;
	memset(&xfer,0, sizeof(xfer));

	xfer.tx_buf= (unsigned long) (&cmd);
	xfer.rx_buf= (unsigned long) (&status);
	xfer.len = 1;
	xfer.speed_hz =8000000;
	xfer.delay_usecs =0;
//	xfer.cs_change = 0;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &xfer);
	if (ret < 1)
		LOG_MSG(ERR_LEVEL,"SPI_WriteCommand can't send spi message\n");
	return status;
}

/*
 *
 * */
uint8_t SPI_WriteCommandWithAck(uint8_t cmd, uint8_t tx_dump)
{
	int ret;
	int fd = g_SPI_Fd;

	uint8_t rx_ack=0;
	uint8_t status;

	struct spi_ioc_transfer xfers[2];
	memset(xfers,0, sizeof(xfers));

	xfers[0].tx_buf= (unsigned long) (&cmd);
	xfers[0].rx_buf= (unsigned long) (&status);
	xfers[0].len = 1;
	xfers[0].speed_hz =8000000;
	xfers[0].delay_usecs =0;
//	xfers[0].cs_change = 0;


	xfers[1].tx_buf= (unsigned long) (&tx_dump);
	xfers[1].rx_buf= (unsigned long) (&rx_ack);
	xfers[1].len = 1;
	xfers[1].speed_hz =8000000;
	xfers[1].delay_usecs =0;
//	xfers[1].cs_change = 0;

	ret = ioctl(fd, SPI_IOC_MESSAGE(2), &xfers);
	if (ret < 1)
		LOG_MSG(ERR_LEVEL,"SPI_WriteCommandWithAck can't send spi message\n");

	return rx_ack;
}



///*
// *
// * */
//uint8_t SPI_ExchangeByte(uint8_t TxByte)
//{
//
//}



/**
 * 功 能：打开设备 并初始化设备
 * 入口参数 ：
 * 出口参数：
 * 返回值：0 表示已打开 0XF1 表示SPI已打开 其它出错
 * 开发人员：Lzy 2013－5－22
 */
int SPI_Open(void)
{
	int fd;
	int ret = 0;

	if (g_SPI_Fd != 0) /* 设备已打开 */
		return 0xF1;

	fd = open(L01_spidev_path, O_RDWR);
	if (fd < 0)
		pabort("can't open device\n");
	else
		LOG_MSG(0,"SPI - Open Succeed. Start Init SPI...\n");

	g_SPI_Fd = fd;
	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("SPI_Open, can't set spi mode\n");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("SPI_Open, can't get spi mode\n");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("SPI_Open, can't set bits per word\n");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("SPI_Open, can't get bits per word\n");


	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("SPI_Open, can't set max speed hz\n");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("SPI_Open, can't set max speed hz\n");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("SPI_Open, can't get max speed hz\n");

	LOG_MSG(0,"spi mode: %d\n", mode);
	LOG_MSG(0,"bits per word: %d\n", bits);
	LOG_MSG(0,"max speed: %d KHz (%d MHz)\n", speed / 1000, speed / 1000 / 1000);

	return ret;
}

/**
 * 功 能：关闭SPI模块
 */
int SPI_Close(void) {
	int fd = g_SPI_Fd;

	if (fd == 0) /* SPI是否已经打开*/
		return 0;
	close(fd);
	g_SPI_Fd = 0;

	return 0;
}
