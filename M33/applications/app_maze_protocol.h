#ifndef __APP_MAZE_PROTOCOL_H__
#define __APP_MAZE_PROTOCOL_H__

#include <stdint.h>

#define MAZE_PAYLOAD_V1              0x4D01U
#define DEMO_PAYLOAD_V1              0x4401U

#define MAZE_CH_POSITION             0U
#define MAZE_CH_STATUS               1U
#define MAZE_CH_STEP_COUNT           2U
#define MAZE_CH_TOTAL_STEPS          3U
#define MAZE_CH_COLLISIONS           4U
#define MAZE_CH_LAST_REWARD          5U
#define MAZE_CH_TOTAL_REWARD         6U
#define MAZE_CH_PAYLOAD              7U

#define DEMO_CH_STATE                0U
#define DEMO_CH_NEXT_STATE           1U
#define DEMO_CH_STATUS               2U
#define DEMO_CH_EPISODE              3U
#define DEMO_CH_STEP_INDEX           4U
#define DEMO_CH_REWARD               5U
#define DEMO_CH_RESERVED             6U
#define DEMO_CH_PAYLOAD              7U

#define MAZE_PACK_POSITION(x, y) \
    ((uint16_t)(((uint16_t)(x) & 0xFFU) | (((uint16_t)(y) & 0xFFU) << 8)))
#define MAZE_POSITION_X(value)       ((uint8_t)((value) & 0xFFU))
#define MAZE_POSITION_Y(value)       ((uint8_t)(((value) >> 8) & 0xFFU))

#define MAZE_PACK_STATUS(map, action, result, done) \
    ((uint16_t)(((uint16_t)(map) & 0x7U) | \
                (((uint16_t)(action) & 0x7U) << 3) | \
                (((uint16_t)(result) & 0x3U) << 6) | \
                (((uint16_t)(done) & 0x1U) << 8)))
#define MAZE_STATUS_MAP(value)       ((uint8_t)((value) & 0x7U))
#define MAZE_STATUS_ACTION(value)    ((uint8_t)(((value) >> 3) & 0x7U))
#define MAZE_STATUS_RESULT(value)    ((uint8_t)(((value) >> 6) & 0x3U))
#define MAZE_STATUS_DONE(value)      ((uint8_t)(((value) >> 8) & 0x1U))

#endif /* __APP_MAZE_PROTOCOL_H__ */
