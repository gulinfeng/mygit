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
#ifndef DOOR_CARD_H
#define DOOR_CARD_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif


int dc_start(DOOR_CALLBACK callback);

void dc_stop();

#ifdef __cplusplus
}
#endif

#endif//

