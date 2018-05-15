/*
* Copyright (c) 2017, 西谷数字
* All rights reserved.
*
* 文件名称：  door_signal.h
* 文件标识：  
* 摘 要 门口机私有信令实现
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2017-12-4
*
*/
#ifndef DOOR_SIGNAL_H
#define DOOR_SIGNAL_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Open_Door
{
	int  type;
	char infor1[16];   
	char infor2[16];
}OPEN_DR_INFO_S;
int door_start(DOOR_CALLBACK callback);

void door_stop();


//启动看门狗
int door_watchdog_enable();


//获取door当前stata
DR_STATA_E door_get_cur_stata();


//关闭所有，包括预览，对讲和留言
int door_close_every();

//呼叫电梯
int call_elevator(char* elevator_ip,  unsigned int data_buf, 
						DOOR_CALLBACK callback, unsigned int usr_data, 
						ELEVATOR_CARD_OR_NET open_flag);



/******门口机主动开门********/
int door_lock_open(int type, char* id, char* room);


//开始呼叫
/* room 房号 00为管理中心
*  callback 呼叫状态回调 
*/
int door_make_call(char* room, DOOR_CALLBACK callback, unsigned int usr_data);


/*
//应许预览
int door_allow_preview( DOOR_CALLBACK callback);
//取消预览
int door_cancel_preview();
*/

//开始留言
/* room 房号
*/
int door_make_message(char* room, DOOR_CALLBACK callback, unsigned int usr_data);
//取消留言
int door_cancel_message();

//
int door_get_time(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_room_ip_list(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_ic_cord(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_black_list(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_passwd(DOOR_CALLBACK callback, unsigned int usr_data);

int door_post_rest_passwd(char* room, DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_port_info(DOOR_CALLBACK callback, unsigned int usr_data);

int door_get_door_unit(DOOR_CALLBACK callback, unsigned int usr_data);

int door_send_remote_cmd(char* host, unsigned int cmd);
#ifdef __cplusplus
}
#endif

#endif //


