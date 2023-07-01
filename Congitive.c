/*
 * Congitive.c
 *
 *  Created on: May 23, 2018
 *      Author: root
 */


/*
 * 高龙飞：
 *低速的当信令，主站随机选一个频率，告诉从站，然后测试一下丢包率，经过几次测试，得到可用频率
 * */

/*
 * 周鑫：
 * 针对ICR电台，信令信道上，主站和子网内各从站一跳可达;在业务/感知信道上，支持主从组网（Ad Hoc网络）模式。
 * 设计时，应该认为开机时，整网可用于Ad Hoc通信的频率、高速速率档的均不可知，为此需要通过频谱感知
 * 、波形参数智能优先等流程，智能决策生成公共通信频率集和波形参数;然后在子网内广播下发用于
 * Ad Hoc网络业务通信的波形参数，包括频率和速率。
 *
 * */

/*
 *	吴济文：
 *	针对哈工程项目，由水面1大艇、3小艇构成的水面超短波移动自组织Ad Hoc网络。
 *	单个节点上存在3个超短波电台，1个用于业务通信的5W高速超短波设备，支持19.2kbps，1个用于信令通信的超短波设备，
 *	支持低速、远距离通信。1个用于本地频谱电磁环境监测的小功率13dBm超短波模块，支持RSSI输出。
 *	为实现频谱感知--->波形参数决策-->AdHoc网络通信系统流程，系统设计如下：
 *	1> 设备开机时，由大艇节点通过信令超短波电台广播发送“本感结果请求”（Locl_Cog_Start）命令,
 *	   同时大艇处理本节点的结果，并保存本地数组，用1维数组表示，每个元素用1个结构体表示。
 *	   struct{
 *	    u8 node_id;
 *	    u8 ch_id;
 *	    u8 avg_rssi;
 *	    u8 min_rssi;
 *	    u8 max_rssi;
 *	    u8 rssi[MAX_FREQS_CHNL_NUM];	//每个信道的RSSI值
 *	   };
 *
 *	   同时记录网络感知的开始时间 cog_start_t;
 *
 *	   打印输出本节点的频谱监测结果。
 *
 *	   启动网络感知定时器，定时器到则运行频谱决策算法，将子网内收到的各节点本感结果
 *	   进行判决，优选出RSSI最小的频率，并通过信令发送给各从站。
 *
 *	2> 子节点收到频谱感知请求（Locl_Cog_Start）命令，将本节点的感知结果通过信令发给主节点。
 *	   并启动定时器，监听主站下发的参数;收到主站下发参数后，取消定时器。否则。。。如何处理
 *
 *	3> 主节点收到子节点的本感上报结果，将结果存储在子节点id为下标的数组中
 *	   每次记录本感结束时间，在网络感知定时器到时，计算整网本感耗时。
 *
 *	4> 如1）所述，在网络感知定时器超时事件发生后，运行频谱决策算法。
 *	   频谱决策算法1: 拟采用二分图-极大团的思想（参考二分图-幼儿园问题），求得整网的可用频率集。
 *	   频谱决策算法2: 拟采用求所有节点每个信道公用最大值和平均值，然后求所有信道公用最大值的最小值，其对应的频率推荐为整网的可用频率。
 * */
/*
 * @fuction: Cog_local_start
 * 本感开始,由主站发起本感开始命令，各从站收到命令后，上报本地本感结果。
 * */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "aux_func.h"
#include "radio.h"
#include "Congitive.h"
#include "serial.h"
#include "gpio.h"
#include "queues.h"

time_t 			wd_Net_Congitive;

/*
 * 网感发起后，主节点创建定时器，等待从节点上报结果，超时则开始运算感知决策算法
 * */
#define 		WD_NET_CONG_MSG_SIG 		64

u8 avg_rssi [CH_MAX_NUM ];		//记录每个信道N次扫描rssi平均值
u8 max_rssi [CH_MAX_NUM ];		//记录每个信道N次扫描rssi值的最大值
u8 min_rssi [CH_MAX_NUM ];		//记录每个信道N次扫描rssi值的最小值


static 	int send_msgq_id;

extern unsigned short 	self_node_id;
/*
 * 网络频谱认知发起的节点id
 * */
u8 master_cong_id =-1;
void Cong_Local_Start();
void CongNet_Timer_Start();
static void Local_Cong_Upload();
//全局的子网内本感结果
Net_Congitive_Result_t  g_net_cong_result_list[MAX_NET_NODES];


/*
 *@function:
 *	Net_Cong_Desicion_Handle
 *
 *@desc:
 *	子网内网络感知结束后，运行的频谱决策算法
 *
 *@return :
 *	ch_i, 优先RSSI最小的信道id
 *
 * */
u8 NetCong_Desicion_Handle()
{
	/*
	 * 子网内感知结果g_net_cong_result_list[MAX_NET_NODES]处理流程：
	 * 1、先获取列方向avg_rssi[i]的最大值avg_max[i]
	 *
	 * 2、然后比较avg_max[0....Max-1]，取最小的下标为优先的信道id。
	 *
	 * 3、各节点感知结果二维表表示如下：
	 * 		|	ch0			ch1			ch2				ch3			ch4		   ...	ch15
	 * Node0| avg_rssi[0]  avg_rssi[1]  avg_rssi[2]   avg_rssi[3]   avg_rssi[4] ...  avg_rssi[15]
	 * Node1| avg_rssi[0]  avg_rssi[1]  avg_rssi[2]   avg_rssi[3]   avg_rssi[4] ...  avg_rssi[15]
	 * Node2| avg_rssi[0]  avg_rssi[1]  avg_rssi[2]   avg_rssi[3]   avg_rssi[4] ...  avg_rssi[15]
	 * Node3| avg_rssi[0]  avg_rssi[1]  avg_rssi[2]   avg_rssi[3]   avg_rssi[4] ...  avg_rssi[15]
	 *  						.......
     * NodeMax| avg_rssi[0]  avg_rssi[1]  avg_rssi[2]   avg_rssi[3]   avg_rssi[4] ...  avg_rssi[15]
	 * */

	u8 avgRssi_Common_max_list[CH_MAX_NUM]={0,};

	volatile int n=0, k=0;

	int i	=0;
	int ch_i =0;
	volatile int tmp_rssi_max =0;
	volatile int tmp_rssi_min=0;

	LOG_MSG(INFO_LEVEL,"--------------------------------------Start Net_Cong_Desicion_Handle...\n");

	for(k =0; k <CH_MAX_NUM;k++)
	{
		for(n=0; n< MAX_NET_NODES ; n++)
		{
			//依次获取每个节点的所有信道感知结果
			if(g_net_cong_result_list[n].node_sta == CONG_OK)
			{
//				LOG_MSG(INFO_LEVEL,"---------------ch=%d-----node%d g_net_cong_result_list[] stata OK...\n",k,n);

				//如果第n个节点网络感知结果有效,取其上报的第ch个信道的AVG_RSSI值，与第ch个信道的公用最大值比较
				tmp_rssi_max  =g_net_cong_result_list[n].cong_result.avg_rssi[k];

				if(tmp_rssi_max >  avgRssi_Common_max_list[k])
				{
					avgRssi_Common_max_list[k] = tmp_rssi_max;
				}

			}//if

			//如果第n个节点的无感知结果，则将本次网感结果“优先时”不考虑该节点的频谱环境


		}//for
	}//for

	//遍历查找最小值
	ch_i =0;
	tmp_rssi_min =avgRssi_Common_max_list[0];
	for(i=0; i<CH_MAX_NUM; i++ )
	{
		if (tmp_rssi_min > avgRssi_Common_max_list[i])
		{
			ch_i =i;
			tmp_rssi_min = avgRssi_Common_max_list[i];
		}
	}
	LOG_MSG(INFO_LEVEL,"--------------------------------------Net_Cong_Desicion_Handle return channel id= %d, avg_rssi=%d\n",ch_i, tmp_rssi_min);

	return ch_i;

}

/*
 * @function:
 * 			 Net_Cong_Result_Handle
 * @brief:
 * 			处理各个节点上报的频谱感知结果-即各信道的RSSI状态，保存在全局变量中
 *
 * @params: sub_node_cong_pkt
 * 			子网内从节点上报的本感结果
 * @return
 * 			空
 *
 * */
void NetCong_Result_Receive_Handle(u8 *sub_node_cong_pkt)
{
	//
	u8 send_id=0;
	int pk_len = sizeof(Congitive_Result_t);

	int i=0;

	//Check CRC
	if(getCRC(sub_node_cong_pkt, pk_len) !=0)
	{
		LOG_MSG(ERR_LEVEL,"rcv_cong_pack crc error\n");

		g_net_cong_result_list[send_id].node_sta = CONG_ERR;

		return ;
	}

	send_id = sub_node_cong_pkt[1];	//发送节点id

	g_net_cong_result_list[send_id].node_sta = CONG_OK;

	//保存该发送节点的本感上报结果
	memcpy((u8*)(&(g_net_cong_result_list[send_id].cong_result)), sub_node_cong_pkt, pk_len)  ;

#if 1
	Congitive_Result_t* tmp =(Congitive_Result_t*)sub_node_cong_pkt;
	printf("CongResult :");
	for(i=0; i< CH_MAX_NUM;i++)
	{
		printf("ch[%d]->rssi= %d   ", i, tmp->avg_rssi[i] );
	}
	printf("\n");
#endif
}

/*
 * @function :
 * 			Net_Congitive_Timer_CallbackHandler
 * @brief:
 * 			发起网络感知的主节点，等待从节点上报结果定时器-超时处理函数
 * @return
 * 			void
 * */
void NetCong_TimeOut_CallbackHandler(int sig, siginfo_t *si, void *uc)
{
	u8 rssi_mini_ch=0;
	int ret ;

	/*
	 * 通过网络感知，选出了RSSI最小的信道，将其频率序号下发至其它节点
	 * */

	CONG_CMD_PK	cong_pk;

	int pk_len = sizeof(CONG_CMD_PK);

	LOG_MSG(INFO_LEVEL,"--------------------------------------Net_Congitive_TimeOut\n");

	/*
	 * 频谱决策定时器超时：
	 * STEP1：运行频段决策算法，优先RSSI最小的信道id
	 * */
	rssi_mini_ch = NetCong_Desicion_Handle();


	/*
	 * STEP2:下发频谱参数，生成CONG_FREQ_PK_TYPE
	 * */
	memset(&cong_pk, 0, sizeof(CONG_CMD_PK));

	cong_pk.pk_type = CONG_FREQ_RESULT_PK_TYPE;
	cong_pk.send_id = master_cong_id;

	//第一个字节携带信道id
	cong_pk.cong_cmd[0] =rssi_mini_ch;
	cong_pk.cong_cmd[1] =rssi_mini_ch;
	cong_pk.cong_cmd[2] =rssi_mini_ch;
	cong_pk.cong_cmd[3] =rssi_mini_ch;
	cong_pk.pk_crc = getCRC((u8*)&cong_pk,pk_len-2);


//	global_freq_config_state = FREQ_CONFIG_INIT;

	global_running_state = FREQ_CONFIG_INIT;

	cong_pk.pk_crc = getCRC((u8*)&cong_pk, sizeof(CONG_CMD_PK)-2);


	/*
	 * 	暂时通过业务无线模块把网络感知结果下发给各节点
	 * */
#if 1
	/*
	 * 方式一：将数据发送至无线模块发送消息队列中，由无线模块按照“先进先出”原则无线发送出去
	 * */
	int timeout_cnt =0;
	NetCong_Wireless_MsgQSnd(send_msgq_id, (u8*)&cong_pk, sizeof(CONG_CMD_PK), WLAN_ROUTE_CTRL,IPC_NOWAIT);

	//等待“新的频率和速率参数”发送出去。。。然后往下执行
	//见uartWirelessSend.c timer_Pkt_Wireless_SendHandler(...)函数
	while(	global_running_state==FREQ_CONFIG_INIT && timeout_cnt++ < 0x1f) {usleep(300000);}
#else

	/*
	 * 方式二：直接通过无线模块下发至各节点
	 * */
	radio_pks_sendto(e32_uart_fd, (u8*)&cong_pk , pk_len, 255);

#endif

	if(timeout_cnt >0x1f)
	{
		LOG_MSG(INFO_LEVEL,"--------------------------------------Net_Congitive_Freq timeout_cnt(0x20)\n");

		MAC_Wireless_Centralize_SendHandler(0, NULL, NULL);
	}

	config_E32_radio_params(COM1,M_e32_aux_gpio_input_fd,rssi_mini_ch, AIR_SPEEDTYPE_19K2,POWER_21dBm_ON);

	LOG_MSG(INFO_LEVEL,"--------------------------------------Net_Congitive_Freq(%d) Send to others Over \n", rssi_mini_ch);

	//网感结束，恢复正常模式
	global_running_state = NET_NORMAL_RUNNING;
//	global_freq_config_state= FREQ_CONFIG_INIT;
	wdStart(wd_LSA, PERIOD_LSA,TIMER_ONE_SHOT);
}



/*
 * 获取无线模块发送消息队列
 * */
int  NetCong_get_Wireless_SndQueueId()
{
	int ret;
	ret = msgget(WLAN_SEND_CQ,IPC_EXCL);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"get WLAN SEND msg queue");
		return ret;
		exit(-1);
	}else
	{
		send_msgq_id = ret;
	}
	return ret;
}
/*
 * 关于消息队列，需要几点说明：
 * 1>消息队列是一种内核自行实现同步机制的进程间/多线程间通信方式，因此在消息队列使用时不需要
 * 添加额外的方式就可以实现进程间的同步以及资源共享。
 *   关于这一点很重要，本项目中ethrecv.c\uartWirelessRecv.c中都会对同一个消息队列ROUTING_SERVR
 *   进行写操作（msgsnd...），由内核完成多线程对同一消息队列操作时的同步。
 * */

int NetCong_Wireless_MsgQSnd(int msgqid, void *msg, int msgsize,long msgtype, int msgflg)
{
	int ret;
	struct q_item titem;

	titem.type = msgtype;
	memcpy(titem.data,msg,msgsize);
	/*
	msgrcv()可以从消息队列中读取消息，
	msgsnd()将一个新的消息写入队列。
	函数原型:
		int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
		ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
	*/
	ret = msgsnd(msgqid,&titem,msgsize,msgflg);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"NetCong_Wireless_MsgQSnd");
		return ret;
	}
	LOG_MSG(INFO_LEVEL,"send msg into queue id = %d, msgsnd ret=%d \n",msgqid, ret);
	return ret;
}

/*
 * @function
 * 			Start_Net_Congitive
 * @brief：
 * 			网感入口。接收用户指令start_net_congitive, 主站开始子网内频谱感知过程，收集各从节点的本地各信道频谱结果，
 * 			最后通过决策算法，智能优先可用频率
 * @Note：
 * 			各从节点收到本感开始指令后，在Routing.c文件的handleWLanData（）中处理CONG_START_PK_TYPE,调用Cong_Local_Start
 *
 * @author: jiwen@iscas.ac.cn
 * */
void Start_Net_Congitive()
{
	//暂时通过业务无线模块把所有节点的本感结果上报给节点0

	/*
	 * STEP1: 初始化并生成网络感知指令数据包
	 * */
	CONG_CMD_PK	cong_pk;
	int 		cmd_len = strlen(NET_CONG_CMD);

	master_cong_id = self_node_id;

	memset(&cong_pk, 0, sizeof(CONG_CMD_PK));
	cong_pk.pk_type = CONG_START_PK_TYPE;

	cong_pk.send_id = master_cong_id;

	memcpy(cong_pk.cong_cmd,NET_CONG_CMD , cmd_len);

	cong_pk.pk_crc = getCRC((u8*)&cong_pk, sizeof(CONG_CMD_PK)-2);

	//本节点本感运行状态
	global_running_state = NET_CONG_RUNNING;			//开始本感

	/*
	 * STEP2： 将本感开始指令广播发送给子网内其它节点
	 * */
	//TODO ...是否将WLAN_ROUTE_CTRL类型历史消息先清空掉。。。

	NetCong_Wireless_MsgQSnd(send_msgq_id, (u8*)&cong_pk, sizeof(CONG_CMD_PK), WLAN_ROUTE_CTRL,IPC_NOWAIT);

	/*
	 * STEP3：主节点启动网络感知定时器，定时器超时结束后，运行频谱决策算法
	 * */

	CongNet_Timer_Start();

	/*
	 * STEP4：主节点也发起开始新一轮本地感知过程
	 * */
	Cong_Local_Start();

}




/*
 *@function:
 *	 		Loc_Congitive_Timer_CallbackHandler
 *@brief:
 *		 	本地感知-定时器函数,定时更改当前信道的频率，获取信道的RSSI，反映本地频谱情况
 *@Note:
 *			在RSSI模式下，模块会每隔100ms定时输出RSSI信息，用于用户侦测实际使用环境的信号强度
 * */
void Loc_Congitive_Timer_CallbackHandler(int sig, siginfo_t *si, void *uc)
{
	u8  ch =0;		//信道下标计数器
	u8 rssi = 0;
	int rv= -1;
	int e43_uart_fd =-1;
	int cong_cnt	 =0;
	int temp_rssi_acc =0;

	Congitive_Result_t* cong_res_pk_ptr = &g_net_cong_result_list[self_node_id-1].cong_result ;

	int pk_len = sizeof(Congitive_Result_t);
	/*
	 *判断感知信道下标是否大于最大值溢出，溢出则重新扫描
	 * */
	if(ch > CH_MAX_VAL)
	{
		ch =0;
	}

	/*
	 * 开始全频段本感，获取所有信道的RSSI。。。
	 * */
	do{
		/*
		 *设置当前信道下标对应的频率为感知信道
		 * */
		e43_uart_fd = E43_channl_rssi_congitive((Radio_CHL_t)ch);
		temp_rssi_acc =0;
		/*
		 * 多次扫描当前信道RSSI。。。记录&计算RSSI最大值，最小值、平均值
		 * */
		while(cong_cnt < CONG_MAX_TIMES_PER_CHNL )
		{
			rv = uartrecvfrom(e43_uart_fd, &rssi, 0);
			if(rv >0)
			{
//				if(cong_cnt==0)
//				{
//					min_rssi[ch] = rssi;
//					max_rssi[ch] = rssi;	//求当前信道RSSI最大值
//				}
//				else
//				{
//					if(rssi > max_rssi[ch])
//					{
//						max_rssi[ch] = rssi;	//求当前信道RSSI最大值
//					}
//
//					if(rssi < min_rssi[ch])
//					{
//						min_rssi[ch] = rssi;	//求当前信道RSSI最小值
//					}
//				}
				temp_rssi_acc += rssi;		//累加RSSI值，
			}
			cong_cnt++;
			//LOG_MSG(DBG_LEVEL,"cong_cnt%d,CH%d, RSSI bytes %d, bytes: %d\n", cong_cnt,ch, rv ,rssi);
		}

		avg_rssi[ch] = (u8)(temp_rssi_acc /CONG_MAX_TIMES_PER_CHNL);//求当前信道RSSI平均值
		LOG_MSG(DBG_LEVEL,"-------------------------------------------------------ch%d' rssi max=%d, avg=%d, min=%d,\n",\
					ch,\
					max_rssi[ch],\
					avg_rssi[ch],\
					min_rssi[ch]);

		//准备扫描下一个信道
		ch++;
		close(e43_uart_fd);

	}while(ch < CH_MAX_NUM );

	//上报/记录本感结果
	LOG_MSG(DBG_LEVEL,"---------------------Master_Cong_id=%d, self_node_id=%d \n", master_cong_id, self_node_id);
	if(master_cong_id == self_node_id)
	{
		//本节点发起网络感知，则将本节点的感知结果，保存至对应节点的感知结果中
		memset((u8*)cong_res_pk_ptr,0, pk_len);
		/*
		 * 将本感结果的数据，拷贝至对应的字段
		 * */
		g_net_cong_result_list[self_node_id-1].node_sta = CONG_OK;

		cong_res_pk_ptr->pk_type = CONG_RET_PK_TYPE;
		cong_res_pk_ptr->send_id = self_node_id;
		cong_res_pk_ptr->recv_id = master_cong_id;

		//将感知结果保存至全局数组中

		memcpy(cong_res_pk_ptr->avg_rssi, avg_rssi, CH_MAX_NUM);
		cong_res_pk_ptr->pk_crc = getCRC((u8*)cong_res_pk_ptr, pk_len-2);

	}
	else
	{
		Local_Cong_Upload();
	}

}


/*
 * @function:
 * 			Cong_local_upload
 * @brief:
 * 		   各从站上报本感结果，本感结果RSSI反映本地电磁环境，RSSI的值为相对值
 * */
static void Local_Cong_Upload()
{
	//暂时通过业务无线模块把所有节点的本感结果上报给节点0
	Congitive_Result_t cong_res_pk ;

	int pk_len = sizeof(Congitive_Result_t);

	LOG_MSG(DBG_LEVEL,"--------------------------------------Local_Cong_Upload\n");

	memset(&cong_res_pk,0, pk_len);

	/*
	 * 将本感结果的数据，拷贝至对应的字段
	 * */
	cong_res_pk.pk_type = CONG_RET_PK_TYPE;
	cong_res_pk.send_id = self_node_id;
	cong_res_pk.recv_id = master_cong_id;

	memcpy(cong_res_pk.avg_rssi, avg_rssi, CH_MAX_NUM);

	cong_res_pk.pk_crc = getCRC((u8*)&cong_res_pk, pk_len-2);

	//通过无线发送至主节点
	NetCong_Wireless_MsgQSnd(send_msgq_id, &cong_res_pk, pk_len, WLAN_ROUTE_DATA,IPC_NOWAIT);
	return ;

}


/*
 * @function:
 * 		 Cong_Local_Start
 * @brief:
 * 		 1>主站收到“start_net_congitive”命令，立即开始本感操作Cong_Local_Start()...，
 * 		 将本节点的本感结果保存在下标为node_id的g_net_cong_result_list[]数组中。
 *
 * 		 2>同时由主站通过信令信道下发本感开始命令,从站收到后，开始本感功能
 * @return :
 * 		void
 * */
extern int g_e43_uart_fd;

void Cong_Local_Start()
{

	u8  ch =0;		//信道下标计数器
	u8 	rssi = 0;
	int rv= -1;
	int e43_uart_fd =-1;
	int cong_cnt	 =0;
	int temp_rssi_acc =0;

	LOG_MSG(DBG_LEVEL,"\n-------------------------------------------------------my_node_id =%d, Start Local Congitive\n", self_node_id);

	/*
	 * 子网节点id从0开始，
	 * */
	Congitive_Result_t* cong_res_pk_ptr = &g_net_cong_result_list[self_node_id].cong_result ;

	int pk_len = sizeof(Congitive_Result_t);
	close(g_e43_uart_fd);
	g_e43_uart_fd =-1;
	/*
	 * 开始全频段本感，获取所有信道的RSSI。。。
	 * */
	do{
		/*
		 *设置当前信道下标对应的频率为感知信道
		 * */
		e43_uart_fd = E43_channl_rssi_congitive((Radio_CHL_t)ch);
		temp_rssi_acc =0;
		cong_cnt =0;
		/*
		 * 多次扫描当前信道RSSI。。。记录并计算RSSI最大值，最小值、平均值
		 * */
		while(cong_cnt < CONG_MAX_TIMES_PER_CHNL )
		{
			rv = uartrecvfrom(e43_uart_fd, &rssi, 1);
			if(rv >0)
			{
				if(cong_cnt==0)
				{
					min_rssi[ch] = rssi;
					max_rssi[ch] = rssi;	//求当前信道RSSI最大值
				}
				else
				{
					if(rssi > max_rssi[ch])
					{
						max_rssi[ch] = rssi;	//求当前信道RSSI最大值
					}

					if(rssi < min_rssi[ch])
					{
						min_rssi[ch] = rssi;	//求当前信道RSSI最小值
					}
				}
				temp_rssi_acc += rssi;		//累加RSSI值，
			}
			cong_cnt++;
//			LOG_MSG(DBG_LEVEL,"cong_cnt%d,CH%d, RSSI bytes %d, bytes: %d\n", cong_cnt,ch, rv ,rssi);
		}

		avg_rssi[ch] = (u8)(temp_rssi_acc /CONG_MAX_TIMES_PER_CHNL);//求当前信道RSSI平均值
		LOG_MSG(DBG_LEVEL,"-------------------------------------------------------ch%d' rssi max=%d, avg=%d, min=%d,\n",\
					ch,\
					max_rssi[ch],\
					avg_rssi[ch],\
					min_rssi[ch]);

		//准备扫描下一个信道
		ch++;
	}while(ch < CH_MAX_NUM);

	//上报/记录本感结果
	LOG_MSG(DBG_LEVEL,"---------------------Master_Cong_id=%d, self_node_id=%d \n", master_cong_id, self_node_id);
	if(master_cong_id == self_node_id)
	{
		//本节点发起网络感知，则将本节点的感知结果，保存至对应节点的感知结果中
		LOG_MSG(DBG_LEVEL,"---------------------Master Local Cognitive Over, Result Saving...\n");

		memset((u8*)cong_res_pk_ptr,0, pk_len);
		/*
		 * 将本感结果的数据，拷贝至对应的字段
		 * */
		g_net_cong_result_list[self_node_id].node_sta = CONG_OK;


		/*
		 * Note:
		 * 	cong_res_pk_ptr指向g_net_cong_result_list[self_node_id].cong_result
		 * */
		cong_res_pk_ptr->pk_type = CONG_RET_PK_TYPE;
		cong_res_pk_ptr->send_id = self_node_id;
		cong_res_pk_ptr->recv_id = master_cong_id;
		//将感知结果保存至全局数组中
		memcpy(cong_res_pk_ptr->avg_rssi, avg_rssi, CH_MAX_NUM);
		cong_res_pk_ptr->pk_crc = getCRC((u8*)cong_res_pk_ptr, pk_len-2);
	}
	else
	{
		Local_Cong_Upload();
	}

//	global_running_state = NORMAL_RUNNING;
//
//	wdStart(wd_LSA, PERIOD_LSA,TIMER_ONE_SHOT);

}

/*
 *@function:
 * 	CongNet_Timer_Start
 *
 *@desc:
 *  主节点收到用户net_congtive_start指令后，启动主控节点网络感知超时定时器。定时器超时后，运行频谱决策算法。
 *
 *@return :
 *	void
 * */
void CongNet_Timer_Start()
{
	wdStart(wd_Net_Congitive, NET_CONG_DELY , TIMER_ONE_SHOT);
}

/*
 * 创建本地频谱扫描定时器，在定时器函数中更改扫描信道序号，然后获取当前信道的RSSI值
 * */
void NetCong_Module_Init()
{
	int ret= -1;

	/*
	 *STEP1：初始化网络感知结果为0
	 * */
	memset(g_net_cong_result_list, 0, MAX_NET_NODES*sizeof(Net_Congitive_Result_t));

	/*
	 *STEP2:获取MAC层发送“业务”的消息队列句柄
	 * */
	ret = NetCong_get_Wireless_SndQueueId();
	if(ret<0){
		LOG_MSG(ERR_LEVEL,"------------------NetCong_get_Wireless_SndQueueId error\n");
		return ;
	}

	 /*
	  *STEP3: 创建网络感知超时定时器，
	  * */
	ret = wdCreate(WD_NET_CONG_MSG_SIG, &wd_Net_Congitive, &NetCong_TimeOut_CallbackHandler);
	if(ret ==-1)
	{
		LOG_MSG(ERR_LEVEL,"create wd_Cogitive  WD_NET_CONG_MSG_SIG failed\n");
		exit(-1);
	}

//	srand((unsigned)time(NULL));		/*可以指定不同的数为种子，指定time由于时间时刻不同，因此种子不同，产生随机数也不同*/
}
