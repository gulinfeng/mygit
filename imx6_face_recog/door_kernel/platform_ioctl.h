/*********************************************************************************
  Copyright (C), 2017-2020, xigu Tech. Co., Ltd.
  File name   : platform_ioctl.h
  Author      : zlg
  Version     : v1.0
  Date        : 2017/11/14
  Description : sound tools
  Others:
  Function List:

  History:
    1. 

*********************************************************************************/
#ifndef _PLATFORM_IOCTL_H
#define _PLATFORM_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

void EnableRecv();
void EnableSend();

int reset_lcd();

void enable_led();

void disable_led();

void openPowerAmplifier();
void closePowerAmplifier();

/**open door*/
int open_door();

int close_door();

int openTx522Power();

int is_weak_light();

void StartWatchdog();
void KeepAlive();
void StopWatchdog();

#ifdef __cplusplus
}
#endif

#endif

//endif
