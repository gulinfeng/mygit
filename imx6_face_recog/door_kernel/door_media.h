/*
* Copyright (c) 2017, 西谷数字
* All rights reserved.
*
* 文件名称：  door_media.h
* 文件标识：  
* 摘 要 门口机流媒体实现
*
* 当前版本：1.1
* 作 者：zlg
* 完成日期：2017-12-4
*
*/
#ifndef DOOR_MEDIA_H
#define DOOR_MEDIA_H

#include "door_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int md_init(DOOR_CALLBACK callback);

void md_unint();

void md_play_ring();

void md_play_dial();

void md_play_busy();

void md_play_alarm();

void md_play_welcome();

void md_play_main();

void md_play_error();

void md_play_unrecogn();

void md_play(int index, int stop);
void md_stop_play();
int md_set_ite_video(int flg);

int md_start_audio_streaming(char* remote_ip, DOOR_CALLBACK callback);

int md_start_video_streaming(char* remote_ip, DOOR_CALLBACK callback);

int md_stop_audio_streaming();

int md_stop_video_streaming();


#ifdef __cplusplus
}
#endif

#endif //
