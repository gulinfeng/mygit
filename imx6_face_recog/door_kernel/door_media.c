/***implement door media***/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/statfs.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <netinet/in.h>
#include "door_media.h"
#include "door_info.h"
#include "platform_ioctl.h"
#include "libAudio.h"
#include "door_elevator.h"
//#include "gst_video.h"
#include "../sdl_door.h"


//#define DOOR_TEST_VIDEO
//#define DOOR_GST_VIDEO
//#define DOOR_FR_VIDEO

#if ((defined IMX6_CLOCK) || (defined S500_CLOCK))  
	#define DOOR_GST_VIDEO
	#include "gst_video.h"
#elif ((defined IMX6_FR) || (defined S500_FR))  
	#define DOOR_FR_VIDEO
	#include "../videos/gst_video.h"
#endif

/*
#ifdef DOOR_GST_VIDEO
#include "gst_video.h"
#endif

#ifdef DOOR_FR_VIDEO
#include "../videos/gst_video.h"
#endif
*/



#define AUDIO_SEND_PORT 6810
#define AUDIO_RECV_PORT 6811 
#define BUF_SIZE  256  //1024  //320 for gsm
#define RECIEVE_TIMEOUT 3

#define SOUND_WELCOME   "./wavs-16k/welcome.wav"
#define SOUND_ERROR     "./wavs-16k/err.wav"
#define SOUND_MAIN     "./wavs-16k/main.wav"
#define SOUND_DIAL     "./wavs-16k/dial.wav"
#define SOUND_BUSY     "./wavs-16k/busy.wav"

#define SOUND_1   "./wavs-16k/sd1.wav"     // ??????????????????????????????????????????????????????????
#define SOUND_2    "./wavs-16k/sd2.wav"             // ????????
#define SOUND_3    "./wavs-16k/sd3.wav"    // ??????
#define SOUND_4    "./wavs-16k/sd4.wav"    // ??????????????????
#define SOUND_5    "./wavs-16k/sd5.wav"    // ????????
#define SOUND_6    "./wavs-16k/sd6.wav"    // ??????????
#define SOUND_7    "./wavs-16k/sd7.wav"    // ????????
#define SOUND_8    "./wavs-16k/sd8.wav"    // ??????????
#define SOUND_9    "./wavs-16k/sd9.wav"    // ???????????????g?
#define SOUND_10    "./wavs-16k/sd10.wav"    // ???????????????????????????
#define SOUND_11    "./wavs-16k/sd11.wav"    // ????????????
#define SOUND_12    "./wavs-16k/sd12.wav"    // ????????????????

#define SOUND_13    "./wavs-16k/sd13.wav"    // 
#define SOUND_14    "./wavs-16k/sd14.wav"    //
#define SOUND_15    "./wavs-16k/sd15.wav"    //
#define SOUND_16    "./wavs-16k/sd16.wav"    //


static unsigned int g_sound_sample[20] = {0};

typedef struct audio_context
{
	unsigned short send_port;
	unsigned short recv_port;
	int abort;
	pthread_t pd_encode;
	pthread_t pd_decode;
	char*  remote_ip[32];
	DOOR_CALLBACK callback;
}AUDIO_CONTEXT_S;

typedef struct video_context
{
	char*  remote_ip[32];
	unsigned short remote_port;
	DOOR_CALLBACK callback;
}VIDEO_CONTEXT_S;


static AUDIO_CONTEXT_S g_audio_ctx;
static VIDEO_CONTEXT_S g_video_ctx;
static DOOR_CALLBACK g_callback = NULL;


struct topacket{
	unsigned int seqnum;
	unsigned char buf[BUF_SIZE/2]; 
};

#ifdef DOOR_TEST_VIDEO
extern int playing_test_video(char* remote_ip);
extern int stop_test_video();
#endif

extern unsigned char linear2alaw(short pcm_val);
extern short alaw2linear(unsigned char u_val);

#if ((defined IMX6_CLOCK) || (defined IMX6_FR))  
	CircleBufferMngr *imx6_audio_write_buf = NULL;
	CircleBufferMngr *imx6_audio_read_buf = NULL;
#endif



static void AudioEncodeThread(void* param)
{       
    int ret=0,i=0;
    int sockfd=0;
	unsigned int Param1=0,Param2=0,Param3=0;
	short *pPcm =NULL;
    struct topacket my;
    struct sockaddr_in addr;
	AUDIO_CONTEXT_S* pContext = (AUDIO_CONTEXT_S*)param;
	//开始创建收发缓存
	ret = BegineWritingData(); 
	if(0 == ret)
    {	      
		sockfd = socket(AF_INET,SOCK_DGRAM,0);
		memset(&addr,'\0',sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(pContext->send_port);//atoi(port));
		inet_aton(pContext->remote_ip, &addr.sin_addr);
		while(1)
		{
			 pPcm = (short*)GetSendingData(&Param1, &Param2, &Param3);
	 		 if(pPcm)
	 		 {
	 		  	 my.seqnum = Param2;
				 for(i = 0; i < Param1; i++)
				 {
					 my.buf[i] = linear2alaw(pPcm[i]);
				 }
#if ((defined IMX6_CLOCK) || (defined IMX6_FR))  
				 free(pPcm);
				 //把mic读取到的编码后的数据放入 imx6_audio_read_buf 中
				 circleBufApi.cb_write(imx6_audio_read_buf, my.buf, Param1);
				 //判断当前数据是否为128个，有就发送出去
				 while(circleBufApi.cb_datalen(imx6_audio_read_buf) >= 128){
					circleBufApi.cb_read(imx6_audio_read_buf, my.buf, 128);
					sendto(sockfd, &my, sizeof(struct topacket), 0, (struct sockaddr *)&addr, sizeof(addr));
				 }
#elif ((defined S500_CLOCK) || (defined S500_FR))  
				sendto(sockfd, &my, sizeof(struct topacket),0,(struct sockaddr *)&addr,sizeof(addr));
	 		  	free(pPcm);
#endif
	 		 }
			 if(1 == pContext->abort){
			 	break;
			 }
        }
		//关闭收发缓存
		EndWritingData((QDataFree)free);
        close(sockfd);
		printf("INFO:  audioenc thread exit!\n");
    }
}
static void AudioDecodeThread(void* param)
{
	unsigned char buf_tmp[92]={0};
	int sockfd=0,addrlen=0;
    int ret=0,i=0,num=0;
	int counter=0;
	short* pPcm = NULL;
    int bytesample = BUF_SIZE;
	int readsample = (BUF_SIZE >> 1);
    struct sockaddr_in addr;
    struct topacket my;
	AUDIO_CONTEXT_S* pContext = (AUDIO_CONTEXT_S*)param;
    num = 0;  
	printf("INFO: AudioDecodeThread is running!\n");
	addrlen = sizeof(struct sockaddr_in);
    sockfd=socket(AF_INET,SOCK_DGRAM,0);
	memset(&addr,'\0',sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(pContext->recv_port);
    bind(sockfd,(struct sockaddr *)&addr, sizeof(addr));   
	struct timeval timeout;
 	fd_set fs_read;
	

	counter = 0;
    while(1)
    {
    	FD_ZERO(&fs_read);
	 	FD_SET(sockfd, &fs_read);
	 	timeout.tv_sec=1;
	 	timeout.tv_usec=0;
	 	int ret = select((sockfd+1),&fs_read,NULL,NULL,&timeout);
	 	//printf("begine recvfrom-\n");
	 	if(EINTR == ret)
		{
			perror("WARNING: select EINTR");
			usleep(150000);
			counter++;
			if(counter >= RECIEVE_TIMEOUT){
				pContext->abort = 1;
				break;
			}
		}
		if(ret < 0)
		{
			perror("WARNING: select fialed-");
			usleep(150000);
			counter++;
			if(counter >= RECIEVE_TIMEOUT){
				pContext->abort = 1;
				break;
			}
		}
		if(0 == ret)
		{
			printf("WARNING: select timeout-!!\n");
			//pthread_testcancel();
			counter++;
			if(counter >= RECIEVE_TIMEOUT){
				pContext->abort = 1;
				break;
			}
		}
		if(1 == pContext->abort){
		 	break;
		}
		if(FD_ISSET(sockfd, &fs_read))
		{
			ret = recvfrom(sockfd,&my,sizeof(struct topacket),0,(struct sockaddr *)&addr,(socklen_t*)&addrlen);
			if(ret<=0) {
				printf("EEROR: recv error!\n");
				pContext->abort = 1;
				break;
			}
			if(1 == IsInitialedQueue())
			{
#if ((defined IMX6_CLOCK) || (defined IMX6_FR))  
				circleBufApi.cb_write(imx6_audio_write_buf, my.buf, 128);
				while(circleBufApi.cb_datalen(imx6_audio_write_buf) >= 92){
					memset(buf_tmp, 0, 92);//每次取值前先清零
					circleBufApi.cb_read(imx6_audio_write_buf, buf_tmp, 92);
					pPcm = (short*)malloc(92*2);
					if(NULL == pPcm){
						usleep(10000);
						continue;
					}
					for (i = 0; i < 92; i++)//128次循环
						pPcm[i] =  alaw2linear(buf_tmp[i]);//解码
					if(1 == WritingData((unsigned int)pPcm, 92, my.seqnum)){
						printf("INFO: data queue is empty\n");
					}
				}
#elif ((defined S500_CLOCK) || (defined S500_FR))  
				pPcm = (short*)malloc(bytesample);
				if(NULL == pPcm)
				{
					usleep(10000);
					continue;
				}
				for (i = 0; i < readsample; i++)
					pPcm[i] =  alaw2linear(my.buf[i]);
				if(1 == WritingData((unsigned int)pPcm, readsample, my.seqnum))
				{
					printf("INFO: data queue is empty\n");
				}
#endif
			}
		}
		if(1 == pContext->abort){
		 	break;
		 }
    }	
    close(sockfd);
	
    printf("INFO: AudioDecodeThread exit!\n");
}


static int gst_video_msg_ck(int state, unsigned int param1,unsigned int param2)
{
	return 0;
}

int md_init(DOOR_CALLBACK callback)
{
	int ret = 0;
	InitAudio();
	memset(&g_audio_ctx,0,sizeof(AUDIO_CONTEXT_S));
	memset(&g_video_ctx,0,sizeof(VIDEO_CONTEXT_S));
	g_callback = callback;
	g_audio_ctx.send_port = AUDIO_SEND_PORT;
	g_audio_ctx.recv_port = AUDIO_RECV_PORT;
	SetPlayVol(TYPE_PLAY_RING, di_get_dialval());
	SetPlayVol(TYPE_PLAY_WAVE, di_get_talkval());
#ifdef DOOR_GST_VIDEO
	ret = init_gst_video(NULL, 1, di_get_video_width(), di_get_video_height());
	//使用 videotestsrc 测试
	//ret = init_gst_video(NULL, 0, di_get_video_width(), di_get_video_height());
	if (0 != ret){
	 	printf ("ERROR: init_gst_video error!\n");
		return ret;
	}
#endif
	g_sound_sample[0] = load_sound(SOUND_1);
	g_sound_sample[1] = load_sound(SOUND_2);
	g_sound_sample[2] = load_sound(SOUND_3);
	g_sound_sample[3] = load_sound(SOUND_4);
	g_sound_sample[4] = load_sound(SOUND_5);
	g_sound_sample[5] = load_sound(SOUND_6);
	g_sound_sample[6] = load_sound(SOUND_7);
	g_sound_sample[7] = load_sound(SOUND_8);
	g_sound_sample[8] = load_sound(SOUND_9);
	g_sound_sample[9] = load_sound(SOUND_10);
	g_sound_sample[10] = load_sound(SOUND_11);
	g_sound_sample[11] = load_sound(SOUND_12);
	g_sound_sample[12] = load_sound(SOUND_ERROR);
	g_sound_sample[13] = load_sound(SOUND_DIAL);
	g_sound_sample[14] = load_sound(SOUND_BUSY);
	g_sound_sample[15] = load_sound(SOUND_13);
	g_sound_sample[16] = load_sound(SOUND_14);
	g_sound_sample[17] = load_sound(SOUND_15);
	g_sound_sample[18] = load_sound(SOUND_16);
	g_sound_sample[19] = load_sound(SOUND_MAIN);//#号键提示
	return 0;
}

void md_unint()
{
	int i;
	for(i = 0; i < 20; i++){
		unload_sound(g_sound_sample[i]);
	}
	md_stop_audio_streaming();
	md_stop_video_streaming();
	UninitAudio();
#ifdef DOOR_GST_VIDEO
	uninit_gst_video();
#endif
	return;
}

void md_play_ring()
{
	return ;
}

void md_play_dial()
{
	return play_sound(g_sound_sample[13], 1, 1);
}

void md_play_busy()
{
	return play_sound(g_sound_sample[14], 1, 1);
}

void md_play_alarm()
{
	return ;
}

void md_play_welcome()
{
	play_sound(g_sound_sample[1], 0, 1);
}

void md_play_main()
{
	play_sound(g_sound_sample[19], 0, 0); 
}
void md_play_error()
{
	play_sound(g_sound_sample[12], 0, 1); 
}

void md_play_unrecogn()
{
	play_sound(g_sound_sample[2], 0, 0); 
}

void md_play(int index, int stop)
{
	if(index >=0 && index <= 18){
		play_sound(g_sound_sample[index], 0, stop); 
	}
}

void md_stop_play()
{
	play_sound(NULL, 0, 0);
	return;
}

int md_start_audio_streaming(char* remote_ip, DOOR_CALLBACK callback)
{
	int ret;
	md_stop_audio_streaming();
	printf("INFO: start audio stream\n");
	SetPlayVol(TYPE_PLAY_WAVE, di_get_talkval());

#if ((defined IMX6_CLOCK) || (defined IMX6_FR))  
	//audio cycle buffer 
	circleBufApi.cb_init(&imx6_audio_write_buf, BUF_SIZE*10);
	circleBufApi.cb_init(&imx6_audio_read_buf, BUF_SIZE*10);
	circleBufApi.cb_info(imx6_audio_write_buf, printf, 0);
	circleBufApi.cb_info(imx6_audio_read_buf, printf, 0);
#endif
	
	SetMicVal(di_get_micval());
	g_audio_ctx.abort = 0;
	g_audio_ctx.callback = callback;
	strcpy(g_audio_ctx.remote_ip, remote_ip);
	ret=pthread_create(&g_audio_ctx.pd_decode, NULL, (void*)AudioDecodeThread, &g_audio_ctx);
	if(ret!=0)
	{
		printf("ERROR: Create audio decode error!\n");
		return -1;
	}
	
    ret=pthread_create(&g_audio_ctx.pd_encode, NULL, (void*)AudioEncodeThread, &g_audio_ctx);
    if(ret!=0)
    {
		printf("ERROR: Create audio encode error!\n");
		return -2;
    }
    printf("INFO: AudioEncodeThread created successfully!\n");
	return 0;
}
static int g_ite_video_flg = 0;

int md_set_ite_video(int flg)
{
	g_ite_video_flg = flg;
}

int md_start_video_streaming(char* remote_ip, DOOR_CALLBACK callback)
{
	int ret = 0;
	printf("INFO: start video stream\n");
	enable_led();
#ifdef DOOR_TEST_VIDEO
	ret = playing_test_video(remote_ip);
#endif
	
#ifdef DOOR_GST_VIDEO
	 ret = playing_gst_video(remote_ip, g_ite_video_flg);
#endif

#ifdef DOOR_FR_VIDEO
	switch_send_streamer(remote_ip, g_ite_video_flg);
#endif

    return ret;

}

int md_stop_audio_streaming()
{
	AUDIO_CONTEXT_S* pContext = &g_audio_ctx;
	printf("INFO: stop audio stream\n");
	StopAllAudio();
	if(pContext->pd_decode){
		StopTalkImmediatly();
		pContext->abort = 1;
		pthread_join(pContext->pd_decode, NULL);
		pContext->pd_decode = NULL;
	}
	
	if(pContext->pd_encode){
		pContext->abort = 1;
		pthread_join(pContext->pd_encode, NULL);
		pContext->pd_encode = NULL;
	}
	Set_dial_vol();

#if ((defined IMX6_CLOCK) || (defined IMX6_FR))  
	circleBufApi.cb_deinit(&imx6_audio_write_buf);
	circleBufApi.cb_deinit(&imx6_audio_read_buf);
#endif
	return 0;
}

int md_stop_video_streaming()
{
	int ret = 0;
	printf("INFO: stop video stream\n");
	disable_led();
#ifdef DOOR_TEST_VIDEO
	ret = stop_test_video();
#endif

#ifdef DOOR_GST_VIDEO
	 ret = stop_gst_video();
#endif

#ifdef DOOR_FR_VIDEO
	end_sending_video();
#endif
	return ret;
}

//end
