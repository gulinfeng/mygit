/*implement gstreamer video*/
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include "gst_video.h"

typedef struct _CustomData {
	GstElement *videosrc;
	GstElement *pipeline;
	GstElement *select;
	GST_VIDEO_MSG_CALLBACK call_back;

#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
	GstElement *videocap;
	GstElement *pipeline_1;
	GstElement *pipeline_2;
#endif

	
} CustomData;

static CustomData  s_custom_data = {0};

extern GstElement* make_sending_streamer( int width, int height);
extern int perpare_sending(char* remote_ip, int type);
extern void end_sending();
	
extern GstElement* make_fr_streamer( int screen_w, int screen_h);
extern void set_fr_streamer(SDL_Overlay* overlay, GST_VIDEO_MSG_CALLBACK callback);


static gboolean  
on_app_message (GstBus * bus, GstMessage * msg, void *user_data)  
{  
    GstElement *source = NULL;  
    GError *err = NULL;
    gchar *debug_info = NULL;
	CustomData *data = (CustomData *)user_data;
    switch (GST_MESSAGE_TYPE (msg)) {   
        case GST_MESSAGE_ERROR:  
			gst_message_parse_error (msg, &err, &debug_info);
			g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
			g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
			g_clear_error (&err);
			g_free (debug_info);
            if(s_custom_data.call_back) {
				s_custom_data.call_back(video_error,0,0);
            }
            break;  
        default:  
            break;  
    }  
    return TRUE;  
} 


void play_video()
{
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
}

void paused_video()
{
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_PAUSED);
}

void stop_video()
{
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_NULL);
}
#if ((defined IMX6_CLOCK) || (defined IMX6_FR))
gboolean link_elements_with_filter (GstElement *element1, GstElement *element2, int width, int height)
{
	gboolean link_ok = FALSE;
	GstCaps *caps = NULL;
	//"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),

	caps = gst_caps_new_simple ("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'U', '1', '2'),
		"width", G_TYPE_INT, width,
		"height", G_TYPE_INT, height,
		"framerate", GST_TYPE_FRACTION, 30, 1,
		NULL);

	link_ok = gst_element_link_filtered (element1, element2, caps);
	gst_caps_unref (caps);

	if (!link_ok) {
		g_warning ("Failed to link element1 and element2!");
	}
	return link_ok;
}
#endif

int init_gst_video(int video_w, int video_h, int screen_w, int screen_h)
{
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	GstBus *bus;
	GstCaps *caps;
	GstPad *srcpad, *sinkpad;
	GstElement *v4l2;
	GstElement *stream1;
	GstElement *stream2;
	GstElement *videocap;
	memset (&s_custom_data, 0, sizeof (s_custom_data));
	gst_init(NULL,NULL);
	//gst_debug_set_default_threshold(3);        //---3
	s_custom_data.pipeline = gst_pipeline_new (NULL);
    g_assert (s_custom_data.pipeline); 
	
	v4l2 = gst_element_factory_make ("v4l2src", NULL);  //v4l2src  videotestsrc
  	g_assert(v4l2);

	s_custom_data.videosrc = v4l2;
	videocap = gst_element_factory_make ("capsfilter", NULL);
 	g_assert (videocap);
	caps = gst_caps_from_string ("video/x-raw,format=I420,width=640,height=480,framerate=30/1");
	g_object_set (videocap, "caps", caps, NULL);
  	gst_caps_unref (caps);
	s_custom_data.select = gst_element_factory_make ("output-selector", "selector");
	//s_custom_data.select = gst_element_factory_make ("tee", "t");
	g_assert(s_custom_data.select);
	
	/*
	sink = gst_element_factory_make ("fakesink", NULL);
	g_object_set (sink, "sync", TRUE, NULL);
	g_object_set (sink, "silent", TRUE, NULL);
	g_assert (sink);
	*/


	/* add elements to result bin */
	gst_bin_add (GST_BIN (s_custom_data.pipeline), v4l2);
	gst_bin_add (GST_BIN (s_custom_data.pipeline), videocap);
	gst_bin_add (GST_BIN (s_custom_data.pipeline), s_custom_data.select);
	gst_element_link_pads(v4l2,"src", videocap,"sink");
	gst_element_link_pads(videocap,"src", s_custom_data.select,"sink");
	stream1 = make_fr_streamer(screen_w, screen_h);
	g_assert(stream1);
	gst_bin_add (GST_BIN (s_custom_data.pipeline), stream1);
	srcpad = gst_element_get_request_pad (s_custom_data.select, "src_%u");
    sinkpad = gst_element_get_static_pad (stream1, "sink");
    gst_pad_link (srcpad, sinkpad);
    gst_object_unref (srcpad);
    gst_object_unref (sinkpad);
	//gst_object_unref (stream1);
	stream2 = make_sending_streamer(video_w,video_h);
	g_assert(stream2);
	gst_bin_add (GST_BIN (s_custom_data.pipeline), stream2);
	srcpad = gst_element_get_request_pad (s_custom_data.select, "src_%u");
    sinkpad = gst_element_get_static_pad (stream2, "sink");
    gst_pad_link (srcpad, sinkpad);
    gst_object_unref (srcpad);
    gst_object_unref (sinkpad);
	//g_object_set(s_custom_data.select, "pad-negotiation-mode", 2, NULL);
	
	/* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  	bus = gst_element_get_bus (s_custom_data.pipeline);
  	gst_bus_add_watch (bus, (GstBusFunc) on_app_message, (void*)&s_custom_data);  
  	gst_object_unref (GST_OBJECT(bus));
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_NULL);
	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
	return 0;
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	GstPad *srcpad = NULL, *sinkpad = NULL;
	GstBus *bus = NULL;
	
	printf("init_gst_video enter....video_w %d, video_h %d, screen_w %d, screen_h %d\n", video_w,video_h,screen_w,screen_h);
	
	memset (&s_custom_data, 0, sizeof (s_custom_data));
	
	gst_init(NULL,NULL);
	
	s_custom_data.pipeline = gst_pipeline_new (NULL);
    g_assert (s_custom_data.pipeline);
	
	s_custom_data.videosrc = gst_element_factory_make ("mfw_v4lsrc", "videosrc");  //v4l2src  videotestsrc
  	g_assert(s_custom_data.videosrc);
	g_object_set (s_custom_data.videosrc, "device", "/dev/video2", NULL);
	
	s_custom_data.select = gst_element_factory_make ("output-selector", "select");
	g_assert(s_custom_data.select);
	
	gst_bin_add_many (GST_BIN (s_custom_data.pipeline), s_custom_data.videosrc, s_custom_data.select, NULL);
	
	if (link_elements_with_filter (s_custom_data.videosrc, s_custom_data.select, video_w, video_h) != TRUE) {
        g_printerr ("init_gst_video->link_elements_with_filter could not be linked...\n");
        return -1;
    }
	
//////////////////////////////////////////////////////////////////////////////////////////////////////


	s_custom_data.pipeline_1 = make_fr_streamer(screen_w, screen_h);
	g_assert(s_custom_data.pipeline_1);

	gst_bin_add (GST_BIN (s_custom_data.pipeline), s_custom_data.pipeline_1);

	srcpad = gst_element_get_request_pad (s_custom_data.select, "src%d");
	g_assert(srcpad);
	sinkpad = gst_element_get_static_pad (s_custom_data.pipeline_1, "sink");
	g_assert(sinkpad);
	gst_pad_link (srcpad, sinkpad);
	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);

///////////////////////////////////////////////////////////////////////////////////////////////////

	s_custom_data.pipeline_2 = make_sending_streamer(video_w, video_h);
	g_assert(s_custom_data.pipeline_2);

	gst_bin_add (GST_BIN (s_custom_data.pipeline), s_custom_data.pipeline_2);

	srcpad = gst_element_get_request_pad (s_custom_data.select, "src%d");
	g_assert(srcpad);
	sinkpad = gst_element_get_static_pad (s_custom_data.pipeline_2, "sink");
	g_assert(sinkpad);
	gst_pad_link (srcpad, sinkpad);
	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);

///////////////////////////////////////////////////////////////////////////////////////////////////

	bus = gst_element_get_bus (s_custom_data.pipeline);
	gst_bus_add_watch (bus, (GstBusFunc) on_app_message, (void*)&s_custom_data);  
	gst_object_unref (GST_OBJECT(bus));
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_NULL);



	return 0;
#endif

	

}

int uninit_gst_video()
{
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_NULL);
	s_custom_data.call_back = NULL;
	gst_object_unref (GST_OBJECT(s_custom_data.pipeline));
	return 0;
}

void set_video(SDL_Overlay* overlay, GST_VIDEO_MSG_CALLBACK callback)
{
	s_custom_data.call_back = callback;
	set_fr_streamer(overlay, callback);
}

void switch_send_streamer(char* remote_ip, int type)
{
	GstPad *select_src_1 = NULL;
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_PAUSED);
	select_src_1 = gst_element_get_static_pad (s_custom_data.select, "src_1");
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_PAUSED);
	select_src_1 = gst_element_get_static_pad (s_custom_data.select, "src1");
	g_assert(select_src_1);
#endif

	//g_object_set(s_custom_data.select, "pad-negotiation-mode", 2, NULL);
	g_object_set(s_custom_data.select, "active-pad", select_src_1, NULL);
	gst_object_unref (select_src_1);
	perpare_sending(remote_ip, type);
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
#endif
	
	printf("---switch_send_streamer --------\n");
}

void end_sending_video()
{
	end_sending();
}

void switch_fr_streamer()
{

	GstPad *select_src_0 = NULL;
	end_sending();
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_PAUSED);
	select_src_0 = gst_element_get_static_pad (s_custom_data.select, "src_0");
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_PAUSED);
	select_src_0 = gst_element_get_static_pad (s_custom_data.select, "src0");
	g_assert(select_src_0);
#endif
	
	//g_object_set(s_custom_data.select, "pad-negotiation-mode", 2, NULL);
	g_object_set(s_custom_data.select, "active-pad", select_src_0, NULL);
	gst_object_unref (select_src_0);
#if ((defined S500_CLOCK) || (defined S500_FR)) 
	//gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
#elif ((defined IMX6_CLOCK) || (defined IMX6_FR))
	gst_element_set_state (s_custom_data.pipeline, GST_STATE_PLAYING);
#endif
	printf("---switch_fr_streamer --------\n");

	
}


//end
