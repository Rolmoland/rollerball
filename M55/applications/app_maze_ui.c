#include "lvgl.h"
#include <string.h>
#include "app_ball_ui.h"
#include "app_maze_ui.h"

#define MAZE_SIZE        10
#define MAZE_CELL_PX     40
#define MAZE_ORIGIN_X    56
#define MAZE_ORIGIN_Y    96

#define CELL_EMPTY       0
#define CELL_WALL        1
#define CELL_START       2
#define CELL_GOAL        3

static const rt_uint8_t s_map_0[MAZE_SIZE][MAZE_SIZE] =
{
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 0, 1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 0},
};

static lv_obj_t *s_table;
static lv_obj_t *s_status_label;
static lv_obj_t *s_stats_label;
static lv_obj_t *s_complete_panel;
static maze_ui_state_t s_state;
static rt_uint8_t s_visited[MAZE_SIZE][MAZE_SIZE];
static rt_bool_t s_ready = RT_FALSE;

static void maze_draw_event(lv_event_t *event)
{
    lv_draw_task_t *task = lv_event_get_draw_task(event);
    lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(task);
    lv_draw_fill_dsc_t *fill;
    lv_draw_label_dsc_t *label;
    rt_uint32_t row;
    rt_uint32_t col;
    rt_uint8_t cell;
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

    cell = s_map_0[row][col];
    if (cell == CELL_WALL)
    {
        bg = lv_color_hex(0x263238);
        fg = lv_color_hex(0xFFFFFF);
    }
    else if (cell == CELL_START)
    {
        bg = lv_color_hex(0x81C784);
    }
    else if (cell == CELL_GOAL)
    {
        bg = lv_color_hex(0xFFD54F);
    }
    else if (s_visited[row][col])
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

    s_status_label = lv_label_create(lv_screen_active());
    lv_obj_set_pos(s_status_label, MAZE_ORIGIN_X, 32);
    lv_label_set_text(s_status_label, "MAP 0  READY");

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
            if (s_map_0[row][col] == CELL_START) text = "S";
            if (s_map_0[row][col] == CELL_GOAL) text = "G";
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
    memset(s_visited, 0, sizeof(s_visited));
    s_visited[0][0] = 1;
    ball_ui_init(s_table, MAZE_CELL_PX);
    ball_ui_set_cell_locked(0, 0);

    s_complete_panel = lv_obj_create(lv_screen_active());
    lv_obj_remove_flag(s_complete_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_complete_panel, 360, 140);
    lv_obj_align(s_complete_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_complete_panel, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_style_bg_opa(s_complete_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_complete_panel, 0, 0);
    lv_obj_add_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *complete_label = lv_label_create(s_complete_panel);
    lv_label_set_text(complete_label, "GOAL REACHED\nPRESS SW2 TO RESET");
    lv_obj_set_style_text_color(complete_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(complete_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(complete_label);
    s_ready = RT_TRUE;
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
        memset(s_visited, 0, sizeof(s_visited));
    }
    s_visited[state->agent_y][state->agent_x] = 1;
    s_state = *state;

    ball_ui_set_cell_locked(state->agent_x, state->agent_y);
    lv_label_set_text_fmt(s_status_label, "MAP %u  %s",
                          state->map_id, state->done ? "COMPLETE" : "RUNNING");
    set_reward_text(s_stats_label, state);
    if (state->done)
    {
        lv_obj_remove_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_complete_panel);
    }
    else
    {
        lv_obj_add_flag(s_complete_panel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_invalidate(s_table);
    lv_unlock();
}
