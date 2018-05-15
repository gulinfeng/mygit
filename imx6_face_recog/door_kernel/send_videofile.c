/****implement send test video use file****/
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/statfs.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <netinet/in.h>
#include "door_info.h"

#define VIDEO_FILE "./videos/755.h264"

#define STREAMER_BUF_SIZE  320*1024

#define PORT_VIDEO_SERVER 6801

typedef struct streamer_buff
{
	char* pdata_end;
	char* pCursor;
	char* pNalu_begin;
	int   nalu_type;
	int   file_eos;
	char  frame_head[4];
	char  buf[STREAMER_BUF_SIZE + 16];
}STREAMER_BUF_S;

static void init_buffer(STREAMER_BUF_S* streamer)
{
	memset(streamer, 0, sizeof(STREAMER_BUF_S));
	streamer->pNalu_begin= NULL;
	streamer->pdata_end = streamer->buf;
	streamer->pCursor = streamer->buf;
	streamer->frame_head[0] = 0x0;
	streamer->frame_head[1] = 0x0;
	streamer->frame_head[2] = 0x0;
	streamer->frame_head[3] = 0x1;
	return;
}

static int feed_buffer(STREAMER_BUF_S* streamer, FILE* fp){
	int read = 0;
	int remain_len = streamer->buf + STREAMER_BUF_SIZE - streamer->pdata_end;
	read = fread(streamer->pdata_end, sizeof(char), remain_len, fp);
	if (read > 0) {
		//printf("TEST: read len=%d\n", read);
		streamer->pdata_end += read;
		}else {
		//文件读写结束
		streamer->file_eos = 1;
	 }
   
	return read;
}

static char* get_one_nalu(STREAMER_BUF_S* streamer, int* nalu_len, int* nalu_type){

	char* pData = NULL;
	char  nalu_head = 0;
	int   complete = 0;
	(*nalu_len) = 0;
	(*nalu_type) = 0;
	do {
		
		while(streamer->pCursor < streamer->pdata_end){
			if(0 == memcmp(streamer->pCursor,streamer->frame_head, 4)){
				if (NULL == streamer->pNalu_begin){
					//找到桢头
					//printf("find-------1-\n");
					streamer->pNalu_begin = streamer->pCursor;
					streamer->pCursor += 4;
					nalu_head = *(streamer->pCursor);
					streamer->nalu_type = nalu_head&0x1F;
					
				}else{
					//找到下一帧的桢头
					//printf("find complete nalu\n");
					complete = 1;
					//streamer->nal_len = streamer->pCursor - streamer->pNalu_begin;
					*nalu_len = streamer->pCursor - streamer->pNalu_begin;
					(*nalu_type) = streamer->nalu_type;
				}
				break;
			}
			streamer->pCursor++;
		}
		if (1 == complete){
			//读到一帧数据,返回。
			pData = streamer->pNalu_begin;
			streamer->pNalu_begin = NULL;
			break;
		}
		if(streamer->pCursor >= streamer->pdata_end){
			int len = 0;
			if(NULL != streamer->pNalu_begin){
				len = (streamer->pCursor - streamer->pNalu_begin);
				memmove(streamer->buf, streamer->pNalu_begin, len);
				streamer->pNalu_begin = streamer->buf;
			}
			streamer->pCursor = streamer->buf + len;
			streamer->pdata_end = streamer->buf + len;
			//没有数据
			return NULL;
		}
		
	}while(1);
	return pData;
}

static void delay(int msecond){
	struct timeval tempval;
	tempval.tv_sec = msecond / 1000;
    tempval.tv_usec = (msecond % 1000)*1000;
    select(0, NULL, NULL, NULL, &tempval);
	return;
}


static int g_abort = 0;
static pthread_t pd_encode = NULL;
static char g_cur_ip[32] = {0};
static int nMaxPackSize=50000;
static char sendbuffer[80*1024] = {0};
static int  g_Flg = 0;
static int g_seqnum = 0;

static int send_video_stream(int sockfd,  struct sockaddr_in* pclient_address, char* data, int size)
{
	int to_send;
	int sended = 0;
	int remain = size;
	int pack_seq = 0;
	g_seqnum++;
	sendbuffer[0]=0xFF;
	sendbuffer[1]=0x01;
	memcpy(&sendbuffer[6],&g_Flg,4);
	if(size > nMaxPackSize){
		sendbuffer[0]=0x00;
	}
	memcpy(&sendbuffer[2],&g_seqnum, 4);
	while(remain) {
		pack_seq++;
		sendbuffer[1] = pack_seq;
		to_send = remain < nMaxPackSize ? remain : nMaxPackSize;
		memcpy(&sendbuffer[10], (data + sended), to_send);
		sendto(sockfd, sendbuffer, to_send + 10, 0,(struct sockaddr *)pclient_address,sizeof(struct sockaddr_in));
		remain -= to_send;
		sended += to_send;
		if ((remain > 0) && (remain < nMaxPackSize)) {
			//封包结束
			sendbuffer[0]=0xFF;
		}
	}
	return sended;
}

static void VideoEncodeThread(void* param)
{       
    int ret,i;
    int sockfd;
	int width,height;
	FILE* fp;
	char* pNalu;
	int  NaluLen = 0;
	int nal_type = 0; 
    struct sockaddr_in client_address;
	STREAMER_BUF_S streamer;
	char *pRemIP = g_cur_ip;
	fp = fopen(VIDEO_FILE, "r+");
	if (fp == NULL){
		printf("ERROR:%s open failed\n",VIDEO_FILE);
		return;
	}
	printf("INFO: sending video file thread running\n");
	init_buffer(&streamer);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&client_address,'\0', sizeof(struct sockaddr_in));
	client_address.sin_family = AF_INET; //IP4
	client_address.sin_addr.s_addr = inet_addr(pRemIP);
	client_address.sin_port = htons(PORT_VIDEO_SERVER); //6801
	g_seqnum = 0;
	g_abort = 0;
	g_Flg = 0x19;
	width = 720;
	height = 288;

	if (width == 720 && height == 576){
		g_Flg = 0x14;
	}else if (width == 720 && height == 288){
		g_Flg = 0x13;
	}else if (width == 640 && height == 480){
		g_Flg = 0x18;
	}else if(width == 1280 && height == 720) {
		g_Flg = 0x19;
	}else if(width == 320 && height == 240) {
		g_Flg = 0x17;
	} 
	memset(sendbuffer,0,sizeof(sendbuffer));
	while(1)
	{
		pNalu = get_one_nalu(&streamer, &NaluLen, &nal_type);
		if (NULL == pNalu){
			if (1 == streamer.file_eos){
				//printf("INFO: play video loop---\n");
				fseek(fp,0,0);
				init_buffer(&streamer);
			}
			feed_buffer(&streamer, fp);
			
			if(1 == g_abort){
				printf("INFO: test video sending thread quit1\n");
				break;
			}
			continue;
		}
		send_video_stream(sockfd,&client_address, pNalu, NaluLen);
		if(1 == g_abort){
			printf("INFO: test video sending thread quit2\n");
			break;
		}
		delay(40);
		if(1 == g_abort){
			printf("INFO: test video sending thread quit3\n");
			break;
		}
	}
	fclose(fp);
	close(sockfd);
	printf("INFO:sending vidio exit\n");
	return;
}

int playing_test_video(char* remote_ip)
{
	int ret;
	stop_test_video();
	g_abort = 0;
	strcpy(g_cur_ip, remote_ip);
	ret=pthread_create(&pd_encode, NULL, (void*)VideoEncodeThread, 0);
    if(ret!=0)
    {
		printf("ERROR: Create audio encode error!\n");
		return -2;
    }
	return 0;
}

int stop_test_video()
{
	g_abort = 1;
	if(pd_encode){
		pthread_join(pd_encode, NULL);
		pd_encode = NULL;
	}
	return 0;
}

//end

