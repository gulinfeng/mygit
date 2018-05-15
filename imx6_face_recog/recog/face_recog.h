/*********************************************************************************
  Copyright (C), 2017-2020, xigu Tech. Co., Ltd.
  File name   : face_recog.h
  Author      : zlg
  Version     : v1.0
  Date        : 2017/11/14
  Description : ÈËÁ³Ê¶±ðdemo
  Others:
  Function List:

  History:
    1. 

*********************************************************************************/
#ifndef _FACE_RECOG_H
#define _FACE_RECOG_H

#include "FRLibraryTypes.h"



#ifdef __cplusplus
extern "C" {
#endif


typedef enum fr_stata
{
	FR_NULL,
	FR_Recognized,               
	FR_Enrolled,
	FR_FaceComfirmed
}FR_STATA;

typedef struct fr_result
{
	FR_STATA stata;
	struct FRIDList* pDList;
	unsigned int reserved1;
	unsigned int reserved2;
}FR_RESULT;

int fr_init(int frame_w, int frame_h, int spoof, int debug);

void fr_uninit();

void fr_set_enroll_param(int interval, float cfd_thresh, int times);

void fr_set_recognize_param(int interval, float cfd_thresh, int times);

int fr_setmode(enum FRMode eFrMode);

void fr_set_recog_mode(int mode);

//enum FRMode fr_getmode();

int fr_get_enroll_times();

void fr_reduce_recog_counter();

void fr_init_encolling();

void fr_init_recognization();


int fr_enroll(unsigned char *pFrame, unsigned long lRecordID,  FR_RESULT* pResult);

int fr_recognize(unsigned char *pFrame,  FR_RESULT* pResult);


int fr_add_face(unsigned long* pRecordID, const char* firstName, const char* secondName);

int fr_remove_face(unsigned long RecordID);

int fr_rename_face(unsigned long RecordID, const char* firstName, const char* secondName);

#ifdef __cplusplus
}
#endif

#endif //end

