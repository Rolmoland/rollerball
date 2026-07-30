#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include "app_algorithm_manager.h"
#include "app_dqn_training.h"
#include "app_mode_manager.h"
#include "module_dqn_agent.h"
#include "module_maze_env.h"

#define DQN_TRAIN_DEFAULT_EPISODES      1000U
#define DQN_TRAIN_MAX_EPISODES         10000U
#define DQN_TRAIN_MAX_STEPS              200U
#define DQN_TRAIN_UPDATE_FREQUENCY         4U
#define DQN_TRAIN_LOG_INTERVAL             50U
#define DQN_TRAIN_THREAD_STACK_SIZE       3072U
#define DQN_TRAIN_THREAD_PRIORITY           22U
#define DQN_TRAIN_THREAD_TIMESLICE           10U
#define DQN_INFER_UI_STEP_DELAY_MS          200U
#define DQN_REWARD_GOAL                     1.00f
#define DQN_REWARD_COLLISION               -0.10f
#define DQN_REWARD_MOVE                    -0.01f
#define DQN_REWARD_DISTANCE_GAIN            0.05f
#define DQN_REWARD_TIMEOUT                 -1.00f

typedef struct
{
    rt_uint32_t trained_episodes;
    rt_uint32_t successful_episodes;
    rt_uint32_t train_updates;
    rt_uint32_t target_syncs;
    rt_uint32_t last_loss_milli;
    rt_uint16_t last_steps;
    rt_int32_t last_reward_tenths;
    rt_bool_t last_success;
} dqn_training_stats_t;

static dqn_agent_t s_dqn_agent;
static dqn_training_stats_t s_training_stats;
static dqn_training_ui_state_t s_ui_state;
static struct rt_mutex s_training_lock;
static rt_bool_t s_training_active;

static void ui_state_publish_locked(void)
{
    s_ui_state.revision++;
}

static rt_uint32_t loss_to_milli(float loss)
{
    if (!(loss > 0.0f))
    {
        return 0U;
    }
    if (loss >= 4294967.0f)
    {
        return 0xFFFFFFFFU;
    }
    return (rt_uint32_t)(loss * 1000.0f);
}

static float training_reward(maze_env_step_result_t result,
                             rt_uint8_t current_distance,
                             rt_uint8_t next_distance,
                             rt_bool_t timed_out)
{
    if (timed_out)
    {
        return DQN_REWARD_TIMEOUT;
    }
    if (result == MAZE_ENV_STEP_GOAL)
    {
        return DQN_REWARD_GOAL;
    }
    if (result == MAZE_ENV_STEP_COLLISION)
    {
        return DQN_REWARD_COLLISION;
    }
    if (result == MAZE_ENV_STEP_MOVED)
    {
        if (current_distance != MAZE_ENV_DISTANCE_UNREACHABLE &&
            next_distance != MAZE_ENV_DISTANCE_UNREACHABLE)
        {
            return DQN_REWARD_MOVE + DQN_REWARD_DISTANCE_GAIN *
                ((int)current_distance - (int)next_distance);
        }
        return DQN_REWARD_MOVE;
    }
    return 0.0f;
}

rt_err_t dqn_training_get_ui_state(dqn_training_ui_state_t *state)
{
    if (state == RT_NULL)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    *state = s_ui_state;
    rt_mutex_release(&s_training_lock);
    return RT_EOK;
}

rt_err_t dqn_training_reset(void)
{
    rt_uint32_t revision;

    if (app_algorithm_acquire(APP_ALGORITHM_DQN) != RT_EOK)
    {
        return -RT_EBUSY;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    if (s_training_active)
    {
        rt_mutex_release(&s_training_lock);
        app_algorithm_release(APP_ALGORITHM_DQN);
        return -RT_EBUSY;
    }

    dqn_agent_reset(&s_dqn_agent);
    memset(&s_training_stats, 0, sizeof(s_training_stats));
    revision = s_ui_state.revision;
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    s_ui_state.revision = revision;
    s_ui_state.phase = DQN_TRAINING_UI_READY;
    s_ui_state.epsilon_milli =
        (rt_uint32_t)(s_dqn_agent.epsilon * 1000.0f);
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);
    app_algorithm_release(APP_ALGORITHM_DQN);
    app_mode_set(APP_MODE_DQN_TRAIN);
    return RT_EOK;
}

static rt_err_t command_count(int argc, char **argv, rt_uint32_t *count)
{
    unsigned long value;
    char *end;

    if (argc == 1)
    {
        *count = DQN_TRAIN_DEFAULT_EPISODES;
        return RT_EOK;
    }
    if (argc != 2)
    {
        return -RT_EINVAL;
    }

    value = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || value == 0UL ||
        value > DQN_TRAIN_MAX_EPISODES)
    {
        return -RT_EINVAL;
    }
    *count = (rt_uint32_t)value;
    return RT_EOK;
}

static rt_bool_t training_try_start(void)
{
    rt_bool_t started = RT_FALSE;

    if (app_algorithm_acquire(APP_ALGORITHM_DQN) != RT_EOK)
    {
        return RT_FALSE;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    if (!s_training_active)
    {
        s_training_active = RT_TRUE;
        started = RT_TRUE;
    }
    rt_mutex_release(&s_training_lock);
    if (!started)
    {
        app_algorithm_release(APP_ALGORITHM_DQN);
    }
    return started;
}

static void training_finish(void)
{
    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_training_active = RT_FALSE;
    rt_mutex_release(&s_training_lock);
    app_algorithm_release(APP_ALGORITHM_DQN);
}

static void dqn_train_thread_entry(void *parameter)
{
    rt_uint32_t episodes = (rt_uint32_t)(rt_ubase_t)parameter;
    rt_uint32_t completed_episodes = 0U;
    rt_uint32_t run_successes = 0U;
    rt_uint8_t goal_distances[MAZE_ENV_STATE_COUNT];
    rt_uint32_t episode;

    if (maze_env_build_goal_distances(0U, goal_distances) != RT_EOK)
    {
        rt_kprintf("[DQN] goal distance map build failed\n");
        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_ui_state.phase = DQN_TRAINING_UI_READY;
        ui_state_publish_locked();
        rt_mutex_release(&s_training_lock);
        training_finish();
        return;
    }

    for (episode = 1U; episode <= episodes; episode++)
    {
        maze_env_t env;
        float episode_reward = 0.0f;
        float last_loss = 0.0f;
        rt_uint16_t step;

        if (maze_env_init(&env, 0U) != RT_EOK)
        {
            rt_kprintf("[DQN] environment init failed\n");
            break;
        }

        for (step = 0U; step < DQN_TRAIN_MAX_STEPS; step++)
        {
            rt_uint16_t state = maze_env_state_index(env.agent_x,
                                                     env.agent_y);
            maze_env_action_t action =
                dqn_agent_select_action(&s_dqn_agent, state, RT_TRUE);
            maze_env_step_result_t result;
            rt_uint16_t next_state;
            rt_bool_t done;
            rt_bool_t timed_out;
            rt_bool_t terminal;
            float reward;

            result = maze_env_step(&env, action, RT_NULL);
            next_state = maze_env_state_index(env.agent_x, env.agent_y);
            done = maze_env_is_done(&env);
            timed_out = !done && step + 1U >= DQN_TRAIN_MAX_STEPS;
            terminal = done || timed_out;
            reward = training_reward(result,
                                     goal_distances[state],
                                     goal_distances[next_state],
                                     timed_out);
            episode_reward += reward;
            dqn_agent_remember(&s_dqn_agent, state, action, reward,
                               next_state, terminal);
            if (terminal ||
                env.total_steps % DQN_TRAIN_UPDATE_FREQUENCY == 0U)
            {
                dqn_agent_train_batch(&s_dqn_agent, &last_loss);
            }
            if (terminal)
            {
                break;
            }
        }

        completed_episodes = episode;
        dqn_agent_decay_epsilon(&s_dqn_agent);
        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_training_stats.trained_episodes++;
        s_training_stats.last_steps = env.total_steps;
        s_training_stats.last_reward_tenths =
            (rt_int32_t)(episode_reward * 10.0f);
        s_training_stats.last_success = maze_env_is_done(&env);
        s_training_stats.last_loss_milli = loss_to_milli(last_loss);
        s_training_stats.train_updates = s_dqn_agent.train_updates;
        s_training_stats.target_syncs = s_dqn_agent.target_syncs;
        if (s_training_stats.last_success)
        {
            s_training_stats.successful_episodes++;
            run_successes++;
        }

        s_ui_state.phase = DQN_TRAINING_UI_TRAINING;
        s_ui_state.current_episode = episode;
        s_ui_state.successful_episodes = run_successes;
        s_ui_state.training_steps = env.total_steps;
        s_ui_state.replay_count = s_dqn_agent.replay_count;
        s_ui_state.train_updates = s_dqn_agent.train_updates;
        s_ui_state.target_syncs = s_dqn_agent.target_syncs;
        s_ui_state.training_reward_tenths =
            s_training_stats.last_reward_tenths;
        s_ui_state.epsilon_milli =
            (rt_uint32_t)(s_dqn_agent.epsilon * 1000.0f);
        s_ui_state.loss_milli = s_training_stats.last_loss_milli;
        ui_state_publish_locked();

        if (episode == 1U || episode == episodes ||
            episode % DQN_TRAIN_LOG_INTERVAL == 0U)
        {
            rt_kprintf("[DQN] episode=%lu/%lu reward_x10=%ld steps=%u "
                       "success=%u epsilon_x1000=%lu replay=%u "
                       "updates=%lu loss_x1000=%lu\n",
                       (unsigned long)episode,
                       (unsigned long)episodes,
                       (long)s_training_stats.last_reward_tenths,
                       (unsigned int)s_training_stats.last_steps,
                       (unsigned int)s_training_stats.last_success,
                       (unsigned long)s_ui_state.epsilon_milli,
                       (unsigned int)s_ui_state.replay_count,
                       (unsigned long)s_ui_state.train_updates,
                       (unsigned long)s_ui_state.loss_milli);
        }
        rt_mutex_release(&s_training_lock);
        rt_thread_mdelay(1U);
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = DQN_TRAINING_UI_TRAINED;
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);
    training_finish();
    rt_kprintf("[DQN] train complete episodes=%lu success=%lu\n",
               (unsigned long)completed_episodes,
               (unsigned long)run_successes);
}

static int dqn_train_cmd(int argc, char **argv)
{
    rt_uint32_t episodes;
    rt_thread_t thread;
    maze_env_t initial_env;

    if (command_count(argc, argv, &episodes) != RT_EOK)
    {
        rt_kprintf("Usage: dqn_train [1-%u]\n",
                   (unsigned int)DQN_TRAIN_MAX_EPISODES);
        return -RT_EINVAL;
    }
    if (maze_env_init(&initial_env, 0U) != RT_EOK)
    {
        rt_kprintf("[DQN] environment init failed\n");
        return -RT_ERROR;
    }
    if (!training_try_start())
    {
        rt_kprintf("[DQN] algorithm task is busy\n");
        return -RT_EBUSY;
    }
    app_mode_set(APP_MODE_DQN_TRAIN);

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = DQN_TRAINING_UI_TRAINING;
    s_ui_state.agent_x = initial_env.agent_x;
    s_ui_state.agent_y = initial_env.agent_y;
    s_ui_state.training_steps = 0U;
    s_ui_state.current_episode = 0U;
    s_ui_state.target_episodes = episodes;
    s_ui_state.successful_episodes = 0U;
    s_ui_state.training_reward_tenths = 0;
    s_ui_state.loss_milli = 0U;
    s_ui_state.epsilon_milli =
        (rt_uint32_t)(s_dqn_agent.epsilon * 1000.0f);
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);

    thread = rt_thread_create("dqntrain",
                              dqn_train_thread_entry,
                              (void *)(rt_ubase_t)episodes,
                              DQN_TRAIN_THREAD_STACK_SIZE,
                              DQN_TRAIN_THREAD_PRIORITY,
                              DQN_TRAIN_THREAD_TIMESLICE);
    if (thread == RT_NULL)
    {
        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_ui_state.phase = DQN_TRAINING_UI_READY;
        ui_state_publish_locked();
        rt_mutex_release(&s_training_lock);
        training_finish();
        rt_kprintf("[DQN] training thread create failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    rt_kprintf("[DQN] training started episodes=%lu\n",
               (unsigned long)episodes);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(dqn_train_cmd, dqn_train,
                     Train DQN in the local maze environment);

static int dqn_infer_cmd(int argc, char **argv)
{
    maze_env_t env;
    rt_uint16_t step;

    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: dqn_infer\n");
        return -RT_EINVAL;
    }
    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    if (s_training_stats.trained_episodes == 0U)
    {
        rt_mutex_release(&s_training_lock);
        rt_kprintf("[DQN] no trained model; run dqn_train first\n");
        return -RT_EEMPTY;
    }
    rt_mutex_release(&s_training_lock);

    if (!training_try_start())
    {
        rt_kprintf("[DQN] algorithm task is busy\n");
        return -RT_EBUSY;
    }
    if (maze_env_init(&env, 0U) != RT_EOK)
    {
        training_finish();
        rt_kprintf("[DQN] environment init failed\n");
        return -RT_ERROR;
    }
    app_mode_set(APP_MODE_DQN_INFER);

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = DQN_TRAINING_UI_INFERENCE;
    s_ui_state.agent_x = env.agent_x;
    s_ui_state.agent_y = env.agent_y;
    s_ui_state.inference_complete = RT_FALSE;
    s_ui_state.inference_success = RT_FALSE;
    s_ui_state.inference_steps = 0U;
    s_ui_state.inference_reward_tenths = 0;
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);

    for (step = 1U; step <= DQN_TRAIN_MAX_STEPS; step++)
    {
        rt_uint8_t state_x = env.agent_x;
        rt_uint8_t state_y = env.agent_y;
        rt_uint16_t state = maze_env_state_index(state_x, state_y);
        maze_env_action_t action =
            dqn_agent_select_action(&s_dqn_agent, state, RT_FALSE);
        maze_env_step_result_t result;
        float reward;
        rt_bool_t done;

        result = maze_env_step(&env, action, &reward);
        done = maze_env_is_done(&env);
        rt_kprintf("DQN_PATH,%u,%u,%u,%u,%u,%u,%ld,%u,%u\n",
                   (unsigned int)step,
                   (unsigned int)state_x,
                   (unsigned int)state_y,
                   (unsigned int)action,
                   (unsigned int)env.agent_x,
                   (unsigned int)env.agent_y,
                   (long)(reward * 10.0f),
                   (unsigned int)result,
                   (unsigned int)done);

        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_ui_state.agent_x = env.agent_x;
        s_ui_state.agent_y = env.agent_y;
        s_ui_state.inference_success = done;
        s_ui_state.inference_steps = env.total_steps;
        s_ui_state.inference_reward_tenths =
            (rt_int32_t)(env.cumulative_reward * 10.0f);
        ui_state_publish_locked();
        rt_mutex_release(&s_training_lock);
        rt_thread_mdelay(DQN_INFER_UI_STEP_DELAY_MS);
        if (done)
        {
            break;
        }
    }

    rt_kprintf("[DQN] infer success=%u steps=%u reward_x10=%ld\n",
               (unsigned int)maze_env_is_done(&env),
               (unsigned int)env.total_steps,
               (long)(env.cumulative_reward * 10.0f));
    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = DQN_TRAINING_UI_INFERENCE_DONE;
    s_ui_state.inference_complete = RT_TRUE;
    s_ui_state.inference_success = maze_env_is_done(&env);
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);
    training_finish();
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(dqn_infer_cmd, dqn_infer,
                     Run one greedy DQN inference episode);

static int dqn_stats_cmd(int argc, char **argv)
{
    dqn_training_stats_t stats;
    dqn_training_ui_state_t state;
    rt_bool_t active;

    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: dqn_stats\n");
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    stats = s_training_stats;
    state = s_ui_state;
    active = s_training_active;
    rt_mutex_release(&s_training_lock);
    rt_kprintf("[DQN] network=100-32-16-4 replay=%u/%u batch=%u "
               "train_every=%u\n",
               (unsigned int)state.replay_count,
               (unsigned int)DQN_AGENT_REPLAY_CAPACITY,
               (unsigned int)DQN_AGENT_BATCH_SIZE,
               (unsigned int)DQN_TRAIN_UPDATE_FREQUENCY);
    rt_kprintf("[DQN] episodes=%lu success=%lu epsilon_x1000=%lu "
               "updates=%lu target_syncs=%lu busy=%u\n",
               (unsigned long)stats.trained_episodes,
               (unsigned long)stats.successful_episodes,
               (unsigned long)state.epsilon_milli,
               (unsigned long)stats.train_updates,
               (unsigned long)stats.target_syncs,
               (unsigned int)active);
    rt_kprintf("[DQN] last success=%u steps=%u reward_x10=%ld "
               "loss_x1000=%lu\n",
               (unsigned int)stats.last_success,
               (unsigned int)stats.last_steps,
               (long)stats.last_reward_tenths,
               (unsigned long)stats.last_loss_milli);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(dqn_stats_cmd, dqn_stats, Print DQN training statistics);

static int dqn_reset_cmd(int argc, char **argv)
{
    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: dqn_reset\n");
        return -RT_EINVAL;
    }
    if (dqn_training_reset() != RT_EOK)
    {
        rt_kprintf("[DQN] algorithm task is busy\n");
        return -RT_EBUSY;
    }
    rt_kprintf("[DQN] reset complete\n");
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(dqn_reset_cmd, dqn_reset,
                     Reset DQN networks replay and statistics);

static int app_dqn_training_init(void)
{
    rt_mutex_init(&s_training_lock, "dqnlock", RT_IPC_FLAG_PRIO);
    dqn_agent_init(&s_dqn_agent, 0U);
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    s_ui_state.phase = DQN_TRAINING_UI_READY;
    s_ui_state.epsilon_milli =
        (rt_uint32_t)(s_dqn_agent.epsilon * 1000.0f);
    return RT_EOK;
}
INIT_APP_EXPORT(app_dqn_training_init);
