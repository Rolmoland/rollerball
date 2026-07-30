#ifndef __APP_BALL_UI_H__
#define __APP_BALL_UI_H__

#include <rtthread.h>
#include "lvgl.h"

/**
 * @brief 在迷宫表格上创建小球对象并记录单元格尺寸。
 * @param[in] maze_table 小球所属的 LVGL 迷宫表格。
 * @param[in] cell_size 单个迷宫格的像素边长。
 */
void ball_ui_init(lv_obj_t *maze_table, rt_uint16_t cell_size);

/**
 * @brief 将小球居中移动到指定迷宫格。
 * @param[in] x 目标列索引。
 * @param[in] y 目标行索引。
 * @note 调用者必须已经持有 LVGL 锁。
 */
void ball_ui_set_cell_locked(rt_uint8_t x, rt_uint8_t y);

#endif /* __APP_BALL_UI_H__ */
