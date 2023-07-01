#ifndef	AUX_FUNC_H
#define	AUX_FUNC_H
#include	<signal.h>
#include	<time.h>

#include	<stdarg.h>
#include	<stdio.h>

#define		MAC_LEVEL		0
#define		LSA_LEVEL		1
#define		DBG_LEVEL		2		// app level
#define		INFO_LEVEL		3
#define		WARN_LEVEL		4
#define		ERR_LEVEL		5
#define		MAX_LEVEL		100

#define 	TIMER_ONE_SHOT 	1
#define		TIMER_INTERVAL	0

extern int	Log_Level;

extern void LOG_MSG(int level, char *format, ...);
extern unsigned short getCRC(unsigned char *Msg,int MsgL);
extern unsigned short checkSum(unsigned char *buff, int size);
extern unsigned long long getCurrent_ms();
extern unsigned long getCurrent_s();
extern unsigned long long sysTimeGet();
extern char * getCurrentTime();
extern unsigned short getSrcAddr_fromIPHeader(unsigned char *iphead_ptr);
extern unsigned short getDestAddr_fromIPHeader(unsigned char *iphead_ptr);
extern unsigned char getProto_fromIPHeader(unsigned char *iphead_ptr);
extern unsigned short getSrcPort_fromIPHeader(unsigned char *iphead_ptr);
extern unsigned short getDstPort_fromIPHeader(unsigned char *iphead_ptr);
extern unsigned short getIPSize_fromIPHeader(unsigned char *iphead_ptr);
extern int wdStart(timer_t timerid,long msecs,int flag);
extern int wdCreate(int sigNum,timer_t *timerid, void (*handler)(int,siginfo_t *,void *) );
extern unsigned short getSelfID();
char* find_n_comma(char * str,  unsigned char n);
#endif
