/****implement card oporate *******/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "door_card.h"
#include "tx522.h"
#include "door_media.h"
#include "comm_utile.h"
#include "door_info.h"
#include "platform_ioctl.h"
#include "door_log.h"

#define READ_CARD_INTERVAL  100

static DOOR_CALLBACK g_dc_callback = NULL;
static pthread_t g_pcard = (pthread_t)NULL;
static int g_abort = 0;

void select_delay(int msecond)
{
	struct timeval tempval;
	tempval.tv_sec = msecond / 1000;;
    tempval.tv_usec = (msecond % 1000)*1000;
    select(0, NULL, NULL, NULL, &tempval);
	return;
}

static void card_read_error()
{
	md_play_error();
	if(g_dc_callback){
		g_dc_callback(DR_CARDREAD_ERROR, di_get_lock_authentication(), 0);
	}
	return;
}

extern int door_lock_open(int type, char* id, char* room);

static void card_control_thread(void* param)
{
	int ret, i;
	int uart = -1;
	int cardval=0;
	int timeout = 0;
	//unsigned char tagtype = 0;
	unsigned char sak = 0;
	unsigned char snr_len = 0;
	unsigned char cardtype[2]={0};
	unsigned char Card_Snr[8] = {0};
	unsigned char DataBfr[16] = {0};
	unsigned char info[16] = {0};
	char cardid[9]= {0};
	char room[9] = {};
	g_abort = 0;
	openTx522Power();
	printf("INFO: card_control_thread is running!\n");
	select_delay(800);
#if ((defined IMX6_FR) || (defined IMX6_CLOCK))  
	uart = comm_Init("/dev/ttymxc3",9600);
#elif ((defined S500_FR) || (defined S500_CLOCK))  
	uart = comm_Init("/dev/ttyS2",9600);
#endif
	if(uart < 0){
		printf("ERROR: card_control_thread exit: comm_Init error\n");
		return;
	}
	select_delay(READ_CARD_INTERVAL << 1);
	TX_GetInfo(uart, info);
	while(TX_LoadKey(uart) != COMM_OK)
	{
		select_delay(READ_CARD_INTERVAL << 1);
		if(1 == g_abort){
			break;
		}
		timeout++;
		if(timeout > 16){
			printf("ERROR: card is not connected\n");
			if(g_dc_callback){
				g_dc_callback(DR_SYS_ERROR, card_err, 0);
			}
			return;
		}
	}
	printf("INFO: LoadKey success\n");
	while(1){
		if(1 == g_abort){
			break;
		}
		if(TX_GetCardSnr(uart, 0x00, cardtype, &sak, &snr_len, Card_Snr)!= COMM_OK) {
			select_delay(READ_CARD_INTERVAL);
			continue;
		}
		memset(cardid,'\0',9);
		sprintf(cardid,"%.2x%.2x%.2x%.2x",Card_Snr[3],Card_Snr[2],Card_Snr[1],Card_Snr[0]);
		printf("INFO: cardid=%s\n",cardid);

		if(TX_ReadBlock(uart, 60, DataBfr)!= COMM_OK) {
			printf("WARNING: ReadBlock error ret=%d!\n",ret);
			select_delay(READ_CARD_INTERVAL);
			continue;
		}
		for(i=0;i<16;i++) {
			printf(" 0x%02x ", DataBfr[i]);
		}
		printf("\n");
		
		///ÅÐ¶ÏºÚÃûµ¥
		if(1 == di_is_black_user(cardid))
		{
			printf("INFO: %s is black user\n", cardid);
			card_read_error();
			TX_Halt(uart);
			select_delay(READ_CARD_INTERVAL);
			continue;
		}
		switch(DataBfr[0])
		{
		case 0x58:
			if(memcmp(DataBfr,di_get_sys_card(),6)==0){
				door_lock_open(0, cardid, "000");
			}else{
				card_read_error();
			}
			break;
			
		case 0x68:
			if(memcmp(DataBfr,di_get_user_card(),di_get_card_len())==0){
				ret=((DataBfr[5]/16)*10+DataBfr[5]%16)*100+(DataBfr[6]/16)*10+ (DataBfr[6]%16);
				sprintf(room,"%d",ret);  
				printf("INFO: roomnum=%s\n",room);
				door_lock_open(2, cardid, room);
				call_elevator(di_get_elevator_ip(), (unsigned int)DataBfr, NULL, 0, OPEN_DOOR_BY_CARD);
			
			}else{
				card_read_error();
			}
			break;	
			
		case 0x88:
			if(memcmp(DataBfr,di_get_mng_card(),6)==0){
				door_lock_open(1, cardid, "000");
			}else{
				card_read_error();
			}
			break;	
		default:
			card_read_error();
			break;
		}
		TX_Halt(uart);
		select_delay(50);
	}
	close(uart);
	printf("INFO: card_control_thread abort\n");
	return;
}

void dc_stop()
{
	g_abort = 1;
	pthread_join(g_pcard, NULL);
	g_pcard = NULL;
	g_dc_callback = NULL;
	return;
}

int dc_start(DOOR_CALLBACK callback)
{
	int ret;
	if(g_pcard){
		dc_stop();
	}
	g_dc_callback = callback;
	ret=pthread_create(&g_pcard, NULL, (void*)card_control_thread, 0);
	if(ret!=0)
	{
		printf("ERROR: Create card_control_thread error!\n");
		return -1;
	}
}

//end

