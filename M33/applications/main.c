#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#define LED_PIN_B                 GET_PIN(16, 5)

/**
 * @brief M33 主线程入口，输出启动状态并维持状态灯心跳。
 * @return 不会正常返回。
 */
int main(void)
{
    rt_kprintf("\r\n");
    rt_kprintf("========================================\r\n");
    rt_kprintf("   RT-Thread on Cortex-M33 Core         \r\n");
    rt_kprintf("========================================\r\n");
    rt_kprintf("Status:  IMU gesture sender running\r\n");
    rt_kprintf("========================================\r\n");
    rt_kprintf("\r\n");

    rt_pin_mode(LED_PIN_B, PIN_MODE_OUTPUT);

    while (1)
    {
        rt_pin_write(LED_PIN_B, PIN_HIGH);
        rt_thread_mdelay(500);
        rt_pin_write(LED_PIN_B, PIN_LOW);
        rt_thread_mdelay(500);
    }

    return 0;
}
