#ifndef __APP_MAZE_UI_H__
#define __APP_MAZE_UI_H__

#include <rtthread.h>

typedef struct
{
    rt_uint8_t agent_x;
    rt_uint8_t agent_y;
    rt_uint8_t map_id;
    rt_uint8_t action;
    rt_uint8_t result;
    rt_uint8_t done;
    rt_uint16_t step_count;
    rt_uint16_t total_steps;
    rt_uint16_t collision_count;
    rt_int16_t last_reward_tenths;
    rt_int16_t total_reward_tenths;
    rt_uint32_t revision;
} maze_ui_state_t;

/**
 * @brief 创建迷宫、状态区、比较表和完成提示等 LVGL 对象。
 * @note 由 LVGL 用户界面初始化回调调用一次。
 */
void maze_ui_init(void);

/**
 * @brief 提交人工演示环境的最新状态并按需刷新界面。
 * @param[in] state M55 当前迷宫状态，不得为 RT_NULL。
 */
void maze_ui_update(const maze_ui_state_t *state);

/**
 * @brief 更新人工演示样本数量显示。
 * @param[in] count 当前保存在 RAM 中的样本数量。
 */
void maze_ui_set_demo_count(rt_uint16_t count);

/**
 * @brief 当前活动地图改变后重建表格内容并复位各模式轨迹。
 * @return RT_EOK 表示刷新成功；界面未就绪或状态读取失败时返回错误。
 */
rt_err_t maze_ui_reload_map(void);

#endif /* __APP_MAZE_UI_H__ */
