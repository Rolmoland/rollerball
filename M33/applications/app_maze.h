#ifndef __APP_MAZE_H__
#define __APP_MAZE_H__

#include <rtthread.h>

/**
 * @brief 获取 M33 侧复位事件代数。
 * @return SW2 或 maze_reset 每触发一次就递增的 16 位代数值。
 * @note M55 通过检测该值变化来复位当前迷宫，不依赖具体数值大小。
 */
rt_uint16_t maze_app_reset_generation(void);

#endif /* __APP_MAZE_H__ */
