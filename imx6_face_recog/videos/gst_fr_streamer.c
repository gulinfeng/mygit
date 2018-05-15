/*****implement gst face recog streamer******/
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include "gst_video.h"

typedef struct _CustomData {

  SDL_Overlay* overlay;
#if ((defined S500_CLOCK) || (defined S500_FR)) 
  GstVideoInfo v_info;
  GstElement *app_sink;
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
  GstElement *vdscale;
  GstElement *pipeline_1;
  GstElement *app_sink_1;
#endif
  
  GST_VIDEO_MSG_CALLBACK call_back;
} CustomData;


static CustomData s_custom_data = {0};
#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
extern gboolean link_elements_with_filter (GstElement *element1, GstElement *element2, int width, int height);
#endif

static void eos_cb(GstAppSink *appsink, gpointer user_data)
{
  g_print ("eos1\n");
}

#if ((defined S500_CLOCK) || (defined S500_FR)) 
  /* The appsink has received a buffer */
static GstFlowReturn new_sample (GstElement *sink, gpointer user_data) {
  guint i;
  GstSample *sample;
  GstVideoFrame v_frame;
  GstBuffer *buf;
  guint8 *pixels;
  guint stride;
  guint video_h;
  guint plane_size;
  SDL_Overlay* overlay;
  //SDL_Rect rect;
  
  CustomData *data = (CustomData *)user_data;
  overlay = data->overlay;
  if(NULL == overlay){
  	  return GST_FLOW_OK;
  }
  
  sample = gst_app_sink_pull_sample (sink);
  g_assert_nonnull (sample);

  buf = gst_sample_get_buffer (sample);
  g_assert_nonnull (buf);
	
  if(gst_video_frame_map (&v_frame, &data->v_info, buf, GST_MAP_READ)){
	video_h = data->v_info.height;
	plane_size = 0;
	SDL_LockYUVOverlay(overlay);
	for(i=0; i < overlay->planes; i++)
	{
		pixels = GST_VIDEO_FRAME_PLANE_DATA (&v_frame, i);
    	stride = GST_VIDEO_FRAME_PLANE_STRIDE (&v_frame, i);
		if (0 == i){
			plane_size = stride*video_h;
		}else{
			plane_size = stride*(video_h >> 1);
		}
		memcpy(overlay->pixels[i], pixels, plane_size);
	}
	SDL_UnlockYUVOverlay(overlay);
  	gst_video_frame_unmap (&v_frame);
	if(s_custom_data.call_back) {
		s_custom_data.call_back(new_frame, (unsigned int)overlay->pixels[0],0);
    }
  }
  
  gst_buffer_unref (sample);
  return GST_FLOW_OK;
}
static GstFlowReturn new_preroll (GstElement *sink, gpointer user_data) {
  g_print("-------new_preroll------\n");
  return GST_FLOW_OK;
}
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
  static void new_buffer_1 (GstElement* sink, void* user_data)
{
	GstBuffer *buffer = NULL;  
	SDL_Overlay* overlay = NULL;
	CustomData *data = (CustomData *)user_data;
	overlay = data->overlay;
	if(NULL == overlay){
		g_print (" new_buffer_1 overlay is null ");
		return ;
	}

	/* Retrieve the buffer */  
	g_signal_emit_by_name (sink, "pull-buffer", &buffer);  
	
	if (buffer) { 
		//g_print ("new_buffer_1->buffer_size = %d  ", GST_BUFFER_SIZE(buffer));

		SDL_LockYUVOverlay(overlay);
		memcpy(overlay->pixels[0], GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
		SDL_UnlockYUVOverlay(overlay);
	
		if(s_custom_data.call_back) {
			s_custom_data.call_back(new_frame, (unsigned int)overlay->pixels[0],0);
		}

		gst_buffer_unref (buffer);
	}
	
}


static void new_preroll_1 (GstElement *sink, void *user_data) 
{
  GstBuffer *buffer = NULL;
  
  g_signal_emit_by_name (sink, "pull-preroll", &buffer); 
  g_print("-------new_preroll_1------\n");
  
  //g_print ("new_preroll_1 buffer_size=%d	", GST_BUFFER_SIZE(buffer));
  //rtpSend(user_data_s.pRtpSession, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));

  
  gst_buffer_unref (buffer);
}
#endif






GstElement* make_fr_streamer( int screen_w, int screen_h)
{
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	GstElement *result;
	GstElement *videoscale;
	GstElement *appsink;
	GstCaps * caps;
	GstPad *pad;
	GstAppSinkCallbacks callbacks;
	GError *error = NULL;     //target-bitrate=3000000 
	char descr[256];
	memset(&s_custom_data, 0, sizeof(s_custom_data));
	// identity   
	sprintf(descr,"videoscale name=vdscale method=0 ! appsink name=sink1 caps=\"video/x-raw,format=I420,width=%d,height=%d\"", \
		screen_w, screen_h);
	//descr = "videoscale name=convert ! appsink name=sink2 caps=\"video/x-raw,format=I420,width=160,height=120\"";
	printf("fr_streamer:%s\n",descr);
	result = gst_parse_launch (descr, &error);
	if (error != NULL) {
    	g_print ("could not construct pipeline: %s\n", error->message);
    	g_clear_error (&error);
    	return -2;
    }

	//s_custom_data.balance = gst_bin_get_by_name (GST_BIN (result), "balance");
  	//g_assert(s_custom_data.balance);
	
	appsink = gst_bin_get_by_name(GST_BIN (result), "sink1");
	g_assert(appsink);
	g_object_set (appsink, "async", FALSE, "sync", FALSE, NULL);

	s_custom_data.app_sink = appsink;
	
	callbacks.eos = eos_cb;
	callbacks.new_preroll = new_preroll;
	callbacks.new_sample = new_sample;
	gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &callbacks, (gpointer)&s_custom_data, NULL);
	caps = gst_app_sink_get_caps(appsink);
	g_assert (gst_video_info_from_caps (&s_custom_data.v_info, caps));
	gst_caps_unref(caps);
	
	gst_object_unref (GST_OBJECT(appsink));

	videoscale = gst_bin_get_by_name(GST_BIN (result), "vdscale");
	g_assert(videoscale);
	
	/* ghost src and sink pads */
	pad = gst_element_get_static_pad (videoscale, "sink");
	gst_element_add_pad (result, gst_ghost_pad_new ("sink", pad));
	gst_object_unref (pad);
  	gst_object_unref (GST_OBJECT(videoscale));
	return result;
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
    GstPad *pad = NULL;
	
	s_custom_data.vdscale = gst_element_factory_make ("videoscale", "vdscale");
	g_assert(s_custom_data.vdscale);
	g_object_set(s_custom_data.vdscale, "method", 0, NULL);

	s_custom_data.app_sink_1 = gst_element_factory_make ("appsink", "sink1");
	g_assert(s_custom_data.app_sink_1);
	
	s_custom_data.pipeline_1 = gst_pipeline_new ("fr-streamer-pipeline");
	g_assert(s_custom_data.pipeline_1);

	gst_bin_add_many (GST_BIN (s_custom_data.pipeline_1), s_custom_data.vdscale, s_custom_data.app_sink_1, NULL);

	if (link_elements_with_filter (s_custom_data.vdscale, s_custom_data.app_sink_1, screen_w, screen_h) != TRUE) {
		g_printerr ("init_gst_video->Elements could not be linked...\n");
		return NULL;
	}
	g_object_set (s_custom_data.app_sink_1, "async", FALSE, "sync", FALSE, "emit-signals", TRUE, NULL);
	g_signal_connect (s_custom_data.app_sink_1, "new-preroll", G_CALLBACK (new_preroll_1), (void*)&s_custom_data);
	g_signal_connect (s_custom_data.app_sink_1, "new-buffer", G_CALLBACK (new_buffer_1), (void*)&s_custom_data);


	pad = gst_element_get_static_pad (s_custom_data.vdscale, "sink");
	g_assert(pad);
	gst_element_add_pad (s_custom_data.pipeline_1, gst_ghost_pad_new ("sink", pad));
	gst_object_unref (pad);

	return s_custom_data.pipeline_1;
#endif



	
}

void set_fr_streamer(SDL_Overlay* overlay, GST_VIDEO_MSG_CALLBACK callback)
{
	s_custom_data.overlay = overlay;
	s_custom_data.call_back = callback;
}


//end
