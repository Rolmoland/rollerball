#ifndef __APP_RANDOM_BASELINE_H__
#define __APP_RANDOM_BASELINE_H__

#include <rtthread.h>

typedef enum
{
    RANDOM_BASELINE_UI_READY = 0,
    RANDOM_BASELINE_UI_RUNNING,
    RANDOM_BASELINE_UI_COMPLETE,
} random_baseline_ui_phase_t;

typedef struct
{
    random_baseline_ui_phase_t phase;
    rt_uint32_t current_episode;
    rt_uint32_t target_episodes;
    rt_uint32_t successful_episodes;
    rt_uint16_t average_success_steps;
    rt_uint16_t best_steps;
    rt_uint32_t revision;
} random_baseline_ui_state_t;

rt_err_t random_baseline_get_ui_state(random_baseline_ui_state_t *state);
rt_err_t random_baseline_reset(void);
rt_bool_t random_baseline_is_busy(void);

#endif /* __APP_RANDOM_BASELINE_H__ */
