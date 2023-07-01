/**
 * @fuction：	起线程，从uart无线发送优先队列中获取发送报文，通过udp广播该消息。
 * @author：
 * @email:
 * @date：
 * @version：
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include "main.h"
#include "aux_func.h"
#include "queues.h"
#include "Routing.h"
#include "serial.h"
#include "radio.h"
#include "Congitive.h"
#include "mac.h"


time_t 			wd_UART;
#define 		WD_UART_MSG_SIG 63

void* uartWireless_query_and_send_to_routine(void * args);

extern int e32_uart_fd;

extern unsigned short self_node_id;


//#define rand_t  					(rand()  & 0x1ff)

/* 20180620
 * 1> radio接入信道时，接入周期ACCESS_CHNL_PERIOD_MSG+随机数，以减少碰撞发生的几率
 * 2> 在Routing.c中通过周期定时器产生LSA报文，周期值由routing.ini决定
 * 3> 将ACCESS_CHNL_PERIOD_MSG访问信道的周期设置为250ms,是通过计算无线Radio通信速率
 *    以及用户终端每秒最大可产生字节数（最大600字节），来决定的。
 *
 * 	  radio	4.8kbps时，每秒600字节，每500ms 300字节，每250ms 150字节，每100ms 60字节， 每50ms 30字节
 * 	  radio	9.6kbps时，每秒1200字节，每500ms 600字节，每250ms 300字节，每100ms 120字节， 每50ms 60字节
 * 	  radio	19.2kbps时，每秒2400字节，每500ms 1200字节，每250ms 600字节，每100ms 240字节， 每50ms 120字节
 *
 * 4> 当radio正在接收数据包时，发送数据包定时器超时发生，此时应采取随机退避t2时间让出信道，取值为0～25ms
 * */

/*
 * 250ms, 19.2kbps时，电台可传输300字节;由于子网内存在4个节点（电台），并且同频
 * 工作，且都有可能随机接入信道。为避免碰撞，接入时机要求最大概率不同，且要求至少间隔250ms为佳。
 * 因此
 * */
#define ACCESS_CHNL_PERIOD_MSG		300		//19.2kbps时，可传输300字节

/*
 * 信道接入随机因子t1的最大值
 * */
const double rand_t1_max 			=150.0;

/*
 * 信道接入时刻为：固定周期+随机因子t1
 * */
#define rand_t1  					(1+ (int)(rand_t1_max *(rand()/(RAND_MAX+1.0)))) /*0~x00随机数*/

/*
 * 当Radio正处在接收数据包状态时，发送定时器回调函数发生超时则随机避让rand_t2时间
 * */
#define rand_t2  					(1+ (int)(50.0 *(rand()/(RAND_MAX+1.0)))) /*0~x00随机数*/

int snd_msgqid;

//extern u8 	global_radio_on_receving ;
/*
 *
 * 信道动态多点接入技术可分为受控接入和随机接入两类
 * (1) 随机接入　特点是所有的用户都可以根据自己的意愿随机地向信道上发送信息。
 * 当两个或两个以上的用户都在共享的信道上发送信息的时候，就产生了冲突(collision)，
 * 它导致用户的发送失败。随机接入技术主要就是研究解决冲突的网络协议。
 * 随机接入实际上就是争用接入，争用胜利者可以暂时占用共享信道来发送信息。
 * 随机接入的特点是：站点可随时发送数据，争用信道，易冲突，但能够灵活适应站点数目及其通信量的变化。
 * 典型的随机接入技术有ALOHA、CSMA、CSMA/CD。
 *
 * (2) 受控接入　特点是各个用户不能随意接入信道而必须服从一定的控制规则。
 * 可分为集中式控制和分散式控制。典型的有多点线路轮询和令牌传递。
 *    a) 轮询属于集中式控制，控制节点按一定顺序逐一询问各用户节点是否有信息发送。
 *    如果有，则被询问的用户节点就立即将信息发送给控制节点；如没有，则控制节点依次询问下一节点。
 *    b) 令牌环属于分布式控制，在环路中通过特殊的令牌环帧沿着环路逐站传递，只有获得令牌的节点才有权发送信息。
 *       当信息发送完毕，就将令牌传递给下一站。
 *
 * (3) 本项目由于水面存在4个超短波Ad Hoc通信节点， 其中节点1为高耐波大艇，机动能力和通信能力强，其它节点为自然能无人艇
 *    a) 采用受控轮询方式接入信道。其中节点1为控制节点，节点2、3、4为用户节点。轮询方式接入信道
 *    的策略如下：
 *       1）由于节点1发送数据包最大字节数为600字节($UB),其它节点发送数据包最大字节数为300字节,通信速率为19.2kbps,
 *	        radio 4.8kbps时，每秒600字节，每500ms 300字节，每250ms 150字节，每100ms 60字节， 每50ms 30字节
 * 	  		radio 9.6kbps时，每秒1200字节，每500ms 600字节，每250ms 300字节，每100ms 120字节， 每50ms 60字节
 * 	  		radio 19.2kbps时，每秒2400字节，每500ms 1200字节，每250ms 600字节，每100ms 240字节， 每50ms 120字节

 *			因此信道接入最短时间需要250ms。
 *
 *			定时器的周期根据电台的当前传输速率和用户数据包大小确定。
 *
 *	     2) 节点1作为控制节点，注册信道接入定时器，定时器超时周期为250ms。在定时器ISR函数中
 *	        实现各节点访问信道轮询。
 *
 *	     3）  实现思路流程如下：
 *			定时器超时事件发生，执行Node1_Timer_ISR, 首先节点1自己判断是否有消息待发送，
 *	        如果有，则占用信道并发送1次消息， 然后退出ISR。
 *
 *	        若无，依次轮询节点2、3、4是否有数据待发送，即发三次TokenRingReq（x; x=2、3、4）,
 *	        各节点(x)收到TokenRingReq(x)请求后，根据发送消息队列的是否为空，回复ACK（0-无消息，1-有消息待发送）.
 *			节点1采用"节点id小优先级高的策略，处理ACK" ;
 *			若节点n(n=min{2\3\4})回复ACK有消息发送，则节点1给节点n发送TokenRingRep(n)指令。
 *			节点n收到TokenRingRep(n)指令后，则占用信道并发送1次消息;
 * */

void snd_msgTo_radio(struct q_item* msgitem, int msglen)
{
	u8 temp_buff[512]	={0,};
	u16 destid 			=0;
	u8 config_otherRadios_Over=FALSE;

	if(msglen <=0)
		return ;

	if(msgitem->data[0] == LSA_PK_TYPE )
	{
		/*
		 * 对于消息队列WLAN_SEND_CQ中的消息，第0字节为pk_type,类型包括LSA_PK_TYPE/DATA_PK_TYPE/RREQ_PK_TYPE/RREP_PK_TYPE等
		 * CONG_PK_TYPE是网络本感开始指令
		 * */
		LOG_MSG(INFO_LEVEL,"\n============================================radio_pk_send LSA_PK to uartWireless, len = %d\n\n",msglen);
		destid =255;
	}
	else if(msgitem->data[0]== CONG_START_PK_TYPE)
	{
		LOG_MSG(INFO_LEVEL,"\n============================================radio_pk_send CONG_START_PK_TYPE to uartWireless, len = %d\n\n",msglen);
		destid =255;
	}
	else if(msgitem->data[0]== CONG_FREQ_RESULT_PK_TYPE)
	{
		LOG_MSG(INFO_LEVEL,"\n============================================radio_pk_send CONG_FREQ_PK_TYPE to uartWireless, len = %d\n\n",msglen);
		destid =255;

		/*
		 * 主节点运行网络协同感知决策算法后，生成频率和速率参数，然后
		 * 通过CONG_FREQ_PK_TYPE类型包下发至子网内其它节点。下发时：
		 * 1》先将CONG_FREQ_PK_TYPE类型广播发送
		 * 2》然后将主节点自己配置工作在新的频率和速率参数
		 * */
		config_otherRadios_Over = TRUE;

	}
	else if(msgitem->data[0]== CONG_RET_PK_TYPE )
	{
		destid =master_cong_id;			//目的节点为网感发起者，在Routing.c handleWLanData中赋值

		LOG_MSG(INFO_LEVEL,"\n============================================radio_pk_send CONG_RET_PK_TYPE to uartWireless, len = %d\n\n",msglen);
	}
	else
	{
		/*
		 * 非LSA_PK_TYPE,其地址为非广播地址，形如10.0.destid.1，其第4、第5字节为消息的rcv_id
		 *
		 * 2018-06-06 通过设置queues.h中WLAN_ROUTE_DATA=1, WLAN_ROUTE_CTRL=2,
		 * 提升了APP_PK发送优先级，从而达到数据发送具有实时性
		 *
		 * 由于在譬如发送APP_PK时，调用了htons()API函数，将主机short类型变量（小端模式）转变为网络字节顺序（大端模式），
		 * 网络字节顺序时高字节存储在低位，低字节存储在高位，譬如下面的data[4]为低位，data[5]为高位，因此，
		 * 在表示目的id时，data[4]<<8位。
		 *
		 * */
		destid = ((unsigned int)msgitem->data[4]<<8) + ((unsigned int)msgitem->data[5]);
		LOG_MSG(INFO_LEVEL,"\n============================================radio_pk_send PK to uartWireless,dest_id =%d,  len = %d\n",
				destid,\
				msglen);
	}

	//调用无线模块发送函数
	memcpy(temp_buff, msgitem->data, msglen>512 ? 512: msglen);

	radio_pks_sendto(M_e32_uart_fd, temp_buff , msglen, destid);

	/*
	 * 主节点自己配置工作在新的频率和速率参数
	 * */
	if(config_otherRadios_Over ==TRUE)
	{
//		global_freq_config_state = FREQ_CONFIG_END;
		global_running_state = FREQ_CONFIG_END;
		config_otherRadios_Over	= FALSE;
	}




}

/*
 * 集中式控制访问信道
 * */
void MAC_Wireless_Centralize_SendHandler(int sig, siginfo_t *si, void *uc)
{
	struct q_item 	msgitem;
	int 			msglen;
	//LOG_MSG(INFO_LEVEL,"uartWireless_send_routine wlan send cq id = %d\n",msgqid);
	u16 timeouts=0;
	memset(&msgitem,0,sizeof(msgitem));
	timeouts =0;
	while(Get_TR_State() != TR_ST_INIT && \
			timeouts++ < SLICE_TIMEOUT_MS)
	{
		usleep(1000);
	}

	if(timeouts >= SLICE_TIMEOUT_MS )
	{
		//某节点超时未回复ACK，主站则将状态机复位至初始状态，准备询问下一个节点
		LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Master failed/timeout(%d) return TR_ST_INIT \n",timeouts);
//				global_token_ring_state = TR_ST_INIT;
		Set_TR_State(TR_ST_INIT);
		wdStart(wd_UART, SLICE_TIMEOUT_MS, TIMER_ONE_SHOT);
		return;
	}
	/*Step1>
	 * 大船主控节点询问自身是否有消息待发送
	 * 需要考虑多跳中继场景下信道占用的开销
	 * 即下一跳节点收到主控节点的业务数据报文后，给予ＡＣＫ回复的时延。
	 * */
	msglen = msgrcv(snd_msgqid,&msgitem,MAX_MSG_LEN,-2,IPC_NOWAIT);
	if(msglen > 0)
	{
		snd_msgTo_radio(&msgitem, msglen);
	}

	/* Step2>
	 * 轮询其它节点是否有数据待发送
	 *
	 */
	Poll_Members_Control_Channel();

	//轮询结束，大船立即获得控制权
	wdStart(wd_UART, 1, TIMER_ONE_SHOT);
}

/*
 * mgsrcv函数，读取消息队列中的消息
 * int msgrcv(int msgid, struct msgbuf* msgp, int size,long msgType, int flag)
 * 其中：
 * msgType 	0:表示接收消息队列中第一个消息
 * 			大于0：接收消息队列中第一个类型为msgType的消息
 * 			小于0：接收消息队列中第一个类型值不小于|mgsType|绝对值且类型值又最小的消息
 *
 * flag    MSG_NOERROR:表示若返回的消息比size字节多，则消息就会被截短到size字节，且不通知消息发送进程
 * 		   IPC_NOWAIT: 表示若消息并没有立即发送，则调用进程会立即返回
 * 					0：调用阻塞，直到条件满足
 *
 * 返回值： Number of bytes copied into message buffer
 *
 **/
/**
 * 无线发送时包含两种数据类型优先级:
 *   1> WLAN_ROUTE_DATA  1
 *   2> WLAN_ROUTE_CTRL  2
 *
 * 无线接收处理时，进一步根据PK类型分别处理：
 * 	1> #define WLAN_DATA			0x03
 *  2> #define LAN_DATA				0x04
 *  3> #define SELF_DATA			0x05
 *
 * *****
//另外，关于msgrcv()API,其msgtype决定了如何从消息队列中获取数据，
 * 详见Linux Manual https://linux.die.net/man/2/msgrcv中解析如下：
 * msgtype:
 *1）if msgtype is 0, then the first message in the queue is read.
 *2）if msgtyp is greater than 0, then the first message in the queue of type msgtype is read.
 *	unless MSG_EXCEPT was specified in msgflg, in which case the first message in the queue of type not
 *	equal to msgtype will be read.
 *
 *	if msgtyp is less than 0, then the first message in the queue with the lowest type less than or equal
 *	to the absolute value of msgtyp will be read.
 *
 *msgflg:
 * The msgflg argument is a bit mask constructed by ORing together zero or more of the following flags:
 * IPC_NOWAIT:
 * 		Return immediately if no message of the requested type is in the queue. The system call fails with errno set to ENOMSG.
 * MSG_EXCEPT:
 * 		Used with msgtyp greater than 0 to read the first message in the queue with message type that differs from msgtyp.
 * MSG_NOERROR:
 * 		To truncate the message text if longer than msgsz bytes.
 * 	If no message of the requested type is available and IPC_NOWAIT isn't specified in msgflg,
 * 	the calling process is blocked until one of the following conditions occurs:
 * 	1)A message of the desired type is placed in the queue.
 * 	2)The message queue is removed from the system. In this case the system call fails with errno set to EIDRM.
 *
 *Return :
 *On failure  return -1 with errno indicating the error,
 	 otherwise msgsnd() returns 0 and
 	 msgrcv() returns the number of bytes actually copied into the mtext array.
*/

/*
 *STEP1> 首先判断本节点（1）是否有消息要发送非阻塞方式
 * */
//	msglen = msgrcv(snd_msgqid,&msgitem,MAX_MSG_LEN,-2,IPC_NOWAIT);
/*
 *
 * */
//	while(global_token_ring_state != TR_ST_INIT && timouts++ <SLICE_TIMEOUT_MS )
//	{
//		usleep(1000);		//等待255*2ms
//	}


/*
 * 无线发送时包含两种数据类型优先级:
 *   1> WLAN_ROUTE_DATA  1
 *   2> WLAN_ROUTE_CTRL  2
 * 值设置为-2,表示优先获取WLAN_ROUTE_DATA
 * */

/*
 *@function create_uartWireless_sendTimer_thread
 *@描述	创建访问无线信道定时器，即轮旬接入信道
 * */
int create_uartWireless_sendTimer_thread()
{
	int ret;

//	while(global_token_ring_state != TR_ST_INIT)
//	{
//		usleep(1000);
//	}

	snd_msgqid = msgget(WLAN_SEND_CQ,IPC_EXCL);
	if(snd_msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL,"uartWireless_query_and_send_to_routine WLAN_SEND_CQ not found\n");
		exit(-1);
	}

	LOG_MSG(INFO_LEVEL,"uartWirelessSend get WLan_Send_CQ id = %d\n",snd_msgqid);



	//节点1作为信道访问中心控制节点
	if(self_node_id == 1)
	{
		/*
		 * 启动定时器，周期发送测试报文
		 * */
		ret = wdCreate(WD_UART_MSG_SIG, &wd_UART, &MAC_Wireless_Centralize_SendHandler);
		wdStart(wd_UART,1000 , TIMER_ONE_SHOT);
		//wdStart(wd_UART,SLICE_TIMEOUT_MS , TIMER_ONE_SHOT);
	}
	return ret;
}
