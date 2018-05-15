/***implement send video for talking****/
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/sockios.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "gst_video.h"

#define WRITE_FILE    0

#define PORT_VIDEO_SERVER 6801

static int nMaxPackSize=50000;
static int nITEPacksize=10240;
static  guint8 sendbuffer[80*1024];

typedef struct _CustomData {
#if WRITE_FILE
  FILE *fp;
#endif
  int  closed;
  int  Flg;
  int  sockfd;
  int seqnum;
  int packsize;
  char remote_ip[32];
  struct sockaddr_in client_address;
#if ((defined S500_CLOCK) || (defined S500_FR)) 
  GstElement *app_sink;
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
  GstElement *app_sink_2;
  GstElement *identity;
  GstElement *vpuenc;
  GstElement *pipeline_2;
#endif

  //GMainLoop *main_loop;  /* GLib's Main Loop */
  GST_VIDEO_MSG_CALLBACK call_back;
} CustomData;

static CustomData s_custom_data = {0};

static int  init_sendsock(CustomData* custom)
{
	char *pRemIP = custom->remote_ip;
	custom->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&custom->client_address,'\0', sizeof(struct sockaddr_in));
	custom->client_address.sin_family = AF_INET; //IP4
	custom->client_address.sin_addr.s_addr = inet_addr(pRemIP);
	custom->client_address.sin_port = htons(PORT_VIDEO_SERVER); //6801
	custom->seqnum = 0;
	custom->closed = 0;
#if WRITE_FILE
	custom->fp = fopen("v4lsh263.h264", "wb+");
#endif
	return custom->sockfd;
}

static void close_sendsock(CustomData* custom)
{
#if WRITE_FILE
	fclose(custom->fp);
#endif
	custom->closed = 1;
	if(custom->sockfd > 0){
		close(custom->sockfd);
	}
	custom->sockfd = 0;
	return;
}

static int send_video_stream(CustomData* custom, guint8* data, gsize size)
{
#if WRITE_FILE
	static int cursor = 0;
#endif
	int to_send;
	int sended = 0;
	int remain = size;
	int pack_seq = 0;
	int packsize = custom->packsize;
	
	//if (0 == custom->seqnum) {
	//	send_video_foo_header(custom);
	//}
	if(1 == custom->closed){
		return 0;
	}
	custom->seqnum++;
	//if(custom->seqnum % 10 == 0){
	//	printf("send data size=%d\n",remain);
	//}
	sendbuffer[0]=0xFF;
	sendbuffer[1]=0x01;
	memcpy(&sendbuffer[6],&custom->Flg,4);
	if(size > packsize){
		sendbuffer[0]=0x00;
	}
	memcpy(&sendbuffer[2],&custom->seqnum, 4);
	while(remain) {
		pack_seq++;
		sendbuffer[1] = pack_seq;
		to_send = remain < packsize ? remain : packsize;
		memcpy(&sendbuffer[10], (data + sended), to_send);
		sendto(custom->sockfd, sendbuffer, to_send + 10, 0,(struct sockaddr *)&custom->client_address,sizeof(custom->client_address));
		remain -= to_send;
		sended += to_send;
		if ((remain > 0) && (remain < packsize)) {
			//·â°ü½áÊø
			sendbuffer[0]=0xFF;
		}
	}
#if WRITE_FILE
	fwrite(data, sizeof(guint8), size, custom->fp);
	//fprintf(custom->fp1, " %d", size);
	//cursor++;
	//if (cursor > 15){
	//	cursor = 0;
	//	fprintf(custom->fp1, "\n");
	//}
#endif
	return sended;
}

static void eos_cb(GstAppSink *appsink, gpointer user_data)
{
  g_print ("eos2\n");
}

#if ((defined S500_CLOCK) || (defined S500_FR)) 
  /* The appsink has received a buffer */
static GstFlowReturn new_sample (GstElement *sink, void *user_data) {
  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo map;
  GstFlowReturn ret;
  CustomData *data = (CustomData *)user_data;
  /* Retrieve the buffer */
  g_signal_emit_by_name (sink, "pull-sample", &sample, &ret);
  buffer = gst_sample_get_buffer (sample);
  /* Mapping a buffer can fail (non-readable) */
  if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
  	//fwrite(map.data, sizeof(guint8), map.size, data->fp);
	send_video_stream(data, map.data, map.size);
  	gst_buffer_unmap (buffer, &map);
  }
  gst_buffer_unref (sample);
  
  return GST_FLOW_OK;
}
static GstFlowReturn new_preroll (GstElement *sink, void *user_data) {
  g_print("-------new_prerol2------\n");
  return GST_FLOW_OK;
}
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
static void new_buffer_2 (GstElement* sink, void* user_data)
{
	GstBuffer *buffer = NULL;  
	CustomData *data = (CustomData *)user_data;
 
	g_signal_emit_by_name (sink, "pull-buffer", &buffer);  
	
	if (buffer) { 
		//g_print ("new_buffer_2->buffer_size=%d  ", GST_BUFFER_SIZE(buffer));
		send_video_stream(data, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
		gst_buffer_unref (buffer);
	}
	
}

static void new_preroll_2 (GstElement *sink, void *user_data) 
{
	GstBuffer *buffer;

	g_signal_emit_by_name (sink, "pull-preroll", &buffer); 
	g_print("-------new_preroll_2------\n");

	//g_print ("new_preroll_2 buffer_size=%d	", GST_BUFFER_SIZE(buffer));
	//rtpSend(user_data_s.pRtpSession, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));


	gst_buffer_unref (buffer);
}
#endif





GstElement* make_sending_streamer( int width, int height)
{

#if ((defined S500_CLOCK) || (defined S500_FR)) 
    int size_valid;
	GstElement *result;
	GstElement *appsink;
	GstElement *identity;
	GstPad *pad;
	GstAppSinkCallbacks callbacks;
	GError *error = NULL;     //target-bitrate=3000000 
	char descr[256];
	memset (&s_custom_data, 0, sizeof (s_custom_data));
	s_custom_data.packsize = nMaxPackSize;
	size_valid = 1;
	if (width == 640 && height == 480){
		s_custom_data.Flg = 0x18;
	}else if(width == 1280 && height == 720) {
		s_custom_data.Flg = 0x19;
	}else if(width == 320 && height == 240) {
		s_custom_data.Flg = 0x17;
	} else {
		size_valid = 0;
	}
	if(0 == size_valid){
		printf("video size:(%d,%d) is not suport!\n", width, height);
		return -2;
	}
	sprintf(descr, "identity name=vdidentity ! omxh264enc periodicty-idr=20 control-rate=variable  target-bitrate=2000000 ! appsink name=sink2 caps=\"video/x-h264\"");
	printf("sending_streamer:%s\n",descr);
	result = gst_parse_launch (descr, &error);
	if (error != NULL) {
    	g_print ("could not construct pipeline: %s\n", error->message);
    	g_clear_error (&error);
    	return -2;
    }
	
	appsink = gst_bin_get_by_name(GST_BIN (result), "sink2");
	g_assert(appsink);
	g_object_set (appsink, "async", FALSE, "sync", FALSE, NULL);
	
	callbacks.eos = eos_cb;
	callbacks.new_preroll = new_preroll;
	callbacks.new_sample = new_sample;
	gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &callbacks, (gpointer)&s_custom_data, NULL);
	
	gst_object_unref (GST_OBJECT(appsink));

	identity = gst_bin_get_by_name(GST_BIN (result), "vdidentity");
	g_assert(identity);
	
	/* ghost src and sink pads */
	pad = gst_element_get_static_pad (identity, "sink");
	gst_element_add_pad (result, gst_ghost_pad_new ("sink", pad));
	gst_object_unref (pad);
  	gst_object_unref (GST_OBJECT(identity));
	return result;
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	GstCaps * video_caps = NULL;
	GstPad *pad = NULL;
	int size_valid=0;

	s_custom_data.packsize = nMaxPackSize;
	size_valid = 1;
	if (width == 640 && height == 480){
		s_custom_data.Flg = 0x18;
	}else if(width == 1280 && height == 720) {
		s_custom_data.Flg = 0x19;
	}else if(width == 320 && height == 240) {
		s_custom_data.Flg = 0x17;
	} else {
		size_valid = 0;
	}
	if(0 == size_valid){
		printf("video size:(%d,%d) is not suport!\n", width, height);
		return -2;
	}

	s_custom_data.identity = gst_element_factory_make ("identity", "vdidentity");
	g_assert(s_custom_data.identity);
	s_custom_data.vpuenc = gst_element_factory_make ("vpuenc", "vpuenc_element");
	g_assert(s_custom_data.vpuenc);
	g_object_set(s_custom_data.vpuenc, "codec", 6, "seqheader-method", 0, "quant", 30, NULL);
	
	s_custom_data.app_sink_2 = gst_element_factory_make ("appsink", "sink2");
	g_assert(s_custom_data.app_sink_2);
	g_object_set (s_custom_data.app_sink_2, "async", FALSE, "sync", FALSE, NULL);
	video_caps = gst_caps_from_string("video/x-h264");
	g_assert(video_caps);
	g_object_set (s_custom_data.app_sink_2, "caps", video_caps, NULL);

	s_custom_data.pipeline_2 = gst_pipeline_new ("send-streamer-pipeline");
	g_assert(s_custom_data.pipeline_2);
	
	gst_bin_add_many (GST_BIN (s_custom_data.pipeline_2), s_custom_data.identity,s_custom_data.vpuenc, s_custom_data.app_sink_2, NULL);

	if (gst_element_link_many (s_custom_data.identity, s_custom_data.vpuenc, s_custom_data.app_sink_2, NULL) != TRUE ) {
		g_printerr ("init_gst_video->appsink_queue_element and app_sink could not be linked..\n");
		return -1;
	}
	
	g_object_set (s_custom_data.app_sink_2, "async", FALSE, "sync", FALSE, "emit-signals", TRUE, NULL);
	g_signal_connect (s_custom_data.app_sink_2, "new-preroll", G_CALLBACK (new_preroll_2), (void*)&s_custom_data);
	g_signal_connect (s_custom_data.app_sink_2, "new-buffer", G_CALLBACK (new_buffer_2), (void*)&s_custom_data);
	
	/* ghost src and sink pads */
	pad = gst_element_get_static_pad (s_custom_data.identity, "sink");
	g_assert(pad);
	gst_element_add_pad (s_custom_data.pipeline_2, gst_ghost_pad_new ("sink", pad));
	gst_object_unref (pad);

	return s_custom_data.pipeline_2;
#endif
}

int perpare_sending(char* remote_ip, int type)
{
	int ret;
	strcpy(s_custom_data.remote_ip, remote_ip);
	s_custom_data.packsize = nMaxPackSize;
	if(1 == type){
		s_custom_data.packsize = nITEPacksize;
		printf("GST_VD:--playing_gst_video-----ITE--used--\n");
	}
	ret = init_sendsock(&s_custom_data);
	if (0 == ret) {
		g_print("GST_VD: init_sendsock failed \n");
		return -1;
	}
	return ret;
}

void end_sending()
{
	close_sendsock(&s_custom_data);
}

//end
