/***implement gstreamer video playing***/
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
#include "gst_video.h"

#define WRITE_FILE    0

//0:将video编码发送
//1:将video编码发送，另一条线程将自己编码的数据再解码再显示
//2:将video编码发送，另一条线程从获取到video直接显示，不再自我编解码
//3:将video编码存储至本地，另一条线程从获取到video直接显示，不再自我编解码
#define GST_TEE_DISPLAY  0

#define PORT_VIDEO_SERVER 6801

static int nMaxPackSize=50000;
static int nITEPacksize=10240;
static  guint8 sendbuffer[80*1024];

typedef struct _CustomData {
#if WRITE_FILE
  FILE *fp;
#endif
  int  Flg;
  int  sockfd;
  int seqnum;
  int packsize;
  char remote_ip[32];
  struct sockaddr_in client_address;
  GstElement *pipeline;
  //GstElement *v4l2_src;
  //GstElement *video_enc; 
  GstElement *app_sink;
  //GMainLoop *main_loop;  /* GLib's Main Loop */
  GST_VIDEO_MSG_CALLBACK call_back;
  
#if ((defined IMX6_CLOCK) || (defined IMX6_FR))  
  GstElementFactory *source_factory;
  GstElementFactory *enc_factory;
  GstElementFactory *sink_factory;
  GstElement        *source_element ;
  GstElement        *enc_element;
  

#if GST_TEE_DISPLAY
  GstElementFactory *tee_factory;
  GstElementFactory *appsink_queue;
  GstElementFactory *display_queue;
  GstElementFactory *dec_factory;
  GstElementFactory *mfw_isink_factory;

  GstElement        *tee_element;
  GstElement        *appsink_queue_element;
  GstElement        *display_queue_element;
  GstElement        *dec_element;
  GstElement		*mfw_isink_element;
  
  GstPad           *tee_appsink_pad;
  GstPad           *tee_display_pad;
  GstPad           *queue_appsink_pad;
  GstPad           *queue_display_pad;

  GstPadTemplate   *tee_src_pad_template;

  #endif
#endif
} CustomData;

static CustomData s_custom_data;

static int  init_sendsock(CustomData* custom)
{
	char *pRemIP = custom->remote_ip;
	custom->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&custom->client_address,'\0', sizeof(struct sockaddr_in));
	custom->client_address.sin_family = AF_INET; //IP4
	custom->client_address.sin_addr.s_addr = inet_addr(pRemIP);
	custom->client_address.sin_port = htons(PORT_VIDEO_SERVER); //6801
	custom->seqnum = 0;
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
	close(custom->sockfd);
	custom->sockfd = 0;
	return;
}

static int send_video_foo_header(CustomData* custom)
{
	static unsigned char SPS_PPS_320x240[34]={0x00,0x00,0x00,0x01,0x67,0x42,0xc0,0x1f,0xb9,0x88,
		0x0a,0x0f,0xD0,0x80,0x00,0x00,0x03,0x00,0x80,
		0x00,0x00,0x19,0x47,0x8c,0x18, 0x89, 0x00,0x00,0x00,0x01,
		0x68,0xce,0x31,0x52};

	/*320x240  flg=0x17*/ 
	//00000000h: 00 00 00 01 67 42 C0 1F B9 88 0A 0F D0 80 00 00 ; ....gB?箞..衻..
    //00000010h: 03 00 80 00 00 19 47 8C 18 89 00 00 00 01 68 CE ; ..�...G??...h?
    //00000020h: 31 52                                           ; 1R

	/*640x480*/
	//00000000h: 00 00 00 01 67 42 C0 1F B9 88 05 01 ED 08 00 00 ; ....gB?箞..?..
    //00000010h: 03 00 08 00 00 03 01 94 78 C1 88 90 00 00 00 01 ; .......攛翀?...
    //00000020h: 68 CE 31 52                                     ; h?R

	/*640x480 v4l2src  flg=0x18 */
	//00004662h: 00 00 00 01 67 42 C0 1F B9 88 05 01 ED 08 00 00 ; ....gB?箞..?..
    //00004672h: 03 00 08 00 00 03 01 94 78 C1 88 90 00 00 00 01 ; .......攛翀?...
    //00004682h: 68 CE 31 52                                     ; h?R

	static unsigned char SPS_PPS_640x480[36]={0x00,0x00,0x00,0x01,0x67,0x42,0xc0,0x1f,0xb9,0x88,
		0x05,0x01,0xed,0x08,0x00,0x00,0x03,0x00,0x08,
		0x00,0x00,0x03,0x01,0x94,0x78, 0xc1, 0x88,0x90,0x00,0x00,0x00,0x01,
		0x68,0xce,0x31,0x52};
    /*720p v4l2src*/
	//0000602dh: 00 00 00 01 67 42 C0 1F B9 88 02 80 2D D0 80 00 ; ....gB?箞.�-衻.
    //0000603dh: 00 03 00 80 00 00 19 47 8C 18 89 00 00 00 01 68 ; ...�...G??...h
    //0000604dh: CE 31 52                                        ; ?R
	
	int nRet=0;
	unsigned char* sps_pps=SPS_PPS_640x480;
	int flag = custom->Flg;
	int sps_pps_len = 0;
	sendbuffer[0]=0xFF;
	sendbuffer[1]=0x01;
	sendbuffer[2] = 0;
	sendbuffer[3] = 0;
	sendbuffer[4] = 0;
	sendbuffer[5] = 0;
	if (0x17 == flag){
		sps_pps_len = 34;
		sps_pps = SPS_PPS_320x240;
	}
	if (0x18 == flag) {
		sps_pps_len = 36;
		sps_pps = SPS_PPS_640x480;
	}
	memcpy(&sendbuffer[6],&custom->Flg,4);
	memcpy(&sendbuffer[10],sps_pps,sps_pps_len);
	//memcpy(&sendbuffer[44],SPS_PPS_640x480,34);
	sendto(custom->sockfd, sendbuffer, sps_pps_len + 10, 0,(struct sockaddr *)&custom->client_address,sizeof(custom->client_address));
	return 0;
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
	//printf("packsize =%d\n", packsize);//50 000 大小
	//if (0 == custom->seqnum) {
		//send_video_foo_header(custom);
		
	//}
	custom->seqnum++;
	//if(custom->seqnum <= 20){
	//	printf("DEBUG: send data size=%d\n",remain);
	//}
	//printf("custom->remote_ip = %s\n", custom->remote_ip);
	sendbuffer[0]=0xFF;
	sendbuffer[1]=0x01;
	memcpy(&sendbuffer[6], &custom->Flg, 4);
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
			//封包结束
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

#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
static void new_buffer (GstElement* sink, void * user_data)
{
	GstBuffer *buffer = NULL;  
    CustomData *data = (CustomData *)user_data;

	/* Retrieve the buffer */  
	g_signal_emit_by_name (sink, "pull-buffer", &buffer);  
	
	if (buffer) { 
		//g_print ("buffer_size[%d]=%d  ", buffer_cnt, GST_BUFFER_SIZE(buffer));
		send_video_stream(data, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
		gst_buffer_unref (buffer);
	}  
	
}
static void new_preroll (GstElement *sink, void *user_data) {
  //GstSample *sample;
  GstBuffer *buffer;
  //GstMapInfo map;
  //GstFlowReturn ret;
  //CustomData *data = (CustomData *)user_data;
  /* Retrieve the buffer */
 // g_signal_emit_by_name (sink, "pull-preroll", &sample, &ret);
  
  g_signal_emit_by_name (sink, "pull-preroll", &buffer); 
  g_print("-------new_preroll------\n");
  gst_buffer_unref (buffer);

  //return GST_FLOW_OK;
}
#elif ((defined S500_CLOCK) || (defined S500_FR))  
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
  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo map;
  GstFlowReturn ret;
  CustomData *data = (CustomData *)user_data;
  /* Retrieve the buffer */
  g_signal_emit_by_name (sink, "pull-preroll", &sample, &ret);
  g_print("-------new_preroll------\n");
  gst_buffer_unref (sample);

  return GST_FLOW_OK;
}
#endif





static gboolean  
on_app_message (GstBus * bus, GstMessage * msg, void *user_data)  
{ 

    GstElement *source;  
    GError *err;
    gchar *debug_info;
	CustomData *data = (CustomData *)user_data;
    switch (GST_MESSAGE_TYPE (msg)) {  
        case GST_MESSAGE_EOS:  
            g_print ("The source got dry\n");  
            source = gst_bin_get_by_name (GST_BIN (data->pipeline), "video_src");  
            //gst_app_src_end_of_stream (GST_APP_SRC (source));  
            g_signal_emit_by_name (source, "end-of-stream", NULL);  
            gst_object_unref (source);  
            break;  
        case GST_MESSAGE_ERROR:  
			gst_message_parse_error (msg, &err, &debug_info);
			g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
			g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
			g_clear_error (&err);
			g_free (debug_info);
            //g_main_loop_quit (data->loop); 
            if(s_custom_data.call_back) {
				s_custom_data.call_back(1,0,0);
            }
            break;  
        default:  
            break;  
    }  
    return TRUE; 

} 
#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
static gboolean link_elements_with_filter (GstElement *element1, GstElement *element2, int width, int height)
{
	gboolean link_ok = FALSE;
	GstCaps *caps = NULL;

	caps = gst_caps_new_simple ("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
		"width", G_TYPE_INT, width,
		"height", G_TYPE_INT, height,
		"framerate", GST_TYPE_FRACTION, 25, 1,
		NULL);

	link_ok = gst_element_link_filtered (element1, element2, caps);
	gst_caps_unref (caps);

	if (!link_ok) {
		g_warning ("Failed to link element1 and element2!");
	}
	return link_ok;
}
#endif

int init_gst_video(GST_VIDEO_MSG_CALLBACK callback, int capture_flg, int width, int height)
{

#if ((defined S500_CLOCK) || (defined S500_FR))  
	int size_valid;
  	GstBus *bus;
	GError *error = NULL;     //target-bitrate=3000000 
	char* descr = NULL;
	char descr_sz[256];
	char* src = "v4l2src";
	//char* descr="v4l2src name= video_src! video/x-raw,format=I420,width=640,height=480,framerate=25/1 ! omxh264enc periodicty-idr=20 control-rate=1 ! appsink name=sink caps=\"video/x-h264\"";
    //descr="videotestsrc ! video/x-raw,format=I420,width=640,height=480,framerate=25/1 ! omxh264enc ! filesink location=v4l-1.h264";
	
	if(0 == capture_flg){
		src = "videotestsrc";
	}
	memset (&s_custom_data, 0, sizeof (s_custom_data));
	s_custom_data.call_back = callback;
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
	sprintf(descr_sz, "%s ! video/x-raw,format=I420,width=%d,height=%d,framerate=25/1 ! omxh264enc periodicty-idr=20 control-rate=1 ! appsink name=sink caps=\"video/x-h264\"",
		src, width, height);
	
	descr = descr_sz;

	g_print ("cmd: gst-launch-1.0 %s\n",descr); 

	gst_init(NULL,NULL);
	
    
	
	s_custom_data.pipeline = gst_parse_launch (descr, &error);
	if (error != NULL) {
    	g_print ("could not construct pipeline: %s\n", error->message);
    	g_clear_error (&error);
    	return -2;
    }
	
    /* get sink */
    s_custom_data.app_sink = gst_bin_get_by_name (GST_BIN (s_custom_data.pipeline), "sink");

	g_object_set(s_custom_data.app_sink, "emit-signals", TRUE, NULL);

    g_signal_connect (s_custom_data.app_sink, "new-preroll", G_CALLBACK (new_preroll), (void*)&s_custom_data);

    g_signal_connect (s_custom_data.app_sink, "new-sample", G_CALLBACK (new_sample), (void*)&s_custom_data);

	gst_object_unref (GST_OBJECT(s_custom_data.app_sink));
	
	/* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  	bus = gst_element_get_bus (s_custom_data.pipeline);
  	gst_bus_add_watch (bus, (GstBusFunc) on_app_message, (void*)&s_custom_data);  
  	gst_object_unref (GST_OBJECT(bus));

	gst_element_set_state (s_custom_data.pipeline, GST_STATE_NULL);
	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
	return 0;

#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	int size_valid = 0;
  	GstBus *bus = NULL;
	//char* descr = NULL;
	//char descr_sz[256] = {0};
	char* src = "mfw_v4lsrc";
	//char* descr="v4l2src name= video_src! video/x-raw,format=I420,width=640,height=480,framerate=25/1 ! omxh264enc periodicty-idr=20 control-rate=1 ! appsink name=sink caps=\"video/x-h264\"";
    //descr="videotestsrc ! video/x-raw,format=I420,width=640,height=480,framerate=25/1 ! omxh264enc ! filesink location=v4l-1.h264";
	
	if(0 == capture_flg){
		src = "videotestsrc";
	}
	memset (&s_custom_data, 0, sizeof (s_custom_data));
	s_custom_data.call_back = callback;
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
	

	gst_init(NULL, NULL);

	//创建factory
    s_custom_data.source_factory = gst_element_factory_find (src);
	s_custom_data.enc_factory = gst_element_factory_find ("vpuenc");
	
//#if (GST_TEE_DISPLAY == 3)
	//s_custom_data.sink_factory = gst_element_factory_find ("filesink");
//#else
	s_custom_data.sink_factory = gst_element_factory_find ("appsink");
//#endif

	
	if (!s_custom_data.source_factory || !s_custom_data.enc_factory || !s_custom_data.sink_factory) {  
      g_printerr ("init_gst_video->Not all element factories could be created.\n");  
      return -3;
    }

#if (GST_TEE_DISPLAY == 1)
	s_custom_data.tee_factory = gst_element_factory_find ("tee");
	s_custom_data.dec_factory = gst_element_factory_find ("vpudec");
	s_custom_data.appsink_queue = gst_element_factory_find ("queue");
	s_custom_data.display_queue = gst_element_factory_find ("queue");
	s_custom_data.mfw_isink_factory = gst_element_factory_find ("mfw_isink");
	if (!s_custom_data.tee_factory || !s_custom_data.appsink_queue || !s_custom_data.display_queue || !s_custom_data.dec_factory || !s_custom_data.mfw_isink_factory){
		g_printerr ("init_gst_video->Not all element factories could be created...\n");  
		return -3;
	}
#elif (GST_TEE_DISPLAY == 2)
	s_custom_data.tee_factory = gst_element_factory_find ("tee");
	s_custom_data.appsink_queue = gst_element_factory_find ("queue");
	s_custom_data.display_queue = gst_element_factory_find ("queue");
	s_custom_data.mfw_isink_factory = gst_element_factory_find ("mfw_isink");
	if (!s_custom_data.tee_factory || !s_custom_data.appsink_queue || !s_custom_data.display_queue  || !s_custom_data.mfw_isink_factory){
		g_printerr ("init_gst_video->Not all element factories could be created...\n");  
		return -3;
	}
#endif	
	printf("init_gst_video->gst_element_factory_find ok \n");
	//创建element
	s_custom_data.source_element = gst_element_factory_create (s_custom_data.source_factory, "source"); 
	s_custom_data.enc_element = gst_element_factory_create (s_custom_data.enc_factory, "enc");
    s_custom_data.app_sink = gst_element_factory_create (s_custom_data.sink_factory, "sink");
	if (!s_custom_data.source_element || !s_custom_data.enc_element || !s_custom_data.app_sink ) {
        g_printerr ("init_gst_video->Not all elements could be created.\n");
        return -2;
    }
	
#if (GST_TEE_DISPLAY == 1)
	s_custom_data.tee_element = gst_element_factory_create (s_custom_data.tee_factory, "tee_element");
	s_custom_data.dec_element = gst_element_factory_create (s_custom_data.dec_factory, "dec_element");
	s_custom_data.appsink_queue_element = gst_element_factory_create (s_custom_data.appsink_queue, "appsink_queue_element");
	s_custom_data.display_queue_element = gst_element_factory_create (s_custom_data.display_queue, "display_queue_element");
	s_custom_data.mfw_isink_element = gst_element_factory_create (s_custom_data.mfw_isink_factory, "mfw_isink_element");
	if (!s_custom_data.dec_element || !s_custom_data.mfw_isink_element || !s_custom_data.appsink_queue_element || !s_custom_data.display_queue_element || !s_custom_data.tee_element) {
		g_printerr ("init_gst_video->Not all elements could be created...\n");
		return -2;
	}
#elif (GST_TEE_DISPLAY == 2)
	s_custom_data.tee_element = gst_element_factory_create (s_custom_data.tee_factory, "tee_element");
	s_custom_data.appsink_queue_element = gst_element_factory_create (s_custom_data.appsink_queue, "appsink_queue_element");
	s_custom_data.display_queue_element = gst_element_factory_create (s_custom_data.display_queue, "display_queue_element");
	s_custom_data.mfw_isink_element = gst_element_factory_create (s_custom_data.mfw_isink_factory, "mfw_isink_element");
	if (!s_custom_data.mfw_isink_element || !s_custom_data.appsink_queue_element || !s_custom_data.display_queue_element || !s_custom_data.tee_element) {
		g_printerr ("init_gst_video->Not all elements could be created...\n");
		return -2;
	}
#endif
	printf("init_gst_video->gst_element_factory_create ok \n");
	s_custom_data.pipeline = gst_pipeline_new ("door-pipeline");
	if (!s_custom_data.pipeline){
		g_printerr ("init_gst_video->Not all elements could be created.\n");
		return -2;
	}

#if (GST_TEE_DISPLAY == 1)
	gst_bin_add_many (GST_BIN (s_custom_data.pipeline), s_custom_data.source_element, s_custom_data.enc_element, s_custom_data.tee_element, \
						s_custom_data.appsink_queue_element, s_custom_data.app_sink, \
						s_custom_data.display_queue_element, s_custom_data.dec_element, s_custom_data.mfw_isink_element, NULL);
#elif (GST_TEE_DISPLAY == 2)
	gst_bin_add_many (GST_BIN (s_custom_data.pipeline), s_custom_data.source_element,s_custom_data.tee_element,  \
						s_custom_data.appsink_queue_element, s_custom_data.enc_element, s_custom_data.app_sink, \
						s_custom_data.display_queue_element, s_custom_data.mfw_isink_element, NULL);
#else
	gst_bin_add_many (GST_BIN (s_custom_data.pipeline), s_custom_data.source_element, s_custom_data.enc_element, s_custom_data.app_sink, NULL);
#endif
	printf("init_gst_video->gst_bin_add_many ok \n");


#if (GST_TEE_DISPLAY == 2)
	//加上过滤器 
	if (link_elements_with_filter (s_custom_data.source_element, s_custom_data.tee_element, width, height) != TRUE) {
        g_printerr ("init_gst_video->Elements could not be linked...\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
    }
#else
	//加上过滤器 
	if (link_elements_with_filter (s_custom_data.source_element, s_custom_data.enc_element, width, height) != TRUE) {
        g_printerr ("init_gst_video->Elements could not be linked.\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
    }
#endif
	printf("init_gst_video->link_elements_with_filter ok \n");

	//(6): avc	   - video/x-h264 
	//g_object_set(s_custom_data.enc_element, "codec", 6, "gopsize", 20, "force-framerate", TRUE,  NULL);
	//g_object_set(s_custom_data.enc_element, "codec", 6, "gopsize", 20, "bitrate", 1,  NULL);
	g_object_set(s_custom_data.enc_element, "codec", 6, "seqheader-method", 0, "quant", 30, NULL);
	//printf("init_gst_video-> seqheader-method, 0, quant, 30 \n");
	//g_object_set(s_custom_data.enc_element, "codec", 6,"quant", 10, "framerate-nu", 25, "framerate-de", 1, "force-framerate", TRUE, NULL);
    if(1 == capture_flg){
		//目前只有在720p时才是1280*720的采集，其他值都是640*480
		if (width == 640 && height == 480){
			g_object_set(s_custom_data.source_element, "rotate", 3, "capture-mode", 0, NULL);
		}else if(width == 1280 && height == 720) {
			g_object_set(s_custom_data.source_element, "rotate", 3, "capture-mode", 4, NULL);
		}else if(width == 320 && height == 240) {
			g_object_set(s_custom_data.source_element, "rotate", 3, "capture-mode", 0, NULL);
		} else {
			g_object_set(s_custom_data.source_element, "rotate", 3, "capture-mode", 0, NULL);
		}
		printf("init_gst_video->camera set size:(%d, %d)!\n", width, height);
	}

	printf("init_gst_video->g_object_set (\"codec\", 6,\"seqheader-method\", 3) ok\n");

#if (GST_TEE_DISPLAY == 1)
	if(0 == capture_flg){//如果是测试视频，播放一个小球运动的video
		g_object_set (s_custom_data.source_element, "pattern", 18, NULL);
	}
	g_object_set(s_custom_data.dec_element, "output-format", 1, NULL);//(1): i420    - I420
	if (gst_element_link_many(s_custom_data.enc_element, s_custom_data.tee_element, NULL) != TRUE) {
		g_printerr ("init_gst_video->enc_element and tee_element could not be linked.\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
	}
	if (gst_element_link_many (s_custom_data.appsink_queue_element, s_custom_data.app_sink, NULL) != TRUE ) {
		g_printerr ("init_gst_video->appsink_queue_element and app_sink could not be linked.\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
	}
	if (gst_element_link_many (s_custom_data.display_queue_element, s_custom_data.dec_element, s_custom_data.mfw_isink_element, NULL) != TRUE ) {
		g_printerr ("init_gst_video->display_queue_element and dec_element and mfw_isink_element  could not be linked.\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
	}
	
	s_custom_data.tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (s_custom_data.tee_element), "src%d");
	s_custom_data.tee_appsink_pad = gst_element_request_pad (s_custom_data.tee_element, s_custom_data.tee_src_pad_template, NULL, NULL);
	g_print ("init_gst_video->Obtained request pad %s for appsink branch.\n", gst_pad_get_name (s_custom_data.tee_appsink_pad));
	s_custom_data.queue_appsink_pad = gst_element_get_static_pad (s_custom_data.appsink_queue_element, "sink");

	s_custom_data.tee_display_pad = gst_element_request_pad (s_custom_data.tee_element, s_custom_data.tee_src_pad_template, NULL, NULL);
	g_print ("init_gst_video->Obtained request pad %s for dec and display branch.\n", gst_pad_get_name (s_custom_data.tee_display_pad));
	s_custom_data.queue_display_pad = gst_element_get_static_pad (s_custom_data.display_queue_element, "sink");
	if (gst_pad_link (s_custom_data.tee_appsink_pad, s_custom_data.queue_appsink_pad) != GST_PAD_LINK_OK ||
	  	gst_pad_link (s_custom_data.tee_display_pad, s_custom_data.queue_display_pad) != GST_PAD_LINK_OK) {
		g_printerr ("init_gst_video->Tee could not be linked.\n");
		gst_object_unref (s_custom_data.pipeline);
		return -1;
	}
	gst_object_unref (s_custom_data.queue_appsink_pad);
	gst_object_unref (s_custom_data.queue_display_pad);
	
#elif (GST_TEE_DISPLAY == 2)
	if(0 == capture_flg){//如果是测试视频，播放一个小球运动的video
	//	g_object_set (s_custom_data.source_element, "pattern", 18, NULL);
	}

	if (gst_element_link_many (s_custom_data.appsink_queue_element, s_custom_data.enc_element, s_custom_data.app_sink, NULL) != TRUE ) {
		g_printerr ("init_gst_video->appsink_queue_element and app_sink could not be linked..\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
	}
	if (gst_element_link_many (s_custom_data.display_queue_element, s_custom_data.mfw_isink_element, NULL) != TRUE ) {
		g_printerr ("init_gst_video->appsink_queue_element and app_sink could not be linked....\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
	}
	s_custom_data.tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (s_custom_data.tee_element), "src%d");
	s_custom_data.tee_appsink_pad = gst_element_request_pad (s_custom_data.tee_element, s_custom_data.tee_src_pad_template, NULL, NULL);
	g_print ("init_gst_video->Obtained request pad %s for appsink branch.\n", gst_pad_get_name (s_custom_data.tee_appsink_pad));
	s_custom_data.queue_appsink_pad = gst_element_get_static_pad (s_custom_data.appsink_queue_element, "sink");

	s_custom_data.tee_display_pad = gst_element_request_pad (s_custom_data.tee_element, s_custom_data.tee_src_pad_template, NULL, NULL);
	g_print ("init_gst_video->Obtained request pad %s for dec and display branch.\n", gst_pad_get_name (s_custom_data.tee_display_pad));
	s_custom_data.queue_display_pad = gst_element_get_static_pad (s_custom_data.display_queue_element, "sink");
	if (gst_pad_link (s_custom_data.tee_appsink_pad, s_custom_data.queue_appsink_pad) != GST_PAD_LINK_OK ||
	  	gst_pad_link (s_custom_data.tee_display_pad, s_custom_data.queue_display_pad) != GST_PAD_LINK_OK) {
		g_printerr ("init_gst_video->Tee could not be linked.\n");
		gst_object_unref (s_custom_data.pipeline);
		return -1;
	}
	gst_object_unref (s_custom_data.queue_appsink_pad);
	gst_object_unref (s_custom_data.queue_display_pad);

#else
	if (gst_element_link (s_custom_data.enc_element, s_custom_data.app_sink) != TRUE) {
        g_printerr ("init_gst_video->Elements could not be linked.\n");
        gst_object_unref (s_custom_data.pipeline);
        return -1;
    }
#endif
	
	printf("init_gst_video->gst_element_link ok \n");


#if (GST_TEE_DISPLAY == 3)
	//本地存储为 v4l-1.h264 文件
	g_object_set(s_custom_data.app_sink, "location", "v4l-3.h264", NULL);
	g_object_set (s_custom_data.source_element, "num-buffers", 1000, NULL);
#else
	g_object_set(s_custom_data.app_sink, "name", "sink", "caps", gst_caps_from_string("video/x-h264"), "emit-signals", TRUE, NULL);
	g_signal_connect (s_custom_data.app_sink, "new-preroll", G_CALLBACK (new_preroll), (void*)&s_custom_data);
	g_signal_connect (s_custom_data.app_sink, "new-buffer", G_CALLBACK (new_buffer), (void*)&s_custom_data);
#endif

	printf("init_gst_video->g_object_set and g_signal_connect app_sink ok \n");
	/* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  	bus = gst_element_get_bus (s_custom_data.pipeline);
  	gst_bus_add_watch (bus, (GstBusFunc) on_app_message, (void*)&s_custom_data);  
  	gst_object_unref (GST_OBJECT(bus));

	gst_element_set_state (s_custom_data.pipeline, GST_STATE_NULL);

	
	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_READY);

	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_PAUSED);
	//
	printf("init_gst_video->gst_element_set_state GST_STATE_NULL ok \n");
	return 0;
#endif	 
}

int playing_gst_video(char* remote_ip, int type)
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
	//send_video_foo_header(&s_custom_data);
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
    //g_print("GST_VD: return playing_gst_video \n");
	return ret;

}

int stop_gst_video()
{
	int ret=0;
	
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_NULL);
	close_sendsock(&s_custom_data);
	return ret;
}

int uninit_gst_video()
{
	int ret=0;
	stop_gst_video();
	s_custom_data.call_back = NULL;

#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
#if (GST_TEE_DISPLAY == 1)
	gst_object_unref (GST_OBJECT(s_custom_data.dec_element));
	gst_object_unref (s_custom_data.dec_factory);
#endif

#if (GST_TEE_DISPLAY == 1) || (GST_TEE_DISPLAY == 2)
	gst_element_release_request_pad (s_custom_data.tee_element, s_custom_data.tee_appsink_pad);
	gst_element_release_request_pad (s_custom_data.tee_element, s_custom_data.tee_display_pad);
	gst_object_unref (s_custom_data.tee_appsink_pad);
	gst_object_unref (s_custom_data.tee_display_pad);

	gst_object_unref (GST_OBJECT(s_custom_data.tee_element));
	gst_object_unref (GST_OBJECT(s_custom_data.appsink_queue_element));
	gst_object_unref (GST_OBJECT(s_custom_data.display_queue_element));
	gst_object_unref (GST_OBJECT(s_custom_data.mfw_isink_element));

	gst_object_unref (s_custom_data.tee_factory);
	gst_object_unref (s_custom_data.appsink_queue);
	gst_object_unref (s_custom_data.display_queue);

	gst_object_unref (s_custom_data.mfw_isink_factory);
#endif

	gst_object_unref (GST_OBJECT(s_custom_data.source_element));
	gst_object_unref (GST_OBJECT(s_custom_data.enc_element));
	gst_object_unref (GST_OBJECT(s_custom_data.app_sink));
	gst_object_unref (s_custom_data.source_factory);  
  	gst_object_unref (s_custom_data.sink_factory);  
	gst_object_unref (s_custom_data.enc_factory);
#endif
	gst_object_unref (GST_OBJECT(s_custom_data.pipeline));
	return ret;

}


