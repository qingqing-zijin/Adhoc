/*
 * radio.c
 *
 *  Created on: May 4, 2018
 *      Author: root
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "radio.h"
#include "serial.h"
#include "gpio.h"
#include "aux_func.h"
#include "Congitive.h"
#include "nRF24L01.h"

extern int e43_aux_gpio_input_fd ;

extern int self_node_id ;
extern int radio_dat_mode;
//extern int e32_uart_fd;

//extern u8 	global_radio_on_receving ;

/*关于E32-无线模块的功能说明：
 * AUX引脚：AUX用于无线手法缓冲指示和自检指示
 * 它指示模块是否有数据尚未通过无线发射出去，或已经收到无线数据包是否尚未通过串口全部发出
 * 或模块正在初始化自检过程中。
 * 5.6.1 串口数据输出指示
 * 用于唤醒休眠中的外部MCU，模块串口外发数据时，AUX引脚
 * 可提前2～3ms输出低电平，用于唤醒外部MCU。所有收到的无线数据都已经
 * 通过TXD发出后，AUX输出高电平，即实质是RX缓冲区为空。
 *
 * 5.6.2 无线发射指示
 * 当AUX=1时，代表缓冲区最大512字节的数据都被写入到无线芯片（自动分包），也即最后一包数据已经写入RF芯片并启动发射。
 * 用户可以继续发出512字节的数据，不会溢出。
 * 注意：当AUX =1时，并不代表模块全部串口数据均通过无线发射完毕，也可能最后一包数据正在发射中。
 * 当AUX=0时，缓冲区不为空，此时模块有可能在等待用户数据结束超时，或内部最大512字节缓冲区的数据尚未全部写入到无线芯片并开启发射。
 * 此时正在进行无线分包发射。
 *
 * 注意5.6.2和5.6.3上述功能1&2,AUX输出低电平优先，即满足任何一个输出低电平条件，AUX就输出低电平。当所有
 * 条件均不满足时，AUX输出高电平。
 * 另外关于M0M1=00模式收发控制参考6.2。
 *
 * *
 * 5.6.3模块正在配置过程中
 * 仅在复位和退出休眠模式（配置模式M0M1=11）的时候，均会产生AUX=0,并且自检流程开始。
 * 自检完成后正常工作，AUX=1;
 * 注意：用户切换到新的模式后，至少需要在AUX上升沿2ms后，模块才会真正进入该模式。如果AUX
 * 一直处于高电平，那么模式切换将立即生效。此外，用户从模式3进入到其它模式或在复位过程中，
 * 模块会重新设置用于参数，期间AUX=0。
 *
 * *
 *  6.2 一般模式（模式0）
 *  当M0M1=00时，模块工作在模式0。
 *  *
 *
 *  发射时，模块接收来自串口的用户数据，模块发射无线数据包长度为58字节。当用户
 *  输入数据量达到58字节时，模块将启动无线发射;此时用户可以继续输入需要发射的数据。
 *  关于模块的自动分包策略：
 *  1》当用户需要传输的字节小于58字节时，模块等待3字节时间，若无用户数据继续输入，
 *  则认为数据终止，此时模块将所有数据经无线发出;
 *  2》当模块收到第一个用户数据后，将AUX输出低电平，当模块把所有数据都放到RF芯片后，且最后一包数据据
 *  启动发射后，AUX输出高电平。
 * *
 *
 *  接收时，模块一直打开无线接收功能，可以接收来自模式0、模式1发出的数据包。
 *  收到数据包后，模块AUX输出低电平，并延迟5ms后，开始将无线数据通过串口TXD引脚输出。所有无线
 *  数据都通过串口输出后，模块将AUX输出高电平。
 * *
 *
 * */
/*
 *1》 16进制格式发送C0+5BYTES ，掉电后参数保存
 * */
u8 cfgSet_cmd0[6] = { 0xC0, 0, };
/*
 *2》 16进制格式发送C1+C1+C1 ，查询已保存的参数	 * */
u8 Get_Params_Cmd[3] = { 0xC1, 0xC1, 0xC1 };

/*
 *3》 16进制格式发送C2+5BYTES ，掉电后参数丢失
 * */
u8 cfgSet_cmd1[6] = { 0xC2, 0, };

/*
 *4》在休眠模式下，C3 C3 C3指令查询当前的配置参数，返回比如C3 32 XX YY,
 *CHAN32表示433MHz工作频率。计算公式为：410MHz+ CHAN*1MHz
 * */
u8 version_cmd[3] = { 0xC3, 0xC3, 0xC3 };

/*
 * 5》16进制格式发送,模块将产生一次复位
 * */
u8 reset_cmd[3] = { 0xC4, 0XC4, 0XC4 };

int total_rving_bytes = -1;


#define APP_MALLOC_EN		0


#if APP_MALLOC_EN

#else

RADIO_E01_APP_t e01_app_pks[E01_RADIO_PKTS_MAX_SZ+1];
RADIO_E32_APP_t e32_app_pks[E32_RADIO_PKTS_MAX_SZ+1];

#endif


static int e01_radio_pks_sendto( u8* sendBytes, int send_len, unsigned short addrto)
{
	int pks = 0;
	int last_pk_sz = 0;
	int i=0;
	int offset =0;
	/*
	 *STEP1> 计算待发送的字节需要分几包发送
	 * */
	int len = send_len;

	/*
	 *判断待发送字节数是否为APP_PACK_PAY_LOAD_MAX的整数倍。若不是还得处理最后一包
	 * */
	last_pk_sz = len % E01_APP_PKT_PAY_LOAD_MAX;

	if(last_pk_sz !=0 )
	{
		//待发送的字节数不为PACK_LEN的整数倍
		pks = (len / E01_APP_PKT_PAY_LOAD_MAX) +1;
	}
	else
	{
		//待发送的字节数为PACK_LEN整数倍
		pks = len / E01_APP_PKT_PAY_LOAD_MAX ;
	}
	if(pks ==0 || sendBytes == NULL)
		return 0;

#if APP_MALLOC_EN
	RADIO_APP_t *e01_app_pks = malloc(sizeof(RADIO_APP_t)*pks);
	if(e01_app_pks == NULL)
	{
		LOG_MSG(ERR_LEVEL, "e01_radio_pks_sendto malloc error!\n");
		exit(-1);
	}
#endif

	memset((u8*)e01_app_pks,0, sizeof(RADIO_E01_APP_t)*pks);

	/*
	 * 将待发送的数据的，拷贝至对应的分组
	 * */
	for(i=0; i< (pks-1);i++)
	{
//		app_pks[i].head.pk_type = Others_PK;
		e01_app_pks[i].head.rcv_id  = addrto & 0xff;
		e01_app_pks[i].head.pay_total_bytes = len;

		e01_app_pks[i].head.pk_seq = i;
		e01_app_pks[i].head.pk_payload_len = E01_APP_PKT_PAY_LOAD_MAX;

		e01_app_pks[i].head.pk_head_crc = getCRC((u8*)&e01_app_pks[i], sizeof(RADIO_APP_Head)-2);

		//拷贝待发送字节至buffer中
		offset = i*E01_APP_PKT_PAY_LOAD_MAX;
		memcpy(e01_app_pks[i].data ,&sendBytes[offset], E01_APP_PKT_PAY_LOAD_MAX);
	}

	//处理最后一包
	e01_app_pks[i].head.rcv_id  = addrto & 0xff;
//	app_pks[i].head.pk_type = Others_PK;
	e01_app_pks[i].head.pay_total_bytes = len;
	e01_app_pks[i].head.pk_seq = i;

	//若last_pk_sz=0,说明最后一包为整包
	e01_app_pks[i].head.pk_payload_len = last_pk_sz > 0 ? last_pk_sz : E01_APP_PKT_PAY_LOAD_MAX;
	e01_app_pks[i].head.pk_head_crc = getCRC((u8*)&e01_app_pks[i], sizeof(RADIO_APP_Head)-2);
	offset = i*E01_APP_PKT_PAY_LOAD_MAX;
	memcpy(e01_app_pks[i].data ,&sendBytes[offset], e01_app_pks[i].head.pk_payload_len);

	i=0;
	//开始发送至无线模块
	for(i=0;i<(pks-1); i++)
	{
		L01_SendPacket((u8*)&e01_app_pks[i],E01_RADIO_PKT_MAX_BYTES);
	}
	//发送最后一包
	L01_SendPacket((u8*)&e01_app_pks[i],(sizeof(RADIO_APP_Head) + e01_app_pks[i].head.pk_payload_len) );
	LOG_MSG(DBG_LEVEL, "e01_radio_pks_sendto over!\n");
#if APP_MALLOC_EN
	free(e01_app_pks);
	e01_app_pks= NULL;
#endif

	return 0;
}

/*
 * @function:
 * 		e32_radio_pks_sendto
 * @desc:
 * 		E32电台发送函数
 *  E32发射时，模块接收来自串口的用户数据，模块发射无线数据包长度为58字节。关于模块的自动分包策略：
 *  1》当用户需要传输的字节小于58字节时，模块等待3字节时间，若无用户数据继续输入，则认为数据终止，此时模块将所有数据经无线发出;
 *  2》当用户输入数据量达到58字节时，模块将启动无线发射;此时用户可以继续输入需要发射的数据。
 *  3》此外当模块收到第一个用户数据后，将AUX输出低电平，当模块把所有数据都放到RF芯片后，且最后一包数据据启动发射后，AUX输出高电平。
 *
 * @param sendBytes:
 * 		发送字节数组
 * @param send_len:
 * 		发送字节数组长度；最大限制650字节.
 * @param addrto
 * 		发送目的地址
 * @return:
 * 		int, 0 成功。
 *
 * */
static int e32_radio_pks_sendto(int uart_fd, int aux_fd, u8* sendBytes, int send_len, unsigned short addrto)
{
	int pks = 0;
	int last_pk_sz = 0;
	int i=0;
	int offset =0;
	int last_send_sz =0;
	int len =  send_len;
	/*
	 *STEP1> 计算待发送的字节需要分几包发送
	 *判断待发送字节数是否为APP_PACK_PAY_LOAD_MAX的整数倍。若不是还得处理最后一包
	 * */
	last_pk_sz = len % E32_APP_PKT_PAY_LOAD_MAX;

	if(last_pk_sz !=0 )
	{
		//待发送的字节数不为PACK_LEN的整数倍
		pks = (len /E32_APP_PKT_PAY_LOAD_MAX) +1;
	}
	else
	{
		//待发送的字节数为PACK_LEN整数倍
		pks = len / E32_APP_PKT_PAY_LOAD_MAX ;
	}


	if(pks ==0)
		return 0;
//	LOG_MSG(ERR_LEVEL, "last_pk_sz=%d,pks=%d \n", last_pk_sz, pks);

#if APP_MALLOC_EN
	RADIO_APP_t* e32_app_pks= (RADIO_APP_t*)malloc(sizeof(RADIO_APP_t)*pks);

	if(e32_app_pks == NULL)
	{
		LOG_MSG(ERR_LEVEL, "e32_radio_pks_sendto malloc error!\n");
		exit(-1);
	}
#endif

	memset((u8*)e32_app_pks,0, sizeof(RADIO_E32_APP_t)*pks);

	/*
	 * 将待发送的数据的，拷贝至对应的分组
	 * */
	i=0;
	for(i=0; i< (pks-1);i++)
	{
//		app_pks[i].head.pk_type = Others_PK;
		e32_app_pks[i].head.rcv_id  = addrto & 0xff;
		e32_app_pks[i].head.pay_total_bytes = len;

		e32_app_pks[i].head.pk_seq = i;
		e32_app_pks[i].head.pk_payload_len = E32_APP_PKT_PAY_LOAD_MAX;

		e32_app_pks[i].head.pk_head_crc = getCRC((u8*)&e32_app_pks[i], sizeof(RADIO_APP_Head)-2);

		//拷贝待发送字节至buffer中
		offset = i*E32_APP_PKT_PAY_LOAD_MAX;
		memcpy(e32_app_pks[i].data ,&sendBytes[offset], E32_APP_PKT_PAY_LOAD_MAX);
	}

	//处理最后一包
	e32_app_pks[i].head.rcv_id  = addrto & 0xff;
//	app_pks[i].head.pk_type = Others_PK;
	e32_app_pks[i].head.pay_total_bytes = len;
	e32_app_pks[i].head.pk_seq = i;

	e32_app_pks[i].head.pk_payload_len = last_pk_sz > 0 ? last_pk_sz : E32_APP_PKT_PAY_LOAD_MAX;
//	e32_app_pks[i].head.pk_payload_len = E32_APP_PKT_PAY_LOAD_MAX;
	e32_app_pks[i].head.pk_head_crc = getCRC((u8*)&e32_app_pks[i], sizeof(RADIO_APP_Head)-2);
	offset = i*E32_APP_PKT_PAY_LOAD_MAX;

	memcpy(e32_app_pks[i].data ,&sendBytes[offset], e32_app_pks[i].head.pk_payload_len);



	/*
	 * 2018-07-23
	 * E32-DTU-5W自动分包发送规则：
	 * 	1> 发射长度：缓存512字节，内部自动分包58字节发送;
	 * 	   如果选择透明传输，则发送buffer可达512字节
	 *  2> 接收长度：缓存512字节，内部自动分包58字节发送
	 *
	 * 针对无线e32模块内部自动分包58字节发送原则，考虑最后一组分组：
	 * 1> 若发送包长小于58字节，不够58字节的在末尾填充字节0， 然后按58字节发送，
	 * 从而确保每个一小包控制帧（MAC_REQ/REP/OK/OK_AC）能及时发送，每个大包的最后小包也能独立发送出去。
	 * 此策略需要radio_pkts_recvfrom()根据pay_total_bytes,区分是否接收完成。
	 * */
	if(radio_dat_mode == DTU_MOD)
	{
		/*
		if(last_pk_sz == 0)
		{
			last_send_sz = E32_RADIO_PKT_MAX_BYTES* pks;
		}
		else
		{
			last_send_sz = E32_RADIO_PKT_MAX_BYTES* (pks-1) +sizeof(RADIO_APP_Head)+last_pk_sz ;
		}*/
		last_send_sz = E32_RADIO_PKT_MAX_BYTES* pks;		//最后一个分组，也按58字节发送

		//DTU模式，将所有分组一起发送至无线模块
		uartsendto(uart_fd, (u8*)e32_app_pks, last_send_sz);

		/*
		 * 1>当E32模块把所有数据都放到RF芯片后，且最后一包数据据启动发射后，AUX输出高电平。
		 * 下面函数gpio_edge_poll阻塞等待AUX上升沿到来后，然后返回.
		 * 2> Poll函数参数timeout =-1时, 表示阻塞等待TX sent事件发生
		 * */
		gpio_edge_poll(aux_fd, TX_AUX, -1); //0x1ffff);
		LOG_MSG(DBG_LEVEL, "e32_radio_pks_sendto over\n");
//		usleep(10000);	//等待10ms
//		LOG_MSG(ERR_LEVEL, "e32_radio_pks_sendto over, sizeof(RADIO_APP_Head)=%d last_send_sz=%d!\n", sizeof(RADIO_APP_Head),last_send_sz);

//		if(sendBytes[0]==0x22)
//		{
//			print_buffer((u8*)e32_app_pks, last_send_sz);
//		}
	}
	else
	{
		/*
		 * 定点传输模式；
		 * 由于芯片缺陷，定点传输模式下只有第一包带有地址信息，其它包不含地址信息。
		 * 因此工程不支持定点传输模式。
		 * */
	}

#if APP_MALLOC_EN
	free(e32_app_pks);
	e32_app_pks =NULL;
#endif

	return 0;

}

/*
 * @function:
 * 		radio_pks_sendto
 * @desc:
 * 		E32电台发送函数
 *  E32发射时，模块接收来自串口的用户数据，模块发射无线数据包长度为58字节。关于模块的自动分包策略：
 *  1》当用户需要传输的字节小于58字节时，模块等待3字节时间，若无用户数据继续输入，则认为数据终止，此时模块将所有数据经无线发出;
 *  2》当用户输入数据量达到58字节时，模块将启动无线发射;此时用户可以继续输入需要发射的数据。
 *  3》此外当模块收到第一个用户数据后，将AUX输出低电平，当模块把所有数据都放到RF芯片后，且最后一包数据据启动发射后，AUX输出高电平。
 *
 * @param sendBytes:
 * 		发送字节数组
 * @param send_len:
 * 		发送字节数组长度；最大限制650字节.
 * @param addrto
 * 		发送目的地址
 * @return:
 * 		int, 0 成功。
 *
 * */
int radio_pks_sendto(int fd, u8* sendBytes, int send_len, unsigned short addrto)
{
	int len = send_len > 512 ? 512: send_len;

	if(g_UPG_on == TRUE)
	{
		return 0;
	}

	switch(g_radio_speed)
	{
		case SPD_E32_4K8:
		case SPD_E32_9K6:
		case SPD_E32_19K2:
		case SPD_E32_FLAG:
		{
			e32_radio_pks_sendto(fd,M_e32_aux_gpio_input_fd, sendBytes, len, addrto);
			break;
		}
		case SPD_E01_250K:
		case SPD_E01_1M:
		case SPD_E01_2M:
		case SPD_E01_FLAG:
		{
			e01_radio_pks_sendto(sendBytes, len, addrto);
			break;
		}
		default:
			break;
	}
	return 0;
}

void print_buffer(u8* buf, int len)
{
	int i=0;
	for(i=0; i<len;i++)
	{
		printf("[%d]=%02x ",i, buf[i]);
		if((i+1)%5==0)
		{
			printf("\n");
		}
	}
	printf("\n");
}

extern int L01_IRQ_fd;

static int e01_radio_pks_recvfrom(u8* buffer, int len)
{
	int rv_sz = 0;

	u8  pk_seq=0;
	u8  pk_payload_len=0;

	u8 rcv_id;

	RADIO_E01_APP_t e01_app_data;

	memset((u8*)&e01_app_data ,0, sizeof(e01_app_data));

	int rv_total_sz = 0;		/*已接收多少字节*/

	total_rving_bytes =0;		/*剩余接收多少字节待接收*/

	u8 rv_pk_head = FALSE;

	//u8 tempBuff[58]={0,};

	//接收新的数据包到来
	do{

		gpio_edge_poll(L01_IRQ_fd, RX_AUX,0x1ff);//0x1ffff); //0x1ff);		//timeout =0，表示阻塞方式

		rv_sz = L01_RecvPacket((u8*) &e01_app_data, E01_RADIO_PKT_MAX_BYTES);

		if(rv_sz <=0)
		{
			//接收数据等待超时或错误发生
			continue;
		}

		/*
		 * 不管接收到的数据包目的地址是否为本节点，只要接收到数据包，则认为
		 * 信道正在被其它节点访问。。。
		 * 因为子网内所有节点访问信道方式是同频、且以广播的方式发送至无线空口
		 * */
//		global_radio_on_receving = TRUE;

		if(getCRC((u8*)&e01_app_data, sizeof(RADIO_APP_Head)) !=0)
		{
			LOG_MSG(ERR_LEVEL, "e01_radio_pks_recvfrom rcv_pk_crc error!, rv_sz=%d\n", rv_sz);
			total_rving_bytes =0;
			rv_total_sz =0;
			rv_pk_head = FALSE;
			continue;
		}
		//判断目的地址是否为广播地址或本节点的id
		rcv_id = e01_app_data.head.rcv_id ;
		pk_seq = e01_app_data.head.pk_seq ;

		pk_payload_len =e01_app_data.head.pk_payload_len;

		if(rcv_id != 255 && rcv_id!= self_node_id)
		{
//			printf("--------------------------------------rcv_pk dest_id=%d ,discard!\n", rcv_id);
			continue;
		}
		else
		{
			if(pk_seq ==0)
			{
				//接收到第一包
				total_rving_bytes = e01_app_data.head.pay_total_bytes;
				memcpy(buffer, e01_app_data.data, pk_payload_len);
				//printf("rcv_pack[%d], total_bytes = %d, rcv_pk_sz =%d\n",pk_seq, total_rving_bytes, pk_payload_len);
				//重新计算待接收的字节和已收的字节数
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				rv_pk_head = TRUE;
			}
			else
			{
				//将非第一包数据拷贝至相应的位置
				memcpy(&buffer[rv_total_sz], e01_app_data.data, pk_payload_len);

				//重新计算待接收的字节
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				//printf("rcv_pack[%d], rv_total_sz = %d, pk_rv_sz=%d, total_rving_bytes=%d \n",pk_seq ,rv_total_sz, pk_payload_len ,total_rving_bytes);
			}
		}
	}while(total_rving_bytes >0);

//	global_radio_on_receving = FALSE;

	if(rv_pk_head == TRUE)
	{
		return rv_total_sz;
	}
	else
	{
		return 0;
	}
}


static int e32_radio_pks_recvfrom(int uart_fd, int aux_fd,u8* buffer, int len)
{
	int rv_sz = 0;
	u8  pk_seq=0;
	u8  pk_payload_len=0;
	u8 rcv_id;

	RADIO_E32_APP_t app_data;
	int rv_total_sz = 0;		/*已接收多少字节*/

	memset((u8*)&app_data ,0, sizeof(app_data));
	total_rving_bytes =0;		/*剩余接收多少字节待接收*/

	u8 rv_pk_head = FALSE;

	/*
	 * 分组数据包重组:
	 * 下面的功能是从无线接收每一包RADIO_APP_PKT, 并根据RADIO_APP_HEAD数据，
	 * 提取有效用户pkt_payload数据。
	 * 同时，对接收到的每一包pkt_payload数据，拷贝至buffer对应的位置。
	 * */
	do{
//		gpio_edge_poll(e32_aux_gpio_input_fd, RX_AUX,0);
		/*
		 * 设置超时时间(0x1ff ms)，等待RX事件发生
		 * */
		gpio_edge_poll(aux_fd, RX_AUX,-1);//0x1ff);//0x1ffff);

		rv_sz = uartrecvfrom(uart_fd,(u8*) &app_data, E32_RADIO_PKT_MAX_BYTES);
		if(rv_sz <=0)
		{
//			total_rving_bytes =0;
//			rv_total_sz =0;
//			rv_pk_head = false;
			//接收数据等待超时或错误发生
			continue;
		}

		/*
		 * 不管接收到的数据包目的地址是否为本节点，只要接收到数据包，则认为
		 * 信道正在被其它节点访问。。。
		 * 因为子网内所有节点访问信道方式是同频、且以广播的方式发送至无线空口
		 * */
//		global_radio_on_receving = TRUE;

		if(getCRC((u8*)&app_data, sizeof(RADIO_APP_Head)) !=0)
		{
			LOG_MSG(ERR_LEVEL, "e32_radio_pks_recvfrom rcv_pk_crc error, rv_sz=%d!\n", rv_sz);

			print_buffer((u8*)&(app_data),  rv_sz);

			total_rving_bytes =0;
			rv_total_sz =0;
			rv_pk_head = FALSE;
			continue;
		}
//		LOG_MSG(ERR_LEVEL, "e32_radio_pks_recvfrom rcvd_pk, rv_sz=%d!\n", rv_sz);

		//判断目的地址是否为广播地址或本节点的id
		rcv_id = app_data.head.rcv_id ;
		pk_seq = app_data.head.pk_seq ;
		pk_payload_len =app_data.head.pk_payload_len;

		if(rcv_id != 255 && rcv_id!= self_node_id)
		{
//			printf("--------------------------------------rcv_pk dest_id=%d ,discard!\n", rcv_id);
			continue;
		}
		else
		{

			/*2018-06-22加入包（packets）监听机制
			 * 由于E32无线电一次发送的报文长度限制，用户终端发送的数据被拆分成为多个子包发送
			 * 因此如果正在接收子包，但发生线程切换，尤其是uartSender定时器超时发生，此时
			 * uartSender定时器在定时器回调函数中，应该采用“随机退避策略”，让出信道，等待接收完毕，在发送。
			 * */
			if(pk_seq ==0)
			{
				/*
				 * 接收到第一个分组包，解析获得完整报文长度pay_total_bytes字段，
				 * 并根据该字段计算何时接收完毕一整包报文，
				 * 然后返回处理。
				 * */
				total_rving_bytes = app_data.head.pay_total_bytes;

				memcpy(buffer, app_data.data, pk_payload_len);
				//printf("rcv_pack[%d], total_bytes = %d, rcv_pk_sz =%d\n",pk_seq, total_rving_bytes, pk_payload_len);
				//重新计算待接收的字节和已收的字节数
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				rv_pk_head = TRUE;
			}
			else
			{
				//将非第一包数据拷贝至相应的位置
				memcpy(&buffer[rv_total_sz], app_data.data, pk_payload_len);

				//重新计算待接收的字节
				total_rving_bytes -= pk_payload_len;
				rv_total_sz += pk_payload_len;
				//printf("rcv_pack[%d], rv_total_sz = %d, pk_rv_sz=%d, total_rving_bytes=%d \n",pk_seq ,rv_total_sz, pk_payload_len ,total_rving_bytes);
			}
		}
	}while(total_rving_bytes >0);

//	global_radio_on_receving = FALSE;

	if(rv_pk_head == TRUE)
	{
		return rv_total_sz;
	}
	else
	{
		return 0;
	}
}

/*
 * @function radio_recvfrom
 *
 * @desc 从无线模块接收空口数据
 *  接收时，模块一直打开无线接收功能，可以接收来自另端无线模块模式0、模式1发出的数据包。
 *  收到数据包后，模块AUX输出低电平，并延迟5ms后，开始将无线数据通过串口TXD引脚输出。所有无线
 *  数据都通过串口输出后，模块将AUX输出高电平。
 *
 * @return :返回从无线接收到的完整的pkt_payload, 保存至buffer中，
 * 		返回值为接收到pkt_payload字节数
 * */
int radio_pks_recvfrom(int fd, int aux_fd, u8* buffer, int len)
{
	int ret=0;
	if(g_UPG_on == TRUE)
	{
		return 0;
	}

	switch(g_radio_speed)
	{
		case SPD_E32_4K8:
		case SPD_E32_9K6:
		case SPD_E32_19K2:
		case SPD_E32_FLAG:
		{
			ret = e32_radio_pks_recvfrom(fd, aux_fd, buffer, len);
			break;
		}
		case SPD_E01_250K:
		case SPD_E01_1M:
		case SPD_E01_2M:
		case SPD_E01_FLAG:
		{
			ret = e01_radio_pks_recvfrom(buffer, len);
			break;
		}
		default:
			break;
	}
	return ret;
}

int L_E32_pks_sendto(int uart_fd,int aux_fd, u8* sendBytes, int send_len, unsigned short addrto)
{
	e32_radio_pks_sendto(uart_fd,aux_fd, sendBytes, send_len, addrto);
	return 0;
}

int L_E32_pks_recvfrom(int fd, int aux_fd, u8* buffer, int len)
{
	int ret= e32_radio_pks_recvfrom(fd, aux_fd, buffer, len);
	return ret;
}

/*
 *初始化E32\E43无线模块， 配置连接无线模块AUX的MCU引脚为输入方向, 并使能上升沿中断
 * */
int radios_gpio_init()
{
	/********************************************************
	 *				中速E32模块GPIO初始化
	 *******************************************************/
	gpio_export(M_E32_AUX_GPIO_PIN_NUM);
	gpio_direction(M_E32_AUX_GPIO_PIN_NUM, GPIO_IN);
	//使能上升沿中断
	gpio_edge(M_E32_AUX_GPIO_PIN_NUM,RISING);
	//打开AUX 对应/sys/class/gpio/gpiox/value文件，实时获取AUX值
	M_e32_aux_gpio_input_fd = open_gpio_rw_fd(M_E32_AUX_GPIO_PIN_NUM);

	if(M_e32_aux_gpio_input_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open_gpio_rw_fd M_e32_aux_gpio_input_fd value!\n");
		return -1;
	}

	gpio_export(M_E32_M0_GPIO_PIN_NUM);
	gpio_direction(M_E32_M0_GPIO_PIN_NUM, GPIO_OUT);



	/********************************************************
	 *				低速E32模块GPIO初始化
	 *******************************************************/
	gpio_export(L_E32_AUX_GPIO_PIN_NUM);
	gpio_direction(L_E32_AUX_GPIO_PIN_NUM, GPIO_IN);
	//使能上升沿中断
	gpio_edge(L_E32_AUX_GPIO_PIN_NUM,RISING);
	//打开AUX 对应/sys/class/gpio/gpiox/value文件，实时获取AUX值
	L_e32_aux_gpio_input_fd = open_gpio_rw_fd(L_E32_AUX_GPIO_PIN_NUM);

	if(L_e32_aux_gpio_input_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open_gpio_rw_fd L_e32_aux_gpio_input_fd value!\n");
		return -1;
	}

	gpio_export(L_E32_M0_GPIO_PIN_NUM);
	gpio_direction(L_E32_M0_GPIO_PIN_NUM, GPIO_OUT);

	/********************************************************
	 *				E43模块GPIO初始化
	 *******************************************************/
	if(global_e43_existed == FALSE)
	{
		return 0;		//如果E43模块不存在，则初始化完成返回
	}
	/*
	 *STEP2> 配置连接无线感知模块MCU AUX引脚为输入方向, 并使能上升沿中断
	 * */
	if(e43_aux_gpio_input_fd >0)
		close(e43_aux_gpio_input_fd);

	gpio_export(E43_AUX_GPIO_PIN_NUM);
	gpio_direction(E43_AUX_GPIO_PIN_NUM, GPIO_IN);
	//使能上升沿中断
	gpio_edge(E43_AUX_GPIO_PIN_NUM,RISING);
	//打开AUX 对应/sys/class/gpio/gpiox/value文件，实时获取AUX值
	e43_aux_gpio_input_fd = open_gpio_rw_fd(E43_AUX_GPIO_PIN_NUM);

	if(e43_aux_gpio_input_fd < 0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to open value!\n");
		return -1;
	}

	gpio_export(E43_M0_GPIO_PIN_NUM);
	gpio_export(E43_M1_GPIO_PIN_NUM);

	gpio_direction(E43_M0_GPIO_PIN_NUM, GPIO_OUT);
	gpio_direction(E43_M1_GPIO_PIN_NUM, GPIO_OUT);

	return 0;
}

/*
 *
 *配置无线模块工作模式
 * */
int radio_cfg_mode(RADIO_MOD_t modType, RADIO_DEV_t dev_t)
{
	/*
	 * 模块进入休眠模式模式，M0M1=11
	 * */
	int rv =0;


	if(dev_t==E32_M)
	{
		switch(modType)
		{
			case DTU_MOD:
			case ADDR_MOD:
				gpio_write(M_E32_M0_GPIO_PIN_NUM, GPIO_L);
				break;

			case STANDBY_MOD:
				gpio_write(M_E32_M0_GPIO_PIN_NUM, GPIO_H);
				break;

			default:
				rv =-1;
				break;
		}

	}
	else if(dev_t==E32_L)
	{
		switch(modType)
		{
			case DTU_MOD:
			case ADDR_MOD:
				gpio_write(L_E32_M0_GPIO_PIN_NUM, GPIO_L);
				break;
			case STANDBY_MOD:
				gpio_write(L_E32_M0_GPIO_PIN_NUM, GPIO_H);
				break;

			default:
				rv =-1;
				break;
		}

	}
	else if(dev_t == E43)
	{

		switch(modType)
		{
				//E43模块，设置模式M1=1, M0=0
			case STANDBY_MOD:
				gpio_write(E43_M0_GPIO_PIN_NUM, GPIO_L);
				gpio_write(E43_M1_GPIO_PIN_NUM, GPIO_H);
				break;
			case RSSI_MOD:
				//RSSI模式，M1=0, M0=1
				gpio_write(E43_M0_GPIO_PIN_NUM, GPIO_H);
				gpio_write(E43_M1_GPIO_PIN_NUM, GPIO_L);
				break;

			default:
			{
				rv =-1;
				break;
			}
		}
	}
	return rv;
}

/*
 * M_E32_set_radio_params
 * 配置无线电模块串口速率、工作模式（DTU/定点传输）、IO模式、FEC打开/关闭以及发射功率大小
 * */
int M_E32_set_radio_params(Radio_CHL_t ch_i, u8 air_link_speed)
{
	g_UPG_on = TRUE;

	/*
	 * 模式切换
	 * 在模式0（一般模式）或模式1（唤醒模式），用户连续输入大量数据，同时
	 * 进行模式切换，此时的切换模式操作是无效的;
	 * 模块会将所有用户数据处理完毕后，才
	 * 进行新的模式检测，所以一般建议为：
	 * 检测AUX引脚输出状态，等待AUX输出高电平后2ms再进行切换。
	 * */
	radio_cfg_mode(STANDBY_MOD,E32_M);
	gpio_edge_poll(M_e32_aux_gpio_input_fd, CFG_AUX, -1);//0x1ffff);

	usleep(2000);

	if(M_e32_uart_fd>0)
	{
		printf("M_e32_uart_fd=%d, L_e32_uart_fd=%d\n", M_e32_uart_fd,L_e32_uart_fd);
		close(M_e32_uart_fd);
	}

	M_e32_uart_fd = Uart_Open(COM1, 9600, 8, 'N', 1, 1);
	if(M_e32_uart_fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM1 for configure Radio!\n");
		return -1;
	}

	radio_cfg_params(
					E32_M,						//或E32_L,该参数取E32_M/L一样，通过uart_fd区分配哪个电台
					C0_FLASH_PARAM,
					M_e32_uart_fd,
					UART_SPEEDTYPE_115200,
					UART_MOD_8N1,
					air_link_speed,
					DTU_MOD_ON,					//注意透明传输时，设置为DTU_MOD_ON
					TXD_AUX_OC_RX_PULLUP,		//6,5,4,3,2位建议写0
					FEC_OFF,
					POWER_30dBm_ON,
					(ch_i > CH_MAX_VAL ? CH_MAX_VAL : ch_i),
					0xFFFF );


	/*
	 * STEP6> 推出休眠模式，配置无线模块进入DTU模式.
	 * 注意：6.5.当从休眠（配置）模式进入其它模式，模块会重新配置参数，配置过程中，
	 * AUX保持低电平，完毕后输出高电平，所以建议用户检测AUX上升沿。
	 ** */
	radio_cfg_mode(DTU_MOD,E32_M);

	/*
	 * 设置超时时间(0x1ffff ms)，等待模式切换AUX事件发生
	 * */
	gpio_edge_poll(M_e32_aux_gpio_input_fd, CFG_AUX, -1);//0x1ffff);

	usleep(2000);	/**/

	close(M_e32_uart_fd);

	LOG_MSG(INFO_LEVEL,"--------------------------------------E32 radio cfg param over\n");

	/*
	 * STEP7> 最后以配置的波特率打开MCU串口，进行用户数据透明传输
	 * */
	M_e32_uart_fd = Uart_Open(COM1, 115200, 8, 'N', 1,1);
	if(M_e32_uart_fd <=-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM1 for configure Radio!\n");
		exit (-1);
	}
	g_UPG_on = FALSE;

	printf("LAST, M_e32_uart_fd=%d, L_e32_uart_fd=%d\n", M_e32_uart_fd,L_e32_uart_fd);

	return TRUE;
}

/*
 * radio_params_cfg
 * 配置无线电模块串口速率、工作模式（DTU/定点传输）、IO模式、FEC打开/关闭以及发射功率大小
 * */
int radio_cfg_params(RADIO_DEV_t dev_t,HEAD_TYPE_t head_type,  int fd, int uartSpeed, int uart_mod, int link_speed,
		int dtu_mod_on, int io_mod, int fec_on, int pwr_type, Radio_CHL_t channl, unsigned short addr)
{
	u8 cfg_params[7] = { 0, };
	u8 getted_params[20] = { 0, };
	int recv_sz = 0;
	int cfg_times = 20;
	int cfg_state = 0;

	Radio_CHL_t temp_ch = 0;
	temp_ch = (dev_t ==E43) ? (channl*4) : ((dev_t ==E32_L||dev_t ==E32_M) ? channl+15: 0);

	do{
		cfg_params[0] = head_type; //掉电后，所有参数将保存

		cfg_params[1] = (addr>>8) &0xff; //模块地址-高字节
		cfg_params[2] = addr & 0xff; //模块地址-低字节

		//SPED-字节byte3; 配置无线模块UART 115200bps, 8N1模式;空口速率为19.2kbps
		cfg_params[3] &=~(UART_SPEEDTYPE_Msk | UART_MOD_8N1_Msk | AIR_SPEEDTYPE_Msk);
		cfg_params[3] |= (uartSpeed) | (uart_mod) | (link_speed);

		//根据设备类型，并根据radio.h中定义的统一信道计算公式计算信道id

		cfg_params[4] = temp_ch;

		//OPTION字节，配置无线模块：DTU透传模式、TX_AUX开路输出、RX上拉输入;FEC打开;功率选择30dBm输出
		cfg_params[5] &= ~(DTU_MOD_Msk | IO_MOD_Msk | FEC_ON_Msk | TX_POWER__Msk);
		cfg_params[5] |= (dtu_mod_on) | (io_mod) | (fec_on) | (pwr_type);


		//发送配置命令配置，返回的结果比如C0 00 00 1A 17 44
		uartsendto(fd, cfg_params, sizeof(cfg_params)-1);

		//usleep(200);

//		uartsendto(fd, Get_Params_Cmd, 3);
		memset(getted_params,0, 6);
//		printf("----------------uartrecvfrom parameters from chip %s ...\n", dev_t==E43 ?"E43": "E32");
		recv_sz = uartrecvfrom(fd, getted_params, 6);
		if(strcmp(cfg_params, getted_params) !=0)
		{
			//配置不成功
			printf("\n----------------------------------radio cfgParams failed:\n cfg_bytes[%d] :%02x %02x %02x %02x %02x %02x\n get_bytes[%d] :%02x %02x %02x %02x %02x %02x\n",
				 6, cfg_params[0], cfg_params[1], cfg_params[2], cfg_params[3],	cfg_params[4], cfg_params[5],
				 recv_sz,
				 getted_params[0], getted_params[1], getted_params[2], getted_params[3], getted_params[4], getted_params[5]
			);

			printf("\----------------------------------end\n");

			//exit(-1);
			cfg_state = FALSE;

		}
		else
		{
//			//配置成功
//			printf("\----------------------------------radio cfgParams ok:\n cfg_bytes[%d] :%02x %02x %02x %02x %02x %02x\n get_bytes[%d] :%02x %02x %02x %02x %02x %02x\n",
//				  6, cfg_params[0], cfg_params[1], cfg_params[2], cfg_params[3],	cfg_params[4], cfg_params[5],
//				 recv_sz,
//				 getted_params[0], getted_params[1], getted_params[2], getted_params[3], getted_params[4], getted_params[5]
//			);
			LOG_MSG(INFO_LEVEL, "--------------------------------------radio_cfg_params ok\n");

			cfg_state = TRUE;
			break;
		}
	}while(cfg_times-- >0);

	if(cfg_state == FALSE)
	{
		close(fd);
		printf("----------------%s Config Failed Times(20), ------------------\n",dev_t==E43? "E43": (dev_t==E32_L?"E32_L": "E32_M"));
		return -1;
//		exit(-1);
	}


	return 0;
}



/*
 *@function:
 *	 config_E43_radio_rssi_mode_test
 *@brief:
 *  	配置E43 RSSI无线模块，配置其串口通信速率以及RSSI工作模式，监测本地频谱环境
 *@Note:
 *		仅用于测试模式
 * */
int g_e43_uart_fd =-1;

int config_E43_radio_rssi_mode_test()
{
	int sz =-1;
	u8 rv_buff[20]={0,};

	int test_cnt = CONG_MAX_TIMES_PER_CHNL;

	/*
	 *STEP2> 配置无线模块进入休眠模式M0M1 =11，用于模块参数设置,包括设置频率信道、通信串口速率
	 * */
	radio_cfg_mode(STANDBY_MOD,E43);

	/*
	 *STEP3> 打开MCU串口,配置串口波特率为9600、8N1模式，与无线模块进行通信。
	 * */
	g_e43_uart_fd = Uart_Open(COM2, 9600, 8, 'N', 1, 0);
	if(g_e43_uart_fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
		exit(-1) ;
	}
	/*
	 *STEP4> 休眠模式下，发送执行查询版本命令,验证通信是否正常
	 * */
//	memset(rv_buff, 0,20);
//	uartsendto(e43_uart_fd, version_cmd, 3);
//
//	//等待ACK
//	//sleep(1);	//80ms
//
//	sz = uartrecvfrom(e43_uart_fd, rv_buff, 4);
//	if(sz >0)rv_buff[sz >0] = 0;
//	//C3 32 31 14，其中32代表无线电模块频段为433MHz, 31是其软件版本
//	LOG_MSG(DBG_LEVEL,"version bytes %d, bytes: %02x %02x %02x %02x\n", sz ,rv_buff[0], rv_buff[1], rv_buff[2], rv_buff[3]);

	//
	/*
	 * STEP5> 休眠模式下，对无线模块配置参数如下：
	 * （1）波特率配置为115200、8N1模式
	 * （2）空口速率配置为19.2k
	 * （3）透明传输模式 或定点传输模式（后者支持广播地址）
	 * （4）FEC_ON模式
	 * （5）发射功率最大
	 *  (6) 信道为CH20
	 *  (7) 模块地址（节点id）
	 * */
	radio_dat_mode = DTU_MOD;		//通信模式：透传或点对点模式
	radio_cfg_params(E43,C2_RAM_PARAM,
					g_e43_uart_fd,
					UART_SPEEDTYPE_9600,
					UART_MOD_8N1,
					AIR_SPEEDTYPE_300,	//针对E43,为1.2Kbps
					(radio_dat_mode == DTU_MOD? DTU_MOD_ON:ADDR_MOD_ON),			//注意透明传输时，设置为DTU_MOD_ON
					TXD_AUX_OPEN_RX_OPEN,		//6,5,4,3,2位建议写0
					FEC_OFF,
					POWER_30dBm_ON,
					CHANL5,		//之前测试时，CH20对应430MHz，新的计算公式下，425+ CHANL*1MHz
					(radio_dat_mode == DTU_MOD? 0xFFFF :self_node_id));

	/*
	 * STEP6> 退出休眠模式，配置无线模块进入RSSI模式.
	 * 注意：6.5.当从休眠（配置）模式进入其它模式，模块会重新配置参数，配置过程中，
	 * AUX保持低电平，完毕后输出高电平，所以建议用户检测AUX上升沿。
	 *
	 * RSSI模式下，不可进行无线发射，收到的串口数据将被丢弃;同样不可接收
	 * 空中无线数据，仅扫描当前信道的信号强度，每100ms通过串口输出一个强度值（相对值）
	 ** */
	radio_cfg_mode(RSSI_MOD,E43);

	/*
	 * 设置超时时间(0x1ffff ms)，等待模式切换AUX事件发生
	 * */
	gpio_edge_poll(e43_aux_gpio_input_fd, CFG_AUX, 0x1ffff);


//	usleep(2000);
//	close(e43_uart_fd);

	/*
	 * 接下来以配置的波特率打开MCU串口，获取RSSI输出值
	 * */
//	e43_uart_fd = Uart_Open(COM2, 115200, 8, 'N', 1,1);
//	if(e43_uart_fd ==-1)
//	{
//		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
//		exit (-1);
//	}
	memset(rv_buff, 0,4);

	/*
	 * 读取RSSI值 test_cnt次数，打印输出
	 * */
	while(test_cnt--)
	{
		sz = uartrecvfrom(g_e43_uart_fd, rv_buff, 4);
		if(sz >0)
			rv_buff[sz] = 0;

		LOG_MSG(DBG_LEVEL,"-------------------------RSSI bytes %d, bytes: %d %02x %02x %02x\n", sz ,rv_buff[0], rv_buff[1], rv_buff[2], rv_buff[3]);

	}


	close(g_e43_uart_fd);
	g_e43_uart_fd =-1;
	return 0;

}

//

/*@funtion:
 * 		 E43_channl_rssi_congitive
 *@brief:
 *		 感知本地频谱环境，获取信道ch_i当前空口RSSI值
 *@param ch_i：
 *		信道的id
 *@return
 *		返回用于用户获取RSSI值的串口句柄
 * */
//int g_e43_uart_fd =-1;
int E43_channl_rssi_congitive(Radio_CHL_t ch_i)
{

//	static u8 config_aux_once = 0;

	int ret=0;
	/*
	 *STEP2> 配置无线模块进入休眠（设置）模式M0M1 =11，用于模块参数设置,包括设置频率信道、通信串口速率
	 * */
	do{


	radio_cfg_mode(STANDBY_MOD,E43);

	/*
	 *STEP3> 设置模式下，打开主机MCU串口,配置串口波特率为9600、8N1模式，与无线模块进行通信。
	 * */
	if(g_e43_uart_fd == -1 || ret ==-1 )
	{
		g_e43_uart_fd = Uart_Open(COM2, 9600, 8, 'N', 1,0);
		if(g_e43_uart_fd ==-1)
		{
			LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
			return -1;
		}

	}

	/*
	 * 利用select函数来实现串口的非阻塞（超时）读模式
	 * */
//	LOG_MSG(DBG_LEVEL,"-------------------------------------congitive ch_i =%d\n",  ch_i);

	radio_dat_mode = DTU_MOD;		//通信模式：透传或点对点模式
	//
	/*
	 * STEP4> 对无线模块配置参数如下：
	 * （1）波特率配置为115200、8N1模式
	 * （2）空口速率配置为19.2k
	 * （3）透明传输模式 或定点传输模式（后者支持广播地址）
	 * （4）FEC_ON模式
	 * （5）发射功率最大
	 *  (6) 信道为ch_i
	 *  (7) 模块地址（节点id或广播地址）
	 *  在radio_cfg_params函数中，通过判断返回的参数与配置的参数是否一致，不一致则会多次配置，直到配置超次数或成功返回。
	 * */
	ret =radio_cfg_params(E43,
					C2_RAM_PARAM,
					g_e43_uart_fd,
					UART_SPEEDTYPE_9600,
					UART_MOD_8N1,
					AIR_SPEEDTYPE_300,	//针对E43,为1.2Kbps
					(radio_dat_mode == DTU_MOD? DTU_MOD_ON:ADDR_MOD_ON),			//注意透明传输时，设置为DTU_MOD_ON
					TXD_AUX_OPEN_RX_OPEN,		//6,5,4,3,2位建议写0
					FEC_OFF,
					POWER_30dBm_ON,
					(ch_i > CH_MAX_VAL ? CH_MAX_VAL : ch_i),		//之前测试时，CH20对应430MHz，新的计算公式下，425+ CHANL*1MHz
					(radio_dat_mode == DTU_MOD? 0xFFFF :self_node_id));


	}while(ret <0);
	/*
	 * STEP5> 退出休眠模式，配置无线模块进入RSSI模式.
	 * 注意：6.5.当从休眠（配置）模式进入其它模式，模块会重新配置参数，配置过程中，
	 * AUX保持低电平，完毕后输出高电平，所以建议用户检测AUX上升沿。
	 *
	 * RSSI模式下，不可进行无线发射，收到的串口数据将被丢弃;同样不可接收
	 * 空中无线数据，仅扫描当前信道的信号强度，每100ms通过串口输出一个强度值（相对值）
	 ** */
	radio_cfg_mode(RSSI_MOD,E43);
	/*
	 * 设置超时时间(0x1ffff ms)，等待模式切换AUX事件发生
	 * */
	gpio_edge_poll(e43_aux_gpio_input_fd, CFG_AUX, 0x1ffff);

//	close(e43_uart_fd);
//	LOG_MSG(INFO_LEVEL,"---------------------radio cfg param over\n");

	/*
	 * 接下来以配置的波特率打开MCU串口，获取RSSI输出值
	 * */
//	e43_uart_fd = Uart_Open(COM2, 115200, 8, 'N', 1,1);
	if(g_e43_uart_fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", COM1);
		exit (-1);
	}

//	FD_ZERO(&rdset);
//	FD_SET(e43_uart_fd, &rdset);

	return g_e43_uart_fd;
}



/*
 * 配置E32数传无线模块工作参数，包括空口速率、串口通信参数等
 * */
int config_E32_radio_params(RADIO_COM_t com, int aux_fd, Radio_CHL_t ch_val, u8 speed_val, u8 pwr_val)
{
	int fd =-1;
	RADIO_DEV_t dev = E32_M;;
	/*
	 *STEP2> 配置无线模块进入休眠模式M0M1 =11，用于模块参数设置
	 * */
    if(com == COM1)
    {
    	dev = E32_M;
    	if(  M_e32_uart_fd >0)	close(M_e32_uart_fd);
    }
    else if(com == COM7 )
    {
    	dev = E32_L;
    	if(  L_e32_uart_fd >0)	close(L_e32_uart_fd);
    }

	radio_cfg_mode(STANDBY_MOD,dev);

	/*
	 *STEP3> 打开MCU串口,配置串口波特率为9600、8N1模式，与无线模块进行通信。
	 * */
	fd = Uart_Open(com, 9600, 8, 'N', 1, 1);
	if(fd ==-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", com);
		return -1;
	}
	/*
	 *STEP4> 休眠模式下，发送执行查询版本命令,验证通信是否正常
	 * */
	/*
	memset(rv_buff, 0,sizeof(rv_buff));
	uartsendto(uart_fd, version_cmd, 3);
	sz = uartrecvfrom(uart_fd, rv_buff, 4);
	if(sz >0)rv_buff[sz >0] = 0;
	//C3 32 31 14，其中32代表无线电模块频段为433MHz, 31是其软件版本
	LOG_MSG(DBG_LEVEL,"version bytes %d, bytes: %02x %02x %02x %02x\n", sz ,rv_buff[0], rv_buff[1], rv_buff[2], rv_buff[3]);
	*/

	//
	/*
	 * STEP5> 休眠模式下，对无线模块配置参数如下：
	 * （1）波特率配置为115200、8N1模式
	 * （2）空口速率配置为19.2k
	 * （3）透明传输模式 或定点传输模式（后者支持广播地址）
	 * （4）FEC_ON模式
	 * （5）发射功率最大
	 *  (6) 信道为CH20
	 *  (7) 模块地址（节点id）
	 * */
	LOG_MSG(DBG_LEVEL,"--------------------------------------my node id =%d\n", self_node_id);

	radio_dat_mode = DTU_MOD;		//通信模式：透传或点对点模式
	radio_cfg_params(dev,
					C0_FLASH_PARAM,
					fd,
					UART_SPEEDTYPE_115200,
					UART_MOD_8N1,
					speed_val,
					(radio_dat_mode == DTU_MOD? DTU_MOD_ON:ADDR_MOD_ON),			//注意透明传输时，设置为DTU_MOD_ON
					TXD_AUX_OC_RX_PULLUP,
					FEC_OFF,
					pwr_val,
					ch_val,
					(radio_dat_mode == DTU_MOD? 0xFFFF :self_node_id));

	/*
	 * STEP6> 推出休眠模式，配置无线模块进入DTU模式.
	 * 注意：6.5.当从休眠（配置）模式进入其它模式，模块会重新配置参数，配置过程中，
	 * AUX保持低电平，完毕后输出高电平，所以建议用户检测AUX上升沿。
	 ** */
	radio_cfg_mode(radio_dat_mode,dev);

	/*
	 * 设置超时时间(0x1ffff ms)，等待模式切换AUX事件发生
	 * */
	gpio_edge_poll(aux_fd, CFG_AUX, -1);//0x1ffff);

	close(fd);

	LOG_MSG(INFO_LEVEL,"--------------------------------------E32 radio cfg param over\n");

	/*
	 * STEP7> 最后以配置的波特率打开MCU串口，进行用户数据透明传输
	 * */
	fd = Uart_Open(com, 115200, 8, 'N', 1,1);
	if(fd <=-1)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open COM[%d] for configure Radio!\n", com);
		exit (-1);
	}

	return fd;

}


/*
 * @function:
 * 			 e43_cong_band_rssi_test
 * @brief:
 * 			e43全频段信道RSSI感知测试函数
 * @param:
 * @return:
 * 			空
 * */
void e43_cong_band_rssi_test()
{
	int i =0;
	u8 rssi, max,min, avg,temp_rssi_acc =0;
	int fd=0, rv, cong_cnt=0;

	for(i=0;i < CH_MAX_NUM;i++)
	{
		fd =E43_channl_rssi_congitive(i);

		max =0, min=0, avg=0;
		temp_rssi_acc =0;
		cong_cnt =0;
		/*
		 * 多次扫描当前信道RSSI。。。记录&计算RSSI最大值，最小值、平均值
		 * */
		while(cong_cnt < CONG_MAX_TIMES_PER_CHNL )
		{
			rv = uartrecvfrom(fd, &rssi, 1);
			if(rv >0)
			{
				if(cong_cnt==0)
				{
					min = rssi;
					max = rssi;	//求当前信道RSSI最大值
				}
				else
				{
					if(rssi > max)
					{
						max = rssi;	//求当前信道RSSI最大值
					}

					if(rssi < min)
					{
						min = rssi;	//求当前信道RSSI最小值
					}
				}
				temp_rssi_acc += rssi;		//累加RSSI值，
			}
			cong_cnt++;
			LOG_MSG(DBG_LEVEL,"cong_cnt%d,CH%d, RSSI bytes %d, bytes: %d\n", cong_cnt,i, rv ,rssi);
		}//while

		avg = (u8)(temp_rssi_acc /CONG_MAX_TIMES_PER_CHNL);//求当前信道RSSI平均值
		LOG_MSG(DBG_LEVEL,"-------------------------------------------------------ch%d' rssi max=%d, avg=%d, min=%d,\n",\
					i,\
					max,\
					avg,\
					min);

		close(fd);
		fd =-1;

		//准备扫描下一个信道
	}//for
}


