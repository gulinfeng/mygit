/**implement chontrol soud  door  led io**/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "config_pin.h"
#include "platform_ioctl.h"
//#define IO_DISABLE 
//#define  WATCHDOG_DISABLE

#ifndef WATCHDOG_DISABLE
#include <linux/watchdog.h>
static int g_watchdog_fd = -1;
#endif

/*485发送前拉高*/
void EnableSend()
{
	int fd=0, ret=0;
	unsigned long arg = GP_PORT_B15;
	unsigned int cmd = GP_OUTPUT_HI;
	
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd, cmd, arg);
	//printf("------EnableSend---fd=%d ret=%d \n", fd, ret);
	close(fd);
}

/*485接收前拉低*/
void EnableRecv()
{
	int fd=0, ret=0;
	unsigned long arg = GP_PORT_B15;
	unsigned int cmd = GP_OUTPUT_LO;
	
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd, cmd, arg);
	//printf("------EnableRecv---fd=%d ret=%d \n", fd, ret);
	close(fd);

}

void openPowerAmplifier()
{
	//GP_PORT_C3
#ifndef IO_DISABLE
	int ret,fd;
	unsigned long arg = GP_PORT_C3;
	unsigned int cmd = GP_OUTPUT_HI;

	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd,cmd,arg);

	close(fd);
#endif
	return;
}

void closePowerAmplifier()
{
#ifndef IO_DISABLE
	int ret,fd;
	unsigned long arg = GP_PORT_C3;
	unsigned int cmd = GP_OUTPUT_LO;

	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd,cmd,arg);

	close(fd);
#endif
	return;
}
int is_weak_light()
{
	int ret = 0;
#ifndef IO_DISABLE
	int fd;
	unsigned long arg = GP_PORT_C4;
	unsigned int cmd = GP_OUTPUT_HI;
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd, GP_INPUT, arg);
	close(fd);
	ret = (ret == GP_OUTPUT_LO) ? 1 : 0;
#endif
	return ret;
}
void enable_led()
{
#ifndef IO_DISABLE
	int ret,fd;
	unsigned long arg = GP_PORT_C4;
	unsigned int cmd = GP_OUTPUT_HI;
	fd = open("/dev/gioctl", O_RDWR);
	//ret = ioctl(fd, GP_INPUT, arg);
	ret = GP_OUTPUT_LO;
	if (ret == GP_OUTPUT_LO){
		arg = GP_PORT_B13;
		cmd = GP_OUTPUT_HI;
		ret = ioctl(fd,cmd,arg);
	}else {
		arg = GP_PORT_B13;
		cmd = GP_OUTPUT_LO;
		ret = ioctl(fd,cmd,arg);
	}
	close(fd);
#endif
	return;
}

void disable_led()
{
#ifndef IO_DISABLE
	int ret,fd;
	unsigned long arg = GP_PORT_B13;
	unsigned int cmd = GP_OUTPUT_LO;
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd,cmd,arg);
	close(fd);
#endif
	return;
}

int close_door()
{
#ifndef IO_DISABLE
	int ret,fd;
	unsigned long arg = GP_PORT_B14;
	unsigned int cmd = GP_OUTPUT_LO;
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd,cmd,arg);
	close(fd);
#endif
	return 0;
}

int open_door()
{
#ifndef IO_DISABLE
	int ret,fd;
	unsigned long arg = GP_PORT_B14;
	unsigned int cmd = GP_OUTPUT_HI;
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd,cmd,arg);
	close(fd);
#endif
	return 0;
}


int reset_lcd()
{
	int ret,fd;
#ifndef IO_DISABLE
	unsigned long arg = GP_PORT_C5;
	unsigned int cmd = GP_OUTPUT_LO;
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd,cmd,arg);
	usleep(120000);
	cmd = GP_OUTPUT_HI;
	ret = ioctl(fd,cmd,arg);
	close(fd);
#endif
    return 0;
}

int openTx522Power()
{
	int ret,fd;
#ifndef IO_DISABLE
	unsigned long arg = GP_PORT_B19;
	unsigned int cmd = GP_OUTPUT_HI;
	fd = open("/dev/gioctl", O_RDWR);
	ret = ioctl(fd,cmd,arg);
	close(fd);
#endif
	return 0;
}

//看门狗
void StartWatchdog()
{
#ifndef WATCHDOG_DISABLE
	if(g_watchdog_fd > 0){
		return;
	}
	g_watchdog_fd = open("/dev/watchdog",0);
	if(g_watchdog_fd<0)
   	{
		perror("open dc_timer_wdt error\n");	
		return;
	}
	ioctl(g_watchdog_fd,WDIOC_KEEPALIVE);
#endif
}

void KeepAlive()
{
	//printf("-----KeepAlive-------------------------------\n");
#ifndef WATCHDOG_DISABLE	
	if(g_watchdog_fd >= 0){
		
		ioctl(g_watchdog_fd,WDIOC_KEEPALIVE);
		return;
	}
#endif
}

void StopWatchdog()
{
#ifndef WATCHDOG_DISABLE
	if(g_watchdog_fd > 0){
		close(g_watchdog_fd);
	}
	g_watchdog_fd = -1;
#endif	
}

//end
