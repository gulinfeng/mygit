/*********************************************************************************
  Copyright (C), 2017-2020, xigu Tech. Co., Ltd.
  File name   : gui_utile.h
  Author      : zlg
  Version     : v1.0
  Date        : 2017/11/14
  Description : base gui tools
  Others:
  Function List:

  History:
    1. 

*********************************************************************************/
#ifndef _GUI_UTILE_H
#define _GUI_UTILE_H

#include "SDL.h"
#include "SDL_ttf.h"

#define SCREEN_W  320
#define SCREEN_H  240


#ifdef __cplusplus
extern "C" {
#endif


int init_gui_utile();

void uninit_gui_utile();

int is_in_except_rect(int x, int y, int w, int h);

void draw_licence_error(int code);

void draw_recognization(SDL_Surface *screen, int model, float process);

void draw_record_finished(SDL_Surface *screen, int ret_id, int id);

void draw_record_face(SDL_Surface *screen,  int counter, float process);

void draw_face_info(SDL_Surface *screen, int x1,int y1, int width, int height, float confidence, unsigned long id, char* name);

Uint32 make_rgba(Uint8 r, Uint8 g, Uint8 b, Uint8 a);

TTF_Font* get_font(int index);


#ifdef __cplusplus
}
#endif

#endif//



