/**complement sound utile**/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SDL_mixer.h"
#include "sound_utile.h"

#define SOUND_FILE1   "wav/1.wav"
#define SOUND_FILE2   "wav/2.wav"
#define SOUND_FILE3   "wav/3.wav"
#define SOUND_FILE4   "wav/4.wav"
#define SOUND_FILE5   "wav/5.wav"
#define SOUND_FILE6   "wav/6.wav"
#define SOUND_FILE7   "wav/7.wav"
#define SOUND_FILE8   "wav/8.wav"
#define SOUND_FILE9   "wav/9.wav"
#define SOUND_FILE10  "wav/10.wav"

#define SOUND_NUM 9

/*
* 0 stop
* 1 -> 识别成功
* 2 -> 无法识别
* 3 -> 录制人脸请按井号键
* 4 -> 开始录脸
* 5 -> 录脸已完成
* 6 -> 录脸已取消
* 7 -> 录脸超时
* 8 -> 退出录脸请按星号键
* 9 -> 请对准屏幕
* 10 -> 无法添加人脸
*/
static int g_cur_index = 0;
static int g_audio_opend = 0;
static Mix_Chunk* g_sample[SOUND_NUM] = {0};

int load_sounds()
{
	int i;
	int audio_channels = 2;
	int audio_rate = 16000;
	Uint16 audio_format = MIX_DEFAULT_FORMAT;
	const char* sample_file[SOUND_NUM];
	if(g_audio_opend){
		Mix_CloseAudio();
	}
	/* Open the audio device */
	if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, 4096) < 0) {
		printf("Couldn't open audio: %s\n", SDL_GetError());
		return -1;
	} else {
		Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
		printf("Opened audio at %d Hz %d bit %s", audio_rate,
			(audio_format&0xFF),
			(audio_channels > 2) ? "surround" :
			(audio_channels > 1) ? "stereo" : "mono");
		putchar('\n');
		
	}
	sample_file[0] = SOUND_FILE1;
	sample_file[1] = SOUND_FILE2;
	sample_file[2] = SOUND_FILE3;
	sample_file[3] = SOUND_FILE4;
	sample_file[4] = SOUND_FILE5;
	sample_file[5] = SOUND_FILE6;
	sample_file[6] = SOUND_FILE7;
	sample_file[7] = SOUND_FILE8;
	sample_file[8] = SOUND_FILE9;
	//sample_file[9] = SOUND_FILE10;
	/* Load the requested wave file */
	for(i =0; i < SOUND_NUM; i++) {
		g_sample[i]= Mix_LoadWAV(sample_file[i]);
		if ( g_sample[i] == NULL ) {
			printf("Couldn't load %s: %s\n",sample_file[i], SDL_GetError());
		}
	}
	g_audio_opend = 1;
	g_cur_index = 0;
	return 0;
}

void free_sounds()
{
	int i;
	for(i = 0; i < SOUND_NUM; i++)
	{
		if(g_sample[i]){
			Mix_FreeChunk(g_sample[i]);
		}
	}
	if(g_audio_opend){
		Mix_CloseAudio();
	}
	return;
}

void play_sound(int index, int flg)
{
	if(0 == index){
		Mix_HaltChannel(0);
		return;
	}
	if(index < 1 || index > SOUND_NUM){
		printf("play_sound index=%d error\n",index);
		return;
	}
	if(flg || (index != g_cur_index)){
		Mix_HaltChannel(0);
	}else if(index == g_cur_index){
		if(1 == Mix_Playing(0)){
			return;
		}
	}
	g_cur_index = index;
	Mix_PlayChannel(0, g_sample[index - 1], 0);
	return;
}

void set_volume(int volume)
{
	Mix_Volume(0,volume);
	return;
}

//endif

