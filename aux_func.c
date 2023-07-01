/**				***********文件描述**************
	提供消息队列，计时器，系统时间读取，线程，CRC校验等函数

*/

#include	"aux_func.h"
#include	<sys/msg.h>
#include	<sys/time.h>
#include 	<stdlib.h>
#include	<unistd.h>
#include	<stdio.h>
#include	<signal.h>
#include	<time.h>
#include	<arpa/inet.h>
#include	<netinet/in.h>
#include	<sys/socket.h>
#include 	<ifaddrs.h>
#include	"queues.h"


int		Log_Level = INFO_LEVEL;


//extern struct ifaddrs g_wlanif;

extern struct ifaddrs g_lanif;

extern struct timeval starttime;

extern char  	Flog_name[32];
															// IBM:G(x)=x^16+x^15+x^2+1
const unsigned short Tab_CRC16_IBM[256]={
           0 ,	 32773 ,      32783,          10,       32795,          30,          20,       32785,
       32819,	    54,          60,       32825,          40,       32813,       32807,          34,
       32867,	   102,         108,       32873,         120,       32893,       32887,         114,
          80,	 32853,       32863,          90,       32843,          78,          68,       32833,
       32963,	   198,         204,       32969,         216,       32989,       32983,         210,
         240,	 33013,       33023,         250,       33003,         238,         228,       32993,
         160,	 32933,       32943,         170,       32955,         190,         180,       32945,
       32915,	   150,         156,       32921,         136,       32909,       32903,         130,
       33155,	   390,         396,       33161,         408,       33181,       33175,         402,
         432,	 33205,       33215,         442,       33195,         430,         420,       33185,
         480,	 33253,       33263,         490,       33275,         510,         500,       33265,
       33235,	   470,         476,       33241,         456,       33229,       33223,         450,
         320,	 33093,       33103,         330,       33115,         350,         340,       33105,
       33139,	   374,         380,       33145,         360,       33133,       33127,         354,
       33059,	   294,         300,       33065,         312,       33085,       33079,         306,
         272,	 33045,       33055,         282,       33035,         270,         260,       33025,
       33539,	   774,         780,       33545,         792,       33565,       33559,         786,
         816,	 33589,       33599,         826,       33579,         814,         804,       33569,
         864,	 33637,       33647,         874,       33659,         894,         884,       33649,
       33619,	   854,         860,       33625,         840,       33613,       33607,         834,
         960,	 33733,       33743,         970,       33755,         990,         980,       33745,
       33779,	  1014,        1020,       33785,        1000,       33773,       33767,         994,
       33699,	   934,         940,       33705,         952,       33725,       33719,         946,
         912,	 33685,       33695,         922,       33675,         910,         900,       33665,
         640,	 33413,       33423,         650,       33435,         670,         660,       33425,
       33459,	   694,         700,       33465,         680,       33453,       33447,         674,
       33507,	   742,         748,       33513,         760,       33533,       33527,         754,
         720,	 33493,       33503,         730,       33483,         718,         708,       33473,
       33347,	   582,         588,       33353,         600,       33373,       33367,         594,
         624,	 33397,       33407,         634,       33387,         622,         612,       33377,
         544,	 33317,       33327,         554,       33339,         574,         564,       33329,
       33299,	   534,         540,       33305,         520,       33293,       33287,         514};

unsigned short getCRC(unsigned char *Msg,int MsgL)
{
	int i;
	unsigned short cReg;
	unsigned char  MsgChar;

	cReg=0;
	i=0;
	//for (i=0;i<MsgL;i++)
	while(i++<MsgL)
	{
		MsgChar = *Msg++;
		cReg=(Tab_CRC16_IBM[((MsgChar<<8)^cReg)>>8])^(cReg<<8);
	}	
	
	return htons(cReg);		// 按BIG_ENDIAN 返回
}
unsigned short checkSum(unsigned char *buff, int size)
{
	unsigned short *msg = (unsigned short *)buff;
	unsigned long cksum = 0;
	int	i = size;
	
	while(i >=2)
	{
		cksum += *msg++;
		i -=2;
	}
	if(i)
	{
		cksum += *(unsigned char*)msg;
	}
	
	cksum = (cksum>>16)+(cksum&0xffff);
	cksum += (cksum>>16);
	
	cksum = ~cksum;
	
	return cksum;		// 按little_ENDIAN 返回,应用是直接ushort赋值
}
/* LOG_MSG		-- 调试输出
	若 level >= Log_Level, 则打印;否则，不打印 
*/
void LOG_MSG(int level, char *format, ...)
{
	va_list	args;
	unsigned long long time;
	struct timeval curtime;
    FILE 	*Flog_ptr;
    
//	if (Log_Level <=0)
//	{
//		// 按|Log_Level| 进行输出，目标是stdout
//		if(level <(0-Log_Level) )
//			return;
//
//		va_start(args, format);
//		time = sysTimeGet();
//		printf("%lld:%lld:%lld.%03lld  ",time/1000/3600, (time/1000/60)%60, (time/1000)%60, time%1000);
//		vprintf(format, args);
//		va_end(args);
//	}
//	else
//	{
//		if(level < Log_Level)
//			return;
//
//        Flog_ptr = fopen(Flog_name,"a+");
//        if(Flog_ptr == NULL)
//            return;
//
//		va_start(args, format);
//
//        time = sysTimeGet();
//		fprintf(Flog_ptr, "%lld:%lld:%lld.%03lld  ",time/1000/3600, (time/1000/60)%60, (time/1000)%60, time%1000);
//		vfprintf(Flog_ptr,format, args);
//		fflush(Flog_ptr);
//		va_end(args);
//
//        fclose(Flog_ptr);
//	}
//	if (Log_Level <=0)
//	{
//		// 按|Log_Level| 进行输出，目标是stdout
//		if(level <(0-Log_Level) )
//			return;
//
//		va_start(args, format);
//		time = sysTimeGet();
//		printf("%lld:%lld:%lld.%03lld  ",time/1000/3600, (time/1000/60)%60, (time/1000)%60, time%1000);
//		vprintf(format, args);
//		va_end(args);
//	}
//	else
//	{
 		if(level < Log_Level)		//级别小于LOG_LEVEL的不打印
 			return;
 		//
 		va_start(args, format);
 		time = sysTimeGet();
 		printf("%lld:%lld:%lld.%03lld  ",time/1000/3600, (time/1000/60)%60, 
 			(time/1000)%60, time%1000);
 		vprintf(format, args);
 		va_end(args);
 //	}
 
}
/*
unsigned short getCRC(unsigned char *Msg,int MsgL)
{
    int i, j;
    unsigned short crc, flag;
    crc = 0x0000;
    for (i = 0; i < MsgL; i++)
    {
        crc ^= (unsigned short)(((unsigned short)Msg[i]) << 8);
        for (j = 0; j < 8; j++)
        {
            flag = (unsigned short)(crc & 0x8000);
            crc <<= 1;
            if (flag > 0)
            {
                crc &= 0xfffe;
                crc ^= 0x8005;
            }
        }
    }
	return htons(crc);
}
*/

/**
* 获取到与开机时刻之间的时间差，返回毫秒值
*/
unsigned long long getCurrent_ms()
{
	struct timeval curtime;
	gettimeofday(&curtime,NULL);
    /*系统启动时，时间为2000年1月1日，Debian用户登录后，将系统时间更新为2014年4月24日。
    所以要在程序中间判定，变为2014年后，给starttime重赋值*/
    if(curtime.tv_sec > 1398280000 && starttime.tv_sec < 946685000)
    {
        starttime.tv_sec = curtime.tv_sec;
        starttime.tv_usec = curtime.tv_usec;
    }
	return (curtime.tv_sec - starttime.tv_sec) * 1000 + (curtime.tv_usec - starttime.tv_usec)/1000;	
}


/**
* seconds from epoch to now
*/
unsigned long getCurrent_s()
{
	time_t curtime;
	time(&curtime);
	return curtime;
}

unsigned long long sysTimeGet()
{
	return getCurrent_ms();
}

char * getCurrentTime()
{
	time_t curtime;
	struct tm *now;
	time(&curtime);
	now = localtime(&curtime);
	return asctime(now);
}

void hexprint(const char* data, int len)
{
	int i = 0;
	printf("data len = %d\n",len);
	while (i < len)
	{
		if (i++ % 16 == 0)
			printf("\n");
		printf("%02x ", (*data++)&0xff);
	}
	printf("\n");
}

unsigned short getSelfID()
{

#if 0
	//WiBox Wifi网卡
	struct in_addr gwl = (struct in_addr)((struct sockaddr_in*)(&g_wlanif)->ifa_addr)->sin_addr;	
#else
	//E32/E52无线模块
	struct in_addr gwl = (struct in_addr)((struct sockaddr_in*)(&g_lanif)->ifa_addr)->sin_addr;
#endif

	return (unsigned short)((htonl(gwl.s_addr) >> 8) & 0xffff);	 
}

unsigned short getSrcAddr_fromIPHeader(unsigned char *iphead_ptr)
{
	//struct ip iphdr;
	//memcpy(&iphdr, iphead_ptr, sizeof(iphdr));
	//unsigned short ret =0;
	//iphdr = (struct ip *)iphead_ptr;
	//ret = (unsigned short)((iphdr.ip_src.s_addr >> 8) & 0xffff);
	//printf("src addr = %d",ret);

	unsigned short ret = 0;
	ret = iphead_ptr[13];
	ret <<= 8;
	ret |= iphead_ptr[14];
	
	return ret;
}

unsigned short getDestAddr_fromIPHeader(unsigned char *iphead_ptr)
{
	unsigned short ret = 0;
	unsigned char ipaddr_h;
	
	ipaddr_h = iphead_ptr[16];
	
	if(ipaddr_h >= 224)
		return 0xffff;
	else
	{
		ret = iphead_ptr[17];
		ret <<= 8;
		ret |= iphead_ptr[18];
		return ret;
	}
}

unsigned char getProto_fromIPHeader(unsigned char *iphead_ptr)
{
	return (unsigned char) iphead_ptr[9];	 
}
unsigned short getIPSize_fromIPHeader(unsigned char *iphead_ptr)
{
	unsigned short ret = 0;
	ret = iphead_ptr[2];
	ret <<= 8;
	ret |= iphead_ptr[3];
	return ret;
	
}
unsigned short getSrcPort_fromIPHeader(unsigned char *iphead_ptr)
{
	
	unsigned short ret = 0;
	ret = iphead_ptr[20];
	ret <<= 8;
	ret |= iphead_ptr[21];
	
	return ret;
}

unsigned short getDstPort_fromIPHeader(unsigned char *iphead_ptr)
{
	unsigned short ret = 0;
	ret = iphead_ptr[22];
	ret <<= 8;
	ret |= iphead_ptr[23];
	
	return ret;
}


/**
* create a CLOCK_REALTIME type timer
* input:
	sigNum 		signalnum 
	timerid 	the id of timer if create success
	void 		handler when the timer fire
  return:
	0 	if create timer success;
	-1 	create timer fail
*/
int wdCreate(int sigNum,timer_t *timerid, void (*handler)(int,siginfo_t *,void *) )
{
    struct sigevent sev;
    sigset_t mask;
	
    struct sigaction sa;
	/*
	sa.sa_flags
	为实现多个定时器产生了同一个信号后，区分是哪一个定时器产生了信号，
	必需在使用struct sigaction的成员sa_flags，标识为SA_SIGNFO
	*/
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset(&sa.sa_mask);
	if(sigaction(sigNum,&sa,NULL)==-1)
	{
		LOG_MSG(ERR_LEVEL,"create tiemr sigaction");
		return -1;
	}

	/* Create the timer */
	/*
	1)evp->sigev_notify: 说明了定时器到期是应该采取的行动，通常，这个成员值设为SIGEV_SIGNAL，表明
						 在定时器到期时，会产生一个信号。
						 > 当设为SIGEV_NONE，什么都不做，只提供通过timer_gettime和timer_getoverrun来查询超时信息。来防止定时器到期时产生信号
						 > 当设为SIGEV_SIGNAL,当定时器到期，内核会将sigev_signo所指定的信号传送给进程，在信号处理函数中，si_value会被设定为
						 	sigev_value;
	2)evp->sigev_signo:  如果要产生除默认信号之外的其它信号，程序必须将evp->sigev_signo设置为期望的信号码
	*/	
    sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = sigNum;

	/*
	如果几个定时器产生了同一个信号，处理程序可以用evp->sigev_value来区分是哪一个定时器产生了信号。

	union sigval
	{
		int sival_int; //integer_value
		void* sival_ptr; //pointer value
	}
	*/
    sev.sigev_value.sival_ptr = timerid;

	/*
	调用timer_create()所创建的定时器并未启动，要启动定时器，可以使用timer_settime()
	*/
    if (timer_create(CLOCK_REALTIME, &sev, timerid) == -1)
   {
	      LOG_MSG(ERR_LEVEL,"timer_create");
	      return -1;
	}
	return 0;	
	
}

/**
*start timer witch id is timerid
* input:
	timerid         timer id
	msecs 		interval macrosecends
	flag		TIMER_ONE_SHOT: one shot; TIMER_INTERVAL interval
  return:
	0  success
	-1 fail
*/
int wdStart(timer_t timerid,long msecs,int flag)
{

	struct itimerspec its;
	/* Start the timer */
	/*
	 * 设定第一次运行超时时间
	 * */
	its.it_value.tv_sec = msecs / 1000;
    its.it_value.tv_nsec = (msecs%1000) *1000000;

	/*
	调用timer_settime函数，设置定时器的定时周期interval并启动定时器
	*/
	if(flag == TIMER_INTERVAL )
	{
		//为间隔定时器类型
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;
	}else
	{
		//为运行一次定时器类型，超时则回到未启动状态
		its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
	}

	//启动定时器
    if (timer_settime(timerid, 0, &its, NULL) == -1)
    {
		LOG_MSG(ERR_LEVEL,"timer_settime");
		return -1;
	}
	return 0;	
}

/**
*@function find_n_comma
*@brief 	 查找第n次出现 ',' 字符，返回第n次出现的首地址
*@param		str: 输入字符串； n:代表第几个',', n>1
*@return
*/
char* find_n_comma(char * str,  unsigned char n)
{
	//u8  comma[2] =",";
	char comma= ',';
	char *p, *q;
	if(str == NULL || n<1)
		return NULL;
	else
	{
			//p= strstr(str, comma);
			p=strchr(str, comma);
			if(NULL == p)
			{
				return NULL;
			}
			q= p+ 1;	//strlen(comma)返回不包括'\0'结束符的字符串长度
			n--;

			while(n>0)
			{
//				p = strstr(q, comma);
				p=strchr(q, comma);

				if(NULL== p)
					break;
				q= p+ 1;
				n--;
			}
	}

	return p;
}
