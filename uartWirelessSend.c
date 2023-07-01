/**
 * @fuction��	���̣߳���uart���߷������ȶ����л�ȡ���ͱ��ģ�ͨ��udp�㲥����Ϣ��
 * @author��
 * @email:
 * @date��
 * @version��
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
 * 1> radio�����ŵ�ʱ����������ACCESS_CHNL_PERIOD_MSG+��������Լ�����ײ�����ļ���
 * 2> ��Routing.c��ͨ�����ڶ�ʱ������LSA���ģ�����ֵ��routing.ini����
 * 3> ��ACCESS_CHNL_PERIOD_MSG�����ŵ�����������Ϊ250ms,��ͨ����������Radioͨ������
 *    �Լ��û��ն�ÿ�����ɲ����ֽ��������600�ֽڣ����������ġ�
 *
 * 	  radio	4.8kbpsʱ��ÿ��600�ֽڣ�ÿ500ms 300�ֽڣ�ÿ250ms 150�ֽڣ�ÿ100ms 60�ֽڣ� ÿ50ms 30�ֽ�
 * 	  radio	9.6kbpsʱ��ÿ��1200�ֽڣ�ÿ500ms 600�ֽڣ�ÿ250ms 300�ֽڣ�ÿ100ms 120�ֽڣ� ÿ50ms 60�ֽ�
 * 	  radio	19.2kbpsʱ��ÿ��2400�ֽڣ�ÿ500ms 1200�ֽڣ�ÿ250ms 600�ֽڣ�ÿ100ms 240�ֽڣ� ÿ50ms 120�ֽ�
 *
 * 4> ��radio���ڽ������ݰ�ʱ���������ݰ���ʱ����ʱ��������ʱӦ��ȡ����˱�t2ʱ���ó��ŵ���ȡֵΪ0��25ms
 * */

/*
 * 250ms, 19.2kbpsʱ����̨�ɴ���300�ֽ�;���������ڴ���4���ڵ㣨��̨��������ͬƵ
 * �������Ҷ��п�����������ŵ���Ϊ������ײ������ʱ��Ҫ�������ʲ�ͬ����Ҫ�����ټ��250msΪ�ѡ�
 * ���
 * */
#define ACCESS_CHNL_PERIOD_MSG		300		//19.2kbpsʱ���ɴ���300�ֽ�

/*
 * �ŵ������������t1�����ֵ
 * */
const double rand_t1_max 			=150.0;

/*
 * �ŵ�����ʱ��Ϊ���̶�����+�������t1
 * */
#define rand_t1  					(1+ (int)(rand_t1_max *(rand()/(RAND_MAX+1.0)))) /*0~x00�����*/

/*
 * ��Radio�����ڽ������ݰ�״̬ʱ�����Ͷ�ʱ���ص�����������ʱ���������rand_t2ʱ��
 * */
#define rand_t2  					(1+ (int)(50.0 *(rand()/(RAND_MAX+1.0)))) /*0~x00�����*/

int snd_msgqid;

//extern u8 	global_radio_on_receving ;
/*
 *
 * �ŵ���̬�����뼼���ɷ�Ϊ�ܿؽ���������������
 * (1) ������롡�ص������е��û������Ը����Լ�����Ը��������ŵ��Ϸ�����Ϣ��
 * ���������������ϵ��û����ڹ�����ŵ��Ϸ�����Ϣ��ʱ�򣬾Ͳ����˳�ͻ(collision)��
 * �������û��ķ���ʧ�ܡ�������뼼����Ҫ�����о������ͻ������Э�顣
 * �������ʵ���Ͼ������ý��룬����ʤ���߿�����ʱռ�ù����ŵ���������Ϣ��
 * ���������ص��ǣ�վ�����ʱ�������ݣ������ŵ����׳�ͻ�����ܹ������Ӧվ����Ŀ����ͨ�����ı仯��
 * ���͵�������뼼����ALOHA��CSMA��CSMA/CD��
 *
 * (2) �ܿؽ��롡�ص��Ǹ����û�������������ŵ����������һ���Ŀ��ƹ���
 * �ɷ�Ϊ����ʽ���ƺͷ�ɢʽ���ơ����͵��ж����·��ѯ�����ƴ��ݡ�
 *    a) ��ѯ���ڼ���ʽ���ƣ����ƽڵ㰴һ��˳����һѯ�ʸ��û��ڵ��Ƿ�����Ϣ���͡�
 *    ����У���ѯ�ʵ��û��ڵ����������Ϣ���͸����ƽڵ㣻��û�У�����ƽڵ�����ѯ����һ�ڵ㡣
 *    b) ���ƻ����ڷֲ�ʽ���ƣ��ڻ�·��ͨ����������ƻ�֡���Ż�·��վ���ݣ�ֻ�л�����ƵĽڵ����Ȩ������Ϣ��
 *       ����Ϣ������ϣ��ͽ����ƴ��ݸ���һվ��
 *
 * (3) ����Ŀ����ˮ�����4�����̲�Ad Hocͨ�Žڵ㣬 ���нڵ�1Ϊ���Ͳ���ͧ������������ͨ������ǿ�������ڵ�Ϊ��Ȼ������ͧ
 *    a) �����ܿ���ѯ��ʽ�����ŵ������нڵ�1Ϊ���ƽڵ㣬�ڵ�2��3��4Ϊ�û��ڵ㡣��ѯ��ʽ�����ŵ�
 *    �Ĳ������£�
 *       1�����ڽڵ�1�������ݰ�����ֽ���Ϊ600�ֽ�($UB),�����ڵ㷢�����ݰ�����ֽ���Ϊ300�ֽ�,ͨ������Ϊ19.2kbps,
 *	        radio 4.8kbpsʱ��ÿ��600�ֽڣ�ÿ500ms 300�ֽڣ�ÿ250ms 150�ֽڣ�ÿ100ms 60�ֽڣ� ÿ50ms 30�ֽ�
 * 	  		radio 9.6kbpsʱ��ÿ��1200�ֽڣ�ÿ500ms 600�ֽڣ�ÿ250ms 300�ֽڣ�ÿ100ms 120�ֽڣ� ÿ50ms 60�ֽ�
 * 	  		radio 19.2kbpsʱ��ÿ��2400�ֽڣ�ÿ500ms 1200�ֽڣ�ÿ250ms 600�ֽڣ�ÿ100ms 240�ֽڣ� ÿ50ms 120�ֽ�

 *			����ŵ��������ʱ����Ҫ250ms��
 *
 *			��ʱ�������ڸ��ݵ�̨�ĵ�ǰ�������ʺ��û����ݰ���Сȷ����
 *
 *	     2) �ڵ�1��Ϊ���ƽڵ㣬ע���ŵ����붨ʱ������ʱ����ʱ����Ϊ250ms���ڶ�ʱ��ISR������
 *	        ʵ�ָ��ڵ�����ŵ���ѯ��
 *
 *	     3��  ʵ��˼·�������£�
 *			��ʱ����ʱ�¼�������ִ��Node1_Timer_ISR, ���Ƚڵ�1�Լ��ж��Ƿ�����Ϣ�����ͣ�
 *	        ����У���ռ���ŵ�������1����Ϣ�� Ȼ���˳�ISR��
 *
 *	        ���ޣ�������ѯ�ڵ�2��3��4�Ƿ������ݴ����ͣ���������TokenRingReq��x; x=2��3��4��,
 *	        ���ڵ�(x)�յ�TokenRingReq(x)����󣬸��ݷ�����Ϣ���е��Ƿ�Ϊ�գ��ظ�ACK��0-����Ϣ��1-����Ϣ�����ͣ�.
 *			�ڵ�1����"�ڵ�idС���ȼ��ߵĲ��ԣ�����ACK" ;
 *			���ڵ�n(n=min{2\3\4})�ظ�ACK����Ϣ���ͣ���ڵ�1���ڵ�n����TokenRingRep(n)ָ�
 *			�ڵ�n�յ�TokenRingRep(n)ָ�����ռ���ŵ�������1����Ϣ;
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
		 * ������Ϣ����WLAN_SEND_CQ�е���Ϣ����0�ֽ�Ϊpk_type,���Ͱ���LSA_PK_TYPE/DATA_PK_TYPE/RREQ_PK_TYPE/RREP_PK_TYPE��
		 * CONG_PK_TYPE�����籾�п�ʼָ��
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
		 * ���ڵ���������Эͬ��֪�����㷨������Ƶ�ʺ����ʲ�����Ȼ��
		 * ͨ��CONG_FREQ_PK_TYPE���Ͱ��·��������������ڵ㡣�·�ʱ��
		 * 1���Ƚ�CONG_FREQ_PK_TYPE���͹㲥����
		 * 2��Ȼ�����ڵ��Լ����ù������µ�Ƶ�ʺ����ʲ���
		 * */
		config_otherRadios_Over = TRUE;

	}
	else if(msgitem->data[0]== CONG_RET_PK_TYPE )
	{
		destid =master_cong_id;			//Ŀ�Ľڵ�Ϊ���з����ߣ���Routing.c handleWLanData�и�ֵ

		LOG_MSG(INFO_LEVEL,"\n============================================radio_pk_send CONG_RET_PK_TYPE to uartWireless, len = %d\n\n",msglen);
	}
	else
	{
		/*
		 * ��LSA_PK_TYPE,���ַΪ�ǹ㲥��ַ������10.0.destid.1�����4����5�ֽ�Ϊ��Ϣ��rcv_id
		 *
		 * 2018-06-06 ͨ������queues.h��WLAN_ROUTE_DATA=1, WLAN_ROUTE_CTRL=2,
		 * ������APP_PK�������ȼ����Ӷ��ﵽ���ݷ��;���ʵʱ��
		 *
		 * ������Ʃ�緢��APP_PKʱ��������htons()API������������short���ͱ�����С��ģʽ��ת��Ϊ�����ֽ�˳�򣨴��ģʽ����
		 * �����ֽ�˳��ʱ���ֽڴ洢�ڵ�λ�����ֽڴ洢�ڸ�λ��Ʃ�������data[4]Ϊ��λ��data[5]Ϊ��λ����ˣ�
		 * �ڱ�ʾĿ��idʱ��data[4]<<8λ��
		 *
		 * */
		destid = ((unsigned int)msgitem->data[4]<<8) + ((unsigned int)msgitem->data[5]);
		LOG_MSG(INFO_LEVEL,"\n============================================radio_pk_send PK to uartWireless,dest_id =%d,  len = %d\n",
				destid,\
				msglen);
	}

	//��������ģ�鷢�ͺ���
	memcpy(temp_buff, msgitem->data, msglen>512 ? 512: msglen);

	radio_pks_sendto(M_e32_uart_fd, temp_buff , msglen, destid);

	/*
	 * ���ڵ��Լ����ù������µ�Ƶ�ʺ����ʲ���
	 * */
	if(config_otherRadios_Over ==TRUE)
	{
//		global_freq_config_state = FREQ_CONFIG_END;
		global_running_state = FREQ_CONFIG_END;
		config_otherRadios_Over	= FALSE;
	}




}

/*
 * ����ʽ���Ʒ����ŵ�
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
		//ĳ�ڵ㳬ʱδ�ظ�ACK����վ��״̬����λ����ʼ״̬��׼��ѯ����һ���ڵ�
		LOG_MSG(INFO_LEVEL,"-------------------------------------------------------------Master failed/timeout(%d) return TR_ST_INIT \n",timeouts);
//				global_token_ring_state = TR_ST_INIT;
		Set_TR_State(TR_ST_INIT);
		wdStart(wd_UART, SLICE_TIMEOUT_MS, TIMER_ONE_SHOT);
		return;
	}
	/*Step1>
	 * �����ؽڵ�ѯ�������Ƿ�����Ϣ������
	 * ��Ҫ���Ƕ����м̳������ŵ�ռ�õĿ���
	 * ����һ���ڵ��յ����ؽڵ��ҵ�����ݱ��ĺ󣬸�����ã˻ظ���ʱ�ӡ�
	 * */
	msglen = msgrcv(snd_msgqid,&msgitem,MAX_MSG_LEN,-2,IPC_NOWAIT);
	if(msglen > 0)
	{
		snd_msgTo_radio(&msgitem, msglen);
	}

	/* Step2>
	 * ��ѯ�����ڵ��Ƿ������ݴ�����
	 *
	 */
	Poll_Members_Control_Channel();

	//��ѯ��������������ÿ���Ȩ
	wdStart(wd_UART, 1, TIMER_ONE_SHOT);
}

/*
 * mgsrcv��������ȡ��Ϣ�����е���Ϣ
 * int msgrcv(int msgid, struct msgbuf* msgp, int size,long msgType, int flag)
 * ���У�
 * msgType 	0:��ʾ������Ϣ�����е�һ����Ϣ
 * 			����0��������Ϣ�����е�һ������ΪmsgType����Ϣ
 * 			С��0��������Ϣ�����е�һ������ֵ��С��|mgsType|����ֵ������ֵ����С����Ϣ
 *
 * flag    MSG_NOERROR:��ʾ�����ص���Ϣ��size�ֽڶ࣬����Ϣ�ͻᱻ�ض̵�size�ֽڣ��Ҳ�֪ͨ��Ϣ���ͽ���
 * 		   IPC_NOWAIT: ��ʾ����Ϣ��û���������ͣ�����ý��̻���������
 * 					0������������ֱ����������
 *
 * ����ֵ�� Number of bytes copied into message buffer
 *
 **/
/**
 * ���߷���ʱ�������������������ȼ�:
 *   1> WLAN_ROUTE_DATA  1
 *   2> WLAN_ROUTE_CTRL  2
 *
 * ���߽��մ���ʱ����һ������PK���ͷֱ���
 * 	1> #define WLAN_DATA			0x03
 *  2> #define LAN_DATA				0x04
 *  3> #define SELF_DATA			0x05
 *
 * *****
//���⣬����msgrcv()API,��msgtype��������δ���Ϣ�����л�ȡ���ݣ�
 * ���Linux Manual https://linux.die.net/man/2/msgrcv�н������£�
 * msgtype:
 *1��if msgtype is 0, then the first message in the queue is read.
 *2��if msgtyp is greater than 0, then the first message in the queue of type msgtype is read.
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
 *STEP1> �����жϱ��ڵ㣨1���Ƿ�����ϢҪ���ͷ�������ʽ
 * */
//	msglen = msgrcv(snd_msgqid,&msgitem,MAX_MSG_LEN,-2,IPC_NOWAIT);
/*
 *
 * */
//	while(global_token_ring_state != TR_ST_INIT && timouts++ <SLICE_TIMEOUT_MS )
//	{
//		usleep(1000);		//�ȴ�255*2ms
//	}


/*
 * ���߷���ʱ�������������������ȼ�:
 *   1> WLAN_ROUTE_DATA  1
 *   2> WLAN_ROUTE_CTRL  2
 * ֵ����Ϊ-2,��ʾ���Ȼ�ȡWLAN_ROUTE_DATA
 * */

/*
 *@function create_uartWireless_sendTimer_thread
 *@����	�������������ŵ���ʱ��������Ѯ�����ŵ�
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



	//�ڵ�1��Ϊ�ŵ��������Ŀ��ƽڵ�
	if(self_node_id == 1)
	{
		/*
		 * ������ʱ�������ڷ��Ͳ��Ա���
		 * */
		ret = wdCreate(WD_UART_MSG_SIG, &wd_UART, &MAC_Wireless_Centralize_SendHandler);
		wdStart(wd_UART,1000 , TIMER_ONE_SHOT);
		//wdStart(wd_UART,SLICE_TIMEOUT_MS , TIMER_ONE_SHOT);
	}
	return ret;
}
