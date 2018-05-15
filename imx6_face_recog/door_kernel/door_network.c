/***implement door network****/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>      // Error number definitions 
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
#include "door_network.h"
#include "taskpool.h"
#include "door_info.h"
#include "door_log.h"

#define SERVER_PORT 10002

static NET_RESPONES_CK g_callback = NULL;

int send_completed(int fd, char* send_buf, int len)
{
	int cur_len;
	int sended = 0;
	while(sended != len){
		cur_len = send(fd, (send_buf + sended),len - sended, 0);
		sended += cur_len;
	}
	return sended;
}

int recv_completed(int fd, char* recv_buf, int len)
{
	int cur_len;
	int recved = 0;
	while(recved != len){
		cur_len = recv(fd, recv_buf + recved, len - recved,0);
		if(0 == cur_len){
			break;
		}
		recved += cur_len;
	}
	return recved;
}

int nw_sendtcpcom(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int ret, len, packet_size;
	int recv_start_flg, continue_flg;
	int error = -1;
	int listenfd;
	struct timeval tm;
	struct sockaddr_in servaddr;
	struct tcppacket * pmy = (struct tcppacket *)Data1;
	DOOR_CMD_DT_S* pCMDDt = (DOOR_CMD_DT_S*)Data2;
	fd_set set;
	ret = 0;
	len=sizeof(int);
	
	printf("INFO: sendtcpcom(%s, 0x%x)\n", pCMDDt->address, (int)pmy->state);

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
		dr_log_enable(0);
		printf("WARNING: Cannot Connect!\n");
		return 0;
	}
	printf("INFO: Connected!\n");
	dr_log_enable(1);

	packet_size = sizeof(struct tcppacket);

	recv_start_flg = 0;

	strcpy(pmy->ip,di_get_door_ip());//  第一次发送本机IP地址给服务器

	send_completed(listenfd, (char*)pmy, packet_size);

	len = recv_completed(listenfd, (char*)pmy, packet_size);
	

	if(len != packet_size){
		close( listenfd );
		if(pCMDDt->net_respones_ck){
			pCMDDt->net_respones_ck(DR_NET_DISCONNECTED,pmy, pCMDDt);
		}
		printf("WARNING: recv data error!\n");
		return 0;
	}

	//printf("1  \n");
	
	/*有些命令发一个pmy包的数据还不够，可能要发多个(如:获取室内机IP列表)，
	 *但是协议中没有明确说明要发送多少数据，什么时候发送完成，
	 *因此现在只能认为strlen(pmy->ip) < 1024 and pmy->ip[1023] != 0时，
	 *数据没有发送完，还需继续接收。*/
	 /*太奇怪的想法了，但是又能怎么办呢!!!*/
	do{
		continue_flg = 0;
		//printf(" sendtcpcom here-0x%x--%d-1\n",pmy->state, pmy->ip[1023]);
		//printf(" %d \n", strlen(pmy->ip));
		if((pmy->ip[1023] != '\0')&&(strlen(pmy->ip) > 1024)){
			printf(" sendtcpcom here---2\n");
			continue_flg = 1;
			if(0 == recv_start_flg){
				recv_start_flg = 1;
				if(pCMDDt->net_respones_ck){
					pCMDDt->net_respones_ck(DR_RECV_START, pmy, pCMDDt);
				}
			}else{
				if(pCMDDt->net_respones_ck){
					pCMDDt->net_respones_ck(DR_RECV_CONTINUE, pmy, pCMDDt);
				}
			}
			len = recv_completed(listenfd, (char*)pmy, packet_size);
			if(0 == len){
				//防止服务端发送的my.ip没有初始化0；
				close( listenfd );
				if(pCMDDt->net_respones_ck){
					pCMDDt->net_respones_ck(DR_RECV_COMPLETED, pmy, pCMDDt);
				}
				printf("WARNING: recv data my.ip do not memeset!\n");
				return 0;
			}
			printf(" sendtcpcom here---3\n");
		}
	}while(1 == continue_flg);

	//printf("2  \n");

	if(pCMDDt->net_respones_ck){
		pCMDDt->net_respones_ck(DR_RECV_COMPLETED, pmy, pCMDDt);
	}
	close( listenfd );
	//printf("3  \n");
	return 1;
}


static int control_recvtcp(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int ret;
	int connfd;
	struct tcppacket my;  //only used in ControlThread
	connfd = (int)Data1;
	ret = recv_completed(connfd,(char*)&my, sizeof(my));
	printf("INFO: recieved=%d, cmd=0x%x, ip=%s\n",ret, my.state, my.ip);
	// 时间同步改用向服务器主动申请	
	// 分机发同步信号，要求同步时间，门口机接收后发时间同步包
	if(ret==sizeof(my))
	{
		if(g_callback){
			ret = g_callback(DR_RECV_COMPLETED, &my, 0);
			if(1 == ret){
				send_completed(connfd,(char*)&my, sizeof(my));
				//usleep(80000);
			}
		}
	}
	close(connfd);
	dr_log_enable(1);
	return 1;
}

static void ControlThread(void* param)
{
    int listenfd, connfd;
    socklen_t sin_size;
    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;
	int reuse = 1;
	pTASK_PARAM pParam = NULL;

    listenfd=socket(AF_INET,SOCK_STREAM,0);
    if(listenfd<0){
		printf("ERROR: socket failed\n");
		return;
    }
	
    memset(&servaddr,'\0',sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port=htons(SERVER_PORT);
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));//设置套接字属性为重用bind地址
    if(bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr))<0){
		printf("ERROR: bind failed\n");
		return;
    }
    if(listen(listenfd,10)<0){
		printf("ERROR: Listen failed\n");
		return;
    }
	
    sin_size=sizeof(struct sockaddr);
    printf("INFO: TCP server is running\n");
    for(;;)
    {
		connfd=accept(listenfd, (struct sockaddr *)&clientaddr, &sin_size);
		if(connfd > 0){
			//分配任务给这条链接
			pParam = malloc_task_param();
			if(!pParam)
			{
				printf("ERROR: malloc_task_param fialed\n");
				continue;
			}
			pParam->Data1 = (unsigned int)connfd;
			pParam->processing = (task_processing)control_recvtcp;
			put_task_by_param(pParam);
		}else{
			printf("WARNING: accept fialed\n");
		}
    }
	return;
}

int nw_init(NET_RESPONES_CK callback)
{
	int ret;
	pthread_t pControl = NULL;
	g_callback = callback;
	ret=pthread_create(&pControl,NULL,(void*)ControlThread,NULL);
    if(ret!=0)
    {
		printf ("ERROR: Create ControlThread error!\n");
		return -1;
    }
	pthread_detach(pControl);
    printf("INFO: ControlThread created successfully!\n");
	return 0;
}

void nw_uninit()
{
	return;
}

//end

