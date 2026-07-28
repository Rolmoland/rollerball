#ifndef __APP_BALL_UI_H__
#define __APP_BALL_UI_H__

#include <rtthread.h>
#include "lvgl.h"

void ball_ui_init(lv_obj_t *maze_table, rt_uint16_t cell_size);
void ball_ui_set_cell(rt_uint8_t x, rt_uint8_t y);
void ball_ui_set_cell_locked(rt_uint8_t x, rt_uint8_t y);

#endif /* __APP_BALL_UI_H__ */
