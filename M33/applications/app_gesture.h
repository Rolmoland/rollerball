#ifndef __APP_GESTURE_H__
#define __APP_GESTURE_H__

typedef enum
{
    GESTURE_NONE = 0,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_LEFT,
    GESTURE_RIGHT,
} gesture_dir_t;

/* Latest debounced tilt direction */
gesture_dir_t gesture_get(void);

#endif /* __APP_GESTURE_H__ */
