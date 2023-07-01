/**
 * @fuction：	创建从无线接收udp 报文的线程，将接收报文加入到接收队列，供roter模块使用。
 * @author：  	lsp
 * @email: 		siping@iscas.ac.cn
 * @date：		2015-2-3
 * @version：	v0.0.1
 */
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <net/if.h>           // struct ifreq
#include <sys/ioctl.h>        // macro ioctl is defined
#include <ifaddrs.h>

#include "pthread.h"
#include "main.h"
#include "aux_func.h"
#include "queues.h"

/*
 * 接收buffer长度
 */
#define MAX_RECV_BUFF_SIZE 		2048
/**
 * WLAN监听端口
 */

#define BACKLOG				10

extern struct in_addr gwl;
extern struct in_addr gl;
extern in_addr_t self;

extern long long glInTime,glOutTime;
extern long long gwlInTime,gwlOutTime;

int wlan_filter(struct in_addr srcip);
void * wlan_recv_routine(void *args);

/*
 * 创建无线络接收线程，接收从无线网卡接收的udp报文
 * return pthread_t 创建接收线程id
 */
pthread_t create_wlan_recv_thread()
{
	int ret ;
	pthread_t tid;
	
	ret = pthread_create( &tid, NULL, &wlan_recv_routine, NULL);
	if(ret != 0)
	{
		LOG_MSG(ERR_LEVEL,"create_wlan_recv_thread\n");
		return ERROR;
	}
	//LOG_MSG(INFO_LEVEL,"Create wlan recv thread success\n ");


	return tid;
}

/**
 * 无线接收例程处理函数，简单接收打印接收数据，以后将接收到的报文存入队列
 */


void * wlan_recv_routine(void *args)
{
	int msgqid;
	struct sockaddr_in server_sockaddr,client_sockaddr;
	int sin_size,recvbytes;
	int sockfd,client_fd;
	char buf[MAX_RECV_BUFF_SIZE];
	
	//绑定到指定的网卡符号
	char* interface = WLAN_INTERFACE;
	struct ifreq ifr;	
	struct q_item qitem;
	struct msqid_ds ds;
	
	bzero(&server_sockaddr,sizeof(server_sockaddr));

	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = htons(WLAN_LISTEN_PORT);
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if((sockfd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == -1)
	{
		LOG_MSG(ERR_LEVEL,"wlan create sock err\n");
		pthread_exit(1);
	}
	// Use ioctl() to look up interface index which we will use to
	// bind socket descriptor sd to specified interface with setsockopt() since
	// none of the other arguments of sendto() specify which interface to use.
	memset(&ifr, 0, sizeof(ifr));

	/*
	 * 将sockfd绑定指定的网卡interface
	 * */
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface);
	if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) { //SIOCGIFINDEX
		LOG_MSG(ERR_LEVEL,"wlan ioctl() failed to find interface \n");
		exit (-1);
	}

	if(bind(sockfd,(struct sockaddr*)&server_sockaddr,sizeof(server_sockaddr))==-1)
	{
		LOG_MSG(ERR_LEVEL,"wlan bind err\n");
		pthread_exit(1);
	}

	msgqid = msgget(RECV_CQ,IPC_EXCL);
	if(msgqid < 0)
	{
		LOG_MSG(ERR_LEVEL,"get wlan recv cq");
		pthread_exit(1);
	}
	//LOG_MSG(INFO_LEVEL,"wlan recv cq id = %d\n",msgqid);

	
	//LOG_MSG(INFO_LEVEL,"wlan create recv sock success,start listening...\n");
	while (1)
	{
		sin_size = sizeof(client_sockaddr);

		//阻塞方式调用recvfrom,并接受client_sockaddr修改，返回client_sockaddr信息
		recvbytes = recvfrom(sockfd, buf, MAX_RECV_BUFF_SIZE, 0,
				(struct sockaddr*) &client_sockaddr, &sin_size);
		
		
		if (recvbytes > 0)
		{
			//if(recvbytes == 98)
			//{
			//	gwlInTime = sysTimeGet();
			//}
			
			if(wlan_filter(client_sockaddr.sin_addr) < 0) //filter recv broadcast from myself
			{
				
				continue;
			}
			
			//LOG_MSG(INFO_LEVEL,"\n============================================WLAN Recv from = %s, len = %d\n\n", inet_ntoa(client_sockaddr.sin_addr),recvbytes);
			//hexprint(buf, recvbytes);
				
			//设置消息类型
			qitem.type = WLAN_DATA;

			//拷贝接收内容
			memcpy(&qitem.data,buf,recvbytes);

			//发送至消息队列
			if( msgsnd(msgqid,&qitem,recvbytes,IPC_NOWAIT) < 0)
			{
				if(0 == msgctl(msgqid,IPC_STAT,&ds))
				{
					LOG_MSG(INFO_LEVEL,"current size = %ld,max size = %ld\n",ds.__msg_cbytes,ds.msg_qbytes);
				}
				LOG_MSG(ERR_LEVEL,"wlan recv msgsnd ");
				break;
			}
			
		} else
		{
            LOG_MSG(ERR_LEVEL, "wlan rcv %d bytes\n",recvbytes);
			continue;
		}
	}
	close(sockfd);
	close(client_fd);
}

/*
 * 过滤wlan 广播被自己接收
 * 如果不需要处理返回-1
 * 否则返回1;
 */
int wlan_filter(struct in_addr srcip)
{
	//struct in_addr gwl = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_addr)->sin_addr;	
	
	//printf("src_ip = %x,gwl.s_addr = %x\n",srcip.s_addr, gwl.s_addr);

	//接收的IP报文源地址与本地gwl IP相同，则认为是自己发送的数据报文，丢弃
#ifndef UART_Wireless_INTERFACE

	if(gwl.s_addr == srcip.s_addr || self == srcip.s_addr)
	{
			return -1;
	}
#endif

	return 1;
}
