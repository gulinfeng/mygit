/**demo for face recognition**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FRLibrary.h"
#include "face_recog.h"

//#define ANTI_SPOOFING	


#define FR_DATA_PATH               "/home"           /* 人脸数据保存路径 */
#define FR_RE_FACE_MAX             1                 /* 最多一次能识别人脸的数量 */
#define FR_NAME_LEN_MAX            128               /* 名字最大长度 */
#define FR_MAX_TIMES               126


#define FR_NO_FACE_TIMEOUT         20

typedef struct _face_recog
{
    void* gpHandle;	
	enum FRMode gFRMode;
	int gImageWidth;
	int gImageHeight;
	int frame_interval;
	int enroll_times;
	int recog_times;
	int no_face_counter;
	int no_face_timeout;
	float confidence_thresh;
	int frame_counter;
	int recog_counter;
	int last_recog_sta;
	int enablespoof;
}FACE_RECOG_S;

static FACE_RECOG_S gFraceRecog;


static void  _iner_init_parm(FACE_RECOG_S* face_recog)
{
	face_recog->frame_interval = 3;
	face_recog->recog_times = 2;
	face_recog->enroll_times = 17;
	face_recog->recog_counter = 0;
	face_recog->frame_counter = 0;
	face_recog->confidence_thresh = 54.0; 
	if(1 == face_recog->enablespoof){
		face_recog->confidence_thresh = 50.0; 
	}
	face_recog->no_face_counter = 0;
	face_recog->no_face_timeout = FR_NO_FACE_TIMEOUT;
}

int fr_init(int frame_w, int frame_h, int spoof, int debug)
{
	int ret = FR_OK;
	memset(&gFraceRecog, 0, sizeof(FACE_RECOG_S));
	gFraceRecog.gImageWidth = frame_w;
	gFraceRecog.gImageHeight = frame_h;
	gFraceRecog.gpHandle = NULL;
	gFraceRecog.enablespoof = (spoof > 0) ? 1 : 0;
	_iner_init_parm(&gFraceRecog);
	ret = wfFR_VerifyLic(FR_DATA_PATH);
	if(ret != FR_OK){
		printf("ERROR: wfFR_VerifyLic fialed error code=%d\n", ret);
	}
	if(1 == debug){
		wfFR_setVerbose(1);
	}
	return ret;
}

int fr_get_enroll_times()
{
	return gFraceRecog.enroll_times;
}

void fr_uninit()
{
	if(gFraceRecog.gpHandle){
		
	}
}

void fr_set_enroll_param(int interval, float cfd_thresh, int times)
{
	return;
}

void fr_set_recognize_param(int interval, float cfd_thresh, int times)
{
	return;
}

void fr_set_recog_mode(int mode)
{
	gFraceRecog.enablespoof = (mode > 0) ? 1 : 0;
	if(0 != fr_setmode(FRMODE_RECOGNIZE)){
		printf("ERROR: fr setmode fialed.\n");
	}
}
int fr_setmode(enum FRMode eFrMode)
{
	int ret;
	int enableAntiSpoofing = 0;
	if(NULL != gFraceRecog.gpHandle){
		wfFR_Release(&gFraceRecog.gpHandle);
	}
	_iner_init_parm(&gFraceRecog);
	
	if((1 == gFraceRecog.enablespoof) && (FRMODE_RECOGNIZE == eFrMode)){
		
		enableAntiSpoofing = 1;
		printf("DEBUG: FR enable spoof!\n");
	}

	ret = wfFR_Init(&gFraceRecog.gpHandle, FR_DATA_PATH, gFraceRecog.gImageWidth, gFraceRecog.gImageHeight, gFraceRecog.gImageWidth, eFrMode, enableAntiSpoofing);
    if ((FR_OK != ret) || (NULL == gFraceRecog.gpHandle))
    {
    	if(gFraceRecog.gpHandle){
			wfFR_Release(&gFraceRecog.gpHandle);
    	}
    	gFraceRecog.gpHandle = NULL;
        printf("FR enroll init failed!");
        return -1;
    }
	if((1 == gFraceRecog.enablespoof) && (FRMODE_RECOGNIZE == eFrMode)){	
		wfFR_setSpoofingSensitivity(gFraceRecog.gpHandle, 2);
	}
	gFraceRecog.gFRMode = eFrMode;
	return ret;
}
/*
enum FRMode fr_getmode()
{
	if(NULL == gFraceRecog.gpHandle){
		printf("face recognize does not init\n");
		return -1;
	}
	return gFraceRecog.gFRMode;
}*/

void fr_init_encolling()
{
	gFraceRecog.frame_counter = 0;
	gFraceRecog.recog_counter = 0;
	gFraceRecog.no_face_counter = 0;
	gFraceRecog.no_face_timeout = FR_NO_FACE_TIMEOUT;
	return;
}

void fr_init_recognization()
{
	gFraceRecog.frame_counter = 0;
	gFraceRecog.recog_counter = 0;
	gFraceRecog.last_recog_sta = 0;
	return;
}

int fr_enroll(unsigned char *pFrame, unsigned long lRecordID, FR_RESULT* pResult)
{
	int ret;
	int spoof = 0;
	struct FRIDList* pstFRResult;
	pResult->pDList = NULL;
	pResult->stata = FR_NULL;
	if(NULL == gFraceRecog.gpHandle){
		printf("face recognize does not init\n");
		return -1;
	}
	gFraceRecog.frame_counter++;
	if(gFraceRecog.frame_counter % gFraceRecog.frame_interval == 0){
		gFraceRecog.frame_counter = 0;
		pResult->reserved2 = 0;
		pResult->pDList = (struct FRIDList*)malloc(sizeof(struct FRIDList));
		if(NULL == pResult->pDList ){
			printf("malloc FRIDList fialed\n");
			return -2;
		}
		pstFRResult = pResult->pDList;
		//printf("  gFraceRecog.gImageWidth = %d, gFraceRecog.gImageHeight = %d ,lRecordID = %ld, spoof = %d  \n",gFraceRecog.gImageWidth, gFraceRecog.gImageHeight, lRecordID, spoof);
		ret = wfFR_EnrollFace(gFraceRecog.gpHandle, pFrame, gFraceRecog.gImageWidth, gFraceRecog.gImageHeight, gFraceRecog.gImageWidth, lRecordID,0,pstFRResult, &spoof);
		//ret = wfFR_EnrollFaceFromSavedImage(gFraceRecog.gpHandle, "g.png", lRecordID, pstFRResult, &spoof);
		//printf("wfFR_EnrollFace ret=%d \n", ret);
		if (FR_OK != ret)
	    {
	    	free(pResult->pDList);
			pResult->pDList = NULL;
	        printf("Error in wfFR_EnrollFace\n");
	        return -3;
	    }
		//printf("DEBUG: enroll spoof=%d--------------------\n",spoof);
		pResult->stata = FR_Recognized;
		if(pstFRResult->nResults < 1) {
			free(pResult->pDList);
			pResult->pDList = NULL;
			gFraceRecog.no_face_counter++;
			if(gFraceRecog.no_face_counter >= gFraceRecog.no_face_timeout) {
				pResult->reserved2 = 1;
				if(FR_NO_FACE_TIMEOUT == gFraceRecog.no_face_timeout) {
					gFraceRecog.no_face_timeout = gFraceRecog.no_face_timeout + 10;
				}else{
					gFraceRecog.no_face_timeout = gFraceRecog.no_face_timeout + 10;
				}
				gFraceRecog.no_face_counter = 0;
				return 0;
			}
			return -4;
		}
		gFraceRecog.no_face_counter = 0;
		gFraceRecog.no_face_timeout = FR_NO_FACE_TIMEOUT;
		gFraceRecog.recog_counter++;
		pResult->reserved1 = gFraceRecog.enroll_times - gFraceRecog.recog_counter;
		if(0 >= pResult->reserved1){
			pResult->stata = FR_Enrolled;
			gFraceRecog.recog_counter = 0;
		}
	}
	return 0;
}

void fr_reduce_recog_counter()
{
	if(gFraceRecog.recog_counter > 0){
		gFraceRecog.recog_counter--;
	}
}

int fr_recognize(unsigned char *pFrame, FR_RESULT* pResult)
{
	int ret;
	int spoof = 0;
	struct FRIDList* pstFRResult;
	pResult->pDList = NULL;
	pResult->stata = FR_NULL;
	if(NULL == gFraceRecog.gpHandle){
		printf("face recognize does not init\n");
		return -1;
	}
	gFraceRecog.frame_counter++;
	if(gFraceRecog.frame_counter % gFraceRecog.frame_interval == 0){
		gFraceRecog.frame_counter = 0;
		pResult->pDList = (struct FRIDList*)malloc(sizeof(struct FRIDList));
		if(NULL == pResult->pDList ){
			printf("malloc FRIDList fialed\n");
			return -2;
		}
		pstFRResult = pResult->pDList;
		ret = wfFR_RecognizeFaces(gFraceRecog.gpHandle, pFrame, gFraceRecog.gImageWidth, gFraceRecog.gImageHeight, gFraceRecog.gImageWidth, pstFRResult,&spoof);
	    if (FR_OK != ret)
	    {
	    	free(pResult->pDList);
			pResult->pDList = NULL;
	        printf("Error in wfFR_RecognizeFaces\n");
	        return -3;
	    }
		//printf("DEBUG: recogn spoof=%d--------------------\n",spoof);
		pResult->stata = FR_Recognized;
		if(pstFRResult->nResults < 1) {
			gFraceRecog.recog_counter = 0;
			free(pResult->pDList);
			pResult->pDList = NULL;
			return -4;
		}
		if(pstFRResult->pConfidence[0] > gFraceRecog.confidence_thresh) {
			if(1 == gFraceRecog.last_recog_sta)
			{
				gFraceRecog.recog_counter++;
			}else{
				gFraceRecog.last_recog_sta = 1;
				gFraceRecog.recog_counter = 2;
			}
			if(gFraceRecog.recog_times == gFraceRecog.recog_counter){
				pResult->stata = FR_FaceComfirmed;
				pResult->reserved1 = 1;
				gFraceRecog.recog_counter = 0;
			}
		}else{
			
			if(0 == gFraceRecog.last_recog_sta)
			{
				gFraceRecog.recog_counter++;
			}else{
				gFraceRecog.last_recog_sta = 0;
				gFraceRecog.recog_counter = 0;
			}
			if(gFraceRecog.recog_times == gFraceRecog.recog_counter){
				pResult->stata = FR_FaceComfirmed;
				pResult->reserved1 = 0;
				gFraceRecog.recog_counter = 0;
			}
		}
	}
	return 0;
}

int fr_remove_face(unsigned long RecordID)
{
	if(NULL == gFraceRecog.gpHandle){
		printf("face recognize does not init\n");
		return -1;
	}
	return wfFR_RemoveSingleRecord(gFraceRecog.gpHandle, RecordID); //wfFR_RemoveRecord(gFraceRecog.gpHandle, RecordID);
}
int fr_add_face(unsigned long* pRecordID, const char* firstName, const char* secondName)
{
	if(NULL == gFraceRecog.gpHandle){
		printf("face recognize does not init\n");
		return -1;
	}
	return wfFR_AddRecord(gFraceRecog.gpHandle, pRecordID, firstName,secondName);
}

int fr_rename_face(unsigned long RecordID, const char* firstName, const char* secondName)
{
	if(NULL == gFraceRecog.gpHandle){
		printf("face recognize does not init\n");
		return -1;
	}
	return wfFR_RenameRecordID(gFraceRecog.gpHandle, RecordID, firstName,secondName);
}
//end
