/**
 * @fuction：	系统总入口，初始化系统各个模块
 * @author：  	lsp
 * @email: 		siping@iscas.ac.cn
 * @date：		2015-2-2
 * @version：	v0.0.1
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <net/if.h>
#include <time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include "main.h"
#include <ifaddrs.h>
#include "queues.h"
#include "Routing.h"
#include "aux_func.h"
#include "cmdline.h"
#include "serial.h"
#include "radio.h"
#include "gpio.h"
#include "auv.h"
#include "Congitive.h"
#include "mac.h"
#include "E01.h"

#include "uartWirelessRecv.h"
#include "uartWirelessSend.h"

pthread_t L_e32_recv_thread_id;

pthread_t M_e32_recv_thread_id;
pthread_t uartWireless_send_thread_id;

int 	L_e32_uart_fd = -1;
int 	L_e32_aux_gpio_input_fd =-1;

int 	M_e32_uart_fd = -1;
int 	M_e32_aux_gpio_input_fd =-1;

int 	e43_aux_gpio_input_fd =-1;

int 	radio_dat_mode = DTU_MOD;

unsigned short 	self_node_id = 0;

pthread_t lan_recv_thread_id;
pthread_t lan_sendto_thread_id;
pthread_t route_process_thread_id;

//WJW
pthread_t mac_process_thread_id;

struct ifaddrs g_lanif;
struct in_addr gl;		//lan ip
struct in_addr glbc; 	//lan broadcast ip
struct in_addr glnm; 	//lan netmask ip


in_addr_t self;			//127.0.0.1

//long long glInTime,glOutTime;
//long long gwlInTime,gwlOutTime;

struct timeval starttime;

extern pthread_t create_lan_recv_thread();
extern pthread_t create_lan_sendto_thread();
extern pthread_t create_route_thread();
extern int create_queues();

int  init();
int  create_threads();
void getAllIfconfig();

char Flog_name[32];
int BG_Flag = 0;

int global_running_state = NET_NORMAL_RUNNING;

//u8  global_freq_config_state = FREQ_CONFIG_INIT;

u8 	global_e43_existed = TRUE;

/*
 * 信令电台L_E32当前频率
 * */
Radio_CHL_t		g_l_e32_ch = CHANL14;	//[CH0~CH15]

/*
 * 业务电台M_E32当前频率
 * */
Radio_CHL_t		g_m_e32_ch = CHANL1;  //[CH0~CH15]

/*
 * 业务电M_E32当前速率
 * */
u8				g_m_e32_speed = AIR_SPEEDTYPE_19K2	;//SPD_E01_250K;//SPD_E32_19K2;

/*
 * 业务电台M_E32或E01当前速率
 * */
RADIO_SPEED_t	g_radio_speed = SPD_E01_250K;//SPD_E01_250K;//SPD_E32_19K2;

/*
 * 业务电台M_E32或E01升级模式开关量：1-升级 0-正常
 * */
u8				g_UPG_on = FALSE;


/*
 * @function sys_start_init
 * @desc:
 * 		系统初始化，包括初始化打印输出、网卡信息获取等
 * @return :
 * 		TRUE: 成功
 * 		FALSE：失败
 * */
int sys_start_init(int argc, char** argvs)
{
	int num;
	char buff[64];
	FILE *f_ptr = NULL;
	FILE *Flog_ptr = NULL;

	if (argc > 1) {
		if (strcmp(argvs[1], "-bg") == 0) {
			BG_Flag = 1;
		}

		else if (strcmp(argvs[1], "-e43") == 0) {
			global_e43_existed  = TRUE;
		}
	}

	// 1. 打开路由配置文件，获取路由参数
	f_ptr = fopen("routeCfg.ini", "r");
	if (f_ptr == NULL)
	{
		f_ptr = fopen("routeCfg.ini", "w");
		if (f_ptr == NULL) {

			return 0;
		}
		//如果路由配置文件不存在，则将默认的参数写入新建的配置文件中
		fprintf(f_ptr, "%d:Log_Level\n", Log_Level);
		fflush(f_ptr);
		fprintf(f_ptr, "%d:RADIUS\n", RADIUS);
		fflush(f_ptr);
		fprintf(f_ptr, "%d:CLUSTER_TO_ZRP_NUM\n", CLUSTER_TO_ZRP_NUM);
		fflush(f_ptr);
		fprintf(f_ptr, "%d:ZRP_TO_CLUSTER_NUM\n", ZRP_TO_CLUSTER_NUM);
		fflush(f_ptr);
		fprintf(f_ptr, "%d:RETRY_TIMES\n", RETRY_TIMES);
		fflush(f_ptr);
		fprintf(f_ptr, "%d:PERIOD_LSA\n", PERIOD_LSA);
		fflush(f_ptr);
		fprintf(f_ptr, "%d:IERP_RREQ_TIMEOUT\n", IERP_RREQ_TIMEOUT);
		fflush(f_ptr);

		fclose(f_ptr);

	} else {
		//W：如果配置文件在，则从配置文件中读取各项参数、包括调试开关、域内路由半径等
		fgets(buff, 64, f_ptr);
		Log_Level = atoi(buff);

		fgets(buff, 64, f_ptr);
		RADIUS = atoi(buff);

		fgets(buff, 64, f_ptr);
		CLUSTER_TO_ZRP_NUM = atoi(buff);

		fgets(buff, 64, f_ptr);
		ZRP_TO_CLUSTER_NUM = atoi(buff);

		fgets(buff, 64, f_ptr);
		RETRY_TIMES = atoi(buff);

		fgets(buff, 64, f_ptr);
		PERIOD_LSA = atoi(buff);

		fgets(buff, 64, f_ptr);
		IERP_RREQ_TIMEOUT = atoi(buff);

		fclose(f_ptr);

	}

	//
	// 2.打开LOG配置文件
	Flog_ptr = fopen("logIndex.txt", "r");
	if (Flog_ptr == NULL) {
		Flog_ptr = fopen("logIndex.txt", "w");
		if (Flog_ptr == NULL) {
			return 0;
		}
		num = 0;
		fputs("0", Flog_ptr);
		fflush(Flog_ptr);
		fclose(Flog_ptr);

	}
	else {
		fgets(buff, 8, Flog_ptr);
		num = (atoi(buff) + 1) & 0xf;		// 最多保存16个文件
		fclose(Flog_ptr);

		Flog_ptr = fopen("logIndex.txt", "w");
		if (Flog_ptr == NULL) {
			return 0;
		}

		fprintf(Flog_ptr, "%d", num);
		fflush(Flog_ptr);
		fclose(Flog_ptr);
	}

	// 3. 打开 log 输出文件
	sprintf(Flog_name, "log%d.txt", num);
	Flog_ptr = fopen(Flog_name, "w");
	if (Flog_ptr == NULL) {

		return 0;
	}
	fclose(Flog_ptr);

	init();

	return TRUE;
}

/*
 * @function:
 * 	  main
 * @desc:
 * 	  程序入口
 * */
int main(int argc, char** argvs)
{
	int ret=0;
	//获取本机网卡信息，初始化g_lanif网卡结构变量
	sys_start_init(argc,  argvs);

	self_node_id =getSelfID() ;

	/*
	 * 配置中、低速E32、E43无线电台的GPIO管脚
	 * */
	ret=radios_gpio_init();
	if(ret<0)
	{
		LOG_MSG(ERR_LEVEL, "radios_gpio_init error!\n");
		return -1;
	}
	/*
	 * 配置业务数传无线模块工作参数，包括串口速率、空口速率及工作模式
	 * */
	/********************************************************
	 *				E32业务模块初始化，用于中速业务数传
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config Radio Medium Speed E32...!\n");
	if((M_e32_uart_fd =config_E32_radio_params(COM1,M_e32_aux_gpio_input_fd, g_m_e32_ch, g_m_e32_speed,POWER_21dBm_ON ))<0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to config_E32(Medium Speed )_radio_params!\n");
		return -1;
	}
	/********************************************************
	 *				E32信令模块初始化，用于低速信令数传
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config Radio Low Speed E32...!\n");
	if((L_e32_uart_fd =config_E32_radio_params(COM7,L_e32_aux_gpio_input_fd,g_l_e32_ch, AIR_SPEEDTYPE_2K4,POWER_21dBm_ON ))<0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to config_E32(Low Speed )_radio_params!\n");
		return -1;
	}

//	return 0;		//test E32
	/********************************************************
	 *				E43模块初始化,用于频谱感知
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config Radio E43...!\n");

	if(global_e43_existed == TRUE && config_E43_radio_rssi_mode_test()<0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to config_E43_radio_rssi_mode_test!\n");
		return -1;
	}
//	return 0;		//test E43

	/********************************************************
	 *				E01+模块初始化，用于高速业务数传
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config High Speed Radio E01...!\n");
	config_E01_radio_params();

	/********************************************************
	 *
	 *******************************************************/
	nsv_network_init();
	/*
	 * 创建各种消息队列
	 * */
	create_queues();
	/*
	 * 创建各种线程
	 * */
	create_threads();
	return 0;
}

/**
 * 输出打印网卡信息，IPv4 IP地址、地址掩码和广播地址
 */
void printIfInfos(const struct ifaddrs *ifinfos) {
	void* tmpAddrPtr;

	/*
	 * 获取IP_V4 IP地址，转换后的主机地址保存在addressBuffer中
	 * */
	char addressBuffer[INET_ADDRSTRLEN];
	LOG_MSG(INFO_LEVEL, "\n%s: config\n", ifinfos->ifa_name);
	tmpAddrPtr = &((struct sockaddr_in*) (ifinfos)->ifa_addr)->sin_addr;

	//调用inet_ntop将网络字节地址转换为主机字节
	inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
	LOG_MSG(INFO_LEVEL, "address  : %s\n", addressBuffer);

	/*
	 *获取IP_V4 IP地址掩码，转换后的主机ip掩码地址保存在addressBuffer中
	 * */
	tmpAddrPtr = &((struct sockaddr_in*) (ifinfos)->ifa_netmask)->sin_addr;


	inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

	LOG_MSG(INFO_LEVEL, "net mask : %s\n", addressBuffer);

	/*
	 *获取IP_V4 IP广播地址，转换后的主机ip广播地址保存在addressBuffer中
	 * */
	tmpAddrPtr =&((struct sockaddr_in*) (ifinfos)->ifa_ifu.ifu_broadaddr)->sin_addr;
	inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
	LOG_MSG(INFO_LEVEL, "broadaddr: %s\n\n", addressBuffer);
}

/**
 * 初始化系统参数
 * return if success return 1;else return 0;
 */
int init()
{

	gettimeofday(&starttime, NULL);

	LOG_MSG(INFO_LEVEL, "\n************system start at %d:%d:%d.%03d\n",
			starttime.tv_sec / 3600, (starttime.tv_sec / 60) % 60,
			starttime.tv_sec % 60, starttime.tv_usec / 1000);

	/*
	 * 获取本机（host）的所有网卡信息，诸如eth0\eth1等网卡信息
	 * */
	getAllIfconfig();

	//打印本地lan网卡信息
	printIfInfos(&g_lanif);


#ifndef UART_Wireless_INTERFACE

	//打印本地wlan网卡信息
	printIfInfos(&g_wlanif);
#endif

	return 1;
}

/**
 * 获取到所有网卡的信息，只取出WLAN_INTREFACE和LAN_INTERFACE两个网卡的信息，
 * 并将信息存入到g_wlaninfos 和 g_laninfos.
 */
#ifndef UART_Wireless_INTERFACE

struct sockaddr_in wlan_ina;
struct sockaddr_in wlan_ina_bdr;
struct sockaddr_in wlan_ina_mask;
const char wlan_name[6] = "wlan0";

#endif

/*
 * NoteAuthor: wjw
 * function: getAllIfconfig
 * desc:	函数调用getifaddrs获取本机所有网卡信息，
 * 			以太网卡ethx结果保存在全局变量g_lanif中
 * 			如有由WIFI网卡，则保留在全局变量g_wlanif中
 * */
void getAllIfconfig() {
	struct ifaddrs *ifaddrStruct = NULL;
	struct ifaddrs *ifa = NULL;
	int m1 = 0, m2 = 0;

	/*
	 函数getifaddrs（int getifaddrs (struct ifaddrs **__ifap)）获取本地网络接口信息，
	 将之存储于链表中，链表头结点指针存储于__ifap中带回，
	 函数执行成功返回0，失败返回-1，且为errno赋值
	 */
	if (0 != getifaddrs(&ifaddrStruct)) {
		LOG_MSG(ERR_LEVEL, "get ifaddrs err\n");
		return;
	}

	for (ifa = ifaddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr) {
			if (ifa->ifa_addr->sa_family == AF_INET) {
#ifndef UART_Wireless_INTERFACE
				if(strcmp(WLAN_INTERFACE,ifa->ifa_name) == 0)
				{
					//获取无线网卡wlan0信息
					g_wlanif.ifa_name = ifa->ifa_name;//Name of interface
					g_wlanif.ifa_addr = ifa->ifa_addr; /* Address of interface */
					g_wlanif.ifa_netmask = ifa->ifa_netmask; /* Netmask of interface */
					g_wlanif.ifa_ifu.ifu_broadaddr = ifa->ifa_ifu.ifu_broadaddr; /* Broadcast address of interface */
					m1= 1;
				}
				else if(strcmp(LAN_INTERFACE,ifa->ifa_name) == 0)
				{
					//获取本地网卡"eth0"
					g_lanif.ifa_name = ifa->ifa_name;
					g_lanif.ifa_addr = ifa->ifa_addr;
					g_lanif.ifa_netmask = ifa->ifa_netmask;
					g_lanif.ifa_ifu.ifu_broadaddr = ifa->ifa_ifu.ifu_broadaddr;
					m2 = 1;
				}
#else
				if (strcmp(LAN_INTERFACE, ifa->ifa_name) == 0) {
					//获取本地网卡"eth0"
					g_lanif.ifa_name = ifa->ifa_name;
					g_lanif.ifa_addr = ifa->ifa_addr;
					g_lanif.ifa_netmask = ifa->ifa_netmask;
					g_lanif.ifa_ifu.ifu_broadaddr = ifa->ifa_ifu.ifu_broadaddr;
					m2 = 1;
					break;
				}
#endif
			}
		}
	}

#ifndef UART_Wireless_INTERFACE

	bzero((char*) &wlan_ina, sizeof(wlan_ina));
	wlan_ina.sin_family = AF_INET;
	wlan_ina.sin_addr.s_addr = inet_addr("10.0.2.1");
	//sockaddr_in结构体中sin_port和 sin_addr必须为NBO顺序
	wlan_ina.sin_port = htons(23);

	//获取无线网卡wlan0信息
	g_wlanif.ifa_name = wlan_name;
	g_wlanif.ifa_next = NULL;
	g_wlanif.ifa_data = NULL;
	g_wlanif.ifa_addr = (struct sockaddr*) &wlan_ina;	//Address of interface

	//* 子网掩码
	bzero((char*) &wlan_ina_mask, sizeof(wlan_ina_mask));
	wlan_ina_mask.sin_family = AF_INET;
	wlan_ina_mask.sin_port = htons(23);
	wlan_ina_mask.sin_addr.s_addr = inet_addr("255.0.0.0");
	g_wlanif.ifa_netmask = (struct sockaddr*) &wlan_ina_mask;// Netmask of interface

	//* 广播地址
	bzero((char*) &wlan_ina_bdr, sizeof(wlan_ina_bdr));
	wlan_ina_bdr.sin_family = AF_INET;
	wlan_ina_bdr.sin_port = htons(23);
	wlan_ina_bdr.sin_addr.s_addr = inet_addr("10.0.2.255");
	g_wlanif.ifa_ifu.ifu_broadaddr = (struct sockaddr*) &wlan_ina_bdr; // Broadcast address of interface
	m1 = 1;
#endif

	if (ifaddrStruct != NULL)
		freeifaddrs(ifaddrStruct);

#ifndef UART_Wireless_INTERFACE
	if (m1 == 0)
	{
		LOG_MSG(ERR_LEVEL, "no valid WLAN_INTERFACE\n");
		exit(-1);
	}

	else if (m2 == 0) {
		LOG_MSG(ERR_LEVEL, "no valid LAN_INTERFACE\n");
		exit(-1);
	}
#else
	 if (m2 == 0)
	 {
		LOG_MSG(ERR_LEVEL, "no valid LAN_INTERFACE\n");
		exit(-1);
	 }
#endif

	//依次对wlan ip、local ip 、广播ip、地址掩码进行赋值
	/**/
#ifndef UART_Wireless_INTERFACE
	gwl 	=(struct in_addr) ((struct sockaddr_in*) (&g_wlanif)->ifa_addr)->sin_addr;
	gwlbc 	=(struct in_addr) ((struct sockaddr_in*) (&g_wlanif)->ifa_ifu.ifu_broadaddr)->sin_addr;
	gwlnm 	=(struct in_addr) ((struct sockaddr_in*) (&g_wlanif)->ifa_netmask)->sin_addr;
#endif

	gl 		=(struct in_addr) ((struct sockaddr_in*) (&g_lanif)->ifa_addr)->sin_addr;
	glbc 	=(struct in_addr) ((struct sockaddr_in*) (&g_lanif)->ifa_ifu.ifu_broadaddr)->sin_addr;
	glnm 	=(struct in_addr) ((struct sockaddr_in*) (&g_lanif)->ifa_netmask)->sin_addr;

	self = inet_addr("127.0.0.1");
	return;
}
/**
 *创建系统 需要的线程
 */
int create_threads()
{
	 /*
	 *Ad Hoc 框架设计：
	 *
	 * 	Cmd_Line-------->PC
	 *			 	 	 |
	 *					\|/
	 *				LAN_RECV_CQ
	 *					 |
	 *					\|/
	 *无线空口<---MAC----->ROUTE<--------->LAN_SEND_CQ---> PC
	 *
	 *
	 * */
	pthread_t cmd_line_thread_id;

	/*
	 *创建从无线空口接收信令报文线程
	 * */
	L_e32_recv_thread_id = create_L_e32_uartWireless_recv_thread();

	/*
	 *创建从无线空口接收中速E32数据报文线程
	 * */
	M_e32_recv_thread_id = create_M_e32_uartWireless_recv_thread();

	/*
	 * 创建MAC控制访问线程
	 * */
	mac_process_thread_id 	= create_mac_thread();


	/*
	 * 创建网络频谱感知定时器，绑定定时器超时回调函数
	 * */
	NetCong_Module_Init();

	//负责从lan消息队列中读消息，消息为IP数据报。为路由层服务
	/*
	 * 创建负责处理从无线接收到IP报文的线程。从无线接收到目的地址为本节点的IP报文均放入LAN_SEND_CQ消息队列中。
	 * */
	lan_sendto_thread_id 	= create_lan_sendto_thread();

	/*
	 *创建运行Ad Hoc路由协议(ROUTE)线程。
	 *
	 * */
	route_process_thread_id = create_route_thread();

	// MAC层处理信道接入
	//	create_uartWireless_sendTimer_thread();


	/*
	 *创建从PC接收、处理UDP IP报文线程,接收到的有效IP报文添加至ROUTE消息队列中
	 * */
	lan_recv_thread_id 		= create_lan_recv_thread();

	/*
	 *
	 * */
	if (BG_Flag == 0)
	{
		cmd_line_thread_id 	= create_cmd_line_thread();
	}

//	mac_process_thread_id 	= create_mac_thread();

	/*
	 函数pthread_join用来等待一个线程的结束,线程间同步的操作 int pthread_join(pthread_t thread, void **retval);
	 以阻塞的方式等待thread指定的线程结束
	 */
#ifndef UART_Wireless_INTERFACE
	pthread_join(wlan_sendto_thread_id, NULL);
#else
	//pthread_join(uartWireless_send_thread_id, NULL);
#endif

	pthread_join(mac_process_thread_id, NULL);

	pthread_join(lan_sendto_thread_id, NULL);

	pthread_join(route_process_thread_id, NULL);

#ifndef UART_Wireless_INTERFACE
	pthread_join(wlan_recv_thread_id, NULL);
#else
	pthread_join(M_e32_recv_thread_id, NULL);
	pthread_join(L_e32_recv_thread_id, NULL);
#endif
	pthread_join(lan_recv_thread_id, NULL);


	if (BG_Flag == 0) {
		pthread_join(cmd_line_thread_id, NULL);
	}
	return 0;
}
