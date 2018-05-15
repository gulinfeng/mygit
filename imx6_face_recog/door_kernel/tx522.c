/*****implement tx522 with uart****/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "tx522.h"


#define TX_RECV_TIMEOUT 100000

#define TX_MAX_DATA_LEN 128
#define ONCE_READ_SIZE  80
#define TX_RECV_BUFF_SIZE 160

#define TX_FRAME_BEGIN  0x20
#define TX_FRAME_END    0x03

#define TX_FRAM_HEAD_LEN  4
#define TX_FRAM_TIAL_LEN  2

#define FRAME_RECV_CONTINUE   2
#define FRAME_RECV_COMPLETE   1
#define FRAME_RECV_TIMEOUT    0
#define FRAME_RECV_ERROR     -1
#define FRAME_RECV_DISCARD   -2


typedef struct _tg_tx522_datablock
{
	unsigned char seqnr;
	unsigned char cmd;
	unsigned char status;
	unsigned char len;
	unsigned char data[TX_MAX_DATA_LEN];
}TX_DATABLOCK_S;

static int is_frame_begin(unsigned char* buff, int buff_size)
{
	unsigned char* pPos = buff;
	int to_cmp = buff_size;
	while (to_cmp > 0)
	{
		if(TX_FRAME_BEGIN == (*pPos))
		{
			return (buff_size - to_cmp);
		}
		to_cmp -= 1;
		pPos += 1;
	}
	return -1;
}

static int write_completed(int fd, char* send_buf, int len)
{
	int cur_len;
	int sended = 0;
	while(sended != len){
		cur_len = write(fd, send_buf + sended, len - sended);//send(fd, (send_buf + sended),len - sended, 0);
		sended += cur_len;
	}
	return sended;
}

static int send_frame_complete(int uart, TX_DATABLOCK_S* datablock)
{
	int i, datalen;
	char bcc = 0;
	unsigned char sendbuf[ONCE_READ_SIZE];
	memset(sendbuf, 0, ONCE_READ_SIZE);
	sendbuf[0] = TX_FRAME_BEGIN;
	sendbuf[1] = datablock->seqnr;
	sendbuf[2] = datablock->cmd;
	sendbuf[3] = datablock->len;
	datalen = datablock->len;
	memcpy(&sendbuf[4], datablock->data, datalen);
	sendbuf[datalen + 5] = TX_FRAME_END;
	for(i=1;i<(datalen + 5);i++)	{
		if(i < (datalen + 4))
			bcc^=sendbuf[i];
		if(i == (datalen + 4))
			bcc=~bcc;
	}
	sendbuf[datalen + 4] = bcc;
	i = write_completed(uart, sendbuf, datalen + 6);
	return i;
}

static int read_frame_complete(int uart, TX_DATABLOCK_S* datablock)
{
	int ret, i;
	int pos,data_len;
	int fd_begin;
	int cur_rcv_len;
	int recvd = 0;
	char* pRecv = NULL;
	fd_set fs_read;
	unsigned char recvbuf[TX_RECV_BUFF_SIZE];
	struct timeval timeout;
	timeout.tv_sec=0;
 	timeout.tv_usec= TX_RECV_TIMEOUT;
	fd_begin = 0;
	cur_rcv_len = 0;
	recvd = 0;
	data_len = 0;
	pos = 0;
	memset(recvbuf, 0, TX_RECV_BUFF_SIZE);
	//printf("RECV: cmd=0x%x\n",datablock->cmd);
	//一直接收数据直到收到完整的数据帧，或者超时，或者接收出错
	//一旦超时或出错，上层会重发命令直到接收完整数据帧
	do {
		FD_ZERO(&fs_read);
		FD_SET(uart,&fs_read);
	 	ret = select((uart+1),&fs_read,NULL,NULL, &timeout);
		if (ret < 0)
		{
			printf("uart: select error\n");
			return -1;
		}
		else if(0 == ret)
		{
			//超时重发
			printf("uart: select timeout\n");
			return FRAME_RECV_TIMEOUT;
			
		}
		if(FD_ISSET(uart, &fs_read))
		{	
				ret = FRAME_RECV_DISCARD;
				pRecv = recvbuf + recvd;
				cur_rcv_len = read(uart, pRecv, ONCE_READ_SIZE);
				//printf("INFO: read ret=%d\n",ret);
				if(cur_rcv_len > 0){
					//for(i=0;i<cur_rcv_len;i++) {
					//	printf(" 0x%02x ", pRecv[i]);
					//}
					if(pos + recvd + cur_rcv_len > TX_RECV_BUFF_SIZE){
						printf("WARNINF: recv data is too long\n");
						break;
					}
					if(0 == fd_begin){
						pos = is_frame_begin(pRecv, cur_rcv_len);
						if(pos < 0 ){
							//未找到帧头 
							printf("WARNING: not found begin 0x20 cmd=0x%x recvd=%d\n",datablock->cmd, cur_rcv_len);
							break;
						}
						if(cur_rcv_len - pos < 4){
							printf("WARNING: frame recv error cmd=0x%x recvd=%d\n",datablock->cmd, cur_rcv_len);
							break;
						}
						if(datablock->seqnr != pRecv[pos + 1]){
							//命令未同步
							printf("WARNING: seqnr error cmd=0x%x recvd=%d\n",datablock->cmd, cur_rcv_len);
							break;
						}
						data_len = (int)pRecv[pos + 3];
					}
					recvd += cur_rcv_len;
					if(data_len + 6 > recvd){
						printf("INFO:recv continue datalen=%d,recved=%d\n",data_len +6,recvd);
						fd_begin = 1;
						ret = FRAME_RECV_CONTINUE;
						continue;
					}
					if(TX_FRAME_END == recvbuf[pos + data_len + 5]){
						ret = FRAME_RECV_COMPLETE;
						break;
					}
					//帧结束符不正确
					printf("WARNING: not found end 0x03 cmd=0x%x ret=%d\n",datablock->cmd, recvd);
					break;
				}
			}
		}while(1);
	//printf("\n");
	if(FRAME_RECV_COMPLETE == ret){
		data_len = recvbuf[pos + 3];
		datablock->len = recvbuf[pos + 3];
		datablock->status = recvbuf[pos + 2];
		memcpy(datablock->data, &recvbuf[pos + 4], data_len);
		return (data_len + 6);
	}
	return ret;
}

unsigned char TX_GetInfo(int uart, unsigned char* Info)
{
	int ret = 0;
	
	unsigned char infoLen = 0;
	TX_DATABLOCK_S recv_buff;
	TX_DATABLOCK_S send_buff;
	memset(&send_buff, 0, sizeof(TX_DATABLOCK_S));
	memset(&recv_buff, 0, sizeof(TX_DATABLOCK_S));
	memset(&send_buff, 0, sizeof(TX_DATABLOCK_S));
	send_buff.seqnr = 0;
	send_buff.cmd = 0x2b;
	send_buff.len = 0x0;
	recv_buff.seqnr =send_buff.seqnr;
	recv_buff.cmd = send_buff.cmd;
	do {
		send_frame_complete(uart,&send_buff);
		ret = read_frame_complete(uart,&recv_buff);
	}while(FRAME_RECV_DISCARD == ret);
	if(ret > 0){
		if(0 != recv_buff.status){
			printf("TX_ERROR: TX_GetInfo( 0x%x )\n", recv_buff.status);
		}else{
			infoLen = recv_buff.data[3];
		    if(infoLen < 16 ){	
		 		memcpy(Info,&recv_buff.data[4],infoLen);
		    }
		}
		return recv_buff.status;
	}
	
	return COMM_ERR;
}
unsigned char TX_LoadKey(int uart)
{
	int ret = 0;
	unsigned char Key[6] = {0x6B,0x75,0x4D,0x30,0x73,0xBB};
	TX_DATABLOCK_S recv_buff;
	TX_DATABLOCK_S send_buff;
	memset(&send_buff, 0, sizeof(TX_DATABLOCK_S));
	memset(&recv_buff, 0, sizeof(TX_DATABLOCK_S));
	memset(&send_buff, 0, sizeof(TX_DATABLOCK_S));
	send_buff.seqnr = 0;
	send_buff.cmd = 0x20;
	send_buff.len = 0x06;
	memcpy(send_buff.data,Key, 6);
	recv_buff.seqnr =send_buff.seqnr;
	recv_buff.cmd = send_buff.cmd;
	do {
		send_frame_complete(uart,&send_buff);
		ret = read_frame_complete(uart,&recv_buff);
	}while(FRAME_RECV_DISCARD == ret);
	if(ret > 0){
		if(0 != recv_buff.status){
			printf("TX_ERROR: TX_LoadKey( 0x%x )\n", recv_buff.status);
		}
		return recv_buff.status;
	}
	
	return COMM_ERR;
}

unsigned char TX_GetCardSnr(int uart, unsigned char ReqCode, unsigned char *TagType, unsigned char *Sak, unsigned char *SnrLen, unsigned char *Snr)
{

	int ret = 0;
	TX_DATABLOCK_S recv_buff;
	TX_DATABLOCK_S send_buff;
	memset(&send_buff, 0, sizeof(TX_DATABLOCK_S));
	memset(&recv_buff, 0, sizeof(TX_DATABLOCK_S));
	send_buff.seqnr = 0;
	send_buff.cmd = 0x21;
	send_buff.len = 0x01;
	send_buff.data[0] = ReqCode;
	recv_buff.seqnr =send_buff.seqnr;
	recv_buff.cmd = send_buff.cmd;
	//ret = read_frame_complete(uart,&buff);
	do{
		send_frame_complete(uart,&send_buff);
		ret = read_frame_complete(uart,&recv_buff);
	}while(FRAME_RECV_DISCARD == ret);
	//printf("get car snr ret =%d, status=%d**snrlen=%d*******\n",ret,(int)buff.status, (int)buff.data[3]);
	if((ret > 0) && (0x00 == recv_buff.status)){
		 *TagType = recv_buff.data[0];
		 *(TagType+1) = recv_buff.data[1];
		 *Sak = recv_buff.data[2];
		 *SnrLen = recv_buff.data[3];
		 if(*SnrLen > 7){	
		 	return COMM_ERR;
		 }
		 memcpy(Snr,&recv_buff.data[4],*SnrLen);
		 return recv_buff.status;
	}
	if((ret > 0)&&(0x00 != recv_buff.status)&&(0x01 != recv_buff.status)){
		printf("TX_ERROR: TX_GetCardSnr( 0x%x )\n", recv_buff.status);
	}
	return COMM_ERR;	
}


unsigned char TX_ReadBlock(int uart, unsigned char Block, unsigned char *Data)
{
	int ret = 0;
	TX_DATABLOCK_S recv_buff;
	TX_DATABLOCK_S send_buff;
	memset(&send_buff, 0, sizeof(TX_DATABLOCK_S));
	memset(&recv_buff, 0, sizeof(TX_DATABLOCK_S));
	send_buff.seqnr = 0x01;
	send_buff.cmd = 0x22;
	send_buff.len = 0x01;
	send_buff.data[0] = Block;
	recv_buff.seqnr =send_buff.seqnr;
	recv_buff.cmd = send_buff.cmd;
	do{
		send_frame_complete(uart,&send_buff);
		ret = read_frame_complete(uart,&recv_buff);
	}while(FRAME_RECV_DISCARD == ret);
	if((ret > 0) && (0x00 == recv_buff.status)){
		memcpy(Data,recv_buff.data,16);
		return recv_buff.status;
	}
	if((ret > 0) &&(0 != recv_buff.status)){
		printf("TX_ERROR: TX_ReadBlock( 0x%x )\n", recv_buff.status);
	}
	return COMM_ERR;	
}

unsigned char TX_Halt(int uart)
{
	int ret = 0;
	TX_DATABLOCK_S recv_buff;
	TX_DATABLOCK_S send_buff;
	memset(&send_buff, 0, sizeof(TX_DATABLOCK_S));
	memset(&recv_buff, 0, sizeof(TX_DATABLOCK_S));
	send_buff.seqnr = 0x02;
	send_buff.cmd = 0x27;
	send_buff.len = 0x00;
	recv_buff.seqnr =send_buff.seqnr;
	recv_buff.cmd = send_buff.cmd;
	
	//ret = read_frame_complete(uart,&buff);
	do{
		send_frame_complete(uart,&send_buff);
		ret = read_frame_complete(uart,&recv_buff);
	}while(FRAME_RECV_DISCARD == ret);
	if(ret > 0){
		if(0 != recv_buff.status){
			printf("TX_ERROR: TX_Halt( 0x%x )\n", recv_buff.status);
		}
		return recv_buff.status;
	}
	return COMM_ERR;
}




