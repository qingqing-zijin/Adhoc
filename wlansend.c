/**
 * @fuction��	���̣߳������߷������ȶ����л�ȡ���ͱ��ģ�ͨ��udp�㲥����Ϣ��
 * @author��  	lsp
 * @email: 		siping@iscas.ac.cn
 * @date��		2015-2-4
 * @version��	v0.0.1
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <net/if.h>           // struct ifreq
#include <sys/ioctl.h>        // macro ioctl is defined
#include <pthread.h>
#include "main.h"
#include "aux_func.h"
#include "queues.h"
#include <ifaddrs.h>
#include "Routing.h"
void * send_to_wlan_routine(void * args);
void wlan_query_and_send(int sock);
extern struct ifaddrs g_wlanif;
extern struct ifaddrs g_lanif;
extern long long glInTime,glOutTime;
extern long long gwlInTime,gwlOutTime;

pthread_t create_wlan_sendto_thread()
{
	int ret;
	pthread_t tid;
	ret = pthread_create(&tid,NULL,&send_to_wlan_routine,NULL);
	if(ret != 0)
	{
		LOG_MSG(ERR_LEVEL,"create send_to_wlan_routine err\n");
	}
	//LOG_MSG(INFO_LEVEL,"Create wlan send thread success\n");
	return tid;
}

/**
 * ���������߶˷��͵�udp socket����ȡ���ȶ����ڲ�������Ϣ�������䷢�ͳ�ȥ��
 */
void *send_to_wlan_routine(void * args)
{
	int s;

	const int on = 1;
	char* interface = WLAN_INTERFACE;
	struct ifreq ifr;

	//�����շ�UDP���ݱ��׽���
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"error:");
		exit(-1);
	}

	//��socket��ָ������ӿ�
	// Use ioctl() to look up interface index which we will use to
	// bind socket descriptor sd to specified interface with setsockopt() since
	// none of the other arguments of sendto() specify which interface to use.
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface);
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
	{
		LOG_MSG(ERR_LEVEL,"ioctl() failed to find interface ");
		exit(-1);
	}

	//û�е���IP_HDRINCL

	/*
	����Ҫ��д�Լ���IP���ݰ��ײ�ʱ��������ԭʼ�׽����������׽���ѡ��IP_HDRINCL.
	�ڲ��������ѡ�������£�IPЭ���Զ����IP���ݰ����ײ���

����int on = 1��if��setsockopt��sockfd�� IPPROTO_IP�� IP_HDRINCL�� &on�� sizeof��on���� < 0��

����{ fprintf��stderr�� "setsockopt IP_HDRINCL ERROR�� /n"����exit��1����}

	*/	

    //��LEVELΪSOL_SOCKET��SO_BROADCAST�������׽��������óɶԹ㲥��Ϣ���з���
    //�����͹㲥��Ϣ

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"setsockopt() failed to set SO_BROADCAST ");
		exit(-1);
	}
	wlan_query_and_send(s);
}

void wlan_query_and_send(int sock)
{
	struct sockaddr_in daddr;
	struct q_item msgitem;
	int msglen;
	int msgqid;
	struct in_addr dest;
	unsigned int destid;

	msgqid = msgget(WLAN_SEND_CQ,IPC_EXCL);
	if(msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL,"WLAN_SEND_CQ not found\n");
		pthread_exit(1);
	}
	//LOG_MSG(INFO_LEVEL,"wlan send cq id = %d\n",msgqid);

	while(1)
	{
		memset(&msgitem,0,sizeof(msgitem));
		
		msglen = msgrcv(msgqid,&msgitem,MAX_MSG_LEN,-2,MSG_NOERROR);
		
		if(msglen > 0)
		{
			//LOG_MSG(INFO_LEVEL,"\n============================================Send to WLAN, len = %d\n\n",msglen);
			//hexprint(msgitem.data,msglen);

			//��װ�����ַ����ʼ��Э�����͡�Ŀ�ĵ�ַ��Ŀ�Ķ˿ں�. daddrΪstruct sockaddr_in
			daddr.sin_family = AF_INET;
			daddr.sin_port = htons(WLAN_LISTEN_PORT);
			
			if(msgitem.data[0] == LSA_PK_TYPE )
			{
				//LSA��·״̬��255�㲥��ʽ����
#ifndef UART_Wireless_INTERFACE
				daddr.sin_addr = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_ifu.ifu_broadaddr)->sin_addr;
			}
			else
			{

				//����Wlan RREQ�Թ㲥��ʽ���ͣ�MPR�ڵ�ת����ΪĿ�Ľڵ㴦��RREQ,�����ڵ�ֻ�ղ�����
				destid = ((unsigned int)msgitem.data[4]) + ((unsigned int)msgitem.data[5]<<8);

				//Ŀ�ĵ�ַΪ255���㲥
				if(destid == 0xffff)
				{
					daddr.sin_addr = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_ifu.ifu_broadaddr)->sin_addr;
				}

				//Ŀ�ĵ�ַ��255��
				else
				{
					//�洢�����ַ
					//���������ֽ���Ϊ�͵�ַ�洢��λ���ߵ�ַ�洢��λ�����Զ�IPV4 32λ����IP��ַ����Ҫ���д�С��ת����
					//htonl() htons()s
					//ip��ַΪ       10.0.destid.1
					dest.s_addr = (0x01<<24)+ (destid<<8) + (0x0a);					
					daddr.sin_addr.s_addr = (dest.s_addr);					
					//printf("%08x, %08x, %08x\n",inet_addr("10.0.1.3"),dest.s_addr,htonl(dest.s_addr));
				}
#endif
			}


			//��sin_zero�������0
			memset(daddr.sin_zero, 0, sizeof(daddr.sin_zero));

			if (sendto(sock, (char *) msgitem.data, msglen, 0, (struct sockaddr *) &daddr,
					(socklen_t) sizeof(daddr)) < 0)
			{
				
				LOG_MSG(ERR_LEVEL,"wlan sendto err\n");
			}
			
		}else
		{

			continue;
		}
	}

}

