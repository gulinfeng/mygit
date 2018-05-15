/*
* Copyright (c) 2017, 西谷数字
* All rights reserved.
*
* 文件名称：  door_card.h
* 文件标识：  
* 摘 要 门口机刷卡模块接口
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2017-12-19
*
*/
#ifndef DOOR_LOG_H
#define DOOR_LOG_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_OPRATE_SUCCESS  1
#define LOG_OPRATE_FAILED   0

void dr_log_enable(int enable);
//刷卡日志
void dr_log_read_card(int result, char* card_id, char* room);


//呼叫日志
void dr_log_call(int result, char* room, char* ztid);

//管理中心开门
void dr_log_center_open(int result);

//室内机开门
void dr_log_room_open(int result, char* room);


#ifdef __cplusplus
}
#endif

#endif //DOOR_LOG_H

