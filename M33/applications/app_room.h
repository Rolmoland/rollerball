#ifndef __APP_ROOM_H__
#define __APP_ROOM_H__

#include <rtthread.h>

/* Thread-safe snapshot of the ball's position, in pixels relative to room center */
void room_get_pos(rt_int16_t *x, rt_int16_t *y);

#endif /* __APP_ROOM_H__ */
