/*********************************************************************************
  Copyright (C), 2017-2020, xigu Tech. Co., Ltd.
  File name   : gst_video.h
  Author      : zlg
  Version     : v1.0
  Date        : 2017/11/14
  Description : gstreamer video
  Others:
  Function List:

  History:
    1. 

*********************************************************************************/
#ifndef _GST_VIDEO_H
#define _GST_VIDEO_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vide_stata
{
	video_error,
	new_frame
}VIDEO_STATA;

typedef int (*GST_VIDEO_MSG_CALLBACK)(VIDEO_STATA state, unsigned int param1,unsigned int param2);


int init_gst_video(int video_w, int video_h, int screen_w, int screen_h);


void play_video();

void paused_video();

void stop_video();

void switch_send_streamer(char* remote_ip, int type);

void end_sending_video();

void switch_fr_streamer();

void set_video(SDL_Overlay* overlay, GST_VIDEO_MSG_CALLBACK callback);

int uninit_gst_video();

#ifdef __cplusplus
}
#endif



#endif//
