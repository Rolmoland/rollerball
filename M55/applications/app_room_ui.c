#include "lvgl.h"
#include "app_room_ui.h"

/* Keep in sync with M33's app_room.c (independent firmware images, no shared header) */
#define ROOM_HALF_W_PX  200
#define ROOM_HALF_H_PX  350

void room_ui_init(void)
{
    lv_obj_t *rect = lv_obj_create(lv_scr_act());

    lv_obj_remove_flag(rect, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(rect, ROOM_HALF_W_PX * 2, ROOM_HALF_H_PX * 2);
    lv_obj_center(rect);

    lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(rect, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(rect, 3, 0);
    lv_obj_set_style_radius(rect, 0, 0);
}
