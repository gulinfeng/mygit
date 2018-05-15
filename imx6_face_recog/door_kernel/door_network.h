/*
* Copyright (c) 2017, 西谷数字
* All rights reserved.
*
* 文件名称：  door_network.h
* 文件标识：  
* 摘 要 门口机网络实现
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2017-12-5
*
*/
#ifndef DOOR_NETWORK_H
#define DOOR_NETWORK_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int nw_init(NET_RESPONES_CK callback);

void nw_uninit();

int nw_sendtcpcom(unsigned int Data1,  unsigned int Data2, unsigned int Data3);

#ifdef __cplusplus
}
#endif

#endif //

