/*
 * Congitive.c
 *
 *  Created on: May 23, 2018
 *      Author: root
 */


/*
 * �����ɣ�
 *���ٵĵ������վ���ѡһ��Ƶ�ʣ����ߴ�վ��Ȼ�����һ�¶����ʣ��������β��ԣ��õ�����Ƶ��
 * */

/*
 * ���Σ�
 * ���ICR��̨�������ŵ��ϣ���վ�������ڸ���վһ���ɴ�;��ҵ��/��֪�ŵ��ϣ�֧������������Ad Hoc���磩ģʽ��
 * ���ʱ��Ӧ����Ϊ����ʱ������������Ad Hocͨ�ŵ�Ƶ�ʡ��������ʵ��ľ�����֪��Ϊ����Ҫͨ��Ƶ�׸�֪
 * �����β����������ȵ����̣����ܾ������ɹ���ͨ��Ƶ�ʼ��Ͳ��β���;Ȼ���������ڹ㲥�·�����
 * Ad Hoc����ҵ��ͨ�ŵĲ��β���������Ƶ�ʺ����ʡ�
 *
 * */

/*
 *	����ģ�
 *	��Թ�������Ŀ����ˮ��1��ͧ��3Сͧ���ɵ�ˮ�泬�̲��ƶ�����֯Ad Hoc���硣
 *	�����ڵ��ϴ���3�����̲���̨��1������ҵ��ͨ�ŵ�5W���ٳ��̲��豸��֧��19.2kbps��1����������ͨ�ŵĳ��̲��豸��
 *	֧�ֵ��١�Զ����ͨ�š�1�����ڱ���Ƶ�׵�Ż�������С����13dBm���̲�ģ�飬֧��RSSI�����
 *	Ϊʵ��Ƶ�׸�֪--->���β�������-->AdHoc����ͨ��ϵͳ���̣�ϵͳ������£�
 *	1> �豸����ʱ���ɴ�ͧ�ڵ�ͨ������̲���̨�㲥���͡����н�����󡱣�Locl_Cog_Start������,
 *	   ͬʱ��ͧ�����ڵ�Ľ���������汾�����飬��1ά�����ʾ��ÿ��Ԫ����1���ṹ���ʾ��
 *	   struct{
 *	    u8 node_id;
 *	    u8 ch_id;
 *	    u8 avg_rssi;
 *	    u8 min_rssi;
 *	    u8 max_rssi;
 *	    u8 rssi[MAX_FREQS_CHNL_NUM];	//ÿ���ŵ���RSSIֵ
 *	   };
 *
 *	   ͬʱ��¼�����֪�Ŀ�ʼʱ�� cog_start_t;
 *
 *	   ��ӡ������ڵ��Ƶ�׼������
 *
 *	   ���������֪��ʱ������ʱ����������Ƶ�׾����㷨�����������յ��ĸ��ڵ㱾�н��
 *	   �����о�����ѡ��RSSI��С��Ƶ�ʣ���ͨ������͸�����վ��
 *
 *	2> �ӽڵ��յ�Ƶ�׸�֪����Locl_Cog_Start����������ڵ�ĸ�֪���ͨ����������ڵ㡣
 *	   ��������ʱ����������վ�·��Ĳ���;�յ���վ�·�������ȡ����ʱ�������򡣡�����δ���
 *
 *	3> ���ڵ��յ��ӽڵ�ı����ϱ������������洢���ӽڵ�idΪ�±��������
 *	   ÿ�μ�¼���н���ʱ�䣬�������֪��ʱ����ʱ�������������к�ʱ��
 *
 *	4> ��1���������������֪��ʱ����ʱ�¼�����������Ƶ�׾����㷨��
 *	   Ƶ�׾����㷨1: ����ö���ͼ-�����ŵ�˼�루�ο�����ͼ-�׶�԰���⣩����������Ŀ���Ƶ�ʼ���
 *	   Ƶ�׾����㷨2: ����������нڵ�ÿ���ŵ��������ֵ��ƽ��ֵ��Ȼ���������ŵ��������ֵ����Сֵ�����Ӧ��Ƶ���Ƽ�Ϊ�����Ŀ���Ƶ�ʡ�
 * */
/*
 * @fuction: Cog_local_start
 * ���п�ʼ,����վ���𱾸п�ʼ�������վ�յ�������ϱ����ر��н����
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
 * ���з�������ڵ㴴����ʱ�����ȴ��ӽڵ��ϱ��������ʱ��ʼ�����֪�����㷨
 * */
#define 		WD_NET_CONG_MSG_SIG 		64

u8 avg_rssi [CH_MAX_NUM ];		//��¼ÿ���ŵ�N��ɨ��rssiƽ��ֵ
u8 max_rssi [CH_MAX_NUM ];		//��¼ÿ���ŵ�N��ɨ��rssiֵ�����ֵ
u8 min_rssi [CH_MAX_NUM ];		//��¼ÿ���ŵ�N��ɨ��rssiֵ����Сֵ


static 	int send_msgq_id;

extern unsigned short 	self_node_id;
/*
 * ����Ƶ����֪����Ľڵ�id
 * */
u8 master_cong_id =-1;
void Cong_Local_Start();
void CongNet_Timer_Start();
static void Local_Cong_Upload();
//ȫ�ֵ������ڱ��н��
Net_Congitive_Result_t  g_net_cong_result_list[MAX_NET_NODES];


/*
 *@function:
 *	Net_Cong_Desicion_Handle
 *
 *@desc:
 *	�����������֪���������е�Ƶ�׾����㷨
 *
 *@return :
 *	ch_i, ����RSSI��С���ŵ�id
 *
 * */
u8 NetCong_Desicion_Handle()
{
	/*
	 * �����ڸ�֪���g_net_cong_result_list[MAX_NET_NODES]�������̣�
	 * 1���Ȼ�ȡ�з���avg_rssi[i]�����ֵavg_max[i]
	 *
	 * 2��Ȼ��Ƚ�avg_max[0....Max-1]��ȡ��С���±�Ϊ���ȵ��ŵ�id��
	 *
	 * 3�����ڵ��֪�����ά���ʾ���£�
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
			//���λ�ȡÿ���ڵ�������ŵ���֪���
			if(g_net_cong_result_list[n].node_sta == CONG_OK)
			{
//				LOG_MSG(INFO_LEVEL,"---------------ch=%d-----node%d g_net_cong_result_list[] stata OK...\n",k,n);

				//�����n���ڵ������֪�����Ч,ȡ���ϱ��ĵ�ch���ŵ���AVG_RSSIֵ�����ch���ŵ��Ĺ������ֵ�Ƚ�
				tmp_rssi_max  =g_net_cong_result_list[n].cong_result.avg_rssi[k];

				if(tmp_rssi_max >  avgRssi_Common_max_list[k])
				{
					avgRssi_Common_max_list[k] = tmp_rssi_max;
				}

			}//if

			//�����n���ڵ���޸�֪������򽫱������н��������ʱ�������Ǹýڵ��Ƶ�׻���


		}//for
	}//for

	//����������Сֵ
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
 * 			��������ڵ��ϱ���Ƶ�׸�֪���-�����ŵ���RSSI״̬��������ȫ�ֱ�����
 *
 * @params: sub_node_cong_pkt
 * 			�����ڴӽڵ��ϱ��ı��н��
 * @return
 * 			��
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

	send_id = sub_node_cong_pkt[1];	//���ͽڵ�id

	g_net_cong_result_list[send_id].node_sta = CONG_OK;

	//����÷��ͽڵ�ı����ϱ����
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
 * 			���������֪�����ڵ㣬�ȴ��ӽڵ��ϱ������ʱ��-��ʱ������
 * @return
 * 			void
 * */
void NetCong_TimeOut_CallbackHandler(int sig, siginfo_t *si, void *uc)
{
	u8 rssi_mini_ch=0;
	int ret ;

	/*
	 * ͨ�������֪��ѡ����RSSI��С���ŵ�������Ƶ������·��������ڵ�
	 * */

	CONG_CMD_PK	cong_pk;

	int pk_len = sizeof(CONG_CMD_PK);

	LOG_MSG(INFO_LEVEL,"--------------------------------------Net_Congitive_TimeOut\n");

	/*
	 * Ƶ�׾��߶�ʱ����ʱ��
	 * STEP1������Ƶ�ξ����㷨������RSSI��С���ŵ�id
	 * */
	rssi_mini_ch = NetCong_Desicion_Handle();


	/*
	 * STEP2:�·�Ƶ�ײ���������CONG_FREQ_PK_TYPE
	 * */
	memset(&cong_pk, 0, sizeof(CONG_CMD_PK));

	cong_pk.pk_type = CONG_FREQ_RESULT_PK_TYPE;
	cong_pk.send_id = master_cong_id;

	//��һ���ֽ�Я���ŵ�id
	cong_pk.cong_cmd[0] =rssi_mini_ch;
	cong_pk.cong_cmd[1] =rssi_mini_ch;
	cong_pk.cong_cmd[2] =rssi_mini_ch;
	cong_pk.cong_cmd[3] =rssi_mini_ch;
	cong_pk.pk_crc = getCRC((u8*)&cong_pk,pk_len-2);


//	global_freq_config_state = FREQ_CONFIG_INIT;

	global_running_state = FREQ_CONFIG_INIT;

	cong_pk.pk_crc = getCRC((u8*)&cong_pk, sizeof(CONG_CMD_PK)-2);


	/*
	 * 	��ʱͨ��ҵ������ģ��������֪����·������ڵ�
	 * */
#if 1
	/*
	 * ��ʽһ�������ݷ���������ģ�鷢����Ϣ�����У�������ģ�鰴�ա��Ƚ��ȳ���ԭ�����߷��ͳ�ȥ
	 * */
	int timeout_cnt =0;
	NetCong_Wireless_MsgQSnd(send_msgq_id, (u8*)&cong_pk, sizeof(CONG_CMD_PK), WLAN_ROUTE_CTRL,IPC_NOWAIT);

	//�ȴ����µ�Ƶ�ʺ����ʲ��������ͳ�ȥ������Ȼ������ִ��
	//��uartWirelessSend.c timer_Pkt_Wireless_SendHandler(...)����
	while(	global_running_state==FREQ_CONFIG_INIT && timeout_cnt++ < 0x1f) {usleep(300000);}
#else

	/*
	 * ��ʽ����ֱ��ͨ������ģ���·������ڵ�
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

	//���н������ָ�����ģʽ
	global_running_state = NET_NORMAL_RUNNING;
//	global_freq_config_state= FREQ_CONFIG_INIT;
	wdStart(wd_LSA, PERIOD_LSA,TIMER_ONE_SHOT);
}



/*
 * ��ȡ����ģ�鷢����Ϣ����
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
 * ������Ϣ���У���Ҫ����˵����
 * 1>��Ϣ������һ���ں�����ʵ��ͬ�����ƵĽ��̼�/���̼߳�ͨ�ŷ�ʽ���������Ϣ����ʹ��ʱ����Ҫ
 * ��Ӷ���ķ�ʽ�Ϳ���ʵ�ֽ��̼��ͬ���Լ���Դ����
 *   ������һ�����Ҫ������Ŀ��ethrecv.c\uartWirelessRecv.c�ж����ͬһ����Ϣ����ROUTING_SERVR
 *   ����д������msgsnd...�������ں���ɶ��̶߳�ͬһ��Ϣ���в���ʱ��ͬ����
 * */

int NetCong_Wireless_MsgQSnd(int msgqid, void *msg, int msgsize,long msgtype, int msgflg)
{
	int ret;
	struct q_item titem;

	titem.type = msgtype;
	memcpy(titem.data,msg,msgsize);
	/*
	msgrcv()���Դ���Ϣ�����ж�ȡ��Ϣ��
	msgsnd()��һ���µ���Ϣд����С�
	����ԭ��:
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
 * @brief��
 * 			������ڡ������û�ָ��start_net_congitive, ��վ��ʼ������Ƶ�׸�֪���̣��ռ����ӽڵ�ı��ظ��ŵ�Ƶ�׽����
 * 			���ͨ�������㷨���������ȿ���Ƶ��
 * @Note��
 * 			���ӽڵ��յ����п�ʼָ�����Routing.c�ļ���handleWLanData�����д���CONG_START_PK_TYPE,����Cong_Local_Start
 *
 * @author: jiwen@iscas.ac.cn
 * */
void Start_Net_Congitive()
{
	//��ʱͨ��ҵ������ģ������нڵ�ı��н���ϱ����ڵ�0

	/*
	 * STEP1: ��ʼ�������������ָ֪�����ݰ�
	 * */
	CONG_CMD_PK	cong_pk;
	int 		cmd_len = strlen(NET_CONG_CMD);

	master_cong_id = self_node_id;

	memset(&cong_pk, 0, sizeof(CONG_CMD_PK));
	cong_pk.pk_type = CONG_START_PK_TYPE;

	cong_pk.send_id = master_cong_id;

	memcpy(cong_pk.cong_cmd,NET_CONG_CMD , cmd_len);

	cong_pk.pk_crc = getCRC((u8*)&cong_pk, sizeof(CONG_CMD_PK)-2);

	//���ڵ㱾������״̬
	global_running_state = NET_CONG_RUNNING;			//��ʼ����

	/*
	 * STEP2�� �����п�ʼָ��㲥���͸������������ڵ�
	 * */
	//TODO ...�Ƿ�WLAN_ROUTE_CTRL������ʷ��Ϣ����յ�������

	NetCong_Wireless_MsgQSnd(send_msgq_id, (u8*)&cong_pk, sizeof(CONG_CMD_PK), WLAN_ROUTE_CTRL,IPC_NOWAIT);

	/*
	 * STEP3�����ڵ����������֪��ʱ������ʱ����ʱ����������Ƶ�׾����㷨
	 * */

	CongNet_Timer_Start();

	/*
	 * STEP4�����ڵ�Ҳ����ʼ��һ�ֱ��ظ�֪����
	 * */
	Cong_Local_Start();

}




/*
 *@function:
 *	 		Loc_Congitive_Timer_CallbackHandler
 *@brief:
 *		 	���ظ�֪-��ʱ������,��ʱ���ĵ�ǰ�ŵ���Ƶ�ʣ���ȡ�ŵ���RSSI����ӳ����Ƶ�����
 *@Note:
 *			��RSSIģʽ�£�ģ���ÿ��100ms��ʱ���RSSI��Ϣ�������û����ʵ��ʹ�û������ź�ǿ��
 * */
void Loc_Congitive_Timer_CallbackHandler(int sig, siginfo_t *si, void *uc)
{
	u8  ch =0;		//�ŵ��±������
	u8 rssi = 0;
	int rv= -1;
	int e43_uart_fd =-1;
	int cong_cnt	 =0;
	int temp_rssi_acc =0;

	Congitive_Result_t* cong_res_pk_ptr = &g_net_cong_result_list[self_node_id-1].cong_result ;

	int pk_len = sizeof(Congitive_Result_t);
	/*
	 *�жϸ�֪�ŵ��±��Ƿ�������ֵ��������������ɨ��
	 * */
	if(ch > CH_MAX_VAL)
	{
		ch =0;
	}

	/*
	 * ��ʼȫƵ�α��У���ȡ�����ŵ���RSSI������
	 * */
	do{
		/*
		 *���õ�ǰ�ŵ��±��Ӧ��Ƶ��Ϊ��֪�ŵ�
		 * */
		e43_uart_fd = E43_channl_rssi_congitive((Radio_CHL_t)ch);
		temp_rssi_acc =0;
		/*
		 * ���ɨ�赱ǰ�ŵ�RSSI��������¼&����RSSI���ֵ����Сֵ��ƽ��ֵ
		 * */
		while(cong_cnt < CONG_MAX_TIMES_PER_CHNL )
		{
			rv = uartrecvfrom(e43_uart_fd, &rssi, 0);
			if(rv >0)
			{
//				if(cong_cnt==0)
//				{
//					min_rssi[ch] = rssi;
//					max_rssi[ch] = rssi;	//��ǰ�ŵ�RSSI���ֵ
//				}
//				else
//				{
//					if(rssi > max_rssi[ch])
//					{
//						max_rssi[ch] = rssi;	//��ǰ�ŵ�RSSI���ֵ
//					}
//
//					if(rssi < min_rssi[ch])
//					{
//						min_rssi[ch] = rssi;	//��ǰ�ŵ�RSSI��Сֵ
//					}
//				}
				temp_rssi_acc += rssi;		//�ۼ�RSSIֵ��
			}
			cong_cnt++;
			//LOG_MSG(DBG_LEVEL,"cong_cnt%d,CH%d, RSSI bytes %d, bytes: %d\n", cong_cnt,ch, rv ,rssi);
		}

		avg_rssi[ch] = (u8)(temp_rssi_acc /CONG_MAX_TIMES_PER_CHNL);//��ǰ�ŵ�RSSIƽ��ֵ
		LOG_MSG(DBG_LEVEL,"-------------------------------------------------------ch%d' rssi max=%d, avg=%d, min=%d,\n",\
					ch,\
					max_rssi[ch],\
					avg_rssi[ch],\
					min_rssi[ch]);

		//׼��ɨ����һ���ŵ�
		ch++;
		close(e43_uart_fd);

	}while(ch < CH_MAX_NUM );

	//�ϱ�/��¼���н��
	LOG_MSG(DBG_LEVEL,"---------------------Master_Cong_id=%d, self_node_id=%d \n", master_cong_id, self_node_id);
	if(master_cong_id == self_node_id)
	{
		//���ڵ㷢�������֪���򽫱��ڵ�ĸ�֪�������������Ӧ�ڵ�ĸ�֪�����
		memset((u8*)cong_res_pk_ptr,0, pk_len);
		/*
		 * �����н�������ݣ���������Ӧ���ֶ�
		 * */
		g_net_cong_result_list[self_node_id-1].node_sta = CONG_OK;

		cong_res_pk_ptr->pk_type = CONG_RET_PK_TYPE;
		cong_res_pk_ptr->send_id = self_node_id;
		cong_res_pk_ptr->recv_id = master_cong_id;

		//����֪���������ȫ��������

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
 * 		   ����վ�ϱ����н�������н��RSSI��ӳ���ص�Ż�����RSSI��ֵΪ���ֵ
 * */
static void Local_Cong_Upload()
{
	//��ʱͨ��ҵ������ģ������нڵ�ı��н���ϱ����ڵ�0
	Congitive_Result_t cong_res_pk ;

	int pk_len = sizeof(Congitive_Result_t);

	LOG_MSG(DBG_LEVEL,"--------------------------------------Local_Cong_Upload\n");

	memset(&cong_res_pk,0, pk_len);

	/*
	 * �����н�������ݣ���������Ӧ���ֶ�
	 * */
	cong_res_pk.pk_type = CONG_RET_PK_TYPE;
	cong_res_pk.send_id = self_node_id;
	cong_res_pk.recv_id = master_cong_id;

	memcpy(cong_res_pk.avg_rssi, avg_rssi, CH_MAX_NUM);

	cong_res_pk.pk_crc = getCRC((u8*)&cong_res_pk, pk_len-2);

	//ͨ�����߷��������ڵ�
	NetCong_Wireless_MsgQSnd(send_msgq_id, &cong_res_pk, pk_len, WLAN_ROUTE_DATA,IPC_NOWAIT);
	return ;

}


/*
 * @function:
 * 		 Cong_Local_Start
 * @brief:
 * 		 1>��վ�յ���start_net_congitive�����������ʼ���в���Cong_Local_Start()...��
 * 		 �����ڵ�ı��н���������±�Ϊnode_id��g_net_cong_result_list[]�����С�
 *
 * 		 2>ͬʱ����վͨ�������ŵ��·����п�ʼ����,��վ�յ��󣬿�ʼ���й���
 * @return :
 * 		void
 * */
extern int g_e43_uart_fd;

void Cong_Local_Start()
{

	u8  ch =0;		//�ŵ��±������
	u8 	rssi = 0;
	int rv= -1;
	int e43_uart_fd =-1;
	int cong_cnt	 =0;
	int temp_rssi_acc =0;

	LOG_MSG(DBG_LEVEL,"\n-------------------------------------------------------my_node_id =%d, Start Local Congitive\n", self_node_id);

	/*
	 * �����ڵ�id��0��ʼ��
	 * */
	Congitive_Result_t* cong_res_pk_ptr = &g_net_cong_result_list[self_node_id].cong_result ;

	int pk_len = sizeof(Congitive_Result_t);
	close(g_e43_uart_fd);
	g_e43_uart_fd =-1;
	/*
	 * ��ʼȫƵ�α��У���ȡ�����ŵ���RSSI������
	 * */
	do{
		/*
		 *���õ�ǰ�ŵ��±��Ӧ��Ƶ��Ϊ��֪�ŵ�
		 * */
		e43_uart_fd = E43_channl_rssi_congitive((Radio_CHL_t)ch);
		temp_rssi_acc =0;
		cong_cnt =0;
		/*
		 * ���ɨ�赱ǰ�ŵ�RSSI��������¼������RSSI���ֵ����Сֵ��ƽ��ֵ
		 * */
		while(cong_cnt < CONG_MAX_TIMES_PER_CHNL )
		{
			rv = uartrecvfrom(e43_uart_fd, &rssi, 1);
			if(rv >0)
			{
				if(cong_cnt==0)
				{
					min_rssi[ch] = rssi;
					max_rssi[ch] = rssi;	//��ǰ�ŵ�RSSI���ֵ
				}
				else
				{
					if(rssi > max_rssi[ch])
					{
						max_rssi[ch] = rssi;	//��ǰ�ŵ�RSSI���ֵ
					}

					if(rssi < min_rssi[ch])
					{
						min_rssi[ch] = rssi;	//��ǰ�ŵ�RSSI��Сֵ
					}
				}
				temp_rssi_acc += rssi;		//�ۼ�RSSIֵ��
			}
			cong_cnt++;
//			LOG_MSG(DBG_LEVEL,"cong_cnt%d,CH%d, RSSI bytes %d, bytes: %d\n", cong_cnt,ch, rv ,rssi);
		}

		avg_rssi[ch] = (u8)(temp_rssi_acc /CONG_MAX_TIMES_PER_CHNL);//��ǰ�ŵ�RSSIƽ��ֵ
		LOG_MSG(DBG_LEVEL,"-------------------------------------------------------ch%d' rssi max=%d, avg=%d, min=%d,\n",\
					ch,\
					max_rssi[ch],\
					avg_rssi[ch],\
					min_rssi[ch]);

		//׼��ɨ����һ���ŵ�
		ch++;
	}while(ch < CH_MAX_NUM);

	//�ϱ�/��¼���н��
	LOG_MSG(DBG_LEVEL,"---------------------Master_Cong_id=%d, self_node_id=%d \n", master_cong_id, self_node_id);
	if(master_cong_id == self_node_id)
	{
		//���ڵ㷢�������֪���򽫱��ڵ�ĸ�֪�������������Ӧ�ڵ�ĸ�֪�����
		LOG_MSG(DBG_LEVEL,"---------------------Master Local Cognitive Over, Result Saving...\n");

		memset((u8*)cong_res_pk_ptr,0, pk_len);
		/*
		 * �����н�������ݣ���������Ӧ���ֶ�
		 * */
		g_net_cong_result_list[self_node_id].node_sta = CONG_OK;


		/*
		 * Note:
		 * 	cong_res_pk_ptrָ��g_net_cong_result_list[self_node_id].cong_result
		 * */
		cong_res_pk_ptr->pk_type = CONG_RET_PK_TYPE;
		cong_res_pk_ptr->send_id = self_node_id;
		cong_res_pk_ptr->recv_id = master_cong_id;
		//����֪���������ȫ��������
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
 *  ���ڵ��յ��û�net_congtive_startָ����������ؽڵ������֪��ʱ��ʱ������ʱ����ʱ������Ƶ�׾����㷨��
 *
 *@return :
 *	void
 * */
void CongNet_Timer_Start()
{
	wdStart(wd_Net_Congitive, NET_CONG_DELY , TIMER_ONE_SHOT);
}

/*
 * ��������Ƶ��ɨ�趨ʱ�����ڶ�ʱ�������и���ɨ���ŵ���ţ�Ȼ���ȡ��ǰ�ŵ���RSSIֵ
 * */
void NetCong_Module_Init()
{
	int ret= -1;

	/*
	 *STEP1����ʼ�������֪���Ϊ0
	 * */
	memset(g_net_cong_result_list, 0, MAX_NET_NODES*sizeof(Net_Congitive_Result_t));

	/*
	 *STEP2:��ȡMAC�㷢�͡�ҵ�񡱵���Ϣ���о��
	 * */
	ret = NetCong_get_Wireless_SndQueueId();
	if(ret<0){
		LOG_MSG(ERR_LEVEL,"------------------NetCong_get_Wireless_SndQueueId error\n");
		return ;
	}

	 /*
	  *STEP3: ���������֪��ʱ��ʱ����
	  * */
	ret = wdCreate(WD_NET_CONG_MSG_SIG, &wd_Net_Congitive, &NetCong_TimeOut_CallbackHandler);
	if(ret ==-1)
	{
		LOG_MSG(ERR_LEVEL,"create wd_Cogitive  WD_NET_CONG_MSG_SIG failed\n");
		exit(-1);
	}

//	srand((unsigned)time(NULL));		/*����ָ����ͬ����Ϊ���ӣ�ָ��time����ʱ��ʱ�̲�ͬ��������Ӳ�ͬ�����������Ҳ��ͬ*/
}
