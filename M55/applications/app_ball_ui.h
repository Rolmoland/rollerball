#ifndef __APP_BALL_UI_H__
#define __APP_BALL_UI_H__

#include <rtthread.h>

void ball_ui_init(void);

/* Move the ball to (dx, dy) pixels relative to screen center */
void ball_ui_set_pos(rt_int16_t dx, rt_int16_t dy);

#endif /* __APP_BALL_UI_H__ */
