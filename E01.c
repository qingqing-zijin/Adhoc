/*
 * main.c
 *
 *  Created on: Jul 12, 2018
 *      Author: root
 */

#include <stdio.h>
#include "nRF24L01.h"
#include "spi.h"

int config_E01_radio_params()
{
	//打开/dev/spidev0.0设备
	SPI_Open();
	L01_GPIOs_Init();
	//初始化L01芯片
	L01_Chip_Init();
	/*
	 * 母板上SPI MISO<----> MOSI短路链接
	 * */
//	SPI_LookBackTest();
//
//	while(1)
//	{
//		L01_SendPacketTest();
//		L01_RecvPacketTest();
//	}
	return 0;
}
