/*
 * nRF24L01.c
 *
 *  Created on: Jul 12, 2018
 *      Author: root
 */
#include <stdio.h>
#include "nRF24L01.h"
#include "aux_func.h"
#include "gpio.h"
#include "spi.h"

int L01_IRQ_fd;

void DelayMs(uint16_t ms)
{
	usleep(1000*ms);
}

/*===========================================================================
* 函数 ：L01_ReadSingleReg() => 读取一个寄存器的值                          *
* 输入 ：Addr，读取的寄存器地址                                             *
* 输出 ：读出的值                                                           *
============================================================================*/
uint8_t L01_ReadSingleReg(uint8_t Addr)
{
    uint8_t btmp;

    L01_CSN_LOW();
#if 0
    SPI_ExchangeByte(R_REGISTER | Addr);
    btmp = SPI_ExchangeByte(0xFF);
#else
    btmp = SPI_ReadSingleByte(Addr);
#endif
    L01_CSN_HIGH();

    return (btmp);
}

/*===========================================================================
* 函数 ：L01_WriteSingleReg() => 写数据到一个寄存器                         *
* 输入 ：Addr，写入寄存器的地址，Value，待写入的值                          *
============================================================================*/
void L01_WriteSingleReg(uint8_t Addr, uint8_t Value)
{
    L01_CSN_LOW();

#if 0
//    SPI_ExchangeByte(W_REGISTER | Addr);
//    SPI_ExchangeByte(Value);
#else
    SPI_WriteSingleByte(Addr, Value);
#endif
    L01_CSN_HIGH();
}

/*===========================================================================
* 函数 ：L01_WriteMultiReg() => 写数据到多个寄存器                          *
* 输入 ：StartAddr,写入寄存器的首地址，pBuff,指向待写入的值，Length,长度    *
============================================================================*/
void L01_WriteMultiReg(uint8_t StartAddr, uint8_t *pBuff, uint8_t Length)
{
    uint8_t i;
    //STEP1：先SPI片选有效，
    L01_CSN_LOW();

#if 0
    //STEP2：发送“写命令+寄存器地址”
    SPI_ExchangeByte(W_REGISTER | StartAddr);
    //STEP3：连续写多个字节
    for (i=0; i<Length; i++)    { SPI_ExchangeByte(*(pBuff+i)); }
#else
    SPI_WriteMultiBytes(StartAddr, pBuff, Length);
#endif
    //STEP4：最后置SPI片选无效
    L01_CSN_HIGH();
}

/*===========================================================================
* 函数 ：L01_FlushTX() => 复位TX FIFO指针                                   *
============================================================================*/
void L01_FlushTX(void)
{
    L01_CSN_LOW();
#if 0
    SPI_ExchangeByte(FLUSH_TX);
#else
    SPI_WriteCommand(FLUSH_TX);
#endif
    L01_CSN_HIGH();
}
/*===========================================================================
* 函数 ：L01_FlushRX() => 复位RX FIFO指针                                   *
============================================================================*/
void L01_FlushRX(void)
{
    L01_CSN_LOW();
#if 0
    SPI_ExchangeByte(FLUSH_RX);
#else
    SPI_WriteCommand(FLUSH_RX);
#endif
    L01_CSN_HIGH();
}

uint8_t L01_ReadStatusReg(void)
{
    uint8_t Status;
    L01_CSN_LOW();
#if 0
    Status = SPI_ExchangeByte(R_REGISTER + L01REG_STATUS);
#else
    Status = SPI_WriteCommand(R_REGISTER + L01REG_STATUS);
#endif

    L01_CSN_HIGH();
    return (Status);
}

/*===========================================================================
* 函数 ：L01_ClearIRQ() => 清除中断                                         *
* 输入 ：IRQ_Source，需要清除的中断源                                       *
============================================================================*/
void L01_ClearIRQ(uint8_t IRQ_Source)
{
    uint8_t btmp = 0;

    //
    IRQ_Source &= (1<<RX_DR) | (1<<TX_DS) | (1<<MAX_RT);
    btmp = L01_ReadStatusReg();

    L01_CSN_LOW();

#if 0
    SPI_ExchangeByte(W_REGISTER + L01REG_STATUS);		//先写寄存器地址
    SPI_ExchangeByte(IRQ_Source | btmp);				//再写寄存器值
#else
    SPI_WriteSingleByte(W_REGISTER + L01REG_STATUS,IRQ_Source | btmp);
#endif
    L01_CSN_HIGH();
    L01_ReadStatusReg();
}

/*===========================================================================
* 函数 ：L01_ReadIRQSource() => 读取中断                                    *
* 输出 ：读出的中断源                                                       *
============================================================================*/
uint8_t L01_ReadIRQSource(void)
{
	/*
	 * STATUS寄存器（0x07）
	 * 1'bit6 :RX_DR, Data Ready RX FIFO interrupt. Assert when new data arrives RX FIFO;
	 * 			Write 1 to clear bit.
	 * 1'bit5 : TX_DR, Data snet TX FIFO interrupt. Assert when packet transmitted on TX;
	 * 			if AUTO_ACK is activated, this bit is set high only when ACK is received.
	 *			Write 1 to clear bit.
	 * 1'bit4 : MAX_RT,Maximum number of TX retransmits interrupt. Write 1 to clear bit.
	 * 			if MAX+RT is asserted, it must be cleared to enable further communication.
	 * */
    return (L01_ReadStatusReg() & ((1<<RX_DR)|(1<<TX_DS)|(1<<MAX_RT)));
}

/*===========================================================================
* 函数 ：L01_ReadTopFIFOWidth() => 读取FIFO数据宽度                         *
============================================================================*/
uint8_t L01_ReadTopFIFOWidth(void)
{
    uint8_t sz;

    L01_CSN_LOW();							//STEP1: SPI使能
#if 0
    SPI_ExchangeByte(R_RX_PL_WID);			//STEP2：发送R_RX_PL_WID命令
    sz = SPI_ExchangeByte(0xFF);			//STEP3：返回PL_WID值
#else
    sz=SPI_WriteCommandWithAck(R_RX_PL_WID,0xFF);
#endif
    L01_CSN_HIGH();

    return (sz);
}

/*===========================================================================
* 函数 ：L01_ReadRXPayload() => 读取接收到的数据                            *
* 输入 ：pBuff，指向收到的数据                                              *
* 输出 ：数据长度                                                           *
============================================================================*/
uint8_t L01_ReadRXPayload(uint8_t *pBuff)
{
    uint8_t width, PipeNum;

    /*
     * Data pipe number for the payload available for reading from RX_FIFO
     * 000-101: Data pipe Number
     * 110: Not Used
     * 111: Rx FIFO Empty
     * */
    PipeNum = (L01_ReadSingleReg(L01REG_STATUS)>>1) & 0x07;
    width = L01_ReadTopFIFOWidth();

    L01_CSN_LOW();
    /*
     * Page51:RX_RX_PAYLOAD命令
     * 		Read Rx-payload: 1-32bytes, A read operation always starts at byte0.
     * 		Payload is deleted from FIFO after it's read. Used in RX mode.
     * */
#if 0

    SPI_ExchangeByte(R_RX_PAYLOAD);		//发送RX_RX_PAYLOAD命令
    for (PipeNum=0; PipeNum<width; PipeNum++)
    {
        *(pBuff+PipeNum) = SPI_ExchangeByte(0xFF);
    }
#else
    SPI_ReadRxFIFO(R_RX_PAYLOAD, pBuff,width );
#endif
    L01_CSN_HIGH();

    L01_FlushRX();

    return (width);
}

/*===========================================================================
* 函数 ：L01_WriteTXPayload_Ack() => 写数据到TXFIFO(带ACK返回)              *
* 输入 ：pBuff，指向待写入的数据，nBytes，写入数据的长度                    *
============================================================================*/
void L01_WriteTXPayload_Ack(uint8_t *pBuff, uint8_t nBytes)
{
    uint8_t btmp;
    uint8_t length = (nBytes>32) ? 32 : nBytes;

    L01_FlushTX();
    L01_CSN_LOW();
#if 0
    SPI_ExchangeByte(W_TX_PAYLOAD);
    for (btmp=0; btmp<length; btmp++)   { SPI_ExchangeByte(*(pBuff+btmp)); }
#else
    SPI_WriteTxFIFO(W_TX_PAYLOAD, pBuff, length);
#endif
    L01_CSN_HIGH();
}

/*===========================================================================
* 函数 ：L01_WriteTXPayload_Ack() => 写数据到TXFIFO(不带ACK返回)            *
* 输入 ：Data，指向待写入的数据，Data_Length，写入数据的长度                *
============================================================================*/
void L01_WriteTXPayload_NoAck(uint8_t *Data, uint8_t Data_Length)
{
    if ((Data_Length>32) || (Data_Length==0))
    { return; }

    L01_CSN_LOW();
#if 0
    SPI_ExchangeByte(W_TX_PAYLOAD_NOACK);
    while (Data_Length--)
	{ SPI_ExchangeByte(*Data++); }
#else
    SPI_WriteTxFIFO(W_TX_PAYLOAD_NOACK, Data, Data_Length);
#endif
    L01_CSN_HIGH();
}

/*===========================================================================
* 函数 ：L01_SetTXAddr() => 设置发送物理地址                                *
* 输入 ：pAddr指向需要设置的地址数据，Addr_Length，地址长度                 *
============================================================================*/
void L01_SetTXAddr(uint8_t *pAddr, uint8_t Addr_Length)
{
    uint8_t Length = (Addr_Length>5) ? 5 : Addr_Length;
    L01_WriteMultiReg(L01REG_TX_ADDR, pAddr, Length);
}

/*===========================================================================
* 函数 ：L01_SetRXAddr() => 设置接收物理地址                                *
* 输入 ：PipeNum，管道号，pAddr指向需要设置地址数据，Addr_Length，地址长度  *
============================================================================*/
void L01_SetRXAddr(uint8_t PipeNum, uint8_t *pAddr, uint8_t Addr_Length)
{
    uint8_t Length = (Addr_Length>5) ? 5 : Addr_Length;
    uint8_t pipe = (PipeNum>5) ? 5 : PipeNum;

    L01_WriteMultiReg(L01REG_RX_ADDR_P0 + pipe, pAddr, Length);
}

/*===========================================================================
* 函数 ：L01_SetSpeed() => 设置L01空速                                      *
* 输入 ：speed，=SPD_250K(250K), =SPD_1M(1M), =SPD_2M(2M)                   *
============================================================================*/
void L01_SetSpeed(L01SPD speed)
{
	uint8_t btmp = L01_ReadSingleReg(L01REG_RF_SETUP);

	btmp &= ~((1<<5) | (1<<3));

	switch (speed)
	{
	    case SPD_250K:  btmp |= (1<<5);             break;  // 250K
        case SPD_1M:    btmp &= ~((1<<5) | (1<<3)); break;  // 1M
        case SPD_2M:    btmp |= (1<<3);             break;  // 2M
        default:        break;

	}
	L01_WriteSingleReg(L01REG_RF_SETUP, btmp);
}

/*===========================================================================
* 函数 ：L01_SetPower() => 设置L01功率                                      *
* 输入 ：power, =P_F18DBM(18DB),=P_F12DBM(12DB),=P_F6DBM(6DB),=P_0DBM(0DB)  *
============================================================================*/
void L01_SetPower(L01PWR power)
{
    uint8_t btmp = L01_ReadSingleReg(L01REG_RF_SETUP) & ~0x07;

    switch(power)
    {
        case P_F18DBM:  btmp |= PWR_18DB; break;    // 18DBM
        case P_F12DBM:  btmp |= PWR_12DB; break;    // 12DBM
        case P_F6DBM:   btmp |= PWR_6DB;  break;    // 6DBM
        case P_0DBM:    btmp |= PWR_0DB;  break;    // 0DBM
        default:        break;
    }
    L01_WriteSingleReg(L01REG_RF_SETUP, btmp);
}


/*===========================================================================
* 函数 ：L01_WriteHoppingPoint() => 设置L01频率                             *
* 输入 ：FreqPoint，待设置的频率                                            *
============================================================================*/
void L01_WriteFreqPoint(uint8_t FreqPoint)
{
    L01_WriteSingleReg(L01REG_RF_CH, FreqPoint & 0x7F);
}

/*===========================================================================
* 函数 ：L01_SetTRMode() => 设置L01模式                                     *
* 输入 ：mode，=TX_MODE, TX mode; =RX_MODE, RX mode                         *
============================================================================*/
void L01_SetTRMode(L01MD mode)
{
    uint8_t controlreg = L01_ReadSingleReg(L01REG_CONFIG);
    if      (mode == TX_MODE)       { controlreg &= ~(1<<PRIM_RX); }
    else if (mode == RX_MODE)       { controlreg |= (1<<PRIM_RX); }

    L01_WriteSingleReg(L01REG_CONFIG, controlreg);
}

/*===========================================================================
* 函数 ：L01_Init() => 初始化L01                                             *
============================================================================*/
void L01_Init(void)
{
	uint8_t REG_Byte1=0, REG_Byte2 =0;
    uint8_t addr[5] = { INIT_ADDR };		//初始地址为0x01,0x02,0x03,0x04,0x05

    L01_CE_LOW();
    /*
     * The IRQ pin resets when MCU write '1' tp the IRQ source bit in the STATUS register.
     * The IRQ mask in the Config register is used to select the IRQ source that allowed to
     * assert the IRQ pin.
     *
     * By setting one of the MASK bits high, the correspondind IRQ source is disabled.
     *
     * By default , all IRQ sources are disabled.
     *
     * TX_DS: Data Sent TX FIFO interrupt. Asserted when packet transmitted on TX.
     * if AUTO_ACK is activated this bit is set high only when ACK is received . Write 1 to clear bit.
     * */
    L01_ClearIRQ(IRQ_ALL);

    // 使能管道0动态包长度
    L01_WriteSingleReg(L01REG_DYNPD, (1<<0));
    REG_Byte1 =L01_ReadSingleReg(L01REG_DYNPD);


    /*
     * bit[2]: Enables Dynamic payload length
     * bit[1]: Enables payload with  ACK
     * bit[0]: EN_DYN_ACK, Enables the W_TX_PAYLOAD_NOACK command
     *
     * 注意必须使能bit[0]
     * */
    L01_WriteSingleReg(L01REG_FEATRUE, 0x07);
    REG_Byte2 =L01_ReadSingleReg(L01REG_FEATRUE);

//    if(REG_Byte1 != 1 || REG_Byte2 !=6)
//    	exit(0);
    //地址0寄存器-CONFIG
    L01_WriteSingleReg(L01REG_CONFIG, (1<<EN_CRC)|(1<<PWR_UP));

    //地址1寄存器-自动应答
    L01_WriteSingleReg(L01REG_EN_AA, (1<<ENAA_P0));     // 自动应答（管道0）

    //地址2寄存器-使能接收数据PIPE地址
    L01_WriteSingleReg(L01REG_EN_RXADDR, (1<<ERX_P0));  // 使能接收（管道0）

    //地址3-设置地址宽度，00-非法，01：3字节，10：4字节，11-5字节
    L01_WriteSingleReg(L01REG_SETUP_AW, AW_5BYTES);     // 地址宽度 5byte

    //地址4-设置自动重传参数，4‘bit[7:4]:1111,表示等待400us; 4'bit[3:0]：1111表示自动重传15次
    L01_WriteSingleReg(L01REG_RETR, ARD_4000US|(REPEAT_CNT&0x0F));

    //地址5： 7'bit[6:0],用于设置工作频率（信道）RF Channel
    L01_WriteSingleReg(L01REG_RF_CH, 60);               // 初始化频率
    REG_Byte2 =L01_ReadSingleReg(L01REG_RF_CH);
    //地址7：RF设置寄存器，包括速率设置，功率;
    /*2'bit[2:1]=00: -18dBm
     * 			 01: -12dBm
     * 			 10: -6dBm
     * 			 11: 0dBm
     *
     *{1'bit[5]:1'bit[3]}:
     *			00: 1Mbps
     *			01: 2Mbps
     *			10: 250kbps
     *			11: 保留
     *
     * */
    L01_WriteSingleReg(L01REG_RF_SETUP, 0x26);			//速率250kbps, 功率设置为0dBm

    L01_SetTXAddr(&addr[0], 5);                         // 设置地址（发送）
    L01_SetRXAddr(0, &addr[0], 5);                      // 设置地址（接收）
    L01_WriteFreqPoint(77);                          // 设置频率

    switch(g_radio_speed)
    {
		case SPD_E01_2M:
		{
			L01_SetSpeed(SPD_2M);                             // 设置空速为2Mbps
			break;
		}
		case SPD_E01_1M:
		{
			L01_SetSpeed(SPD_1M);                             // 设置空速为1Mbps
			break;
		}
		case SPD_E01_250K:
		default:
		{
			L01_SetSpeed(SPD_250K);                             // 设置空速为250K
			break;
		}
    }
}

/*===========================================================================
* 函数 ：L01_GPIOs_Init() =>L01初始化芯片控制引脚	                           *
============================================================================*/
int L01_GPIOs_Init()
{

	//CE引脚初始化
	gpio_export(L01_CE_PIN_NUM);
	gpio_direction(L01_CE_PIN_NUM, GPIO_OUT);

	//IRQ输入引脚
	gpio_export(L01_IRQ_PIN_NUM);
	gpio_direction(L01_IRQ_PIN_NUM, GPIO_IN);
	//使能上升沿中断
//	gpio_edge(L01_IRQ_PIN_NUM,RISING);

	//使能下降沿中断
	gpio_edge(L01_IRQ_PIN_NUM,FALLING);
	//打开AUX 对应/sys/class/gpio/gpiox/value文件，实时获取AUX值
	L01_IRQ_fd = open_gpio_rw_fd(L01_IRQ_PIN_NUM);

	if(L01_IRQ_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open L01_IRQ value!\n");
		return -1;
	}
	return 0;
}

/*===========================================================================
* 函数 ：L01_SendPacket() =>L01无线发送数据函数                                *
============================================================================*/
void L01_SendPacketTest(void)
{
	uint8_t i=0;

	uint8_t length=32;				//发送最大长度为32字节

	uint8_t buffer[65]={0};

	for (i=0; i<length; i++)		//初始化发送内容
	{
		buffer[i] = i;
	}

	L01_CE_LOW();               // CE = 0, 关闭发送

	L01_SetTRMode(TX_MODE);     // 设置为发送模式

    L01_WriteTXPayload_NoAck(buffer, length);

	L01_CE_HIGH();              // CE = 1, 启动发射

	DelayMs(250);

	// 等待发射中断产生
	while (0 != L01_IRQ_READ());

	//中断引脚IRQ变高后，判断STATUS寄存器是否相应位置1
	while (0 == L01_ReadIRQSource());

	//中断发生后，发送结束，将芯片片选置为无效
	L01_CE_LOW();               // CE = 0, 关闭发送

	L01_FlushRX();              // 复位接收FIFO指针
	L01_FlushTX();              // 复位发送FIFO指针
	L01_ClearIRQ(IRQ_ALL);      // 清除中断

	L01_SetTRMode(RX_MODE);     // 转到接收模式
	L01_CE_HIGH();              // 启动接收
}


int L01_SendPacket(uint8_t* buffer, uint32_t length)
{
	if(length > E01_MAX_PKT_BYTES)
	{
		LOG_MSG(ERR_LEVEL, "L01_SendPacket length(%d) value too big!\n",length);
		return -1;
	}
	/*
	 * 关于E01发送操作，Page23:
	 * The TX mode is an active mode for transmitting packets. To enter this mode,
	 * the nRF24L01+must have the PWR_UP bit set high, PRIM_RX bit set low, a payload
	 * int the FIFO and a high pulse on the CE for more than 10us.
	 * */
	L01_CE_LOW();               // CE = 0, 关闭发送

	L01_SetTRMode(TX_MODE);     // 设置为发送模式

    L01_WriteTXPayload_NoAck(buffer, length);

	L01_CE_HIGH();              // CE = 1, 启动发射

//	DelayMs(1);

	// 等待发射中断产生
//	while (0 != L01_IRQ_READ());
	//中断引脚IRQ变高后，判断STATUS寄存器是否相应位置1
//	while (0 == L01_ReadIRQSource());

	/*
	 * 1>e01模块把所有TX FIFO中数据都发送完毕后，将IRQ引脚置为低电平，并且将STATUS寄存器中TS_DS位置位1
	 * 下面函数gpio_edge_poll阻塞等待AUX上升沿到来后，然后返回.
	 * 2> Poll函数参数timeout =-1时, 表示阻塞等待TX sent事件发生
	 * */
	gpio_edge_poll(L01_IRQ_fd, TX_AUX, 0x1ffff);//-1);		//0，表示阻塞等待TX_DS IRQ事件发生
	while (0 == (L01_ReadIRQSource() &(1<<TX_DS)));

	//中断发生后，发送结束，将芯片片选置为无效
	L01_CE_LOW();               // CE = 0, 关闭发送
	L01_FlushRX();              // 复位接收FIFO指针
	L01_FlushTX();              // 复位发送FIFO指针
	L01_ClearIRQ(IRQ_ALL);      // 清除中断
	L01_SetTRMode(RX_MODE);     // 转到接收模式
	L01_CE_HIGH();              // 启动接收
	return 0;
}


/*===========================================================================
* 函数 ：L01_RecvPacket() => L01无线数据接收处理                               *
============================================================================*/
uint32_t cnt=0;

int L01_RecvPacket(uint8_t* rx_buffer, uint32_t length)
{
	uint8_t ret=0;

//    if (0 == L01_IRQ_READ())                    // 检测无线模块是否产生接收中断
//    {
	/*
	 * 对于接收，以“轮询”方式判断是否有接收事件发生
	 * */


	if (L01_ReadIRQSource() & (1<<RX_DR))   // 检测无线模块是否接收到数据
	{
		// 读取接收到的数据长度和数据内容
		ret = L01_ReadRXPayload(rx_buffer);

		L01_FlushRX();                          // 复位接收FIFO指针
		L01_ClearIRQ(IRQ_ALL);                  // 清除中断
	}

//    }
    return ret;
}

void L01_RecvPacketTest()
{

	uint8_t length=0, recv_buffer[64]={0};

	uint8_t i=0;

    if (0 == L01_IRQ_READ())                    // 检测无线模块是否产生接收中断
    {
        if (L01_ReadIRQSource() & (1<<RX_DR))   // 检测无线模块是否接收到数据
        {
            // 读取接收到的数据长度和数据内容
        	length = L01_ReadRXPayload(recv_buffer);
            printf("\n cnt=%d, L01_ReadRXPayload [%d]bytes:\n",++cnt,length );
            for(i=0; i<length; i++)
			{
            	printf("rx[%d]=%d\t", i,recv_buffer[i]);
            	if((i+1)%5==0)
            	{
            		printf("\n");
            	}
			}
        }

        L01_FlushRX();                          // 复位接收FIFO指针
        L01_ClearIRQ(IRQ_ALL);                  // 清除中断
    }
}


/*===========================================================================
* 函数 ：L01_Chip_Init() => 初始化RF芯片                                       *
* 说明 ：L01+的操作，已经被建成C库，见nRF24L01.c文件， 提供SPI和CSN操作，	*
         即可调用其内部所有函数用户无需再关心L01+的寄存器操作问题。			*
============================================================================*/
void L01_Chip_Init(void)
{

	L01_Init();             // 初始化L01寄存器
	L01_SetTRMode(RX_MODE); // 接收模式
	L01_FlushRX();          // 复位接收FIFO指针
    L01_FlushTX();          // 复位发送FIFO指针
    L01_ClearIRQ(IRQ_ALL);  // 清除所有中断
    L01_CE_HIGH();          // CE = 1, 启动接收
}


