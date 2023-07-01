/**
 * @fuction：	起线程，从有线发送队列中获取发送报文，通过RAW_SOCK 发送到有线端。
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
 * 创建往有线端发送的RAWsocket，循环获取队列内部的新消息，并将其发送出去。
 */
void *send_to_lan_routine(void * args)
{
	int s;
	const int on = 1;
	char* interface = LAN_INTERFACE;
	struct ifreq ifr;
	/*
	指定参数IPPROTO_RAW:
	这时候，这个socket只能用来发送IP包，而不能接收任何的数据。发送的数据需要自己填充IP包头，并且自己计算校验和。
	*/
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)		//创建基于IP协议的套接字
	{
		LOG_MSG(ERR_LEVEL,"lan send create sock ");
		exit(-1);
	}

	// Use ioctl() to look up interface index which we will use to
	// bind socket descriptor sd to specified interface with setsockopt() since
	// none of the other arguments of sendto() specify which interface to use.
   
	// Set flag so socket expects us to provide IPv4 header.
	//包装IP头部
	
	/*
	当需要编写自己的IP数据包首部时，可以在原始套接字上设置套接字选项IP_HDRINCL.
	在不设置这个选项的情况下，IP协议自动填充IP数据包的首部。

　　int on = 1；if（setsockopt（sockfd， IPPROTO_IP， IP_HDRINCL， &on， sizeof（on）） < 0）

　　{ fprintf（stderr， "setsockopt IP_HDRINCL ERROR！ /n"）；exit（1）；}

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
	 
    //如LEVEL为SOL_SOCKET，SO_BROADCAST，表明套接字已配置成对广播消息进行发送
    //允许发送广播消息
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

	//获取lan_send_cq消息队列
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
		声明: int msgrcv(int msqid, void  *ptr, size_t  length, long  type, int  flag);
		type: 决定从队列中返回哪条消息：
		=0 返回消息队列中第一条消息
		>0 返回消息队列中等于mtype 类型的第一条消息。
		<0 返回mtype<=type 绝对值最小值的第一条消息。
		msgflg 为０表示阻塞方式，设置IPC_NOWAIT 表示非阻塞方式
		msgflg:取值如下：
		IPC_NOWAIT ,不阻塞
		IPC_NOERROR ，若信息长度超过第三个参数msgsz，则截断信息而不报错。
		*/
		msglen = msgrcv(msgqid,&msgitem,MAX_MSG_LEN,0,MSG_NOERROR);//IPC_NOWAIT|
		if(msglen > 0)
		{
			memset (&daddr, 0, sizeof (struct sockaddr_in));

			//获取IP头
			char* raw_frame = msgitem.data;

			memcpy(&iphdr, (char*)msgitem.data, sizeof(iphdr));
			
			daddr.sin_family = AF_INET;
			daddr.sin_addr.s_addr = iphdr.ip_dst.s_addr;
            
			//LOG_MSG(INFO_LEVEL,"\n============================================Send to LAN : %s,len = %d \n",inet_ntoa(iphdr.ip_dst),msglen);
			//hexprint(msgitem.data,msglen);

			/*
				 * 获取UDP目的端口信息
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

			//调用inet_ntop将网络字节地址转换为主机字节
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
			指向一指定目的地发送数据
			int sendto ( socket s , const void * msg, int len, unsigned int flags, const
			struct sockaddr * to , int tolen ) ;
			参数to 为网络地址
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

