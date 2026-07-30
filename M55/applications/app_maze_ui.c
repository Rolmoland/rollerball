#include "lvgl.h"
#include <string.h>
#include "app_ball_ui.h"
#include "app_demo_collector.h"
#include "app_dqn_training.h"
#include "app_maze_ui.h"
#include "app_mode_manager.h"
#include "app_q_training.h"
#include "app_random_baseline.h"
#include "module_maze_env.h"

#define MAZE_SIZE        MAZE_ENV_SIZE
#define MAZE_CELL_PX     40
#define MAZE_ORIGIN_X    56
#define MAZE_ORIGIN_Y    96
#define UI_POLL_MS       100

static lv_obj_t *s_table;
static lv_obj_t *s_status_label;
static lv_obj_t *s_stats_label;
static lv_obj_t *s_complete_panel;
static lv_obj_t *s_complete_label;
static maze_ui_state_t s_state;
static q_training_ui_state_t s_q_state;
static dqn_training_ui_state_t s_dqn_state;
static random_baseline_ui_state_t s_random_state;
static app_mode_state_t s_mode_state;
static rt_uint8_t s_demo_visited[MAZE_SIZE][MAZE_SIZE];
static rt_uint8_t s_infer_visited[MAZE_SIZE][MAZE_SIZE];
static rt_uint8_t s_dqn_infer_visited[MAZE_SIZE][MAZE_SIZE];
static rt_uint16_t s_demo_count;
static rt_uint8_t s_start_x;
static rt_uint8_t s_start_y;
static rt_bool_t s_ready = RT_FALSE;

static void set_reward_text(lv_obj_t *label, const maze_ui_state_t *state);

static void set_status_text(void)
{
    lv_label_set_text_fmt(s_status_label, "DEMO  MAP %u  %s  N=%u",
                          s_state.map_id,
                          s_state.done ? "COMPLETE" : "RUNNING",
                          s_demo_count);
}

static const char *training_status_text(const q_training_ui_state_t *state)
{
    if (state->phase == Q_TRAINING_UI_PRETRAIN)
    {
        return "TRAIN PREP";
    }
    if (state->phase == Q_TRAINING_UI_TRAINING)
    {
        return "TRAIN RUNNING";
    }
    if (state->current_episode > 0U)
    {
        return "TRAIN COMPLETE";
    }
    return "TRAIN READY";
}

static void set_training_text(const q_training_ui_state_t *state)
{
    rt_int32_t reward_abs = state->training_reward_tenths < 0 ?
                            -state->training_reward_tenths :
                            state->training_reward_tenths;

    lv_label_set_text_fmt(s_status_label, "%s  MAP R%lu  DEMO N=%u",
                          training_status_text(state),
                          (unsigned long)maze_env_map_revision(),
                          s_demo_count);
    lv_label_set_text_fmt(s_stats_label,
                          "EP %lu/%lu  OK %lu  EPS %lu.%03lu\n"
                          "REWARD %s%ld.%ld  STEPS %u",
                          (unsigned long)state->current_episode,
                          (unsigned long)state->target_episodes,
                          (unsigned long)state->successful_episodes,
                          (unsigned long)(state->epsilon_milli / 1000U),
                          (unsigned long)(state->epsilon_milli % 1000U),
                          state->training_reward_tenths < 0 ? "-" : "",
                          (long)(reward_abs / 10),
                          (long)(reward_abs % 10),
                          (unsigned int)state->training_steps);
}

static void set_inference_text(const q_training_ui_state_t *state)
{
    const char *status;
    rt_int32_t reward_abs = state->inference_reward_tenths < 0 ?
                            -state->inference_reward_tenths :
                            state->inference_reward_tenths;

    if (state->phase == Q_TRAINING_UI_INFERENCE)
    {
        status = "INFER RUNNING";
    }
    else if (state->inference_complete)
    {
        status = state->inference_success ?
                 "INFER COMPLETE" : "INFER FAILED";
    }
    else
    {
        status = "INFER READY";
    }

    lv_label_set_text_fmt(s_status_label, "%s  MAP R%lu  DEMO N=%u",
                          status,
                          (unsigned long)maze_env_map_revision(),
                          s_demo_count);
    lv_label_set_text_fmt(s_stats_label,
                          "EP %lu/%lu  EPS %lu.%03lu\n"
                          "REWARD %s%ld.%ld  STEPS %u",
                          (unsigned long)state->current_episode,
                          (unsigned long)state->target_episodes,
                          (unsigned long)(state->epsilon_milli / 1000U),
                          (unsigned long)(state->epsilon_milli % 1000U),
                          state->inference_reward_tenths < 0 ? "-" : "",
                          (long)(reward_abs / 10),
                          (long)(reward_abs % 10),
                          (unsigned int)state->inference_steps);
}

static const char *dqn_training_status_text(
    const dqn_training_ui_state_t *state)
{
    if (state->phase == DQN_TRAINING_UI_TRAINING)
    {
        return "DQN TRAIN RUNNING";
    }
    if (state->current_episode > 0U)
    {
        return "DQN TRAIN COMPLETE";
    }
    return "DQN TRAIN READY";
}

static void set_dqn_training_text(const dqn_training_ui_state_t *state)
{
    rt_int32_t reward_abs = state->training_reward_tenths < 0 ?
                            -state->training_reward_tenths :
                            state->training_reward_tenths;

    lv_label_set_text_fmt(s_status_label, "%s  MAP R%lu",
                          dqn_training_status_text(state),
                          (unsigned long)maze_env_map_revision());
    lv_label_set_text_fmt(s_stats_label,
                          "EP %lu/%lu  OK %lu  EPS %lu.%03lu\n"
                          "REWARD %s%ld.%ld  STEPS %u\n"
                          "LOSS %lu.%03lu  BUF %u  UPD %lu",
                          (unsigned long)state->current_episode,
                          (unsigned long)state->target_episodes,
                          (unsigned long)state->successful_episodes,
                          (unsigned long)(state->epsilon_milli / 1000U),
                          (unsigned long)(state->epsilon_milli % 1000U),
                          state->training_reward_tenths < 0 ? "-" : "",
                          (long)(reward_abs / 10),
                          (long)(reward_abs % 10),
                          (unsigned int)state->training_steps,
                          (unsigned long)(state->loss_milli / 1000U),
                          (unsigned long)(state->loss_milli % 1000U),
                          (unsigned int)state->replay_count,
                          (unsigned long)state->train_updates);
}

static void set_dqn_inference_text(const dqn_training_ui_state_t *state)
{
    const char *status;
    rt_int32_t reward_abs = state->inference_reward_tenths < 0 ?
                            -state->inference_reward_tenths :
                            state->inference_reward_tenths;

    if (state->phase == DQN_TRAINING_UI_INFERENCE)
    {
        status = "DQN INFER RUNNING";
    }
    else if (state->inference_complete)
    {
        status = state->inference_success ?
                 "DQN INFER COMPLETE" : "DQN INFER FAILED";
    }
    else
    {
        status = "DQN INFER READY";
    }

    lv_label_set_text_fmt(s_status_label, "%s  MAP R%lu",
                          status,
                          (unsigned long)maze_env_map_revision());
    lv_label_set_text_fmt(s_stats_label,
                          "EPS %lu.%03lu  UPD %lu\n"
                          "REWARD %s%ld.%ld  STEPS %u",
                          (unsigned long)(state->epsilon_milli / 1000U),
                          (unsigned long)(state->epsilon_milli % 1000U),
                          (unsigned long)state->train_updates,
                          state->inference_reward_tenths < 0 ? "-" : "",
                          (long)(reward_abs / 10),
                          (long)(reward_abs % 10),
                          (unsigned int)state->inference_steps);
}

static rt_uint16_t latest_completed_demo_steps(void)
{
    rt_uint16_t count = demo_collector_count();

    while (count > 0U)
    {
        demo_transition_t transition;

        count--;
        if (demo_collector_get(count, &transition) == RT_EOK &&
            transition.done)
        {
            return transition.step_index;
        }
    }
    return 0U;
}

static void set_compare_text(const q_training_ui_state_t *q_state,
                             const dqn_training_ui_state_t *dqn_state,
                             const random_baseline_ui_state_t *random_state)
{
    rt_uint16_t human_steps = latest_completed_demo_steps();
    char human_text[20];
    char q_text[32];
    char dqn_text[20];

    if (human_steps > 0U)
    {
        rt_snprintf(human_text, sizeof(human_text), "HUMAN %u",
                    (unsigned int)human_steps);
    }
    else
    {
        rt_snprintf(human_text, sizeof(human_text), "HUMAN --");
    }
    if (q_state->inference_complete)
    {
        if (!q_state->inference_success)
        {
            rt_snprintf(q_text, sizeof(q_text), "Q FAIL");
        }
        else if (human_steps > 0U)
        {
            int saved_steps = (int)human_steps -
                              (int)q_state->inference_steps;

            rt_snprintf(q_text, sizeof(q_text), "Q %u SAVE %d",
                        (unsigned int)q_state->inference_steps,
                        saved_steps);
        }
        else
        {
            rt_snprintf(q_text, sizeof(q_text), "Q %u",
                        (unsigned int)q_state->inference_steps);
        }
    }
    else
    {
        rt_snprintf(q_text, sizeof(q_text), "Q --");
    }
    if (dqn_state->inference_complete)
    {
        if (dqn_state->inference_success)
        {
            rt_snprintf(dqn_text, sizeof(dqn_text), "DQN %u",
                        (unsigned int)dqn_state->inference_steps);
        }
        else
        {
            rt_snprintf(dqn_text, sizeof(dqn_text), "DQN FAIL");
        }
    }
    else
    {
        rt_snprintf(dqn_text, sizeof(dqn_text), "DQN --");
    }

    lv_label_set_text_fmt(
        s_status_label, "%s  MAP R%lu",
        random_state->phase == RANDOM_BASELINE_UI_RUNNING ?
        "RANDOM RUNNING" : "COMPARE",
        (unsigned long)maze_env_map_revision());
    if (random_state->phase == RANDOM_BASELINE_UI_RUNNING)
    {
        lv_label_set_text_fmt(s_stats_label,
                              "RND EP %lu/%lu  OK %lu\n%s  %s  %s",
                              (unsigned long)random_state->current_episode,
                              (unsigned long)random_state->target_episodes,
                              (unsigned long)random_state->successful_episodes,
                              human_text, q_text, dqn_text);
    }
    else if (random_state->target_episodes == 0U)
    {
        lv_label_set_text_fmt(s_stats_label, "RND --\n%s  %s  %s",
                              human_text, q_text, dqn_text);
    }
    else if (random_state->successful_episodes == 0U)
    {
        lv_label_set_text_fmt(s_stats_label,
                              "RND 0/%lu  AVG --\n%s  %s  %s",
                              (unsigned long)random_state->target_episodes,
                              human_text, q_text, dqn_text);
    }
    else
    {
        lv_label_set_text_fmt(s_stats_label,
                              "RND %lu/%lu  AVG %u\n%s  %s  %s",
                              (unsigned long)random_state->successful_episodes,
                              (unsigned long)random_state->target_episodes,
                              (unsigned int)random_state->average_success_steps,
                              human_text, q_text, dqn_text);
    }
}

static void hide_complete_panel(void)
{
    lv_obj_add_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);
}

static void show_physical_complete(void)
{
    lv_label_set_text(s_complete_label,
                      "GOAL REACHED\nPRESS SW2 TO RESET");
    lv_obj_center(s_complete_label);
    lv_obj_set_style_bg_color(s_complete_panel,
                              lv_color_hex(0x2E7D32), 0);
    lv_obj_remove_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_complete_panel);
}

static void show_inference_complete(const q_training_ui_state_t *state)
{
    lv_label_set_text_fmt(s_complete_label,
                          state->inference_success ?
                          "Q INFERENCE COMPLETE\n%u STEPS" :
                          "Q INFERENCE FAILED\n%u STEPS",
                          (unsigned int)state->inference_steps);
    lv_obj_center(s_complete_label);
    lv_obj_set_style_bg_color(s_complete_panel,
                              state->inference_success ?
                              lv_color_hex(0x2E7D32) :
                              lv_color_hex(0xC62828),
                              0);
    lv_obj_remove_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_complete_panel);
}

static void show_dqn_inference_complete(
    const dqn_training_ui_state_t *state)
{
    lv_label_set_text_fmt(s_complete_label,
                          state->inference_success ?
                          "DQN INFERENCE COMPLETE\n%u STEPS" :
                          "DQN INFERENCE FAILED\n%u STEPS",
                          (unsigned int)state->inference_steps);
    lv_obj_center(s_complete_label);
    lv_obj_set_style_bg_color(s_complete_panel,
                              state->inference_success ?
                              lv_color_hex(0x2E7D32) :
                              lv_color_hex(0xC62828),
                              0);
    lv_obj_remove_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_complete_panel);
}

static void render_physical_state(void)
{
    if (s_state.agent_x < MAZE_SIZE && s_state.agent_y < MAZE_SIZE)
    {
        ball_ui_set_cell_locked(s_state.agent_x, s_state.agent_y);
    }
    set_status_text();
    set_reward_text(s_stats_label, &s_state);
    if (s_state.done)
    {
        show_physical_complete();
    }
    else
    {
        hide_complete_panel();
    }
}

static void render_current_mode(rt_bool_t mode_changed,
                                q_training_ui_phase_t old_q_phase,
                                dqn_training_ui_phase_t old_dqn_phase)
{
    if (s_mode_state.mode == APP_MODE_DEMO)
    {
        render_physical_state();
    }
    else if (s_mode_state.mode == APP_MODE_TRAIN)
    {
        ball_ui_set_cell_locked(s_start_x, s_start_y);
        set_training_text(&s_q_state);
        hide_complete_panel();
    }
    else if (s_mode_state.mode == APP_MODE_INFER)
    {
        if (s_q_state.phase == Q_TRAINING_UI_INFERENCE &&
            old_q_phase != Q_TRAINING_UI_INFERENCE)
        {
            memset(s_infer_visited, 0, sizeof(s_infer_visited));
            s_infer_visited[s_start_y][s_start_x] = 1U;
        }
        if (s_q_state.agent_x < MAZE_SIZE &&
            s_q_state.agent_y < MAZE_SIZE)
        {
            s_infer_visited[s_q_state.agent_y][s_q_state.agent_x] = 1U;
            ball_ui_set_cell_locked(s_q_state.agent_x,
                                    s_q_state.agent_y);
        }
        else if (mode_changed)
        {
            ball_ui_set_cell_locked(s_start_x, s_start_y);
        }
        set_inference_text(&s_q_state);
        if (s_q_state.inference_complete)
        {
            show_inference_complete(&s_q_state);
        }
        else
        {
            hide_complete_panel();
        }
    }
    else if (s_mode_state.mode == APP_MODE_DQN_TRAIN)
    {
        ball_ui_set_cell_locked(s_start_x, s_start_y);
        set_dqn_training_text(&s_dqn_state);
        hide_complete_panel();
    }
    else if (s_mode_state.mode == APP_MODE_DQN_INFER)
    {
        if (s_dqn_state.phase == DQN_TRAINING_UI_INFERENCE &&
            old_dqn_phase != DQN_TRAINING_UI_INFERENCE)
        {
            memset(s_dqn_infer_visited, 0, sizeof(s_dqn_infer_visited));
            s_dqn_infer_visited[s_start_y][s_start_x] = 1U;
        }
        if (s_dqn_state.agent_x < MAZE_SIZE &&
            s_dqn_state.agent_y < MAZE_SIZE)
        {
            s_dqn_infer_visited[s_dqn_state.agent_y]
                               [s_dqn_state.agent_x] = 1U;
            ball_ui_set_cell_locked(s_dqn_state.agent_x,
                                    s_dqn_state.agent_y);
        }
        else if (mode_changed)
        {
            ball_ui_set_cell_locked(s_start_x, s_start_y);
        }
        set_dqn_inference_text(&s_dqn_state);
        if (s_dqn_state.inference_complete)
        {
            show_dqn_inference_complete(&s_dqn_state);
        }
        else
        {
            hide_complete_panel();
        }
    }
    else
    {
        ball_ui_set_cell_locked(s_start_x, s_start_y);
        set_compare_text(&s_q_state, &s_dqn_state, &s_random_state);
        hide_complete_panel();
    }
    lv_obj_invalidate(s_table);
}

static void ui_timer_cb(lv_timer_t *timer)
{
    app_mode_state_t mode_state;
    q_training_ui_state_t q_state;
    dqn_training_ui_state_t dqn_state;
    random_baseline_ui_state_t random_state;
    q_training_ui_phase_t old_q_phase;
    dqn_training_ui_phase_t old_dqn_phase;
    rt_bool_t mode_changed;
    rt_bool_t q_changed;
    rt_bool_t dqn_changed;
    rt_bool_t random_changed;

    (void)timer;

    if (!s_ready || app_mode_get_state(&mode_state) != RT_EOK ||
        q_training_get_ui_state(&q_state) != RT_EOK ||
        dqn_training_get_ui_state(&dqn_state) != RT_EOK ||
        random_baseline_get_ui_state(&random_state) != RT_EOK)
    {
        return;
    }

    mode_changed = mode_state.revision != s_mode_state.revision;
    q_changed = q_state.revision != s_q_state.revision;
    dqn_changed = dqn_state.revision != s_dqn_state.revision;
    random_changed = random_state.revision != s_random_state.revision;
    if (!mode_changed && !q_changed && !dqn_changed && !random_changed)
    {
        return;
    }

    old_q_phase = s_q_state.phase;
    old_dqn_phase = s_dqn_state.phase;
    s_mode_state = mode_state;
    s_q_state = q_state;
    s_dqn_state = dqn_state;
    s_random_state = random_state;
    render_current_mode(mode_changed, old_q_phase, old_dqn_phase);
}

static void maze_draw_event(lv_event_t *event)
{
    lv_draw_task_t *task = lv_event_get_draw_task(event);
    lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(task);
    lv_draw_fill_dsc_t *fill;
    lv_draw_label_dsc_t *label;
    rt_uint32_t row;
    rt_uint32_t col;
    maze_env_cell_t cell;
    lv_color_t bg;
    lv_color_t fg = lv_color_hex(0x202020);

    if (base == RT_NULL || base->part != LV_PART_ITEMS)
    {
        return;
    }

    row = base->id1;
    col = base->id2;
    if (row >= MAZE_SIZE || col >= MAZE_SIZE)
    {
        return;
    }

    cell = maze_env_cell_at(0, col, row);
    if (cell == MAZE_ENV_CELL_WALL)
    {
        bg = lv_color_hex(0x263238);
        fg = lv_color_hex(0xFFFFFF);
    }
    else if (cell == MAZE_ENV_CELL_START)
    {
        bg = lv_color_hex(0x81C784);
    }
    else if (cell == MAZE_ENV_CELL_GOAL)
    {
        bg = lv_color_hex(0xFFD54F);
    }
    else if ((s_mode_state.mode == APP_MODE_DEMO &&
              s_demo_visited[row][col]) ||
             (s_mode_state.mode == APP_MODE_INFER &&
              s_infer_visited[row][col]) ||
             (s_mode_state.mode == APP_MODE_DQN_INFER &&
              s_dqn_infer_visited[row][col]))
    {
        bg = lv_color_hex(0xBBDEFB);
    }
    else
    {
        bg = lv_color_hex(0xFAFAFA);
    }

    fill = lv_draw_task_get_fill_dsc(task);
    if (fill != RT_NULL)
    {
        fill->color = bg;
        fill->opa = LV_OPA_COVER;
    }

    label = lv_draw_task_get_label_dsc(task);
    if (label != RT_NULL)
    {
        label->color = fg;
        label->align = LV_TEXT_ALIGN_CENTER;
    }
}

static void set_reward_text(lv_obj_t *label, const maze_ui_state_t *state)
{
    int last_abs = state->last_reward_tenths < 0 ? -state->last_reward_tenths : state->last_reward_tenths;
    int total_abs = state->total_reward_tenths < 0 ? -state->total_reward_tenths : state->total_reward_tenths;

    lv_label_set_text_fmt(label,
                          "STEP %u  TOTAL %u  HIT %u\nREWARD %s%d.%d  SUM %s%d.%d",
                          state->step_count, state->total_steps, state->collision_count,
                          state->last_reward_tenths < 0 ? "-" : "",
                          last_abs / 10, last_abs % 10,
                          state->total_reward_tenths < 0 ? "-" : "",
                          total_abs / 10, total_abs % 10);
}

void maze_ui_init(void)
{
    rt_uint32_t row;
    rt_uint32_t col;
    maze_env_t env;

    if (maze_env_init(&env, 0U) == RT_EOK)
    {
        s_start_x = env.start_x;
        s_start_y = env.start_y;
    }

    s_status_label = lv_label_create(lv_screen_active());
    lv_obj_set_pos(s_status_label, MAZE_ORIGIN_X, 32);
    lv_label_set_text_fmt(s_status_label, "MAP 0  READY  DEMO N=%u",
                          s_demo_count);

    s_table = lv_table_create(lv_screen_active());
    lv_table_set_row_count(s_table, MAZE_SIZE);
    lv_table_set_column_count(s_table, MAZE_SIZE);
    for (col = 0; col < MAZE_SIZE; col++)
    {
        lv_table_set_column_width(s_table, col, MAZE_CELL_PX);
    }
    for (row = 0; row < MAZE_SIZE; row++)
    {
        for (col = 0; col < MAZE_SIZE; col++)
        {
            const char *text = "";
            maze_env_cell_t cell = maze_env_cell_at(0, col, row);
            if (cell == MAZE_ENV_CELL_START) text = "S";
            if (cell == MAZE_ENV_CELL_GOAL) text = "G";
            lv_table_set_cell_value(s_table, row, col, text);
        }
    }
    lv_obj_set_size(s_table, MAZE_SIZE * MAZE_CELL_PX, MAZE_SIZE * MAZE_CELL_PX);
    lv_obj_set_pos(s_table, MAZE_ORIGIN_X, MAZE_ORIGIN_Y);
    lv_obj_set_style_height(s_table, MAZE_CELL_PX, LV_PART_ITEMS);
    lv_obj_set_style_min_height(s_table, MAZE_CELL_PX, LV_PART_ITEMS);
    lv_obj_set_style_max_height(s_table, MAZE_CELL_PX, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(s_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_table, lv_color_hex(0x607D8B), LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_table, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_table, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_table, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_table, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style(s_table, RT_NULL, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_table, maze_draw_event, LV_EVENT_DRAW_TASK_ADDED, RT_NULL);
    lv_obj_add_flag(s_table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

    s_stats_label = lv_label_create(lv_screen_active());
    lv_obj_set_pos(s_stats_label, MAZE_ORIGIN_X, 528);
    lv_label_set_text(s_stats_label, "STEP 0  TOTAL 0  HIT 0\nREWARD 0.0  SUM 0.0");

    memset(&s_state, 0, sizeof(s_state));
    memset(s_demo_visited, 0, sizeof(s_demo_visited));
    memset(s_infer_visited, 0, sizeof(s_infer_visited));
    memset(s_dqn_infer_visited, 0, sizeof(s_dqn_infer_visited));
    s_demo_visited[s_start_y][s_start_x] = 1U;
    s_infer_visited[s_start_y][s_start_x] = 1U;
    s_dqn_infer_visited[s_start_y][s_start_x] = 1U;
    ball_ui_init(s_table, MAZE_CELL_PX);
    ball_ui_set_cell_locked(s_start_x, s_start_y);

    s_complete_panel = lv_obj_create(lv_screen_active());
    lv_obj_remove_flag(s_complete_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_complete_panel, 360, 140);
    lv_obj_align(s_complete_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_complete_panel, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_style_bg_opa(s_complete_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_complete_panel, 0, 0);
    lv_obj_add_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);

    s_complete_label = lv_label_create(s_complete_panel);
    lv_label_set_text(s_complete_label, "GOAL REACHED\nPRESS SW2 TO RESET");
    lv_obj_set_style_text_color(s_complete_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(s_complete_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_complete_label);
    s_ready = RT_TRUE;
    lv_timer_create(ui_timer_cb, UI_POLL_MS, RT_NULL);
}

void maze_ui_update(const maze_ui_state_t *state)
{
    RT_ASSERT(state != RT_NULL);

    if (!s_ready || state->map_id != 0 ||
        state->agent_x >= MAZE_SIZE || state->agent_y >= MAZE_SIZE ||
        state->revision == s_state.revision)
    {
        return;
    }

    lv_lock();
    if (state->total_steps == 0)
    {
        memset(s_demo_visited, 0, sizeof(s_demo_visited));
    }
    s_demo_visited[state->agent_y][state->agent_x] = 1U;
    s_state = *state;
    if (s_mode_state.mode == APP_MODE_DEMO)
    {
        render_physical_state();
        lv_obj_invalidate(s_table);
    }
    lv_unlock();
}

void maze_ui_set_demo_count(rt_uint16_t count)
{
    if (!s_ready)
    {
        s_demo_count = count;
        return;
    }

    lv_lock();
    s_demo_count = count;
    if (s_mode_state.mode == APP_MODE_DEMO)
    {
        set_status_text();
    }
    else if (s_mode_state.mode == APP_MODE_TRAIN)
    {
        set_training_text(&s_q_state);
    }
    else if (s_mode_state.mode == APP_MODE_INFER)
    {
        set_inference_text(&s_q_state);
    }
    else if (s_mode_state.mode == APP_MODE_DQN_TRAIN)
    {
        set_dqn_training_text(&s_dqn_state);
    }
    else if (s_mode_state.mode == APP_MODE_DQN_INFER)
    {
        set_dqn_inference_text(&s_dqn_state);
    }
    else
    {
        set_compare_text(&s_q_state, &s_dqn_state, &s_random_state);
    }
    lv_unlock();
}

rt_err_t maze_ui_reload_map(void)
{
    maze_env_t env;
    app_mode_state_t mode_state;
    q_training_ui_state_t q_state;
    dqn_training_ui_state_t dqn_state;
    random_baseline_ui_state_t random_state;
    rt_uint32_t row;
    rt_uint32_t col;

    if (!s_ready)
    {
        return -RT_EBUSY;
    }
    if (maze_env_init(&env, 0U) != RT_EOK ||
        app_mode_get_state(&mode_state) != RT_EOK ||
        q_training_get_ui_state(&q_state) != RT_EOK ||
        dqn_training_get_ui_state(&dqn_state) != RT_EOK ||
        random_baseline_get_ui_state(&random_state) != RT_EOK)
    {
        return -RT_ERROR;
    }

    lv_lock();
    s_start_x = env.start_x;
    s_start_y = env.start_y;
    for (row = 0U; row < MAZE_SIZE; row++)
    {
        for (col = 0U; col < MAZE_SIZE; col++)
        {
            const char *text = "";
            maze_env_cell_t cell = maze_env_cell_at(0U, col, row);

            if (cell == MAZE_ENV_CELL_START)
            {
                text = "S";
            }
            else if (cell == MAZE_ENV_CELL_GOAL)
            {
                text = "G";
            }
            lv_table_set_cell_value(s_table, row, col, text);
        }
    }

    memset(&s_state, 0, sizeof(s_state));
    s_state.agent_x = s_start_x;
    s_state.agent_y = s_start_y;
    memset(s_demo_visited, 0, sizeof(s_demo_visited));
    memset(s_infer_visited, 0, sizeof(s_infer_visited));
    memset(s_dqn_infer_visited, 0, sizeof(s_dqn_infer_visited));
    s_demo_visited[s_start_y][s_start_x] = 1U;
    s_infer_visited[s_start_y][s_start_x] = 1U;
    s_dqn_infer_visited[s_start_y][s_start_x] = 1U;
    s_mode_state = mode_state;
    s_q_state = q_state;
    s_dqn_state = dqn_state;
    s_random_state = random_state;
    render_current_mode(RT_TRUE, Q_TRAINING_UI_NONE,
                        DQN_TRAINING_UI_NONE);
    lv_unlock();
    return RT_EOK;
}
