/*
* Copyright (c) 2017, ��������
* All rights reserved.
*
* �ļ����ƣ�  door_network.h
* �ļ���ʶ��  
* ժ Ҫ �ſڻ�����ʵ��
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2017-12-5
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

