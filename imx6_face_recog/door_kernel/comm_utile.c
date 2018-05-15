/****implement comm utile ****/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>  
#include <sys/ioctl.h>

//波特率名称
static int name_arr[] = {921600, 576000, 500000, 460800, 230400, 115200, 57600, 38400, 19200, 9600,4800, 2400, 1800, 1200, 300 };
//波特率值
static int speed_arr[] = {B921600, B576000, B500000, B460800, B230400, B115200, B57600, B38400, B19200, B9600,	B4800, B2400, B1800, B1200, B600, B300 };

int set_serial(int fd, int databits, int parity, int stopbits, int flowcontrol,int vtime,int vmin)
{
	struct termios options;
	if (tcgetattr(fd, &options) != 0)//get property 
	{
		printf("SetupSerial 1\n");
		return -1;
	}
	
	/* set up the data size */
	options.c_cflag &= ~CSIZE;
	switch (databits)
	{
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr,"Unsupported data size\n");
		return -1;
	}
	
	/* set up parity */
	switch (parity)
	{
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;	/* Clear parity enable */
		options.c_iflag &= ~INPCK;	/* Enable parity checking */
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);	/* Enable odd parity*/ 
		options.c_iflag |= INPCK;		/* Disnable parity checking */
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;	/* Enable parity */
		options.c_cflag &= ~PARODD;	/* Turn odd off =>Even parity*/  
		options.c_iflag |= INPCK;	/* Disnable parity checking */
		break;
	case 'S':
	case 's':  // as no parity 
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;
		
	default:
		fprintf(stderr,"Unsupported parity\n");
		return (-1);
	}
	
	/* set up the stop bit(s) */   
	switch (stopbits)
	{
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr,"Unsupported stop bits\n");
		return (-1);
	}
	
	/* Set input parity option */
	if (parity != 'n')
	{
		options.c_iflag |= INPCK;
	}
	
	switch (flowcontrol)
	{
	case 'n':
	case 'N':
		options.c_cflag &= ~CRTSCTS;	// Disable hardware flow control
		break;
	case 'y':
	case 'Y':	
		options.c_cflag |= CRTSCTS;	// Enable hardware flow control
		break;
	default:
		fprintf(stderr, "Unsupported format to assign hardware flow control\n");
		return (-1);
	}
	
	
	options.c_cflag |= (CLOCAL | CREAD);		// Enable receiver and set local mode

    options.c_iflag &= ~(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK|ISTRIP|INLCR|IGNCR|ICRNL|IUCLC|IXON|IXOFF|IXANY); 
    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ECHO|ECHONL|ISIG|IEXTEN|ICANON); 	


	options.c_cc[VTIME] = vtime; 	// VTIME则是一个读操作等待定时值,read函数读取时间超过0.1*vtime则返回
	options.c_cc[VMIN] = vmin;        //VMIN表示为了满足读操作，需要读取的最少字符数，缓存内数据大于vmin，read函数返回
	
	tcflush(fd,TCIFLUSH);		
	/* Update the options and do it NOW */
	if (tcsetattr(fd, TCSANOW, &options) != 0)
	{
		printf("SetupSerial 3\n");
		return -1;
	}
	return 0;
}

void set_speed(int fd, int speed)
{
	int	i;
	int	status;
	struct termios Opt;
	tcgetattr(fd, &Opt);   //save current serial port setting 
	for (i = 0; i < sizeof(speed_arr) / sizeof(int);  i++)
	{
		if  (speed == name_arr[i])
		{
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&Opt, speed_arr[i]); //input bps
			cfsetospeed(&Opt, speed_arr[i]);// output bps 
			status = tcsetattr(fd, TCSANOW, &Opt);//set new property 
			if  (status != 0)
			{
				printf("tcsetattr fd\n");
				return;
			}
			tcflush(fd,TCIOFLUSH);
		}
	}
}

int comm_Init(char* dev,int nSpeed)
{
	int txfd;
	txfd = open(dev,O_RDWR);//打开串口  ttyS1
	if (-1 == txfd)
	{
		perror("Can't Open Serial Port");
		return txfd;	
	}
	set_speed(txfd,nSpeed); 
	if (set_serial(txfd,8,'s',1,'n',10,4) == -1)//15sec
	{
		printf("Databits, Stopbit(s), Parity Setting Error\n");
		return -1;
	}
	return txfd;
}



