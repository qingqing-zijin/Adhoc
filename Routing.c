
#include	"Routing.h"
#include	"linklist.h"
#include	"aux_func.h"		// 内含消息队列，定时器，系统时间读取，线程，CRC校验等操作
#include 	"queues.h"
#include 	"main.h"
#include 	"auv.h"
#include 	"Congitive.h"
#include 	"mac.h"
#include 	"Routing.h"
#include <errno.h>

void Report_Table();
void Report_Node_Attr();

//24*60*60*1000
#define 	MACRO_SECENDS_A_DAY	86400000

#define 	WD_LSA_SIG 		60
#define 	WD_RREQ_SIG		61

int			wd_LSA, wd_RREQ;
int			msgQ_Wifi_Snd,msgQ_Lan_Snd,msgQ_Route_Serv;

// Log_Level
unsigned int    RADIUS = 1;                 // //域半径：
unsigned int	CLUSTER_TO_ZRP_NUM = 3;		// Cluster过渡到ZRP的节点度门限  3
unsigned int	ZRP_TO_CLUSTER_NUM = 5;		// ZRP过渡到Cluster的节点度门限 5
unsigned int    RETRY_TIMES = 7;            // LSA超时时间
unsigned int    PERIOD_LSA  = 1000;          //LSA 发送周期
unsigned int    IERP_RREQ_TIMEOUT = 2000;   // RREQ 超时时间

static unsigned short	RREQ_Seq;
static unsigned short	Bcast_Seq;

unsigned char 			Is_Routing;		// 值0表示没有在寻路过程；值1表示在寻路（发RREQ并等待RREP的过程中）
unsigned short			RREQ_Dest_ID;	// 与Is_Routing配合使用，表示正在寻路的目的ID

unsigned short			Bcast_ForwardSeq_Record[MAX_NODE_NUM];		// 下标表示发起广播的源ID；值表示所接收到的对应ID的最新广播包序号,防止多次广播转发
unsigned short			Bcast_ReportSeq_Record[MAX_NODE_NUM];		// 下标表示发起广播的源ID；值表示所接收到的对应ID的最新广播包序号,防止多次广播上报PC

unsigned char			tempBool[MAX_NODE_NUM];				// 计算两跳邻居个数时临时使用

unsigned char 			LSA_Flag;		// LSA 发送开关，为0关闭，为1打开
extern const double		rand_t1_max;

extern u8 master_cong_id;
extern u16 self_node_id;

NODE_ATTR	Node_Attr;		// 本地节点属性，包括ID，路由状态，收发统计等

List*	APP_Buff_List = NULL;		// APP_BUFF链表，元素是BUFF_ITEM
List*	RREQ_Record_List = NULL;	// RREQ记录链表，元素是RREQ_RECORD

List*	TOKEN_RING_BC_REQ_Record_List = NULL;	// RREQ记录链表，元素是RREQ_RECORD

List*	oneHopNb_List = NULL;		// 一跳邻居链表，元素是ONE_HOP_NB_ITEM
List*	mHopNb_List = NULL;			// 多跳邻居链表，元素是MULTI_HOP_NB_ITEM
List*	IERP_List = NULL;			// 域间路由链表，元素是IERP_ITEM


//int		Sock;

pthread_t create_route_thread()
{
	pthread_t tid;
	int ret;
	ret = pthread_create(&tid,NULL,&Route_Serv,NULL);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"create route process thread");
		return ERROR;
	}
	
	return tid;
}


int createWds()
{
	int ret;
	/*
	wdCreate(...)
	@param1 sigNum 代表信号ID
	@param2 timerid 代表定时器句柄
	@param3 void(*handler)(int , siginfo_t*, void*) 定时器服务函数
	*/

	//创建消息为WD_LSA_SIG定时器
	ret = wdCreate(WD_LSA_SIG,&wd_LSA,&LSA_PK_SendHandler);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"create wd_LSA");
		return ret;
	}

	//创建消息为WD_RREQ_SIG定时器
	ret = wdCreate(WD_RREQ_SIG,&wd_RREQ,&RREQ_TimeoutHandler);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"create wd_RREQ");
		return ret;
	}
	return ret;
	
}

/*
*获取到队列的id 并存入到全局变量，如果获取成功，返回值大于等于0，否则返回值小于0
*/
int getQueuesIds()
{
	int recv_msgq_id,
		lan_send_msgq_id,
		wlan_send_msgq_id;

	int ret;
	ret = msgget(LAN_SEND_CQ,IPC_EXCL);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"get LAN SEND msg queue");
		return ret;
	}else
	{
		lan_send_msgq_id = ret;
	}

	ret = msgget(RECV_CQ,IPC_EXCL);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"get RECV msgs queue");
		return ret;
	}else
	{
		recv_msgq_id = ret;
	}

	ret = msgget(WLAN_SEND_CQ,IPC_EXCL);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"get WLAN SEND msg queue");
		return ret;
	}else
	{
		wlan_send_msgq_id = ret;
	}
	
	msgQ_Wifi_Snd = wlan_send_msgq_id;
	msgQ_Lan_Snd = lan_send_msgq_id;
	msgQ_Route_Serv = recv_msgq_id;
	
	return ret;
}

/* 路由伺服线程初始化 
 *   对路由协议相关的变量进行初始化，
 *   调用子函数，对1Hop，mHop邻居和IARP、IERP路由表及APP_BUFF缓冲区进行初始化
 *   启动LSA报文的周期发送
 
 	初始化成功返回1，失败返回0
 */
int Route_Init()
{
	const int on = 1;
	
	Node_Attr.ID = getSelfID();
	Node_Attr.cluster_state = CLUSTER_NONE;
	Node_Attr.cluster_size = 0;
	Node_Attr.cluster_header = 0xffff;
	Node_Attr.node_qos = 1;			// 暂均填写默认值1

	//域半径
	Node_Attr.IARP_radius = RADIUS;		// LSA携带一跳邻居，维护2跳邻居表

	//双向一跳邻居个数，方便分簇比较和LSA发送
	Node_Attr.degree = 1;

	//从无线端收到的业务报文的个数
	Node_Attr.rcvd_pk_num = 0;

	//发给无线端的业务报文的个数
	Node_Attr.send_pk_num = 0;

	///rcvd总字节数
	Node_Attr.rcvd_pk_size = 0;
	
	//send总字节数
	Node_Attr.send_pk_size = 0;
	

	RREQ_Seq = 0;
	Bcast_Seq = 0;
	
	Is_Routing = 0;
	RREQ_Dest_ID = 0xffff;
	
	memset((unsigned char *)Bcast_ForwardSeq_Record,0,sizeof(Bcast_ForwardSeq_Record));
	memset((unsigned char *)Bcast_ReportSeq_Record,0,sizeof(Bcast_ReportSeq_Record));

	//LSA发送开关，0-关闭，1-打开
	LSA_Flag = 1;	
	
	prg_list_delete(APP_Buff_List);
	prg_list_delete(RREQ_Record_List);
	prg_list_delete(oneHopNb_List);
	prg_list_delete(mHopNb_List);
	prg_list_delete(IERP_List);


	if( NULL == (APP_Buff_List = prg_list_create()))
		return 0; 
	if(NULL == (RREQ_Record_List = prg_list_create()))
		return 0;
	if(NULL == (oneHopNb_List = prg_list_create()))
		return 0;
	if(NULL == (mHopNb_List = prg_list_create()))
		return 0;
	if(NULL == (IERP_List = prg_list_create()))
		return 0;


	//创建LSA\RREQ定时器
	createWds();
	
	srand(time(NULL));

	
	Print_Node_Attr();

	//获取各个消息队列的id
	if(getQueuesIds() < 0)
	{
		LOG_MSG(ERR_LEVEL,"getQueuesIds");
		return -1;
	}

	/*
	if ((Sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"error\n");
		return -1;
	}

	//设置套接字，允许广播
	if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"setsockopt() failed to set SO_BROADCAST ");
		return -1;
	}*/

	/*
	wdStart函数，flag为TIMER_ONE_SHOT,表明定时器运行一个周期，定时器超时则回到未启动状态
	*/

	//广播时间, 当前是1秒，至少10秒；涉及到信道接入
	//半径-ZRP，最大支持9跳，因此根据哈工程场景，可以设置为3
	//数据包大小，256/64，上层可能需要分帧处理
	//wdStart(wd_LSA, PERIOD_LSA+(rand()&0x1f),TIMER_ONE_SHOT);
	wdStart(wd_LSA, PERIOD_LSA,TIMER_ONE_SHOT);
	return 1;
}


void handleWLanData(unsigned char *msgBuff,int msgLen)
{
		unsigned char	*wLanData_pkptr = msgBuff;
		
		u8 send_id, recv_id;

		//默认收到数据包时，则认为信道状态回到初始状态（TOKEN_RING_INIT）
		// 1. 根据Route报头类型，解析出源、目的ID及其它报文信息
		if (wLanData_pkptr[0] == DATA_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;
			APP_PK_Rcvd(wLanData_pkptr, msgLen);

#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
		}
	    else if(wLanData_pkptr[0] == LSA_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;
			LSA_PK_Rcvd((LSA_PK*)wLanData_pkptr,msgLen);
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
		}
		else if(wLanData_pkptr[0] == RREQ_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;
			RREQ_PK_Rcvd((RREQ_PK*)wLanData_pkptr, msgLen);
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
		}
		else if(wLanData_pkptr[0] == RREP_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;
			RREP_PK_Rcvd((RREP_PK*)wLanData_pkptr,msgLen);
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
		}
		else if(wLanData_pkptr[0] == RERR_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;
			RERR_PK_Rcvd((RERR_PK*)wLanData_pkptr,msgLen);
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
		}
		else if(wLanData_pkptr[0] == CONG_START_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;
			if(global_e43_existed == FALSE)
			{
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
				return ;
			}

			if(getCRC(wLanData_pkptr, sizeof(CONG_CMD_PK)) !=0)
			{
				LOG_MSG(ERR_LEVEL,"\n-------------recv CONG_START_PK_TYPE crc error\n");
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
				return;
			}
			//收到本感开始指令，则开始本节点业务各信道频谱感知过程，结束则上报。
			master_cong_id = wLanData_pkptr[1];

			global_running_state = NET_CONG_RUNNING;

			LOG_MSG(INFO_LEVEL,"\n-------------------------------------------------------recv net_cong_pk from node%d\n",master_cong_id);
			Cong_Local_Start();
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
		}
		else if(wLanData_pkptr[0] == CONG_RET_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;

			if(global_e43_existed == FALSE)
			{
#ifdef NOT_USED_OK_ACK
				Set_TR_State(TR_ST_INIT);
#endif
				return ;
			}

			if(getCRC(wLanData_pkptr, sizeof(Congitive_Result_t)) !=0)
			{
				LOG_MSG(ERR_LEVEL,"-------------recv CONG_RET_PK_TYPE crc error\n");
#ifdef NOT_USED_OK_ACK
				Set_TR_State(TR_ST_INIT);
#endif
				return;
			}
			//收到本感上报结果，判断
			send_id= wLanData_pkptr[1];
			recv_id= wLanData_pkptr[2];

			if(recv_id != self_node_id)
			{
#ifdef NOT_USED_OK_ACK
				Set_TR_State(TR_ST_INIT);
#endif
				return;
			}


			LOG_MSG(INFO_LEVEL,"-------------------------------------------------------recv net_cong_result  from node%d\n",send_id);
			NetCong_Result_Receive_Handle(wLanData_pkptr);
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
		}
		else if(wLanData_pkptr[0] == CONG_FREQ_RESULT_PK_TYPE)
		{
//			global_token_ring_state = TR_ST_INIT;
			//收到网络感知下发的结果结果

			if(getCRC(wLanData_pkptr, sizeof(CONG_CMD_PK)) !=0)
			{
				LOG_MSG(ERR_LEVEL,"-------------recv CONG_FREQ_PK_TYPE crc error \n");
#ifdef NOT_USED_OK_ACK
				Set_TR_State(TR_ST_INIT);
#endif
				return ;
			}
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
			global_running_state = NET_NORMAL_RUNNING;

			send_id= wLanData_pkptr[1];

			LOG_MSG(INFO_LEVEL,"--------------------------------recv CONG_FREQ(%d)_PK_TYPE  from node%d\n",wLanData_pkptr[2],send_id);
			config_E32_radio_params(COM1,M_e32_aux_gpio_input_fd,wLanData_pkptr[2], AIR_SPEEDTYPE_19K2,POWER_21dBm_ON);
		}
		else
		{
#ifdef NOT_USED_OK_ACK
			Set_TR_State(TR_ST_INIT);
#endif
			LOG_MSG(WARN_LEVEL,"unknown pk [0x%x] from radio\n",wLanData_pkptr[0]);
		}
}

/*
handleLanData
处理Lan口RAWSOCK，经过路由模块寻路后，增加路由头,保存在msgBuff[0...99]
*/
void handleLanData(unsigned char *msgBuff,int msgLen)
{
		unsigned char	*APP_pkptr;		//定义变量指针
		APP_pkptr = msgBuff;				//指针指向数组首地址
		//hexprint(APP_pkptr,msgLen);
					
		APP_PK_Send(APP_pkptr, msgLen);	//调用APP_PK_Send函数，开启路由过程，查找IARP、IERP路由表等...
}

void handleSelfData(unsigned char *msgBuff,int msgLen)
{
	//hexprint(APP_pkptr,msgLen);

	if(msgBuff[0] == 0)	// LSA_PK_Send计时器超时
	{
		LSA_PK_Send();
	}
	else if(msgBuff[0] == 1)	// RREQ_Timeout计时器超时
	{
		RREQ_Timeout();
	}
	else
	{
		LOG_MSG(WARN_LEVEL,"unknown pk from self\n");
	}
}

/* 路由伺服线程 主函数*/
void Route_Serv()
{
	int				msgLen;
	unsigned char	msgBuff[BUFF_ITEM_SIZE];	 //接收来自LanRcvThread和WiFiRcvThread的消息
	unsigned short	msgType;
	long long wstart,wend;
	long long lstart,lend;
	struct 		q_item item;	
	// 对路由协议相关的变量进行初始化，调用子函数，对1Hop，mHop邻居和IARP、IERP路由表及APP_BUFF缓冲区进行初始化
	if (0 == Route_Init())
	{	
		LOG_MSG(ERR_LEVEL,"Route_Init error, please restart WiBox\n");
		return;
	}
	
    //test_update_mympr();
    
   
    
	for(;;)
	{
		
		memset(&item,0,sizeof(item));	
		//第4个参数type 0：fifo，-1，消息类型小的先，tpye >0 仅接收 该类型的消息。
		//成功时返回有效消息的长度。否则返回-1
	
//		msgLen = msgrcv(msgQ_Route_Serv,&item,MAX_MSG_LEN,0,MSG_NOERROR);

		//wjw:从无线接收到消息优先级最高，然后是从LAN口接收的数据，最后是LSA和RREQ控制报文
		msgLen = msgrcv(msgQ_Route_Serv,&item,MAX_MSG_LEN,-3,MSG_NOERROR);

		//printf("recv msgLen = %d, item.type = %d\n",msgLen,item.type);
		
		if(msgLen > 0)
		{
			/*
			wujiwen: 
				每个msgBuff为1600字节，不小于最大业务报文长度
				读取拷贝RECV_CQ消息队列的内容，从msgBuff的第100字节开始存储
				msgBuff的头100字节为路由头
			*/	
			memcpy(msgBuff+100,item.data,msgLen);
		
			switch(item.type)
			{
				case WLAN_DATA:
					//from wlan data route header + row socket data;or route ctrls
					//发到有线的队列的数据必须是RAWSOCK的数据，从无线接收的数据剥去routehead等消息剩下的有效数据
					//wstart = sysTimeGet();
				
					handleWLanData(msgBuff+100,msgLen);
					
					//wend = sysTimeGet();
					//printf("wlan cost time = %lld\n",wend - wstart);
					break;
				case LAN_DATA:
					//from lan data raw sock
					//经过路由模块寻路后，增加路由头+RAWSOCK的数据。
					//lstart = sysTimeGet();
					
					//void handleLanData(unsigned char *msgBuff, int msgLen);
					/*
					wujiwen:
					处理msgBuff内容，增加了路由头
					msgBuff[0...99]保留分配给路由头
					msgBuff+100 访问到Lan口 RAWSOCK数据包
					msgBuff+100-ROUTE_HEAD_LEN访问处理ROUTE_HEAD + RAWSOCK内容
					*/			
					handleLanData(msgBuff+100-ROUTE_HEAD_LEN,msgLen+ROUTE_HEAD_LEN);
					
					//lend = sysTimeGet();
					//printf("lan cost time = %lld\n",lend - lstart);
					break;
				case SELF_DATA:
					
					handleSelfData(msgBuff+100,msgLen);
					
					break;
				default:
				{
					
					break;
				}
			}
		}else
		{			
            LOG_MSG(ERR_LEVEL, "msgQ_Route_Serv rcv msgLen = %d\n",msgLen);
			continue;
		}
	}
}


/* 初始化一跳邻居表*/
List* Init_1HopNb_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* 初始化多跳邻居表*/
List* Init_mHopNb_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}


/* 初始化域间路由表 */
List* Init_IERP_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* 初始化APP_BUFF报文缓冲区*/
List* Init_APP_Buff()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* 初始化RREQ记录表*/
List* Init_RREQ_Record_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* 比较两个rreq_seq或bc_seq的新旧，不同于大小.且x必须是新来报文的序号，y是记录
	x新,返回1；
	相等,返回0；
	x旧,返回-1.

    理论上，x，y 不相等，就应该转发。但是网络中之前残存的广播报文，仍有可能再次被接收。此时，若仅判是否相等，则会造成额外转发。
    此函数只用于广播业务包，不用于RREQ。因为广播业务包发送速度快，网络内残存报文较多，RREQ发送频度小，不可能残存报文。
    
    
	x=y时，相等；
    x>y时，则x新;
    x<y时，理论上是y新，但是为了消除网络上仍在广播的报文的影响，若x经历了最大值反转，这x新；否则，y新
	65535+x-y<32时：经过了最大值翻转，且x新； 
	65535+y-x<32时，经过了最大值翻转，且x旧； 
	其它情况时，没有经过最大值翻转
		|x-y|<32 时，x>y，则x新，x<y时，x旧
		|x-y|>32 时，总是x新。即100比300新。这里默认网络内不会同时存在100和300的包。
						【注，此处比较体现了x,y的顺序不可颠倒。即如果报文序号与本地记录相差过大，则总认为报文是新的】
*/
int Seq_Cmp(unsigned short x, unsigned short y)
{
	if(x == y)
		return 0;
	else if (65535+x-y<32)	
		return 1;
	else if (65535+y-x<32)
		return -1;
	else if(x-y >0 && x-y< 32)
		return 1;
	else if(y-x >0 && y-x< 32)
		return -1;
	else
		return 1;
}	

/* Get_Next_ID()  ----- 从trace_list中提取 id 的下一跳ID。没找到或输入出错，返回0 
 */
unsigned short Get_Next_ID(unsigned short *trace_list, unsigned short id)
{
	int	i;

	if (id == 0 || trace_list == NULL)
		return 0;
		
	for(i = 0; i< MAX_HOP; i++)
	{
		if(trace_list[i] == id)
		{
			break;
		}	
	}
	if (i>=MAX_HOP-1)
		return 0;
	else
		return trace_list[i+1];
}

/* Get_Prev_ID()  ----- 从trace_list中提取 id 的上一跳ID。没找到或输入出错，返回0 
 */
unsigned short Get_Prev_ID(unsigned short *trace_list, unsigned short id)
{
	int	i;
	
	if (id == 0 || trace_list == NULL)
		return 0;
		
	for(i = 0; i< MAX_HOP; i++)
	{
		if(trace_list[i] == id)
		{
			break;
		}	
	}
	if (i>=MAX_HOP || i == 0)
		return 0;
	else
		return trace_list[i-1];
}

/* 根据输入的一跳和多跳邻居表，将radius跳内的邻居信息，填入nb_list。 最多填入MAX_NB_NUM条信息，其它信息不填，超出系统容量
	如果radius ==1，只需要查找一跳邻居表
	如果radius >1, 还需要查找多跳邻居表
	返回值：radius跳内的邻居个数，含一跳单向邻居
*/
unsigned char Pad_LSA_Nb_List(NB_INFO *nb_list, unsigned char radius, List* onehop_list, List* mhop_list)
{
	int					n,i;
	ONE_HOP_NB_ITEM		*element_ptr1;
	MULTI_HOP_NB_ITEM	*element_ptr2;
	unsigned char		num;
	
	unsigned short	current_node_id, prev_node_id;
	if(radius == 0)
		return 0;
	
	num = 0;
	if(radius >= 1)
	{
		// 查onehop_list，填写一跳邻居信息	
		n = prg_list_size(onehop_list);
		
		for(i = 1; i<= n; i++)
		{
			element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i);
			
			nb_list[num].node_ID = htons(element_ptr1->node_ID);	
			nb_list[num].path_QOS = element_ptr1->rcv_qos;					// [注] LSA 的nb_list中，一跳邻居封的是rcv_qos,多跳邻居封的是最佳path_QOS
			nb_list[num].mpr_distance = (element_ptr1->is_my_mpr <<7)+ (element_ptr1->direction<<6)+ 1;
			
			if(++num >= MAX_NB_NUM )
				return num;
		
		}
		
	}
	// 查mhop_list，填写多跳邻居信息.由于mhop_list是按照hop顺序排列，因此，直接从头遍历即可。第二，由于mhop_list中相同node_ID的项目hop必定相同，且位置紧邻，所有可以按照差异判决

	n = prg_list_size(mhop_list);
	prev_node_id = 0;
	
	for(i = 1; i<= n; i++)
	{
		element_ptr2 = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i);
		if(element_ptr2->hop <= radius)		
		{
			current_node_id = htons(element_ptr2->node_ID);
			if(prev_node_id != current_node_id)		//	差异判决,前后ID不同，表示为新node_ID，记录到nb_list中，否则不记录。
			{
				prev_node_id = current_node_id;
				
				nb_list[num].node_ID = current_node_id;	
				nb_list[num].path_QOS = element_ptr2->path_QOS;
				nb_list[num].mpr_distance = element_ptr2->hop;
				
				if(++num >= MAX_NB_NUM )
					return num;
			}
			else if(nb_list[num-1].path_QOS < element_ptr2->path_QOS)		// 若前后node_ID相同，则按照较大的QOS赋值
			{
				nb_list[num-1].path_QOS = element_ptr2->path_QOS;
			}
			
		}
		else
		{
			return num;	
		}
	}

	return num;
	
}
/* *************************************************************************************************
 * 在TCP/UDP协议中，规定IP Header定义如下：
 * 4-bit version | 4-bit header length | 8bit typeof service | 16bit total length in bytes          |
 * 16-bit identification			   					     | 3-bit flags  | 13-bit fragment offset|
 * 8-bit time to live				   | 8-bit protocol		 |    16-bit header checksum			|
 * 32-bit source IP address																			|
 * 32-bit destination IP address																	|
 * options(if any)																					|
 * data	（UDP头+ AppData）
 *
 * *IP头中4-bit header length字段（见如下）代表了IP头多少个4个字节，包括协议中的option(if any)字段。
 * 																						|
 * **************************************************************************************************
 * 在TCP/UDP协议中，规定UDP Header协议如下：
 *
 * 16-bit source port number    | 16-bit destination port number |
 * 16-bit UDP length			| 16-bit UDP checksum			 |
 * data															 |
 * 其中UDP length 包含了8字节UDP头+ UDP data字节
 * ***************************************************************
 * */
/**
 * 创建RAW_SOCKET,截获网卡上的所IP报文，过滤掉不需要的报文，然后转到接收队列，待route处理。
 */
int Is_myPing(unsigned char * ip_pkptr, int len)
{
	unsigned char iphd_len;
	unsigned char *icmp_pkptr;
	int			icmp_len;
	
	unsigned long addr;
	unsigned short cksum;
	
	
	APP_PK	app_buff;
	APP_PK *app_pkptr = &app_buff;
	
	iphd_len = (ip_pkptr[0]&0x0f)<<2;
	
	icmp_len = len - iphd_len;
	icmp_pkptr = ip_pkptr + iphd_len;
	
	/*  满足下列3个条件的，是待回复的ping.
		1. IP头内协议类型 = 1
		2. 目的IP是本地WiBox上的IP， 即目的ID是自身，且IP最低位是1或2
		3. IP数据第一个字节 = 8，第二个字节 = 0
	*/
	if (	getProto_fromIPHeader(ip_pkptr) == 1
			&& getDestAddr_fromIPHeader(ip_pkptr) == Node_Attr.ID && (ip_pkptr[19] == 1 || ip_pkptr[19] == 2))
	{
		if (icmp_pkptr[0] == 8 && icmp_pkptr[1] == 0)
		{
			/* 返回ping_reply: 颠倒源和目的IP，frag和offset强制为0，重新计算ip头checksum; ICMP第一个字节改为0，重新计算整个报文的checksum,--不做(不足60字节缀0补齐)*/
		
			addr = *(unsigned long*)(ip_pkptr+12);		// store src_ip
			*(unsigned long*)(ip_pkptr+12) = *(unsigned long*)(ip_pkptr+16);	// swap src and dst ip
			*(unsigned long*)(ip_pkptr+16) = addr;
					
			*(unsigned short*)(ip_pkptr+6) = 0;		// force set frag_flag and offset to 0
			
			*(unsigned short*)(ip_pkptr+10) = 0;	// set cksum to 0 for recalculation
					
			cksum = checkSum(ip_pkptr, 20);
			*(unsigned short*)(ip_pkptr+10) = cksum;
			
				
			icmp_pkptr[0] = 0;
			icmp_pkptr[1] = 0;		
			*(unsigned short*)(icmp_pkptr+2) = 0;
			
			cksum = checkSum(icmp_pkptr, icmp_len);
			*(unsigned short*)(icmp_pkptr+2) = cksum;

			// small ping pk, directly reply; huge ping pk, just reply a fragment.
			memcpy(app_pkptr->data, ip_pkptr, len);
			APP_PK_Send((unsigned char*)app_pkptr, len + ROUTE_HEAD_LEN);
		}
		else
		{
			LOG_MSG(DBG_LEVEL, "huge ping frags, not reply\n");
		}
	
		return 1;
	}
	else
	{
		return 0;
	}
}

/* Is_1hopNb() --- 判断id是否为自身的一跳邻居，是双向邻居返回2，单向邻居返回1，不是邻居返回0*/
int Is_1hopNb(List* onehop_list, unsigned short id, int *index)
{
	int				i = 1;
	ONE_HOP_NB_ITEM	*element_ptr;
	
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		if(element_ptr->node_ID == id )
		{
			if (index != NULL)
				*index = i;
				
			if(element_ptr->direction == 1)	
				return 2;
			else
				return 1;			
		}
		
		i++;		
	}

	if (index != NULL)
		*index = -1;
		
	return 0;
	
}
/* 先根据rcv_state计算出rcv_qos*/
unsigned char Get_Qos(unsigned short rcv_state)
{
	unsigned char rcv_qos = 0;
	int		i;

	for(i = 0; i< 16; i++)
	{
		if (((rcv_state << i) & 0x8000 ) == 0x8000)
			rcv_qos++;
	}
	

	return rcv_qos;
}
/* Get_1hopNb_Num() ---- 获取一跳内的双向邻居数 */
int Get_1hopNb_Num(List* onehop_list)
{
	int				i = 1;
	int				num = 0;
	ONE_HOP_NB_ITEM	*element_ptr;
	
	// 删除所有以id为node_ID的一跳邻居表项
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		if(element_ptr->direction == 1)
		{
			num++;			
		}	
			
		i++;	
	}

	return num;	
}

/** Del_1HopNb_Item() -- 删除一跳邻居表中所有node_ID == id的表项。
	返回值：成功删除的表项个数
*/
int Del_1HopNb_Item(List* onehop_list, unsigned short id)
{
	int				i = 1;
	int				num = 0;
	ONE_HOP_NB_ITEM	*element_ptr;
	
	// 删除所有以id为node_ID的一跳邻居表项
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		if(element_ptr->node_ID == id )
		{
			prg_list_remove(onehop_list,i);
			num++;			
		}
		else
		{
			i++;
		}
	}

	return num;
}



/** Add_1hopNb_Item() -- 在1跳路由表中增加一条表项。此操作仅在收到LSA报文后才能发生，已经调用Update_Nb_Tab，完成更强大的操作
	
*/


/* Get_MprDistance  -- 在NB_INFO列表中，查找地址为id的节点的mpr_distance，并返回该值。查找失败返回0 */
unsigned char Get_MprDistance(NB_INFO *nb_list, int	nb_num, unsigned short id, unsigned char *path_qos)
{
	int		i;

	for(i = 0; i< nb_num; i++)
	{
		if(id == nb_list[i].node_ID)
		{
			if(path_qos != NULL)
			{
				*path_qos = nb_list[i].path_QOS;		/*nb_list[i]记录了本节点发送的LSA在 到“LSA发送者”的接收情况*/
			}
			//LOG_MSG(DBG_LEVEL, "rcvd LSA,it's nb_num(%d), I'Node_id=%d,  I'send LSA it's recvd =%d\n", nb_num,id, *path_qos );
			return nb_list[i].mpr_distance;
		}	
	}
	if(path_qos != NULL)
		*path_qos = 0;

	//LOG_MSG(DBG_LEVEL, "rcvd LSA nb_num(%d), I'Node_id=%d,  I'send LSA recvd =%d\n", nb_num,id, *path_qos );

	return 0;
}

unsigned char Get_Path_QOS(unsigned short id)
{
    int				i = 1;
	ONE_HOP_NB_ITEM	*element_ptr;
	unsigned char  path_qos = 0;
	
	if(id == Node_Attr.ID)
        return 16;
    
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
	{
		if(element_ptr->node_ID == id )
		{
			path_qos = element_ptr->path_QOS;
			
			return 	path_qos;		
		}
		
		i++;		
	}
	
	return 0;
}

/** Del_mHopNb_Item() -- 删除多跳路由表中所有relay_ID == relay_id, node_ID == node_id的表项。
	返回值：删除的多跳邻居表条目数
*/
int Del_mHopNb_Item(List* mhop_list, unsigned short node_id, unsigned short relay_id)
{
	int					i = 1;
	int					n=0,num = 0;
	MULTI_HOP_NB_ITEM	*element_ptr;
	
	// 删除所有以relay_id为relay_Id的多跳邻居表项
	while(NULL != (element_ptr = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i)))
	{
		if((element_ptr->node_ID == node_id || node_id == 0xffff) && (element_ptr->relay_ID == relay_id || relay_id == 0xffff))
		{
			prg_list_remove(mhop_list,i);
			num++;			
		}
		else
		{
			i++;
		}
	}

	return num;

}

/** Add_mHopNb_Item() -- 在多跳路由表中增加一条表项。注意按既定顺序规则加入链表。调用之前，已经先将相同relay_ID的清空一次，保证没有重复项	
	返回值：1成功，0失败
*/
int Add_mHopNb_Item(List* mhop_list, unsigned short node_id, unsigned short relay_id, unsigned char path_qos, unsigned char hop)
{
	MULTI_HOP_NB_ITEM	*new_elem_ptr;
	MULTI_HOP_NB_ITEM	*element_ptr;
	int					i = 1;
	
	// 1. 若node_id为自身，则直接返回0，不添加
	if(node_id == Node_Attr.ID)
		return 0;
	
	// 2. 若node_id为双向一跳邻居，则直接返回0，不添加
	if(Is_1hopNb(oneHopNb_List, node_id, NULL )>1)
		return 0;
		
	// 3. 遍历整个多跳邻居表，寻找记录中node_ID相同的表项，并比较hop
	
	i = 1;
	while(NULL != (element_ptr = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i)))
	{
		if(element_ptr->node_ID == node_id ) 
		{
			if(element_ptr->hop < hop)			// 3.1 记录表中hop较小，则不添加，返回0
			{
				return 0;
			}
			else if(element_ptr->hop == hop)	// 3.2 记录表中hop相等，若relay_id相同，则不添加，返回0； 否则，准备添加
			{
				if(element_ptr->relay_ID == relay_id)
					return 0;
				else
					i++;
			}
			else								// 3.3 记录表中hop较大，则删除该表项，准备添加新表项
			{
				prg_list_remove(mhop_list,i);
			}
		}
		else
		{
			i++;
		}	
	}
	
	// 4. 第一遍查找完毕，并完成必要的删除操作。接下来确定要添加新表项，给新表项赋值。
	new_elem_ptr = (MULTI_HOP_NB_ITEM*)malloc(sizeof(MULTI_HOP_NB_ITEM));
	new_elem_ptr->node_ID = node_id;
	new_elem_ptr->relay_ID = relay_id;
	new_elem_ptr->path_QOS = path_qos;
	new_elem_ptr->hop = hop;
	
		
	// 5. 确定插入位置：顺序查找hop相同且relay_ID也相同的表项：若存在，则插到该表项之后；若不存在，则插到该hop值的最后一项的后面
	i = 1;
	while(NULL != (element_ptr = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i)))
	{
		if(element_ptr->hop < hop)	
		{
			i++;	
		}
		else if(element_ptr->hop == hop)	
		{
			// 存在hop相同且relay_ID也相同的表项，则插到该表项之前
			if(element_ptr->relay_ID == relay_id)
			{
				prg_list_insert(mhop_list,new_elem_ptr,i);
				return 1;	
			}
			else
				i++;
		}
		else		
		{	
			// 不存在，插到该hop值的最后一项的后面
			prg_list_insert(mhop_list,new_elem_ptr,i);
			return 1;
		}
					
	}
	
	// 6. 该hop值在多跳邻居表内最大，已不存在更大的项，则不会在else 内退出，加到链表末尾
	prg_list_insert(mhop_list,new_elem_ptr,LIST_POS_TAIL);
	return 1;
	
}

/* Del_IERP_Item_By_Seq -- 根据rreq_src_id和rreq_seq，删除或更新域间路由。返回删除的条目个数*/
int Del_IERP_Item_By_Seq(List* ierp_list, unsigned short rreq_src_id, unsigned short rreq_seq)
{
	int				i = 1;
	int				num = 0;
	IERP_ITEM		*element_ptr;
	
	while(NULL != (element_ptr = (IERP_ITEM*)prg_list_access(ierp_list, i)))
	{
		if(element_ptr->rreq_src_ID == rreq_src_id && element_ptr->rreq_seq == rreq_seq )
		{
			prg_list_remove(ierp_list,i);
			num++;			
		}
		else
		{
			i++;
		}
	}

	return num;
}
/* Del_IERP_Item_By_NextID -- 删除所有以next_id为next_Id的域间路由*/
int Del_IERP_Item_By_NextID(List* ierp_list, unsigned short next_id)
{
	int				i = 1;
	int				num = 0;
	IERP_ITEM		*element_ptr;
	
	while(NULL != (element_ptr = (IERP_ITEM*)prg_list_access(ierp_list, i)))
	{
		if(element_ptr->next_ID == next_id)
		{
			prg_list_remove(ierp_list,i);
			num++;			
		}
		else
		{
			i++;
		}
	}

	return num;
}

/* Del_IERP_Item_By_DestID -- 删除所有以dest_id为dest_ID的域间路由*/
int Del_IERP_Item_By_DestID(List* ierp_list, unsigned short dest_id)
{
	int				i = 1;
	int				num = 0;
	IERP_ITEM		*element_ptr;
	
	while(NULL != (element_ptr = (IERP_ITEM*)prg_list_access(ierp_list, i)))
	{
		if(element_ptr->dest_ID == dest_id)
		{
			prg_list_remove(ierp_list,i);
			num++;			
		}
		else
		{
			i++;
		}
	}

	return num;
}

/** Del_1hopNb() -- 删除一个1跳邻居，并作其它善后处理：
				删除所有以id为node_ID的一跳邻居表项
				若成功删除，则
					删除所有以id为relay_ID的多跳邻居表项
					删除所有以id为next_ID的域间路由表项。
	返回值：成功删除的一跳邻居表项个数
*/
int Del_1hopNb(List* onehop_list, List* mhop_list, List *ierp_list,  unsigned short id)
{

	int		num = 0;

	num = Del_1HopNb_Item(onehop_list, id);
	
	if(num > 0)
	{
		Del_mHopNb_Item(mhop_list, id, 0xffff);
		Del_IERP_Item_By_NextID(ierp_list, id);
	}
	
	return num;
}

/* 删除一跳邻居表中记录超时的邻居，即last_lsa_rcv_time与当前系统时间之差大于4*PERIOD_LSA的记录，并作善后操作. 
    同时，更新rcv_state
	返回值：删除的一跳邻居的个数
*/
int	Del_Timeout_1hopNb(List* onehop_list, List* mhop_list, List *ierp_list)
{
	int					i = 1;
	int					num = 0;
	ONE_HOP_NB_ITEM		*element_ptr;
	long long  diff;

	//printf("**********************************************************************************************current = %lld\n",sysTimeGet());
	/*
	 * 遍历一跳邻居列表所有节点，统计其收到的各个节点LSA的状态，更新rcv_QOS值
	 * */
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		diff = sysTimeGet() - element_ptr->last_lsa_rcv_time ;
		//printf("**********************************************************************************************diff = %lld\n",diff);
		if( diff >  (RETRY_TIMES*PERIOD_LSA) )
		{
			LOG_MSG(INFO_LEVEL, "delete timeout node %d(dir =%d)\n",element_ptr->node_ID,element_ptr->direction);
									
			// 删除所有以id为relay_ID的多跳邻居表项					
			Del_mHopNb_Item(mhop_list, 0xffff, element_ptr->node_ID);
			
			// 删除所有以id为next_ID的域间路由表项。
			Del_IERP_Item_By_NextID(ierp_list, element_ptr->node_ID);
            
            // 删除超时的一跳邻居表项. 必须先删上面的，否则element_ptr指向为空，后续删除不掉
			prg_list_remove(onehop_list,i);
			num++;
		}
		else
		{
			/*
			 * WJW:
			 * 在发送LSA前，更新所有邻居的rcv_qos，若距离上次接收时间差>PERIOD+50,直接左移1位，否则移位后+1。
			 * */

#if 0
			//if (diff > (PERIOD_LSA + rand_t1_max *3.0) )
			if (diff > (PERIOD_LSA+ 100) )
			{
				// 丢LSA, 更新rcv_state，path_QOS
				printf("diff > PERIOD_LSA,  nb_node %d, rcv_state = %04x,\n",element_ptr->node_ID ,element_ptr->rcv_state);

				/*
				 * WJW：
				 * 某个节点LSA在一个LSA周期超时没有收到，则链路在限定的时间内中断或异常一次，rcv_qos “1”的个数减1，然后
				 * 通过本节点以LSA报文通告其它节点，
				 * */
				element_ptr->rcv_state = element_ptr->rcv_state<<1;
				element_ptr->rcv_qos = Get_Qos(element_ptr->rcv_state);
				element_ptr->path_QOS = (element_ptr->rcv_qos > element_ptr->send_qos)?element_ptr->send_qos:element_ptr->rcv_qos;
			}
			else
			{
				// LSA正常，更新rcv_state，path_QOS
				/*
				 * WJW：
				 * 某个节点LSA在一个LSA周期内收到，则链路正常，rcv_qos “1”的个数加1，然后
				 * 本节点以LSA报文通告其它节点，
				 * */
//				printf("diff < PERIOD_LSA+ 2.0*rand_t_max,  nb_node %d , rcv_state = %04x, ",element_ptr->node_ID ,element_ptr->rcv_state);
				element_ptr->rcv_state = (element_ptr->rcv_state<<1) +1;

				//统计rcv_state中1的个数
				element_ptr->rcv_qos = Get_Qos(element_ptr->rcv_state);
				element_ptr->path_QOS = (element_ptr->rcv_qos > element_ptr->send_qos)?element_ptr->send_qos:element_ptr->rcv_qos;
			}
#else
				element_ptr->rcv_state = (element_ptr->rcv_state<<1) +1;
				//统计rcv_state中1的个数
				element_ptr->rcv_qos = Get_Qos(element_ptr->rcv_state);
				element_ptr->path_QOS = (element_ptr->rcv_qos > element_ptr->send_qos)?element_ptr->send_qos:element_ptr->rcv_qos;

#endif

			i++;
		}
	}
	
	return num;
}

/* 判断某id是否在数组的前num个元素中
	返回：在数组内，返回1；不在，返回0
	注：num > MAX_NB_NUM时，恒返回1，方便不屏蔽的情况
*/
int	Is_inList(unsigned short id, unsigned short *id_list, int num)
{
	int	i;
	if(num > MAX_NB_NUM)
		return 1;
		
	for(i = 0; i< num; i++)
	{
		if(id == id_list[i])
			return 1;	
	}
	return 0;
}
/** Get_2HopNb_Num() 计算以relay_list为relay_ID的两跳邻居点的个数
	返回：个数
*/
int	Get_2HopNb_Num(List* mhop_list, unsigned short *relay_list, int num)
{
	int					i = 1;
	int					ret=0;
	MULTI_HOP_NB_ITEM	*element_ptr;
	
	memset(tempBool,0,sizeof(tempBool));
	
	while(NULL != (element_ptr = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i)))
	{
		if(element_ptr->hop == 2)
		{
			if(Is_inList(element_ptr->relay_ID, relay_list, num)) 
			{
				// relay_ID在relay_list列表内，该node_ID即为2跳邻居，并记录
				if(tempBool[element_ptr->node_ID] ==0)
				{
					tempBool[element_ptr->node_ID] = 1;	
					ret++;
				}
			}
		}
		else
		{
			// 由于mhop_list按hop升序排列，所有发现hop超过2时，可以直接终止搜索
			break;	
		}
		i++;
	}
		
	return ret;

}

/*  判定mask按二进制展开后，其中'1'的个数是否为relay_num。
		将mask按二进制展开，统计其'1'的个数；
		若==relay_num，则返回1；否则返回0
*/
int Check_Mask_By_RelayNum(unsigned int mask, int relay_num)
{
	unsigned char num =0;

	while(mask)
	{		
		if((mask&1) == 1)
			num++;

		mask = mask>>1;
	}
	
	return (num == relay_num);
}

/*  根据mask，从old_relay_list中抽取relay_num个元素，作为new_relay_list.返回值为 relay_num
		若mask中'1'的个数 == relay_num，以其二进制展开式中'1'的位置作为索引，从old_relay_list中抽取relay_num个元素，作为new_relay_list
		否则，返回0*/
int Get_New_RelayList_By_Mask(unsigned int mask, int relay_num, unsigned short *old_relay_list,unsigned short *new_relay_list)
{
	unsigned char	i=0, num =0;
	
	if(Check_Mask_By_RelayNum(mask, relay_num))
	{
		while(mask)
		{	
			if((mask&1) == 1)
			{
				// 二进制展开的第i个位置为'1'，给new_relay_list赋值
				new_relay_list[num++] = old_relay_list[i];	
			}

			i++;
			mask = mask>>1;
		}	
		
		return relay_num;
	}
	else
	{
		return 0;	
	}
}

/* 根据mask给onehop_list->is_my_mpr赋值。mask为'1'的位，is_my_mpr置1. 注意，扣除单向邻居*/
void Set_Mpr_By_Mask(List* onehop_list, unsigned int mask)
{
	unsigned char	i=1;
	ONE_HOP_NB_ITEM	*element_ptr;
	
    while( NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
    {
        if(element_ptr->direction == 1)
        {
            if((mask&1) == 1)
            {
                // 二进制展开的第i个位置为'1'，给onehop_list的第i个元素的is_my_mpr赋值
                element_ptr->is_my_mpr = 1;
            }
            else
            {
                element_ptr->is_my_mpr = 0;	
            }
            
            mask = mask>>1;
            
        }
        else
        {
            element_ptr->is_my_mpr = 0;
        }
        
        i++;
    }


}

/** 

	关于MPR节点有两种操作：
	   1. 我的MPR是谁？在发送LSA报文时，对这些节点属性进行注明。
	   2. 我是谁的MPR？通过解析邻居发来的LSA报文，解析并记录我是不是某节点的MPR
*/

/* Update_myMPR 
 *  根据一跳邻居表和多跳邻居表，计算本地的MPR节点集，并更新一跳邻居表：将对应邻居节点元素中的is_my_mpr置0(不是我的MPR)或1(是我的MPR)
 *    算法中可能需要将邻居表中的部分元素提取出来，组成一种更适合计算的结构
 *  返回值：myMPR的个数
 */

/*
 * wjw;
 * OLSR路由协议是一种基于链路状态算法的表驱动协议，OLSR使用逐跳路由，即每个节点使用其本地信息为分组
 * 选择传输路由。在OLSR中，每个节点通过周期性地发送自己拓扑信息达到路由表更新地作用。网络中每个节点从1跳邻居节点中
 * 选择部分节点作为MPR（MultiPoint Relay, 多点中继），只有被选为MPR地节点才负责转发控制信息，
 * 并且MPR还要负责向网络中报告当前地链路状态。
 * MPR提供一种高效地控制消息泛洪机制，减少了所要求的传输量。
 * */

int Update_myMPR(List* onehop_list, List* mhop_list)
{
	/* 
		1. 先根据onehop_list，赋值为一个一跳邻居ID数组(必须是双向邻居)，便于后续操作
		2. 遍历多跳邻居表，不作任何屏蔽，计算两跳邻居点的个数twohop_NUM;
		3. 按从小到大的顺序给relay_list赋值，计算两跳邻居点的个数twohop_num；
		4. 当num == Nnum时，该relay_list就是MPR集，更新onehop_list并退出.
	*/
	int				i = 1;
	unsigned int	j;
	int				onehop_num = 0, twohop_NUM, twohop_num, relay_num;
	ONE_HOP_NB_ITEM	*element_ptr;
	unsigned short		relay_list[MAX_NB_NUM];
	unsigned short		new_relay_list[MAX_NB_NUM];
	
	unsigned int	relay_mask;
	
	// 1. 先根据onehop_list，赋值为一个一跳邻居ID数组(必须是双向邻居)，便于后续操作
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		if(element_ptr->direction == 1)
		{
			relay_list[onehop_num++]=element_ptr->node_ID;	
		}	
			
		i++;	
	}
	
	// 2. 遍历多跳邻居表，不作任何屏蔽，计算两跳邻居点的个数twohop_NUM;
	twohop_NUM = Get_2HopNb_Num(mhop_list, relay_list, MAX_NB_NUM+1 );
	
	if(twohop_NUM == 0)
	{
		Set_Mpr_By_Mask(onehop_list, 0);
		return 0;
	}
	// 3. 按从少到多的顺序给relay_list赋值，计算两跳邻居点的个数twohop_num；若(twohop_num == twohop_NUM)，返回，并按照此时的掩码给一跳邻居my_MPR项赋值
	relay_num = (onehop_num >32)?32:onehop_num;
	relay_mask = pow(2,relay_num);	
    
	for(i = 1; i<= relay_num; i++)
	{
		// 3.1 第一重优化：mask不再逐1递增，二是根据数值特征，从1开始，按2^n递增
		for(j = 1; j< relay_mask; j++)
		{
			if(Get_New_RelayList_By_Mask(j,i,relay_list, new_relay_list))
			{
				twohop_num = Get_2HopNb_Num(mhop_list, new_relay_list, i);
                
				if(twohop_num == twohop_NUM)
				{
                    /*
                    LOG_MSG(INFO_LEVEL, "2hopN=%d, new_2hopN=%d, new_mask=%d\n",twohop_NUM,twohop_num,j);
                    LOG_MSG(INFO_LEVEL, "relayN=%d, [%d,%d,%d]\n",relay_num, relay_list[0],relay_list[1],relay_list[2]);
                    LOG_MSG(INFO_LEVEL, "new_relayN=%d, [%d,%d,%d]\n",i,new_relay_list[0],new_relay_list[1],new_relay_list[2]);
                    */
                    
					Set_Mpr_By_Mask(onehop_list, j);
					return i;
				}
			}
		}
	}
	Set_Mpr_By_Mask(onehop_list, relay_mask-1);
	return relay_num;
}

/* Is_yourMPR() 
 *  遍历一跳邻居表，判断我是不是某个节点的MPR，是则返回1，不是返回0。
	为了适应DSR路由，IARP_radius = 0 时，也应返回1.
 *
 */
int	Is_yourMPR(List* onehop_list, unsigned short id, unsigned char *path_qos)
{
	int				i = 1;
	ONE_HOP_NB_ITEM	*element_ptr;
	
	
		
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		if(element_ptr->node_ID == id )
		{
			if(path_qos != NULL)
				*path_qos = element_ptr->path_QOS;
			
			if(Node_Attr.IARP_radius == 0)
				return 1;
		
			if(element_ptr->is_your_mpr == 1)	
				return 1;
			else
				return 0;			
		}
		
		i++;		
	}
	if(path_qos != NULL)
		*path_qos  =0;
		
	return 0;
}

/** Update_Nb_Tab ---- 根据收到的LSA报文后，更新一跳邻居表、多跳邻居表(域内路由表)和域间路由表。

	处理：
		1. 查找一跳邻居表中是否有该LSA报文发送者的记录，若有，则更新该记录；若无，则添加一条记录，并更新。
		2. 前面的字段直接从收到的LSA报文中提取赋值，并查找其 nb_list中(mpr_distance&0x0F) == 1 的ID列表中是否包含自身ID：
			若包含，则direction置1；
				若mpr_distance == 1，则将is_your_mpr置0；
				若mpr_distance == 0x81，则将is_your_mpr置1；
                删除所有以send_ID为 目的ID的多跳邻居 和 IERP 表项。
				更新所有以send_ID为relay_ID的多跳邻居表项；（先全删，再根据nb_list逐个添加）
			否则，将direction置0.
				将is_your_mpr置0；
				将is_my_mpr置0;
				删除所有以send_ID为relay_ID的多跳邻居表项
				删除所有以send_ID为next_ID的域间路由表项
		3. 将last_lsa_rcv_time赋值为当前系统时间
		4. 调用 Update_myMPR()计算本地的MPR节点集，并更新一跳邻居表中的 is_my_mpr字段

*/
void Update_Nb_Tab(LSA_PK *lsa_pkptr, unsigned short lsa_pklen)
{

	unsigned short	send_id;			// 发送该LSA报文的ID
	
	unsigned char	nb_num;				// 域内邻居个数,包含N跳邻居，根据设置处理
	NB_INFO			nb_list[MAX_NB_NUM];			// 域内邻居列表,最多有250个，占据1000字节
	
	int				i = 1;

	ONE_HOP_NB_ITEM	*new_elem_ptr;
	
	unsigned char	mpr_distance, hop, dir;
	unsigned char	qos;
	
	send_id = ntohs(lsa_pkptr->send_ID);
	nb_num = lsa_pkptr->nb_num;

	for(i = 0; i<nb_num; i++ )
	{
		nb_list[i].node_ID = ntohs(lsa_pkptr->nb_list[i].node_ID);	
		/*
		 * nb_list[].path_QOS记录了LSA发送者收到其它各节点的链路状态rcv_QOS值
		 * */
		nb_list[i].path_QOS = lsa_pkptr->nb_list[i].path_QOS;	
		nb_list[i].mpr_distance = lsa_pkptr->nb_list[i].mpr_distance;	
	}

	// 1. 查找一跳邻居表中是否有该LSA报文发送者的记录
	if (Is_1hopNb(oneHopNb_List, send_id, &i) > 0)
	{
		// 有记录，直接更新
		/*WJW：
		 * 1》若一跳邻居表中有LSA_Sender的记录,则先获取对应记录项,i代表下表值
		 * */
		new_elem_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i);
        

		/*
		 * WJW:
		 * 2> 查找本节点在LSA_Sender中的邻居状态,方法是根据Node_Attr.Id查找nb_list;
		 * 	  此外更新一跳邻居表中本节点-> LAS_Sender链路的SendQos, 方法是查找nb_list,
		 * 	  获得接收本节点LSA的次数。
		 * 	  最后返回mpr_distance
		 *
		 *
		 * 	  typedef struct{
				unsigned short	node_ID;			// 节点ID，即邻居节点ID
				unsigned char	path_QOS;			// 节点质量，LSA报文中send_ID到node_ID的链路质量。
				unsigned char	mpr_distance;		// 组合字段，bit7：是否为LSA报文发送者的MPR节点，0不是，1是,只有一跳邻居才有可能是MPR；
                                        			//           bit6: 是否为LSA报文发送者的双向邻居，0不是，1是。只有一跳邻居才有可能是1
													//			bit3~0:到LSA报文发送者的跳数，表示为几跳邻居

				}NB_INFO;						// 邻居列表信息，用于填充报文内容,从邻居表中读取
		 *
		 *
		 *
		 *
		 * */
        // 根据LSA报文中的nb_list, 确定mpr_distance。进而更新NB信息
		/*
		 * WJW：
		 * 由于nb_list[].path_QOS记录了LSA发送者收到其它各节点的链路状态rcv_QOS值
		 * 调用Get_MprDistance（），指定查找“我”本节点在LSA发送者nb_list[]中的记录，返回:
		 * 1> LSA收到的次数，标记为Send_QOS
		 * 2> 链路是否双向可通
		 * 3> 跳数
		 * 4> mpr值
		 * */
        mpr_distance = Get_MprDistance(nb_list, nb_num, Node_Attr.ID, &(new_elem_ptr->send_qos));

        /*
         * 获取mpr_distance &0xF值， 代表本节点到 LSA Sender的跳数
         * */
        dir  = ((mpr_distance&0x0F) == 1)?1:0;
        
        if(new_elem_ptr->direction == 0)    // 原本是单向，为提高连通要求，即使dir==1，若qos太低，也置dir =0;
        {
//        	if(dir == 1 && new_elem_ptr->send_qos >= 6 && new_elem_ptr->rcv_qos >= 5)
        	/*
        	 * WJW:
        	 * 注意new_elem_ptr->send_qos 代表的是Node_Attr.ID->LSA发送者,并且由后者返回来的rcv_QOS
        	 * 注意new_elem_ptr->rcv_qos  代表的是Node_Attr.ID rcv_QOS，在LSA_PK_Send()中统计
        	 * 	本节点收到其它节点LSA的次数，即rcv_qos
        	 * */
            if(dir == 1 && new_elem_ptr->send_qos >= 2&& new_elem_ptr->rcv_qos >= 1)
            {
                // 当rcv_qos=5时又收到了LSA， 1秒以内本节点LSA发送时将会+1变成6. 而send_qos必须是大于6时才可以。
                dir = 1;    
            }
            else
            {
                dir = 0;
            } 
        }
        
        if(new_elem_ptr->direction != dir)
        {
            // 若direction 发生改变，打印
             LOG_MSG(INFO_LEVEL, "node %d change dir from %d to %d(my send_qos=%d, Dude rcv_qos=%d)\n",
                        send_id, new_elem_ptr->direction, dir, new_elem_ptr->send_qos, new_elem_ptr->rcv_qos);
        }
        
	}
	else
	{
		// 无记录，直接添加
		new_elem_ptr = (ONE_HOP_NB_ITEM*)malloc(sizeof(ONE_HOP_NB_ITEM));
		prg_list_insert(oneHopNb_List, new_elem_ptr, LIST_POS_TAIL);

		/*
		 * WJW：
		 * 第一次收到某节点的LSA，则在一跳邻居列表末尾最加，并将
		 * */
		new_elem_ptr->rcv_state = 0x0020;   // 接下来9s内收到4次
		new_elem_ptr->rcv_qos = 1;
        
        // 根据LSA报文中的nb_list, 确定mpr_distance。进而更新NB信息
		/*
		 * WJW:
		 * 第一次收到某节点的LSA，根据LSA携带的nb_list,查找本节点在到其的链路状态，
		 * 结果保存在new_elem_ptr->send_qos和mpr_distance，其中send_qos反映链路质量
		 * */
        mpr_distance = Get_MprDistance(nb_list, nb_num, Node_Attr.ID, &(new_elem_ptr->send_qos)); 
        
        dir = 0;    // 首次收到LSA，qos置为2，仍认为是单向，直至升至4，才认为可通
        
        LOG_MSG(INFO_LEVEL, "add new node %d\n",send_id);
	}

	// 2. 根据LSA报文信息，给新一跳邻居表项赋值, 并更新多跳邻居表和域间路由表
	new_elem_ptr->node_ID = send_id;
	new_elem_ptr->cluster_state = lsa_pkptr->cluster_state;
	new_elem_ptr->cluster_size = lsa_pkptr->cluster_size;
	new_elem_ptr->cluster_header = ntohs(lsa_pkptr->cluster_header);
	

	new_elem_ptr->degree = lsa_pkptr->degree;
	new_elem_ptr->last_lsa_rcv_time = sysTimeGet();
	
	new_elem_ptr->path_QOS = (new_elem_ptr->send_qos > new_elem_ptr->rcv_qos)? new_elem_ptr->rcv_qos: new_elem_ptr->send_qos;
	
	if(dir == 1)
	{
		// 本节点在其一跳邻居表内，则是双向邻居
		new_elem_ptr->direction = 1;
		new_elem_ptr->is_my_mpr = 0;
		
		if((mpr_distance>>7) == 1)		// 本节点是send_id的mpr
			new_elem_ptr->is_your_mpr = 1;
		else							// 本节点不是send_id的mpr
			new_elem_ptr->is_your_mpr = 0;
		
		// 删除所有以send_ID为node_ID的多跳邻居表项。因为到达该send_ID为一跳，应仅在一跳邻居表中
		Del_mHopNb_Item(mHopNb_List, send_id, 0xffff);
        
        // 删除所有以send_ID为dest_ID的IERP表项。因为到达该send_ID为一跳，应仅在一跳邻居表中
		Del_IERP_Item_By_DestID(IERP_List, send_id);
		
		// 更新以send_ID为relay_ID的多跳邻居表项，先删除，再根据nb_list逐个添加
		Del_mHopNb_Item(mHopNb_List, 0xffff, send_id);
				
		for(i = 0; i<nb_num; i++)
		{
            if((nb_list[i].mpr_distance & 0x7F)== 1)        // LSA发送者的单向一跳邻居，略过.双向一跳因为0x41或0xA1
            {
                continue;
            }
            else 
            {
                //取本链路和携带链路QOS的较小值
                qos = (nb_list[i].path_QOS > new_elem_ptr->path_QOS)?new_elem_ptr->path_QOS:nb_list[i].path_QOS;	
                hop = 1+(nb_list[i].mpr_distance&0x0F);
                    
                Add_mHopNb_Item(mHopNb_List,nb_list[i].node_ID, send_id, qos, hop);
                
                // 删除所有以nb_list[i].node_ID为dest_ID的IERP表项。因为该ID在域内，应仅在多跳邻居表中
                Del_IERP_Item_By_DestID(IERP_List, nb_list[i].node_ID);
            }
		}
		
	}
	else
	{
		// 本节点不在其一跳邻居表内，仅是单向邻居，更新一跳邻居表项
		new_elem_ptr->direction = 0;
		new_elem_ptr->is_your_mpr = 0;
		new_elem_ptr->is_my_mpr = 0;
		
		// 删除所有以send_ID为relay_ID的多跳邻居表项
		Del_mHopNb_Item(mHopNb_List, 0xffff, send_id);
		
		// 	删除所有以send_ID为next_ID的域间路由表项
		Del_IERP_Item_By_NextID(IERP_List, send_id);
		
	}
	
	// 3. 根据更新后的一跳和多跳邻居表，计算MPR
	Update_myMPR(oneHopNb_List,mHopNb_List);	
	
}



/* 超时后，若Is_Routing == 1，则删除APP_BUFF中所有目的ID是RREQ_Dest_ID的报文，并上报主机不可达 ICMP报文*/
void RREQ_Timeout()
{
	LOG_MSG(INFO_LEVEL,"RREQ to %d timeout\n",RREQ_Dest_ID);
	Is_Routing = 0;
}

void RREQ_TimeoutHandler(int sig,siginfo_t *si,void *uc)
{
	//RREQ_Timeout();
	
	unsigned char msg[4];
	msg[0] = 1;
	
	msgQ_snd(msgQ_Route_Serv, msg, sizeof(msg),SELF_DATA, NO_WAIT);
	
}




/* 将APP_BUFF中目的ID是dest_ID的记录移出，对路由头赋值后，发给WiFi, 路由头按输入参数赋值
	返回值：移除报文的个数
*/
int App_Buff_Flush(List *app_list, unsigned short dest_id)
{
	int				i = 1;
	int				num = 0;
	BUFF_ITEM		*element_ptr;
	
	while(NULL != (element_ptr = (BUFF_ITEM*)prg_list_access(app_list, i)))
	{
		if(element_ptr->dest_ID == dest_id)
		{
			APP_PK_Send(element_ptr->app_pk, element_ptr->app_len);

			prg_list_remove(app_list,i);
			num++;			
		}
		else
		{
			i++;
		}
	}

	return num;
}

/* 删除APP_BUFF中所有目的节点是dest_ID的报文
	返回值：移除报文的个数
*/
int Del_APP_PK(List* app_list, unsigned short dest_id)
{
	int				i = 1;
	int				num = 0;
	BUFF_ITEM		*element_ptr;

	while(NULL != (element_ptr = (BUFF_ITEM*)prg_list_access(app_list, i)))
	{
		if(element_ptr->dest_ID == dest_id)
		{
			
			prg_list_remove(app_list,i);
			num++;			
		}
		else
		{
			i++;
		}
	}

	return num;
}

int Add_APP_PK(List* app_list, unsigned short dest_id,unsigned char *app_pkptr, unsigned short app_len)
{
	BUFF_ITEM		*element_ptr;
	
	element_ptr = (BUFF_ITEM*)malloc(sizeof(BUFF_ITEM));
	
	element_ptr->record_time = sysTimeGet();
	element_ptr->dest_ID = dest_id;
	element_ptr->app_len = app_len;
	memcpy(element_ptr->app_pk, app_pkptr,app_len);
	
	return prg_list_insert(app_list, element_ptr, LIST_POS_TAIL);

	
}

/** Route_Search() -- 寻路，为APP报头封装提供必要信息。注意，最后如果寻路为域间路由，要更新IERP中的最近访问时间
	输入：dest_id,
	输出：下一跳ID，总跳数，路径列表，RREQ源ID(只在域间寻路时有效)
	返回值：rreq序号。当rreq_seq = 0 时，表示为域内路由，路径列表无效，1~65535为域间、分簇或混合路由,路径列表有效; -1为无路由，查找失败
	*/
int Route_Search(unsigned short dest_id,unsigned short *next_idptr, unsigned short *rreq_src_idptr, unsigned char *hopptr, unsigned short *trace_list)
{
	int					rreq_seq = -1;
	int					i = 1;
	ONE_HOP_NB_ITEM		*element_ptr1;
	MULTI_HOP_NB_ITEM	*element_ptr2;
	IERP_ITEM			*element_ptr3;
	
	unsigned char		qos;
	
	*hopptr = 0;
	
	// 1. 若目的为广播，则直接发送，下一跳也是广播
	if(dest_id == 0xffff)
	{
		*next_idptr = 0xffff;
		*rreq_src_idptr = 0xffff;
		*hopptr = MAX_HOP-1;
		
		return 0; 
	}
	// 目的ID是自己，不发送.APP_PK_Send中调用前已屏蔽。此处无法屏蔽

	// 2. 先找一跳邻居表中node_ID == dest_id 且双向可通的项，若存在，直接返回第一个结果
	i = 1;
	qos = 0;
	while(NULL != (element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
	{
		if(element_ptr1->node_ID == dest_id && element_ptr1->direction == 1)
		{
			if(qos < element_ptr1->path_QOS)		// 找最佳（QOS最大）的路径
			{
				*next_idptr = element_ptr1->node_ID;
				*rreq_src_idptr = 0;		
				*hopptr = 1;
				
				qos = element_ptr1->path_QOS;
			}
			
				
		}
		
		i++;		
	}
	
	if (*hopptr > 0)
		return 0;		// 域内路由
	
	// 3. 再找多跳邻居表中node_ID == dest_id的项，若存在，直接返回第一个结果
	i = 1;
	qos = 0;
	while(NULL != (element_ptr2 = (MULTI_HOP_NB_ITEM*)prg_list_access(mHopNb_List, i)))
	{
		if(element_ptr2->node_ID == dest_id )
		{
			if(qos < element_ptr2->path_QOS)		// 找最佳（QOS最大）的路径
			{
				*next_idptr = element_ptr2->relay_ID;
				*rreq_src_idptr = 0;		
				*hopptr = element_ptr2->hop;
				
				qos = element_ptr2->path_QOS;
			}		
			
		}
		
		i++;		
	}
	if (*hopptr > 0)
		return 0;		// 域内路由
	
	
	// 4. 最后找域间路由表中 dest_ID == dest_id的项，若存在，直接返回第一个结果
	i = 1;
	qos = 0;
	while(NULL != (element_ptr3 = (IERP_ITEM*)prg_list_access(IERP_List, i)))
	{
		if(element_ptr3->dest_ID == dest_id )
		{
			if(qos < element_ptr3->path_QOS && *hopptr <= element_ptr3->hop )		// 找最佳（hop最小的中选QOS最大）的路径
			{
				*next_idptr = element_ptr3->next_ID;
				*rreq_src_idptr = element_ptr3->rreq_src_ID;
				rreq_seq = element_ptr3->rreq_seq;		
				*hopptr = element_ptr3->hop;
				
				memcpy((unsigned char*)trace_list, (unsigned char*)(element_ptr3->trace_list), 2*MAX_HOP);
				
				qos = element_ptr3->path_QOS;
			}

		}
		
		i++;		
	}
	if (*hopptr > 0)
		return rreq_seq;		// 域间路由
	
	return -1;
}

/* 根据rreq_src_id与rreq_dest_id，找RREQ记录表,并返回找到的第一个表项位置，没有则返回-1.
	
	返回：该记录在链表中的位置，没有则返回-1
*/
int Search_RREQ_Record(List* rreq_list, unsigned short rreq_src_id, unsigned short rreq_dest_id)
{
	int				i = 1;
	RREQ_RECORD		*element_ptr;
	
	while(NULL != (element_ptr = (RREQ_RECORD*)prg_list_access(rreq_list, i)))
	{
		if(element_ptr->rreq_src_ID == rreq_src_id && element_ptr->rreq_dest_ID == rreq_dest_id )
		{
			return i;			
		}
		
		i++;		
	}

	return -1;
}


/* 根据接收到的RREQ报文，更新RREQ记录表。
	RREQ记录表不能仅根据rreq_src_ID与seq区分，还要根据rreq_dest_ID,因为一个节点肯定会向多个目的寻路，而不同目的ID的寻路过程，仅比较seq不能确定新旧
	1. 根据src_ID和dest_ID查找RREQ_Record记录表。
	2. 若记录表内无此记录，则新建一块内存，赋值后插入RREQ链表末尾，并返回1。
	3. 若有记录，则按照下述规则更新该记录
			若报文中的的rreq_seq较新，则更新REQ_Record，并返回1。
			若rreq_seq相同，但新报文的hop较小，则更新REQ_Record，并返回1。
								hop相同或较大，并返回0。
			若报文中的的rreq_seq较旧，并返回0。
			
	返回值：0：未发生更新；1：发生更新
    
    注意更新hop,trace_list,path_QOS.加入本节点的影响
*/
int Update_RREQ_Tab(RREQ_PK *rreq_pkptr, unsigned short rreq_pklen)
{
	int				record_pos;
	RREQ_RECORD		*record_ptr;
	int				cmp_res, i;
	
	unsigned short	src_ID, send_ID;			
	unsigned short	dest_ID;		
	unsigned short	rreq_seq;		
	unsigned char	hop;	
    unsigned char   path_qos;
	
	
	// 1. 节点收到后，解析出src_ID,dest_ID,rreq_seq和hop;
	
	src_ID = ntohs(rreq_pkptr->src_ID);
    send_ID = ntohs(rreq_pkptr->send_ID);

	dest_ID = ntohs(rreq_pkptr->dest_ID);
	rreq_seq = ntohs(rreq_pkptr->rreq_seq);

	hop = rreq_pkptr->hop;
	path_qos = Get_Path_QOS(send_ID);
    path_qos = (rreq_pkptr->path_QOS > path_qos)?path_qos:rreq_pkptr->path_QOS;
	// 2. 依据src_ID和dest_ID搜索RREQ_RECORD表，找到第一个记录。

	record_pos = Search_RREQ_Record(RREQ_Record_List, src_ID,dest_ID);	//根据RREQ请求的源节点id和目的id，查找RREQ表项
	record_ptr = (RREQ_RECORD *)prg_list_access(RREQ_Record_List, record_pos);
    
    
	if(record_pos == -1)
	{
		// 3. 若记录表内无此记录，则新建一块内存，赋值后插入RREQ链表末尾，并返回1。
        //LOG_MSG(INFO_LEVEL, "new RREQ record\n");
		record_ptr = (RREQ_RECORD*)malloc(sizeof(RREQ_RECORD));
		
		record_ptr->rreq_src_ID = src_ID;
		record_ptr->rreq_dest_ID = dest_ID;
		record_ptr->rreq_seq = rreq_seq;
		record_ptr->path_QOS = path_qos;
		record_ptr->hop = hop+1;
		for(i = 0; i< hop;i++)
		{
			record_ptr->trace_list[i] = ntohs(rreq_pkptr->trace_list[i]);
		}
		record_ptr->trace_list[hop] = Node_Attr.ID;
		
		prg_list_insert(RREQ_Record_List, record_ptr, LIST_POS_TAIL);
		
		return 1;
	}
	else
	{
		// 4. 若有记录，当报文内的rreq_seq较新，或rreq_seq相等但新报文的跳数较少，或rreq_seq和hop都相等但path_QOS 较大时，
        //              更新RREQ链表的第record_pos个元素,并返回1。
        //LOG_MSG(INFO_LEVEL,"pk/record = seq=[%d, %d], hop=[%d,%d]\n",rreq_seq,record_ptr->rreq_seq,hop,record_ptr->hop-1);
		cmp_res = Seq_Cmp(rreq_seq, record_ptr->rreq_seq);
        
		if(cmp_res != 0 || (cmp_res == 0 && hop < record_ptr->hop-1) 
            || (cmp_res == 0 && hop == record_ptr->hop-1 && path_qos >= record_ptr->path_QOS +2))
		{			
			record_ptr = (RREQ_RECORD*)prg_list_access(RREQ_Record_List, record_pos);
			
			record_ptr->rreq_src_ID = src_ID;
			record_ptr->rreq_dest_ID = dest_ID;
			record_ptr->rreq_seq = rreq_seq;
			record_ptr->path_QOS = path_qos;
			record_ptr->hop = hop+1;        // hop+1 保存
			for(i = 0; i< hop;i++)
			{
				record_ptr->trace_list[i] = ntohs(rreq_pkptr->trace_list[i]);
			}
			
			record_ptr->trace_list[hop] = Node_Attr.ID;     // 末尾加上本地ID 
			
			return 1;
			
		}
		else
		{
			// 5. 其它情况不处理，返回0
			return 0;
		}
	}
	
}

int Check_Cluster_2ZRP()
{
    // 检查自身邻居度是否越线（建簇或拆簇）
	
    if(Node_Attr.cluster_state == CLUSTER_NONE)
    {
        // 2.1 原本是ZRP，degree超上限时，自举为簇头
        if(Node_Attr.degree >= ZRP_TO_CLUSTER_NUM)
        {
            Node_Attr.cluster_state = CLUSTER_HEADER;
            Node_Attr.cluster_size = Node_Attr.degree;
            Node_Attr.cluster_header = Node_Attr.ID;
        }
    }
    else if(Node_Attr.cluster_state == CLUSTER_MEMBER || Node_Attr.cluster_state == CLUSTER_GATEWAY)
    {
        // 2.2 原本是簇成员或网关
        if(Is_1hopNb(oneHopNb_List,Node_Attr.cluster_header, NULL) == 2)
        {
            // 2.2.1 仍与簇头相连，比较自身与原簇头，德高者居之
            if(Node_Attr.degree >= ZRP_TO_CLUSTER_NUM && Node_Attr.ID < Node_Attr.cluster_header)
            {
                Node_Attr.cluster_state = CLUSTER_HEADER;
                Node_Attr.cluster_size = Node_Attr.degree;
                Node_Attr.cluster_header = Node_Attr.ID;
            }
        }
        else
        {
            // 2.2.2 与原簇头断开连接，恢复至ZRP
            Node_Attr.cluster_state = CLUSTER_NONE;
            Node_Attr.cluster_size = 0;
            Node_Attr.cluster_header = 0xffff;	
        }
        
    }
    else if(Node_Attr.cluster_state == CLUSTER_HEADER)
    {
        // 2.3 原本是簇头，若degree低于下限，拆除恢复至ZRP
        Node_Attr.cluster_size = Node_Attr.degree;
        if(Node_Attr.degree <= CLUSTER_TO_ZRP_NUM)
        {
            Node_Attr.cluster_state = CLUSTER_NONE;
            Node_Attr.cluster_size = 0;
            Node_Attr.cluster_header = 0xffff;	
        }
    }
    
    return 1;
}
void LSA_PK_SendHandler(int sig,siginfo_t *si,void *uc)
{
	//LSA_PK_Send();
	// 改为向Route_Serv发消息，不行。发出的消息很大几率无法被接收

	if(	global_running_state == NET_NORMAL_RUNNING)
	{
		unsigned char msg[4];
		msg[0] = 0;

		msgQ_snd(msgQ_Route_Serv, msg, sizeof(msg),SELF_DATA, NO_WAIT);
	//	wdStart(wd_LSA, PERIOD_LSA+(rand()&0x1f),TIMER_ONE_SHOT);
	}
	/*
	 * LSA报文更新周期固定为1秒
	 * */
	wdStart(wd_LSA, PERIOD_LSA,TIMER_ONE_SHOT);
}
/** int	LSA_PK_Send()	-- 封装LSA报文,并通过无线端发送。同时先完成一项附加功能：删除超时邻居
	
	返回值: 发送报文字节数
	处理：
		1. 删除一跳邻居表中last_lsa_rcv_time与当前系统时间之差大于3*PERIOD_LSA的记录，并作善后操作.[由发送触发超时比接收触发可靠。]
		2. 在ONE_HOP_NB_TAB的剩余记录中找双向可通的一跳邻居，其个数即为LSA报文中的degree，其它大部分字段均从NODE_ATTR中直接读取；
		3. 给nb_list的赋值：找到所有IARP_FSR_Radius跳内的邻居，并将其信息填入列表内（包括node_ID，node_qos和mpr_distance）
			3.1 首先填入双向一跳邻居，distance为1，其中ONE_HOP_NB_TAB->is_my_mpr == 1的节点是本节点的MPR，要将mpr_distance的最高位置1；
			3.2 再填入双向两跳邻居：搜索MULTI_HOP_NB_TAB，找到所有distance为2的node_ID，填入报文内；注，具有相同node_ID和distance的条目可能有多个，只填入一个即可。
			3.3 若IARP_radius大于1，依次填入distance不同的node信息。
		4. 更新node_num,并填充CRC。
		5. 调用无线端发送函数，广播发送
		6. 设置定时器，PERIOD_LSA后再次执行LSA_PK_Send
*/
void LSA_PK_Send()
{
	int ret=0;
//	struct msqid_ds ds;

	LSA_PK	send_pk;
	unsigned short *pkptr = (unsigned short *)&send_pk;
	int	len;
	// 1. 删除一跳邻居表中记录超时的邻居，即last_lsa_rcv_time与当前系统时间之差大于3*PERIOD_LSA的记录，并作善后操作
	Del_Timeout_1hopNb(oneHopNb_List, mHopNb_List, IERP_List); 	// 返回删除邻居的个数
    
	// 2. 在ONE_HOP_NB_TAB的剩余记录中找双向可通的一跳邻居，其个数即为LSA报文中的degree，其它大部分字段均从NODE_ATTR中直接读取；
	Node_Attr.degree = Get_1hopNb_Num(oneHopNb_List)+1;
    
    // 3. 更新平面分簇路由状态，防止邻居突然全部关机的问题
    Check_Cluster_2ZRP();
	
	send_pk.pk_type = LSA_PK_TYPE;
	send_pk.degree = Node_Attr.degree;
	send_pk.send_ID = htons(Node_Attr.ID);
	send_pk.cluster_state = Node_Attr.cluster_state;
	send_pk.cluster_size = Node_Attr.cluster_size;
	send_pk.cluster_header = htons(Node_Attr.cluster_header);
	send_pk.node_qos = Node_Attr.node_qos;
	
	Report_Node_Attr();
	// 4. 给nb_list的赋值：找到所有IARP_radius跳内的邻居，并将其信息填入列表内（包括node_ID，node_qos和mpr_distance）


	/*
	 * WJW:
	 *	将本节点的一跳邻居列表信息，以nb_list的形式发给邻居，
	 *	其中携带了所有一跳邻居节点的：
	 *	1》rcv_QoS, （某节点收到后，保存为send_Qos）
	 *	2》mpr\链路是否双向可达\跳数（1）\
	 *
	 * */
	send_pk.nb_num = Pad_LSA_Nb_List(send_pk.nb_list, Node_Attr.IARP_radius, oneHopNb_List, mHopNb_List);
	
	// 5. 填充CRC
	len = LSA_PK_LEN(send_pk.nb_num);
	*(pkptr+(len>>1)-1) = getCRC((unsigned char*)&send_pk,len-2);
	
	// 6. 发给WiFiSndThread的Route控制队列

//	LOG_MSG(INFO_LEVEL,"router Get WLan_Send_CQ id = %d,	ds.msg_qnum=%d \n",msgQ_Wifi_Snd, 	ds.msg_qnum);

//	memset(&ds,0,sizeof(ds));
//	ret = msgctl(msgQ_Wifi_Snd,IPC_STAT,&ds);

//	LOG_MSG(INFO_LEVEL,"before LSA send,ds.msg_qnum=%d \n", 	ds.msg_qnum);

	errno =0;
//	do{
	ret = msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)&send_pk, LSA_PK_LEN(send_pk.nb_num), WLAN_ROUTE_CTRL,0);//NO_WAIT);
//	}while(ret !=0 || errno == EINTR);
	
//	LOG_MSG(LSA_LEVEL,"route insert LSA ,Nb_num %d, ret=%d , errno=%d \n",send_pk.nb_num,ret , errno);

//	memset(&ds,0,sizeof(ds));
//	ret = msgctl(msgQ_Wifi_Snd,IPC_STAT,&ds);

//	LOG_MSG(INFO_LEVEL,"After LSA send,ds.msg_qnum=%d \n", 	ds.msg_qnum);


	// 7. 设置定时器，PERIOD_LSA后再次执行LSA_PK_Send. 	 LSA_PK_SendHandler 中启动
	//wdStart(wd_LSA, PERIOD_LSA+(rand()&0x1f) ,TIMER_ONE_SHOT);
	
	// 8. 上报信息
//	Report_Table();

	nsv_NodeLinks_TableReport();
	
	
}

/**  void LSA_PK_Rcvd()  -- 解析LSA报文，计算更新相关变量，若触发了链路变化条件（下游邻居丢失或变成单向），还需根据情况进行路由维护
	处理：
		1. 解析LSA报文，更新一跳邻居表\多跳邻居表\域内和域间路由表。更新后，一跳表内没有node_ID相同的项，但是多跳表内会经常存在(relay_ID不同)
		2. 先检查自身邻居度是否越线（建簇或拆簇）
			2.1 若当前在平面路由下，且双向一跳邻居超过上限，则建簇，更改Node_Attr中的分簇状态
			2.2 若当前为簇头下，且双向一跳邻居低于下限，则拆簇，更改Node_Attr中的分簇状态
		3. 再检查该邻居分簇状态，是否会影响自身
			3.1 若LSA报文中的分簇状态为簇头，（可能将我拉进簇）
				若其degree大于自身一跳邻居数(相等则ID号小者优先)，则更改Node_Attr中的分簇状态，进入该簇
				否则，不处理
			3.2 若LSA报文中的分簇状态为平面、成员或网关，不处理
		
*/
int LSA_PK_Rcvd(LSA_PK *lsa_pkptr, unsigned short lsa_pklen)
{
	unsigned char	degree;				// 节点度，即双向可达的一跳邻居个数
	
	unsigned char	cluster_state;		// 分簇状态，0为ZRP平面路由；1为普通簇成员，2为网关，3为簇头。
	unsigned char	cluster_size;		// 簇大小，及所有成员数目，仅分簇时有效，ZRP时置为0
	unsigned short	cluster_header;		// 簇头ID，仅分簇时有效，ZRP时置为0xff
	
	unsigned short	send_id;
	// 1. 校验整个报文CRC,校验错误则返回-1
	if (getCRC((unsigned char*)lsa_pkptr, lsa_pklen) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(LSA_LEVEL,"LSA_PK crc err\n");
		return -1;
	}
	send_id = ntohs(lsa_pkptr->send_ID);
	LOG_MSG(LSA_LEVEL, "rcvd LSA from %d\n", send_id);
	
	// 1. 解析LSA报文，更新一跳邻居表、多跳邻居表、域内和域间路由表。
	Update_Nb_Tab(lsa_pkptr, lsa_pklen);
	
    // 2. 若不是双向一跳邻居，不触发簇状态改变
    if (Is_1hopNb(oneHopNb_List, send_id, NULL )<2)
    {
        return 0;
    }
	degree = lsa_pkptr->degree;
	cluster_state = lsa_pkptr->cluster_state;
	cluster_size = lsa_pkptr->cluster_size;
	cluster_header = ntohs(lsa_pkptr->cluster_header);
	
    
	// 2. 检查该邻居分簇状态，是否会影响自身
	if(cluster_state == CLUSTER_HEADER)
	{   
        // 3.1 发送者是簇头
        
		if(Node_Attr.cluster_state == CLUSTER_NONE)
		{
			// 本身是平面路由，则直接入簇。如果是该节点的MPR，则置为簇网关
            if(Is_yourMPR(oneHopNb_List, send_id, NULL))
            {
                Node_Attr.cluster_state = CLUSTER_GATEWAY;
            }
            else
            {
                Node_Attr.cluster_state = CLUSTER_MEMBER;
            }
			
			Node_Attr.cluster_size = cluster_size;
			Node_Attr.cluster_header = cluster_header;
		}
		else if(Node_Attr.cluster_state == CLUSTER_MEMBER || Node_Attr.cluster_state == CLUSTER_GATEWAY)	
		{
			// 本身是簇成员或簇网关，簇头相同时更新簇大小和簇状态
			if(cluster_header == Node_Attr.cluster_header)
			{
				Node_Attr.cluster_size = cluster_size;	
                
                if(Is_yourMPR(oneHopNb_List, send_id, NULL))
                {
                    Node_Attr.cluster_state = CLUSTER_GATEWAY;
                }
                else
                {
                    Node_Attr.cluster_state = CLUSTER_MEMBER;
                }
			}
		}
		else if (Node_Attr.cluster_state == CLUSTER_HEADER)
		{
			// 本身是簇头，则比较节点ID，吸收该簇或被该簇吸收
			if(Node_Attr.ID > cluster_header)
			{
				// 被吸收
                if(Is_yourMPR(oneHopNb_List, send_id, NULL))
                {
                    Node_Attr.cluster_state = CLUSTER_GATEWAY;
                }
                else
                {
                    Node_Attr.cluster_state = CLUSTER_MEMBER;
                }
				
				Node_Attr.cluster_size = cluster_size;
				Node_Attr.cluster_header = cluster_header;
			}
			
		}
	}
    else
    {
        // 3.2 发送者不是簇头。若该发送者刚好是本节点原簇头，则本节点需恢复至ZRP
        if(send_id == Node_Attr.cluster_header)
        {
            Node_Attr.cluster_state = CLUSTER_NONE;
            Node_Attr.cluster_size = 0;
            Node_Attr.cluster_header = 0xffff;	
        }
    }
	
	return 0;
	
}

/** RREQ_PK_Send() -- 源节点封装RREQ_PK，并通过无线端发送. 无复杂处理。注意设置RREQ应答超时即可*/
void RREQ_PK_Send(unsigned short dest_id ) 
{
	RREQ_PK send_pk;
	
	// 1. 封装报文
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));
	
	send_pk.pk_type = RREQ_PK_TYPE;
	send_pk.TTL = MAX_HOP -1 ;
	send_pk.send_ID = htons(Node_Attr.ID);

	//广播发送RREQ
	send_pk.rcv_ID = htons(0xffff);				//广播发送RREQ

	send_pk.src_ID = htons(Node_Attr.ID);


	//RREQ的目的节点id
	send_pk.dest_ID = htons(dest_id);			//RREQ的目的id

	send_pk.rreq_seq = htons(SEQ_INC(RREQ_Seq)); //生成rreq_seq序列号
	
	send_pk.path_QOS = 16;
	send_pk.hop = 1;
	
	send_pk.trace_list[0] = htons(Node_Attr.ID);
	
	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(send_pk)-2);
	
	// 2. 更新本地RREQ表，加入本条记录。以识别邻居转发来的该报文
	Update_RREQ_Tab(&send_pk,sizeof(send_pk));
	
	// 2. LOG 打印
	LOG_MSG(INFO_LEVEL,"send RREQ(%d -> %d, seq=%d)\n",Node_Attr.ID,dest_id,ntohs(send_pk.rreq_seq));
	
	// 3. 发给WiFiSndThread的Route控制队列
	msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)&send_pk, sizeof(send_pk),WLAN_ROUTE_CTRL, NO_WAIT);
	
	// 4. 设置定时器， IERP_RREQ_TIMEOUT 后再次执行 RREQ_Timeout，向Route_Serv发超时消息。若期间收到RREP，则cancel；若没有，则提示寻路失败，删除缓存报文
	wdStart(wd_RREQ, IERP_RREQ_TIMEOUT,TIMER_ONE_SHOT);	
	
}

/** RREQ_PK_Rcvd() -- 收到RREQ_PK后的处理。
	
	处理：
		1. 节点收到后，解析出src_ID,dest_ID;
		2. 调用Update_RREQ_Tab()更新RREQ表；
		3. 若产生了更新，则目的节点反馈RREP，中间节点转发RREQ
		4. 若没产生更新，则不处理。
		
*/
int RREQ_PK_Rcvd(RREQ_PK *rreq_pkptr, unsigned short rreq_pklen)
{
    IERP_ITEM	*ierp_ptr;
    ONE_HOP_NB_ITEM	*element_ptr;
	int             i;
	unsigned short	src_ID;			
	unsigned short	dest_ID;		
	unsigned short	send_ID;
    unsigned short  rreq_seq;
    unsigned char	path_qos;
    
	int				update_res;
    
	
	// 1. 校验整个报文CRC,校验错误则返回-1
	if (getCRC((unsigned char*)rreq_pkptr, sizeof(RREQ_PK)) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"RREQ_PK crc err\n");
		return -1;
	}
	
	// 1. 节点收到后，解析出src_ID,dest_ID;
	src_ID = ntohs(rreq_pkptr->src_ID);
	dest_ID = ntohs(rreq_pkptr->dest_ID);

	send_ID = ntohs(rreq_pkptr->send_ID);
    rreq_seq = ntohs(rreq_pkptr->rreq_seq);
   
    
	// LOG 打印
	LOG_MSG(INFO_LEVEL, "rcvd RREQ(%d -> %d, seq=%d) from %d\n",src_ID, dest_ID, rreq_seq, send_ID);
	

    if(dest_ID == Node_Attr.ID)
    {
        // 2. 若自己为目的节点，返回RREP报文。
		 /* WJW:
		 * 收到RREQ，而且目的节点id是本节点，此时通过调用Is_1hopNb()，判断
		 * 本节点是否与send_ID的双向可通：
		 * 1》若是，则更新RREQ记录表，譬如为新的RREQ，则新建一项记录。
		 * 	  然后发送RREP;最后模拟自己发送RREQ（本节点至 srcId）并收到RREP的处理，在IERP建立域间路由项。
		 * 2》
		 * 包括TTL、中间发送者（Send_id）E的id, 跳数，以及把本节点id追加至trace_list[]末尾，
		 * 最后转发出去。
		 * */

//		if(src_ID == 1)
//		{
//			TokenRing_OK_ACK_Send(1,dest_ID );
//		}

        if(Is_1hopNb(oneHopNb_List,send_ID,NULL) == 2)
        {
            
            update_res = Update_RREQ_Tab(rreq_pkptr, rreq_pklen);
            if(update_res == 1)	
            {
                RREP_PK_Send(src_ID, dest_ID);
                
                // 删除所有以src_ID为dest_ID的IERP表项。准备添加最新路由
                Del_IERP_Item_By_DestID(IERP_List, src_ID);
                
                // 添加逆向域间路由表项，模拟自己发出RREQ并收到RREP的处理
                path_qos = Get_Path_QOS(send_ID);
                
                ierp_ptr = (IERP_ITEM*)malloc(sizeof(IERP_ITEM));
                ierp_ptr->rreq_src_ID = Node_Attr.ID;
                ierp_ptr->rreq_seq = SEQ_INC(RREQ_Seq);
                ierp_ptr->dest_ID = src_ID;
                ierp_ptr->next_ID = send_ID;
                ierp_ptr->path_QOS = (rreq_pkptr->path_QOS > path_qos)? path_qos:rreq_pkptr->path_QOS;
                ierp_ptr->hop = rreq_pkptr->hop+1;
                ierp_ptr->trace_list[0] = Node_Attr.ID;
                for(i =1; i<= rreq_pkptr->hop; i++)
                {
                    ierp_ptr->trace_list[i]= ntohs(rreq_pkptr->trace_list[rreq_pkptr->hop-i]);
                }
                
                ierp_ptr->last_use_time = sysTimeGet();

                prg_list_insert(IERP_List, ierp_ptr,LIST_POS_TAIL);
                
                LOG_MSG(INFO_LEVEL,"add inverse IERP route\n");
                
                Print_All();
            }
            else
            {
                LOG_MSG(INFO_LEVEL, "not new, discard it\n");
            }
        }
        else
        {
           LOG_MSG(INFO_LEVEL, "not my dual Nb, discard it\n");
           return 0;
        }
        
        
    }//if(dest_ID == Node_Attr.ID)
    else
    {
        // 3. 否则，转发RREQ报文（仅MPR节点转发,且TTL>0）。
        
        if(rreq_pkptr->TTL >0 && Is_yourMPR(oneHopNb_List, send_ID, &path_qos) == 1 )
        {

        	/*
        	 * WJW:
        	 * 收到RREQ，但目的节点id不是本节点，此时通过调用Is_yourMPR()，判断
        	 * 本节点是否是send_ID的 MPR,若是，则更新此RREQ信息:
        	 * 包括TTL、中间发送者的id, 跳数，以及把本节点id追加至trace_list[]末尾，
        	 * 最后转发出去。
        	 * */
            update_res = Update_RREQ_Tab(rreq_pkptr, rreq_pklen);
            if(update_res == 1)	
            {           
                rreq_pkptr->TTL--;
                rreq_pkptr->send_ID = htons(Node_Attr.ID);	
                // path_QOS 更新为 min{本地到send_ID的QOS， 报文内的path_QOS}
                rreq_pkptr->path_QOS = (rreq_pkptr->path_QOS > path_qos)?path_qos:rreq_pkptr->path_QOS;	
                
                rreq_pkptr->trace_list[rreq_pkptr->hop] = htons(Node_Attr.ID);
                rreq_pkptr->hop++;
                                
                rreq_pkptr->pk_crc = getCRC((unsigned char*)rreq_pkptr,  rreq_pklen-2);
                
                // 发给WiFiSndThread的Route控制队列
                msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)rreq_pkptr, rreq_pklen, WLAN_ROUTE_CTRL,NO_WAIT);
                
                // LOG 打印
                LOG_MSG(INFO_LEVEL, "forward RREQ\n");
            }
            else
            {
                LOG_MSG(INFO_LEVEL, "not new, discard it\n");
            }
        }
        else
        {
            LOG_MSG(INFO_LEVEL, "not forward RREQ due to TTL(=%d) or MPR\n",rreq_pkptr->TTL);
        }
    }
    
	// 4. 若没产生更新，则不处理
	return 0;
}


/** RREP_PK_Send() -- 目的节点向RREQ的源节点发送RREP报文，逆向单播发送。从RREQ_Record表中找到该记录，封装RREP发送。
	输入：src_id为RREQ的源节点，这与通常的源地址定义不同，是为了简化操作，便于查找RREQ表
*/
void RREP_PK_Send(unsigned short rreq_src_id, unsigned short rreq_dest_id)
{
	RREP_PK send_pk;
	
	RREQ_RECORD		*record_ptr;
	int				record_pos;
	int				i;	

	// 1. 从RREQ_RECORD表中提取记录项，用于给RREP报文赋值	
	if (-1== (record_pos = Search_RREQ_Record(RREQ_Record_List, rreq_src_id, rreq_dest_id)))
		return;
		
	record_ptr = (RREQ_RECORD*)prg_list_access(RREQ_Record_List, record_pos);
	
	// 2. 封装报文
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));
	
	send_pk.pk_type = RREP_PK_TYPE;
	send_pk.reserved = 0;
	
	send_pk.send_ID = htons(Node_Attr.ID);
	send_pk.rcv_ID = htons(Get_Prev_ID(record_ptr->trace_list, Node_Attr.ID));
	send_pk.rreq_src_ID = htons(rreq_src_id);
	send_pk.rreq_dest_ID = htons(rreq_dest_id);
	send_pk.rreq_seq = htons(record_ptr->rreq_seq);		
	
	send_pk.path_QOS = record_ptr->path_QOS;
	send_pk.hop = record_ptr->hop;
	
	for(i = 0; i< record_ptr->hop; i++)
	{
		send_pk.trace_list[i] = htons(record_ptr->trace_list[i]);
	}	
	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(send_pk)-2);
	
	// 3. 发给WiFiSndThread的Route控制队列
	msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)&send_pk, sizeof(send_pk),WLAN_ROUTE_CTRL, NO_WAIT);
	
	// 4. LOG 打印
	LOG_MSG(INFO_LEVEL,"send RREP(%d -> %d)to %d\n",rreq_dest_id,rreq_src_id, ntohs(send_pk.rcv_ID));
}

/** RREP_PK_Rcvd() -- 收到RREP_PK后的处理。
	
	处理：
		1. rcv_ID收到后，根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发，中间节点不维护该条路径。
		2. 若自己为源节点，则不再转发，仅更新IERP。而且需要开启状态锁，在无线端的发送缓冲区内提取报文发送
*/
int RREP_PK_Rcvd(RREP_PK *rrep_pkptr, unsigned short rrep_pklen)
{
	IERP_ITEM	*ierp_ptr;
	
	unsigned short	send_ID;
	unsigned short	rcv_ID;
	unsigned short	rreq_src_ID;
	unsigned short	rreq_dest_ID;
	unsigned short	rreq_seq;
	unsigned char	hop;
	unsigned short	trace_list[MAX_HOP]={0};
	
	int	i;
	
	// 1. 校验整个报文CRC,校验错误则返回-1
	if (getCRC((unsigned char*)rrep_pkptr, sizeof(RREP_PK)) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"RREP_PK crc err\n");
		return -1;
	}
	
	// 2. 解析rcv_ID，src_ID，dest_ID等
	send_ID = ntohs(rrep_pkptr->send_ID);
	rcv_ID = ntohs(rrep_pkptr->rcv_ID);
	rreq_src_ID = ntohs(rrep_pkptr->rreq_src_ID);
	rreq_dest_ID = ntohs(rrep_pkptr->rreq_dest_ID);
	rreq_seq = ntohs(rrep_pkptr->rreq_seq);
	hop = rrep_pkptr->hop;
	
	
	for(i =0; i< hop; i++)
	{
		trace_list[i]= ntohs(rrep_pkptr->trace_list[i]);
	}
	
	if(rcv_ID == Node_Attr.ID)		//  由于RREP是单播，自己是接收者时才接收
	{
		
		LOG_MSG(INFO_LEVEL, "rcvd RREP(%d -> %d) from %d\n", rreq_dest_ID, rreq_src_ID,send_ID);	
	
		if(rreq_src_ID == Node_Attr.ID)
		{
//			if(rreq_dest_ID == 1)
//			{
//				TokenRing_OK_ACK_Send(1 , rreq_src_ID );
//			}

			// 3.1 如果自己是RREQ的源ID，更新IERP,将APP_BUFF内目的ID是dest_ID的报文提取出来，发给WiFiSndThread的App队列
			ierp_ptr = (IERP_ITEM*)malloc(sizeof(IERP_ITEM));
			ierp_ptr->rreq_src_ID = rreq_src_ID;
			ierp_ptr->rreq_seq = rreq_seq;
			ierp_ptr->dest_ID = rreq_dest_ID;
			ierp_ptr->next_ID = send_ID;
			ierp_ptr->path_QOS = rrep_pkptr->path_QOS;
			ierp_ptr->hop = hop;
			for(i =0; i< hop; i++)
			{
				ierp_ptr->trace_list[i]= trace_list[i];
			}
			ierp_ptr->last_use_time = sysTimeGet();

			prg_list_insert(IERP_List, ierp_ptr,LIST_POS_TAIL);
			
			App_Buff_Flush(APP_Buff_List, rreq_dest_ID);
			
			// 3.2 取消计时器和阻塞变量
			Is_Routing = 0;
			wdStart(wd_RREQ, 0,TIMER_ONE_SHOT);	
			
			// 打印
			Print_All();
		}
		else
		{
			// 4. 如果自己不是源ID，则根据trace_list填入新的rcv_ID和send_ID(本地ID)再转发
			rcv_ID = Get_Prev_ID(trace_list, Node_Attr.ID);	
			rrep_pkptr->rcv_ID = htons(rcv_ID);
			rrep_pkptr->send_ID = htons(Node_Attr.ID);
			rrep_pkptr->pk_crc = getCRC((unsigned char*)rrep_pkptr, rrep_pklen-2);
			
			LOG_MSG(INFO_LEVEL, "forward RREP to %d\n",rcv_ID);
			msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)rrep_pkptr, rrep_pklen,WLAN_ROUTE_CTRL, NO_WAIT);
			
		}
	}
	else
	{
		LOG_MSG(INFO_LEVEL, "rcvd RREP, not for me\n");
	}
	
	return 0;
}


/** RERR_PK_Send() -- 发现链路断裂的上游节点向源节点封装并发送RERR报文
						只有在转发域间路由的业务报文时，才会发现链路断裂，平时不靠HELLO包维护IERP，中间节点也不记录IERP。（否则，全网寻路后，就变成了全主动式路由了）
	处理：
		1. 发现断裂后，先尝试本地简单修复，即查找域内路由表，
			若有路径，则认为可修复，并据此封装RERR；
			若无路径，则认为不可修复，并据此封装RERR；
		2. 不论是否修复，均直接提交无线端发送。
		
		本地修复有问题：若发现断裂的中间节点刚好为目的节点的一跳或两跳邻居，可以把修复好的整条路径发给源端；
		但若是3跳及以上邻居，本地修复无法找到完整路径，只知道下一跳。
		因此，两种解决方案：
			1) 只做两跳内的本地修复
			2) 做整个域内的本地修复，但在APP报文转发时，若为域间路由，但trace_list中下一跳未知时，要采用域内路由予以转发
			3) 不作本地修复
		先按方案3作
*/
void RERR_PK_Send(unsigned short rcv_id, unsigned short dest_id, unsigned short err_down_ID, APP_PK *app_pkptr)
{
	RERR_PK			send_pk;
	
	int				routing_state = -1;
	unsigned char	hop = 3;
	//unsigned short	trace_list[MAX_HOP];
	
	// 按照方案3，不作本地修复，则此处不需查本地路由表
	// routing_state = Route_Search(dest_id, &rcv_ID, &hop, trace_list);
	
	memcpy((unsigned char*)&send_pk, (unsigned char*)app_pkptr, sizeof(send_pk));	// 完成rreq_src_ID，rreq_seq，hop，trace_list[]字段的赋值	

	if(routing_state == 0 && hop <=2)
	{
		// 1. 本地修复成功，封装RERR，并发给src_id
		send_pk.err_state = 1;
	}
	else
	{
		// 2. 本地修复失败，封装后向源端发送。封装时复用业务报文路由头的信息格式
		
		send_pk.err_state = 0;
	}
	
	send_pk.err_state = 0;	// 不作修复，强制为0
	send_pk.pk_type = RERR_PK_TYPE;
	
	send_pk.send_ID = htons(Node_Attr.ID);
	send_pk.rcv_ID = htons(rcv_id);
	send_pk.src_ID = htons(Node_Attr.ID);
	send_pk.dest_ID = htons(dest_id);
	
	send_pk.err_up_ID = htons(Node_Attr.ID);
	send_pk.err_down_ID = htons(err_down_ID);
	send_pk.err_repair_ID = 0xffff;

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(send_pk)-2);

	// 3. 发给WiFiSndThread的Route控制队列
	msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)&send_pk, sizeof(send_pk),WLAN_ROUTE_CTRL, NO_WAIT);
	
	// 4. LOG 打印
	LOG_MSG(INFO_LEVEL,"send RERR(%d -> %d, fail_ID=%d) to %d\n",Node_Attr.ID,dest_id, err_down_ID, rcv_id);
}

/** RERR_PK_Rcvd() -- 收到RERR_PK后的处理。

	处理：
		1. 若rcv_ID为中间节点，则先查验err_state
			若已修复，则转发RERR；
			若未修复，则尝试修复，即即查找域内路由表，
				若有路径，则认为可修复，更新接收到的RERR报文的trace_list等字段,并据此封装转发RERR；
				若无路径，则认为不可修复，并据此封装RERR；
					
		2. 若rcv_ID为源节点，先查验err_state
			若已修复，则更新IERP， // 不发送：并发送一包测试报文，帮助整条链路更新IERP，同时起到端到端验证的作用。
			若未修复，则删除IERP，再按照先域内后域外的原则，重新建立路由。
		
*/
int RERR_PK_Rcvd(RERR_PK *rerr_pkptr, unsigned short rerr_pklen)
{
	unsigned short	send_id;
	unsigned short	rcv_id;
	unsigned short	dest_id;
	unsigned short	rreq_src_id;
	unsigned short	rreq_seq;
	unsigned short	err_up_id;
	unsigned short	err_down_id;
	
	unsigned short	trace_list[MAX_HOP];
	int		i;
	
	// 1. 校验整个报文CRC,校验错误则返回-1
	if (getCRC((unsigned char*)rerr_pkptr, sizeof(RERR_PK)) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"RERR_PK crc err\n");
		return -1;
	}
	
	// 1. 解析报文
	send_id = ntohs(rerr_pkptr->send_ID);
	rcv_id = ntohs(rerr_pkptr->rcv_ID);
	dest_id = ntohs(rerr_pkptr->dest_ID);	
	rreq_src_id = ntohs(rerr_pkptr->rreq_src_ID);
	rreq_seq = ntohs(rerr_pkptr->rreq_seq);
	err_up_id = ntohs(rerr_pkptr->err_up_ID);
	err_down_id = ntohs(rerr_pkptr->err_down_ID);
	
	for(i = 0; i< MAX_HOP; i++)
	{
		trace_list[i] = ntohs(rerr_pkptr->trace_list[i]);			
	}
	
	if(rcv_id == Node_Attr.ID)		//  由于RERR是单播，自己是接收者时才接收
	{	
		// LOG 打印
		LOG_MSG(INFO_LEVEL, "rcvd RERR(%d -> %d, fail_ID=%d) from %d\n",err_up_id, dest_id, err_down_id,send_id);
	
		if(rcv_id == dest_id)
		{
			// 2. 如果自己就是源节点，则根据rreq_src_id和rreq_seq，删除或更新域间路由			
			if(rerr_pkptr->err_state == 0)
			{
				// 未修复，则删除
				Del_IERP_Item_By_Seq(IERP_List, rreq_src_id, rreq_seq);
			}
			else
			{
				// 已修复，则更新域间路由。暂不处理	
				
			}
			
		}
		else
		{
			// 3. 不是源节点，则根据trace_list找到上一跳后，转发至无线端	
			Del_IERP_Item_By_Seq(IERP_List, rreq_src_id, rreq_seq);	// 仅当DSR旁听时才会起作用
			
			rcv_id = Get_Prev_ID(trace_list, Node_Attr.ID);		//理论上，此处必然能够更新rcv_ID，否则，就是源端封装路由头时出错了
			
			rerr_pkptr->rcv_ID = htons(rcv_id);
			rerr_pkptr->send_ID = htons(Node_Attr.ID);
			
			rerr_pkptr->pk_crc = getCRC((unsigned char*)rerr_pkptr, rerr_pklen-2);
			
			LOG_MSG(INFO_LEVEL, "forward RERR to %d\n",rcv_id);
			
			msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)rerr_pkptr, rerr_pklen,WLAN_ROUTE_CTRL, NO_WAIT);
		}	
	}
	else
	{
		LOG_MSG(INFO_LEVEL, "rcvd RERR to %d , not for me\n",rcv_id);
	}
	return 0;
	
}

/** APP_PK_Send() -- 源端发送报文，即报文从有线端传来后，需要通过无线端发出时，调用此函数
	
	处理：
		1. 顺序查找IARP和IERP路由表，
			1.1 若IARP内存在，则选择cost最小的一条路径，填写下一跳。但是trace_list置无效不理；
			1.2 否则若IERP内存在，则填写trace_list及其它报头；
			1.3 若都不存在，则开启域间寻路过程，系统状态置为寻路状态，该状态下不会再次寻路与发送业务报文
*/
void APP_PK_Send(unsigned char *app_pkptr, unsigned short app_len)
{
	APP_PK 	*APP_pkptr = (APP_PK*)app_pkptr; 
	unsigned char	*IP_pkptr;
	unsigned short	ip_len;
	
	unsigned short	rcv_ID;
	unsigned short	src_ID;
	unsigned short	dest_ID;
	unsigned short	rreq_src_id;

	unsigned char	protocol;
	
	unsigned char	hop;
	unsigned short	trace_list[MAX_HOP];
	
	int	routing_state;
	int	i;
	
	
	IP_pkptr = APP_pkptr->data;
	ip_len = app_len - ROUTE_HEAD_LEN;

	// 1. 根据IP报头格式，解析出源、目的IP地址 和 协议类型 (ICMP, UDP, TCP)
	src_ID = getSrcAddr_fromIPHeader(IP_pkptr);
	dest_ID = getDestAddr_fromIPHeader(IP_pkptr);
	protocol = getProto_fromIPHeader(IP_pkptr);
	
	// 2. 根据目的地址，查路由表：若有路由，则封装APP报文，发给WiFi；若无，则将IP_pkptr缓存到APP_BUFF,并发起寻路
	if(dest_ID == Node_Attr.ID)
		return;

	/*
	 * WJW：
	 * 调用Route_Search，查找路由表
	 * 1》先查找一跳路由表，oneHopNb_List，如果dest_Id在表项中，且表项为双向可通（direction=1）,
	 *	  则返回qos最大的节点，作为下一跳，并且：
	 *	  rcv_Id为一跳邻居列表中节点元素id;
	 *	  rreq_src_id = 0;
	 *	  hop =1;
	 *	  trace_list[]无效
	 *	  qos为最大值
	 *	  返回值为0,表示成功
	 * 2》一跳邻居列表中无，则查找m跳路由表，mHopNb_List，如果dest_Id在表项中，返回：
	 *	  则返回qos最大的节点并且：
	 *	  rcv_Id为m跳邻居列表中节点元素为dest_id对应的Relay_ID;
	 *	  rreq_src_id = 0;
	 *	  hop =element_ptr2->hop;;
	 *	  trace_list[]无效
	 *	  qos为最大值
	 *	  返回值为0,表示成功
	 *
	 * 3》若一跳邻居列表、m跳路由表均不存在，则查找域间路由表：
	 *	  rcv_Id为域间路由表中节点元素为dest_id对应的next_id;
	 *	  rreq_src_id = element_ptr3->rreq_src_ID;
	 *	  hop = element_ptr3->hop;
	 *	  trace_list[]有效
	 *	  qos为最大值
	 *	  返回值为rreq_seq,表示成功
	 *
	 *	否则返回-1,表示无路由

	 * */
	routing_state = Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);
	
	if(routing_state >= 0)		// 有域内或域间路由
	{
		// 3. 在IP_pkptr基础上，封装路由报头。按网络字节序封装
		APP_pkptr->pk_type = DATA_PK_TYPE;
		APP_pkptr->reserved = 0;
		APP_pkptr->send_ID = htons(src_ID);
		APP_pkptr->rcv_ID = htons(rcv_ID);		//表示到达目的节点的中间节点或目的ID本身。
		APP_pkptr->src_ID = htons(src_ID);
		APP_pkptr->dest_ID = htons(dest_ID);
		APP_pkptr->rreq_src_ID = htons(rreq_src_id);
		/*
		 * WJW：
		 * rreq_seq为0时，表示域内有路由
		 * */
		APP_pkptr->rreq_seq = htons(routing_state);									
		APP_pkptr->TTL = MAX_HOP-1;
		APP_pkptr->hop = hop;
		for(i =0; i< MAX_HOP; i++)
		{
			APP_pkptr->trace_list[i] = htons(trace_list[i]);
		}
		
		if(dest_ID == 0xffff)
		{		
			// 注意 发广播时，要更新本地的广播号，并记入广播号表。之后再受到邻居转发的该报文，不再转发
			Bcast_ForwardSeq_Record[Node_Attr.ID] = SEQ_INC(Bcast_Seq);
			APP_pkptr->bc_seq = htons(Bcast_ForwardSeq_Record[Node_Attr.ID]);
		}
		else
		{
			APP_pkptr->bc_seq = 0;
		}	
		APP_pkptr->len = htons(ip_len);
		APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
		
		// 4. 发给WiFiSndThread的app队列
		msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len,WLAN_ROUTE_DATA, NO_WAIT);
		
		// 5. LOG 打印 并统计
		//LOG_MSG(DBG_LEVEL,"send APP %d->%d (proto=%d, size=%d, src port=%d, dst port=%d) to %d\n",src_ID, dest_ID,protocol,ip_len, getSrcPort_fromIPHeader(IP_pkptr), getDstPort_fromIPHeader(IP_pkptr),rcv_ID);
        LOG_MSG(DBG_LEVEL,"routing.c-> msgQ_snd, send APP %d->%d (proto=%d, size=%d, rreq_seq=%d) to %d\n",src_ID, dest_ID,protocol,ip_len, routing_state ,rcv_ID);
		Node_Attr.send_pk_num++;
		Node_Attr.send_pk_size += ip_len;
	}
	else						// 无路由
	{
		// 3. 将IP_pkptr缓存到APP_BUFF，注意BUFF_ITEM的封装
		Add_APP_PK(APP_Buff_List, dest_ID, (unsigned char*)APP_pkptr, app_len);
		
		LOG_MSG(DBG_LEVEL,"add APP(to %d) to Buffer\n",dest_ID);
		
		// 4. 若当前不处在寻路状态，则开启寻路。同一时间只能向某一个目的节点寻路，超时后才寻下一个目的节点。
		//LOG_MSG(DBG_LEVEL,"send APP %d->%d (proto=%d, size=%d, src port=%d, dst port=%d), no route\n",src_ID, dest_ID,protocol,ip_len, getSrcPort_fromIPHeader(IP_pkptr), getDstPort_fromIPHeader(IP_pkptr));
        LOG_MSG(DBG_LEVEL,"send APP %d->%d (proto=%d, size=%d), no route\n",src_ID, dest_ID,protocol,ip_len);
		if(Is_Routing == 0)
		{
			Is_Routing = 1;
			RREQ_Dest_ID = dest_ID;
			RREQ_PK_Send(dest_ID);	
		}
	}
}

/** APP_PK_Rcvd() -- 节点通过无线端收到APP_PK后的处理。

	处理：
		1. 查看APP头，
			1.1 若目的节点是广播，则若自己为发送节点的MPR且TTL>0且bc_seq更新，则随机退避后转发，否则不转发。上传给有线端PC机
			1.2 若接收节点是自身，
					若rreq_seq==0，表示域内路由，直接查找本地IARP路由表转发，若没有条目，直接丢弃报文（超时后源节点会发现）
					若rreq_seq != 0，表示域间路由，解析报头内的trace_list，确定下一跳；
						若下一跳双向可达，则重封装后转发；
						否则直接丢弃报文，并向源端发送REER（先尝试本地修复）
				上传给有线端PC机
			1.3 否则，不处理
	返回：
	    -1： 报头CRC校验错误
	    0：接收ID不是广播或自身，丢弃
	    1：正确接收
*/
int APP_PK_Rcvd(unsigned char *app_pkptr, unsigned short app_len)
{
	APP_PK 	*APP_pkptr = (APP_PK*)app_pkptr; 
	unsigned char	*IP_pkptr;
	unsigned short	ip_len;
	
	unsigned short	send_ID;
	unsigned short	rcv_ID;
	unsigned short	src_ID;
	unsigned short	dest_ID;
	unsigned short	rreq_src_id;
	unsigned short	rreq_seq;
	unsigned short	bc_seq;
	unsigned char	TTL;			// 广播生存时间，限制广播范围。源端置为6，每转发一次递减1，到0时不再广播转发，即发6次			
	unsigned char	hop;
	unsigned short	trace_list[MAX_HOP];
	
	int	routing_state;
	int	i;
	

	IP_pkptr = APP_pkptr->data;
	ip_len = app_len - ROUTE_HEAD_LEN;
	
	// 1. 校验Route报头CRC,校验错误则返回-1
	if (getCRC(app_pkptr, ROUTE_HEAD_LEN) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"APP_PK crc err\n");
		return -1;
	}
	// 2. 根据Route报头格式，解析出源、目的IP地址 和 TTL，bc_seq等信息; 注意，报文内容为网络字节序
	send_ID = ntohs(APP_pkptr->send_ID);
	rcv_ID = ntohs(APP_pkptr->rcv_ID);
	src_ID = ntohs(APP_pkptr->src_ID);
	dest_ID = ntohs(APP_pkptr->dest_ID);
	rreq_seq = ntohs(APP_pkptr->rreq_seq);
	bc_seq = ntohs(APP_pkptr->bc_seq);
	TTL = APP_pkptr->TTL;
	hop = APP_pkptr->hop;


	for(i = 0; i< MAX_HOP; i++)
	{
		trace_list[i] = ntohs(APP_pkptr->trace_list[i]);			
	}
		
	//LOG_MSG(DBG_LEVEL,"rcvd APP %d->%d (proto=%d, size=%d, src port=%d, dst port=%d) from %d\n",src_ID, dest_ID, getProto_fromIPHeader(IP_pkptr),ip_len,getSrcPort_fromIPHeader(IP_pkptr), getDstPort_fromIPHeader(IP_pkptr), send_ID);
    LOG_MSG(DBG_LEVEL,"rcvd APP %d->%d (proto=%d, size=%d, rreq_seq=%d) from %d\n",src_ID, dest_ID, getProto_fromIPHeader(IP_pkptr),ip_len,rreq_seq, send_ID);
	Node_Attr.rcvd_pk_num++;
	Node_Attr.rcvd_pk_size += ip_len;
	
	if(rcv_ID == 0xffff)	// 接收节点是广播
	{
        // 如果之前未收到过该报文，这上报PC，并记录Bcast_ReportSeq_Record
        if(Seq_Cmp(bc_seq,Bcast_ReportSeq_Record[src_ID])>0 )
        {
            Bcast_ReportSeq_Record[src_ID] = bc_seq;

//            if(node_relay_auv_pkt(IP_pkptr))
//            {
//    			LOG_MSG(DBG_LEVEL, " nsv_relay_auv_pkt true,\n");
//            }

			LOG_MSG(DBG_LEVEL,"Give APP to PC\n");
			msgQ_snd(msgQ_Lan_Snd, (unsigned char*)IP_pkptr, ip_len,LAN_DATA, NO_WAIT);
        }
        
        // 如果MPR，TTL，bc_seq均满足条件，则修改报头后转发给无线端
        // 只有广播才存在MPR机制
        if(TTL > 0 && Is_yourMPR(oneHopNb_List, send_ID, NULL) == 1 && Seq_Cmp(bc_seq,Bcast_ForwardSeq_Record[src_ID])>0)
        {
            // 若我是send_ID的MPR，且TTL>0，则应转发该广播包。修改收发地址、TTL，重新计算CRC.并更新 Bcast_ForwardSeq_Record
            APP_pkptr->send_ID = htons(Node_Attr.ID);
            APP_pkptr->rcv_ID = htons(0xffff);					
            APP_pkptr->TTL--;

            APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
            
            Bcast_ForwardSeq_Record[src_ID] = bc_seq;
            
            //  随机退避后发给WiFiSndThread的App队列
            LOG_MSG(DBG_LEVEL,"forward APP to %d\n",rcv_ID);
            msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len, WLAN_ROUTE_DATA, NO_WAIT);
        }
        
        // 其它情况下，既不上报PC，也不转发，直接丢弃
		
	}	

	/*
	 * 以下为点对点传输，多跳中继情况下，通过trace_list[...]指导如何传输至下一个节点
	 * */
	else if(rcv_ID == Node_Attr.ID)
	{
		if(dest_ID == rcv_ID)
		{
			// 4.1 接收节点自身，且为最终目的节点，不再转发，直接递交PC即可：将报文卸去路由头，发给LanSndThread,

//			if(src_ID == 1)
//			{
//				TokenRing_OK_ACK_Send(1,dest_ID );
//			}


			if(!Is_myPing(IP_pkptr,ip_len))
			{

				LOG_MSG(DBG_LEVEL,"Give APP to PC\n");
				msgQ_snd(msgQ_Lan_Snd, (unsigned char*)IP_pkptr, ip_len,LAN_DATA, NO_WAIT);
			}

//	            if(node_relay_auv_pkt(IP_pkptr))
//	            {
//	    			LOG_MSG(DBG_LEVEL, "nsv_relay_auv_pkt true,\n");
//	            }

//默认不支持ping
//				LOG_MSG(DBG_LEVEL,"Give APP to PC\n");
//				msgQ_snd(msgQ_Lan_Snd, (unsigned char*)IP_pkptr, ip_len,LAN_DATA, NO_WAIT);

		}
		else if(rreq_seq == 0)
		{
			// 4.2 接收节点是自身，且为域内路由，则查IARP找下一跳
			routing_state = Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);
			
			if(routing_state == 0)	
			{
				// 4.2.1 有域内路由, 修改收发地址，重新计算CRC
				APP_pkptr->send_ID = htons(Node_Attr.ID);
				APP_pkptr->rcv_ID = htons(rcv_ID);
				APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
				
				// 4.2.2 随机退避后发给WiFiSndThread的App队列
				LOG_MSG(DBG_LEVEL,"forward APP to %d\n",rcv_ID);
				msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len,WLAN_ROUTE_DATA, NO_WAIT);
					
			}
			else		
			{
				// 4.2.3 只有域间路由或无路由，直接丢弃报文
                LOG_MSG(DBG_LEVEL,"no IARP route to forward\n");
				
			}
		}
		else	
		{
			// 4.3 接收节点是自身，且为域间路由，则查报头内的trace_list找下一跳	
			rcv_ID = Get_Next_ID(trace_list, Node_Attr.ID);		//理论上，此处必然能够更新rcv_ID，否则，就是源端封装路由头时出错了
			
			// 4.3.1 查一跳邻居表，判断到rcv_ID是否可达？可达则封装后发送，不可达，则逆向发rerr_pklen
			i = Is_1hopNb(oneHopNb_List, rcv_ID, NULL);
			if( i >1 || (i == 1 && Node_Attr.IARP_radius == 0))
			{
				//  是双向邻居, 或radius = 0 的情况下为单向邻居，则修改收发地址，重新计算CRC
				APP_pkptr->send_ID = htons(Node_Attr.ID);
				APP_pkptr->rcv_ID = htons(rcv_ID);
				APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
				
				//  随机退避后发给WiFiSndThread的App队列
				LOG_MSG(DBG_LEVEL,"forward APP to %d\n",rcv_ID);
				msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len, WLAN_ROUTE_DATA,NO_WAIT);
			}
			else
			{
				// 直接丢弃报文，并向源端发送RERR（先尝试本地修复）
				RERR_PK_Send(send_ID, src_ID, rcv_ID, APP_pkptr);	
			}
			
		}
		
	}
	else
	{
		//4.5 接收ID既不是广播，又不是自身，直接丢弃。
		return 0;
	}
	
	return 1;
}



int Print_oneHopNb_List()
{
	int				i = 1;
	ONE_HOP_NB_ITEM	*element_ptr;
	
	LOG_MSG(ERR_LEVEL,"***oneHopNb_List:***\n");
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
	{

		LOG_MSG(ERR_LEVEL,"node %d: degree %d, dir %d, myMPR %d, yourMPR %d, path_QOS =%d (Dude rcv %d, my send %d)\n",\
				element_ptr->node_ID,\
				element_ptr->degree,\
				element_ptr->direction,\
				element_ptr->is_my_mpr,\
				element_ptr->is_your_mpr,\
				element_ptr->path_QOS,\
				element_ptr->rcv_qos,\
				element_ptr->send_qos);
		i++;
		
	}
	return 0;
}

int Print_mHopNb_List()
{
	int				i = 1;
	MULTI_HOP_NB_ITEM	*element_ptr;
	
	LOG_MSG(ERR_LEVEL,"***mHopNb_List:***\n");
	while(NULL != (element_ptr = (MULTI_HOP_NB_ITEM*)prg_list_access(mHopNb_List, i)))
	{
		
		LOG_MSG(ERR_LEVEL,"node %d: relayID %d, pathQOS %d, hop %d\n",\
				element_ptr->node_ID, \
				element_ptr->relay_ID,\
				element_ptr->path_QOS,\
				element_ptr->hop);
		i++;
		
	}
	return 0;
}

int Print_IERP_List()
{
	int				i = 1;
	IERP_ITEM		*element_ptr;
	int				j;
	char			buff[128];

	LOG_MSG(ERR_LEVEL,"***IERP_List:***\n");
	while(NULL != (element_ptr = (IERP_ITEM*)prg_list_access(IERP_List, i)))
	{
		
		sprintf(buff,"rreq_src %d (%d): dest %d, next %d, pathQOS %d, hop %d = [",\
			element_ptr->rreq_src_ID, \
			element_ptr->rreq_seq, \
			element_ptr->dest_ID, \
			element_ptr->next_ID, \
			element_ptr->path_QOS, \
			element_ptr->hop-1);
		for(j = 1; j<element_ptr->hop; j++ )
		{
			sprintf(buff+strlen(buff),"%d, ",element_ptr->trace_list[j]);
		}
		sprintf(buff+strlen(buff)-2,"]\n");

		i++;
		LOG_MSG(ERR_LEVEL,buff);
	}
	
	return 0;

}

int Print_Node_Attr()
{
	LOG_MSG(ERR_LEVEL,"\n***NODE_ATTR: ID=%d, rt_mode=%d***\n",Node_Attr.ID,Node_Attr.cluster_state);
	LOG_MSG(ERR_LEVEL,"setting: IARP_radius=%d, cu_size_low=%d, cu_size_high=%d\n", Node_Attr.IARP_radius,CLUSTER_TO_ZRP_NUM, ZRP_TO_CLUSTER_NUM);
	LOG_MSG(ERR_LEVEL,"status: degree=%d, cu_size=%d, cu_head=%d\n", Node_Attr.degree, Node_Attr.cluster_size,Node_Attr.cluster_header);
	LOG_MSG(ERR_LEVEL,"tx: %d pks(%d bytes)   rx: %d pks(%d bytes)\n*****************************\n",
		Node_Attr.send_pk_num, Node_Attr.send_pk_size,Node_Attr.rcvd_pk_num,Node_Attr.rcvd_pk_size);
	
	return 0;
}

int Print_RREQ_Record_List()
{
	int				i = 1,j=0;
	RREQ_RECORD		*element_ptr;
	char			buff[128];
	
	LOG_MSG(ERR_LEVEL,"***RREQ_Record_List:***\n");
	while(NULL != (element_ptr = (RREQ_RECORD*)prg_list_access(RREQ_Record_List, i)))
	{
		sprintf(buff, "rreq_src %d (%d): dest %d, pathQOS %d, hop %d = [",
			element_ptr->rreq_src_ID, element_ptr->rreq_seq, element_ptr->rreq_dest_ID, element_ptr->path_QOS, element_ptr->hop);
		for(j = 0; j<element_ptr->hop; j++ )
		{
			sprintf(buff + strlen(buff),"%d, ",element_ptr->trace_list[j]);
		}
		sprintf(buff+strlen(buff)-2,"]\n");

		i++;
		LOG_MSG(ERR_LEVEL,buff);
	}
	
	return 0;
}

int Print_All()
{
	LOG_MSG(ERR_LEVEL, "\n***NODE_ATTR: ID=%d, rt_mode=%d***\n",Node_Attr.ID,Node_Attr.cluster_state);
	Print_oneHopNb_List();
	Print_mHopNb_List();
	Print_IERP_List();
	Print_RREQ_Record_List();
	return 0;
	
}

void Report_Node_Attr()
{
/*	unsigned char msg[32];
	int			msglen = 0;
	struct sockaddr_in daddr;

	int self_id = Node_Attr.ID;
	
	daddr.sin_family = AF_INET;
	daddr.sin_port = htons(WLAN_LISTEN_PORT+1);
	daddr.sin_addr.s_addr = (0xff<<24)+((self_id&0xff)<<16)+(self_id&0xff00)+0x0a;

	msg[0] = 0x00;
	msg[1] = 0x01;
	
	msg[4] = (Node_Attr.ID>>8)&0xff;
	msg[5] = Node_Attr.ID&0xff;
	msg[6] = Node_Attr.cluster_state;
	msg[7] = Node_Attr.IARP_radius;
	msg[8] = Node_Attr.degree;
	msg[9] = Node_Attr.cluster_size;
	msg[10] = (Node_Attr.cluster_header>>8)&0xff;
	msg[11] = Node_Attr.cluster_header&0xff;

	msglen = 12;
	msg[msglen++] = (Node_Attr.send_pk_num>>24) &0xff;
	msg[msglen++] = (Node_Attr.send_pk_num>>16) &0xff;
	msg[msglen++] = (Node_Attr.send_pk_num>>8) &0xff;
	msg[msglen++] = (Node_Attr.send_pk_num>>0) &0xff;
	msg[msglen++] = (Node_Attr.send_pk_size>>24) &0xff;
	msg[msglen++] = (Node_Attr.send_pk_size>>16) &0xff;
	msg[msglen++] = (Node_Attr.send_pk_size>>8) &0xff;
	msg[msglen++] = (Node_Attr.send_pk_size>>0) &0xff;

	msg[msglen++] = (Node_Attr.rcvd_pk_num>>24) &0xff;
	msg[msglen++] = (Node_Attr.rcvd_pk_num>>16) &0xff;
	msg[msglen++] = (Node_Attr.rcvd_pk_num>>8) &0xff;
	msg[msglen++] = (Node_Attr.rcvd_pk_num>>0) &0xff;
	msg[msglen++] = (Node_Attr.rcvd_pk_size>>24) &0xff;
	msg[msglen++] = (Node_Attr.rcvd_pk_size>>16) &0xff;
	msg[msglen++] = (Node_Attr.rcvd_pk_size>>8) &0xff;
	msg[msglen++] = (Node_Attr.rcvd_pk_size>>0) &0xff;

	msg[2] = (msglen>>8)&0xff;
	msg[3] = (msglen>>0)&0xff;
	
	if ( sendto(Sock, msg, msglen, 0, (struct sockaddr *) &daddr,sizeof(daddr)) <0)
	{
		LOG_MSG(ERR_LEVEL,"report err\n");
	}*/
	
}
//void Report_Table()
//{
//	unsigned char 	msg[1024];
//	int		msglen = 0;
//	struct sockaddr_in daddr;
//
//	int self_id = Node_Attr.ID;
//	unsigned short	node_num = 0;
//
//	int	i = 1;
//	ONE_HOP_NB_ITEM		*element_ptr1;
//	MULTI_HOP_NB_ITEM	*element_ptr2;
//	IERP_ITEM		*element_ptr3;
//
//	daddr.sin_family = AF_INET;
//	daddr.sin_port = htons(WLAN_LISTEN_PORT+1);
//	daddr.sin_addr.s_addr = (0xff<<24)+((self_id&0xff)<<16)+(self_id&0xff00)+0x0a;
//
//	msg[0] = 0x00;
//	msg[1] = 0x02;
//	msg[2] = (msglen>>8)&0xff;			// 发送前再修改
//	msg[3] = (msglen>>0)&0xff;
//
//	msg[4] = (self_id>>8)&0xff;
//	msg[5] = (self_id>>0)&0xff;
//	msg[6] = (node_num>>8)&0xff;			// 发送前再修改
//	msg[7] = (node_num>>0)&0xff;
//	msg[8] = 0x00;
//	msg[9] = 0x00;
//
//	msglen = 10;
//
//	while(NULL != (element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
//	{
//
//		if(element_ptr1->direction == 1)	// 只报双向邻居
//		{
//			msg[msglen++] = ( element_ptr1->node_ID>>8)&0xff;		// dest id
//			msg[msglen++] = ( element_ptr1->node_ID>>0)&0xff;
//			msg[msglen++] = ( element_ptr1->node_ID>>8)&0xff;		// next id
//			msg[msglen++] = ( element_ptr1->node_ID>>0)&0xff;
//			msg[msglen++] = 0x01;						// hop
//			msg[msglen++] = (element_ptr1->is_my_mpr<<7)+element_ptr1->path_QOS;			// mpr
//
//			node_num++;
//		}
//
//		i++;
//
//	}
//
//	i= 1;
//	while(NULL != (element_ptr2 = (MULTI_HOP_NB_ITEM*)prg_list_access(mHopNb_List, i)))
//	{
//		msg[msglen++] = ( element_ptr2->node_ID>>8)&0xff;		// dest id
//		msg[msglen++] = ( element_ptr2->node_ID>>0)&0xff;
//		msg[msglen++] = ( element_ptr2->relay_ID>>8)&0xff;		// next id
//		msg[msglen++] = ( element_ptr2->relay_ID>>0)&0xff;
//		msg[msglen++] = element_ptr2->hop;				// hop
//		msg[msglen++] = element_ptr2->path_QOS;						// mpr
//		i++;
//		node_num++;
//
//	}
//
//	i = 1;
//	while(NULL != (element_ptr3 = (IERP_ITEM*)prg_list_access(IERP_List, i)))
//	{
//
//		msg[msglen++] = ( element_ptr3->dest_ID>>8)&0xff;		// dest id
//		msg[msglen++] = ( element_ptr3->dest_ID>>0)&0xff;
//		msg[msglen++] = ( element_ptr3->next_ID>>8)&0xff;		// next id
//		msg[msglen++] = ( element_ptr3->next_ID>>0)&0xff;
//		msg[msglen++] = element_ptr3->hop-1;				// hop
//		msg[msglen++] = element_ptr3->path_QOS;						// mpr
//		i++;
//		node_num++;
//
//	}
//
//
//	msg[2] = (msglen>>8)&0xff;			// 发送前再修改
//	msg[3] = (msglen>>0)&0xff;
//
//	msg[6] = (node_num>>8)&0xff;			// 发送前再修改
//	msg[7] = (node_num>>0)&0xff;
//
//	if ( sendto(Sock, msg, msglen, 0, (struct sockaddr *) &daddr,sizeof(daddr)) <0)
//	{
//		LOG_MSG(ERR_LEVEL,"report err\n");
//	}
//
//}



void test_update_mympr()
{
    ONE_HOP_NB_ITEM* new_elem_ptr;
    MULTI_HOP_NB_ITEM* new_elem_ptr2;
    // 1hop Nb
    new_elem_ptr = (ONE_HOP_NB_ITEM*)malloc(sizeof(ONE_HOP_NB_ITEM));
	prg_list_insert(oneHopNb_List, new_elem_ptr, LIST_POS_TAIL);
	new_elem_ptr->node_ID = 3;
	new_elem_ptr->direction = 0;
    
    new_elem_ptr = (ONE_HOP_NB_ITEM*)malloc(sizeof(ONE_HOP_NB_ITEM));
	prg_list_insert(oneHopNb_List, new_elem_ptr, LIST_POS_TAIL);
	new_elem_ptr->node_ID = 5;
	new_elem_ptr->direction = 1;
    
    new_elem_ptr = (ONE_HOP_NB_ITEM*)malloc(sizeof(ONE_HOP_NB_ITEM));
	prg_list_insert(oneHopNb_List, new_elem_ptr, LIST_POS_TAIL);
	new_elem_ptr->node_ID = 1;
	new_elem_ptr->direction = 1;
    // 2 hop Nb
    
    new_elem_ptr2 = (MULTI_HOP_NB_ITEM*)malloc(sizeof(MULTI_HOP_NB_ITEM));
    prg_list_insert(mHopNb_List, new_elem_ptr2, LIST_POS_TAIL);
	new_elem_ptr2->node_ID = 3;
	new_elem_ptr2->relay_ID = 5;
	new_elem_ptr2->hop = 2;
    
    new_elem_ptr2 = (MULTI_HOP_NB_ITEM*)malloc(sizeof(MULTI_HOP_NB_ITEM));
    prg_list_insert(mHopNb_List, new_elem_ptr2, LIST_POS_TAIL);
	new_elem_ptr2->node_ID = 3;
	new_elem_ptr2->relay_ID = 1;
	new_elem_ptr2->hop = 2;
    
    new_elem_ptr2 = (MULTI_HOP_NB_ITEM*)malloc(sizeof(MULTI_HOP_NB_ITEM));
    prg_list_insert(mHopNb_List, new_elem_ptr2, LIST_POS_TAIL);
	new_elem_ptr2->node_ID = 4;
	new_elem_ptr2->relay_ID = 1;
	new_elem_ptr2->hop = 2;
    
    
    Update_myMPR(oneHopNb_List,mHopNb_List);
    Print_All();
}
