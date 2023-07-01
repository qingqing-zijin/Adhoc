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
* ���� ��L01_ReadSingleReg() => ��ȡһ���Ĵ�����ֵ                          *
* ���� ��Addr����ȡ�ļĴ�����ַ                                             *
* ��� ��������ֵ                                                           *
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
* ���� ��L01_WriteSingleReg() => д���ݵ�һ���Ĵ���                         *
* ���� ��Addr��д��Ĵ����ĵ�ַ��Value����д���ֵ                          *
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
* ���� ��L01_WriteMultiReg() => д���ݵ�����Ĵ���                          *
* ���� ��StartAddr,д��Ĵ������׵�ַ��pBuff,ָ���д���ֵ��Length,����    *
============================================================================*/
void L01_WriteMultiReg(uint8_t StartAddr, uint8_t *pBuff, uint8_t Length)
{
    uint8_t i;
    //STEP1����SPIƬѡ��Ч��
    L01_CSN_LOW();

#if 0
    //STEP2�����͡�д����+�Ĵ�����ַ��
    SPI_ExchangeByte(W_REGISTER | StartAddr);
    //STEP3������д����ֽ�
    for (i=0; i<Length; i++)    { SPI_ExchangeByte(*(pBuff+i)); }
#else
    SPI_WriteMultiBytes(StartAddr, pBuff, Length);
#endif
    //STEP4�������SPIƬѡ��Ч
    L01_CSN_HIGH();
}

/*===========================================================================
* ���� ��L01_FlushTX() => ��λTX FIFOָ��                                   *
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
* ���� ��L01_FlushRX() => ��λRX FIFOָ��                                   *
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
* ���� ��L01_ClearIRQ() => ����ж�                                         *
* ���� ��IRQ_Source����Ҫ������ж�Դ                                       *
============================================================================*/
void L01_ClearIRQ(uint8_t IRQ_Source)
{
    uint8_t btmp = 0;

    //
    IRQ_Source &= (1<<RX_DR) | (1<<TX_DS) | (1<<MAX_RT);
    btmp = L01_ReadStatusReg();

    L01_CSN_LOW();

#if 0
    SPI_ExchangeByte(W_REGISTER + L01REG_STATUS);		//��д�Ĵ�����ַ
    SPI_ExchangeByte(IRQ_Source | btmp);				//��д�Ĵ���ֵ
#else
    SPI_WriteSingleByte(W_REGISTER + L01REG_STATUS,IRQ_Source | btmp);
#endif
    L01_CSN_HIGH();
    L01_ReadStatusReg();
}

/*===========================================================================
* ���� ��L01_ReadIRQSource() => ��ȡ�ж�                                    *
* ��� ���������ж�Դ                                                       *
============================================================================*/
uint8_t L01_ReadIRQSource(void)
{
	/*
	 * STATUS�Ĵ�����0x07��
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
* ���� ��L01_ReadTopFIFOWidth() => ��ȡFIFO���ݿ��                         *
============================================================================*/
uint8_t L01_ReadTopFIFOWidth(void)
{
    uint8_t sz;

    L01_CSN_LOW();							//STEP1: SPIʹ��
#if 0
    SPI_ExchangeByte(R_RX_PL_WID);			//STEP2������R_RX_PL_WID����
    sz = SPI_ExchangeByte(0xFF);			//STEP3������PL_WIDֵ
#else
    sz=SPI_WriteCommandWithAck(R_RX_PL_WID,0xFF);
#endif
    L01_CSN_HIGH();

    return (sz);
}

/*===========================================================================
* ���� ��L01_ReadRXPayload() => ��ȡ���յ�������                            *
* ���� ��pBuff��ָ���յ�������                                              *
* ��� �����ݳ���                                                           *
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
     * Page51:RX_RX_PAYLOAD����
     * 		Read Rx-payload: 1-32bytes, A read operation always starts at byte0.
     * 		Payload is deleted from FIFO after it's read. Used in RX mode.
     * */
#if 0

    SPI_ExchangeByte(R_RX_PAYLOAD);		//����RX_RX_PAYLOAD����
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
* ���� ��L01_WriteTXPayload_Ack() => д���ݵ�TXFIFO(��ACK����)              *
* ���� ��pBuff��ָ���д������ݣ�nBytes��д�����ݵĳ���                    *
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
* ���� ��L01_WriteTXPayload_Ack() => д���ݵ�TXFIFO(����ACK����)            *
* ���� ��Data��ָ���д������ݣ�Data_Length��д�����ݵĳ���                *
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
* ���� ��L01_SetTXAddr() => ���÷��������ַ                                *
* ���� ��pAddrָ����Ҫ���õĵ�ַ���ݣ�Addr_Length����ַ����                 *
============================================================================*/
void L01_SetTXAddr(uint8_t *pAddr, uint8_t Addr_Length)
{
    uint8_t Length = (Addr_Length>5) ? 5 : Addr_Length;
    L01_WriteMultiReg(L01REG_TX_ADDR, pAddr, Length);
}

/*===========================================================================
* ���� ��L01_SetRXAddr() => ���ý��������ַ                                *
* ���� ��PipeNum���ܵ��ţ�pAddrָ����Ҫ���õ�ַ���ݣ�Addr_Length����ַ����  *
============================================================================*/
void L01_SetRXAddr(uint8_t PipeNum, uint8_t *pAddr, uint8_t Addr_Length)
{
    uint8_t Length = (Addr_Length>5) ? 5 : Addr_Length;
    uint8_t pipe = (PipeNum>5) ? 5 : PipeNum;

    L01_WriteMultiReg(L01REG_RX_ADDR_P0 + pipe, pAddr, Length);
}

/*===========================================================================
* ���� ��L01_SetSpeed() => ����L01����                                      *
* ���� ��speed��=SPD_250K(250K), =SPD_1M(1M), =SPD_2M(2M)                   *
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
* ���� ��L01_SetPower() => ����L01����                                      *
* ���� ��power, =P_F18DBM(18DB),=P_F12DBM(12DB),=P_F6DBM(6DB),=P_0DBM(0DB)  *
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
* ���� ��L01_WriteHoppingPoint() => ����L01Ƶ��                             *
* ���� ��FreqPoint�������õ�Ƶ��                                            *
============================================================================*/
void L01_WriteFreqPoint(uint8_t FreqPoint)
{
    L01_WriteSingleReg(L01REG_RF_CH, FreqPoint & 0x7F);
}

/*===========================================================================
* ���� ��L01_SetTRMode() => ����L01ģʽ                                     *
* ���� ��mode��=TX_MODE, TX mode; =RX_MODE, RX mode                         *
============================================================================*/
void L01_SetTRMode(L01MD mode)
{
    uint8_t controlreg = L01_ReadSingleReg(L01REG_CONFIG);
    if      (mode == TX_MODE)       { controlreg &= ~(1<<PRIM_RX); }
    else if (mode == RX_MODE)       { controlreg |= (1<<PRIM_RX); }

    L01_WriteSingleReg(L01REG_CONFIG, controlreg);
}

/*===========================================================================
* ���� ��L01_Init() => ��ʼ��L01                                             *
============================================================================*/
void L01_Init(void)
{
	uint8_t REG_Byte1=0, REG_Byte2 =0;
    uint8_t addr[5] = { INIT_ADDR };		//��ʼ��ַΪ0x01,0x02,0x03,0x04,0x05

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

    // ʹ�ܹܵ�0��̬������
    L01_WriteSingleReg(L01REG_DYNPD, (1<<0));
    REG_Byte1 =L01_ReadSingleReg(L01REG_DYNPD);


    /*
     * bit[2]: Enables Dynamic payload length
     * bit[1]: Enables payload with  ACK
     * bit[0]: EN_DYN_ACK, Enables the W_TX_PAYLOAD_NOACK command
     *
     * ע�����ʹ��bit[0]
     * */
    L01_WriteSingleReg(L01REG_FEATRUE, 0x07);
    REG_Byte2 =L01_ReadSingleReg(L01REG_FEATRUE);

//    if(REG_Byte1 != 1 || REG_Byte2 !=6)
//    	exit(0);
    //��ַ0�Ĵ���-CONFIG
    L01_WriteSingleReg(L01REG_CONFIG, (1<<EN_CRC)|(1<<PWR_UP));

    //��ַ1�Ĵ���-�Զ�Ӧ��
    L01_WriteSingleReg(L01REG_EN_AA, (1<<ENAA_P0));     // �Զ�Ӧ�𣨹ܵ�0��

    //��ַ2�Ĵ���-ʹ�ܽ�������PIPE��ַ
    L01_WriteSingleReg(L01REG_EN_RXADDR, (1<<ERX_P0));  // ʹ�ܽ��գ��ܵ�0��

    //��ַ3-���õ�ַ��ȣ�00-�Ƿ���01��3�ֽڣ�10��4�ֽڣ�11-5�ֽ�
    L01_WriteSingleReg(L01REG_SETUP_AW, AW_5BYTES);     // ��ַ��� 5byte

    //��ַ4-�����Զ��ش�������4��bit[7:4]:1111,��ʾ�ȴ�400us; 4'bit[3:0]��1111��ʾ�Զ��ش�15��
    L01_WriteSingleReg(L01REG_RETR, ARD_4000US|(REPEAT_CNT&0x0F));

    //��ַ5�� 7'bit[6:0],�������ù���Ƶ�ʣ��ŵ���RF Channel
    L01_WriteSingleReg(L01REG_RF_CH, 60);               // ��ʼ��Ƶ��
    REG_Byte2 =L01_ReadSingleReg(L01REG_RF_CH);
    //��ַ7��RF���üĴ����������������ã�����;
    /*2'bit[2:1]=00: -18dBm
     * 			 01: -12dBm
     * 			 10: -6dBm
     * 			 11: 0dBm
     *
     *{1'bit[5]:1'bit[3]}:
     *			00: 1Mbps
     *			01: 2Mbps
     *			10: 250kbps
     *			11: ����
     *
     * */
    L01_WriteSingleReg(L01REG_RF_SETUP, 0x26);			//����250kbps, ��������Ϊ0dBm

    L01_SetTXAddr(&addr[0], 5);                         // ���õ�ַ�����ͣ�
    L01_SetRXAddr(0, &addr[0], 5);                      // ���õ�ַ�����գ�
    L01_WriteFreqPoint(77);                          // ����Ƶ��

    switch(g_radio_speed)
    {
		case SPD_E01_2M:
		{
			L01_SetSpeed(SPD_2M);                             // ���ÿ���Ϊ2Mbps
			break;
		}
		case SPD_E01_1M:
		{
			L01_SetSpeed(SPD_1M);                             // ���ÿ���Ϊ1Mbps
			break;
		}
		case SPD_E01_250K:
		default:
		{
			L01_SetSpeed(SPD_250K);                             // ���ÿ���Ϊ250K
			break;
		}
    }
}

/*===========================================================================
* ���� ��L01_GPIOs_Init() =>L01��ʼ��оƬ��������	                           *
============================================================================*/
int L01_GPIOs_Init()
{

	//CE���ų�ʼ��
	gpio_export(L01_CE_PIN_NUM);
	gpio_direction(L01_CE_PIN_NUM, GPIO_OUT);

	//IRQ��������
	gpio_export(L01_IRQ_PIN_NUM);
	gpio_direction(L01_IRQ_PIN_NUM, GPIO_IN);
	//ʹ���������ж�
//	gpio_edge(L01_IRQ_PIN_NUM,RISING);

	//ʹ���½����ж�
	gpio_edge(L01_IRQ_PIN_NUM,FALLING);
	//��AUX ��Ӧ/sys/class/gpio/gpiox/value�ļ���ʵʱ��ȡAUXֵ
	L01_IRQ_fd = open_gpio_rw_fd(L01_IRQ_PIN_NUM);

	if(L01_IRQ_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open L01_IRQ value!\n");
		return -1;
	}
	return 0;
}

/*===========================================================================
* ���� ��L01_SendPacket() =>L01���߷������ݺ���                                *
============================================================================*/
void L01_SendPacketTest(void)
{
	uint8_t i=0;

	uint8_t length=32;				//������󳤶�Ϊ32�ֽ�

	uint8_t buffer[65]={0};

	for (i=0; i<length; i++)		//��ʼ����������
	{
		buffer[i] = i;
	}

	L01_CE_LOW();               // CE = 0, �رշ���

	L01_SetTRMode(TX_MODE);     // ����Ϊ����ģʽ

    L01_WriteTXPayload_NoAck(buffer, length);

	L01_CE_HIGH();              // CE = 1, ��������

	DelayMs(250);

	// �ȴ������жϲ���
	while (0 != L01_IRQ_READ());

	//�ж�����IRQ��ߺ��ж�STATUS�Ĵ����Ƿ���Ӧλ��1
	while (0 == L01_ReadIRQSource());

	//�жϷ����󣬷��ͽ�������оƬƬѡ��Ϊ��Ч
	L01_CE_LOW();               // CE = 0, �رշ���

	L01_FlushRX();              // ��λ����FIFOָ��
	L01_FlushTX();              // ��λ����FIFOָ��
	L01_ClearIRQ(IRQ_ALL);      // ����ж�

	L01_SetTRMode(RX_MODE);     // ת������ģʽ
	L01_CE_HIGH();              // ��������
}


int L01_SendPacket(uint8_t* buffer, uint32_t length)
{
	if(length > E01_MAX_PKT_BYTES)
	{
		LOG_MSG(ERR_LEVEL, "L01_SendPacket length(%d) value too big!\n",length);
		return -1;
	}
	/*
	 * ����E01���Ͳ�����Page23:
	 * The TX mode is an active mode for transmitting packets. To enter this mode,
	 * the nRF24L01+must have the PWR_UP bit set high, PRIM_RX bit set low, a payload
	 * int the FIFO and a high pulse on the CE for more than 10us.
	 * */
	L01_CE_LOW();               // CE = 0, �رշ���

	L01_SetTRMode(TX_MODE);     // ����Ϊ����ģʽ

    L01_WriteTXPayload_NoAck(buffer, length);

	L01_CE_HIGH();              // CE = 1, ��������

//	DelayMs(1);

	// �ȴ������жϲ���
//	while (0 != L01_IRQ_READ());
	//�ж�����IRQ��ߺ��ж�STATUS�Ĵ����Ƿ���Ӧλ��1
//	while (0 == L01_ReadIRQSource());

	/*
	 * 1>e01ģ�������TX FIFO�����ݶ�������Ϻ󣬽�IRQ������Ϊ�͵�ƽ�����ҽ�STATUS�Ĵ�����TS_DSλ��λ1
	 * ���溯��gpio_edge_poll�����ȴ�AUX�����ص�����Ȼ�󷵻�.
	 * 2> Poll��������timeout =-1ʱ, ��ʾ�����ȴ�TX sent�¼�����
	 * */
	gpio_edge_poll(L01_IRQ_fd, TX_AUX, 0x1ffff);//-1);		//0����ʾ�����ȴ�TX_DS IRQ�¼�����
	while (0 == (L01_ReadIRQSource() &(1<<TX_DS)));

	//�жϷ����󣬷��ͽ�������оƬƬѡ��Ϊ��Ч
	L01_CE_LOW();               // CE = 0, �رշ���
	L01_FlushRX();              // ��λ����FIFOָ��
	L01_FlushTX();              // ��λ����FIFOָ��
	L01_ClearIRQ(IRQ_ALL);      // ����ж�
	L01_SetTRMode(RX_MODE);     // ת������ģʽ
	L01_CE_HIGH();              // ��������
	return 0;
}


/*===========================================================================
* ���� ��L01_RecvPacket() => L01�������ݽ��մ���                               *
============================================================================*/
uint32_t cnt=0;

int L01_RecvPacket(uint8_t* rx_buffer, uint32_t length)
{
	uint8_t ret=0;

//    if (0 == L01_IRQ_READ())                    // �������ģ���Ƿ���������ж�
//    {
	/*
	 * ���ڽ��գ��ԡ���ѯ����ʽ�ж��Ƿ��н����¼�����
	 * */


	if (L01_ReadIRQSource() & (1<<RX_DR))   // �������ģ���Ƿ���յ�����
	{
		// ��ȡ���յ������ݳ��Ⱥ���������
		ret = L01_ReadRXPayload(rx_buffer);

		L01_FlushRX();                          // ��λ����FIFOָ��
		L01_ClearIRQ(IRQ_ALL);                  // ����ж�
	}

//    }
    return ret;
}

void L01_RecvPacketTest()
{

	uint8_t length=0, recv_buffer[64]={0};

	uint8_t i=0;

    if (0 == L01_IRQ_READ())                    // �������ģ���Ƿ���������ж�
    {
        if (L01_ReadIRQSource() & (1<<RX_DR))   // �������ģ���Ƿ���յ�����
        {
            // ��ȡ���յ������ݳ��Ⱥ���������
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

        L01_FlushRX();                          // ��λ����FIFOָ��
        L01_ClearIRQ(IRQ_ALL);                  // ����ж�
    }
}


/*===========================================================================
* ���� ��L01_Chip_Init() => ��ʼ��RFоƬ                                       *
* ˵�� ��L01+�Ĳ������Ѿ�������C�⣬��nRF24L01.c�ļ��� �ṩSPI��CSN������	*
         ���ɵ������ڲ����к����û������ٹ���L01+�ļĴ����������⡣			*
============================================================================*/
void L01_Chip_Init(void)
{

	L01_Init();             // ��ʼ��L01�Ĵ���
	L01_SetTRMode(RX_MODE); // ����ģʽ
	L01_FlushRX();          // ��λ����FIFOָ��
    L01_FlushTX();          // ��λ����FIFOָ��
    L01_ClearIRQ(IRQ_ALL);  // ��������ж�
    L01_CE_HIGH();          // CE = 1, ��������
}


