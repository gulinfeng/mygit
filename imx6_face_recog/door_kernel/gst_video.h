/*
* Copyright (c) 2013, ��������
* All rights reserved.
*
* �ļ����ƣ�  gst_video.h
* �ļ���ʶ��  ��Ƶ������
* ժ Ҫ��     
*
* ��ǰ�汾��1.1
* �� �ߣ�zlg
* ������ڣ�2017-4-17
*/

#ifndef _GSTREAMER_VIDEO_H
#define _GSTREAMER_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*GST_VIDEO_MSG_CALLBACK)(int state, unsigned int param1,unsigned int param2);

typedef int (*GST_VIDEO_PROVIDE_CB)(char* video_data, int size, unsigned int user_data);

int init_gst_video(GST_VIDEO_MSG_CALLBACK callback, int capture_flg, int width, int height);
// 0 ��ͨlinuix���ڻ� 1 ΪITE���ڻ�
int playing_gst_video(char* remote_ip, int type);

int stop_gst_video();

int uninit_gst_video();


#ifdef __cplusplus
}
#endif


#endif //_GSTREAMER_VIDEO_H

