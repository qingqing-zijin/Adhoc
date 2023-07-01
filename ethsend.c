/**
 * @fuction��	���̣߳������߷��Ͷ����л�ȡ���ͱ��ģ�ͨ��RAW_SOCK ���͵����߶ˡ�
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

void * send_to_lan_routine(void * args);
void lan_query_and_send(int sockfd);
extern struct ifaddrs g_wlanif;
extern long long glInTime,glOutTime;
extern long long gwlInTime,gwlOutTime;

pthread_t create_lan_sendto_thread()
{
	int ret;
	pthread_t sendto_lan_thread_id;
	ret = pthread_create(&sendto_lan_thread_id,NULL,&send_to_lan_routine,NULL);
	if(ret != 0)
	{
		LOG_MSG(ERR_LEVEL,"create sendto_lan_thread_routine ");
	}
	//LOG_MSG(INFO_LEVEL,"Create lan send thread success\n");
	return  sendto_lan_thread_id;
}

/**
 * ���������߶˷��͵�RAWsocket��ѭ����ȡ�����ڲ�������Ϣ�������䷢�ͳ�ȥ��
 */
void *send_to_lan_routine(void * args)
{
	int s;
	const int on = 1;
	char* interface = LAN_INTERFACE;
	struct ifreq ifr;
	/*
	ָ������IPPROTO_RAW:
	��ʱ�����socketֻ����������IP���������ܽ����κε����ݡ����͵�������Ҫ�Լ����IP��ͷ�������Լ�����У��͡�
	*/
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)		//��������IPЭ����׽���
	{
		LOG_MSG(ERR_LEVEL,"lan send create sock ");
		exit(-1);
	}

	// Use ioctl() to look up interface index which we will use to
	// bind socket descriptor sd to specified interface with setsockopt() since
	// none of the other arguments of sendto() specify which interface to use.
   
	// Set flag so socket expects us to provide IPv4 header.
	//��װIPͷ��
	
	/*
	����Ҫ��д�Լ���IP���ݰ��ײ�ʱ��������ԭʼ�׽����������׽���ѡ��IP_HDRINCL.
	�ڲ��������ѡ�������£�IPЭ���Զ����IP���ݰ����ײ���

����int on = 1��if��setsockopt��sockfd�� IPPROTO_IP�� IP_HDRINCL�� &on�� sizeof��on���� < 0��

����{ fprintf��stderr�� "setsockopt IP_HDRINCL ERROR�� /n"����exit��1����}

	*/
	
	if (setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"lan setsockopt() failed to set IP_HDRINCL \n");
		exit(-1);
	}

     /*
      *
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface);
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
	{
		LOG_MSG(ERR_LEVEL,"lan ioctl() failed to find interface ");
		return (-1);
	}
    
	// Bind socket to interface index.
	 if (setsockopt (s, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof (ifr)) < 0) {
	   LOG_MSG(ERR_LEVEL,"setsockopt() failed to bind to interface\n");
	   exit (EXIT_FAILURE);
	 }
	 */
	 
    //��LEVELΪSOL_SOCKET��SO_BROADCAST�������׽��������óɶԹ㲥��Ϣ���з���
    //�����͹㲥��Ϣ
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"lan setsockopt() failed to set SO_BROADCAST\n");
		exit(-1);
	}
	lan_query_and_send(s);
}

void lan_query_and_send(int sock)
{
	struct sockaddr_in daddr;
	int msgqid;
	int msglen=0;
	struct q_item msgitem;
	struct ip iphdr;

	//��ȡlan_send_cq��Ϣ����
	msgqid = msgget(LAN_SEND_CQ,IPC_EXCL);
	if(msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL,"LAN_SEND_CQ not found\n");
		pthread_exit(1);
	}
	//LOG_MSG(INFO_LEVEL,"lan send cq id = %d\n",msgqid);

	while(1)
	{
		
		memset(&msgitem,0,sizeof(msgitem));

		/*
		����: int msgrcv(int msqid, void  *ptr, size_t  length, long  type, int  flag);
		type: �����Ӷ����з���������Ϣ��
		=0 ������Ϣ�����е�һ����Ϣ
		>0 ������Ϣ�����е���mtype ���͵ĵ�һ����Ϣ��
		<0 ����mtype<=type ����ֵ��Сֵ�ĵ�һ����Ϣ��
		msgflg Ϊ����ʾ������ʽ������IPC_NOWAIT ��ʾ��������ʽ
		msgflg:ȡֵ���£�
		IPC_NOWAIT ,������
		IPC_NOERROR ������Ϣ���ȳ�������������msgsz����ض���Ϣ��������
		*/
		msglen = msgrcv(msgqid,&msgitem,MAX_MSG_LEN,0,MSG_NOERROR);//IPC_NOWAIT|
		if(msglen > 0)
		{
			memset (&daddr, 0, sizeof (struct sockaddr_in));

			//��ȡIPͷ
			char* raw_frame = msgitem.data;

			memcpy(&iphdr, (char*)msgitem.data, sizeof(iphdr));
			
			daddr.sin_family = AF_INET;
			daddr.sin_addr.s_addr = iphdr.ip_dst.s_addr;
            
			//LOG_MSG(INFO_LEVEL,"\n============================================Send to LAN : %s,len = %d \n",inet_ntoa(iphdr.ip_dst),msglen);
			//hexprint(msgitem.data,msglen);

			/*
				 * ��ȡUDPĿ�Ķ˿���Ϣ
				 * */
			struct udphdr udphdr;
			char 	*tranhead;
			int 	destport;
			int		srcPort;
			int  	user_data_len =0;
			char*	user_data = NULL;

			char ip_src[16], ip_dst[16];
			char user_pk_hd[4]={0,};

			int ip_len = ntohs(iphdr.ip_len); // get the real len from ip header

			//����inet_ntop�������ֽڵ�ַת��Ϊ�����ֽ�
			inet_ntop(AF_INET, &(iphdr.ip_src), ip_src, INET_ADDRSTRLEN);
			inet_ntop(AF_INET, &(iphdr.ip_dst), ip_dst, INET_ADDRSTRLEN);

			tranhead = raw_frame + iphdr.ip_hl * 4; //to the udp or tcp head;

			memcpy(&udphdr, tranhead, sizeof(udphdr)); //tcp header 20B and upd header 8B, first 4 Byte are the port info.
			destport = ntohs(udphdr.dest);
			srcPort	= ntohs(udphdr.source);
	/**/
			user_data_len = ntohs(udphdr.len)-8;
			user_data = tranhead +8;

			user_pk_hd[0] = user_data[0];
			user_pk_hd[1] = user_data[1];
			user_pk_hd[2] = user_data[2];

			LOG_MSG(INFO_LEVEL,	"============================================LAN Recv %s ,srcIP %s: %d , destIP %s: %d, IP ,IP_head_len %d, total_bytes %d, user_data bytes %d ,proto = %s\n",
					user_pk_hd,\
					ip_src,\
					srcPort,\
					ip_dst,\
					destport,\
					iphdr.ip_hl ,\
					ip_len, \
					user_data_len , \
					iphdr.ip_p==IPPROTO_TCP? "TCP" :(iphdr.ip_p==IPPROTO_UDP? "UDP": "Other PPROTO"));

			/*
			ָ��һָ��Ŀ�ĵط�������
			int sendto ( socket s , const void * msg, int len, unsigned int flags, const
			struct sockaddr * to , int tolen ) ;
			����to Ϊ�����ַ
			*/
			if (sendto(sock, (char *) msgitem.data, msglen, 0, (struct sockaddr *) &daddr,
					(socklen_t) sizeof(daddr)) < 0)
			{
				LOG_MSG(ERR_LEVEL,"packet send to lan error,msglen=%d, proto =%d, from %08x to %08x\n",
                    msglen, iphdr.ip_p, iphdr.ip_src.s_addr, iphdr.ip_dst.s_addr);
				
			}	
			
		
		}else
		{
			
            LOG_MSG(ERR_LEVEL,"LAN_SEND_CQ rcv %d bytes\n",msglen);
			continue;
		}
	}

}

