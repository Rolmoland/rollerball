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

/**
 * @brief 获取经过阈值和回差处理后的当前倾斜方向。
 * @return 当前方向；板卡回正或未达到触发阈值时返回 GESTURE_NONE。
 */
gesture_dir_t gesture_get(void);

#endif /* __APP_GESTURE_H__ */
