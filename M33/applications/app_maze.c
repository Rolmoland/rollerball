#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_maze.h"

#define MAZE_RESET_POLL_MS 50
#define MAZE_RESET_PIN GET_PIN(CYBSP_USER_BTN_PORT_NUM, CYBSP_USER_BTN_PIN)

static struct rt_mutex g_reset_lock;
static volatile rt_bool_t g_reset_requested = RT_FALSE;
static rt_uint16_t g_reset_generation = 1U;

static void maze_app_reset(void);

static void maze_reset_button_irq(void *args)
{
    (void)args;
    g_reset_requested = RT_TRUE;
}

static void maze_reset_thread_entry(void *parameter)
{
    rt_base_t previous_button = PIN_HIGH;
    (void)parameter;

    while (1)
    {
        rt_base_t button = rt_pin_read(MAZE_RESET_PIN);

        if (g_reset_requested || (previous_button == PIN_HIGH && button == PIN_LOW))
        {
            g_reset_requested = RT_FALSE;
            maze_app_reset();
            rt_kprintf("[IMU] reset event from SW2\n");
        }
        previous_button = button;
        rt_thread_mdelay(MAZE_RESET_POLL_MS);
    }
}

static void maze_app_reset(void)
{
    rt_mutex_take(&g_reset_lock, RT_WAITING_FOREVER);
    g_reset_generation++;
    rt_mutex_release(&g_reset_lock);
}

rt_uint16_t maze_app_reset_generation(void)
{
    rt_uint16_t generation;

    rt_mutex_take(&g_reset_lock, RT_WAITING_FOREVER);
    generation = g_reset_generation;
    rt_mutex_release(&g_reset_lock);
    return generation;
}

static void maze_reset_cmd(void)
{
    maze_app_reset();
    rt_kprintf("[IMU] reset event sent\n");
}
MSH_CMD_EXPORT_ALIAS(maze_reset_cmd, maze_reset, Send a maze reset event to M55);

static void maze_print(void)
{
    rt_kprintf("[IMU] reset_generation=%u\n",
               (unsigned int)maze_app_reset_generation());
}
MSH_CMD_EXPORT(maze_print, Print current reset generation);

static int app_maze_init(void)
{
    rt_thread_t thread;

    rt_mutex_init(&g_reset_lock, "reset_lock", RT_IPC_FLAG_PRIO);
    rt_pin_mode(MAZE_RESET_PIN, PIN_MODE_INPUT_PULLUP);
    if (rt_pin_attach_irq(MAZE_RESET_PIN, PIN_IRQ_MODE_FALLING,
                          maze_reset_button_irq, RT_NULL) == RT_EOK)
    {
        rt_pin_irq_enable(MAZE_RESET_PIN, PIN_IRQ_ENABLE);
    }
    thread = rt_thread_create("maze_rst", maze_reset_thread_entry,
                              RT_NULL, 512, 17, 10);
    if (thread == RT_NULL)
    {
        rt_kprintf("[MAZE] thread create failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
INIT_APP_EXPORT(app_maze_init);
