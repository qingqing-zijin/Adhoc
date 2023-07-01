/**
 * @fuction��	ϵͳ����ڣ���ʼ��ϵͳ����ģ��
 * @author��  	lsp
 * @email: 		siping@iscas.ac.cn
 * @date��		2015-2-2
 * @version��	v0.0.1
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
 * �����̨L_E32��ǰƵ��
 * */
Radio_CHL_t		g_l_e32_ch = CHANL14;	//[CH0~CH15]

/*
 * ҵ���̨M_E32��ǰƵ��
 * */
Radio_CHL_t		g_m_e32_ch = CHANL1;  //[CH0~CH15]

/*
 * ҵ���M_E32��ǰ����
 * */
u8				g_m_e32_speed = AIR_SPEEDTYPE_19K2	;//SPD_E01_250K;//SPD_E32_19K2;

/*
 * ҵ���̨M_E32��E01��ǰ����
 * */
RADIO_SPEED_t	g_radio_speed = SPD_E01_250K;//SPD_E01_250K;//SPD_E32_19K2;

/*
 * ҵ���̨M_E32��E01����ģʽ��������1-���� 0-����
 * */
u8				g_UPG_on = FALSE;


/*
 * @function sys_start_init
 * @desc:
 * 		ϵͳ��ʼ����������ʼ����ӡ�����������Ϣ��ȡ��
 * @return :
 * 		TRUE: �ɹ�
 * 		FALSE��ʧ��
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

	// 1. ��·�������ļ�����ȡ·�ɲ���
	f_ptr = fopen("routeCfg.ini", "r");
	if (f_ptr == NULL)
	{
		f_ptr = fopen("routeCfg.ini", "w");
		if (f_ptr == NULL) {

			return 0;
		}
		//���·�������ļ������ڣ���Ĭ�ϵĲ���д���½��������ļ���
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
		//W����������ļ��ڣ���������ļ��ж�ȡ����������������Կ��ء�����·�ɰ뾶��
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
	// 2.��LOG�����ļ�
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
		num = (atoi(buff) + 1) & 0xf;		// ��ౣ��16���ļ�
		fclose(Flog_ptr);

		Flog_ptr = fopen("logIndex.txt", "w");
		if (Flog_ptr == NULL) {
			return 0;
		}

		fprintf(Flog_ptr, "%d", num);
		fflush(Flog_ptr);
		fclose(Flog_ptr);
	}

	// 3. �� log ����ļ�
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
 * 	  �������
 * */
int main(int argc, char** argvs)
{
	int ret=0;
	//��ȡ����������Ϣ����ʼ��g_lanif�����ṹ����
	sys_start_init(argc,  argvs);

	self_node_id =getSelfID() ;

	/*
	 * �����С�����E32��E43���ߵ�̨��GPIO�ܽ�
	 * */
	ret=radios_gpio_init();
	if(ret<0)
	{
		LOG_MSG(ERR_LEVEL, "radios_gpio_init error!\n");
		return -1;
	}
	/*
	 * ����ҵ����������ģ�鹤�������������������ʡ��տ����ʼ�����ģʽ
	 * */
	/********************************************************
	 *				E32ҵ��ģ���ʼ������������ҵ������
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config Radio Medium Speed E32...!\n");
	if((M_e32_uart_fd =config_E32_radio_params(COM1,M_e32_aux_gpio_input_fd, g_m_e32_ch, g_m_e32_speed,POWER_21dBm_ON ))<0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to config_E32(Medium Speed )_radio_params!\n");
		return -1;
	}
	/********************************************************
	 *				E32����ģ���ʼ�������ڵ�����������
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config Radio Low Speed E32...!\n");
	if((L_e32_uart_fd =config_E32_radio_params(COM7,L_e32_aux_gpio_input_fd,g_l_e32_ch, AIR_SPEEDTYPE_2K4,POWER_21dBm_ON ))<0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to config_E32(Low Speed )_radio_params!\n");
		return -1;
	}

//	return 0;		//test E32
	/********************************************************
	 *				E43ģ���ʼ��,����Ƶ�׸�֪
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config Radio E43...!\n");

	if(global_e43_existed == TRUE && config_E43_radio_rssi_mode_test()<0)
	{
		LOG_MSG(ERR_LEVEL, "Failed to config_E43_radio_rssi_mode_test!\n");
		return -1;
	}
//	return 0;		//test E43

	/********************************************************
	 *				E01+ģ���ʼ�������ڸ���ҵ������
	 *******************************************************/
	LOG_MSG(INFO_LEVEL,"--------------------------------------Config High Speed Radio E01...!\n");
	config_E01_radio_params();

	/********************************************************
	 *
	 *******************************************************/
	nsv_network_init();
	/*
	 * ����������Ϣ����
	 * */
	create_queues();
	/*
	 * ���������߳�
	 * */
	create_threads();
	return 0;
}

/**
 * �����ӡ������Ϣ��IPv4 IP��ַ����ַ����͹㲥��ַ
 */
void printIfInfos(const struct ifaddrs *ifinfos) {
	void* tmpAddrPtr;

	/*
	 * ��ȡIP_V4 IP��ַ��ת�����������ַ������addressBuffer��
	 * */
	char addressBuffer[INET_ADDRSTRLEN];
	LOG_MSG(INFO_LEVEL, "\n%s: config\n", ifinfos->ifa_name);
	tmpAddrPtr = &((struct sockaddr_in*) (ifinfos)->ifa_addr)->sin_addr;

	//����inet_ntop�������ֽڵ�ַת��Ϊ�����ֽ�
	inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
	LOG_MSG(INFO_LEVEL, "address  : %s\n", addressBuffer);

	/*
	 *��ȡIP_V4 IP��ַ���룬ת���������ip�����ַ������addressBuffer��
	 * */
	tmpAddrPtr = &((struct sockaddr_in*) (ifinfos)->ifa_netmask)->sin_addr;


	inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

	LOG_MSG(INFO_LEVEL, "net mask : %s\n", addressBuffer);

	/*
	 *��ȡIP_V4 IP�㲥��ַ��ת���������ip�㲥��ַ������addressBuffer��
	 * */
	tmpAddrPtr =&((struct sockaddr_in*) (ifinfos)->ifa_ifu.ifu_broadaddr)->sin_addr;
	inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
	LOG_MSG(INFO_LEVEL, "broadaddr: %s\n\n", addressBuffer);
}

/**
 * ��ʼ��ϵͳ����
 * return if success return 1;else return 0;
 */
int init()
{

	gettimeofday(&starttime, NULL);

	LOG_MSG(INFO_LEVEL, "\n************system start at %d:%d:%d.%03d\n",
			starttime.tv_sec / 3600, (starttime.tv_sec / 60) % 60,
			starttime.tv_sec % 60, starttime.tv_usec / 1000);

	/*
	 * ��ȡ������host��������������Ϣ������eth0\eth1��������Ϣ
	 * */
	getAllIfconfig();

	//��ӡ����lan������Ϣ
	printIfInfos(&g_lanif);


#ifndef UART_Wireless_INTERFACE

	//��ӡ����wlan������Ϣ
	printIfInfos(&g_wlanif);
#endif

	return 1;
}

/**
 * ��ȡ��������������Ϣ��ֻȡ��WLAN_INTREFACE��LAN_INTERFACE������������Ϣ��
 * ������Ϣ���뵽g_wlaninfos �� g_laninfos.
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
 * desc:	��������getifaddrs��ȡ��������������Ϣ��
 * 			��̫����ethx���������ȫ�ֱ���g_lanif��
 * 			������WIFI������������ȫ�ֱ���g_wlanif��
 * */
void getAllIfconfig() {
	struct ifaddrs *ifaddrStruct = NULL;
	struct ifaddrs *ifa = NULL;
	int m1 = 0, m2 = 0;

	/*
	 ����getifaddrs��int getifaddrs (struct ifaddrs **__ifap)����ȡ��������ӿ���Ϣ��
	 ��֮�洢�������У�����ͷ���ָ��洢��__ifap�д��أ�
	 ����ִ�гɹ�����0��ʧ�ܷ���-1����Ϊerrno��ֵ
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
					//��ȡ��������wlan0��Ϣ
					g_wlanif.ifa_name = ifa->ifa_name;//Name of interface
					g_wlanif.ifa_addr = ifa->ifa_addr; /* Address of interface */
					g_wlanif.ifa_netmask = ifa->ifa_netmask; /* Netmask of interface */
					g_wlanif.ifa_ifu.ifu_broadaddr = ifa->ifa_ifu.ifu_broadaddr; /* Broadcast address of interface */
					m1= 1;
				}
				else if(strcmp(LAN_INTERFACE,ifa->ifa_name) == 0)
				{
					//��ȡ��������"eth0"
					g_lanif.ifa_name = ifa->ifa_name;
					g_lanif.ifa_addr = ifa->ifa_addr;
					g_lanif.ifa_netmask = ifa->ifa_netmask;
					g_lanif.ifa_ifu.ifu_broadaddr = ifa->ifa_ifu.ifu_broadaddr;
					m2 = 1;
				}
#else
				if (strcmp(LAN_INTERFACE, ifa->ifa_name) == 0) {
					//��ȡ��������"eth0"
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
	//sockaddr_in�ṹ����sin_port�� sin_addr����ΪNBO˳��
	wlan_ina.sin_port = htons(23);

	//��ȡ��������wlan0��Ϣ
	g_wlanif.ifa_name = wlan_name;
	g_wlanif.ifa_next = NULL;
	g_wlanif.ifa_data = NULL;
	g_wlanif.ifa_addr = (struct sockaddr*) &wlan_ina;	//Address of interface

	//* ��������
	bzero((char*) &wlan_ina_mask, sizeof(wlan_ina_mask));
	wlan_ina_mask.sin_family = AF_INET;
	wlan_ina_mask.sin_port = htons(23);
	wlan_ina_mask.sin_addr.s_addr = inet_addr("255.0.0.0");
	g_wlanif.ifa_netmask = (struct sockaddr*) &wlan_ina_mask;// Netmask of interface

	//* �㲥��ַ
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

	//���ζ�wlan ip��local ip ���㲥ip����ַ������и�ֵ
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
 *����ϵͳ ��Ҫ���߳�
 */
int create_threads()
{
	 /*
	 *Ad Hoc �����ƣ�
	 *
	 * 	Cmd_Line-------->PC
	 *			 	 	 |
	 *					\|/
	 *				LAN_RECV_CQ
	 *					 |
	 *					\|/
	 *���߿տ�<---MAC----->ROUTE<--------->LAN_SEND_CQ---> PC
	 *
	 *
	 * */
	pthread_t cmd_line_thread_id;

	/*
	 *���������߿տڽ���������߳�
	 * */
	L_e32_recv_thread_id = create_L_e32_uartWireless_recv_thread();

	/*
	 *���������߿տڽ�������E32���ݱ����߳�
	 * */
	M_e32_recv_thread_id = create_M_e32_uartWireless_recv_thread();

	/*
	 * ����MAC���Ʒ����߳�
	 * */
	mac_process_thread_id 	= create_mac_thread();


	/*
	 * ��������Ƶ�׸�֪��ʱ�����󶨶�ʱ����ʱ�ص�����
	 * */
	NetCong_Module_Init();

	//�����lan��Ϣ�����ж���Ϣ����ϢΪIP���ݱ���Ϊ·�ɲ����
	/*
	 * ��������������߽��յ�IP���ĵ��̡߳������߽��յ�Ŀ�ĵ�ַΪ���ڵ��IP���ľ�����LAN_SEND_CQ��Ϣ�����С�
	 * */
	lan_sendto_thread_id 	= create_lan_sendto_thread();

	/*
	 *��������Ad Hoc·��Э��(ROUTE)�̡߳�
	 *
	 * */
	route_process_thread_id = create_route_thread();

	// MAC�㴦���ŵ�����
	//	create_uartWireless_sendTimer_thread();


	/*
	 *������PC���ա�����UDP IP�����߳�,���յ�����ЧIP���������ROUTE��Ϣ������
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
	 ����pthread_join�����ȴ�һ���̵߳Ľ���,�̼߳�ͬ���Ĳ��� int pthread_join(pthread_t thread, void **retval);
	 �������ķ�ʽ�ȴ�threadָ�����߳̽���
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
