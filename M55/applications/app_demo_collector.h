#ifndef __APP_DEMO_COLLECTOR_H__
#define __APP_DEMO_COLLECTOR_H__

#include <rtthread.h>

#define DEMO_MAX_TRANSITIONS 256U

typedef struct
{
    rt_uint8_t state_x;
    rt_uint8_t state_y;
    rt_uint8_t next_x;
    rt_uint8_t next_y;
    rt_uint8_t map_id;
    rt_uint8_t action;
    rt_uint8_t result;
    rt_uint8_t done;
    rt_uint16_t episode_id;
    rt_uint16_t step_index;
    rt_int16_t reward_tenths;
    rt_uint32_t transition_id;
} demo_transition_t;

/**
 * @brief 追加一条人工操作产生的状态转移样本。
 * @param[in] transition 待保存的样本，不得为 RT_NULL。
 * @return RT_EOK 表示成功；重复 ID 或缓存已满时返回错误。
 */
rt_err_t demo_collector_append(const demo_transition_t *transition);

/**
 * @brief 按索引读取一条已采集样本。
 * @param[in] index 样本索引，范围为 0 到 demo_collector_count()-1。
 * @param[out] transition 接收样本的缓冲区，不得为 RT_NULL。
 * @return RT_EOK 表示成功，索引越界时返回 -RT_EINVAL。
 */
rt_err_t demo_collector_get(rt_uint16_t index, demo_transition_t *transition);

/**
 * @brief 获取当前 RAM 中保存的样本数量。
 * @return 已保存的状态转移数量。
 */
rt_uint16_t demo_collector_count(void);

/**
 * @brief 清空全部人工演示样本及相关统计计数。
 */
void demo_collector_clear(void);

#endif /* __APP_DEMO_COLLECTOR_H__ */
