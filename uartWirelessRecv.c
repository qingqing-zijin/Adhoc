/**
 * @fuction��	����������uart���ձ��ĵ��̣߳������ձ��ļ��뵽���ն��У���routerģ��ʹ�á�
 * @author��
 * @email:
 * @date��
 * @version��
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

#include "pthread.h"
#include "main.h"
#include "aux_func.h"
#include "queues.h"
#include "serial.h"
#include "radio.h"
#include "Routing.h"
#include "mac.h"
#include "Congitive.h"
#include "nRF24L01.h"


/*
 * ����buffer����
 */
#define MAX_RECV_BUFF_SIZE 		2048

int uartWireless_filter(unsigned char *recvBuf);

void * M_e32__recv_ThreadFun(void *args);

void * L_e32_recv_ThreadFun(void *args);

/**/
extern int e32_uart_fd;
extern unsigned short self_node_id;

static int Handle_RecevieRadioLinkParams(Radio_LinkParam* radio_params)
{
	//��������E32��̨����
	/********************************************************
	 *				E32��������ҵ���̨
	 *******************************************************/
	if(radio_params->cfg_m_e32_spd_en && radio_params->cfg_m_e32_freq_en)
	{
		LOG_MSG(INFO_LEVEL,"------------- Handle_RecevieRadioLinkParams  E32 freq(%d) speed(%d) \n", radio_params->m_e32_freq, radio_params->m_e32_spd);
		g_radio_speed = SPD_E32_FLAG;		//
		M_E32_set_radio_params(radio_params->m_e32_freq,radio_params->m_e32_spd);
		return 0;
	}
	else if(radio_params->cfg_m_e32_spd_en)
	{
		LOG_MSG(INFO_LEVEL,"------------- Handle_RecevieRadioLinkParams  E32 freq(%d), speed(%d) \n",g_m_e32_ch,  radio_params->m_e32_spd);
		g_radio_speed = SPD_E32_FLAG;		//
		M_E32_set_radio_params(g_m_e32_ch,radio_params->m_e32_spd);
		return 0;
	}
	//���õ���E32��̨Ƶ��
	else if(radio_params->cfg_m_e32_freq_en)
	{
		//g_radio_speed = SPD_E32_FLAG;	  //��������Ƶ��ʱ�������л�ͨ���ֶ�
		LOG_MSG(INFO_LEVEL,"------------- Handle_RecevieRadioLinkParams  E32 freq(%d), speed(%d) \n", radio_params->m_e32_freq,g_m_e32_speed );
		M_E32_set_radio_params(radio_params->m_e32_freq,g_m_e32_speed);
		return 0;
	}

	/********************************************************
	 *				E01��������ҵ���̨
	 *******************************************************/
	if(radio_params->cfg_e01_freq_en)
	{
		LOG_MSG(INFO_LEVEL,"------------- Handle_RecevieRadioLinkParams  E01 freq(%d) \n", radio_params->e01_freq);
		L01_WriteFreqPoint(radio_params->e01_freq);		//����������
		return 0;

	}
	//���ø���E01����
	if(radio_params->cfg_e01_spd_en)
	{
		LOG_MSG(INFO_LEVEL,"------------- Handle_RecevieRadioLinkParams  E01 speed(%d) \n",radio_params->e01_spd);
		g_radio_speed = SPD_E01_FLAG;		//
		L01_SetSpeed(radio_params->e01_spd);
	}
	return 0;
}

void * L_e32_recv_ThreadFun(void *args)
{
	int recv_sz =0;
//	u8 recv_buf[60]={0,};
	Radio_LinkParam recv_buf;
	while (1)
	{
//		memset(recv_buf, 0,60);
		recv_sz = L_E32_pks_recvfrom(L_e32_uart_fd, L_e32_aux_gpio_input_fd, &recv_buf, sizeof(Radio_LinkParam));

		if (recv_sz > 0)
		{
			if(getCRC((u8*)&recv_buf, sizeof(Radio_LinkParam))!=0)
			{
				LOG_MSG(ERR_LEVEL,"------------- L_E32_pks_recvfrom  crc error \n");
				continue;
			}
			Handle_RecevieRadioLinkParams(&recv_buf);
		}
		else
		{
			continue;
		}
	}
}

pthread_t create_L_e32_uartWireless_recv_thread()
{
	int ret ;
	pthread_t tid;

	ret = pthread_create( &tid, NULL, &L_e32_recv_ThreadFun, NULL);
	if(ret != 0)
	{
		LOG_MSG(ERR_LEVEL,"create_uart_recv_thread\n");
		return ERROR;
	}
	//LOG_MSG(INFO_LEVEL,"create_uart_recv_thread success\n ");

	return tid;
}


/*
 * ��������������̣߳����մ������������յ�udp����
 * return pthread_t ���������߳�id
 */
pthread_t create_M_e32_uartWireless_recv_thread()
{
	int ret ;
	pthread_t tid;

	ret = pthread_create( &tid, NULL, &M_e32__recv_ThreadFun, NULL);
	if(ret != 0)
	{
		LOG_MSG(ERR_LEVEL,"create_uart_recv_thread\n");
		return ERROR;
	}
	//LOG_MSG(INFO_LEVEL,"create_uart_recv_thread success\n ");

	return tid;
}

/**
 * ���߽������̴��������򵥽��մ�ӡ�������ݣ��Ժ󽫽��յ��ı��Ĵ������
 */
void * M_e32__recv_ThreadFun(void *args)
{
	int route_msgqid, mac_msgqid;

	int recv_sz =0;
	unsigned char recv_buf[MAX_RECV_BUFF_SIZE]={0,};

	struct q_item qitem;
	struct msqid_ds ds;
	route_msgqid = msgget(RECV_CQ,IPC_EXCL);
	if(route_msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL,"get uartWireless route_cq");
		pthread_exit(1);
	}
	LOG_MSG(INFO_LEVEL,"uartWirelessRecv---------- route_cq id = %d\n",route_msgqid);


	mac_msgqid = msgget(MAC_QUEUE,IPC_EXCL);
	if(mac_msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL,"get uartWireless mac_cq");
		pthread_exit(1);
	}
	LOG_MSG(INFO_LEVEL,"uartWirelessRecv----------mac_cq id = %d\n",mac_msgqid);

	struct mac_q_item			mac_qitem;
	TokenRing_RX_RouteDATA_PK	TR_rx_data_pkt;
	TR_rx_data_pkt.pk_type = TOKEN_RING_RX_ROUTE_DATA_PK_TYPE;
	TR_rx_data_pkt.src_ID  = self_node_id & 0xff;
	mac_qitem.type = WLAN_DATA;
	memcpy(&mac_qitem.data,(u8*)&TR_rx_data_pkt ,sizeof(TokenRing_RX_RouteDATA_PK));

	while (1)
	{
		memset(recv_buf, 0,MAX_RECV_BUFF_SIZE);

		recv_sz = radio_pks_recvfrom(M_e32_uart_fd, M_e32_aux_gpio_input_fd,recv_buf, MAX_RECV_BUFF_SIZE);

		if (recv_sz > 0)
		{
			qitem.type = WLAN_DATA;
			//������������
			memcpy(&qitem.data,recv_buf ,recv_sz);

			switch (recv_buf[0])
			{
				case TOKEN_RING_BC_REQ_PK_TYPE:
				case TOKEN_RING_REQ_PK_TYPE:
				case TOKEN_RING_REP_PK_TYPE:
				case TOKEN_RING_OK_PK_TYPE:
				case TOKEN_RING_OK_ACK_PK_TYPE:
				{
//					LOG_MSG(ERR_LEVEL,"uart_recv  msgsnd mac_queue [0]=%d\n ", recv_buf[0]);
					//������MAC��Ϣ����
//					if( msgsnd(mac_msgqid,&qitem,recv_sz,IPC_NOWAIT) < 0)
					if( msgsnd(mac_msgqid,&qitem,recv_sz,0) < 0)
					{
						if(0 == msgctl(mac_msgqid,IPC_STAT,&ds))
						{
							LOG_MSG(INFO_LEVEL,"current size = %ld,max size = %ld\n",ds.__msg_cbytes,ds.msg_qbytes);
						}
						LOG_MSG(ERR_LEVEL,"uart_recv  msgsnd mac_queue\n ");
//						break;
					}
					break;
				}

				case DATA_PK_TYPE:
				case LSA_PK_TYPE:
				case RREQ_PK_TYPE:
				case RREP_PK_TYPE:
				case RERR_PK_TYPE:
				case CONG_START_PK_TYPE:
				case CONG_RET_PK_TYPE:
				case CONG_FREQ_RESULT_PK_TYPE:
				{
					//������Router��Ϣ����
					if( msgsnd(route_msgqid,&qitem,recv_sz,IPC_NOWAIT) < 0)
					{
//						LOG_MSG(ERR_LEVEL,"uart_recv  msgsnd route_msgqid\n ");

						if(0 == msgctl(route_msgqid,IPC_STAT,&ds))
						{
							LOG_MSG(INFO_LEVEL,"current size = %ld,max size = %ld\n",ds.__msg_cbytes,ds.msg_qbytes);
						}
						LOG_MSG(ERR_LEVEL,"uart_recv msgsnd route_queue\n");
//						break;
					}

					//����MAC�㷢�ͽ��յ�ҵ�����ݱ���ָʾ

					if( msgsnd(mac_msgqid,&mac_qitem,sizeof(TokenRing_RX_RouteDATA_PK),0) < 0)
					{
						if(0 == msgctl(mac_msgqid,IPC_STAT,&ds))
						{
							LOG_MSG(INFO_LEVEL,"current size = %ld,max size = %ld\n",ds.__msg_cbytes,ds.msg_qbytes);
						}
						LOG_MSG(ERR_LEVEL,"uart_recv  msgsnd mac_queue\n ");
					}
					break;
				}

				default:
				{
					LOG_MSG(WARN_LEVEL,"unknown pkt [0x%x] received from radio\n",recv_buf[0]);
					break;
				}

			}
		}
		else
		{
			continue;
		}
	}
	close(e32_uart_fd);
}

/*
 * ����wlan �㲥���Լ�����
 * �������Ҫ������-1
 * ���򷵻�1;
 */
int uartWireless_filter(unsigned char* recvbuf)
{
	//struct in_addr gwl = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_addr)->sin_addr;

	//printf("src_ip = %x,gwl.s_addr = %x\n",srcip.s_addr, gwl.s_addr);

	//���յ�IP����Դ��ַ�뱾��gwl IP��ͬ������Ϊ���Լ����͵����ݱ��ģ�����

	//���ݱ��ĵ�Դ��ַ�ͱ��ڵ�node_id��ȡ�
	if(recvbuf[0]== self_node_id)
	{
			return -1;
	}
	return 1;
}
