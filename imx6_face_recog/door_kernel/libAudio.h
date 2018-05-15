//#include "typedef.h"
//#include "basic_op.h"
//#include "ld8a.h"
//#include "dtx.h"

//G729编码函数
//输入有误则返回0，成功执行则返回1
// int G729Encode(
// 			   unsigned char* szBuf,             //输入：编码输入数组名
// 			   int nBufLen,               //输入：编码长度
// 			   FILE *fp                   //输入：输出文件句柄
// 			   );

//

#ifndef _LIB_AUDIO_IF_H
#define _LIB_AUDIO_IF_H

#include <alsa/asoundlib.h>
#include "blockqueue.h"

//播放状态 0 ~ 10
#define STATE_PLAY_START            0
#define STATE_PLAY_STOP             1
#define STATE_PLAY_DISPLAY_TIME     2
#define STATE_PLAY_PUT_DATA         3


//类型
#define TYPE_PLAY_RING      0  //播放铃声类型
#define TYPE_PLAY_WAVE     1  //播放wave文件类型
#define TYPE_PLAY_MSG      2  //播放短信类型
#define TYPE_PLAY_ALARM     3  //播放报警类型
#define TYPE_PLAY_KEYDOWN   4  //播放按键类型
#define TYPE_PLAY_TALK      5  //对讲

#define COMMUNITICATION_TIMES  120.0    //最长播放时间为两分钟

#define INIT_MIC_VAL  86


// state   处理状态  
// param1  数据1
// param2  数据2 
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

//开始写入, 创建写缓存队列
int BegineWritingData();

//结束写入, 销毁写缓存队列
int EndWritingData(QDataFree freedata);

//写数据到缓存
int WritingData(unsigned int pData, unsigned int DataLen, unsigned int reqnum);

//开始发送, 创建写缓存队列
int BegineSendingData();

//结束发送, 销毁写缓存队列
int EndSendingData(QDataFree freedata);

int IsInitialedQueue();

//马上停止对讲
int StopTalkImmediatly();


//获取数据到缓存
unsigned int GetSendingData(unsigned int* DataLen, unsigned int* param2,unsigned int* param3);

//datalen 数据字节长度
float GetWavLenght(int datalen);

//读取数据长度 固定为 BUFF_SIZE
int ReadOneFrameData(snd_pcm_t* capture, unsigned char buf[], int framesize);

//读取数据长度 固定为 BUFF_SIZE
int WriteOneFrameData(unsigned char buf[], int framesize);


//设置音量 (呼叫铃声和短信铃声是一样)
void SetPlayVol(int type, int nVol);

//
void ResetVolume(int flg);

//设置播放文件路径
void SetPlayFilePath(int type, char* path);

//设置暂停播放录音
void SetPlayRecordPause(int pause);

//开始播放wav
//无返回
void PlayWav(
           int nType                  //输入：播放wav的种类 
           );                         //0：忙音 1：呼叫等待 2:按键音 3:报警音  4: 呼叫铃声 5:短信铃声

//立刻播放按键音，不关闭其它播放也不改变音量
void Play_KeyDown_Immediatly();

//开始留言
//无返回
void StartRecord(
						PROCESS_STATE_CALLBACK callback    //输入: 录音状态回调
					);


//开始播放留言
//成功播放返回1，输入错误则返回0
int PlayRecord(
			   char *szFileName,    //输入：播放文件名
			   PROCESS_STATE_CALLBACK callback    //输入: 播放状态回调
			   );

//开始播放Riffwave文件
int PlayRiffWave(
			   char *szFileName,    //输入：播放文件名
			   int replayflg,       // 是否循环播放 0 不循环播放, 1 循环播放
			   PROCESS_STATE_CALLBACK callback    //输入: 播放状态回调
			   );

//设置是否循环播放
//void SetPlayFlg(int replayflg);

//设置采集灵敏度
void SetMicVal(int nVal);

//设置音量灵敏度
void Inner_SetPlayVol(int nVol);

void Set_dial_vol();

unsigned int load_sound(char* wav);

void unload_sound(unsigned int sample);

int play_sound(unsigned int sample, int loop, int flg);

void stop_sound();
#endif //_LIB_AUDIO_IF_H

