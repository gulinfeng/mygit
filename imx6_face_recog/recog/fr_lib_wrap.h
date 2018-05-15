/*********************************************************************************
  Copyright (C), 2017-2020, xigu Tech. Co., Ltd.
  File name   : fr_lib_wrap.h
  Author      : zlg
  Version     : v1.0
  Date        : 2017/11/14
  Description : c++ wrap fr lib
  Others:
  Function List:

  History:
    1. 

*********************************************************************************/
#ifndef _FR_LIB_WRAP_H
#define _FR_LIB_WRAP_H

#include "FRLibraryTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

int c_wfFR_Init(void** pHandle, const char* pBasePath, int iFrameWid, int iFrameHig, int iFrameStep, enum FRMode eFrMode, int enableAntiSpoofing);


int c_wfFR_Release     (void** pHandle);


int c_wfFR_RecognizeFaces(void* handle, const unsigned char* pFrameData, int iFrameWid, int  iFrameHig, int iFrameStep, struct FRIDList* pResult);


int c_wfFR_EnrollFace(void* handle, const unsigned char* pImageData, int iImageWid, int iImageHig, int iImageStep, unsigned long lRecordID, int iForce, struct FRIDList* pResult);

int c_wfFR_AddRecord(void* handle, unsigned long* pRecordID, const char* firstName, const char* secondName);

int c_wfFR_RemoveRecord(void* handle, unsigned long  lRecordID);




#ifdef __cplusplus
}
#endif



#endif
//end

