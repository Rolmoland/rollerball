#include "lvgl.h"
#include "app_ball_ui.h"

#define BALL_DIAMETER_PX    60

static lv_obj_t *s_ball;

void ball_ui_init(void)
{
    s_ball = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_ball, BALL_DIAMETER_PX, BALL_DIAMETER_PX);
    lv_obj_set_style_radius(s_ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ball, lv_color_hex(0xFF4B4B), 0);
    lv_obj_set_style_border_width(s_ball, 0, 0);
    lv_obj_center(s_ball);
}

void ball_ui_set_pos(rt_int16_t dx, rt_int16_t dy)
{
    /* Called from the IPC-rx thread, not the LVGL thread itself -> needs lv_lock */
    lv_lock();
    lv_obj_align(s_ball, LV_ALIGN_CENTER, dx, dy);
    lv_unlock();
}
