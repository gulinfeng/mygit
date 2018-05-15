#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "libAudio.h"
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include "dtmf.h"
#include "door_elevator.h"

#define SPEEX_ENABLE               0

#if SPEEX_ENABLE
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"
#endif

#define PLAY_WAV_ALLOW_CLOSE      40    
#define PLAY_WAV_IDLE_TIMEOUT     12000     // 8000ms (1->16ms)
#define WAIT_TIMEOUT              200000
#define COUNTER_OVER              120       

#define FILL_MUTE_DT_TMOUT      2
#define DEF_SOUND_BUF_SIZE      1024
#define DEF_CAPTURE_BUF_SIZE    1024
#define PLAY_RATE     8000
#define SAMPLE_DEEP   16
#define FILTER_LENGHT 512
#define PERIOD_SIZE   32
#define ONE_SOUND_BUF_SIZE 128

#define FLOAT_RATE   8000.0f

#define WAV_TYPE_NONE    100
#define WAV_TYPE_EXIT    14
#define WAV_TYPE_CLOSED  101

#define QUEUE_LEN_THROSHOLD  20
#define WAIT_SND_PCM_TIMEOUT 20

typedef struct MsgNode
{
	unsigned int  InnerId;   
	int  iType;              //0 访客留言   1访客留影  2本机留言         
	int  bRead;              // 0 read,  1 unread
	int  timelenght;
	char msgname[16];
	char VoiceFile[64];
	char PictFile[64];
	char starttime[32];
	void* pDate;
	struct MsgNode* next;
}MsgList;

//wave文件头  
typedef struct WaveHeader  
{  
    unsigned char riff[4];             //资源交换文件标志  
    unsigned int size;               //从下个地址开始到文件结尾的字节数  
    unsigned char wave_flag[4];        //wave文件标识  
    unsigned char fmt[4];              //波形格式标识  
    unsigned int fmt_len;            //过滤字节(一般为00000010H)  
    unsigned short tag;                //格式种类，值为1时，表示PCM线性编码  
    unsigned short channels;           //通道数，单声道为1，双声道为2  
    unsigned int samp_freq;          //采样频率  
    unsigned int byte_rate;          //数据传输率 (每秒字节＝采样频率×每个样本字节数)  
    unsigned short block_align;        //块对齐字节数 = channles * bit_samp / 8  
    unsigned short bit_samp;           //bits per sample (又称量化位数)  
    unsigned char data_flag[4];        //数据标识符  
    unsigned int length;             //采样数据总数 
} wave_header_t; 

typedef struct sound_sample
{
	int len;
	int pos;
	unsigned char* data;
}SOUND_SAMPLE_S;

typedef struct sound_play
{
	int abort;
	int loop;
	pthread_t ptd_play;
	SOUND_SAMPLE_S* cur_sample;
}SOUND_PALY_S;

#define BUFFER_SIZE                128 //128

typedef struct topacket{
	unsigned int seqnum;
	unsigned char buf[BUFFER_SIZE]; //BUF_SIZE/2   1024
}TOPACKET;


static int s_replay_flg = 1;
int g_nMicVal = -1;

//extern int playdtmf;
extern unsigned char linear2alaw(short pcm_val);
extern short alaw2linear(unsigned char u_val);
extern void writelog(int nErrCode,int nErrId);

extern int set_mic_by_i2c(int level);
extern int set_vol_by_i2c(int vol);
extern int mutil_tune_mic_by_i2c(int amp1g0,  int amp0gr1);

static pthread_mutex_t g_mutex;
static pthread_mutex_t g_mutex_get;
//static  pthread_t s_hPlayWav = (pthread_t)NULL;
static  pthread_t s_hPlayRecord = (pthread_t)NULL;
static  pthread_t s_hRecord = (pthread_t)NULL;
static snd_pcm_t*  s_fdAudio = NULL;
//static  int   g_nWavType= WAV_TYPE_NONE;
//static  int   g_nOldWaveType = WAV_TYPE_NONE;
static int s_PlayVol =   255;
static int s_RingVol =  255;
static int s_KeyVol =   255;
static int s_AlarmVol = 255;//90
static int s_CurVol = 0;

static int nPlay=0;
static int nRecord=0;
static int nPlayRecord=0;
static int nPlayPauseFlg = 0;

static char s_AlarmPath[64] = {0};
static char s_RingPath[64] = {0};
static char s_MsgPath[64] = {0};

static PROCESS_STATE_CALLBACK s_audio_record_fun = NULL;
static PROCESS_STATE_CALLBACK s_playing_wave_fun = NULL;
static PROCESS_STATE_CALLBACK s_alase_system_fun = NULL;

static MsgList s_RecordDt;
static FILE*  s_record_fd;
static int s_send_queue_flg = 0;
static int s_writing_quit = 0;
static BLOCK_QUEUE_S s_write_queue;
static BLOCK_QUEUE_S s_send_queue;
static  pthread_t s_hWriting = (pthread_t)NULL;
static unsigned int s_sound_buff_size = DEF_SOUND_BUF_SIZE;
static unsigned int s_capture_buff_size = DEF_CAPTURE_BUF_SIZE;
static unsigned int s_sound_frames = 0;
static unsigned int s_capture_frames = 0;
static unsigned int s_ring_wave_cnter = 0;
static unsigned int g_wave_threadquit =  0;

#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
int set_wm8960_vol(char *sctrlstr, long left_val, long right_val ,int numid)
{  
   /* long volMin=0, volMax=0, leftVal=0, rightVal=0;
    snd_mixer_elem_t *elem;  
    snd_mixer_selem_id_t *sid;  
    snd_mixer_selem_id_alloca(&sid); */
	snd_ctl_elem_value_t *elem_value = NULL;
	int idx = 0;
	char str[128] = {0};
	long *pdata = NULL;
	snd_mixer_t *handle = NULL;
	int err=0; 

	snd_ctl_elem_value_alloca(&elem_value);
	//numid=4,iface=MIXER,name='Playback Volume'  0-255
	//numid=1,iface=MIXER,name='Capture Volume'   0-63    默认23
	snd_ctl_elem_value_set_numid(elem_value, numid);

	idx = -1;
	if ((err = snd_card_next(&idx)) < 0) {
		printf("Card next error: %s\n", snd_strerror(err));
		return -1;
	}

	sprintf(str, "hw:CARD=%i", idx);
	if ((err = snd_ctl_open(&handle, str, 0)) < 0) {
		printf("Open control error: %s\n", snd_strerror(err));
		return -2;
	}

	if ((err = snd_ctl_elem_read(handle, elem_value)) < 0) {
		printf("snd_ctl_elem_read error: %s\n", snd_strerror(err));
		return -3;
	}

	pdata=(long *)snd_ctl_elem_value_get_bytes(elem_value);
	//printf("before changing ,left volume is:%d,right volume is %d\n", *pdata, *(pdata+1));

	//to set the new value    
	snd_ctl_elem_value_set_integer(elem_value, 0, left_val);
	snd_ctl_elem_value_set_integer(elem_value, 1, right_val);

	if ((err = snd_ctl_elem_write(handle, elem_value)) < 0) {
		printf("snd_ctl_elem_write error: %s\n", snd_strerror(err));
		return -4;
	}

	//read it out again to check if we did set the registers.
	if ((err = snd_ctl_elem_read(handle,elem_value)) < 0) {
		printf("snd_ctl_elem_read error: %s\n", snd_strerror(err));
		return -5;
	}
	pdata = (long *)snd_ctl_elem_value_get_bytes(elem_value);
	//printf("after changing, left volume is:%d,right volume is %d\n", *pdata, *(pdata+1));
	snd_ctl_close(handle);
	
	return 0;
}  
#endif

static int64_t get_system_utime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void Set_dial_vol()
{
	Inner_SetPlayVol(s_RingVol);
	s_CurVol = s_RingVol;
}

//音量设置 0 <= nVol <= 100
void Inner_SetPlayVol(int nVol)
{
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	set_vol_by_i2c(nVol);
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	if(set_wm8960_vol(NULL, nVol, nVol, 4) < 0){
		printf("INFO: Inner_SetPlayVol failed\n");
		return;
	}
#endif
	printf("INFO: Inner_SetPlayVol %d \n", nVol);
	return ;
}

//mic输入设置 0 <= nVol <= 100
void SetMicVal(int nVal)
{
	if (g_nMicVal < 0) {

#if ((defined S500_CLOCK) || (defined S500_FR)) 
		mutil_tune_mic_by_i2c(7, 1);
		if (0 == set_mic_by_i2c(nVal)){
			g_nMicVal = nVal;
		}
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
		if(set_wm8960_vol(NULL, nVal, nVal, 1) < 0){
			printf("INFO: Inner_SetPlayVol failed\n");
			return;
		}
		else{
			g_nMicVal = nVal;
		}
#endif

	}
	printf("INFO: SetMicVal-%d \n", nVal);
	return ;
}

void ResetVolume(int flg)
{
	s_CurVol = 0;
	if(1 == flg)
	{
		Inner_SetPlayVol(s_RingVol);
	}
}

//设置音量
void SetPlayVol(int type, int nVol)
{
#if 1
	//printf("--SetPlayVol--type=%d (nVol=%d)\n",type, nVol);
	switch(type)
	{
		case TYPE_PLAY_RING:
			s_RingVol =  nVol;
			break;
		case TYPE_PLAY_WAVE:
			s_PlayVol = nVol;
			break;
		case TYPE_PLAY_MSG:
			s_RingVol =  nVol;
			break;
		case TYPE_PLAY_ALARM:
			s_AlarmVol = nVol;
			break;
		case TYPE_PLAY_KEYDOWN:
			s_KeyVol = nVol;
			break;
		case TYPE_PLAY_TALK:
			if(s_CurVol != nVol)
			{
				Inner_SetPlayVol(nVol);
				s_CurVol = nVol;
			}
			break;
	}
#endif
	return;
}


//设置播放文件路径
void SetPlayFilePath(int type, char* path)
{
	switch(type)
	{
		case TYPE_PLAY_RING:
			strncpy(s_RingPath, path, sizeof(s_RingPath) - 1);
			break;
		case TYPE_PLAY_MSG:
			strncpy(s_MsgPath, path, sizeof(s_MsgPath) - 1);
			break;
		case TYPE_PLAY_ALARM:
			strncpy(s_AlarmPath, path, sizeof(s_AlarmPath) - 1);
			break;
	}
	return;
}

///////////////////////////////////////////////////////
//SND_PCM_FORMAT_S16_LE
static int set_alsa_playparam(snd_pcm_t *pcm, snd_pcm_format_t fmt, unsigned int samplerate, int channels, unsigned int* buffsize, unsigned int* periodsize)
{
	int rc;
	snd_pcm_uframes_t st_srate;
	snd_pcm_uframes_t st_frames;
	int err = -1;
	snd_pcm_uframes_t st_buffsize;
	snd_pcm_hw_params_t* ply_params;
	//unsigned int val1;
	//int nRate, nChann;
	if((channels != 1) && (channels != 2))
	{
		printf("only suport stero or mone\n");
		return 0;
	}
	
	/* Allocate a hardware parameters object. 分配snd_pcm_hw_params_t结构体 */
	snd_pcm_hw_params_alloca(&ply_params);


	/* Fill it in with default values. */
	err = snd_pcm_hw_params_any(pcm, ply_params);
	if (err < 0) 
	{
	    SNDERR("Broken configuration for: no configurations available");
	    return err;
	}
	/* Set the desired hardware parameters. */

	err = snd_pcm_hw_params_set_rate_resample(pcm, ply_params, 1);
	if (err < 0) 
	{
	    SNDERR("Resampling setup failed for: %s", snd_strerror(err));
	    return err;
	}

	/* Interleaved mode 初始化访问权限*/
	err = snd_pcm_hw_params_set_access(pcm, ply_params,
	                  SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		SNDERR("Access type not available for: %s", snd_strerror(err));
		return err;
	}
	/* Signed 16-bit little-endian format 初始化采样格式*/
	err = snd_pcm_hw_params_set_format(pcm, ply_params, fmt);
	if (err < 0) {
		SNDERR("Sample format not available for: %s", snd_strerror(err));
		return err;
	}
	
	//printf("snd_pcm_hw_params_set_format ok\n");
	/* Two channels (stereo) 设置通道数量*/
	err = snd_pcm_hw_params_set_channels(pcm, ply_params, channels);
	if (err < 0) {
		SNDERR("Channels count (%i) not available for: %s", channels, snd_strerror(err));
		return err;
	}
	/* 8000 bits/second sampling rate 设置采样率*/
	st_srate = (snd_pcm_uframes_t)samplerate;
	//printf("st_srate = %d\n",(int)st_srate);
	err = snd_pcm_hw_params_set_rate_near(pcm, ply_params,
	                          &st_srate, NULL);
	if (err < 0) {
		SNDERR("Rate %iHz not available for playback: %s", (int)st_srate, snd_strerror(err));
		return err;
	}
	if (st_srate != (snd_pcm_uframes_t)samplerate) {
		SNDERR("Rate doesn't match (requested %iHz, get %iHz)", samplerate, (int)st_srate);
		return -EINVAL;
	}
	
	st_buffsize = (snd_pcm_uframes_t)(*buffsize);
	//printf("st_buffsize = %d\n",(int)st_buffsize);
	err = snd_pcm_hw_params_set_buffer_size_near(pcm, ply_params, &st_buffsize);
	if (err < 0) {
		printf("Unable to set buffer size %li for: %s\n", (int)st_buffsize, snd_strerror(err));
		return err;
	}

	//st_frames = st_buffsize >> 1;
	st_frames = PERIOD_SIZE;
	/* Set period size to 32 frames. */
	snd_pcm_hw_params_set_period_size_near(pcm,
	                      ply_params, &st_frames, NULL);

	//printf("snd_pcm_hw_params_set_period_size_near ok(%d)\n",(int)st_frames);
	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(pcm, ply_params);
	if (rc < 0) {
		printf("unable to set hw parameters: %s\n",
		        snd_strerror(rc));
		return -1;
	}
	//printf("snd_pcm_hw_params ok\n");
	/* Use a buffer large enough to hold one period */
	if(0 == (*periodsize))
	{
		err = snd_pcm_hw_params_get_buffer_size(ply_params, &st_buffsize);
	    if (err < 0)
	            return err;
	    err = snd_pcm_hw_params_get_period_size(ply_params, &st_frames, NULL);
	    if (err < 0)
	            return err;
		(*periodsize) = (unsigned int)st_frames;
		printf("snd_pcm_hw_params buffsize=%d, periodsize=%d\n",(int)st_buffsize,(int)st_frames);
	}
	return err;
}

static int WaveOpen(FILE* fp, wave_header_t* pheader) ;

static SOUND_PALY_S  g_sound_play = {0};

static void* PlaySoundThread(void* param)
{
	int rc=0, i=0,sound_buff_tmp=0;
	int idle = 0;
	//int cpylen = 0;
	int pos = 0;
	SOUND_SAMPLE_S* pSample = NULL;
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	unsigned short buffer[256];
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	unsigned short buffer[370] = {0};//imx6 : 185*2=370      s500:buffer[256] = {0};
#endif
	snd_pcm_t* fdAudio = NULL;
	printf("PlaySoundThread is running\n");
	Set_dial_vol();
	rc = snd_pcm_open(&fdAudio, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0)
	{
		printf("unable to open pcm device again: %s\n",
		        snd_strerror(rc));
		return 0;
	}
#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
	s_sound_frames = 0;
#endif
	set_alsa_playparam(fdAudio, SND_PCM_FORMAT_S16_LE, 16000, 2, &s_sound_buff_size, &s_sound_frames);//s_sound_frames=185 in imx6
	//printf("---PlaySoundThread fdAudio, SND_PCM_FORMAT_S16_LE, 16000, 2, &s_sound_buff_size, &s_sound_frames=%d\n", s_sound_frames);
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	sound_buff_tmp = ONE_SOUND_BUF_SIZE;
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	sound_buff_tmp = s_sound_frames;
#endif
	while(0 == g_sound_play.abort){
		pthread_mutex_lock(&g_mutex);
		pSample = g_sound_play.cur_sample;
		if(pSample){
			idle = 0;
			for(i = 0; i < sound_buff_tmp; i++){
				pos = pSample->pos;
				memcpy(&buffer[2*i], &pSample->data[pos], 2);
				buffer[2*i + 1] = buffer[2*i];
				pSample->pos += 2;
				if(pSample->pos >= pSample->len){
					pSample->pos = 0;
					if(0 == g_sound_play.loop){
						g_sound_play.cur_sample = NULL;
					}
				}
			}
			
		}else{
			memset(buffer, 0, sizeof(buffer));
			idle++;
			if(idle >= PLAY_WAV_IDLE_TIMEOUT)
			{
				g_sound_play.abort = 1;
				pthread_mutex_unlock(&g_mutex);
				printf("play sound runing idle for long time--\n");
				break;
			}
			
		}
		pthread_mutex_unlock(&g_mutex);
		rc = snd_pcm_writei(fdAudio, (char*)buffer, sound_buff_tmp);
		if (rc == -EPIPE) {
			/* EPIPE means overrun */
			printf( "busy overrun occurred\n");
			//snd_pcm_prepare(s_fdAudio);
		} else if (rc < 0) {
			printf("busy error from read: %s\n",
			snd_strerror(rc));
		} else if (rc != (int)sound_buff_tmp) {//ONE_SOUND_BUF_SIZE
			printf( "ring short write, %d frames\n", rc);
		}
		
		
	}
	snd_pcm_drain(fdAudio);
	snd_pcm_close(fdAudio);
	pthread_mutex_lock(&g_mutex);
	g_sound_play.ptd_play = NULL;
	g_sound_play.cur_sample = NULL;
	pthread_mutex_unlock(&g_mutex);
	printf("PlaySoundThread exit\n");
	return 0;
}
unsigned int load_sound(char* wav)
{
	int ret;
	FILE* fd;
	SOUND_SAMPLE_S* pSamle = NULL;
	wave_header_t waveheader;
	fd = fopen(wav, "rb");
	if(fd == NULL)
	{
		printf("ERROR: can not fine %s\n", wav);
		return 0;
	}
	ret = WaveOpen(fd, &waveheader);
	if(ret != 0){
		fclose(fd);
		printf("ERROR: read wave header err: %s\n", wav);
		return 0;
	}
	if(16000 != waveheader.samp_freq || 16 != waveheader.bit_samp || 1 != waveheader.channels){
		fclose(fd);
		printf("ERROR: wav format invalid (%d,%d,%d)\n",\
			waveheader.samp_freq , waveheader.bit_samp, waveheader.channels);
		return 0;
	}
	pSamle = (SOUND_SAMPLE_S*)malloc(sizeof(SOUND_SAMPLE_S));
	if(!pSamle){
		printf("ERROR: malloc SOUND_SAMPLE_S fialed\n");
		fclose(fd);
		return 0;
	}
	pSamle->pos = 0;
	pSamle->len = waveheader.length;
	pSamle->data = (unsigned char*) malloc(waveheader.length);
	//printf("pSamle->data len = %d \n", waveheader.length);
	if(!pSamle){
		printf("ERROR: malloc sound data fialed\n");
		fclose(fd);
		free(pSamle);
		return 0;
	}
	ret = fread(pSamle->data, 1, waveheader.length, fd);
	pSamle->len = ret;
	fclose(fd);
	return (unsigned int)pSamle;
}

void unload_sound(unsigned int sample)
{
	SOUND_SAMPLE_S* pSl = (SOUND_SAMPLE_S*)sample;
	if(pSl){
		free(pSl->data);
		free(pSl);	
	}
	return;
}

int play_sound(unsigned int sample, int loop, int flg)
{
	int nRet=0;
	SOUND_SAMPLE_S* cur = (SOUND_SAMPLE_S*)sample;
	if(NULL == g_sound_play.ptd_play){
		StopRecord();
		g_sound_play.abort = 0;
		nRet=pthread_create(&g_sound_play.ptd_play,NULL,(void*)PlaySoundThread,NULL);
	    if(nRet!=0)
	    {
			printf ("Create playsound pthread error!\n");
			return -1;
	    }
		pthread_detach(g_sound_play.ptd_play);
	}
	if(0 == flg && cur == g_sound_play.cur_sample){
		return 0;
	}
	if(cur){
		cur->pos = 0;
	}
	pthread_mutex_lock(&g_mutex);	
	// 1?±?
	g_sound_play.loop= loop;
	if(g_sound_play.cur_sample){
		g_sound_play.cur_sample->pos = 0;
	}
	g_sound_play.cur_sample = cur;
	pthread_mutex_unlock(&g_mutex);
	return 0;
}

void stop_sound()
{
	struct timeval tempval;
	g_sound_play.abort = 1;
	while(g_sound_play.ptd_play){
		tempval.tv_sec = 0;
    	tempval.tv_usec = 40000;
    	select(0, NULL, NULL, NULL, &tempval);
	}
}


void Play_KeyDown_Immediatly()
{
	int counter = 0;
	struct timeval tempval;
	if(g_sound_play.ptd_play)
	{
		return;
	}
	StopAllAudio();
	if(s_fdAudio)
	{
		//′?ê±???2?a×??±?ó2￥·?°′?ü
		if(s_hWriting)
		{
			int i, j;
			int datalen = 0;
			int write_frames = 0;
			short* pbuffer;
			datalen = sizeof(dtmf11);
			write_frames = s_sound_frames;
			for(i=0;i<datalen;i=i+s_sound_frames)
			{
				pbuffer = (short*)malloc(s_sound_frames << 1);
				if(i + s_sound_frames > datalen)
				{
					write_frames = (datalen - i);
					for(j = write_frames; j < s_sound_frames; j++)
					{
						pbuffer[j] = 0;
					}
					//printf("--datalen=%d, write_frames=%d\n",datalen, write_frames);
				}
				for(j=0; j<write_frames; j++)
				{
					pbuffer[j] = (short)alaw2linear(dtmf11[i+j]);
				}
				block_queue_put(&s_write_queue, (unsigned int)pbuffer, s_sound_frames, i, 0);
			}
			usleep(200000);
		}
		return;
	}

}


int CreateCaptureDv(snd_pcm_t** capture)
{
	int rc;
	rc = snd_pcm_open(capture, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) 
	{
		printf("unable to open capture device: %s\n",
		snd_strerror(rc));
		return 0;
	}

	s_capture_frames = 0;
	//printf("-1-capture-CreateCaptureDv s_capture_frames=%d \n", s_capture_frames);
	rc= set_alsa_playparam((*capture), SND_PCM_FORMAT_S16_LE, PLAY_RATE, 2, &s_capture_buff_size, &s_capture_frames);
	//printf("-2-capture-CreateCaptureDv s_capture_frames=%d \n", s_capture_frames);
	return rc;
}

static void* RecordThread(void* param)
{
	int rc;
	int ret,i;
	float sumtime;
	float frametm;
	float fInterVal = 0.0;
	int readsample = 0;
	snd_pcm_t *rcd_handle;
	unsigned char* tempbuf =NULL;
	short* buffer =NULL;
#if SPEEX_ENABLE
	int frame_size;
	int filter_length;
	int denoise = 1;
	int noise_suppress = -10;
	SpeexPreprocessState *micprec;
#endif 
	if(NULL == s_audio_record_fun)
	{
		printf("--RecordThread--exit-:callback=null\n");
		pthread_exit((void*)"exit");
		return 0;
	}

	//snd_pcm_hw_params_free (rcd_params); 
	ret = CreateCaptureDv(&rcd_handle);
	if(ret < 0)
	{
		s_audio_record_fun(STATE_PLAY_STOP,(unsigned int)0, (unsigned int)&s_RecordDt);
		pthread_exit((void*)"exit");
		return 0;
	}
#if SPEEX_ENABLE
	frame_size = ONE_SOUND_BUF_SIZE;
	filter_length = FILTER_LENGHT;
	//mic 预处理
	micprec = speex_preprocess_state_init(frame_size, PLAY_RATE);   //降噪
	speex_preprocess_ctl(micprec, SPEEX_PREPROCESS_SET_DENOISE, &denoise);  //降噪分贝
	speex_preprocess_ctl(micprec, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);
#endif
	ret = s_audio_record_fun(STATE_PLAY_START,(unsigned int)&s_record_fd,(unsigned int)&s_RecordDt);
	if(0 == ret)
	{
		printf("--RecordThread--exit-\n");
#if SPEEX_ENABLE
		speex_preprocess_state_destroy(micprec);
#endif
		pthread_exit((void*)"exit");
		return 0;
	}
	assert(s_capture_frames > 0);
	buffer = (short*) malloc((ONE_SOUND_BUF_SIZE << 2));
	tempbuf = (unsigned char *) malloc(ONE_SOUND_BUF_SIZE);
	if((NULL == buffer) || (NULL == tempbuf))
	{
		printf("record malloc fialed\n");
		snd_pcm_close(rcd_handle);
		if(buffer)
		{
			free(buffer);
		}
		if(tempbuf)
		{
			free(tempbuf);
		}
#if SPEEX_ENABLE
		speex_preprocess_state_destroy(micprec);
#endif
		s_audio_record_fun(STATE_PLAY_STOP,(unsigned int)s_record_fd, (unsigned int)&s_RecordDt);
		pthread_exit((void*)"exit");
		return 0;
	}
	printf("--begine RecordThread---\n");
	printf("begine recode[file=%s]\n", s_RecordDt.VoiceFile);
	nRecord = 1;
	sumtime = 0.0f;
	fInterVal = 0.0f;
	s_RecordDt.timelenght = 0;
	readsample = ONE_SOUND_BUF_SIZE;
	frametm = GetWavLenght((readsample << 1));
	while(nRecord)
	{
		rc = snd_pcm_readi(rcd_handle, (char*)buffer, readsample);
		if (rc == -EPIPE) {
			/* EPIPE means overrun */
			printf( "CAPTURE overrun occurred\n");
			snd_pcm_prepare(rcd_handle);
		} else if (rc < 0) {
			printf("CAPTURE error from read: %s\n",
			snd_strerror(rc));
		} else if (rc != (int)readsample) {
			printf( "CAPTURE short read, read %d frames\n", rc);
		}
#if SPEEX_ENABLE
		speex_preprocess_run(micprec, buffer);
#endif
		for (i = 0; i < rc; i++)
			tempbuf[i] = (unsigned char)linear2alaw(buffer[2*i]); 
		
		if(s_audio_record_fun)
		{
    		fwrite(tempbuf,sizeof(unsigned char), rc, s_record_fd);	
		}
		fInterVal += frametm;
		sumtime += frametm;
		//播放时间回调
		if(s_audio_record_fun && (fInterVal >= 0.5))
		{
			fInterVal = 0.0f;
			s_audio_record_fun(STATE_PLAY_DISPLAY_TIME, (unsigned int)sumtime, 0);
		}
		if(sumtime >= COMMUNITICATION_TIMES)
		{
			nRecord = 0;
		}
	}
	if(s_audio_record_fun)
	{
		s_RecordDt.timelenght = (int)sumtime;
		s_audio_record_fun(STATE_PLAY_STOP,(unsigned int)s_record_fd, (unsigned int)&s_RecordDt);
	}
	snd_pcm_drop(rcd_handle);
	snd_pcm_close(rcd_handle);
	if(buffer)
	{
		free(buffer);
	}
	if(tempbuf)
	{
		free(tempbuf);
	}
#if SPEEX_ENABLE
	speex_preprocess_state_destroy(micprec);
#endif
	printf("RecordThread-normal exit--\n");
	pthread_exit((void*)"exit");
	return 0;
}

void StartRecord(PROCESS_STATE_CALLBACK callback)
{
	int nRet;
	StopAllAudio();
	s_audio_record_fun = callback;
	if((pthread_t)NULL == s_hRecord)
	{
	    nRet=pthread_create(&s_hRecord,NULL,(void*)RecordThread,NULL);
	    if(nRet!=0)
	    {
			printf ("Create record pthread error!\n");
	    }
	}
}

void SetPlayRecordPause(int pause)
{
	 nPlayPauseFlg = pause;
}
static void* PlayRecordThread(void* param)
{
	   int i,ret=0;
	   int rc;
	   int   time100ms;
	   float sumtime;
	   float frametm;
	   float fInterVal = 0.0;
	   int readsample = 0;
	   int couter = 0;
	   unsigned char*  tempbuf =NULL;
	   short* buffer = NULL;
	   FILE* fd = NULL;
	   char* pFilePath = (char*)param;
	   fd = fopen(pFilePath, "rb");
	   if(fd == NULL)
	   {
		   printf("can't open record file!\n");
		   if(s_playing_wave_fun)
		   {
		  	 s_playing_wave_fun(STATE_PLAY_STOP, 1, 0);
		   }
		   s_hPlayRecord =(pthread_t)NULL;
	  	   pthread_exit((void*)"exit");
		   return 0;
	   }
	   stop_sound();
	   StopRecord();
	   GetAudioPC();
	   nPlayRecord = 1;
	   buffer = (short*)malloc(ONE_SOUND_BUF_SIZE << 2);
	   tempbuf = (unsigned char*)malloc(ONE_SOUND_BUF_SIZE);
	   if((NULL == buffer) || (NULL == tempbuf))
		{
			printf("play record malloc fialed\n");
			fclose(fd);
			if(buffer)
			{
				free(buffer);
			}
			if(tempbuf)
			{
				free(tempbuf);
			}
			if(s_playing_wave_fun)
			 {
			   	s_playing_wave_fun(STATE_PLAY_STOP, 0, 0);
			 }
			ReleaseAudioPC();
			pthread_exit((void*)"exit");
			return 0;
		}
	   if(s_playing_wave_fun)
	   {
	   		s_playing_wave_fun(STATE_PLAY_START, 0, 0);
	   }
	   
	   if(s_CurVol != s_PlayVol)
	   {
	  		Inner_SetPlayVol(s_PlayVol);
	  		s_CurVol = s_PlayVol;
	   }  
	   readsample = ONE_SOUND_BUF_SIZE;
	   sumtime = 0.0;
	   fInterVal = 0.0;
	   frametm = GetWavLenght((readsample << 1));
		//printf("----frametm=%f-----\n",frametm);
	   while((1 == nPlayRecord) || (couter < PLAY_WAV_ALLOW_CLOSE))
	   {
           ret=fread(tempbuf, sizeof(unsigned char), readsample, fd);
		   if(ret>0)
		   {
		   	   if(ret < readsample)
		   	   {
				   for(i = ret; i < readsample; i++)
				   {
					  buffer[2*i] = 0;
					  buffer[2*i + 1] = 0;
				   }
			   }
			   for(i=0;i<ret;i++)
			   {
				   buffer[2*i] = (short)alaw2linear(tempbuf[i]);
				   buffer[2*i + 1] = buffer[2*i];
			   }
			   if((0 == nPlayRecord)&&(couter >= PLAY_WAV_ALLOW_CLOSE))
			   {
				   break;
			   }
			   rc = snd_pcm_writei(s_fdAudio, (char*)buffer, ONE_SOUND_BUF_SIZE);
			   if (rc == -EPIPE) {
					/* EPIPE means overrun */
					printf( "play record overrun occurred\n");
					snd_pcm_prepare(s_fdAudio);
				} else if (rc < 0) {
					printf("play record error from read: %s\n",
					snd_strerror(rc));
				} else if (rc != (int)ONE_SOUND_BUF_SIZE) {
					printf( "play record short write %d frames\n", rc);
				}
				couter++;
			   fInterVal += frametm;
			   sumtime += frametm;
			    //printf("read ret=%d, sumtime=%f\n", ret, sumtime);
			   if(s_playing_wave_fun && (fInterVal >= 0.5f))
				{
					fInterVal = 0.0f;
					time100ms = (int)(10*sumtime);
					s_playing_wave_fun(STATE_PLAY_DISPLAY_TIME, time100ms, 0);
				}
		   }
		   if(feof(fd))
		   {
		   	  printf("file be readed to the end--\n");
			  break;
		   }
	   }
	   if(s_playing_wave_fun)
	   {
	   		s_playing_wave_fun(STATE_PLAY_STOP, 0, 0);
	   }
	   fclose(fd);
	   if(buffer)
	   {
			free(buffer);
	   }
	   if(tempbuf)
	   {
			free(tempbuf);
	   }
	   ReleaseAudioPC();
	   //playdtmf = 0;
	   pthread_exit((void*)"exit");
	   return 0;
	  
}

int PlayRecord(char *szFileName, PROCESS_STATE_CALLBACK callback)
{
	int nRet;
	StopAllAudio();
	s_playing_wave_fun = callback;
	if((pthread_t)NULL == s_hPlayRecord)
	{	
		nRet=pthread_create(&s_hPlayRecord,NULL,(void*)PlayRecordThread,(void*)szFileName);
		if(nRet!=0)
		{
			printf("Create Decode G729 pthread error!\n");
		}
	}
	return 0;
}


static float Get_RiffWave_Lenght(int lenght, int byte_rate)
{
	float times = 0.0f;
	times = ((float)lenght) / ((float)byte_rate);
	return times;
}

/* 
 * open *.wav file 
 */  
static int WaveOpen(FILE* fp, wave_header_t* pheader)  
{  
    unsigned char temp = 0;  
    char *channel_mappings[] = {NULL,"mono","stereo"};  
    unsigned int total_time = 0;  
  
    /* read heade information */  
    if(4 != fread(pheader->riff, sizeof(unsigned char), 4, fp))           /* RIFF chunk */  
    {  
        printf("read riff error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->size, sizeof(unsigned int), 1, fp))         /* SIZE : from here to file end */  
    {  
        printf("read size error!\n");  
        return -1;  
    }  
    if(4 != fread(pheader->wave_flag, sizeof(unsigned char), 4, fp))      /* wave file flag */  
    {  
        printf("read wave_flag error!\n");  
        return -1;  
    }  
    if(4 != fread(pheader->fmt, sizeof(unsigned char), 4, fp))             /* fmt chunk */  
    {  
        printf("read fmt error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->fmt_len, sizeof(unsigned int), 1, fp))       /* fmt length */  
    {  
        printf("read fmt_len error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->tag, sizeof(unsigned short), 1, fp))           /* tag : PCM or not */  
    {  
        printf("read tag error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->channels, sizeof(unsigned short), 1, fp))      /* channels */  
    {  
        printf("read channels error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->samp_freq, sizeof(unsigned int), 1, fp))      /* samp_freq */  
    {  
        printf("read samp_freq error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->byte_rate, sizeof(unsigned int), 1, fp))      /* byte_rate : decode how many bytes per second */  
    {                                                                       /* byte_rate = samp_freq * bit_samp */  
        printf("read byte_rate error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->block_align, sizeof(unsigned short), 1, fp))       /* quantize bytes for per samp point */  
    {  
        printf("read byte_samp error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->bit_samp, sizeof(unsigned short), 1, fp))        /* quantize bits for per samp point */  
    {                                                                        /* bit_samp = byte_samp * 8 */  
        printf("read bit_samp error!\n");  
        return -1;  
    }  
  
   //<span style="color:#ff0000;">   </span><span style="color:#000000;">/* jump to "data" for reading data */  
    do  
    {  
        fread(&temp, sizeof(unsigned char), 1, fp);  
    }  
    while('d' != temp);  
    pheader->data_flag[0] = temp;  
    if(3 != fread(&pheader->data_flag[1], sizeof(unsigned char), 3, fp))                 /* data chunk */  
    {  
        printf("read header data error!\n");  
        return -1;  
    }  
    if(1 != fread(&pheader->length, sizeof(unsigned int), 1, fp))                  /* data length */  
    {  
        printf("read length error!\n");  
    }  
  
    /* jduge data chunk flag */  
    if(strncmp(pheader->data_flag, "data", 4))  
    {  
        printf("error : cannot read data!\n");  
        return -1;  
    }  
  	
    total_time = pheader->length / pheader->byte_rate;  
    /* printf file header information */  
    printf("%s %ldHz %dbit, DataLen: %d, Rate: %d, Length: %2d\n",  
           channel_mappings[pheader->channels],             //声道  
           pheader->samp_freq,                              //采样频率  
           pheader->bit_samp,                               //每个采样点的量化位数  
           pheader->length,  
           pheader->byte_rate,  
           total_time);  
  
	return 0;
}  

static void* PlayRiffFileThread(void* param)
{
	   int ret=0;
	   int rc;
	   int size;
	   int counter;
	   int readsum;
	   char* wave_buff = NULL;
	   FILE* fd = NULL;
	   char* pFilePath = (char*)param;
	   struct timeval tempval;
	   wave_header_t waveheader;
	   printf("PlayRiffFileThread=%s-----\n",pFilePath);
	   fd = fopen(pFilePath, "rb");
	   if(fd == NULL)
	   {
		   printf("can't open record file!\n");
		   if(s_playing_wave_fun)
	   	   {
		   		s_playing_wave_fun(STATE_PLAY_STOP, 1, 0);
		   }
	  	   pthread_exit((void*)"exit");
		   return 0;
	   }
	   stop_sound();
	   StopRecord();
	   nPlayRecord = 1;
	   if(s_playing_wave_fun)
	   {
	   		s_playing_wave_fun(STATE_PLAY_START, 0, 0);
	   }
	   counter = 0;
	   while((s_fdAudio)&&(counter < WAIT_SND_PCM_TIMEOUT))
	   {
	   		counter++;
	   		tempval.tv_sec = 0;
	    	tempval.tv_usec = 120000;
	    	select(0, NULL, NULL, NULL, &tempval);
			printf("wait until pcm device released\n");
	   }
	   if(s_fdAudio)
	   {
			snd_pcm_drain(s_fdAudio);
			snd_pcm_close(s_fdAudio);
	   }
	   pthread_mutex_lock(&g_mutex_get);
	   ret = WaveOpen(fd, &waveheader);
	   if(ret != 0)
	   {
		   printf("can't open record file!\n");
		   goto PLAY_EXIT;
	   }
	   printf("riff wav format(%d,%d,%d)\n",waveheader.bit_samp,waveheader.samp_freq, waveheader.channels);
	   if(16 != waveheader.bit_samp)
	   {
		   printf("wav format have not suported!\n");
		   goto PLAY_EXIT;
	   }
	   rc = snd_pcm_open(&s_fdAudio, "default", SND_PCM_STREAM_PLAYBACK, 0);
	   if (rc < 0)
	   {
			s_fdAudio =NULL;
			printf("unable to open pcm device again: %s\n",
			        snd_strerror(rc));
			goto PLAY_EXIT;
	   }
	   //printf("---PlayRiffFileThread s_fdAudio, SND_PCM_FORMAT_S16_LE,  waveheader.samp_freq, waveheader.channels, &s_sound_buff_size,&s_sound_frames=%d\n", s_sound_frames);
	   ret = set_alsa_playparam(s_fdAudio, SND_PCM_FORMAT_S16_LE,  waveheader.samp_freq, waveheader.channels, &s_sound_buff_size,&s_sound_frames);
	   //printf("---PlayRiffFileThread s_fdAudio, SND_PCM_FORMAT_S16_LE,  waveheader.samp_freq, waveheader.channels, &s_sound_buff_size,&s_sound_frames=%d\n", s_sound_frames);

	   if(ret < 0)
	   {
	   	   printf("playriff set_alsa_playparam fialed\n");
		   snd_pcm_close(s_fdAudio);
		   s_fdAudio = NULL;
		   goto PLAY_EXIT;
	   }
	   printf("play riff wave period size=%d\n",s_sound_frames);
	   size = waveheader.channels*ONE_SOUND_BUF_SIZE*2;
	   wave_buff = (char*)malloc(size);
	   if(NULL == wave_buff)
	   {
		   printf("playriff malloc fialed\n");
		   snd_pcm_close(s_fdAudio);
		   s_fdAudio = NULL;
		   goto PLAY_EXIT;
	   }
	   if(s_CurVol != s_RingVol)
	   {
			Inner_SetPlayVol(s_RingVol);
			s_CurVol = s_RingVol;
	   }
	   readsum = 0;
	   counter = 0;
	   while(1)
	   {
	   	   if((0 == nPlayRecord)&&(counter > PLAY_WAV_ALLOW_CLOSE))
	   	   {
				break;
		   }
           ret=fread(wave_buff, sizeof(unsigned char), size, fd);
		   if(ret>0)
		   {
		   		readsum += ret;
				if(readsum >= waveheader.length)
				{
					ret = (ret - (readsum - waveheader.length));
				}
				if(ret == size)
				{
					rc = snd_pcm_writei(s_fdAudio, wave_buff, ONE_SOUND_BUF_SIZE);
					if (rc == -EPIPE) {
						/* EPIPE means overrun */
						printf( "play fiff overrun occurred\n");
						snd_pcm_prepare(s_fdAudio);
					} else if (rc < 0) {
						printf("play fiff error from read: %s\n",
						snd_strerror(rc));
					} else if (rc != (int)ONE_SOUND_BUF_SIZE) {
						printf( "play fiff short write, %d frames\n", rc);
					}
					counter++;
				}  
		   }
		   if((readsum >= waveheader.length) || feof(fd))
		   {
		   		printf("file be readed to the end--\n");
		   		if(1 == s_replay_flg)
		   		{
		   			if(0 == nPlayRecord)
		   			{
						break;
					}
		   			fseek(fd, 0, 0);
					WaveOpen(fd, &waveheader);
					readsum = 0;
		   		}
				else
				{
					break;
				}
		   }
		   
	   }
	   snd_pcm_drain(s_fdAudio);
	   snd_pcm_close(s_fdAudio);
	   s_fdAudio = NULL;
PLAY_EXIT:
	   pthread_mutex_unlock(&g_mutex_get);
	   if(s_playing_wave_fun)
	   {
	   		s_playing_wave_fun(STATE_PLAY_STOP, 0, 0);
	   }
	   if(wave_buff)
	   {
			free(wave_buff);
	   }
	   fclose(fd);
	   s_replay_flg = 1;
	   printf("PlayRiffFileThread exit normal\n");
	   pthread_exit((void*)"exit");
	   return 0;
	  
}


int PlayRiffWave(char *szFileName,  int replayflg, PROCESS_STATE_CALLBACK callback )
{
	int nRet;
	StopAllAudio();
	s_replay_flg = replayflg;
	s_playing_wave_fun = callback;
	if((pthread_t)NULL == s_hPlayRecord)
	{	
		nRet=pthread_create(&s_hPlayRecord,NULL,(void*)PlayRiffFileThread,(void*)szFileName);
		if(nRet!=0)
		{
			printf("Create Decode G729 pthread error!\n");
		}
	}
	return 0;
}

static int write_mute_data(int num, int framesize)
{
	int i, j;
	short* pkt;	
	for(i = 0; i < num; i++)
	{
		pkt = (short*)malloc((framesize << 1));
		if(pkt == NULL)
		{
			i--;
			printf("malloc toppacker fialed\n");
			usleep(20000);
		}
		for(j = 0; j < framesize; j++)
		{
			pkt[j] = 0;//alaw2linear(0xD5);
		}
		block_queue_put(&s_send_queue, pkt, framesize, i, 0);
	}
	return 0;
}

//unsigned char  s_charbuf[2*BUF_SIZE];
//short s_shortbuf[1024];
static short s_readbuf[512];
static short s_writebuf[512];
static short s_mic_buf[512];
static short s_mute_data[512];
static void* WritingDataThread(void* param)
{
	int i, j;
	int rc;
	int mute_fill = 0;
	int queuesize = 0;
	int readsample;
	int readbyte;
	snd_pcm_t *rcd_handle;
	int64_t  tmbegin, tmend;
	//int write_dex = 0;
	unsigned int Param1, Param2, Param3;
	//short  mic_buf[320];
	short* ref_buf = NULL;
	short* e_buf = NULL;
	TOPACKET* pkt;
	int reqnum = 15;
	short *pPcm;
	int frame_size;
#if SPEEX_ENABLE
	int filter_length;
	int sampleRate = PLAY_RATE;
	int denoise = 1;
	int echodb = -200;
	int noise_suppress = -100;
    SpeexEchoState *st;
    SpeexPreprocessState *den;
	SpeexPreprocessState *micprec;
#endif
	struct timeval tempval;
	s_writing_quit = 0;
	memset(s_mute_data, 0, sizeof(s_mute_data));
    printf("WritingDataThread------\n");
	if(GetAudioPC())
	{
		rc = CreateCaptureDv(&rcd_handle);
		if(rc < 0)
		{
			ReleaseAudioPC();
			printf("WritingDataThread exit CreateCaptureDv fialed\n");
			pthread_exit((void*)"exit");
			return 0;
		}
#if ((defined S500_CLOCK) || (defined S500_FR)) 
		assert((BUFFER_SIZE % s_sound_frames == 0));
		frame_size = BUFFER_SIZE;
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	    frame_size = s_sound_frames;
#endif

#if SPEEX_ENABLE
		filter_length = FILTER_LENGHT;
		//mic 预处理
		micprec = speex_preprocess_state_init(frame_size, sampleRate);   //降噪
		speex_preprocess_ctl(micprec, SPEEX_PREPROCESS_SET_DENOISE, &denoise);  //降噪分贝
		speex_preprocess_ctl(micprec, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);

		st = speex_echo_state_init(frame_size, filter_length);
	    den = speex_preprocess_state_init(frame_size, sampleRate);
	    speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
	    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);
		speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &echodb);
		speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &noise_suppress);
#endif		
		printf("write_mute_data\n");
#if ((defined S500_CLOCK) || (defined S500_FR)) 
		write_mute_data(reqnum, frame_size);
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	    write_mute_data(reqnum, BUFFER_SIZE);
#endif
		
#if 1
		//等待队列里有足够的数据开始播放
		do
		{
			if(1 == s_writing_quit)
			{
				break;
			}
			tempval.tv_sec = 0;
		    tempval.tv_usec = 60000;
		    select(0, NULL, NULL, NULL, &tempval);
			queuesize = block_queue_get_nums(&s_write_queue);
		}while(queuesize <= QUEUE_LEN_THROSHOLD);
		if(1 == s_writing_quit)
		{
			printf("WritingDataThread exit quit\n");
			snd_pcm_drain(rcd_handle);
	   		snd_pcm_close(rcd_handle);
			ReleaseAudioPC();
			pthread_exit((void*)"exit");
			return 0;
		}
		rc = snd_pcm_readi(rcd_handle, (char*)s_mic_buf, s_sound_frames);
		if (rc == -EPIPE) {
			/* EPIPE means overrun */
			printf( "CAPTURE overrun occurred\n");
			snd_pcm_prepare(rcd_handle);
		} else if (rc < 0) {
			printf("CAPTURE error from read: %s\n",
			snd_strerror(rc));
		} else if (rc != (int)s_sound_frames) {
			printf( "CAPTURE short read, read %d frames\n", rc);
		}
		memset(s_mic_buf, 0, sizeof(s_mic_buf));
		//tempval.tv_sec = 0;
		//tempval.tv_usec = 10000;             //等待mic缓存的数据
		//select(0, NULL, NULL, NULL, &tempval);
		//readbyte = 64; //写入播放缓存的数据
		//readbyte = ONE_SOUND_BUF_SIZE;
		snd_pcm_writei(s_fdAudio, s_mic_buf, s_sound_frames);
		//snd_pcm_writei(s_fdAudio, s_mic_buf, s_sound_frames);
#endif
		readbyte = (frame_size << 1);
		while(0 == s_writing_quit)
		{
			if(1 == s_writing_quit)
			{
				break;
			}
			reqnum++;
			//获取接收数据
			ref_buf = (short*)block_queue_get(&s_write_queue, &Param1, &Param2, &Param3);
			if(ref_buf)
			{
				
				//i = 0;
				for(j = 0; j < frame_size; j++){
					s_writebuf[2*j] = ref_buf[j];
					s_writebuf[2*j + 1] = ref_buf[j];
				}
				
				i = 0;
				while(i < frame_size) {
					rc = snd_pcm_writei(s_fdAudio, (char*)(s_writebuf + 2*i), s_sound_frames);
					if (rc == -EPIPE) {
						/* EPIPE means overrun */
						printf( "write undrun occurred\n");
						snd_pcm_prepare(s_fdAudio);
						snd_pcm_writei(s_fdAudio, (char*)(s_mute_data), 256);
						snd_pcm_writei(s_fdAudio, (char*)(s_mute_data), 256);
					} else if (rc < 0) {
						printf("write error from: %s\n",
						snd_strerror(rc));
					} else if (rc != s_sound_frames) {
						printf( "write short, write %d frames\n", rc);
					}

					rc = snd_pcm_readi(rcd_handle, (char*)(s_readbuf + 2*i), s_sound_frames);
					if (rc == -EPIPE) {
						/* EPIPE means overrun */
						printf( "CAPTURE overrun occurred\n");
						snd_pcm_prepare(rcd_handle);
					} else if (rc < 0) {
						printf("CAPTURE error from read: %s\n",
						snd_strerror(rc));
					} else if (rc != (int)s_sound_frames) {
						printf( "CAPTURE short read, read %d frames\n", rc);
					}
					i += s_sound_frames;
				}
				e_buf = (short*)malloc(readbyte);
				if(e_buf == NULL)
				{
					printf("malloc toppacker fialed\n");
					free(ref_buf);
					usleep(20000);
					continue;
				}
#if SPEEX_ENABLE
				for(j = 0; j < frame_size; j++){
					s_mic_buf[j] = s_readbuf[2*j];
				}
				//tmbegin = get_system_utime(); // -1:3000~4000 us  -2:4000~5000
				speex_preprocess_run(micprec, s_mic_buf);
				speex_echo_cancellation(st, s_mic_buf, ref_buf, e_buf);
	      		speex_preprocess_run(den, e_buf);
				//printf("read write process time = %d(us)---\n", (int)(get_system_utime() - tmbegin));
#else
				for(j = 0; j < frame_size; j++){
					e_buf[j] = s_readbuf[2*j];
				}
#endif
                block_queue_put(&s_send_queue, (unsigned int)e_buf, frame_size, reqnum, 0);
				queuesize = block_queue_get_nums(&s_write_queue);
				if(0 == queuesize)
				{
					block_queue_put(&s_write_queue, (unsigned int)ref_buf, Param1, Param2, 0);
					printf("put data again\n");
				}
				else
				{
					free(ref_buf);
				}
			}
			else
			{
				s_writing_quit = 1;
				continue;
			}
		}
		snd_pcm_drain(rcd_handle);
	   	snd_pcm_close(rcd_handle);
#if SPEEX_ENABLE
		speex_preprocess_state_destroy(micprec);
		speex_echo_state_destroy(st);
   		speex_preprocess_state_destroy(den);
#endif
		ReleaseAudioPC();
	}
	printf("WritingDataThread exit normal\n");
	pthread_exit((void*)"exit");
	return 0;
}


//开始发送, 创建写缓存队列
int BegineSendingData()
{
	int	ret = block_queue_init(&s_send_queue, 128);
	if(0 != ret)
	{
		printf("BegineWritingData:queue_init\n");
		return -1;
	}
	s_send_queue_flg = 1;
	return 0;
}

int IsInitialedQueue()
{
	return s_send_queue_flg;
}

int StopTalkImmediatly()
{
	s_writing_quit = 1;
	block_queue_abort(&s_write_queue);
	block_queue_abort(&s_send_queue);
	s_send_queue_flg = 0;
	return 0;
}

//结束发送, 销毁写缓存队列
int EndSendingData(QDataFree freedata)
{
	block_queue_abort(&s_send_queue);
	block_queue_flush(&s_send_queue, freedata);
	block_queue_destroy(&s_send_queue);
	s_send_queue_flg = 0;
	return 0;
}

//获取数据到缓存
unsigned int GetSendingData(unsigned int* DataLen, unsigned int* param2,unsigned int* param3)
{
	return block_queue_get(&s_send_queue, DataLen,  param2, param3);
}

//开始写入, 创建写缓存队列
int BegineWritingData()
{
	int ret;


	

	
	//使用对讲音量
	if(s_CurVol != s_PlayVol)
    {
  		Inner_SetPlayVol(s_PlayVol);
  		s_CurVol = s_PlayVol;
    }
	
	ret = block_queue_init(&s_write_queue, 128);
	if(0 != ret)
	{
		printf("BegineWritingData:queue_init\n");
		return -1;
	}
	BegineSendingData();
	//write_mute_data();
	pthread_mutex_lock(&g_mutex);
	if((pthread_t)NULL == s_hWriting)
	{	
		ret=pthread_create(&s_hWriting,NULL,(void*)WritingDataThread,(void*)0);
		if(ret!=0)
		{
			printf("Create writing data pthread error!\n");
		}
	}
	pthread_mutex_unlock(&g_mutex);
	return ret;
}

//结束写入, 销毁写缓存队列
int EndWritingData(QDataFree freedata)
{
	s_writing_quit = 1;
	block_queue_abort(&s_write_queue);
	block_queue_flush(&s_write_queue, freedata);
	block_queue_destroy(&s_write_queue);
	EndSendingData(freedata);
	if(s_hWriting)
	{
		pthread_join(s_hWriting, NULL); //wait the thread stopped
		s_hWriting = (pthread_t)NULL;
	}


	return 0;
}

//写数据到缓存
int WritingData(unsigned int pData, unsigned int DataLen, unsigned int reqnum)
{
	return block_queue_put(&s_write_queue, pData, DataLen, reqnum, 0);
}

static short s_shortbuf[512];

int ReadOneFrameData(snd_pcm_t* capture, unsigned char buf[], int framesize)
{
	int rc;
	int i;
	
	rc = snd_pcm_readi(capture, (char*)s_shortbuf, s_capture_frames);
	if (rc == -EPIPE) {
		/* EPIPE means overrun */
		printf( "CAPTURE overrun occurred\n");
		snd_pcm_prepare(capture);
		return 0;
	} else if (rc < 0) {
		printf("CAPTURE error from read: %s\n",
		snd_strerror(rc));
	} else if (rc != (int)s_capture_frames) {
		printf( "CAPTURE short read, read %d frames\n", rc);
	}
	if(rc < s_capture_frames)
	{
		for(i = rc; i < s_capture_frames; i++)
		{
			 buf[i] = 0;
		}
	}
    for (i = 0; i < rc; i++)  
	   buf[i] = (unsigned char)linear2alaw(s_shortbuf[i]);    
	return  s_capture_frames;
}


float GetWavLenght(int datalen)
{
	float playtime = 0.0f;
	int samplenum = (datalen >> 1);
	playtime = (((float)(samplenum)) / (FLOAT_RATE));
	//printf("--GetWaveDataTime---%f----\n",playtime);
	return playtime;
}

unsigned int GetSoundFrameSize()
{
	return s_sound_frames;
}
unsigned int GetCaptureFrameSize()
{
	return s_capture_frames;
}


int WriteOneFrameData(unsigned char buf[], int framesize)
{
	int i;
	int rc;
	for (i = 0; i < framesize; i++)
			s_shortbuf[i] =  alaw2linear(buf[i]);
	rc = snd_pcm_writei(s_fdAudio, (char*)s_shortbuf, framesize);
	if (rc == -EPIPE) {
		/* EPIPE means overrun */
		printf( "write overrun occurred\n");
		snd_pcm_prepare(s_fdAudio);
	} else if (rc < 0) {
		printf("write error from read: %s\n",
		snd_strerror(rc));
	} else if (rc != (int)framesize) {
		printf( "write short, write %d frames\n", rc);
	}
	return rc;	
}
#if 0
float GetWaveDataTime(int datalen)
{
	float playtime = 0.0f;
	//int samplenum = (4*datalen);
	//printf("--GetWaveDataTime---%d----\n",samplenum);
	//playtime = (((float)(samplenum)) / (FLOAT_RATE));
	//printf("--GetWaveDataTime---%f----\n",playtime);
	return playtime;
}
#endif

extern void write_i2c(int select);

int InitAudio()
{
	memset(&s_write_queue, 0, sizeof(s_write_queue));
	memset(&s_send_queue, 0, sizeof(s_send_queue));
	s_write_queue.abortflg = 1;
	s_send_queue.abortflg = 1;
	int ret = pthread_mutex_init(&g_mutex,NULL);
	if(ret != 0)
	{
		printf("debug:pthread_mutex_init (g_mutex)fialed\n");
	}
	ret = pthread_mutex_init(&g_mutex_get,NULL);
	if(ret != 0)
	{
		printf("debug:pthread_mutex_init (g_mutex_get)fialed\n");
	}
	//write_i2c();
	return ret;
}

void SetAudioSystemFun(PROCESS_STATE_CALLBACK system_fun)
{
	s_alase_system_fun = system_fun;
}
void UninitAudio()
{
	s_alase_system_fun = NULL;
	pthread_mutex_destroy(&g_mutex);
	pthread_mutex_destroy(&g_mutex_get);
}

snd_pcm_t* GetAudioPC()
{
	int rc;
	//s_immediatly_flg = 0;
	int counter = 0;
	struct timeval tempval;
	//printf("-begin-GetAudioPC--\n");
    while((s_fdAudio)&&(counter < WAIT_SND_PCM_TIMEOUT))
    {
   		counter++;
   		tempval.tv_sec = 0;
	    tempval.tv_usec = 120000;
	    select(0, NULL, NULL, NULL, &tempval);
		printf("GetAudioPC:wait pcm released\n");
    }
	if(s_fdAudio)
	{
		snd_pcm_drain(s_fdAudio);
		snd_pcm_close(s_fdAudio);
	}
	pthread_mutex_lock(&g_mutex_get);
	/* Open PCM device for playback. */
	rc = snd_pcm_open(&s_fdAudio, "default",
	                SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0)
	{
		printf("unable to open pcm device: %s\n",
		        snd_strerror(rc));
		snd_pcm_close(s_fdAudio);
		usleep(50000);
		rc = snd_pcm_open(&s_fdAudio, "default",
	                SND_PCM_STREAM_PLAYBACK, 0);
	}
	if (rc < 0)
	{
		s_fdAudio =NULL;
		printf("unable to open pcm device again: %s\n",
		        snd_strerror(rc));
	}
	else
	{
		//////////////////////////////////////
		//printf("------snd open pcm\n");
		s_sound_frames = 0;
		//printf("-----1--s_fdAudio-----GetAudioPC s_sound_frames=%d \n", s_sound_frames);
		set_alsa_playparam(s_fdAudio, SND_PCM_FORMAT_S16_LE, PLAY_RATE, 2, &s_sound_buff_size, &s_sound_frames);
		//printf("-----2--s_fdAudio-----GetAudioPC s_sound_frames=%d \n", s_sound_frames);
	}
	//printf("--GetAudioPC-finish-\n");
	//pthread_mutex_unlock(&g_mutex_get);
	return s_fdAudio;
}

void ResetAudioPC()
{
	int ctlfd;
	int arg;
	usleep(500000);
	//write_i2c();
	printf("Reset20709Device finish\n");
}


void ReleaseAudioPC()
{
	//pthread_mutex_lock(&g_mutex_get);
	if(s_fdAudio)
	{
		snd_pcm_drain(s_fdAudio);
		snd_pcm_close(s_fdAudio);
		s_fdAudio = NULL;
	}
	//usleep(100000);
	//printf("--ReleaseAudioPC--\n");
	pthread_mutex_unlock(&g_mutex_get);
}


void StopRecord()
{
	nRecord=0;
	if(s_hRecord)
	{
		pthread_join(s_hRecord, NULL); //wait the thread stopped
		s_hRecord = (pthread_t)NULL;
		s_audio_record_fun = NULL;
	}
	return;
}

void StopPlayRecord()
{
   int counter = 0;
   nPlayRecord = 0;
   if(s_hPlayRecord)
   {
		pthread_join(s_hPlayRecord, NULL); //wait the thread stopped
		s_hPlayRecord = (pthread_t)NULL;
		/*
		while(s_hPlayRecord)
		{
			nPlayRecord = 0;
			usleep(80000);
			counter++;
			if(counter >= COUNTER_OVER)
			{
				break;
			}
			printf("wait for StopPlayRecord exiting\n");
		}
		*/
		s_playing_wave_fun = NULL;
   }
}


void StopAllAudio()
{
	stop_sound();
	StopRecord();
	StopPlayRecord();
	return;
}


