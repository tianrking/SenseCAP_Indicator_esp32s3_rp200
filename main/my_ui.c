#include "my_ui.h"
#include "esp_log.h"
#include "esp_random.h" // For esp_random()
#include <string.h>     // For memset, memcpy
#include <stdio.h>      // For sprintf

static const char *TAG = "my_ui_tetris_480_LAYOUT_FIX_v83";

// --- Game Configuration for 480x480 screen ---
#define BOARD_COLS 10
#define BOARD_ROWS 20 // Visible rows
#define BOARD_BUFFER_TOP 4 // Hidden rows at the top for piece spawning
#define BOARD_ROWS_TOTAL (BOARD_ROWS + BOARD_BUFFER_TOP)

// MODIFIED: BLOCK_SIZE to prevent overlap
#define BLOCK_SIZE 14
#define GAME_AREA_WIDTH (BOARD_COLS * BLOCK_SIZE)   // 10 * 14 = 140
#define GAME_AREA_HEIGHT (BOARD_ROWS * BLOCK_SIZE) // 20 * 14 = 280

#define GAME_TICK_MS_INITIAL 500
#define BOTTOM_PANEL_HEIGHT 70 // Height for the bottom control panel
#define TOP_RIGHT_RESTART_BTN_SIZE 40


// --- Piece Type IDs ---
#define PIECE_TYPE_NONE 0
#define PIECE_TYPE_I 1
#define PIECE_TYPE_O 2
#define PIECE_TYPE_T 3
#define PIECE_TYPE_L 4
#define PIECE_TYPE_J 5
#define PIECE_TYPE_S 6
#define PIECE_TYPE_Z 7
#define NUM_PIECE_TYPES 7

// --- Game Colors ---
#define COLOR_EMPTY_CELL lv_color_hex(0x101010)
#define COLOR_GRID_LINE lv_color_hex(0x303030)
#define SCREEN_BG_COLOR lv_color_hex(0x000020)
#define PSEUDO_BUTTON_COLOR lv_color_hex(0x303070)
#define PSEUDO_BUTTON_PRESSED_COLOR lv_color_hex(0x404090)


// --- Game Control Actions (for internal logic) ---
typedef enum {
    ACTION_MOVE_LEFT, ACTION_MOVE_RIGHT, ACTION_ROTATE_CW,
    ACTION_SOFT_DROP, ACTION_HARD_DROP
} game_control_action_t;


// --- Game State Variables ---
static lv_obj_t *screen;
static lv_obj_t *game_area_parent; // The container for the visual grid cells
static lv_obj_t *board_cells[BOARD_ROWS][BOARD_COLS]; // UI objects for visible cells
static lv_color_t displayed_cell_colors[BOARD_ROWS][BOARD_COLS]; // Cache for cell colors

static lv_obj_t *score_label;
static lv_obj_t *level_label;
static lv_obj_t *username_label;
static lv_obj_t *game_over_label_obj;

// Specific control objects
static lv_obj_t *bottom_button_panel;
static lv_obj_t *left_button_pseudo;
static lv_obj_t *right_button_pseudo;
static lv_obj_t *hard_drop_button_pseudo;
static lv_obj_t *rotate_button_pseudo;
static lv_obj_t *top_restart_button_pseudo;

static lv_timer_t *game_timer;

static uint8_t playfield[BOARD_ROWS_TOTAL][BOARD_COLS]; // Logical game board
static int current_score = 0;
static int current_level = 1;
static bool game_is_active = false;
static bool game_is_over = false;

const int8_t TETROMINO_SHAPES[NUM_PIECE_TYPES + 1][4][4][2] = {
    {{{0}}}, // PIECE_TYPE_NONE
    {{{0, -1}, {0, 0}, {0, 1}, {0, 2}}, {{-1, 0}, {0, 0}, {1, 0}, {2, 0}}, {{0, -1}, {0, 0}, {0, 1}, {0, 2}}, {{-1, 0}, {0, 0}, {1, 0}, {2, 0}}}, // I
    {{{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {{0, 0}, {0, 1}, {1, 0}, {1, 1}}}, // O
    {{{0, -1}, {0, 0}, {0, 1}, {1, 0}}, {{-1, 0}, {0, 0}, {1, 0}, {0, -1}}, {{-1, 0}, {0, -1}, {0, 0}, {0, 1}}, {{-1, 0}, {0, 0}, {1, 0}, {0, 1}}}, // T
    {{{0, -1}, {0, 0}, {0, 1}, {1, -1}}, {{-1, 0}, {0, 0}, {1, 0}, {1, 1}}, {{0, 1}, {0, 0}, {0, -1}, {-1, 1}}, {{1, 0}, {0, 0}, {-1, 0}, {-1, -1}}}, // L
    {{{0, -1}, {0, 0}, {0, 1}, {1, 1}}, {{-1, 1}, {-1, 0}, {0, 0}, {1, 0}}, {{-1, -1}, {0, -1}, {0, 0}, {0, 1}}, {{1, -1}, {1, 0}, {0, 0}, {-1, 0}}}, // J
    {{{0, 0}, {0, 1}, {1, -1}, {1, 0}}, {{-1, 0}, {0, 0}, {0, 1}, {1, 1}}, {{0, 0}, {0, 1}, {1, -1}, {1, 0}}, {{-1, 0}, {0, 0}, {0, 1}, {1, 1}}}, // S
    {{{0, -1}, {0, 0}, {1, 0}, {1, 1}}, {{-1, 1}, {0, 1}, {0, 0}, {1, 0}}, {{0, -1}, {0, 0}, {1, 0}, {1, 1}}, {{-1, 1}, {0, 1}, {0, 0}, {1, 0}}}  // Z
};
const uint8_t PIECE_ROTATION_COUNTS[NUM_PIECE_TYPES + 1] = {0, 2, 1, 4, 4, 4, 2, 2};

static struct {
    uint8_t type_id;
    uint8_t rotation;
    lv_color_t color;
    int x_col;
    int y_row;
} active_piece;

// --- Forward Declarations ---
static lv_color_t get_color_for_piece_type(uint8_t piece_type);
static void update_board_display(void);
static void spawn_new_piece(void);
static bool check_collision(int test_x_col, int test_y_row, uint8_t piece_type, uint8_t rotation_idx);
static void lock_piece_to_playfield(void);
static int clear_completed_lines(void);
static void game_timer_callback(lv_timer_t *timer);
static void control_action_event_cb(lv_event_t *e);
static void top_restart_button_event_cb(lv_event_t *e);
static void game_area_restart_cb(lv_event_t *e);
static void handle_game_action(game_control_action_t action);
static void start_tetris_game(void);
static void trigger_game_over(void);

static lv_color_t get_color_for_piece_type(uint8_t piece_type) {
    switch (piece_type) {
        case PIECE_TYPE_I: return lv_color_hex(0x00FFFF);
        case PIECE_TYPE_O: return lv_color_hex(0xFFFF00);
        case PIECE_TYPE_T: return lv_color_hex(0xAA00FF);
        case PIECE_TYPE_L: return lv_color_hex(0xFFAA00);
        case PIECE_TYPE_J: return lv_color_hex(0x0000FF);
        case PIECE_TYPE_S: return lv_color_hex(0x00FF00);
        case PIECE_TYPE_Z: return lv_color_hex(0xFF0000);
        default: return COLOR_EMPTY_CELL;
    }
}

static void update_board_display(void) {
    if (!game_area_parent) return;
    lv_color_t target_cell_colors[BOARD_ROWS][BOARD_COLS];
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            uint8_t piece_type_in_cell = playfield[r + BOARD_BUFFER_TOP][c];
            target_cell_colors[r][c] = get_color_for_piece_type(piece_type_in_cell);
        }
    }
    if (game_is_active && !game_is_over && active_piece.type_id != PIECE_TYPE_NONE) {
        const int8_t(*coords_ptr)[4][2] = &TETROMINO_SHAPES[active_piece.type_id][active_piece.rotation];
        for (int i = 0; i < 4; ++i) {
            int block_playfield_col = active_piece.x_col + (*coords_ptr)[i][1];
            int block_playfield_row = active_piece.y_row + (*coords_ptr)[i][0];
            int visible_row = block_playfield_row - BOARD_BUFFER_TOP;
            if (visible_row >= 0 && visible_row < BOARD_ROWS &&
                block_playfield_col >= 0 && block_playfield_col < BOARD_COLS) {
                target_cell_colors[visible_row][block_playfield_col] = active_piece.color;
            }
        }
    }
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            if (board_cells[r][c]) {
                if (displayed_cell_colors[r][c].full != target_cell_colors[r][c].full) {
                    lv_obj_set_style_bg_color(board_cells[r][c], target_cell_colors[r][c], LV_STATE_DEFAULT);
                    displayed_cell_colors[r][c] = target_cell_colors[r][c];
                }
            }
        }
    }
}

static void spawn_new_piece(void) {
    active_piece.type_id = (esp_random() % NUM_PIECE_TYPES) + 1;
    active_piece.rotation = 0;
    active_piece.color = get_color_for_piece_type(active_piece.type_id);
    active_piece.x_col = BOARD_COLS / 2;
    if (active_piece.type_id == PIECE_TYPE_I || active_piece.type_id == PIECE_TYPE_O) {
         active_piece.x_col -=1;
    }
    const int8_t(*coords_ptr_spawn)[4][2] = &TETROMINO_SHAPES[active_piece.type_id][active_piece.rotation];
    int min_y_offset_in_piece = 0;
    for (int i = 0; i < 4; ++i) {
        if ((*coords_ptr_spawn)[i][0] < min_y_offset_in_piece) {
            min_y_offset_in_piece = (*coords_ptr_spawn)[i][0];
        }
    }
    active_piece.y_row = -min_y_offset_in_piece;
    if (check_collision(active_piece.x_col, active_piece.y_row, active_piece.type_id, active_piece.rotation)) {
        bool can_move_up = false;
        for(int i = 0; i < BOARD_BUFFER_TOP / 2; ++i) {
             if(active_piece.y_row > 0 && !check_collision(active_piece.x_col, active_piece.y_row - 1, active_piece.type_id, active_piece.rotation)) {
                 active_piece.y_row--;
                 can_move_up = true;
             } else { break; }
        }
        if ((!can_move_up || check_collision(active_piece.x_col, active_piece.y_row, active_piece.type_id, active_piece.rotation))) {
             trigger_game_over();
        }
    }
}

static bool check_collision(int test_x_col, int test_y_row, uint8_t piece_type, uint8_t rotation_idx) {
    if (piece_type == PIECE_TYPE_NONE || piece_type > NUM_PIECE_TYPES) return true;
    if (rotation_idx >= PIECE_ROTATION_COUNTS[piece_type]) rotation_idx = 0;
    const int8_t(*coords_ptr)[4][2] = &TETROMINO_SHAPES[piece_type][rotation_idx];
    for (int i = 0; i < 4; ++i) {
        int block_abs_col = test_x_col + (*coords_ptr)[i][1];
        int block_abs_row = test_y_row + (*coords_ptr)[i][0];
        if (block_abs_col < 0 || block_abs_col >= BOARD_COLS || block_abs_row >= BOARD_ROWS_TOTAL) return true;
        if (block_abs_row < 0) continue;
        if (playfield[block_abs_row][block_abs_col] != PIECE_TYPE_NONE) return true;
    }
    return false;
}

static void lock_piece_to_playfield(void) {
    if (active_piece.type_id == PIECE_TYPE_NONE) return;
    const int8_t(*coords_ptr)[4][2] = &TETROMINO_SHAPES[active_piece.type_id][active_piece.rotation];
    for (int i = 0; i < 4; ++i) {
        int block_abs_col = active_piece.x_col + (*coords_ptr)[i][1];
        int block_abs_row = active_piece.y_row + (*coords_ptr)[i][0];
        if (block_abs_row < 0) { trigger_game_over(); return; }
        if (block_abs_row < BOARD_ROWS_TOTAL && block_abs_col >= 0 && block_abs_col < BOARD_COLS) {
            playfield[block_abs_row][block_abs_col] = active_piece.type_id;
        } else { trigger_game_over(); return; }
    }
    int lines_cleared = clear_completed_lines();
    if (lines_cleared > 0) {
        current_score += lines_cleared * lines_cleared * 100;
        if (score_label) lv_label_set_text_fmt(score_label, "Score: %d", current_score);
    }
    if (!game_is_over) spawn_new_piece();
}

static int clear_completed_lines(void) {
    int lines_cleared_count = 0;
    for (int r = BOARD_ROWS_TOTAL - 1; r >= 0; --r) {
        bool line_is_full = true;
        for (int c = 0; c < BOARD_COLS; ++c) {
            if (playfield[r][c] == PIECE_TYPE_NONE) { line_is_full = false; break; }
        }
        if (line_is_full) {
            lines_cleared_count++;
            for (int row_to_move = r; row_to_move > 0; --row_to_move) {
                memcpy(playfield[row_to_move], playfield[row_to_move - 1], BOARD_COLS * sizeof(uint8_t));
            }
            memset(playfield[0], PIECE_TYPE_NONE, BOARD_COLS * sizeof(uint8_t));
            r++;
        }
    }
    return lines_cleared_count;
}

static void game_timer_callback(lv_timer_t *timer) {
    if (!game_is_active || game_is_over) return;
    if (!check_collision(active_piece.x_col, active_piece.y_row + 1, active_piece.type_id, active_piece.rotation)) {
        active_piece.y_row++;
    } else {
        lock_piece_to_playfield();
    }
    if (!game_is_over) update_board_display();
}

static void handle_game_action(game_control_action_t action) {
    if (!game_is_active || game_is_over || active_piece.type_id == PIECE_TYPE_NONE) return;
    int try_x = active_piece.x_col;
    int try_y = active_piece.y_row;
    uint8_t try_rot = active_piece.rotation;
    bool piece_state_changed = false;
    ESP_LOGI(TAG, "Handling action: %d", action);
    switch (action) {
        case ACTION_MOVE_LEFT:
            try_x--;
            if (!check_collision(try_x, try_y, active_piece.type_id, try_rot)) {
                active_piece.x_col = try_x; piece_state_changed = true;
            }
            break;
        case ACTION_MOVE_RIGHT:
            try_x++;
            if (!check_collision(try_x, try_y, active_piece.type_id, try_rot)) {
                active_piece.x_col = try_x; piece_state_changed = true;
            }
            break;
        case ACTION_ROTATE_CW:
            try_rot = (active_piece.rotation + 1) % PIECE_ROTATION_COUNTS[active_piece.type_id];
            if (!check_collision(try_x, try_y, active_piece.type_id, try_rot)) {
                active_piece.rotation = try_rot; piece_state_changed = true;
            } else { 
                if (!check_collision(try_x - 1, try_y, active_piece.type_id, try_rot)) {
                    active_piece.x_col = try_x - 1; active_piece.rotation = try_rot; piece_state_changed = true;
                } else if (!check_collision(try_x + 1, try_y, active_piece.type_id, try_rot)) {
                    active_piece.x_col = try_x + 1; active_piece.rotation = try_rot; piece_state_changed = true;
                }
            }
            break;
        case ACTION_HARD_DROP:
            while (!check_collision(active_piece.x_col, active_piece.y_row + 1, active_piece.type_id, active_piece.rotation)) {
                active_piece.y_row++;
                piece_state_changed = true;
            }
            lock_piece_to_playfield();
            piece_state_changed = true;
            break;
        default: break;
    }
    if (piece_state_changed && !game_is_over) update_board_display();
}

// Event callback for the pseudo-buttons (Left, Right, Hard Drop, Rotate)
static void control_action_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    game_control_action_t action = (game_control_action_t)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Control action event, code: %d, action: %d", code, action);
    if (code == LV_EVENT_CLICKED) {
        if (game_is_active && !game_is_over) {
            handle_game_action(action);
        } else {
            ESP_LOGI(TAG, "Game not active or over, control click ignored.");
        }
    }
}

// Event callback for the new top-right Restart pseudo-button
static void top_restart_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    ESP_LOGI(TAG, "Top Restart pseudo-button event, code: %d", code);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Top Restart button clicked, restarting game.");
        start_tetris_game();
    }
}

// Event callback for clicking game_area_parent (for restart when game is over)
static void game_area_restart_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    ESP_LOGI(TAG, "game_area_restart_cb triggered, code: %d", code);
    if (code == LV_EVENT_CLICKED && game_is_over) {
        ESP_LOGI(TAG, "Game is over, restarting game on click of game area.");
        start_tetris_game();
    }
}

static void start_tetris_game(void) {
    ESP_LOGI(TAG, "Starting Tetris game (%s)...", TAG);
    memset(playfield, PIECE_TYPE_NONE, sizeof(playfield));
    lv_color_t empty_color_val = COLOR_EMPTY_CELL;
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            displayed_cell_colors[r][c].full = empty_color_val.full + 1;
            if (board_cells[r][c]) {
                 lv_obj_set_style_bg_color(board_cells[r][c], empty_color_val, LV_STATE_DEFAULT);
                 displayed_cell_colors[r][c] = empty_color_val;
            }
        }
    }
    current_score = 0; current_level = 1; game_is_over = false; game_is_active = true;
    active_piece.type_id = PIECE_TYPE_NONE;
    if (score_label) lv_label_set_text_fmt(score_label, "Score: %d", current_score);
    if (level_label) lv_label_set_text_fmt(level_label, "Level: %d", current_level);
    if (game_over_label_obj) lv_obj_add_flag(game_over_label_obj, LV_OBJ_FLAG_HIDDEN);

    // Show control objects
    if(left_button_pseudo) lv_obj_clear_flag(left_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    if(right_button_pseudo) lv_obj_clear_flag(right_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    if(hard_drop_button_pseudo) lv_obj_clear_flag(hard_drop_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    if(rotate_button_pseudo) lv_obj_clear_flag(rotate_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    if(top_restart_button_pseudo) lv_obj_clear_flag(top_restart_button_pseudo, LV_OBJ_FLAG_HIDDEN);

    spawn_new_piece(); update_board_display();
    uint32_t timer_period = GAME_TICK_MS_INITIAL;
    if (game_timer) {
        lv_timer_set_period(game_timer, timer_period); lv_timer_reset(game_timer); lv_timer_resume(game_timer);
    } else {
        game_timer = lv_timer_create(game_timer_callback, timer_period, NULL);
    }
}

static void trigger_game_over(void) {
    if (game_is_over) return;
    ESP_LOGI(TAG, "Game Over! Final Score: %d", current_score);
    game_is_active = false; game_is_over = true;
    if (game_timer) lv_timer_pause(game_timer);
    if (game_over_label_obj) {
        lv_label_set_text(game_over_label_obj, "GAME OVER\nClick Board to Restart");
        lv_obj_clear_flag(game_over_label_obj, LV_OBJ_FLAG_HIDDEN);
    }
    // Hide bottom control objects
    if(left_button_pseudo) lv_obj_add_flag(left_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    if(right_button_pseudo) lv_obj_add_flag(right_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    if(hard_drop_button_pseudo) lv_obj_add_flag(hard_drop_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    if(rotate_button_pseudo) lv_obj_add_flag(rotate_button_pseudo, LV_OBJ_FLAG_HIDDEN);
    // Top restart button remains visible
}

// --- UI Initialization ---
void my_ui_init(void) {
    ESP_LOGI(TAG, "Initializing Tetris UI (%s) for 480x480 (Final Touch Controls - Layout Fix)", TAG);
    ESP_LOGW(TAG, "Ensure LVGL input device (touchscreen) is correctly initialized and registered.");

    screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, SCREEN_BG_COLOR, LV_STATE_DEFAULT);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    username_label = lv_label_create(screen);
    lv_label_set_text(username_label, "w0x7ce (Tetris)");
    lv_obj_set_style_text_color(username_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(username_label, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_align(username_label, LV_ALIGN_TOP_LEFT, 10, 10);

    score_label = lv_label_create(screen);
    lv_obj_set_style_text_color(score_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_label_set_text_fmt(score_label, "Score: %d", 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_RIGHT, -10, 10);

    level_label = lv_label_create(screen);
    lv_obj_set_style_text_color(level_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(level_label, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_label_set_text_fmt(level_label, "Level: %d", 1);
    lv_obj_align_to(level_label, score_label, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);

    // --- New Top-Right Restart Button ---
    top_restart_button_pseudo = lv_obj_create(screen);
    lv_obj_set_size(top_restart_button_pseudo, TOP_RIGHT_RESTART_BTN_SIZE + 30, TOP_RIGHT_RESTART_BTN_SIZE); // Wider for "Restart" text
    lv_obj_align_to(top_restart_button_pseudo, level_label, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);
    lv_obj_set_style_bg_color(top_restart_button_pseudo, PSEUDO_BUTTON_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(top_restart_button_pseudo, PSEUDO_BUTTON_PRESSED_COLOR, LV_STATE_PRESSED);
    lv_obj_set_style_radius(top_restart_button_pseudo, 5, LV_STATE_DEFAULT);
    lv_obj_add_flag(top_restart_button_pseudo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(top_restart_button_pseudo, top_restart_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(top_restart_button_pseudo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(top_restart_button_pseudo, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *lbl_top_restart = lv_label_create(top_restart_button_pseudo);
    lv_label_set_text(lbl_top_restart, "Restart"); 
    lv_obj_set_style_text_font(lbl_top_restart, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_center(lbl_top_restart);
    ESP_LOGI(TAG, "Top-right restart pseudo-button created.");


    // --- Game Area Parent (visual grid container) ---
    game_area_parent = lv_obj_create(screen);
    lv_obj_set_size(game_area_parent, GAME_AREA_WIDTH, GAME_AREA_HEIGHT); // 140x280
    lv_obj_set_style_bg_color(game_area_parent, COLOR_EMPTY_CELL, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(game_area_parent, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(game_area_parent, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(game_area_parent, lv_color_hex(0x606070), LV_STATE_DEFAULT);
    // MODIFIED: Y alignment of game_area_parent
    lv_obj_align(game_area_parent, LV_ALIGN_TOP_MID, 0, 110); // y from 110 to 110+280=390

    lv_obj_clear_flag(game_area_parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(game_area_parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(game_area_parent, game_area_restart_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(game_area_parent, LV_OBJ_FLAG_CLICKABLE);

    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            board_cells[r][c] = lv_obj_create(game_area_parent);
            lv_obj_set_size(board_cells[r][c], BLOCK_SIZE, BLOCK_SIZE);
            lv_obj_set_pos(board_cells[r][c], c * BLOCK_SIZE, r * BLOCK_SIZE);
            lv_obj_set_style_radius(board_cells[r][c], 0, LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(board_cells[r][c], 1, LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(board_cells[r][c], COLOR_GRID_LINE, LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(board_cells[r][c], COLOR_EMPTY_CELL, LV_STATE_DEFAULT);
            lv_obj_clear_flag(board_cells[r][c], LV_OBJ_FLAG_SCROLLABLE);
            displayed_cell_colors[r][c] = COLOR_EMPTY_CELL;
        }
    }

    // --- Bottom Button Panel ---
    bottom_button_panel = lv_obj_create(screen);
    lv_obj_remove_style_all(bottom_button_panel);
    lv_obj_set_size(bottom_button_panel, lv_pct(100), BOTTOM_PANEL_HEIGHT); // Height 70
    lv_obj_align(bottom_button_panel, LV_ALIGN_BOTTOM_MID, 0, -5); // Top at Y = 480 - 70 - 5 = 405
    lv_obj_set_style_bg_opa(bottom_button_panel, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(bottom_button_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_button_panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(bottom_button_panel, 5, LV_STATE_DEFAULT); 
    lv_obj_set_style_pad_gap(bottom_button_panel, 5, LV_STATE_DEFAULT);
    lv_obj_clear_flag(bottom_button_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(bottom_button_panel, LV_SCROLLBAR_MODE_OFF);

    lv_coord_t pseudo_btn_flex_grow = 1;
    lv_coord_t pseudo_btn_height = 50;

    // Left Pseudo-Button
    left_button_pseudo = lv_obj_create(bottom_button_panel);
    lv_obj_set_height(left_button_pseudo, pseudo_btn_height);
    lv_obj_set_flex_grow(left_button_pseudo, pseudo_btn_flex_grow);
    lv_obj_set_style_bg_color(left_button_pseudo, PSEUDO_BUTTON_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(left_button_pseudo, PSEUDO_BUTTON_PRESSED_COLOR, LV_STATE_PRESSED);
    lv_obj_set_style_radius(left_button_pseudo, 5, LV_STATE_DEFAULT);
    lv_obj_add_flag(left_button_pseudo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(left_button_pseudo, control_action_event_cb, LV_EVENT_CLICKED, (void*)ACTION_MOVE_LEFT);
    lv_obj_clear_flag(left_button_pseudo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(left_button_pseudo, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *lbl_left = lv_label_create(left_button_pseudo);
    lv_label_set_text(lbl_left, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl_left, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_center(lbl_left);

    // Hard Drop Pseudo-Button
    hard_drop_button_pseudo = lv_obj_create(bottom_button_panel);
    lv_obj_set_height(hard_drop_button_pseudo, pseudo_btn_height);
    lv_obj_set_flex_grow(hard_drop_button_pseudo, pseudo_btn_flex_grow);
    lv_obj_set_style_bg_color(hard_drop_button_pseudo, PSEUDO_BUTTON_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(hard_drop_button_pseudo, PSEUDO_BUTTON_PRESSED_COLOR, LV_STATE_PRESSED);
    lv_obj_set_style_radius(hard_drop_button_pseudo, 5, LV_STATE_DEFAULT);
    lv_obj_add_flag(hard_drop_button_pseudo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hard_drop_button_pseudo, control_action_event_cb, LV_EVENT_CLICKED, (void*)ACTION_HARD_DROP);
    lv_obj_clear_flag(hard_drop_button_pseudo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(hard_drop_button_pseudo, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *lbl_hard_drop = lv_label_create(hard_drop_button_pseudo);
    lv_label_set_text(lbl_hard_drop, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(lbl_hard_drop, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_center(lbl_hard_drop);

    // Rotate Piece Pseudo-Button
    rotate_button_pseudo = lv_obj_create(bottom_button_panel);
    lv_obj_set_height(rotate_button_pseudo, pseudo_btn_height);
    lv_obj_set_flex_grow(rotate_button_pseudo, pseudo_btn_flex_grow);
    lv_obj_set_style_bg_color(rotate_button_pseudo, PSEUDO_BUTTON_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(rotate_button_pseudo, PSEUDO_BUTTON_PRESSED_COLOR, LV_STATE_PRESSED);
    lv_obj_set_style_radius(rotate_button_pseudo, 5, LV_STATE_DEFAULT);
    lv_obj_add_flag(rotate_button_pseudo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(rotate_button_pseudo, control_action_event_cb, LV_EVENT_CLICKED, (void*)ACTION_ROTATE_CW);
    lv_obj_clear_flag(rotate_button_pseudo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(rotate_button_pseudo, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *lbl_rotate = lv_label_create(rotate_button_pseudo);
    lv_label_set_text(lbl_rotate, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(lbl_rotate, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_center(lbl_rotate);

    // Right Pseudo-Button
    right_button_pseudo = lv_obj_create(bottom_button_panel);
    lv_obj_set_height(right_button_pseudo, pseudo_btn_height);
    lv_obj_set_flex_grow(right_button_pseudo, pseudo_btn_flex_grow);
    lv_obj_set_style_bg_color(right_button_pseudo, PSEUDO_BUTTON_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(right_button_pseudo, PSEUDO_BUTTON_PRESSED_COLOR, LV_STATE_PRESSED);
    lv_obj_set_style_radius(right_button_pseudo, 5, LV_STATE_DEFAULT);
    lv_obj_add_flag(right_button_pseudo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(right_button_pseudo, control_action_event_cb, LV_EVENT_CLICKED, (void*)ACTION_MOVE_RIGHT);
    lv_obj_clear_flag(right_button_pseudo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(right_button_pseudo, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *lbl_right = lv_label_create(right_button_pseudo);
    lv_label_set_text(lbl_right, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(lbl_right, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_center(lbl_right);

    ESP_LOGI(TAG, "Bottom pseudo-buttons created in a flex panel.");

    // --- Game Over Label ---
    game_over_label_obj = lv_label_create(screen);
    lv_obj_set_style_text_color(game_over_label_obj, lv_color_hex(0xff0000), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(game_over_label_obj, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(game_over_label_obj, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_label_set_text(game_over_label_obj, "GAME OVER\nClick Board to Restart");
    lv_obj_align(game_over_label_obj, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_flag(game_over_label_obj, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Tetris UI elements created with new control layout.");
    start_tetris_game();
}
