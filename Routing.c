
#include	"Routing.h"
#include	"linklist.h"
#include	"aux_func.h"		// �ں���Ϣ���У���ʱ����ϵͳʱ���ȡ���̣߳�CRCУ��Ȳ���
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
unsigned int    RADIUS = 1;                 // //��뾶��
unsigned int	CLUSTER_TO_ZRP_NUM = 3;		// Cluster���ɵ�ZRP�Ľڵ������  3
unsigned int	ZRP_TO_CLUSTER_NUM = 5;		// ZRP���ɵ�Cluster�Ľڵ������ 5
unsigned int    RETRY_TIMES = 7;            // LSA��ʱʱ��
unsigned int    PERIOD_LSA  = 1000;          //LSA ��������
unsigned int    IERP_RREQ_TIMEOUT = 2000;   // RREQ ��ʱʱ��

static unsigned short	RREQ_Seq;
static unsigned short	Bcast_Seq;

unsigned char 			Is_Routing;		// ֵ0��ʾû����Ѱ·���̣�ֵ1��ʾ��Ѱ·����RREQ���ȴ�RREP�Ĺ����У�
unsigned short			RREQ_Dest_ID;	// ��Is_Routing���ʹ�ã���ʾ����Ѱ·��Ŀ��ID

unsigned short			Bcast_ForwardSeq_Record[MAX_NODE_NUM];		// �±��ʾ����㲥��ԴID��ֵ��ʾ�����յ��Ķ�ӦID�����¹㲥�����,��ֹ��ι㲥ת��
unsigned short			Bcast_ReportSeq_Record[MAX_NODE_NUM];		// �±��ʾ����㲥��ԴID��ֵ��ʾ�����յ��Ķ�ӦID�����¹㲥�����,��ֹ��ι㲥�ϱ�PC

unsigned char			tempBool[MAX_NODE_NUM];				// ���������ھӸ���ʱ��ʱʹ��

unsigned char 			LSA_Flag;		// LSA ���Ϳ��أ�Ϊ0�رգ�Ϊ1��
extern const double		rand_t1_max;

extern u8 master_cong_id;
extern u16 self_node_id;

NODE_ATTR	Node_Attr;		// ���ؽڵ����ԣ�����ID��·��״̬���շ�ͳ�Ƶ�

List*	APP_Buff_List = NULL;		// APP_BUFF����Ԫ����BUFF_ITEM
List*	RREQ_Record_List = NULL;	// RREQ��¼����Ԫ����RREQ_RECORD

List*	TOKEN_RING_BC_REQ_Record_List = NULL;	// RREQ��¼����Ԫ����RREQ_RECORD

List*	oneHopNb_List = NULL;		// һ���ھ�����Ԫ����ONE_HOP_NB_ITEM
List*	mHopNb_List = NULL;			// �����ھ�����Ԫ����MULTI_HOP_NB_ITEM
List*	IERP_List = NULL;			// ���·������Ԫ����IERP_ITEM


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
	@param1 sigNum �����ź�ID
	@param2 timerid ����ʱ�����
	@param3 void(*handler)(int , siginfo_t*, void*) ��ʱ��������
	*/

	//������ϢΪWD_LSA_SIG��ʱ��
	ret = wdCreate(WD_LSA_SIG,&wd_LSA,&LSA_PK_SendHandler);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"create wd_LSA");
		return ret;
	}

	//������ϢΪWD_RREQ_SIG��ʱ��
	ret = wdCreate(WD_RREQ_SIG,&wd_RREQ,&RREQ_TimeoutHandler);
	if(ret < 0)
	{
		LOG_MSG(ERR_LEVEL,"create wd_RREQ");
		return ret;
	}
	return ret;
	
}

/*
*��ȡ�����е�id �����뵽ȫ�ֱ����������ȡ�ɹ�������ֵ���ڵ���0�����򷵻�ֵС��0
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

/* ·���ŷ��̳߳�ʼ�� 
 *   ��·��Э����صı������г�ʼ����
 *   �����Ӻ�������1Hop��mHop�ھӺ�IARP��IERP·�ɱ�APP_BUFF���������г�ʼ��
 *   ����LSA���ĵ����ڷ���
 
 	��ʼ���ɹ�����1��ʧ�ܷ���0
 */
int Route_Init()
{
	const int on = 1;
	
	Node_Attr.ID = getSelfID();
	Node_Attr.cluster_state = CLUSTER_NONE;
	Node_Attr.cluster_size = 0;
	Node_Attr.cluster_header = 0xffff;
	Node_Attr.node_qos = 1;			// �ݾ���дĬ��ֵ1

	//��뾶
	Node_Attr.IARP_radius = RADIUS;		// LSAЯ��һ���ھӣ�ά��2���ھӱ�

	//˫��һ���ھӸ���������ִرȽϺ�LSA����
	Node_Attr.degree = 1;

	//�����߶��յ���ҵ���ĵĸ���
	Node_Attr.rcvd_pk_num = 0;

	//�������߶˵�ҵ���ĵĸ���
	Node_Attr.send_pk_num = 0;

	///rcvd���ֽ���
	Node_Attr.rcvd_pk_size = 0;
	
	//send���ֽ���
	Node_Attr.send_pk_size = 0;
	

	RREQ_Seq = 0;
	Bcast_Seq = 0;
	
	Is_Routing = 0;
	RREQ_Dest_ID = 0xffff;
	
	memset((unsigned char *)Bcast_ForwardSeq_Record,0,sizeof(Bcast_ForwardSeq_Record));
	memset((unsigned char *)Bcast_ReportSeq_Record,0,sizeof(Bcast_ReportSeq_Record));

	//LSA���Ϳ��أ�0-�رգ�1-��
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


	//����LSA\RREQ��ʱ��
	createWds();
	
	srand(time(NULL));

	
	Print_Node_Attr();

	//��ȡ������Ϣ���е�id
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

	//�����׽��֣�����㲥
	if (setsockopt(Sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"setsockopt() failed to set SO_BROADCAST ");
		return -1;
	}*/

	/*
	wdStart������flagΪTIMER_ONE_SHOT,������ʱ������һ�����ڣ���ʱ����ʱ��ص�δ����״̬
	*/

	//�㲥ʱ��, ��ǰ��1�룬����10�룻�漰���ŵ�����
	//�뾶-ZRP�����֧��9������˸��ݹ����̳�������������Ϊ3
	//���ݰ���С��256/64���ϲ������Ҫ��֡����
	//wdStart(wd_LSA, PERIOD_LSA+(rand()&0x1f),TIMER_ONE_SHOT);
	wdStart(wd_LSA, PERIOD_LSA,TIMER_ONE_SHOT);
	return 1;
}


void handleWLanData(unsigned char *msgBuff,int msgLen)
{
		unsigned char	*wLanData_pkptr = msgBuff;
		
		u8 send_id, recv_id;

		//Ĭ���յ����ݰ�ʱ������Ϊ�ŵ�״̬�ص���ʼ״̬��TOKEN_RING_INIT��
		// 1. ����Route��ͷ���ͣ�������Դ��Ŀ��ID������������Ϣ
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
			//�յ����п�ʼָ���ʼ���ڵ�ҵ����ŵ�Ƶ�׸�֪���̣��������ϱ���
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
			//�յ������ϱ�������ж�
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
			//�յ������֪�·��Ľ�����

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
����Lan��RAWSOCK������·��ģ��Ѱ·������·��ͷ,������msgBuff[0...99]
*/
void handleLanData(unsigned char *msgBuff,int msgLen)
{
		unsigned char	*APP_pkptr;		//�������ָ��
		APP_pkptr = msgBuff;				//ָ��ָ�������׵�ַ
		//hexprint(APP_pkptr,msgLen);
					
		APP_PK_Send(APP_pkptr, msgLen);	//����APP_PK_Send����������·�ɹ��̣�����IARP��IERP·�ɱ��...
}

void handleSelfData(unsigned char *msgBuff,int msgLen)
{
	//hexprint(APP_pkptr,msgLen);

	if(msgBuff[0] == 0)	// LSA_PK_Send��ʱ����ʱ
	{
		LSA_PK_Send();
	}
	else if(msgBuff[0] == 1)	// RREQ_Timeout��ʱ����ʱ
	{
		RREQ_Timeout();
	}
	else
	{
		LOG_MSG(WARN_LEVEL,"unknown pk from self\n");
	}
}

/* ·���ŷ��߳� ������*/
void Route_Serv()
{
	int				msgLen;
	unsigned char	msgBuff[BUFF_ITEM_SIZE];	 //��������LanRcvThread��WiFiRcvThread����Ϣ
	unsigned short	msgType;
	long long wstart,wend;
	long long lstart,lend;
	struct 		q_item item;	
	// ��·��Э����صı������г�ʼ���������Ӻ�������1Hop��mHop�ھӺ�IARP��IERP·�ɱ�APP_BUFF���������г�ʼ��
	if (0 == Route_Init())
	{	
		LOG_MSG(ERR_LEVEL,"Route_Init error, please restart WiBox\n");
		return;
	}
	
    //test_update_mympr();
    
   
    
	for(;;)
	{
		
		memset(&item,0,sizeof(item));	
		//��4������type 0��fifo��-1����Ϣ����С���ȣ�tpye >0 ������ �����͵���Ϣ��
		//�ɹ�ʱ������Ч��Ϣ�ĳ��ȡ����򷵻�-1
	
//		msgLen = msgrcv(msgQ_Route_Serv,&item,MAX_MSG_LEN,0,MSG_NOERROR);

		//wjw:�����߽��յ���Ϣ���ȼ���ߣ�Ȼ���Ǵ�LAN�ڽ��յ����ݣ������LSA��RREQ���Ʊ���
		msgLen = msgrcv(msgQ_Route_Serv,&item,MAX_MSG_LEN,-3,MSG_NOERROR);

		//printf("recv msgLen = %d, item.type = %d\n",msgLen,item.type);
		
		if(msgLen > 0)
		{
			/*
			wujiwen: 
				ÿ��msgBuffΪ1600�ֽڣ���С�����ҵ���ĳ���
				��ȡ����RECV_CQ��Ϣ���е����ݣ���msgBuff�ĵ�100�ֽڿ�ʼ�洢
				msgBuff��ͷ100�ֽ�Ϊ·��ͷ
			*/	
			memcpy(msgBuff+100,item.data,msgLen);
		
			switch(item.type)
			{
				case WLAN_DATA:
					//from wlan data route header + row socket data;or route ctrls
					//�������ߵĶ��е����ݱ�����RAWSOCK�����ݣ������߽��յ����ݰ�ȥroutehead����Ϣʣ�µ���Ч����
					//wstart = sysTimeGet();
				
					handleWLanData(msgBuff+100,msgLen);
					
					//wend = sysTimeGet();
					//printf("wlan cost time = %lld\n",wend - wstart);
					break;
				case LAN_DATA:
					//from lan data raw sock
					//����·��ģ��Ѱ·������·��ͷ+RAWSOCK�����ݡ�
					//lstart = sysTimeGet();
					
					//void handleLanData(unsigned char *msgBuff, int msgLen);
					/*
					wujiwen:
					����msgBuff���ݣ�������·��ͷ
					msgBuff[0...99]���������·��ͷ
					msgBuff+100 ���ʵ�Lan�� RAWSOCK���ݰ�
					msgBuff+100-ROUTE_HEAD_LEN���ʴ���ROUTE_HEAD + RAWSOCK����
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


/* ��ʼ��һ���ھӱ�*/
List* Init_1HopNb_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* ��ʼ�������ھӱ�*/
List* Init_mHopNb_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}


/* ��ʼ�����·�ɱ� */
List* Init_IERP_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* ��ʼ��APP_BUFF���Ļ�����*/
List* Init_APP_Buff()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* ��ʼ��RREQ��¼��*/
List* Init_RREQ_Record_Tab()
{
	List*	ret;
	
	ret = prg_list_create();

	return ret;
}

/* �Ƚ�����rreq_seq��bc_seq���¾ɣ���ͬ�ڴ�С.��x�������������ĵ���ţ�y�Ǽ�¼
	x��,����1��
	���,����0��
	x��,����-1.

    �����ϣ�x��y ����ȣ���Ӧ��ת��������������֮ǰ�д�Ĺ㲥���ģ����п����ٴα����ա���ʱ���������Ƿ���ȣ������ɶ���ת����
    �˺���ֻ���ڹ㲥ҵ�����������RREQ����Ϊ�㲥ҵ��������ٶȿ죬�����ڲд汨�Ľ϶࣬RREQ����Ƶ��С�������ܲд汨�ġ�
    
    
	x=yʱ����ȣ�
    x>yʱ����x��;
    x<yʱ����������y�£�����Ϊ���������������ڹ㲥�ı��ĵ�Ӱ�죬��x���������ֵ��ת����x�£�����y��
	65535+x-y<32ʱ�����������ֵ��ת����x�£� 
	65535+y-x<32ʱ�����������ֵ��ת����x�ɣ� 
	�������ʱ��û�о������ֵ��ת
		|x-y|<32 ʱ��x>y����x�£�x<yʱ��x��
		|x-y|>32 ʱ������x�¡���100��300�¡�����Ĭ�������ڲ���ͬʱ����100��300�İ���
						��ע���˴��Ƚ�������x,y��˳�򲻿ɵߵ����������������뱾�ؼ�¼������������Ϊ�������µġ�
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

/* Get_Next_ID()  ----- ��trace_list����ȡ id ����һ��ID��û�ҵ��������������0 
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

/* Get_Prev_ID()  ----- ��trace_list����ȡ id ����һ��ID��û�ҵ��������������0 
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

/* ���������һ���Ͷ����ھӱ���radius���ڵ��ھ���Ϣ������nb_list�� �������MAX_NB_NUM����Ϣ��������Ϣ�������ϵͳ����
	���radius ==1��ֻ��Ҫ����һ���ھӱ�
	���radius >1, ����Ҫ���Ҷ����ھӱ�
	����ֵ��radius���ڵ��ھӸ�������һ�������ھ�
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
		// ��onehop_list����дһ���ھ���Ϣ	
		n = prg_list_size(onehop_list);
		
		for(i = 1; i<= n; i++)
		{
			element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i);
			
			nb_list[num].node_ID = htons(element_ptr1->node_ID);	
			nb_list[num].path_QOS = element_ptr1->rcv_qos;					// [ע] LSA ��nb_list�У�һ���ھӷ����rcv_qos,�����ھӷ�������path_QOS
			nb_list[num].mpr_distance = (element_ptr1->is_my_mpr <<7)+ (element_ptr1->direction<<6)+ 1;
			
			if(++num >= MAX_NB_NUM )
				return num;
		
		}
		
	}
	// ��mhop_list����д�����ھ���Ϣ.����mhop_list�ǰ���hop˳�����У���ˣ�ֱ�Ӵ�ͷ�������ɡ��ڶ�������mhop_list����ͬnode_ID����Ŀhop�ض���ͬ����λ�ý��ڣ����п��԰��ղ����о�

	n = prg_list_size(mhop_list);
	prev_node_id = 0;
	
	for(i = 1; i<= n; i++)
	{
		element_ptr2 = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i);
		if(element_ptr2->hop <= radius)		
		{
			current_node_id = htons(element_ptr2->node_ID);
			if(prev_node_id != current_node_id)		//	�����о�,ǰ��ID��ͬ����ʾΪ��node_ID����¼��nb_list�У����򲻼�¼��
			{
				prev_node_id = current_node_id;
				
				nb_list[num].node_ID = current_node_id;	
				nb_list[num].path_QOS = element_ptr2->path_QOS;
				nb_list[num].mpr_distance = element_ptr2->hop;
				
				if(++num >= MAX_NB_NUM )
					return num;
			}
			else if(nb_list[num-1].path_QOS < element_ptr2->path_QOS)		// ��ǰ��node_ID��ͬ�����սϴ��QOS��ֵ
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
 * ��TCP/UDPЭ���У��涨IP Header�������£�
 * 4-bit version | 4-bit header length | 8bit typeof service | 16bit total length in bytes          |
 * 16-bit identification			   					     | 3-bit flags  | 13-bit fragment offset|
 * 8-bit time to live				   | 8-bit protocol		 |    16-bit header checksum			|
 * 32-bit source IP address																			|
 * 32-bit destination IP address																	|
 * options(if any)																					|
 * data	��UDPͷ+ AppData��
 *
 * *IPͷ��4-bit header length�ֶΣ������£�������IPͷ���ٸ�4���ֽڣ�����Э���е�option(if any)�ֶΡ�
 * 																						|
 * **************************************************************************************************
 * ��TCP/UDPЭ���У��涨UDP HeaderЭ�����£�
 *
 * 16-bit source port number    | 16-bit destination port number |
 * 16-bit UDP length			| 16-bit UDP checksum			 |
 * data															 |
 * ����UDP length ������8�ֽ�UDPͷ+ UDP data�ֽ�
 * ***************************************************************
 * */
/**
 * ����RAW_SOCKET,�ػ������ϵ���IP���ģ����˵�����Ҫ�ı��ģ�Ȼ��ת�����ն��У���route����
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
	
	/*  ��������3�������ģ��Ǵ��ظ���ping.
		1. IPͷ��Э������ = 1
		2. Ŀ��IP�Ǳ���WiBox�ϵ�IP�� ��Ŀ��ID��������IP���λ��1��2
		3. IP���ݵ�һ���ֽ� = 8���ڶ����ֽ� = 0
	*/
	if (	getProto_fromIPHeader(ip_pkptr) == 1
			&& getDestAddr_fromIPHeader(ip_pkptr) == Node_Attr.ID && (ip_pkptr[19] == 1 || ip_pkptr[19] == 2))
	{
		if (icmp_pkptr[0] == 8 && icmp_pkptr[1] == 0)
		{
			/* ����ping_reply: �ߵ�Դ��Ŀ��IP��frag��offsetǿ��Ϊ0�����¼���ipͷchecksum; ICMP��һ���ֽڸ�Ϊ0�����¼����������ĵ�checksum,--����(����60�ֽ�׺0����)*/
		
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

/* Is_1hopNb() --- �ж�id�Ƿ�Ϊ�����һ���ھӣ���˫���ھӷ���2�������ھӷ���1�������ھӷ���0*/
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
/* �ȸ���rcv_state�����rcv_qos*/
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
/* Get_1hopNb_Num() ---- ��ȡһ���ڵ�˫���ھ��� */
int Get_1hopNb_Num(List* onehop_list)
{
	int				i = 1;
	int				num = 0;
	ONE_HOP_NB_ITEM	*element_ptr;
	
	// ɾ��������idΪnode_ID��һ���ھӱ���
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

/** Del_1HopNb_Item() -- ɾ��һ���ھӱ�������node_ID == id�ı��
	����ֵ���ɹ�ɾ���ı������
*/
int Del_1HopNb_Item(List* onehop_list, unsigned short id)
{
	int				i = 1;
	int				num = 0;
	ONE_HOP_NB_ITEM	*element_ptr;
	
	// ɾ��������idΪnode_ID��һ���ھӱ���
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



/** Add_1hopNb_Item() -- ��1��·�ɱ�������һ������˲��������յ�LSA���ĺ���ܷ������Ѿ�����Update_Nb_Tab����ɸ�ǿ��Ĳ���
	
*/


/* Get_MprDistance  -- ��NB_INFO�б��У����ҵ�ַΪid�Ľڵ��mpr_distance�������ظ�ֵ������ʧ�ܷ���0 */
unsigned char Get_MprDistance(NB_INFO *nb_list, int	nb_num, unsigned short id, unsigned char *path_qos)
{
	int		i;

	for(i = 0; i< nb_num; i++)
	{
		if(id == nb_list[i].node_ID)
		{
			if(path_qos != NULL)
			{
				*path_qos = nb_list[i].path_QOS;		/*nb_list[i]��¼�˱��ڵ㷢�͵�LSA�� ����LSA�����ߡ��Ľ������*/
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

/** Del_mHopNb_Item() -- ɾ������·�ɱ�������relay_ID == relay_id, node_ID == node_id�ı��
	����ֵ��ɾ���Ķ����ھӱ���Ŀ��
*/
int Del_mHopNb_Item(List* mhop_list, unsigned short node_id, unsigned short relay_id)
{
	int					i = 1;
	int					n=0,num = 0;
	MULTI_HOP_NB_ITEM	*element_ptr;
	
	// ɾ��������relay_idΪrelay_Id�Ķ����ھӱ���
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

/** Add_mHopNb_Item() -- �ڶ���·�ɱ�������һ�����ע�ⰴ�ȶ�˳����������������֮ǰ���Ѿ��Ƚ���ͬrelay_ID�����һ�Σ���֤û���ظ���	
	����ֵ��1�ɹ���0ʧ��
*/
int Add_mHopNb_Item(List* mhop_list, unsigned short node_id, unsigned short relay_id, unsigned char path_qos, unsigned char hop)
{
	MULTI_HOP_NB_ITEM	*new_elem_ptr;
	MULTI_HOP_NB_ITEM	*element_ptr;
	int					i = 1;
	
	// 1. ��node_idΪ������ֱ�ӷ���0�������
	if(node_id == Node_Attr.ID)
		return 0;
	
	// 2. ��node_idΪ˫��һ���ھӣ���ֱ�ӷ���0�������
	if(Is_1hopNb(oneHopNb_List, node_id, NULL )>1)
		return 0;
		
	// 3. �������������ھӱ�Ѱ�Ҽ�¼��node_ID��ͬ�ı�����Ƚ�hop
	
	i = 1;
	while(NULL != (element_ptr = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i)))
	{
		if(element_ptr->node_ID == node_id ) 
		{
			if(element_ptr->hop < hop)			// 3.1 ��¼����hop��С������ӣ�����0
			{
				return 0;
			}
			else if(element_ptr->hop == hop)	// 3.2 ��¼����hop��ȣ���relay_id��ͬ������ӣ�����0�� ����׼�����
			{
				if(element_ptr->relay_ID == relay_id)
					return 0;
				else
					i++;
			}
			else								// 3.3 ��¼����hop�ϴ���ɾ���ñ��׼������±���
			{
				prg_list_remove(mhop_list,i);
			}
		}
		else
		{
			i++;
		}	
	}
	
	// 4. ��һ�������ϣ�����ɱ�Ҫ��ɾ��������������ȷ��Ҫ����±�����±��ֵ��
	new_elem_ptr = (MULTI_HOP_NB_ITEM*)malloc(sizeof(MULTI_HOP_NB_ITEM));
	new_elem_ptr->node_ID = node_id;
	new_elem_ptr->relay_ID = relay_id;
	new_elem_ptr->path_QOS = path_qos;
	new_elem_ptr->hop = hop;
	
		
	// 5. ȷ������λ�ã�˳�����hop��ͬ��relay_IDҲ��ͬ�ı�������ڣ���嵽�ñ���֮���������ڣ���嵽��hopֵ�����һ��ĺ���
	i = 1;
	while(NULL != (element_ptr = (MULTI_HOP_NB_ITEM*)prg_list_access(mhop_list, i)))
	{
		if(element_ptr->hop < hop)	
		{
			i++;	
		}
		else if(element_ptr->hop == hop)	
		{
			// ����hop��ͬ��relay_IDҲ��ͬ�ı����嵽�ñ���֮ǰ
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
			// �����ڣ��嵽��hopֵ�����һ��ĺ���
			prg_list_insert(mhop_list,new_elem_ptr,i);
			return 1;
		}
					
	}
	
	// 6. ��hopֵ�ڶ����ھӱ�������Ѳ����ڸ������򲻻���else ���˳����ӵ�����ĩβ
	prg_list_insert(mhop_list,new_elem_ptr,LIST_POS_TAIL);
	return 1;
	
}

/* Del_IERP_Item_By_Seq -- ����rreq_src_id��rreq_seq��ɾ����������·�ɡ�����ɾ������Ŀ����*/
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
/* Del_IERP_Item_By_NextID -- ɾ��������next_idΪnext_Id�����·��*/
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

/* Del_IERP_Item_By_DestID -- ɾ��������dest_idΪdest_ID�����·��*/
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

/** Del_1hopNb() -- ɾ��һ��1���ھӣ����������ƺ���
				ɾ��������idΪnode_ID��һ���ھӱ���
				���ɹ�ɾ������
					ɾ��������idΪrelay_ID�Ķ����ھӱ���
					ɾ��������idΪnext_ID�����·�ɱ��
	����ֵ���ɹ�ɾ����һ���ھӱ������
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

/* ɾ��һ���ھӱ��м�¼��ʱ���ھӣ���last_lsa_rcv_time�뵱ǰϵͳʱ��֮�����4*PERIOD_LSA�ļ�¼�������ƺ����. 
    ͬʱ������rcv_state
	����ֵ��ɾ����һ���ھӵĸ���
*/
int	Del_Timeout_1hopNb(List* onehop_list, List* mhop_list, List *ierp_list)
{
	int					i = 1;
	int					num = 0;
	ONE_HOP_NB_ITEM		*element_ptr;
	long long  diff;

	//printf("**********************************************************************************************current = %lld\n",sysTimeGet());
	/*
	 * ����һ���ھ��б����нڵ㣬ͳ�����յ��ĸ����ڵ�LSA��״̬������rcv_QOSֵ
	 * */
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		diff = sysTimeGet() - element_ptr->last_lsa_rcv_time ;
		//printf("**********************************************************************************************diff = %lld\n",diff);
		if( diff >  (RETRY_TIMES*PERIOD_LSA) )
		{
			LOG_MSG(INFO_LEVEL, "delete timeout node %d(dir =%d)\n",element_ptr->node_ID,element_ptr->direction);
									
			// ɾ��������idΪrelay_ID�Ķ����ھӱ���					
			Del_mHopNb_Item(mhop_list, 0xffff, element_ptr->node_ID);
			
			// ɾ��������idΪnext_ID�����·�ɱ��
			Del_IERP_Item_By_NextID(ierp_list, element_ptr->node_ID);
            
            // ɾ����ʱ��һ���ھӱ���. ������ɾ����ģ�����element_ptrָ��Ϊ�գ�����ɾ������
			prg_list_remove(onehop_list,i);
			num++;
		}
		else
		{
			/*
			 * WJW:
			 * �ڷ���LSAǰ�����������ھӵ�rcv_qos���������ϴν���ʱ���>PERIOD+50,ֱ������1λ��������λ��+1��
			 * */

#if 0
			//if (diff > (PERIOD_LSA + rand_t1_max *3.0) )
			if (diff > (PERIOD_LSA+ 100) )
			{
				// ��LSA, ����rcv_state��path_QOS
				printf("diff > PERIOD_LSA,  nb_node %d, rcv_state = %04x,\n",element_ptr->node_ID ,element_ptr->rcv_state);

				/*
				 * WJW��
				 * ĳ���ڵ�LSA��һ��LSA���ڳ�ʱû���յ�������·���޶���ʱ�����жϻ��쳣һ�Σ�rcv_qos ��1���ĸ�����1��Ȼ��
				 * ͨ�����ڵ���LSA����ͨ�������ڵ㣬
				 * */
				element_ptr->rcv_state = element_ptr->rcv_state<<1;
				element_ptr->rcv_qos = Get_Qos(element_ptr->rcv_state);
				element_ptr->path_QOS = (element_ptr->rcv_qos > element_ptr->send_qos)?element_ptr->send_qos:element_ptr->rcv_qos;
			}
			else
			{
				// LSA����������rcv_state��path_QOS
				/*
				 * WJW��
				 * ĳ���ڵ�LSA��һ��LSA�������յ�������·������rcv_qos ��1���ĸ�����1��Ȼ��
				 * ���ڵ���LSA����ͨ�������ڵ㣬
				 * */
//				printf("diff < PERIOD_LSA+ 2.0*rand_t_max,  nb_node %d , rcv_state = %04x, ",element_ptr->node_ID ,element_ptr->rcv_state);
				element_ptr->rcv_state = (element_ptr->rcv_state<<1) +1;

				//ͳ��rcv_state��1�ĸ���
				element_ptr->rcv_qos = Get_Qos(element_ptr->rcv_state);
				element_ptr->path_QOS = (element_ptr->rcv_qos > element_ptr->send_qos)?element_ptr->send_qos:element_ptr->rcv_qos;
			}
#else
				element_ptr->rcv_state = (element_ptr->rcv_state<<1) +1;
				//ͳ��rcv_state��1�ĸ���
				element_ptr->rcv_qos = Get_Qos(element_ptr->rcv_state);
				element_ptr->path_QOS = (element_ptr->rcv_qos > element_ptr->send_qos)?element_ptr->send_qos:element_ptr->rcv_qos;

#endif

			i++;
		}
	}
	
	return num;
}

/* �ж�ĳid�Ƿ��������ǰnum��Ԫ����
	���أ��������ڣ�����1�����ڣ�����0
	ע��num > MAX_NB_NUMʱ���㷵��1�����㲻���ε����
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
/** Get_2HopNb_Num() ������relay_listΪrelay_ID�������ھӵ�ĸ���
	���أ�����
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
				// relay_ID��relay_list�б��ڣ���node_ID��Ϊ2���ھӣ�����¼
				if(tempBool[element_ptr->node_ID] ==0)
				{
					tempBool[element_ptr->node_ID] = 1;	
					ret++;
				}
			}
		}
		else
		{
			// ����mhop_list��hop�������У����з���hop����2ʱ������ֱ����ֹ����
			break;	
		}
		i++;
	}
		
	return ret;

}

/*  �ж�mask��������չ��������'1'�ĸ����Ƿ�Ϊrelay_num��
		��mask��������չ����ͳ����'1'�ĸ�����
		��==relay_num���򷵻�1�����򷵻�0
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

/*  ����mask����old_relay_list�г�ȡrelay_num��Ԫ�أ���Ϊnew_relay_list.����ֵΪ relay_num
		��mask��'1'�ĸ��� == relay_num�����������չ��ʽ��'1'��λ����Ϊ��������old_relay_list�г�ȡrelay_num��Ԫ�أ���Ϊnew_relay_list
		���򣬷���0*/
int Get_New_RelayList_By_Mask(unsigned int mask, int relay_num, unsigned short *old_relay_list,unsigned short *new_relay_list)
{
	unsigned char	i=0, num =0;
	
	if(Check_Mask_By_RelayNum(mask, relay_num))
	{
		while(mask)
		{	
			if((mask&1) == 1)
			{
				// ������չ���ĵ�i��λ��Ϊ'1'����new_relay_list��ֵ
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

/* ����mask��onehop_list->is_my_mpr��ֵ��maskΪ'1'��λ��is_my_mpr��1. ע�⣬�۳������ھ�*/
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
                // ������չ���ĵ�i��λ��Ϊ'1'����onehop_list�ĵ�i��Ԫ�ص�is_my_mpr��ֵ
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

	����MPR�ڵ������ֲ�����
	   1. �ҵ�MPR��˭���ڷ���LSA����ʱ������Щ�ڵ����Խ���ע����
	   2. ����˭��MPR��ͨ�������ھӷ�����LSA���ģ���������¼���ǲ���ĳ�ڵ��MPR
*/

/* Update_myMPR 
 *  ����һ���ھӱ�Ͷ����ھӱ����㱾�ص�MPR�ڵ㼯��������һ���ھӱ�����Ӧ�ھӽڵ�Ԫ���е�is_my_mpr��0(�����ҵ�MPR)��1(���ҵ�MPR)
 *    �㷨�п�����Ҫ���ھӱ��еĲ���Ԫ����ȡ���������һ�ָ��ʺϼ���Ľṹ
 *  ����ֵ��myMPR�ĸ���
 */

/*
 * wjw;
 * OLSR·��Э����һ�ֻ�����·״̬�㷨�ı�����Э�飬OLSRʹ������·�ɣ���ÿ���ڵ�ʹ���䱾����ϢΪ����
 * ѡ����·�ɡ���OLSR�У�ÿ���ڵ�ͨ�������Եط����Լ�������Ϣ�ﵽ·�ɱ���µ����á�������ÿ���ڵ��1���ھӽڵ���
 * ѡ�񲿷ֽڵ���ΪMPR��MultiPoint Relay, ����м̣���ֻ�б�ѡΪMPR�ؽڵ�Ÿ���ת��������Ϣ��
 * ����MPR��Ҫ�����������б��浱ǰ����·״̬��
 * MPR�ṩһ�ָ�Ч�ؿ�����Ϣ������ƣ���������Ҫ��Ĵ�������
 * */

int Update_myMPR(List* onehop_list, List* mhop_list)
{
	/* 
		1. �ȸ���onehop_list����ֵΪһ��һ���ھ�ID����(������˫���ھ�)�����ں�������
		2. ���������ھӱ������κ����Σ����������ھӵ�ĸ���twohop_NUM;
		3. ����С�����˳���relay_list��ֵ�����������ھӵ�ĸ���twohop_num��
		4. ��num == Nnumʱ����relay_list����MPR��������onehop_list���˳�.
	*/
	int				i = 1;
	unsigned int	j;
	int				onehop_num = 0, twohop_NUM, twohop_num, relay_num;
	ONE_HOP_NB_ITEM	*element_ptr;
	unsigned short		relay_list[MAX_NB_NUM];
	unsigned short		new_relay_list[MAX_NB_NUM];
	
	unsigned int	relay_mask;
	
	// 1. �ȸ���onehop_list����ֵΪһ��һ���ھ�ID����(������˫���ھ�)�����ں�������
	while(NULL != (element_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(onehop_list, i)))
	{
		if(element_ptr->direction == 1)
		{
			relay_list[onehop_num++]=element_ptr->node_ID;	
		}	
			
		i++;	
	}
	
	// 2. ���������ھӱ������κ����Σ����������ھӵ�ĸ���twohop_NUM;
	twohop_NUM = Get_2HopNb_Num(mhop_list, relay_list, MAX_NB_NUM+1 );
	
	if(twohop_NUM == 0)
	{
		Set_Mpr_By_Mask(onehop_list, 0);
		return 0;
	}
	// 3. �����ٵ����˳���relay_list��ֵ�����������ھӵ�ĸ���twohop_num����(twohop_num == twohop_NUM)�����أ������մ�ʱ�������һ���ھ�my_MPR�ֵ
	relay_num = (onehop_num >32)?32:onehop_num;
	relay_mask = pow(2,relay_num);	
    
	for(i = 1; i<= relay_num; i++)
	{
		// 3.1 ��һ���Ż���mask������1���������Ǹ�����ֵ��������1��ʼ����2^n����
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
 *  ����һ���ھӱ��ж����ǲ���ĳ���ڵ��MPR�����򷵻�1�����Ƿ���0��
	Ϊ����ӦDSR·�ɣ�IARP_radius = 0 ʱ��ҲӦ����1.
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

/** Update_Nb_Tab ---- �����յ���LSA���ĺ󣬸���һ���ھӱ������ھӱ�(����·�ɱ�)�����·�ɱ�

	����
		1. ����һ���ھӱ����Ƿ��и�LSA���ķ����ߵļ�¼�����У�����¸ü�¼�����ޣ������һ����¼�������¡�
		2. ǰ����ֶ�ֱ�Ӵ��յ���LSA��������ȡ��ֵ���������� nb_list��(mpr_distance&0x0F) == 1 ��ID�б����Ƿ��������ID��
			����������direction��1��
				��mpr_distance == 1����is_your_mpr��0��
				��mpr_distance == 0x81����is_your_mpr��1��
                ɾ��������send_IDΪ Ŀ��ID�Ķ����ھ� �� IERP ���
				����������send_IDΪrelay_ID�Ķ����ھӱ������ȫɾ���ٸ���nb_list�����ӣ�
			���򣬽�direction��0.
				��is_your_mpr��0��
				��is_my_mpr��0;
				ɾ��������send_IDΪrelay_ID�Ķ����ھӱ���
				ɾ��������send_IDΪnext_ID�����·�ɱ���
		3. ��last_lsa_rcv_time��ֵΪ��ǰϵͳʱ��
		4. ���� Update_myMPR()���㱾�ص�MPR�ڵ㼯��������һ���ھӱ��е� is_my_mpr�ֶ�

*/
void Update_Nb_Tab(LSA_PK *lsa_pkptr, unsigned short lsa_pklen)
{

	unsigned short	send_id;			// ���͸�LSA���ĵ�ID
	
	unsigned char	nb_num;				// �����ھӸ���,����N���ھӣ��������ô���
	NB_INFO			nb_list[MAX_NB_NUM];			// �����ھ��б�,�����250����ռ��1000�ֽ�
	
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
		 * nb_list[].path_QOS��¼��LSA�������յ��������ڵ����·״̬rcv_QOSֵ
		 * */
		nb_list[i].path_QOS = lsa_pkptr->nb_list[i].path_QOS;	
		nb_list[i].mpr_distance = lsa_pkptr->nb_list[i].mpr_distance;	
	}

	// 1. ����һ���ھӱ����Ƿ��и�LSA���ķ����ߵļ�¼
	if (Is_1hopNb(oneHopNb_List, send_id, &i) > 0)
	{
		// �м�¼��ֱ�Ӹ���
		/*WJW��
		 * 1����һ���ھӱ�����LSA_Sender�ļ�¼,���Ȼ�ȡ��Ӧ��¼��,i�����±�ֵ
		 * */
		new_elem_ptr = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i);
        

		/*
		 * WJW:
		 * 2> ���ұ��ڵ���LSA_Sender�е��ھ�״̬,�����Ǹ���Node_Attr.Id����nb_list;
		 * 	  �������һ���ھӱ��б��ڵ�-> LAS_Sender��·��SendQos, �����ǲ���nb_list,
		 * 	  ��ý��ձ��ڵ�LSA�Ĵ�����
		 * 	  ��󷵻�mpr_distance
		 *
		 *
		 * 	  typedef struct{
				unsigned short	node_ID;			// �ڵ�ID�����ھӽڵ�ID
				unsigned char	path_QOS;			// �ڵ�������LSA������send_ID��node_ID����·������
				unsigned char	mpr_distance;		// ����ֶΣ�bit7���Ƿ�ΪLSA���ķ����ߵ�MPR�ڵ㣬0���ǣ�1��,ֻ��һ���ھӲ��п�����MPR��
                                        			//           bit6: �Ƿ�ΪLSA���ķ����ߵ�˫���ھӣ�0���ǣ�1�ǡ�ֻ��һ���ھӲ��п�����1
													//			bit3~0:��LSA���ķ����ߵ���������ʾΪ�����ھ�

				}NB_INFO;						// �ھ��б���Ϣ��������䱨������,���ھӱ��ж�ȡ
		 *
		 *
		 *
		 *
		 * */
        // ����LSA�����е�nb_list, ȷ��mpr_distance����������NB��Ϣ
		/*
		 * WJW��
		 * ����nb_list[].path_QOS��¼��LSA�������յ��������ڵ����·״̬rcv_QOSֵ
		 * ����Get_MprDistance������ָ�����ҡ��ҡ����ڵ���LSA������nb_list[]�еļ�¼������:
		 * 1> LSA�յ��Ĵ��������ΪSend_QOS
		 * 2> ��·�Ƿ�˫���ͨ
		 * 3> ����
		 * 4> mprֵ
		 * */
        mpr_distance = Get_MprDistance(nb_list, nb_num, Node_Attr.ID, &(new_elem_ptr->send_qos));

        /*
         * ��ȡmpr_distance &0xFֵ�� �����ڵ㵽 LSA Sender������
         * */
        dir  = ((mpr_distance&0x0F) == 1)?1:0;
        
        if(new_elem_ptr->direction == 0)    // ԭ���ǵ���Ϊ�����ͨҪ�󣬼�ʹdir==1����qos̫�ͣ�Ҳ��dir =0;
        {
//        	if(dir == 1 && new_elem_ptr->send_qos >= 6 && new_elem_ptr->rcv_qos >= 5)
        	/*
        	 * WJW:
        	 * ע��new_elem_ptr->send_qos �������Node_Attr.ID->LSA������,�����ɺ��߷�������rcv_QOS
        	 * ע��new_elem_ptr->rcv_qos  �������Node_Attr.ID rcv_QOS����LSA_PK_Send()��ͳ��
        	 * 	���ڵ��յ������ڵ�LSA�Ĵ�������rcv_qos
        	 * */
            if(dir == 1 && new_elem_ptr->send_qos >= 2&& new_elem_ptr->rcv_qos >= 1)
            {
                // ��rcv_qos=5ʱ���յ���LSA�� 1�����ڱ��ڵ�LSA����ʱ����+1���6. ��send_qos�����Ǵ���6ʱ�ſ��ԡ�
                dir = 1;    
            }
            else
            {
                dir = 0;
            } 
        }
        
        if(new_elem_ptr->direction != dir)
        {
            // ��direction �����ı䣬��ӡ
             LOG_MSG(INFO_LEVEL, "node %d change dir from %d to %d(my send_qos=%d, Dude rcv_qos=%d)\n",
                        send_id, new_elem_ptr->direction, dir, new_elem_ptr->send_qos, new_elem_ptr->rcv_qos);
        }
        
	}
	else
	{
		// �޼�¼��ֱ�����
		new_elem_ptr = (ONE_HOP_NB_ITEM*)malloc(sizeof(ONE_HOP_NB_ITEM));
		prg_list_insert(oneHopNb_List, new_elem_ptr, LIST_POS_TAIL);

		/*
		 * WJW��
		 * ��һ���յ�ĳ�ڵ��LSA������һ���ھ��б�ĩβ��ӣ�����
		 * */
		new_elem_ptr->rcv_state = 0x0020;   // ������9s���յ�4��
		new_elem_ptr->rcv_qos = 1;
        
        // ����LSA�����е�nb_list, ȷ��mpr_distance����������NB��Ϣ
		/*
		 * WJW:
		 * ��һ���յ�ĳ�ڵ��LSA������LSAЯ����nb_list,���ұ��ڵ��ڵ������·״̬��
		 * ���������new_elem_ptr->send_qos��mpr_distance������send_qos��ӳ��·����
		 * */
        mpr_distance = Get_MprDistance(nb_list, nb_num, Node_Attr.ID, &(new_elem_ptr->send_qos)); 
        
        dir = 0;    // �״��յ�LSA��qos��Ϊ2������Ϊ�ǵ���ֱ������4������Ϊ��ͨ
        
        LOG_MSG(INFO_LEVEL, "add new node %d\n",send_id);
	}

	// 2. ����LSA������Ϣ������һ���ھӱ��ֵ, �����¶����ھӱ�����·�ɱ�
	new_elem_ptr->node_ID = send_id;
	new_elem_ptr->cluster_state = lsa_pkptr->cluster_state;
	new_elem_ptr->cluster_size = lsa_pkptr->cluster_size;
	new_elem_ptr->cluster_header = ntohs(lsa_pkptr->cluster_header);
	

	new_elem_ptr->degree = lsa_pkptr->degree;
	new_elem_ptr->last_lsa_rcv_time = sysTimeGet();
	
	new_elem_ptr->path_QOS = (new_elem_ptr->send_qos > new_elem_ptr->rcv_qos)? new_elem_ptr->rcv_qos: new_elem_ptr->send_qos;
	
	if(dir == 1)
	{
		// ���ڵ�����һ���ھӱ��ڣ�����˫���ھ�
		new_elem_ptr->direction = 1;
		new_elem_ptr->is_my_mpr = 0;
		
		if((mpr_distance>>7) == 1)		// ���ڵ���send_id��mpr
			new_elem_ptr->is_your_mpr = 1;
		else							// ���ڵ㲻��send_id��mpr
			new_elem_ptr->is_your_mpr = 0;
		
		// ɾ��������send_IDΪnode_ID�Ķ����ھӱ����Ϊ�����send_IDΪһ����Ӧ����һ���ھӱ���
		Del_mHopNb_Item(mHopNb_List, send_id, 0xffff);
        
        // ɾ��������send_IDΪdest_ID��IERP�����Ϊ�����send_IDΪһ����Ӧ����һ���ھӱ���
		Del_IERP_Item_By_DestID(IERP_List, send_id);
		
		// ������send_IDΪrelay_ID�Ķ����ھӱ����ɾ�����ٸ���nb_list������
		Del_mHopNb_Item(mHopNb_List, 0xffff, send_id);
				
		for(i = 0; i<nb_num; i++)
		{
            if((nb_list[i].mpr_distance & 0x7F)== 1)        // LSA�����ߵĵ���һ���ھӣ��Թ�.˫��һ����Ϊ0x41��0xA1
            {
                continue;
            }
            else 
            {
                //ȡ����·��Я����·QOS�Ľ�Сֵ
                qos = (nb_list[i].path_QOS > new_elem_ptr->path_QOS)?new_elem_ptr->path_QOS:nb_list[i].path_QOS;	
                hop = 1+(nb_list[i].mpr_distance&0x0F);
                    
                Add_mHopNb_Item(mHopNb_List,nb_list[i].node_ID, send_id, qos, hop);
                
                // ɾ��������nb_list[i].node_IDΪdest_ID��IERP�����Ϊ��ID�����ڣ�Ӧ���ڶ����ھӱ���
                Del_IERP_Item_By_DestID(IERP_List, nb_list[i].node_ID);
            }
		}
		
	}
	else
	{
		// ���ڵ㲻����һ���ھӱ��ڣ����ǵ����ھӣ�����һ���ھӱ���
		new_elem_ptr->direction = 0;
		new_elem_ptr->is_your_mpr = 0;
		new_elem_ptr->is_my_mpr = 0;
		
		// ɾ��������send_IDΪrelay_ID�Ķ����ھӱ���
		Del_mHopNb_Item(mHopNb_List, 0xffff, send_id);
		
		// 	ɾ��������send_IDΪnext_ID�����·�ɱ���
		Del_IERP_Item_By_NextID(IERP_List, send_id);
		
	}
	
	// 3. ���ݸ��º��һ���Ͷ����ھӱ�����MPR
	Update_myMPR(oneHopNb_List,mHopNb_List);	
	
}



/* ��ʱ����Is_Routing == 1����ɾ��APP_BUFF������Ŀ��ID��RREQ_Dest_ID�ı��ģ����ϱ��������ɴ� ICMP����*/
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




/* ��APP_BUFF��Ŀ��ID��dest_ID�ļ�¼�Ƴ�����·��ͷ��ֵ�󣬷���WiFi, ·��ͷ�����������ֵ
	����ֵ���Ƴ����ĵĸ���
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

/* ɾ��APP_BUFF������Ŀ�Ľڵ���dest_ID�ı���
	����ֵ���Ƴ����ĵĸ���
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

/** Route_Search() -- Ѱ·��ΪAPP��ͷ��װ�ṩ��Ҫ��Ϣ��ע�⣬������Ѱ·Ϊ���·�ɣ�Ҫ����IERP�е��������ʱ��
	���룺dest_id,
	�������һ��ID����������·���б�RREQԴID(ֻ�����Ѱ·ʱ��Ч)
	����ֵ��rreq��š���rreq_seq = 0 ʱ����ʾΪ����·�ɣ�·���б���Ч��1~65535Ϊ��䡢�ִػ���·��,·���б���Ч; -1Ϊ��·�ɣ�����ʧ��
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
	
	// 1. ��Ŀ��Ϊ�㲥����ֱ�ӷ��ͣ���һ��Ҳ�ǹ㲥
	if(dest_id == 0xffff)
	{
		*next_idptr = 0xffff;
		*rreq_src_idptr = 0xffff;
		*hopptr = MAX_HOP-1;
		
		return 0; 
	}
	// Ŀ��ID���Լ���������.APP_PK_Send�е���ǰ�����Ρ��˴��޷�����

	// 2. ����һ���ھӱ���node_ID == dest_id ��˫���ͨ��������ڣ�ֱ�ӷ��ص�һ�����
	i = 1;
	qos = 0;
	while(NULL != (element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
	{
		if(element_ptr1->node_ID == dest_id && element_ptr1->direction == 1)
		{
			if(qos < element_ptr1->path_QOS)		// ����ѣ�QOS��󣩵�·��
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
		return 0;		// ����·��
	
	// 3. ���Ҷ����ھӱ���node_ID == dest_id��������ڣ�ֱ�ӷ��ص�һ�����
	i = 1;
	qos = 0;
	while(NULL != (element_ptr2 = (MULTI_HOP_NB_ITEM*)prg_list_access(mHopNb_List, i)))
	{
		if(element_ptr2->node_ID == dest_id )
		{
			if(qos < element_ptr2->path_QOS)		// ����ѣ�QOS��󣩵�·��
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
		return 0;		// ����·��
	
	
	// 4. ��������·�ɱ��� dest_ID == dest_id��������ڣ�ֱ�ӷ��ص�һ�����
	i = 1;
	qos = 0;
	while(NULL != (element_ptr3 = (IERP_ITEM*)prg_list_access(IERP_List, i)))
	{
		if(element_ptr3->dest_ID == dest_id )
		{
			if(qos < element_ptr3->path_QOS && *hopptr <= element_ptr3->hop )		// ����ѣ�hop��С����ѡQOS��󣩵�·��
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
		return rreq_seq;		// ���·��
	
	return -1;
}

/* ����rreq_src_id��rreq_dest_id����RREQ��¼��,�������ҵ��ĵ�һ������λ�ã�û���򷵻�-1.
	
	���أ��ü�¼�������е�λ�ã�û���򷵻�-1
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


/* ���ݽ��յ���RREQ���ģ�����RREQ��¼��
	RREQ��¼���ܽ�����rreq_src_ID��seq���֣���Ҫ����rreq_dest_ID,��Ϊһ���ڵ�϶�������Ŀ��Ѱ·������ͬĿ��ID��Ѱ·���̣����Ƚ�seq����ȷ���¾�
	1. ����src_ID��dest_ID����RREQ_Record��¼��
	2. ����¼�����޴˼�¼�����½�һ���ڴ棬��ֵ�����RREQ����ĩβ��������1��
	3. ���м�¼����������������¸ü�¼
			�������еĵ�rreq_seq���£������REQ_Record��������1��
			��rreq_seq��ͬ�����±��ĵ�hop��С�������REQ_Record��������1��
								hop��ͬ��ϴ󣬲�����0��
			�������еĵ�rreq_seq�Ͼɣ�������0��
			
	����ֵ��0��δ�������£�1����������
    
    ע�����hop,trace_list,path_QOS.���뱾�ڵ��Ӱ��
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
	
	
	// 1. �ڵ��յ��󣬽�����src_ID,dest_ID,rreq_seq��hop;
	
	src_ID = ntohs(rreq_pkptr->src_ID);
    send_ID = ntohs(rreq_pkptr->send_ID);

	dest_ID = ntohs(rreq_pkptr->dest_ID);
	rreq_seq = ntohs(rreq_pkptr->rreq_seq);

	hop = rreq_pkptr->hop;
	path_qos = Get_Path_QOS(send_ID);
    path_qos = (rreq_pkptr->path_QOS > path_qos)?path_qos:rreq_pkptr->path_QOS;
	// 2. ����src_ID��dest_ID����RREQ_RECORD���ҵ���һ����¼��

	record_pos = Search_RREQ_Record(RREQ_Record_List, src_ID,dest_ID);	//����RREQ�����Դ�ڵ�id��Ŀ��id������RREQ����
	record_ptr = (RREQ_RECORD *)prg_list_access(RREQ_Record_List, record_pos);
    
    
	if(record_pos == -1)
	{
		// 3. ����¼�����޴˼�¼�����½�һ���ڴ棬��ֵ�����RREQ����ĩβ��������1��
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
		// 4. ���м�¼���������ڵ�rreq_seq���£���rreq_seq��ȵ��±��ĵ��������٣���rreq_seq��hop����ȵ�path_QOS �ϴ�ʱ��
        //              ����RREQ����ĵ�record_pos��Ԫ��,������1��
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
			record_ptr->hop = hop+1;        // hop+1 ����
			for(i = 0; i< hop;i++)
			{
				record_ptr->trace_list[i] = ntohs(rreq_pkptr->trace_list[i]);
			}
			
			record_ptr->trace_list[hop] = Node_Attr.ID;     // ĩβ���ϱ���ID 
			
			return 1;
			
		}
		else
		{
			// 5. �����������������0
			return 0;
		}
	}
	
}

int Check_Cluster_2ZRP()
{
    // ��������ھӶ��Ƿ�Խ�ߣ����ػ��أ�
	
    if(Node_Attr.cluster_state == CLUSTER_NONE)
    {
        // 2.1 ԭ����ZRP��degree������ʱ���Ծ�Ϊ��ͷ
        if(Node_Attr.degree >= ZRP_TO_CLUSTER_NUM)
        {
            Node_Attr.cluster_state = CLUSTER_HEADER;
            Node_Attr.cluster_size = Node_Attr.degree;
            Node_Attr.cluster_header = Node_Attr.ID;
        }
    }
    else if(Node_Attr.cluster_state == CLUSTER_MEMBER || Node_Attr.cluster_state == CLUSTER_GATEWAY)
    {
        // 2.2 ԭ���Ǵس�Ա������
        if(Is_1hopNb(oneHopNb_List,Node_Attr.cluster_header, NULL) == 2)
        {
            // 2.2.1 �����ͷ�������Ƚ�������ԭ��ͷ���¸��߾�֮
            if(Node_Attr.degree >= ZRP_TO_CLUSTER_NUM && Node_Attr.ID < Node_Attr.cluster_header)
            {
                Node_Attr.cluster_state = CLUSTER_HEADER;
                Node_Attr.cluster_size = Node_Attr.degree;
                Node_Attr.cluster_header = Node_Attr.ID;
            }
        }
        else
        {
            // 2.2.2 ��ԭ��ͷ�Ͽ����ӣ��ָ���ZRP
            Node_Attr.cluster_state = CLUSTER_NONE;
            Node_Attr.cluster_size = 0;
            Node_Attr.cluster_header = 0xffff;	
        }
        
    }
    else if(Node_Attr.cluster_state == CLUSTER_HEADER)
    {
        // 2.3 ԭ���Ǵ�ͷ����degree�������ޣ�����ָ���ZRP
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
	// ��Ϊ��Route_Serv����Ϣ�����С���������Ϣ�ܴ����޷�������

	if(	global_running_state == NET_NORMAL_RUNNING)
	{
		unsigned char msg[4];
		msg[0] = 0;

		msgQ_snd(msgQ_Route_Serv, msg, sizeof(msg),SELF_DATA, NO_WAIT);
	//	wdStart(wd_LSA, PERIOD_LSA+(rand()&0x1f),TIMER_ONE_SHOT);
	}
	/*
	 * LSA���ĸ������ڹ̶�Ϊ1��
	 * */
	wdStart(wd_LSA, PERIOD_LSA,TIMER_ONE_SHOT);
}
/** int	LSA_PK_Send()	-- ��װLSA����,��ͨ�����߶˷��͡�ͬʱ�����һ��ӹ��ܣ�ɾ����ʱ�ھ�
	
	����ֵ: ���ͱ����ֽ���
	����
		1. ɾ��һ���ھӱ���last_lsa_rcv_time�뵱ǰϵͳʱ��֮�����3*PERIOD_LSA�ļ�¼�������ƺ����.[�ɷ��ʹ�����ʱ�Ƚ��մ����ɿ���]
		2. ��ONE_HOP_NB_TAB��ʣ���¼����˫���ͨ��һ���ھӣ��������ΪLSA�����е�degree�������󲿷��ֶξ���NODE_ATTR��ֱ�Ӷ�ȡ��
		3. ��nb_list�ĸ�ֵ���ҵ�����IARP_FSR_Radius���ڵ��ھӣ���������Ϣ�����б��ڣ�����node_ID��node_qos��mpr_distance��
			3.1 ��������˫��һ���ھӣ�distanceΪ1������ONE_HOP_NB_TAB->is_my_mpr == 1�Ľڵ��Ǳ��ڵ��MPR��Ҫ��mpr_distance�����λ��1��
			3.2 ������˫�������ھӣ�����MULTI_HOP_NB_TAB���ҵ�����distanceΪ2��node_ID�����뱨���ڣ�ע��������ͬnode_ID��distance����Ŀ�����ж����ֻ����һ�����ɡ�
			3.3 ��IARP_radius����1����������distance��ͬ��node��Ϣ��
		4. ����node_num,�����CRC��
		5. �������߶˷��ͺ������㲥����
		6. ���ö�ʱ����PERIOD_LSA���ٴ�ִ��LSA_PK_Send
*/
void LSA_PK_Send()
{
	int ret=0;
//	struct msqid_ds ds;

	LSA_PK	send_pk;
	unsigned short *pkptr = (unsigned short *)&send_pk;
	int	len;
	// 1. ɾ��һ���ھӱ��м�¼��ʱ���ھӣ���last_lsa_rcv_time�뵱ǰϵͳʱ��֮�����3*PERIOD_LSA�ļ�¼�������ƺ����
	Del_Timeout_1hopNb(oneHopNb_List, mHopNb_List, IERP_List); 	// ����ɾ���ھӵĸ���
    
	// 2. ��ONE_HOP_NB_TAB��ʣ���¼����˫���ͨ��һ���ھӣ��������ΪLSA�����е�degree�������󲿷��ֶξ���NODE_ATTR��ֱ�Ӷ�ȡ��
	Node_Attr.degree = Get_1hopNb_Num(oneHopNb_List)+1;
    
    // 3. ����ƽ��ִ�·��״̬����ֹ�ھ�ͻȻȫ���ػ�������
    Check_Cluster_2ZRP();
	
	send_pk.pk_type = LSA_PK_TYPE;
	send_pk.degree = Node_Attr.degree;
	send_pk.send_ID = htons(Node_Attr.ID);
	send_pk.cluster_state = Node_Attr.cluster_state;
	send_pk.cluster_size = Node_Attr.cluster_size;
	send_pk.cluster_header = htons(Node_Attr.cluster_header);
	send_pk.node_qos = Node_Attr.node_qos;
	
	Report_Node_Attr();
	// 4. ��nb_list�ĸ�ֵ���ҵ�����IARP_radius���ڵ��ھӣ���������Ϣ�����б��ڣ�����node_ID��node_qos��mpr_distance��


	/*
	 * WJW:
	 *	�����ڵ��һ���ھ��б���Ϣ����nb_list����ʽ�����ھӣ�
	 *	����Я��������һ���ھӽڵ�ģ�
	 *	1��rcv_QoS, ��ĳ�ڵ��յ��󣬱���Ϊsend_Qos��
	 *	2��mpr\��·�Ƿ�˫��ɴ�\������1��\
	 *
	 * */
	send_pk.nb_num = Pad_LSA_Nb_List(send_pk.nb_list, Node_Attr.IARP_radius, oneHopNb_List, mHopNb_List);
	
	// 5. ���CRC
	len = LSA_PK_LEN(send_pk.nb_num);
	*(pkptr+(len>>1)-1) = getCRC((unsigned char*)&send_pk,len-2);
	
	// 6. ����WiFiSndThread��Route���ƶ���

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


	// 7. ���ö�ʱ����PERIOD_LSA���ٴ�ִ��LSA_PK_Send. 	 LSA_PK_SendHandler ������
	//wdStart(wd_LSA, PERIOD_LSA+(rand()&0x1f) ,TIMER_ONE_SHOT);
	
	// 8. �ϱ���Ϣ
//	Report_Table();

	nsv_NodeLinks_TableReport();
	
	
}

/**  void LSA_PK_Rcvd()  -- ����LSA���ģ����������ر���������������·�仯�����������ھӶ�ʧ���ɵ��򣩣���������������·��ά��
	����
		1. ����LSA���ģ�����һ���ھӱ�\�����ھӱ�\���ں����·�ɱ����º�һ������û��node_ID��ͬ������Ƕ������ڻᾭ������(relay_ID��ͬ)
		2. �ȼ�������ھӶ��Ƿ�Խ�ߣ����ػ��أ�
			2.1 ����ǰ��ƽ��·���£���˫��һ���ھӳ������ޣ��򽨴أ�����Node_Attr�еķִ�״̬
			2.2 ����ǰΪ��ͷ�£���˫��һ���ھӵ������ޣ����أ�����Node_Attr�еķִ�״̬
		3. �ټ����ھӷִ�״̬���Ƿ��Ӱ������
			3.1 ��LSA�����еķִ�״̬Ϊ��ͷ�������ܽ��������أ�
				����degree��������һ���ھ���(�����ID��С������)�������Node_Attr�еķִ�״̬������ô�
				���򣬲�����
			3.2 ��LSA�����еķִ�״̬Ϊƽ�桢��Ա�����أ�������
		
*/
int LSA_PK_Rcvd(LSA_PK *lsa_pkptr, unsigned short lsa_pklen)
{
	unsigned char	degree;				// �ڵ�ȣ���˫��ɴ��һ���ھӸ���
	
	unsigned char	cluster_state;		// �ִ�״̬��0ΪZRPƽ��·�ɣ�1Ϊ��ͨ�س�Ա��2Ϊ���أ�3Ϊ��ͷ��
	unsigned char	cluster_size;		// �ش�С�������г�Ա��Ŀ�����ִ�ʱ��Ч��ZRPʱ��Ϊ0
	unsigned short	cluster_header;		// ��ͷID�����ִ�ʱ��Ч��ZRPʱ��Ϊ0xff
	
	unsigned short	send_id;
	// 1. У����������CRC,У������򷵻�-1
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
	
	// 1. ����LSA���ģ�����һ���ھӱ������ھӱ����ں����·�ɱ�
	Update_Nb_Tab(lsa_pkptr, lsa_pklen);
	
    // 2. ������˫��һ���ھӣ���������״̬�ı�
    if (Is_1hopNb(oneHopNb_List, send_id, NULL )<2)
    {
        return 0;
    }
	degree = lsa_pkptr->degree;
	cluster_state = lsa_pkptr->cluster_state;
	cluster_size = lsa_pkptr->cluster_size;
	cluster_header = ntohs(lsa_pkptr->cluster_header);
	
    
	// 2. �����ھӷִ�״̬���Ƿ��Ӱ������
	if(cluster_state == CLUSTER_HEADER)
	{   
        // 3.1 �������Ǵ�ͷ
        
		if(Node_Attr.cluster_state == CLUSTER_NONE)
		{
			// ������ƽ��·�ɣ���ֱ����ء�����Ǹýڵ��MPR������Ϊ������
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
			// �����Ǵس�Ա������أ���ͷ��ͬʱ���´ش�С�ʹ�״̬
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
			// �����Ǵ�ͷ����ȽϽڵ�ID�����ոôػ򱻸ô�����
			if(Node_Attr.ID > cluster_header)
			{
				// ������
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
        // 3.2 �����߲��Ǵ�ͷ�����÷����߸պ��Ǳ��ڵ�ԭ��ͷ���򱾽ڵ���ָ���ZRP
        if(send_id == Node_Attr.cluster_header)
        {
            Node_Attr.cluster_state = CLUSTER_NONE;
            Node_Attr.cluster_size = 0;
            Node_Attr.cluster_header = 0xffff;	
        }
    }
	
	return 0;
	
}

/** RREQ_PK_Send() -- Դ�ڵ��װRREQ_PK����ͨ�����߶˷���. �޸��Ӵ���ע������RREQӦ��ʱ����*/
void RREQ_PK_Send(unsigned short dest_id ) 
{
	RREQ_PK send_pk;
	
	// 1. ��װ����
	memset((unsigned char*)&send_pk, 0, sizeof(send_pk));
	
	send_pk.pk_type = RREQ_PK_TYPE;
	send_pk.TTL = MAX_HOP -1 ;
	send_pk.send_ID = htons(Node_Attr.ID);

	//�㲥����RREQ
	send_pk.rcv_ID = htons(0xffff);				//�㲥����RREQ

	send_pk.src_ID = htons(Node_Attr.ID);


	//RREQ��Ŀ�Ľڵ�id
	send_pk.dest_ID = htons(dest_id);			//RREQ��Ŀ��id

	send_pk.rreq_seq = htons(SEQ_INC(RREQ_Seq)); //����rreq_seq���к�
	
	send_pk.path_QOS = 16;
	send_pk.hop = 1;
	
	send_pk.trace_list[0] = htons(Node_Attr.ID);
	
	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(send_pk)-2);
	
	// 2. ���±���RREQ�����뱾����¼����ʶ���ھ�ת�����ĸñ���
	Update_RREQ_Tab(&send_pk,sizeof(send_pk));
	
	// 2. LOG ��ӡ
	LOG_MSG(INFO_LEVEL,"send RREQ(%d -> %d, seq=%d)\n",Node_Attr.ID,dest_id,ntohs(send_pk.rreq_seq));
	
	// 3. ����WiFiSndThread��Route���ƶ���
	msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)&send_pk, sizeof(send_pk),WLAN_ROUTE_CTRL, NO_WAIT);
	
	// 4. ���ö�ʱ���� IERP_RREQ_TIMEOUT ���ٴ�ִ�� RREQ_Timeout����Route_Serv����ʱ��Ϣ�����ڼ��յ�RREP����cancel����û�У�����ʾѰ·ʧ�ܣ�ɾ�����汨��
	wdStart(wd_RREQ, IERP_RREQ_TIMEOUT,TIMER_ONE_SHOT);	
	
}

/** RREQ_PK_Rcvd() -- �յ�RREQ_PK��Ĵ���
	
	����
		1. �ڵ��յ��󣬽�����src_ID,dest_ID;
		2. ����Update_RREQ_Tab()����RREQ��
		3. �������˸��£���Ŀ�Ľڵ㷴��RREP���м�ڵ�ת��RREQ
		4. ��û�������£��򲻴���
		
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
    
	
	// 1. У����������CRC,У������򷵻�-1
	if (getCRC((unsigned char*)rreq_pkptr, sizeof(RREQ_PK)) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"RREQ_PK crc err\n");
		return -1;
	}
	
	// 1. �ڵ��յ��󣬽�����src_ID,dest_ID;
	src_ID = ntohs(rreq_pkptr->src_ID);
	dest_ID = ntohs(rreq_pkptr->dest_ID);

	send_ID = ntohs(rreq_pkptr->send_ID);
    rreq_seq = ntohs(rreq_pkptr->rreq_seq);
   
    
	// LOG ��ӡ
	LOG_MSG(INFO_LEVEL, "rcvd RREQ(%d -> %d, seq=%d) from %d\n",src_ID, dest_ID, rreq_seq, send_ID);
	

    if(dest_ID == Node_Attr.ID)
    {
        // 2. ���Լ�ΪĿ�Ľڵ㣬����RREP���ġ�
		 /* WJW:
		 * �յ�RREQ������Ŀ�Ľڵ�id�Ǳ��ڵ㣬��ʱͨ������Is_1hopNb()���ж�
		 * ���ڵ��Ƿ���send_ID��˫���ͨ��
		 * 1�����ǣ������RREQ��¼��Ʃ��Ϊ�µ�RREQ�����½�һ���¼��
		 * 	  Ȼ����RREP;���ģ���Լ�����RREQ�����ڵ��� srcId�����յ�RREP�Ĵ�����IERP�������·���
		 * 2��
		 * ����TTL���м䷢���ߣ�Send_id��E��id, �������Լ��ѱ��ڵ�id׷����trace_list[]ĩβ��
		 * ���ת����ȥ��
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
                
                // ɾ��������src_IDΪdest_ID��IERP���׼���������·��
                Del_IERP_Item_By_DestID(IERP_List, src_ID);
                
                // ����������·�ɱ��ģ���Լ�����RREQ���յ�RREP�Ĵ���
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
        // 3. ����ת��RREQ���ģ���MPR�ڵ�ת��,��TTL>0����
        
        if(rreq_pkptr->TTL >0 && Is_yourMPR(oneHopNb_List, send_ID, &path_qos) == 1 )
        {

        	/*
        	 * WJW:
        	 * �յ�RREQ����Ŀ�Ľڵ�id���Ǳ��ڵ㣬��ʱͨ������Is_yourMPR()���ж�
        	 * ���ڵ��Ƿ���send_ID�� MPR,���ǣ�����´�RREQ��Ϣ:
        	 * ����TTL���м䷢���ߵ�id, �������Լ��ѱ��ڵ�id׷����trace_list[]ĩβ��
        	 * ���ת����ȥ��
        	 * */
            update_res = Update_RREQ_Tab(rreq_pkptr, rreq_pklen);
            if(update_res == 1)	
            {           
                rreq_pkptr->TTL--;
                rreq_pkptr->send_ID = htons(Node_Attr.ID);	
                // path_QOS ����Ϊ min{���ص�send_ID��QOS�� �����ڵ�path_QOS}
                rreq_pkptr->path_QOS = (rreq_pkptr->path_QOS > path_qos)?path_qos:rreq_pkptr->path_QOS;	
                
                rreq_pkptr->trace_list[rreq_pkptr->hop] = htons(Node_Attr.ID);
                rreq_pkptr->hop++;
                                
                rreq_pkptr->pk_crc = getCRC((unsigned char*)rreq_pkptr,  rreq_pklen-2);
                
                // ����WiFiSndThread��Route���ƶ���
                msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)rreq_pkptr, rreq_pklen, WLAN_ROUTE_CTRL,NO_WAIT);
                
                // LOG ��ӡ
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
    
	// 4. ��û�������£��򲻴���
	return 0;
}


/** RREP_PK_Send() -- Ŀ�Ľڵ���RREQ��Դ�ڵ㷢��RREP���ģ����򵥲����͡���RREQ_Record�����ҵ��ü�¼����װRREP���͡�
	���룺src_idΪRREQ��Դ�ڵ㣬����ͨ����Դ��ַ���岻ͬ����Ϊ�˼򻯲��������ڲ���RREQ��
*/
void RREP_PK_Send(unsigned short rreq_src_id, unsigned short rreq_dest_id)
{
	RREP_PK send_pk;
	
	RREQ_RECORD		*record_ptr;
	int				record_pos;
	int				i;	

	// 1. ��RREQ_RECORD������ȡ��¼����ڸ�RREP���ĸ�ֵ	
	if (-1== (record_pos = Search_RREQ_Record(RREQ_Record_List, rreq_src_id, rreq_dest_id)))
		return;
		
	record_ptr = (RREQ_RECORD*)prg_list_access(RREQ_Record_List, record_pos);
	
	// 2. ��װ����
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
	
	// 3. ����WiFiSndThread��Route���ƶ���
	msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)&send_pk, sizeof(send_pk),WLAN_ROUTE_CTRL, NO_WAIT);
	
	// 4. LOG ��ӡ
	LOG_MSG(INFO_LEVEL,"send RREP(%d -> %d)to %d\n",rreq_dest_id,rreq_src_id, ntohs(send_pk.rcv_ID));
}

/** RREP_PK_Rcvd() -- �յ�RREP_PK��Ĵ���
	
	����
		1. rcv_ID�յ��󣬸���trace_list�����µ�rcv_ID��send_ID(����ID)��ת�����м�ڵ㲻ά������·����
		2. ���Լ�ΪԴ�ڵ㣬����ת����������IERP��������Ҫ����״̬���������߶˵ķ��ͻ���������ȡ���ķ���
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
	
	// 1. У����������CRC,У������򷵻�-1
	if (getCRC((unsigned char*)rrep_pkptr, sizeof(RREP_PK)) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"RREP_PK crc err\n");
		return -1;
	}
	
	// 2. ����rcv_ID��src_ID��dest_ID��
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
	
	if(rcv_ID == Node_Attr.ID)		//  ����RREP�ǵ������Լ��ǽ�����ʱ�Ž���
	{
		
		LOG_MSG(INFO_LEVEL, "rcvd RREP(%d -> %d) from %d\n", rreq_dest_ID, rreq_src_ID,send_ID);	
	
		if(rreq_src_ID == Node_Attr.ID)
		{
//			if(rreq_dest_ID == 1)
//			{
//				TokenRing_OK_ACK_Send(1 , rreq_src_ID );
//			}

			// 3.1 ����Լ���RREQ��ԴID������IERP,��APP_BUFF��Ŀ��ID��dest_ID�ı�����ȡ����������WiFiSndThread��App����
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
			
			// 3.2 ȡ����ʱ������������
			Is_Routing = 0;
			wdStart(wd_RREQ, 0,TIMER_ONE_SHOT);	
			
			// ��ӡ
			Print_All();
		}
		else
		{
			// 4. ����Լ�����ԴID�������trace_list�����µ�rcv_ID��send_ID(����ID)��ת��
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


/** RERR_PK_Send() -- ������·���ѵ����νڵ���Դ�ڵ��װ������RERR����
						ֻ����ת�����·�ɵ�ҵ����ʱ���Żᷢ����·���ѣ�ƽʱ����HELLO��ά��IERP���м�ڵ�Ҳ����¼IERP��������ȫ��Ѱ·�󣬾ͱ����ȫ����ʽ·���ˣ�
	����
		1. ���ֶ��Ѻ��ȳ��Ա��ؼ��޸�������������·�ɱ�
			����·��������Ϊ���޸������ݴ˷�װRERR��
			����·��������Ϊ�����޸������ݴ˷�װRERR��
		2. �����Ƿ��޸�����ֱ���ύ���߶˷��͡�
		
		�����޸������⣺�����ֶ��ѵ��м�ڵ�պ�ΪĿ�Ľڵ��һ���������ھӣ����԰��޸��õ�����·������Դ�ˣ�
		������3���������ھӣ������޸��޷��ҵ�����·����ֻ֪����һ����
		��ˣ����ֽ��������
			1) ֻ�������ڵı����޸�
			2) ���������ڵı����޸�������APP����ת��ʱ����Ϊ���·�ɣ���trace_list����һ��δ֪ʱ��Ҫ��������·������ת��
			3) ���������޸�
		�Ȱ�����3��
*/
void RERR_PK_Send(unsigned short rcv_id, unsigned short dest_id, unsigned short err_down_ID, APP_PK *app_pkptr)
{
	RERR_PK			send_pk;
	
	int				routing_state = -1;
	unsigned char	hop = 3;
	//unsigned short	trace_list[MAX_HOP];
	
	// ���շ���3�����������޸�����˴�����鱾��·�ɱ�
	// routing_state = Route_Search(dest_id, &rcv_ID, &hop, trace_list);
	
	memcpy((unsigned char*)&send_pk, (unsigned char*)app_pkptr, sizeof(send_pk));	// ���rreq_src_ID��rreq_seq��hop��trace_list[]�ֶεĸ�ֵ	

	if(routing_state == 0 && hop <=2)
	{
		// 1. �����޸��ɹ�����װRERR��������src_id
		send_pk.err_state = 1;
	}
	else
	{
		// 2. �����޸�ʧ�ܣ���װ����Դ�˷��͡���װʱ����ҵ����·��ͷ����Ϣ��ʽ
		
		send_pk.err_state = 0;
	}
	
	send_pk.err_state = 0;	// �����޸���ǿ��Ϊ0
	send_pk.pk_type = RERR_PK_TYPE;
	
	send_pk.send_ID = htons(Node_Attr.ID);
	send_pk.rcv_ID = htons(rcv_id);
	send_pk.src_ID = htons(Node_Attr.ID);
	send_pk.dest_ID = htons(dest_id);
	
	send_pk.err_up_ID = htons(Node_Attr.ID);
	send_pk.err_down_ID = htons(err_down_ID);
	send_pk.err_repair_ID = 0xffff;

	send_pk.pk_crc = getCRC((unsigned char*)&send_pk,  sizeof(send_pk)-2);

	// 3. ����WiFiSndThread��Route���ƶ���
	msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)&send_pk, sizeof(send_pk),WLAN_ROUTE_CTRL, NO_WAIT);
	
	// 4. LOG ��ӡ
	LOG_MSG(INFO_LEVEL,"send RERR(%d -> %d, fail_ID=%d) to %d\n",Node_Attr.ID,dest_id, err_down_ID, rcv_id);
}

/** RERR_PK_Rcvd() -- �յ�RERR_PK��Ĵ���

	����
		1. ��rcv_IDΪ�м�ڵ㣬���Ȳ���err_state
			�����޸�����ת��RERR��
			��δ�޸��������޸���������������·�ɱ�
				����·��������Ϊ���޸������½��յ���RERR���ĵ�trace_list���ֶ�,���ݴ˷�װת��RERR��
				����·��������Ϊ�����޸������ݴ˷�װRERR��
					
		2. ��rcv_IDΪԴ�ڵ㣬�Ȳ���err_state
			�����޸��������IERP�� // �����ͣ�������һ�����Ա��ģ�����������·����IERP��ͬʱ�𵽶˵�����֤�����á�
			��δ�޸�����ɾ��IERP���ٰ��������ں������ԭ�����½���·�ɡ�
		
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
	
	// 1. У����������CRC,У������򷵻�-1
	if (getCRC((unsigned char*)rerr_pkptr, sizeof(RERR_PK)) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"RERR_PK crc err\n");
		return -1;
	}
	
	// 1. ��������
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
	
	if(rcv_id == Node_Attr.ID)		//  ����RERR�ǵ������Լ��ǽ�����ʱ�Ž���
	{	
		// LOG ��ӡ
		LOG_MSG(INFO_LEVEL, "rcvd RERR(%d -> %d, fail_ID=%d) from %d\n",err_up_id, dest_id, err_down_id,send_id);
	
		if(rcv_id == dest_id)
		{
			// 2. ����Լ�����Դ�ڵ㣬�����rreq_src_id��rreq_seq��ɾ����������·��			
			if(rerr_pkptr->err_state == 0)
			{
				// δ�޸�����ɾ��
				Del_IERP_Item_By_Seq(IERP_List, rreq_src_id, rreq_seq);
			}
			else
			{
				// ���޸�����������·�ɡ��ݲ�����	
				
			}
			
		}
		else
		{
			// 3. ����Դ�ڵ㣬�����trace_list�ҵ���һ����ת�������߶�	
			Del_IERP_Item_By_Seq(IERP_List, rreq_src_id, rreq_seq);	// ����DSR����ʱ�Ż�������
			
			rcv_id = Get_Prev_ID(trace_list, Node_Attr.ID);		//�����ϣ��˴���Ȼ�ܹ�����rcv_ID�����򣬾���Դ�˷�װ·��ͷʱ������
			
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

/** APP_PK_Send() -- Դ�˷��ͱ��ģ������Ĵ����߶˴�������Ҫͨ�����߶˷���ʱ�����ô˺���
	
	����
		1. ˳�����IARP��IERP·�ɱ�
			1.1 ��IARP�ڴ��ڣ���ѡ��cost��С��һ��·������д��һ��������trace_list����Ч����
			1.2 ������IERP�ڴ��ڣ�����дtrace_list��������ͷ��
			1.3 ���������ڣ��������Ѱ·���̣�ϵͳ״̬��ΪѰ·״̬����״̬�²����ٴ�Ѱ·�뷢��ҵ����
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

	// 1. ����IP��ͷ��ʽ��������Դ��Ŀ��IP��ַ �� Э������ (ICMP, UDP, TCP)
	src_ID = getSrcAddr_fromIPHeader(IP_pkptr);
	dest_ID = getDestAddr_fromIPHeader(IP_pkptr);
	protocol = getProto_fromIPHeader(IP_pkptr);
	
	// 2. ����Ŀ�ĵ�ַ����·�ɱ�����·�ɣ����װAPP���ģ�����WiFi�����ޣ���IP_pkptr���浽APP_BUFF,������Ѱ·
	if(dest_ID == Node_Attr.ID)
		return;

	/*
	 * WJW��
	 * ����Route_Search������·�ɱ�
	 * 1���Ȳ���һ��·�ɱ�oneHopNb_List�����dest_Id�ڱ����У��ұ���Ϊ˫���ͨ��direction=1��,
	 *	  �򷵻�qos���Ľڵ㣬��Ϊ��һ�������ң�
	 *	  rcv_IdΪһ���ھ��б��нڵ�Ԫ��id;
	 *	  rreq_src_id = 0;
	 *	  hop =1;
	 *	  trace_list[]��Ч
	 *	  qosΪ���ֵ
	 *	  ����ֵΪ0,��ʾ�ɹ�
	 * 2��һ���ھ��б����ޣ������m��·�ɱ�mHopNb_List�����dest_Id�ڱ����У����أ�
	 *	  �򷵻�qos���Ľڵ㲢�ң�
	 *	  rcv_IdΪm���ھ��б��нڵ�Ԫ��Ϊdest_id��Ӧ��Relay_ID;
	 *	  rreq_src_id = 0;
	 *	  hop =element_ptr2->hop;;
	 *	  trace_list[]��Ч
	 *	  qosΪ���ֵ
	 *	  ����ֵΪ0,��ʾ�ɹ�
	 *
	 * 3����һ���ھ��б�m��·�ɱ�������ڣ���������·�ɱ�
	 *	  rcv_IdΪ���·�ɱ��нڵ�Ԫ��Ϊdest_id��Ӧ��next_id;
	 *	  rreq_src_id = element_ptr3->rreq_src_ID;
	 *	  hop = element_ptr3->hop;
	 *	  trace_list[]��Ч
	 *	  qosΪ���ֵ
	 *	  ����ֵΪrreq_seq,��ʾ�ɹ�
	 *
	 *	���򷵻�-1,��ʾ��·��

	 * */
	routing_state = Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);
	
	if(routing_state >= 0)		// �����ڻ����·��
	{
		// 3. ��IP_pkptr�����ϣ���װ·�ɱ�ͷ���������ֽ����װ
		APP_pkptr->pk_type = DATA_PK_TYPE;
		APP_pkptr->reserved = 0;
		APP_pkptr->send_ID = htons(src_ID);
		APP_pkptr->rcv_ID = htons(rcv_ID);		//��ʾ����Ŀ�Ľڵ���м�ڵ��Ŀ��ID����
		APP_pkptr->src_ID = htons(src_ID);
		APP_pkptr->dest_ID = htons(dest_ID);
		APP_pkptr->rreq_src_ID = htons(rreq_src_id);
		/*
		 * WJW��
		 * rreq_seqΪ0ʱ����ʾ������·��
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
			// ע�� ���㲥ʱ��Ҫ���±��صĹ㲥�ţ�������㲥�ű�֮�����ܵ��ھ�ת���ĸñ��ģ�����ת��
			Bcast_ForwardSeq_Record[Node_Attr.ID] = SEQ_INC(Bcast_Seq);
			APP_pkptr->bc_seq = htons(Bcast_ForwardSeq_Record[Node_Attr.ID]);
		}
		else
		{
			APP_pkptr->bc_seq = 0;
		}	
		APP_pkptr->len = htons(ip_len);
		APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
		
		// 4. ����WiFiSndThread��app����
		msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len,WLAN_ROUTE_DATA, NO_WAIT);
		
		// 5. LOG ��ӡ ��ͳ��
		//LOG_MSG(DBG_LEVEL,"send APP %d->%d (proto=%d, size=%d, src port=%d, dst port=%d) to %d\n",src_ID, dest_ID,protocol,ip_len, getSrcPort_fromIPHeader(IP_pkptr), getDstPort_fromIPHeader(IP_pkptr),rcv_ID);
        LOG_MSG(DBG_LEVEL,"routing.c-> msgQ_snd, send APP %d->%d (proto=%d, size=%d, rreq_seq=%d) to %d\n",src_ID, dest_ID,protocol,ip_len, routing_state ,rcv_ID);
		Node_Attr.send_pk_num++;
		Node_Attr.send_pk_size += ip_len;
	}
	else						// ��·��
	{
		// 3. ��IP_pkptr���浽APP_BUFF��ע��BUFF_ITEM�ķ�װ
		Add_APP_PK(APP_Buff_List, dest_ID, (unsigned char*)APP_pkptr, app_len);
		
		LOG_MSG(DBG_LEVEL,"add APP(to %d) to Buffer\n",dest_ID);
		
		// 4. ����ǰ������Ѱ·״̬������Ѱ·��ͬһʱ��ֻ����ĳһ��Ŀ�Ľڵ�Ѱ·����ʱ���Ѱ��һ��Ŀ�Ľڵ㡣
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

/** APP_PK_Rcvd() -- �ڵ�ͨ�����߶��յ�APP_PK��Ĵ���

	����
		1. �鿴APPͷ��
			1.1 ��Ŀ�Ľڵ��ǹ㲥�������Լ�Ϊ���ͽڵ��MPR��TTL>0��bc_seq���£�������˱ܺ�ת��������ת�����ϴ������߶�PC��
			1.2 �����սڵ�������
					��rreq_seq==0����ʾ����·�ɣ�ֱ�Ӳ��ұ���IARP·�ɱ�ת������û����Ŀ��ֱ�Ӷ������ģ���ʱ��Դ�ڵ�ᷢ�֣�
					��rreq_seq != 0����ʾ���·�ɣ�������ͷ�ڵ�trace_list��ȷ����һ����
						����һ��˫��ɴ���ط�װ��ת����
						����ֱ�Ӷ������ģ�����Դ�˷���REER���ȳ��Ա����޸���
				�ϴ������߶�PC��
			1.3 ���򣬲�����
	���أ�
	    -1�� ��ͷCRCУ�����
	    0������ID���ǹ㲥����������
	    1����ȷ����
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
	unsigned char	TTL;			// �㲥����ʱ�䣬���ƹ㲥��Χ��Դ����Ϊ6��ÿת��һ�εݼ�1����0ʱ���ٹ㲥ת��������6��			
	unsigned char	hop;
	unsigned short	trace_list[MAX_HOP];
	
	int	routing_state;
	int	i;
	

	IP_pkptr = APP_pkptr->data;
	ip_len = app_len - ROUTE_HEAD_LEN;
	
	// 1. У��Route��ͷCRC,У������򷵻�-1
	if (getCRC(app_pkptr, ROUTE_HEAD_LEN) != 0)
	{
#ifdef NOT_USED_OK_ACK
		Set_TR_State(TR_ST_INIT);
#endif
		LOG_MSG(INFO_LEVEL,"APP_PK crc err\n");
		return -1;
	}
	// 2. ����Route��ͷ��ʽ��������Դ��Ŀ��IP��ַ �� TTL��bc_seq����Ϣ; ע�⣬��������Ϊ�����ֽ���
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
	
	if(rcv_ID == 0xffff)	// ���սڵ��ǹ㲥
	{
        // ���֮ǰδ�յ����ñ��ģ����ϱ�PC������¼Bcast_ReportSeq_Record
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
        
        // ���MPR��TTL��bc_seq���������������޸ı�ͷ��ת�������߶�
        // ֻ�й㲥�Ŵ���MPR����
        if(TTL > 0 && Is_yourMPR(oneHopNb_List, send_ID, NULL) == 1 && Seq_Cmp(bc_seq,Bcast_ForwardSeq_Record[src_ID])>0)
        {
            // ������send_ID��MPR����TTL>0����Ӧת���ù㲥�����޸��շ���ַ��TTL�����¼���CRC.������ Bcast_ForwardSeq_Record
            APP_pkptr->send_ID = htons(Node_Attr.ID);
            APP_pkptr->rcv_ID = htons(0xffff);					
            APP_pkptr->TTL--;

            APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
            
            Bcast_ForwardSeq_Record[src_ID] = bc_seq;
            
            //  ����˱ܺ󷢸�WiFiSndThread��App����
            LOG_MSG(DBG_LEVEL,"forward APP to %d\n",rcv_ID);
            msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len, WLAN_ROUTE_DATA, NO_WAIT);
        }
        
        // ��������£��Ȳ��ϱ�PC��Ҳ��ת����ֱ�Ӷ���
		
	}	

	/*
	 * ����Ϊ��Ե㴫�䣬�����м�����£�ͨ��trace_list[...]ָ����δ�������һ���ڵ�
	 * */
	else if(rcv_ID == Node_Attr.ID)
	{
		if(dest_ID == rcv_ID)
		{
			// 4.1 ���սڵ�������Ϊ����Ŀ�Ľڵ㣬����ת����ֱ�ӵݽ�PC���ɣ�������жȥ·��ͷ������LanSndThread,

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

//Ĭ�ϲ�֧��ping
//				LOG_MSG(DBG_LEVEL,"Give APP to PC\n");
//				msgQ_snd(msgQ_Lan_Snd, (unsigned char*)IP_pkptr, ip_len,LAN_DATA, NO_WAIT);

		}
		else if(rreq_seq == 0)
		{
			// 4.2 ���սڵ���������Ϊ����·�ɣ����IARP����һ��
			routing_state = Route_Search(dest_ID, &rcv_ID, &rreq_src_id, &hop, trace_list);
			
			if(routing_state == 0)	
			{
				// 4.2.1 ������·��, �޸��շ���ַ�����¼���CRC
				APP_pkptr->send_ID = htons(Node_Attr.ID);
				APP_pkptr->rcv_ID = htons(rcv_ID);
				APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
				
				// 4.2.2 ����˱ܺ󷢸�WiFiSndThread��App����
				LOG_MSG(DBG_LEVEL,"forward APP to %d\n",rcv_ID);
				msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len,WLAN_ROUTE_DATA, NO_WAIT);
					
			}
			else		
			{
				// 4.2.3 ֻ�����·�ɻ���·�ɣ�ֱ�Ӷ�������
                LOG_MSG(DBG_LEVEL,"no IARP route to forward\n");
				
			}
		}
		else	
		{
			// 4.3 ���սڵ���������Ϊ���·�ɣ���鱨ͷ�ڵ�trace_list����һ��	
			rcv_ID = Get_Next_ID(trace_list, Node_Attr.ID);		//�����ϣ��˴���Ȼ�ܹ�����rcv_ID�����򣬾���Դ�˷�װ·��ͷʱ������
			
			// 4.3.1 ��һ���ھӱ��жϵ�rcv_ID�Ƿ�ɴ�ɴ����װ���ͣ����ɴ������rerr_pklen
			i = Is_1hopNb(oneHopNb_List, rcv_ID, NULL);
			if( i >1 || (i == 1 && Node_Attr.IARP_radius == 0))
			{
				//  ��˫���ھ�, ��radius = 0 �������Ϊ�����ھӣ����޸��շ���ַ�����¼���CRC
				APP_pkptr->send_ID = htons(Node_Attr.ID);
				APP_pkptr->rcv_ID = htons(rcv_ID);
				APP_pkptr->head_crc = getCRC((unsigned char*)APP_pkptr, ROUTE_HEAD_LEN-2);
				
				//  ����˱ܺ󷢸�WiFiSndThread��App����
				LOG_MSG(DBG_LEVEL,"forward APP to %d\n",rcv_ID);
				msgQ_snd(msgQ_Wifi_Snd, (unsigned char*)APP_pkptr, app_len, WLAN_ROUTE_DATA,NO_WAIT);
			}
			else
			{
				// ֱ�Ӷ������ģ�����Դ�˷���RERR���ȳ��Ա����޸���
				RERR_PK_Send(send_ID, src_ID, rcv_ID, APP_pkptr);	
			}
			
		}
		
	}
	else
	{
		//4.5 ����ID�Ȳ��ǹ㲥���ֲ�������ֱ�Ӷ�����
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
//	msg[2] = (msglen>>8)&0xff;			// ����ǰ���޸�
//	msg[3] = (msglen>>0)&0xff;
//
//	msg[4] = (self_id>>8)&0xff;
//	msg[5] = (self_id>>0)&0xff;
//	msg[6] = (node_num>>8)&0xff;			// ����ǰ���޸�
//	msg[7] = (node_num>>0)&0xff;
//	msg[8] = 0x00;
//	msg[9] = 0x00;
//
//	msglen = 10;
//
//	while(NULL != (element_ptr1 = (ONE_HOP_NB_ITEM*)prg_list_access(oneHopNb_List, i)))
//	{
//
//		if(element_ptr1->direction == 1)	// ֻ��˫���ھ�
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
//	msg[2] = (msglen>>8)&0xff;			// ����ǰ���޸�
//	msg[3] = (msglen>>0)&0xff;
//
//	msg[6] = (node_num>>8)&0xff;			// ����ǰ���޸�
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
