#ifndef __APP_MAZE_PROTOCOL_H__
#define __APP_MAZE_PROTOCOL_H__

#include <stdint.h>

/* M33 到 M55 的 IMU 方向事件协议版本标识。 */
#define IMU_GESTURE_PAYLOAD_V1       0x4701U

/* edge_rc_frame_t.channel[] 的通道分配。 */
#define IMU_CH_DIRECTION             0U
#define IMU_CH_RESET_GENERATION      1U
#define IMU_CH_PAYLOAD               7U

/* 线协议方向值，接收端将其映射为迷宫动作。 */
#define IMU_DIRECTION_NONE           0U
#define IMU_DIRECTION_UP             1U
#define IMU_DIRECTION_DOWN           2U
#define IMU_DIRECTION_LEFT           3U
#define IMU_DIRECTION_RIGHT          4U

#endif /* __APP_MAZE_PROTOCOL_H__ */
