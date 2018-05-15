/*
* Copyright (c) 2018, 西谷数字
* All rights reserved.
*
* 文件名称：  sdl_door.h
* 文件标识：  基于SDL的门口机UI
* 摘 要：     
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2018-1-09
*/

#ifndef _SDL_DOOR_H
#define _SDL_DOOR_H

#include "sdl_page.h"

#define VIDE_W  640
#define VIDE_H  480

#define OVERLAY_W  640//608 //608
#define OVERLAY_H  480//448 //448

#if ((defined IMX6_FR) || (defined S500_FR))  
	#define FTP_BACKUP
#endif

#ifdef __cplusplus
extern "C" {
#endif

//门口机event
#define MSG_NET_ERROR           (UE_USR_PAGE_EVENT + 1)	
#define MSG_CALL_CANCEL         (UE_USR_PAGE_EVENT + 2)
#define MSG_CALL_ENSTABLISHED    (UE_USR_PAGE_EVENT + 3)
#define MSG_CALL_DIALED          (UE_USR_PAGE_EVENT + 4)
#define MSG_CALL_BUSY            (UE_USR_PAGE_EVENT + 5)
#define MSG_CONNECTED_SERVER     (UE_USR_PAGE_EVENT + 6)
#define MSG_NO_ROOM              (UE_USR_PAGE_EVENT + 7)

//人脸识别event
#define UE_RECOGNIZATION        (UE_USR_PAGE_EVENT + 10)	
#define UE_USR_COMFIRMED_S      (UE_USR_PAGE_EVENT + 11)
#define UE_USR_COMFIRMED_F      (UE_USR_PAGE_EVENT + 12)
#define UE_USR_ENROLLING_IN     (UE_USR_PAGE_EVENT + 13)
#define UE_USR_ENROLLING_OUT    (UE_USR_PAGE_EVENT + 14)
#define UE_USR_ENROLL_AUTHEN    (UE_USR_PAGE_EVENT + 15)
#define UE_USR_ENROLLED         (UE_USR_PAGE_EVENT + 16)
#define UE_USR_RESET_LIB        (UE_USR_PAGE_EVENT + 17)
#define UE_USR_AUTHEN_FINISHED  (UE_USR_PAGE_EVENT + 18)

#define UE_USR_BEGIN_PREVIEW    (UE_USR_PAGE_EVENT + 20)
#define UE_USR_END_PREVIEW      (UE_USR_PAGE_EVENT + 21)

#define UE_USR_AUTHEN_SECCESS   (UE_USR_PAGE_EVENT + 22)
#define UE_USR_AUTHEN_FAILED    (UE_USR_PAGE_EVENT + 23)

#define UE_USR_WEAK_LIGHT       (UE_USR_PAGE_EVENT + 24)

//向服务器获取时间后产生的event
#define UE_USR_CLOCK_UPDATE       (UE_USR_PAGE_EVENT + 25)


int create_call_page(char input);
int create_main_menu_page(void);

void set_door_net_state(int isconnected);
void set_door_link_state(int linkstate);

int sdl_door_msg_process(SDL_Event* pEven);

int sdl_door_init(SDL_Surface* screen);

void sdl_door_uninit();


#ifdef __cplusplus
}
#endif

#endif //_SDL_DOOR_H

