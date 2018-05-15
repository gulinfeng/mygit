/**implement wrap cpp for c called***/
#include <vector>
#include <string>
#include "FRLibrary.h"
#include "fr_lib_wrap.h"

int c_wfFR_Init(void** pHandle, const char* pBasePath, int iFrameWid, int iFrameHig, int iFrameStep, enum FRMode eFrMode, int enableAntiSpoofing)
{
	return wfFR_Init(pHandle,  pBasePath, iFrameWid, iFrameHig, iFrameStep, eFrMode, enableAntiSpoofing);
}


int c_wfFR_Release(void** pHandle)
{
	return wfFR_Release(pHandle);
}


int c_wfFR_RecognizeFaces(void* handle, const unsigned char* pFrameData, int iFrameWid, int  iFrameHig, int iFrameStep, struct FRIDList* pResult)
{
	return wfFR_RecognizeFaces( handle, pFrameData,  iFrameWid, iFrameHig,iFrameStep, pResult);
}


int c_wfFR_EnrollFace(void* handle, const unsigned char* pImageData, int iImageWid, int iImageHig, int iImageStep, unsigned long lRecordID, int iForce, struct FRIDList* pResult)
{
	return wfFR_EnrollFace(handle, pImageData, iImageWid, iImageHig,  iImageStep, lRecordID, iForce,  pResult);
}

int c_wfFR_AddRecord(void* handle, unsigned long* pRecordID, const char* firstName, const char* secondName)
{
	return wfFR_AddRecord(handle, pRecordID,  firstName, secondName);
}

int c_wfFR_RemoveRecord(void* handle, unsigned long  lRecordID)
{
	return wfFR_RemoveRecord(handle, lRecordID);
}

//endif
