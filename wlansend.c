/**
 * @fuction：	起线程，从无线发送优先队列中获取发送报文，通过udp广播该消息。
 * @author：  	lsp
 * @email: 		siping@iscas.ac.cn
 * @date：		2015-2-4
 * @version：	v0.0.1
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
 * 创建往无线端发送的udp socket，获取优先队列内部的新消息，并将其发送出去。
 */
void *send_to_wlan_routine(void * args)
{
	int s;

	const int on = 1;
	char* interface = WLAN_INTERFACE;
	struct ifreq ifr;

	//创建收发UDP数据报套接字
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		LOG_MSG(ERR_LEVEL,"error:");
		exit(-1);
	}

	//绑定socket至指定网络接口
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

	//没有调用IP_HDRINCL

	/*
	当需要编写自己的IP数据包首部时，可以在原始套接字上设置套接字选项IP_HDRINCL.
	在不设置这个选项的情况下，IP协议自动填充IP数据包的首部。

　　int on = 1；if（setsockopt（sockfd， IPPROTO_IP， IP_HDRINCL， &on， sizeof（on）） < 0）

　　{ fprintf（stderr， "setsockopt IP_HDRINCL ERROR！ /n"）；exit（1）；}

	*/	

    //如LEVEL为SOL_SOCKET，SO_BROADCAST，表明套接字已配置成对广播消息进行发送
    //允许发送广播消息

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

			//封装网络地址，初始化协议类型、目的地址和目的端口号. daddr为struct sockaddr_in
			daddr.sin_family = AF_INET;
			daddr.sin_port = htons(WLAN_LISTEN_PORT);
			
			if(msgitem.data[0] == LSA_PK_TYPE )
			{
				//LSA链路状态以255广播形式发送
#ifndef UART_Wireless_INTERFACE
				daddr.sin_addr = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_ifu.ifu_broadaddr)->sin_addr;
			}
			else
			{

				//其它Wlan RREQ以广播形式发送，MPR节点转发或为目的节点处理RREQ,其它节点只收不处理
				destid = ((unsigned int)msgitem.data[4]) + ((unsigned int)msgitem.data[5]<<8);

				//目的地址为255，广播
				if(destid == 0xffff)
				{
					daddr.sin_addr = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_ifu.ifu_broadaddr)->sin_addr;
				}

				//目的地址非255，
				else
				{
					//存储网络地址
					//由于网络字节序为低地址存储高位，高地址存储地位，所以对IPV4 32位主机IP地址，需要进行大小端转换。
					//htonl() htons()s
					//ip地址为       10.0.destid.1
					dest.s_addr = (0x01<<24)+ (destid<<8) + (0x0a);					
					daddr.sin_addr.s_addr = (dest.s_addr);					
					//printf("%08x, %08x, %08x\n",inet_addr("10.0.1.3"),dest.s_addr,htonl(dest.s_addr));
				}
#endif
			}


			//对sin_zero进行填充0
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

