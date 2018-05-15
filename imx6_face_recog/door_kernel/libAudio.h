//#include "typedef.h"
//#include "basic_op.h"
//#include "ld8a.h"
//#include "dtx.h"

//G729���뺯��
//���������򷵻�0���ɹ�ִ���򷵻�1
// int G729Encode(
// 			   unsigned char* szBuf,             //���룺��������������
// 			   int nBufLen,               //���룺���볤��
// 			   FILE *fp                   //���룺����ļ����
// 			   );

//

#ifndef _LIB_AUDIO_IF_H
#define _LIB_AUDIO_IF_H

#include <alsa/asoundlib.h>
#include "blockqueue.h"

//����״̬ 0 ~ 10
#define STATE_PLAY_START            0
#define STATE_PLAY_STOP             1
#define STATE_PLAY_DISPLAY_TIME     2
#define STATE_PLAY_PUT_DATA         3


//����
#define TYPE_PLAY_RING      0  //������������
#define TYPE_PLAY_WAVE     1  //����wave�ļ�����
#define TYPE_PLAY_MSG      2  //���Ŷ�������
#define TYPE_PLAY_ALARM     3  //���ű�������
#define TYPE_PLAY_KEYDOWN   4  //���Ű�������
#define TYPE_PLAY_TALK      5  //�Խ�

#define COMMUNITICATION_TIMES  120.0    //�����ʱ��Ϊ������

#define INIT_MIC_VAL  86


// state   ����״̬  
// param1  ����1
// param2  ����2 
//return not 0 failed,  0 success
typedef int (*PROCESS_STATE_CALLBACK)(int state, unsigned int param1,unsigned int param2);


int  InitAudio();

void UninitAudio();

void SetAudioSystemFun(PROCESS_STATE_CALLBACK system_fun);


snd_pcm_t* GetAudioPC();

int CreateCaptureDv(snd_pcm_t** capture);

void ReleaseAudioPC();

void ResetAudioPC();

void StopWav();
void StopRecord();
void StopPlayRecord();

void CloseWave();

void StopAllAudio();

unsigned int GetSoundFrameSize();
unsigned int GetCaptureFrameSize();

//��ʼд��, ����д�������
int BegineWritingData();

//����д��, ����д�������
int EndWritingData(QDataFree freedata);

//д���ݵ�����
int WritingData(unsigned int pData, unsigned int DataLen, unsigned int reqnum);

//��ʼ����, ����д�������
int BegineSendingData();

//��������, ����д�������
int EndSendingData(QDataFree freedata);

int IsInitialedQueue();

//����ֹͣ�Խ�
int StopTalkImmediatly();


//��ȡ���ݵ�����
unsigned int GetSendingData(unsigned int* DataLen, unsigned int* param2,unsigned int* param3);

//datalen �����ֽڳ���
float GetWavLenght(int datalen);

//��ȡ���ݳ��� �̶�Ϊ BUFF_SIZE
int ReadOneFrameData(snd_pcm_t* capture, unsigned char buf[], int framesize);

//��ȡ���ݳ��� �̶�Ϊ BUFF_SIZE
int WriteOneFrameData(unsigned char buf[], int framesize);


//�������� (���������Ͷ���������һ��)
void SetPlayVol(int type, int nVol);

//
void ResetVolume(int flg);

//���ò����ļ�·��
void SetPlayFilePath(int type, char* path);

//������ͣ����¼��
void SetPlayRecordPause(int pause);

//��ʼ����wav
//�޷���
void PlayWav(
           int nType                  //���룺����wav������ 
           );                         //0��æ�� 1�����еȴ� 2:������ 3:������  4: �������� 5:��������

//���̲��Ű����������ر���������Ҳ���ı�����
void Play_KeyDown_Immediatly();

//��ʼ����
//�޷���
void StartRecord(
						PROCESS_STATE_CALLBACK callback    //����: ¼��״̬�ص�
					);


//��ʼ��������
//�ɹ����ŷ���1����������򷵻�0
int PlayRecord(
			   char *szFileName,    //���룺�����ļ���
			   PROCESS_STATE_CALLBACK callback    //����: ����״̬�ص�
			   );

//��ʼ����Riffwave�ļ�
int PlayRiffWave(
			   char *szFileName,    //���룺�����ļ���
			   int replayflg,       // �Ƿ�ѭ������ 0 ��ѭ������, 1 ѭ������
			   PROCESS_STATE_CALLBACK callback    //����: ����״̬�ص�
			   );

//�����Ƿ�ѭ������
//void SetPlayFlg(int replayflg);

//���òɼ�������
void SetMicVal(int nVal);

//��������������
void Inner_SetPlayVol(int nVol);

void Set_dial_vol();

unsigned int load_sound(char* wav);

void unload_sound(unsigned int sample);

int play_sound(unsigned int sample, int loop, int flg);

void stop_sound();
#endif //_LIB_AUDIO_IF_H

