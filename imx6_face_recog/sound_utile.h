/*********************************************************************************
  Copyright (C), 2017-2020, xigu Tech. Co., Ltd.
  File name   : sound_utile.h
  Author      : zlg
  Version     : v1.0
  Date        : 2017/11/14
  Description : sound tools
  Others:
  Function List:

  History:
    1. 

*********************************************************************************/
#ifndef _SOUND_UTILE_H
#define _SOUND_UTILE_H

#ifdef __cplusplus
extern "C" {
#endif

int load_sounds();

void free_sounds();

void play_sound(int index, int flg);

/*The volume to use from 0 to MIX_MAX_VOLUME(128).*/
void set_volume(int volume);

#ifdef __cplusplus
}
#endif

#endif


//endif
