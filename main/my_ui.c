#include "my_ui.h"
#include "esp_log.h"
#include "esp_random.h" // For esp_random()
#include <string.h>     // For memset, memcpy
#include <stdio.h>      // For sprintf
#include <stdbool.h>    // For bool type

static const char *TAG = "ui_space_invaders_v83_fix2"; // Updated TAG

// --- Game Configuration ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480
#define GAME_AREA_X_OFFSET 40
#define GAME_AREA_Y_OFFSET 50
#define GAME_AREA_WIDTH (SCREEN_WIDTH - 2 * GAME_AREA_X_OFFSET)
#define GAME_AREA_HEIGHT (SCREEN_HEIGHT - GAME_AREA_Y_OFFSET - 20)
#define PLAYER_WIDTH 40
#define PLAYER_HEIGHT 20
#define PLAYER_Y_POS (GAME_AREA_HEIGHT - PLAYER_HEIGHT - 10)
#define PLAYER_MOVE_STEP 10
#define PLAYER_LIVES_INITIAL 3
#define PLAYER_BULLET_WIDTH 4
#define PLAYER_BULLET_HEIGHT 12
#define PLAYER_BULLET_SPEED 15
#define MAX_PLAYER_BULLETS 3
#define ALIEN_ROWS 4
#define ALIEN_COLS 7
#define ALIEN_WIDTH 30
#define ALIEN_HEIGHT 20
#define ALIEN_SPACING_X 10
#define ALIEN_SPACING_Y 10
#define ALIEN_INITIAL_Y 20
#define ALIEN_MOVE_STEP_X 5
#define ALIEN_MOVE_STEP_Y 10
#define ALIEN_MOVE_INTERVAL_MS 500
#define ALIEN_FIRE_CHANCE_PERCENT 5
#define ALIEN_BULLET_WIDTH 4
#define ALIEN_BULLET_HEIGHT 10
#define ALIEN_BULLET_SPEED 8
#define MAX_ALIEN_BULLETS 10
#define GAME_LOOP_TIMER_MS 50
#define COLOR_PLAYER lv_color_hex(0x00FF00)
#define COLOR_PLAYER_BULLET lv_color_hex(0x00FFFF)
#define COLOR_ALIEN lv_color_hex(0xFF0000)
#define COLOR_ALIEN_BULLET lv_color_hex(0xFFFF00)
#define COLOR_GAME_AREA_BG lv_color_hex(0x000000)
#define COLOR_SCREEN_BG lv_color_hex(0x100010)
#define PLAYER_FIRE_COOLDOWN_MS 300

typedef struct {
    lv_obj_t *obj;
    bool active;
} game_object_t;

static lv_obj_t *screen_obj;
static lv_obj_t *game_area_container;
static game_object_t player;
static game_object_t player_bullets[MAX_PLAYER_BULLETS];
static game_object_t aliens[ALIEN_ROWS][ALIEN_COLS];
static game_object_t alien_bullets[MAX_ALIEN_BULLETS];
static lv_obj_t *left_touch_zone;
static lv_obj_t *center_touch_zone;
static lv_obj_t *right_touch_zone;
static lv_obj_t *score_label;
static lv_obj_t *lives_label;
static lv_obj_t *game_over_msg_label;
static lv_timer_t *game_loop_timer;
static lv_timer_t *alien_move_timer;
static int current_score;
static int player_lives;
static bool game_is_active;
static bool game_is_over;
static int alien_move_direction = 1;
static uint32_t last_player_fire_time = 0;

// --- Forward Declarations ---
static void create_ui_elements(void);
static void init_game_objects(void);
static void start_new_game(void);
static void game_loop_cb(lv_timer_t *timer);
static void alien_move_logic_cb(lv_timer_t *timer);
static void touch_zone_event_cb(lv_event_t *e);
static void screen_click_restart_cb(lv_event_t *e);
static void player_move(int dx);
static void player_fire(void);
static void spawn_alien_bullet(lv_obj_t *alien_obj);
static void update_score_lives_display(void);
static void handle_game_over(void);
static bool check_rect_collision(lv_obj_t* obj1, lv_obj_t* obj2);


// --- Function Implementations ---

static void create_ui_elements(void) {
    screen_obj = lv_scr_act();
    lv_obj_set_style_bg_color(screen_obj, COLOR_SCREEN_BG, LV_STATE_DEFAULT);
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);

    score_label = lv_label_create(screen_obj);
    lv_obj_set_style_text_color(score_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_align(score_label, LV_ALIGN_TOP_LEFT, 10, 10);

    lives_label = lv_label_create(screen_obj);
    lv_obj_set_style_text_color(lives_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lives_label, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_align(lives_label, LV_ALIGN_TOP_RIGHT, -10, 10);

    game_area_container = lv_obj_create(screen_obj);
    lv_obj_set_size(game_area_container, GAME_AREA_WIDTH, GAME_AREA_HEIGHT);
    lv_obj_align(game_area_container, LV_ALIGN_TOP_MID, 0, GAME_AREA_Y_OFFSET);
    lv_obj_set_style_bg_color(game_area_container, COLOR_GAME_AREA_BG, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(game_area_container, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(game_area_container, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(game_area_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(game_area_container, LV_SCROLLBAR_MODE_OFF);

    game_over_msg_label = lv_label_create(screen_obj);
    lv_obj_set_style_text_color(game_over_msg_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(game_over_msg_label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(game_over_msg_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_label_set_long_mode(game_over_msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(game_over_msg_label, SCREEN_WIDTH - 40);
    lv_obj_center(game_over_msg_label);
    lv_obj_add_flag(game_over_msg_label, LV_OBJ_FLAG_HIDDEN);

    player.obj = lv_obj_create(game_area_container);
    if (!player.obj) {ESP_LOGE(TAG, "Failed to create player.obj!"); return;}
    lv_obj_set_size(player.obj, PLAYER_WIDTH, PLAYER_HEIGHT);
    lv_obj_set_style_bg_color(player.obj, COLOR_PLAYER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(player.obj, 3, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(player.obj, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(player.obj, LV_OBJ_FLAG_SCROLLABLE);

    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            aliens[r][c].obj = lv_obj_create(game_area_container);
            if (!aliens[r][c].obj) {ESP_LOGE(TAG, "Failed to create alien %d,%d!", r,c); return;}
            lv_obj_set_size(aliens[r][c].obj, ALIEN_WIDTH, ALIEN_HEIGHT);
            lv_obj_set_style_bg_color(aliens[r][c].obj, COLOR_ALIEN, LV_STATE_DEFAULT);
            lv_obj_set_style_radius(aliens[r][c].obj, 3, LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(aliens[r][c].obj, 0, LV_STATE_DEFAULT);
            lv_obj_add_flag(aliens[r][c].obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(aliens[r][c].obj, LV_OBJ_FLAG_SCROLLABLE);
            aliens[r][c].active = false;
        }
    }

    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        player_bullets[i].obj = lv_obj_create(game_area_container);
        if (!player_bullets[i].obj) {ESP_LOGE(TAG, "Failed to create player_bullet %d!", i); return;}
        lv_obj_set_size(player_bullets[i].obj, PLAYER_BULLET_WIDTH, PLAYER_BULLET_HEIGHT);
        lv_obj_set_style_bg_color(player_bullets[i].obj, COLOR_PLAYER_BULLET, LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(player_bullets[i].obj, 0, LV_STATE_DEFAULT);
        lv_obj_add_flag(player_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(player_bullets[i].obj, LV_OBJ_FLAG_SCROLLABLE);
        player_bullets[i].active = false;
    }

    for (int i = 0; i < MAX_ALIEN_BULLETS; ++i) {
        alien_bullets[i].obj = lv_obj_create(game_area_container);
        if (!alien_bullets[i].obj) {ESP_LOGE(TAG, "Failed to create alien_bullet %d!", i); return;}
        lv_obj_set_size(alien_bullets[i].obj, ALIEN_BULLET_WIDTH, ALIEN_BULLET_HEIGHT);
        lv_obj_set_style_bg_color(alien_bullets[i].obj, COLOR_ALIEN_BULLET, LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(alien_bullets[i].obj, 0, LV_STATE_DEFAULT);
        lv_obj_add_flag(alien_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(alien_bullets[i].obj, LV_OBJ_FLAG_SCROLLABLE);
        alien_bullets[i].active = false;
    }

    lv_coord_t zone_width = SCREEN_WIDTH / 3;
    lv_coord_t zone_height = SCREEN_HEIGHT;

    left_touch_zone = lv_obj_create(screen_obj);
    lv_obj_set_size(left_touch_zone, zone_width, zone_height);
    lv_obj_set_pos(left_touch_zone, 0, 0);
    lv_obj_set_style_bg_opa(left_touch_zone, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(left_touch_zone, 0, LV_STATE_DEFAULT);
    lv_obj_add_flag(left_touch_zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(left_touch_zone, touch_zone_event_cb, LV_EVENT_CLICKED, (void*)0);

    center_touch_zone = lv_obj_create(screen_obj);
    lv_obj_set_size(center_touch_zone, zone_width, zone_height);
    lv_obj_set_pos(center_touch_zone, zone_width, 0);
    lv_obj_set_style_bg_opa(center_touch_zone, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(center_touch_zone, 0, LV_STATE_DEFAULT);
    lv_obj_add_flag(center_touch_zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(center_touch_zone, touch_zone_event_cb, LV_EVENT_CLICKED, (void*)1);

    right_touch_zone = lv_obj_create(screen_obj);
    lv_obj_set_size(right_touch_zone, SCREEN_WIDTH - (2 * zone_width), zone_height);
    lv_obj_set_pos(right_touch_zone, 2 * zone_width, 0);
    lv_obj_set_style_bg_opa(right_touch_zone, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(right_touch_zone, 0, LV_STATE_DEFAULT);
    lv_obj_add_flag(right_touch_zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(right_touch_zone, touch_zone_event_cb, LV_EVENT_CLICKED, (void*)2);

    lv_obj_add_event_cb(screen_obj, screen_click_restart_cb, LV_EVENT_CLICKED, NULL);
}

static void init_game_objects(void) {
    if (!lv_obj_is_valid(player.obj)) { ESP_LOGE(TAG, "Player object invalid in init_game_objects!"); return; }
    lv_obj_set_pos(player.obj, (GAME_AREA_WIDTH - PLAYER_WIDTH) / 2, PLAYER_Y_POS);
    lv_obj_clear_flag(player.obj, LV_OBJ_FLAG_HIDDEN);
    player.active = true;

    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            if (!lv_obj_is_valid(aliens[r][c].obj)) { ESP_LOGE(TAG, "Alien %d,%d object invalid in init_game_objects!", r,c); continue; }
            lv_coord_t alien_x = c * (ALIEN_WIDTH + ALIEN_SPACING_X) + (GAME_AREA_WIDTH - (ALIEN_COLS * ALIEN_WIDTH + (ALIEN_COLS - 1) * ALIEN_SPACING_X)) / 2;
            lv_coord_t alien_y = r * (ALIEN_HEIGHT + ALIEN_SPACING_Y) + ALIEN_INITIAL_Y;
            lv_obj_set_pos(aliens[r][c].obj, alien_x, alien_y);
            lv_obj_clear_flag(aliens[r][c].obj, LV_OBJ_FLAG_HIDDEN);
            aliens[r][c].active = true;
        }
    }
    alien_move_direction = 1;

    for(int i=0; i<MAX_PLAYER_BULLETS; ++i) {
        if (!lv_obj_is_valid(player_bullets[i].obj)) { ESP_LOGE(TAG, "Player bullet %d object invalid in init_game_objects!", i); continue; }
        player_bullets[i].active = false; 
        lv_obj_add_flag(player_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
    }
    for(int i=0; i<MAX_ALIEN_BULLETS; ++i) {
        if (!lv_obj_is_valid(alien_bullets[i].obj)) { ESP_LOGE(TAG, "Alien bullet %d object invalid in init_game_objects!", i); continue; }
        alien_bullets[i].active = false; 
        lv_obj_add_flag(alien_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void start_new_game(void) {
    ESP_LOGI(TAG, "Starting new game...");
    current_score = 0;
    player_lives = PLAYER_LIVES_INITIAL;
    game_is_active = true;
    game_is_over = false;

    if (lv_obj_is_valid(game_over_msg_label)) lv_obj_add_flag(game_over_msg_label, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_is_valid(left_touch_zone)) lv_obj_clear_flag(left_touch_zone, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_is_valid(center_touch_zone)) lv_obj_clear_flag(center_touch_zone, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_is_valid(right_touch_zone)) lv_obj_clear_flag(right_touch_zone, LV_OBJ_FLAG_HIDDEN);

    init_game_objects();
    update_score_lives_display();

    if (game_loop_timer) {
        lv_timer_reset(game_loop_timer);
        lv_timer_resume(game_loop_timer);
    } else {
        game_loop_timer = lv_timer_create(game_loop_cb, GAME_LOOP_TIMER_MS, NULL);
    }
    if (alien_move_timer) {
        lv_timer_reset(alien_move_timer);
        lv_timer_resume(alien_move_timer);
    } else {
        alien_move_timer = lv_timer_create(alien_move_logic_cb, ALIEN_MOVE_INTERVAL_MS, NULL);
    }
}

static void player_move(int dx) {
    if (!game_is_active || !player.active || !lv_obj_is_valid(player.obj)) return;
    lv_coord_t current_x = lv_obj_get_x(player.obj);
    lv_coord_t new_x = current_x + dx;
    if (new_x < 0) new_x = 0;
    if (new_x > GAME_AREA_WIDTH - PLAYER_WIDTH) new_x = GAME_AREA_WIDTH - PLAYER_WIDTH;
    lv_obj_set_x(player.obj, new_x);
}

static void player_fire(void) {
    if (!game_is_active || !player.active || !lv_obj_is_valid(player.obj)) return;
    uint32_t current_time = lv_tick_get();
    if (current_time - last_player_fire_time < PLAYER_FIRE_COOLDOWN_MS) return;
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        if (!player_bullets[i].active && lv_obj_is_valid(player_bullets[i].obj)) {
            player_bullets[i].active = true;
            lv_obj_set_pos(player_bullets[i].obj,
                           lv_obj_get_x(player.obj) + PLAYER_WIDTH / 2 - PLAYER_BULLET_WIDTH / 2,
                           lv_obj_get_y(player.obj) - PLAYER_BULLET_HEIGHT);
            lv_obj_clear_flag(player_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            last_player_fire_time = current_time;
            ESP_LOGD(TAG, "Player fired bullet %d", i);
            break;
        }
    }
}

static void spawn_alien_bullet(lv_obj_t *firing_alien_obj) {
    if (!game_is_active || !lv_obj_is_valid(firing_alien_obj)) return;
    for (int i = 0; i < MAX_ALIEN_BULLETS; ++i) {
        if (!alien_bullets[i].active && lv_obj_is_valid(alien_bullets[i].obj)) {
            alien_bullets[i].active = true;
            lv_obj_set_pos(alien_bullets[i].obj,
                           lv_obj_get_x(firing_alien_obj) + ALIEN_WIDTH / 2 - ALIEN_BULLET_WIDTH / 2,
                           lv_obj_get_y(firing_alien_obj) + ALIEN_HEIGHT);
            lv_obj_clear_flag(alien_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGD(TAG, "Alien fired bullet %d", i);
            break;
        }
    }
}

static void update_score_lives_display(void) {
    if (lv_obj_is_valid(score_label)) lv_label_set_text_fmt(score_label, "Score: %d", current_score);
    if (lv_obj_is_valid(lives_label)) lv_label_set_text_fmt(lives_label, "Lives: %d", player_lives);
}

// CORRECTED: check_rect_collision function
static bool check_rect_collision(lv_obj_t* obj1, lv_obj_t* obj2) {
    if (!lv_obj_is_valid(obj1) || !lv_obj_is_valid(obj2)) {
        // ESP_LOGW(TAG, "Collision check with invalid object(s). obj1_valid: %d, obj2_valid: %d", lv_obj_is_valid(obj1), lv_obj_is_valid(obj2));
        return false;
    }
    if (lv_obj_has_flag(obj1, LV_OBJ_FLAG_HIDDEN) || lv_obj_has_flag(obj2, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }

    lv_area_t area1, area2, res_area; // res_area to store intersection result
    lv_obj_get_coords(obj1, &area1);
    lv_obj_get_coords(obj2, &area2);

    // Correctly call _lv_area_intersect
    // The first parameter is for the result, the next two are the areas to check.
    return _lv_area_intersect(&res_area, &area1, &area2);
}


static void handle_game_over(void) {
    ESP_LOGI(TAG, "Game Over! Final Score: %d", current_score);
    game_is_active = false;
    game_is_over = true;
    if (game_loop_timer) lv_timer_pause(game_loop_timer);
    if (alien_move_timer) lv_timer_pause(alien_move_timer);

    if (lv_obj_is_valid(game_over_msg_label)) {
        lv_label_set_text_fmt(game_over_msg_label, "GAME OVER\nScore: %d\nClick Screen to Restart", current_score);
        lv_obj_clear_flag(game_over_msg_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (lv_obj_is_valid(left_touch_zone)) lv_obj_add_flag(left_touch_zone, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_is_valid(center_touch_zone)) lv_obj_add_flag(center_touch_zone, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_is_valid(right_touch_zone)) lv_obj_add_flag(right_touch_zone, LV_OBJ_FLAG_HIDDEN);
}

static void game_loop_cb(lv_timer_t *timer) {
    if (!game_is_active) return;

    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        if (player_bullets[i].active && lv_obj_is_valid(player_bullets[i].obj)) {
            lv_coord_t y = lv_obj_get_y(player_bullets[i].obj);
            y -= PLAYER_BULLET_SPEED;
            if (y < -PLAYER_BULLET_HEIGHT) {
                player_bullets[i].active = false;
                lv_obj_add_flag(player_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_set_y(player_bullets[i].obj, y);
                for (int r = 0; r < ALIEN_ROWS; ++r) {
                    for (int c = 0; c < ALIEN_COLS; ++c) {
                        if (aliens[r][c].active && lv_obj_is_valid(aliens[r][c].obj) &&
                            check_rect_collision(player_bullets[i].obj, aliens[r][c].obj)) {
                            aliens[r][c].active = false;
                            lv_obj_add_flag(aliens[r][c].obj, LV_OBJ_FLAG_HIDDEN);
                            player_bullets[i].active = false;
                            lv_obj_add_flag(player_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
                            current_score += 10;
                            update_score_lives_display();
                            goto next_player_bullet_loop;
                        }
                    }
                }
            }
            next_player_bullet_loop:;
        }
    }

    for (int i = 0; i < MAX_ALIEN_BULLETS; ++i) {
        if (alien_bullets[i].active && lv_obj_is_valid(alien_bullets[i].obj)) {
            lv_coord_t y = lv_obj_get_y(alien_bullets[i].obj);
            y += ALIEN_BULLET_SPEED;
            if (y > GAME_AREA_HEIGHT) {
                alien_bullets[i].active = false;
                lv_obj_add_flag(alien_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_set_y(alien_bullets[i].obj, y);
                if (player.active && lv_obj_is_valid(player.obj) &&
                    check_rect_collision(alien_bullets[i].obj, player.obj)) {
                    alien_bullets[i].active = false;
                    lv_obj_add_flag(alien_bullets[i].obj, LV_OBJ_FLAG_HIDDEN);
                    player_lives--;
                    update_score_lives_display();
                    if (player_lives <= 0) {
                        handle_game_over();
                        return;
                    }
                }
            }
        }
    }
    
    bool all_aliens_dead = true;
    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            if (aliens[r][c].active) {
                all_aliens_dead = false;
                break;
            }
        }
        if (!all_aliens_dead) break;
    }
    if (all_aliens_dead && game_is_active) {
        ESP_LOGI(TAG, "All aliens cleared! Resetting alien formation.");
        init_game_objects();
    }
}

static void alien_move_logic_cb(lv_timer_t *timer) {
    if (!game_is_active) return;
    bool wall_hit = false;
    lv_coord_t dx = ALIEN_MOVE_STEP_X * alien_move_direction;
    lv_coord_t dy = 0;

    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            if (aliens[r][c].active && lv_obj_is_valid(aliens[r][c].obj)) {
                lv_coord_t alien_x = lv_obj_get_x(aliens[r][c].obj);
                if ((alien_move_direction == 1 && alien_x + ALIEN_WIDTH + dx > GAME_AREA_WIDTH) ||
                    (alien_move_direction == -1 && alien_x + dx < 0)) {
                    wall_hit = true;
                    break;
                }
            }
        }
        if (wall_hit) break;
    }

    if (wall_hit) {
        alien_move_direction *= -1;
        dy = ALIEN_MOVE_STEP_Y;
        dx = 0;
    }

    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            if (aliens[r][c].active && lv_obj_is_valid(aliens[r][c].obj)) {
                lv_obj_set_x(aliens[r][c].obj, lv_obj_get_x(aliens[r][c].obj) + dx);
                lv_obj_set_y(aliens[r][c].obj, lv_obj_get_y(aliens[r][c].obj) + dy);

                if (lv_obj_get_y(aliens[r][c].obj) + ALIEN_HEIGHT >= PLAYER_Y_POS) {
                    handle_game_over();
                    return;
                }
                if ((esp_random() % 100) < ALIEN_FIRE_CHANCE_PERCENT) {
                    bool is_bottom_most_in_col = true;
                    for(int r_check = r + 1; r_check < ALIEN_ROWS; ++r_check) {
                        if(aliens[r_check][c].active) {
                            is_bottom_most_in_col = false;
                            break;
                        }
                    }
                    if(is_bottom_most_in_col) {
                        spawn_alien_bullet(aliens[r][c].obj);
                    }
                }
            }
        }
    }
}

static void touch_zone_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    intptr_t zone_id = (intptr_t)lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        if (!game_is_active || game_is_over) return;
        switch (zone_id) {
            case 0: player_move(-PLAYER_MOVE_STEP); break;
            case 1: player_fire(); break;
            case 2: player_move(PLAYER_MOVE_STEP); break;
        }
    }
}

static void screen_click_restart_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED && game_is_over) {
        start_new_game();
    }
}

void my_ui_init(void) {
    ESP_LOGI(TAG, "Initializing Space Invaders UI (%s)", TAG);
    ESP_LOGW(TAG, "Ensure LVGL input device (touchscreen) is correctly initialized and registered BEFORE calling this function.");
    create_ui_elements();
    start_new_game();
    ESP_LOGI(TAG, "Space Invaders UI Initialized.");
}
