#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <lv_rt_thread_conf.h>
#include "app_maze_ui.h"

#define LED_PIN_G               GET_PIN(16, 6)

/**
 * @brief LVGL 线程创建完成后的用户界面初始化回调。
 */
void lv_user_gui_init(void)
{
    maze_ui_init();
}

/**
 * @brief M55 主线程入口，启动 LVGL 并维持状态灯心跳。
 * @return 不会正常返回。
 */
int main(void)
{
    rt_kprintf("Hello RT-Thread\n");
    rt_kprintf("It's cortex-m55\n");
    rt_pin_mode(LED_PIN_G, PIN_MODE_OUTPUT);
    lvgl_thread_init();

    while (1)
    {
        rt_pin_write(LED_PIN_G, PIN_LOW);
        rt_thread_mdelay(500);
        rt_pin_write(LED_PIN_G, PIN_HIGH);
        rt_thread_mdelay(500);
    }
    return 0;
}
