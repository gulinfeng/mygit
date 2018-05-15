/**recognization main**/
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <time.h>
#include <string.h>
#include <SDL.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include "videos/gst_video.h"
#include "gui_utile.h"
#include "sdl_door.h"
#include "recog/face_recog.h"
#include "door_kernel/platform_ioctl.h"
#include "door_kernel/door_signal.h"
#include "door_kernel/door_info.h"
#include "door_kernel/door_log.h"
#ifdef FTP_BACKUP
#include "backup/fr_backup.h"
#endif

extern int create_face_recog_page(SDL_Surface* screen);
extern int create_opendoor_page(int result); 

static void quit(int rc)
{
	printf("----------quit-----rc=%d-\n", rc);
	uninit_gst_video();
	door_stop();
	sdl_door_uninit();
	SDL_Quit();
	exit(rc);
}

SDL_Surface* init_sdl_render()
{
	SDL_Surface* g_screen = NULL;
	if ( SDL_Init(SDL_INIT_VIDEO  ) < 0 ) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		return(1);
	}
	/* Initialize the display */
#if ((defined S500_FR) || (defined S500_CLOCK))  
	g_screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
#elif ((defined IMX6_FR) || (defined IMX6_CLOCK))  
	g_screen = SDL_SetVideoMode(800, 480, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
#endif

	if ( g_screen == NULL ) {
		printf("Couldn't set video mode: %s\n",SDL_GetError());
		quit(1);
	}
	printf("Set%s %dx%dx%d mode\n",
			g_screen->flags & SDL_FULLSCREEN ? " fullscreen" : "",
			g_screen->w, g_screen->h, g_screen->format->BitsPerPixel);
	printf("(video surface located in %s memory)\n",
			(g_screen->flags&SDL_HWSURFACE) ? "video" : "system");
	if ( g_screen->flags & SDL_DOUBLEBUF ) {
		printf("Double-buffering enabled\n");
	}

	return g_screen;
}

void SetNetWork()//设置网络
{
	char str[128];
	memset(str,0,128);
	sprintf(str,"ifconfig eth0 %s netmask %s",di_get_door_ip(),di_get_subnet_mast());
	system(str);
	memset(str,0,128);
	sprintf(str,"route add default gw %s",di_get_gateway());
	system(str);
}
#ifdef FTP_BACKUP
static int ftp_update_local_data(unsigned int Data1,  unsigned int Data2, unsigned int Data3)
{
	int ret = 0;
	if(strlen(di_get_door_unit()) > 0){
		ret = ftp_update_local(di_get_door_unit());
		if(ret != 0 ){
			printf("WARNNING: ftp update local data failed\n");
		}else{
			send_page_event(UE_USR_RESET_LIB, (unsigned int)get_main_page());
		}
	}
	return 0;
}
#endif

static int sys_door_callback(int state, unsigned int param, unsigned int usr_data)
{
	if(DR_BE_PREVIEW == state){
		printf("DOOR_CB: BE_PREVIEW--\n");
		send_page_event(UE_USR_BEGIN_PREVIEW, (unsigned int)current_page());
		
	}else if(DR_PREVIEW_CANCELED == state){
		printf("DOOR_CB: PREVIEW CANCELED--\n");
		send_page_event(UE_USR_END_PREVIEW, (unsigned int)current_page());
	}
	else if(DR_LOCK_OPENED == state){
		printf("DOOR_CB: opened\n");
		OPEN_DR_INFO_S* pInfo = (OPEN_DR_INFO_S*)param;
		if(0 == (int)di_get_lock_authentication()){
			create_opendoor_page(1);
			dr_log_read_card(LOG_OPRATE_SUCCESS,(char*)pInfo->infor1,(char*)pInfo->infor2);
		}else{
			PAGA_S *pPage = current_page();
			pPage->usr_data[7] = pInfo->type; //刷卡类型  0->58卡,  1->88卡， 2->用户卡
			strcpy(pPage->usr_str1, (char*)pInfo->infor1); // 卡号
			strcpy(pPage->usr_str2, (char*)pInfo->infor2); // 房号
			send_page_event(UE_USR_AUTHEN_SECCESS, (unsigned int)pPage);
			//printf("DOOR_CB: UE_USR_AUTHEN_SECCESS----\n");
		}
		
	}else if(DR_LOCK_CLOSED== state){
		printf("DOOR_CB: closed\n");
		
	}else if(DR_CARDREAD_ERROR == state){
		printf("DOOR_CB: card read error\n");
		if(0 == (int)param){
			create_opendoor_page(0);
		}else{
			send_page_event(UE_USR_AUTHEN_FAILED, (unsigned int)current_page());
		}
	}else if(DR_FINISH_LOAD_CFG == state){
		SetNetWork();
		usleep(150000);
}else if(DR_NOTIFY_FR_UPDATE == state){
		printf("DOOR_CB: notify face data update\n");
#ifdef FTP_BACKUP
		do_task(ftp_update_local_data, 0, 0);
#endif
	}
	return 0;
}

// return value:
// -1 -- error , details can check errno
// 1 -- interface link up
// 0 -- interface link down.
int GetNetlinkStatus()
{
    int skfd;
    struct ifreq ifr;
    struct ethtool_value edata;
	
    edata.cmd = ETHTOOL_GLINK;
    edata.data = 0;
	
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", sizeof(ifr.ifr_name) - 1);
    ifr.ifr_data = (char *) &edata;
	
    if (( skfd = socket( AF_INET, SOCK_DGRAM, 0 )) < 0)
        return -1;
	
    if(ioctl( skfd, SIOCETHTOOL, &ifr ) == -1)
    {
        close(skfd);
        return -1;
    }
	
    close(skfd);
    return (edata.data > 0 ? 1 : 0);

}


#define FETCH_TIME_INTERVAL  300

#ifdef FTP_BACKUP
static int get_door_unit_callback(int state, unsigned int param, unsigned int usr_data)
{
	int ret;
	char* dr_unit = (char*)param;
	di_set_door_unit("");
	di_set_ip_reported(0);
	if(state == DR_OPERATE_SUCCESS){
		if(strlen(dr_unit) > 0) {
			di_set_door_unit(dr_unit);
			// report ip 
			ret = ftp_report_deviceip(dr_unit, di_get_door_ip());
			if(0 == ret){
				di_set_ip_reported(1);
			}
		}else{
			printf("ERROR: recieved door unit is NULL\n");
		}
	}
}
#endif
static int get_time_callback(int state, unsigned int param, unsigned int usr_data)
{
	//HWND hWnd = (HWND) usr_data;
	int ret;
	set_door_net_state(0);
	if(state == DR_OPERATE_SUCCESS){
		set_door_net_state(1);
		//send event to update clock time
		//send_clock_page_event(UE_USR_CLOCK_UPDATE);
#ifdef FTP_BACKUP
		if(strlen(di_get_door_unit()) <= 0 ){
			//获取单元ID
			door_get_door_unit(get_door_unit_callback, 0);
		}else{
			ret = ftp_report_deviceip(di_get_door_unit(), di_get_door_ip());
			if(0 == ret){
				di_set_ip_reported(1);
			}
		}
#endif
		
	}
	
	return 0;
}
static int g_begin_detect = 0;

static int begin_detect_ck(int id, int interval)
{
	g_begin_detect = 1;
	return 0;
}

static int fetch_once_times(int id, int interval)
{
	int linkstatus = GetNetlinkStatus(); 
	// 向服务器发时间同步包，要求同步时间
	if(linkstatus)
	{
		set_door_link_state(1);
		door_get_time(get_time_callback,0);
	}else{
		dr_log_enable(0);
		set_door_link_state(0);
		return 1;
	}
	return 0;
}

extern void set_weak_light_flg(int flg);

//UE_USR_WEAK_LIGHT
static void watch_weak_light(unsigned long long int cur_tm)
{
#if ((defined IMX6_FR) || (defined S500_FR))  
	static unsigned long long int last_update = 0;
	static int detect_weak_counter = 0;
	static int is_weak = 0;
	if(0 == g_begin_detect){
		return;
	}
	if(cur_tm - last_update >= 400){
		last_update = cur_tm;
		// 连续3次检测到低照度就为低照度模式
		if(1 == is_weak_light()){
			//printf("DEBUG: weak light--------------------\n");
			if(0 == is_weak){
				detect_weak_counter--;
			}else{
				detect_weak_counter = 0;
			}
			if(detect_weak_counter <= -2){
				is_weak = 1;
				set_weak_light_flg(1);
				send_page_event(UE_USR_WEAK_LIGHT, (unsigned int)current_page());
			}
		}else{
			//printf("DEBUG: bright light--------------------\n");
			if(1 == is_weak){
				detect_weak_counter++;
			}else{
				detect_weak_counter = 0;
			}
			if(detect_weak_counter >= 3){
				is_weak = 0;
				set_weak_light_flg(0);
				send_page_event(UE_USR_WEAK_LIGHT, (unsigned int)current_page());
			}
		}
	}
#endif
}

int main(int argc, char **argv)
{
	int ret = 0;
	int random_tm;
	unsigned long long int cur_tm = 0;
	unsigned long long int last_update = 0;
	char user[32] = {0};
	char passwd[32] = {0};
	time_t last_link  =0;
	SDL_Event event;
	const SDL_VideoInfo *info;
	SDL_Surface* screen;
	int linkstatus;

#ifdef IMX6_FR
	printf("----- IMX6_FR -----\n");

#endif

#ifdef IMX6_CLOCK
	printf("----- IMX6_CLOCK -----\n");
#endif

#ifdef S500_FR
	printf("----- S500_FR -----\n");
#endif
	
#ifdef S500_CLOCK
	printf("----- S500_CLOCK -----\n");
#endif

#if ((defined IMX6_FR) || (defined S500_FR))  
    printf("----- xxxxxxxxxxxxxx -----\n");
#endif 

	//重置LCD屏
	reset_lcd();
	
	//置功放有效
	openPowerAmplifier();
	usleep(80000);
	screen = init_sdl_render();
	if(NULL == screen){
		quit(3);
	}
	info = SDL_GetVideoInfo();
	printf("Current display: %dx%d, %d bits-per-pixel\n", \
		info->current_w, info->current_h, info->vfmt->BitsPerPixel);
	if ( info->vfmt->palette == NULL ) {
		printf("	Red Mask = 0x%.8x\n", info->vfmt->Rmask);
		printf("	Green Mask = 0x%.8x\n", info->vfmt->Gmask);
		printf("	Blue Mask = 0x%.8x\n", info->vfmt->Bmask);
	}

	SDL_ShowCursor(0);

	ret = sdl_door_init(screen);
	if(0 != ret){
		quit(4);
	}
	
#if ((defined IMX6_FR) || (defined S500_FR))  
	init_gst_video(VIDE_W, VIDE_H, OVERLAY_W, OVERLAY_H);
#endif

	door_start(sys_door_callback);

#ifdef FTP_BACKUP
	di_get_ftp_config(user, passwd);
	ftp_set_config(di_get_ftp_host(), user, passwd, FTP_PATH);
#endif

#if ((defined IMX6_FR) || (defined S500_FR))  
	ret = fr_init(OVERLAY_W, OVERLAY_H, di_get_recogn_mode(), 0);
	if(0 != ret){
		draw_licence_error(0);
		SDL_Flip(screen);
		SDL_Delay(5000);
		quit(6);
	}
	if(0 != fr_setmode(FRMODE_RECOGNIZE)){
		printf("ERROR: fr_setmode fialed\n");
		quit(7);
	}
#endif

	if(1 == di_get_watch_dog()){
		//打开看门狗
		door_watchdog_enable();
	}
	if(1 == di_get_carused()) {
		//刷卡模块启动
		dc_start(sys_door_callback);
	}
	
#if ((defined IMX6_CLOCK) || (defined S500_CLOCK))  
	create_clock_page(screen);
#elif ((defined IMX6_FR) || (defined S500_FR))  
	create_face_recog_page(screen);
#endif
	
	time(&last_update);
	last_link = last_update;
	srand((int)last_update);
	random_tm = (20 + rand() % 120);
	task_timer((random_tm*100), fetch_once_times);

	task_timer(3000, begin_detect_ck);

	/* Loop, waiting for QUIT or RESIZE */
    while (1)
    {
    	cur_tm = timer_now();
#if ((defined IMX6_FR) || (defined S500_FR))  
		watch_weak_light(cur_tm);
		if(cur_tm - last_update >= 1000){
			last_update = cur_tm;
			linkstatus = GetNetlinkStatus();
			if(linkstatus)
			{
				set_door_link_state(1);
				srand((int)cur_tm);
				random_tm = rand()% 600;
				if(cur_tm - last_update > FETCH_TIME_INTERVAL + random_tm){
					last_update = cur_tm;
					door_get_time(get_time_callback,0);
				}
			}else{
				dr_log_enable(0);
				set_door_link_state(0);
				set_door_net_state(0);
			}
		}
#endif
		
        if (SDL_WaitEvent(&event))
        {
            switch (event.type)
            {
            	case SDL_QUIT:
                     quit(0);
				default:
					sdl_door_msg_process(&event);
					break;
            }
			
        }
		
    }
	quit(0);
	return 0;
}

//endif
