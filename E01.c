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
	//��/dev/spidev0.0�豸
	SPI_Open();
	L01_GPIOs_Init();
	//��ʼ��L01оƬ
	L01_Chip_Init();
	/*
	 * ĸ����SPI MISO<----> MOSI��·����
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
