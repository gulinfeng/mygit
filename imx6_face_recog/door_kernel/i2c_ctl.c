#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>


//#define I2C_DISABLE

#define I2C_ADDRESS	0x65

#define   REG_BASE				0XA0
#define     DAC_VOLUMECTL0                                                    (0x3)

///right ----high  left ----low   0xbeb6


typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned int uint;

#define noof(a) (sizeof(a)/sizeof(a[0]))

char *device=NULL;
char *boot=NULL;
char *image=NULL;
int   dev=-1;
int   opt_f=0;
int   opt_s=0;
int   opt_v=0;
int   opt_i=0;
int   opt_x=0;
FILE *fpl=NULL;
extern int g_nMicVal;
//extern setGph0();
byte data[2]={0xbe,0xb6};

void panic(const char *fmt,...)
{
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	exit(1);
}

int channel_read(word addr,byte *buf,uint len)
{
	struct i2c_rdwr_ioctl_data d;
	struct i2c_msg             m[2];
	byte			     dat[1];
	///change 
	int rc;
	byte out[2];
	
	dat[0]=(byte)(addr);
//	dat[1]=(byte)(addr>>0);
	
	m[0].addr=I2C_ADDRESS; //act2065c 鍦板潃
	m[0].flags=0; //write 
	m[0].len=1;  //鍦板潃闀垮害 16Bit
	m[0].buf=dat;
	
	m[1].addr=I2C_ADDRESS;
	m[1].flags=I2C_M_RD;
	m[1].len=len;
	m[1].buf=buf;
	
	d.msgs=m;
	d.nmsgs=2;
	
	rc=ioctl(dev,I2C_RDWR,&d);
	if(rc<0)
	{
		printf("read ,I2C_RDWD failed: %d\n",errno);
		return (-1);
	}
	return rc;
}

int channel_write(word addr,word buf)
{
	struct i2c_rdwr_ioctl_data d;
	struct i2c_msg             m[1];
	byte                       dat[1024+1];
	int rc;
	//if (len+2>noof(dat))
		//return -1;
	
	dat[0]=(byte)(addr);
	//dat[1]=(byte)(addr>>0);
	//memcpy(&dat[1],buf,len);
	dat[1]=(byte)(buf>>8); //high 
	dat[2]=(byte)(buf>>0);
	
	m[0].addr=I2C_ADDRESS;
	m[0].flags=0;
	m[0].len=3;
	m[0].buf=dat;
	
	d.msgs=m;
	d.nmsgs=1;
	rc=ioctl(dev,I2C_RDWR,&d);
	 if (rc < 0) {
   printf("write, I2C_RDWR failed: %d.\n", errno);
   return (-1);
  }
	return rc;
}

int channel_byte(byte addr, unsigned short *word)
{
	byte dat[2];
	//unsigned short rx_data;
	if (channel_read(addr,&dat[0],2))
	{
		*word =((dat[0]<<8)|(dat[1]));
		printf("addr(0x%x):rx_data is %x\n",addr, (*word));
		return 0;
	}
	//panic("I2C read error\n");
    return 1;
}

int channel_xwrite(word addr, word buf)
{
   int ret;
   unsigned short dat;
	{
		channel_write(addr,buf);
		usleep(100);
		ret = channel_byte(addr, &dat);
		if((0 == ret)&&(dat == buf))
			return 0;
	}
	printf("write i2c addr %x fail!\n",addr);
	return 1;
}

/*

int check_channel_x(word addr,byte *buf,uint len)
{
	int i;
	byte dat = 0;
	usleep(40000);
	dat=channel_byte(addr);
	return (dat == buf[0] ? 1 : 0);
}
*/
#ifdef I2C_DISABLE
int set_mic_by_i2c(int level)
{
	return 0;
}

int set_vol_by_i2c(int vol)
{
	return 0;
}

int mutil_tune_mic_by_i2c(int amp1g0,  int amp0gr1)
{
	return 0;
}
#else
/*
*麦克设置 0 - 15 默认 3 ->db
*/
int set_mic_by_i2c(int level)
{
	int ret = 0;
	int timeout = 0;
	unsigned short mic_dt = 0;
	unsigned short  mask = 0xfc3f;
	unsigned short  readed_rg = 0x0;
	level = level >= 0 ? level : 0;
	level = level <=  15 ? level : 15;
	if ((dev=open("/dev/i2c-0",O_RDWR))<0)
		panic("Can't open i2c device '%s'\n",device);
	
	//mic_dt = (unsigned short)(16 * ((float)(level) / 100.0));
	mic_dt = (unsigned short)level;	
	do{
		ret = channel_byte(0xa8, &readed_rg);    // 0xab
		timeout++;
		if(timeout > 2){
			return -1;
		}
		
	}while(ret != 0);

	mic_dt =  mic_dt << 6;

	readed_rg = (readed_rg & mask) | (mic_dt);
	
	do{
		ret = channel_xwrite(0xa8, readed_rg);
		timeout++;
		if(timeout > 2){
			return -1;
		}
		
	}while(ret != 0);
	printf("write i2c addr 0xa8 = 0x%x\n",readed_rg);
	return ret;
	
}

/*
*麦克设置 0 - 15 默认 7, 1
*/

int mutil_tune_mic_by_i2c(int amp1g0,  int amp0gr1)
{
	int ret = 0;
	int timeout = 0;
	unsigned short mic_dt = 0;
	unsigned short  mask_amplg0 = 0xff;
	unsigned short  mask_amp0gr1 = 0xfffc;
	unsigned short  readed_rg = 0x0;
	amp1g0 = amp1g0 >= 0 ? amp1g0 : 0;
	amp1g0 = amp1g0 <=  15 ? amp1g0 : 15;
	
	amp0gr1 = amp0gr1 >= 0 ? amp0gr1 : 0;
	amp0gr1 = amp0gr1 <=  3 ? amp0gr1 : 3;
	if ((dev=open("/dev/i2c-0",O_RDWR))<0)
		panic("Can't open i2c device '%s'\n",device);
	
	//mic_dt = (unsigned short)(16 * ((float)(level) / 100.0));
		
	do{
		ret = channel_byte(0xab, &readed_rg);    // 0xab
		timeout++;
		if(timeout > 2){
			return -1;
		}
		
	}while(ret != 0);

	mic_dt = (unsigned short)amp1g0;
	mic_dt =  mic_dt << 12;
	readed_rg = (readed_rg & mask_amplg0); 	
	readed_rg = readed_rg  | mic_dt;
	mic_dt = (unsigned short)amp1g0;
	mic_dt =  mic_dt << 8;
	readed_rg = readed_rg  | mic_dt;

	mic_dt = (unsigned short)amp0gr1;
	readed_rg = (readed_rg & mask_amp0gr1);
	readed_rg = readed_rg  | mic_dt;
	
	do{
		ret = channel_xwrite(0xab, readed_rg);
		timeout++;
		if(timeout > 2){
			return -1;
		}
		
	}while(ret != 0);
	printf("write i2c addr 0xab = 0x%x\n",readed_rg);
	return ret;
	
}
/*
*音量设置 0 -100   默认57
*/
int set_vol_by_i2c(int vol)
{
	int ret = 0;
	int timeout = 0;
	unsigned char  vol_base = 0xbe;
	unsigned short channel_vol = 0xbebe;
	if ((dev=open("/dev/i2c-0",O_RDWR))<0)
		panic("Can't open i2c device '%s'\n",device);
	vol = vol >= 0 ? vol : 0;
	vol = vol <= 100 ? vol : 100;
	if(vol < 50){
		vol_base -= (unsigned char)(76*((float)(50 - vol) / 50.0));//最小72
	}else if(vol > 50){
		vol_base += (unsigned char)(45*((float)(vol - 50) / 50.0)); //最大为EB
	}
	else if(0 == vol){
		vol_base = 0x0;
	}
	channel_vol = (unsigned short)vol_base;
	channel_vol = channel_vol << 8;
	channel_vol += (unsigned short)vol_base;
	//printf("vol=%d, vol_base=0x%x, channel_vol=0x%x\n",vol, vol_base, channel_vol);
	do{
		ret = channel_xwrite(0xa3,channel_vol);
		timeout++;
		if(timeout > 3){
			return -1;
		}
	}while(ret != 0);
	printf("write i2c addr 0xa3 = 0x%x\n",channel_vol);
	return ret;
}
#endif
/*
void main()
{

	if ((dev=open("/dev/i2c-0",O_RDWR))<0)
		panic("Can't open i2c device '%s'\n",device);
	
	usleep(30000);
	channel_xwrite(0xa3,0xbebe);
	close(dev);
}
*/

