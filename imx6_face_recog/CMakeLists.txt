CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

#cmake -DDOOR_OPITION=IMX6_CLOCK ../
#cmake -DDOOR_OPITION=IMX6_FR ../
#cmake -DDOOR_OPITION=S500_CLOCK ../
#cmake -DDOOR_OPITION=S500_FR ../

SET(CMAKE_SYSTEM_NAME Linux) 
if((${DOOR_OPITION} STREQUAL "IMX6_FR") OR (${DOOR_OPITION} STREQUAL 
"IMX6_CLOCK"))
	SET(TOOLCHAIN_DIR "/opt/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf")
elseif((${DOOR_OPITION} STREQUAL "S500_CLOCK") OR (${DOOR_OPITION} STREQUAL 
"S500_FR"))
	SET(TOOLCHAIN_DIR "/home/s500/toolchain/arm-linux-gnueabihf-4.9")
endif()
SET(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR})
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-g++)

PROJECT(FACE_RECOG)

#aux_source_directory(. DIR_SRCS)
#MESSAGE(STATUS ${DIR_SRCS})

if(${DOOR_OPITION} STREQUAL "IMX6_FR")
	add_definitions("-DIMX6_FR")
	set(DIR_PREFIX "/home/imx6/local_face_recog")
	option (USE_FR_LIB "Use face recog lib" ON)
	
elseif(${DOOR_OPITION} STREQUAL "IMX6_CLOCK")
	add_definitions("-DIMX6_CLOCK")
	set(DIR_PREFIX "/home/imx6/local_face_recog")
	option (USE_FR_LIB "Use face recog lib" OFF)
	
elseif(${DOOR_OPITION} STREQUAL "S500_FR")
	add_definitions("-DS500_FR")
	set(DIR_PREFIX "/home/s500/local")
	option (USE_FR_LIB "Use face recog lib" ON)
	
elseif(${DOOR_OPITION} STREQUAL "S500_CLOCK")
	add_definitions("-DS500_CLOCK")
	set(DIR_PREFIX "/home/s500/local")
	option (USE_FR_LIB "Use face recog lib" OFF)
	
endif()


set(APP_SRCS 
		door_kernel/blockqueue.c 
		door_kernel/taskpool.c 
		door_kernel/dictionary.c 
		door_kernel/iniparser.c 
		door_kernel/comm_utile.c 
		door_kernel/i2c_ctl.c 
		door_kernel/libAudio.c 
		door_kernel/platform_ioctl.c 
		door_kernel/tx522.c 
		door_kernel/door_signal.c 
		door_kernel/door_network.c 
		door_kernel/door_media.c 
		door_kernel/door_info.c 
		door_kernel/door_log.c 
		door_kernel/door_card.c 
		door_kernel/g711.c 
		door_kernel/door_elevator.c
		sdl_page.c 
		sdl_door.c 
		gui_utile.c 
		main.c 
		backup/fr_backup.c 
		backup/ftplib.c
		)



if(USE_FR_LIB)
	list(APPEND APP_SRCS 
			   videos/gst_video.c
			   face_recog_page.c 
			   fr_extra_page.c
			   videos/gst_fr_streamer.c 
			   videos/gst_send_streamer.c
			   )
else()
	list(APPEND APP_SRCS 
			  door_kernel/gst_video.c
			  clock_page.c
)
endif()

if(USE_FR_LIB)
	list(APPEND APP_SRCS 
			  recog/face_recog.c )
endif()

MESSAGE(STATUS ${APP_SRCS})

include_directories("${DIR_PREFIX}/include")
include_directories("${DIR_PREFIX}/include/SDL")
include_directories("${DIR_PREFIX}/include/glib-2.0")
include_directories("${DIR_PREFIX}/lib/glib-2.0/include")
include_directories("${PROJECT_SOURCE_DIR}/door_kernel")
include_directories("${PROJECT_SOURCE_DIR}/videos")

if(USE_FR_LIB)
	include_directories("${PROJECT_SOURCE_DIR}/recog")	
endif()

link_directories("${DIR_PREFIX}/lib")

if(USE_FR_LIB)
	link_directories("${PROJECT_SOURCE_DIR}/libs")
endif()

set(APP_LIBS pthread 
		dl 
		rt 
		z 
		m 
		png 
		jpeg
		asound
		SDL 
		SDL_gfx 
		SDL_ttf
		SDL_image 
		freetype
		gobject-2.0
		gmodule-2.0 
    glib-2.0)

if((${DOOR_OPITION} STREQUAL "IMX6_FR") OR (${DOOR_OPITION} STREQUAL 
"IMX6_CLOCK"))
	include_directories("${DIR_PREFIX}/include/gstreamer-0.10")
	include_directories("${DIR_PREFIX}/include/gstreamer-0.10/gst")
	include_directories("${DIR_PREFIX}/include/libxml2")
	list(APPEND APP_LIBS gstapp-0.10
		gstvideo-0.10
		gstbase-0.10
		gstreamer-0.10
		iconv)

elseif((${DOOR_OPITION} STREQUAL "S500_CLOCK") OR (${DOOR_OPITION} STREQUAL 
"S500_FR"))
	include_directories("${DIR_PREFIX}/include/gstreamer-1.0") 
	list(APPEND APP_LIBS gstapp-1.0 
		gstvideo-1.0
		gstbase-1.0
		gstreamer-1.0
		ffi)

endif()

if(USE_FR_LIB)
	list(APPEND APP_LIBS wfFR)
endif()

#ADD_DEPENDENCIES("-Wall")

add_definitions("-Wall")
#add_definitions("-Wpointer-to-int-cast")

#SET(CMAKE_C_FLAGS -Wall)
#SET(CMAKE_CXX_FLAGS -Wall)

if(${DOOR_OPITION} STREQUAL "IMX6_FR")
	add_executable(imx6_face_recog ${APP_SRCS})
	target_link_libraries(imx6_face_recog ${APP_LIBS})
	
elseif(${DOOR_OPITION} STREQUAL "IMX6_CLOCK")
	add_executable(imx6_clock ${APP_SRCS})
	target_link_libraries(imx6_clock ${APP_LIBS})
	
elseif(${DOOR_OPITION} STREQUAL "S500_FR")
	add_executable(s500_face_recog ${APP_SRCS})
	target_link_libraries(s500_face_recog ${APP_LIBS})
	
elseif(${DOOR_OPITION} STREQUAL "S500_CLOCK")
	add_executable(s500_clock ${APP_SRCS})
	target_link_libraries(s500_clock ${APP_LIBS})
	
endif()






