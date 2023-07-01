#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include "cmdline.h"
#include "Routing.h"
#include "aux_func.h"
#include "radio.h"
#include "main.h"
#include "nRF24L01.h"

#define MAX_STORE_CMD_NUMBER 	10
#define MAX_CMD_LINE_LEN		50
#define PROMOTE					">"

char lastcmds[MAX_STORE_CMD_NUMBER][MAX_CMD_LINE_LEN];
int cmdindex = 0;


struct cmdEntity
{
	char cmd[16];
	int args;
	char argv[16];
	char * description;
};

pthread_t create_cmd_line_thread()
{
	int ret;
	pthread_t tid;

	ret = pthread_create(&tid, NULL, &cmdline_routine, NULL);
	if (ret != 0)
	{
		LOG_MSG(ERR_LEVEL,"create cmd line thread\n");
	}
	return tid;
}

int cmdParse(char * cmd,int len, struct cmdEntity * entity)
{
	int i,index = 0;
	char * first ;
	int cmdlen;
	if( len <= 0 ) return -1;
	/*
	 *	strchr返回字符串中的字符位置
	 * */
	first = strchr(cmd,' ');
	if (first != NULL)
	{
		cmdlen = first - cmd;
		if(cmdlen == 0)
		{
			cmd++;
			return cmdParse(cmd,len-1,entity);
		}
		*first = '\0';
		strcpy(entity->cmd,cmd);
		strcpy(entity->argv,first+1);
	}
	else
	{
		strcpy(entity->cmd,cmd);
	}
}


void printhelp()
{
	printf("\n\t********** help *********\n");
	printf("\thelp :show cmds infos\n");
	printf("\tset_qos: set node_qos = 1~5\n");
	printf("\tset_radius:param = 1~5\n");
	printf("\tset_cu:CLUSTER_TO_ZRP_NUM = arg-1; ZRP_TO_CLUSTER_NUM = arg+1\n");
	printf("\tset_log:level = debug:1~error:5\n");
	//printf("\tset_lsa:0:close; 1 open\n");
	printf("\tget_node:show node attr\n");
	printf("\tget_table:show route table\n");
	printf("\tstart_net_congitive :Start Network'nodes frequencys congitive \n");
	printf("\tset_speed:param=4K8/9K6/19K2/250K/1M/2M\n");
	printf("\tset_e32_freq:param=[0,15]\n");
	printf("\tset_e01_freq:param=[0,125]\n");
	printf("\quit :exit(0) \n");

}
static void cmd_set_e01_radio_link_freq(char* freq)
{
	Radio_LinkParam cfg;

	int freq_val=0;
	if(freq == NULL || *freq== '\0' || ( freq_val = atoi(freq)) <0)
	{
		LOG_MSG(ERR_LEVEL, "cmd_e01_set_radio_link_freq params error!,freq=[0, 125]\n");
		return;
	}

	if(freq_val > 126)
	{
		LOG_MSG(ERR_LEVEL, "cmd_e01_set_radio_link_freq(%d) params error, freq=[0, 125]!\n",freq_val);
		return;
	}
	L01_WriteFreqPoint(freq_val & 0xff);		//首先自配置
	memset((u8*)&cfg, 0, sizeof(cfg));

	cfg.cfg_e01_freq_en = TRUE;
	cfg.e01_freq = freq_val & 0xff;

	cfg.crc = getCRC((u8*)&cfg, sizeof(Radio_LinkParam)-2);

	//然后通过信令信道发送给其它节点
	L_E32_pks_sendto(L_e32_uart_fd, L_e32_aux_gpio_input_fd,(u8*)&cfg, sizeof(Radio_LinkParam),255);
}

static void cmd_set_e32_radio_link_freq(char* freq)
{
	Radio_LinkParam cfg;
	int freq_val=0;
	if(freq == NULL || *freq== '\0' || ( freq_val = atoi(freq)) <0)
	{
		LOG_MSG(ERR_LEVEL, "cmd_e01_set_radio_link_freq params error!,freq=[0, 15]\n");
		return;
	}

	if(freq_val > 15)
	{
		LOG_MSG(ERR_LEVEL, "cmd_e01_set_radio_link_freq(%d) params error,freq=[0,15]!\n",freq_val);
		return;
	}

	memset((u8*)&cfg, 0, sizeof(cfg));

	cfg.cfg_m_e32_freq_en 	= TRUE;
	cfg.m_e32_freq 			= freq_val & 0xff;
	cfg.crc					= getCRC((u8*)&cfg, sizeof(Radio_LinkParam)-2);
	L_E32_pks_sendto(L_e32_uart_fd, L_e32_aux_gpio_input_fd,(u8*)&cfg, sizeof(Radio_LinkParam),255);

	LOG_MSG(INFO_LEVEL, "L_E32_pks_sendto E32_FreqParams Over!\n");
	M_E32_set_radio_params(freq_val,g_m_e32_speed);

}

static void cmd_set_radio_link_speed(char* speedStr)
{
	Radio_LinkParam cfg;
	memset((u8*)&cfg, 0, sizeof(cfg));
	if(strcmp(speedStr, "4K8")==0 ||strcmp(speedStr, "4k8")==0)
	{
		g_radio_speed= SPD_E32_4K8;
		cfg.m_e32_spd = AIR_SPEEDTYPE_4K8;
		cfg.cfg_m_e32_spd_en = TRUE;
		g_m_e32_speed = AIR_SPEEDTYPE_4K8;
	}
	else if(strcmp(speedStr, "9K6")==0 ||strcmp(speedStr, "9k6")==0)
	{
		g_radio_speed= SPD_E32_9K6;
		cfg.m_e32_spd = AIR_SPEEDTYPE_9K6;
		cfg.cfg_m_e32_spd_en = TRUE;
		g_m_e32_speed = AIR_SPEEDTYPE_9K6;
	}
	else if(strcmp(speedStr, "19K2")==0 ||strcmp(speedStr, "19k2")==0)
	{
		g_radio_speed= SPD_E32_19K2;
		cfg.m_e32_spd = AIR_SPEEDTYPE_19K2;
		cfg.cfg_m_e32_spd_en = TRUE;
		g_m_e32_speed = AIR_SPEEDTYPE_19K2;
	}
	else if(strcmp(speedStr, "250K")==0 ||strcmp(speedStr, "250k")==0)
	{
		g_radio_speed= SPD_E01_250K;
		cfg.e01_spd = SPD_250K;
		cfg.cfg_e01_spd_en = TRUE;
		L01_SetSpeed(SPD_250K);
	}
	else if(strcmp(speedStr, "1M")==0 ||strcmp(speedStr, "1m")==0)
	{
		g_radio_speed= SPD_E01_1M;
		cfg.e01_spd = SPD_1M;
		cfg.cfg_e01_spd_en = TRUE;
		L01_SetSpeed(SPD_1M);
	}
	else if(strcmp(speedStr, "2M")==0 ||strcmp(speedStr, "2m")==0)
	{
		g_radio_speed= SPD_E01_2M;
		cfg.e01_spd = SPD_2M;
		cfg.cfg_e01_spd_en = TRUE;
		L01_SetSpeed(SPD_2M);
	}
	else
	{
		LOG_MSG(ERR_LEVEL, "set_radio_link_speed params error!\n");
		return;
	}
	cfg.crc = getCRC((u8*)&cfg, sizeof(Radio_LinkParam)-2);
	L_E32_pks_sendto(L_e32_uart_fd, L_e32_aux_gpio_input_fd,(u8*)&cfg, sizeof(Radio_LinkParam),255);

	if(cfg.cfg_m_e32_spd_en ==TRUE)
	{
		LOG_MSG(INFO_LEVEL, "Prepare Set M_E32 freq(%d), Speed(%d)!\n", g_m_e32_ch,  g_m_e32_speed);
		M_E32_set_radio_params(g_m_e32_ch,g_m_e32_speed);
	}
}

/*
 * 处理stdin输入的命令
 * */
int handle_cmd (char * cmd , int len)
{
	int arg;
	struct cmdEntity entity;
	if(len==0 )
		return 0;

	cmdParse(cmd,len,&entity);
	/*
	 * 输入help
	 * */
	if(strcmp(entity.cmd,"quit") == 0)
	{
		exit(0);
	}
	/*
	 * 输入help
	 * */
	else if(strcmp(entity.cmd,"help") == 0)
	{
		printhelp();
		return 0;
	}
	/*
	 * 输入set_qos, 设置节点QoS，并输出节点属性
	 * */
	else if(strcmp(entity.cmd,"set_qos") == 0)
	{
		arg = atoi(entity.argv);
		Node_Attr.node_qos = arg;
		Print_Node_Attr();
		return 0;
	}
	/*
	 * 设置节点域内半径
	 * */
	else if(strcmp(entity.cmd,"set_radius") == 0)
	{
		arg = atoi(entity.argv);		
		Node_Attr.IARP_radius = arg;
		Print_Node_Attr();
		return 0;
	}

	else if(strcmp(entity.cmd,"set_cu") == 0)
	{
		arg = atoi(entity.argv);		
		CLUSTER_TO_ZRP_NUM = arg-1;
		ZRP_TO_CLUSTER_NUM = arg+1;
		Print_Node_Attr();
		return 0;
	}
	/*
	 * 设置全局变量Log_Level值
	 * */
	else if(strcmp(entity.cmd,"set_log") == 0)
	{
		arg = atoi(entity.argv);		
		Log_Level = arg;
		return 0;
	}else if(strcmp(entity.cmd,"set_lsa") == 0)
	{
		arg = atoi(entity.argv);		
		//LSA_Flag = arg;
		return 0;
	}else if(strcmp(entity.cmd,"get_node") == 0)
	{
		
		Print_Node_Attr();
		return 0;
	}

	/*
	 * 打印一跳、二跳和域间路由
	 * */
	else if(strcmp(entity.cmd,"get_table") == 0)
	{
		
		Print_All();
		return 0;
	}

	else if(strcmp(entity.cmd, "start_net_congitive")==0  ||strcmp(entity.cmd, "Start_Net_Congitive")==0)
	{
		Start_Net_Congitive();
	}

	/*
	 * 调用set_speed 4K8/9K6/19K2/250K/1M/2M命令
	 * */
	else if(strcmp(entity.cmd, "set_speed")==0)
	{
		cmd_set_radio_link_speed(entity.argv);
	}

	else if(strcmp(entity.cmd, "set_e32_freq")==0)
	{
		cmd_set_e32_radio_link_freq(entity.argv);
	}
	else if(strcmp(entity.cmd, "set_e32_freq")==0)
	{
		cmd_set_e01_radio_link_freq(entity.argv);
	}
	else
	{ 
		return -1;
	}
	return 0;
}

void cmdline_routine()
{
	char ch;
	char cmd[MAX_CMD_LINE_LEN];
	char *curcmd;
	int n = 0,ret;
	
	//printf("%s",PROMOTE);
	while(1)
	{
		
		memset(cmd,0,MAX_CMD_LINE_LEN);
		/*
		 *Get a newline-terminated string of finite length from STREAM.
		 * */
		fgets(cmd,MAX_CMD_LINE_LEN,stdin);
		n = strlen(cmd);

		/*
		 *
		 * */
		curcmd = cmd;
		//memcpy(curcmd,cmd,n);
		//printf("get cmd %d byts\n",n);
		if(n <= 0)
		{
			continue;
		}
		else
		{
			if(curcmd[n-1] == '\n' || curcmd[n-1] == '\r')
			{
				curcmd[n-1] = '\0';
				ret = handle_cmd(curcmd,strlen(curcmd));
				if(ret != 0)
				{
					printf("\ncmd %s is not correct.\n",cmd);
					printhelp();
				}
				printf("\n%s",PROMOTE);
				
			}else
			{
				continue;
			}
		}
	}
}
