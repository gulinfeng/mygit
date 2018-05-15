/***implement send log to server***/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>      // Error number definitions 
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <netinet/in.h>
#include "door_log.h"
#include "taskpool.h"
#include "door_info.h"


typedef struct tg_log_param
{
	int arg;
	char room[8];
	char card_id[32];
	char ztid[4];
}LOG_PARAM_S;

extern int generate_params( struct tcppacket** packet, DOOR_CMD_DT_S** cmd_dt);

extern void free_params(int process_ret, unsigned int Data1, unsigned int Data2, unsigned int Data3);

extern int send_completed(int fd, char* send_buf, int len);

static int g_log_enable = 1;

void dr_log_enable(int enable)
{
	if(g_log_enable != enable){
		if(0 == enable){
			printf("INFO: door log disable\n");
		}else{
			printf("INFO: door log enable\n");
		}
	}
	g_log_enable = (0 == enable) ? 0 : 1;
	
}

static int nw_sendlog(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int ret, len, packet_size;
	int recv_start_flg, continue_flg;
	int error = -1;
	int listenfd;
	struct timeval tm;
	struct sockaddr_in servaddr;
	struct tcppacket * pmy = (struct tcppacket *)Data1;
	DOOR_CMD_DT_S* pCMDDt = (DOOR_CMD_DT_S*)Data2;
	LOG_PARAM_S* pLog = (LOG_PARAM_S*)pCMDDt->reserve1;
	fd_set set;
	ret = 0;
	len=sizeof(int);

	printf("INFO: sendlog(%s, arg:%d)\n", pCMDDt->address,  pLog->arg);

	listenfd=socket(AF_INET,SOCK_STREAM,0);
	if(listenfd<0){
		fprintf(stderr,"socket error\n");
		return 0 ;
	}
	memset(&servaddr,'\0',sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(pCMDDt->address);
	servaddr.sin_port = htons((unsigned short)pCMDDt->port);
	//inet_aton(pCMDDt->address,&servaddr.sin_addr);
	unsigned long ul = 1;
	ioctl(listenfd, FIONBIO, &ul);  //设置为非阻塞模式

	if(connect(listenfd,(struct sockaddr *)&servaddr,sizeof(struct sockaddr_in))==-1)
	{
		if (errno == EINPROGRESS)
		{
			tm.tv_sec  = 5;//0
			tm.tv_usec = 0; //500000
			FD_ZERO(&set);
			FD_SET(listenfd, &set);
			if( select(listenfd+1, NULL, &set, NULL, &tm) > 0)//listenfd+1
			{
				getsockopt(listenfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
				if(error == 0) 
				{
					ret = 1;
				}
				else ret = 0;
			} 
			else ret = 0;
		}
		else ret = 0;
	}
	else ret = 1;
	ul = 0;
	ioctl(listenfd, FIONBIO, &ul);  //设置为阻塞模式
	if(ret==0) 
	{
		close( listenfd );
		if(pCMDDt->net_respones_ck){
			pCMDDt->net_respones_ck(DR_NET_DISCONNECTED, pmy, pCMDDt);
		}
		printf("WARNING: Cannot Connect!\n");
		return 0;
	}
	printf("INFO: Connected!\n");

	packet_size = sizeof(struct tcppacket);

	recv_start_flg = 0;

	strcpy(pmy->ip,di_get_door_ip());//  第一次发送本机IP地址给服务器

	//pmy->state = 0x84;
	if(di_get_door_type() == 3)
	{
		memset(pmy->password,'\0',5);
		strcpy(pmy->password,pLog->ztid);
		pmy->title[1]='1';//围墙机
	}
	else
	{
		pmy->title[1]='2';//门口机
	}
	strcpy(pmy->roomnum,pLog->room);
	switch (pLog->arg){
	case 0://呼叫室内机失败
		pmy->title[0]=0x10;
		strcpy(pmy->roomnum,pLog->room); 
		break; 
	case 1://呼叫室内机成功
		pmy->title[0]=0x11;
		break;
	case 2://刷卡失败
		pmy->title[0]=0x20; 
		break;
	case 3://刷卡成功
		pmy->title[0]=0x21; 
		break;
	case 4://呼叫管理中心失败
		pmy->title[0]=0x30; 	
		break;
	case 5://呼叫管理中心成功
		pmy->title[0]=0x31; 	
		break;
	case 6://密码开门失败
		pmy->title[0]=0x40; 	
		break;
	case 7://密码开门成功
		pmy->title[0]=0x41; 	
		break;
	case 8://修改密码失败
		pmy->title[0]=0x50; 	
		break;
	case 9://修改密码成功
		pmy->title[0]=0x51; 	
		break;
	case 10://管理中心开门失败
		pmy->title[0]=0x60; 	
		break;
	case 11://管理中心开门成功
		pmy->title[0]=0x61; 	
		break;	
	case 12://室内机开门失败
		pmy->title[0]=0x60; 	
		break;
	case 13://室内机开门成功
		pmy->title[0]=0x71; 	
		break;		
	default:
		pmy->title[0]=0x70;
		break;	
	}
	strcpy(&pmy->ip[32], pLog->card_id);
	send_completed(listenfd, (char*)pmy, packet_size);
	close( listenfd );
	free(pLog);
	return 1;
}

static int send_logcmd(LOG_PARAM_S* pLog, struct tcppacket* pPacket, DOOR_CMD_DT_S* pCMDDt)
{
	assert(pPacket);
	assert(pCMDDt);
	pPacket->state = 0x84;
	pCMDDt->reserve1 = (unsigned int)pLog;
	pCMDDt->port = di_get_log_port();
	strncpy(pCMDDt->address, di_get_server_ip(), 32);
	pTASK_PARAM pTask = malloc_task_param();
	if(!pTask)
	{
		free_params(0,(unsigned int)pPacket, (unsigned int)pCMDDt, 0);
		printf("ERROR: malloc_task_param fialed\n");
		return -1;
	}
	pTask->Data1 = (unsigned int)pPacket;
	pTask->Data2 = (unsigned int)pCMDDt;
	pTask->post_processing = (task_callback)free_params;
	pTask->processing = (task_processing)nw_sendlog;
	put_task_by_param(pTask);
	return 0;
}

void dr_log_read_card(int result, char* card_id, char* room)
{
	int ret = 0;
	LOG_PARAM_S* pLog = NULL;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	if(0 == g_log_enable){
		return;
	}
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return;
	}
	pLog = (LOG_PARAM_S*)malloc(sizeof(LOG_PARAM_S));
	if(NULL == pLog)
	{
		free_params(0,(unsigned int)pPacket,(unsigned int)pCMDDt,0);
		return;
	}
	pLog->arg = 3;
	if(0 == result){
		pLog->arg = 2;
	}
	strcpy(pLog->card_id, card_id);
	strcpy(pLog->room, room);
	send_logcmd(pLog, pPacket, pCMDDt);
	return;
}

//呼叫室内机日志
void dr_log_call(int result, char* room, char* ztid)
{
	int ret = 0;
	LOG_PARAM_S* pLog = NULL;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	if(0 == g_log_enable){
		return;
	}
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return;
	}
	pLog = (LOG_PARAM_S*)malloc(sizeof(LOG_PARAM_S));
	if(NULL == pLog)
	{
		free_params(0,(unsigned int)pPacket,(unsigned int)pCMDDt,0);
		return;
	}
	pLog->arg = 1;
	if(0 == result){    //  LOG_OPRATE_FAILED = 0
		pLog->arg = 0;
	}
	//呼叫管理中心
	if(0 == strcmp(room, "001")){
		pLog->arg = 5;
		if(0 == result){
			pLog->arg = 4;
		}
	}
	strcpy(pLog->ztid, ztid);
	strcpy(pLog->room, room);
	
	send_logcmd(pLog, pPacket, pCMDDt);
	return;
}

//管理中心开门
void dr_log_center_open(int result)
{
	int ret = 0;
	LOG_PARAM_S* pLog = NULL;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	if(0 == g_log_enable){
		return;
	}
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return;
	}
	pLog = (LOG_PARAM_S*)malloc(sizeof(LOG_PARAM_S));
	if(NULL == pLog)
	{
		free_params(0,(unsigned int)pPacket,(unsigned int)pCMDDt,0);
		return;
	}
	pLog->arg = 11;
	if(0 == result){
		pLog->arg = 10;
	}
	send_logcmd(pLog, pPacket, pCMDDt);
	return;
}

//室内机开门
void dr_log_room_open(int result, char* room)
{
	int ret = 0;
	LOG_PARAM_S* pLog = NULL;
	struct tcppacket* pPacket = NULL;
	DOOR_CMD_DT_S* pCMDDt = NULL;
	if(0 == g_log_enable){
		return;
	}
	ret = generate_params( &pPacket, &pCMDDt);
	if(0 != ret){
		return;
	}
	pLog = (LOG_PARAM_S*)malloc(sizeof(LOG_PARAM_S));
	if(NULL == pLog)
	{
		free_params(0,(unsigned int)pPacket,(unsigned int)pCMDDt,0);
		return;
	}
	pLog->arg = 13;
	if(0 == result){
		pLog->arg = 12;
	}
	strcpy(pLog->room, room);
	send_logcmd(pLog, pPacket, pCMDDt);
	return;
}

//end
