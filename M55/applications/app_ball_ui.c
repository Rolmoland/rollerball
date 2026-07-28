#include "lvgl.h"
#include "app_ball_ui.h"

#define BALL_DIAMETER_PX    28

static lv_obj_t *s_ball;
static lv_obj_t *s_maze_table;
static rt_uint16_t s_cell_size;

void ball_ui_init(lv_obj_t *maze_table, rt_uint16_t cell_size)
{
    s_maze_table = maze_table;
    s_cell_size = cell_size;
    s_ball = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_ball, BALL_DIAMETER_PX, BALL_DIAMETER_PX);
    lv_obj_set_style_radius(s_ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ball, lv_color_hex(0xFF4B4B), 0);
    lv_obj_set_style_border_width(s_ball, 0, 0);
    lv_obj_remove_flag(s_ball, LV_OBJ_FLAG_SCROLLABLE);
}

void ball_ui_set_cell_locked(rt_uint8_t x, rt_uint8_t y)
{
    lv_area_t content;
    rt_int32_t inset = (s_cell_size - BALL_DIAMETER_PX) / 2;

    lv_obj_get_content_coords(s_maze_table, &content);
    lv_obj_set_pos(s_ball,
                   content.x1 + x * s_cell_size + inset,
                   content.y1 + y * s_cell_size + inset);
}

void ball_ui_set_cell(rt_uint8_t x, rt_uint8_t y)
{
    lv_lock();
    ball_ui_set_cell_locked(x, y);
    lv_unlock();
}
