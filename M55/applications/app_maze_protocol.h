#ifndef __APP_MAZE_PROTOCOL_H__
#define __APP_MAZE_PROTOCOL_H__

#include <stdint.h>

#define IMU_GESTURE_PAYLOAD_V1       0x4701U

#define IMU_CH_DIRECTION             0U
#define IMU_CH_RESET_GENERATION      1U
#define IMU_CH_PAYLOAD               7U

#define IMU_DIRECTION_NONE           0U
#define IMU_DIRECTION_UP             1U
#define IMU_DIRECTION_DOWN           2U
#define IMU_DIRECTION_LEFT           3U
#define IMU_DIRECTION_RIGHT          4U

#endif /* __APP_MAZE_PROTOCOL_H__ */
