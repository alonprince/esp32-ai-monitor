#include "ui.h"
#include "ble_server.h"
#include "audio.h"
#include "lvgl.h"
#include <string.h>
#include "esp_log.h"

static void btn_approve_event_cb(lv_event_t *e)
{
    ble_server_send_interaction(1);
    audio_play_notify();
    ESP_LOGI("ui", "Approve button pressed");
}

static void btn_deny_event_cb(lv_event_t *e)
{
    ble_server_send_interaction(2);
    audio_play_notify();
    ESP_LOGI("ui", "Deny button pressed");
}

static const char *TAG = "ui";

/* AMOLED color palette (design.md) */
#define COLOR_BG         lv_color_hex(0x000000)
#define COLOR_CARD       lv_color_hex(0x121212)
#define COLOR_TEXT       lv_color_hex(0xF3F4F6)
#define COLOR_TEXT_SEC   lv_color_hex(0x9CA3AF)
#define COLOR_CYAN       lv_color_hex(0x00F0FF)
#define COLOR_BLUE       lv_color_hex(0x0088FF)
#define COLOR_GREEN      lv_color_hex(0x10B981)
#define COLOR_AMBER      lv_color_hex(0xF59E0B)
#define COLOR_RED        lv_color_hex(0xEF4444)

static lv_obj_t *guide_screen = NULL;
static lv_obj_t *dashboard_screen = NULL;
static lv_obj_t *approval_screen = NULL;

static lv_obj_t *guide_label = NULL;
static lv_obj_t *dashboard_state_label = NULL;
static lv_obj_t *dashboard_task_label = NULL;
static lv_obj_t *dashboard_agent_label = NULL;
static lv_obj_t *dashboard_workspace_label = NULL;
static lv_obj_t *dashboard_tool_label = NULL;
static lv_obj_t *dashboard_arc = NULL;
static lv_obj_t *approval_cmd_label = NULL;

static ble_monitor_state_t current_state = BLE_STATE_DISCONNECTED;
static lv_obj_t *active_screen = NULL;

static void create_guide_screen(void)
{
    guide_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(guide_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(guide_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(guide_screen, 0, 0);
    lv_obj_set_style_pad_all(guide_screen, 0, 0);

    lv_obj_t *ring = lv_obj_create(guide_screen);
    lv_obj_set_size(ring, 80, 80);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_40, 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *bt_icon = lv_label_create(ring);
    lv_label_set_text(bt_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_center(bt_icon);
    lv_obj_set_style_text_color(bt_icon, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(bt_icon, &lv_font_montserrat_24, 0);

    guide_label = lv_label_create(guide_screen);
    lv_label_set_text(guide_label, "WAITING FOR\nCONNECTION...");
    lv_obj_set_style_text_color(guide_label, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(guide_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(guide_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(guide_label, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t *name_label = lv_label_create(guide_screen);
    lv_label_set_text(name_label, "Buddy-AMOLED");
    lv_obj_set_style_text_color(name_label, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 110);
}

static void create_dashboard_screen(void)
{
    dashboard_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(dashboard_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(dashboard_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dashboard_screen, 0, 0);
    lv_obj_set_style_pad_all(dashboard_screen, 0, 0);

    /* Top status bar */
    lv_obj_t *status_bar = lv_obj_create(dashboard_screen);
    lv_obj_set_size(status_bar, 480, 30);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_hor(status_bar, 12, 0);
    lv_obj_set_style_pad_ver(status_bar, 0, 0);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *ble_status = lv_label_create(status_bar);
    lv_label_set_text(ble_status, LV_SYMBOL_BLUETOOTH " BLE");
    lv_obj_set_style_text_color(ble_status, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(ble_status, &lv_font_montserrat_14, 0);

    dashboard_state_label = lv_label_create(status_bar);
    lv_label_set_text(dashboard_state_label, "IDLE");
    lv_obj_set_style_text_color(dashboard_state_label, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(dashboard_state_label, &lv_font_montserrat_14, 0);

    /* Center console - task display (shrink to make room for scroll) */
    lv_obj_t *center_card = lv_obj_create(dashboard_screen);
    lv_obj_set_size(center_card, 440, 120);
    lv_obj_align(center_card, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(center_card, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(center_card, LV_OPA_80, 0);
    lv_obj_set_style_border_width(center_card, 1, 0);
    lv_obj_set_style_border_color(center_card, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(center_card, 12, 0);
    lv_obj_set_style_pad_all(center_card, 16, 0);

    lv_obj_t *task_title = lv_label_create(center_card);
    lv_label_set_text(task_title, "CURRENT TASK");
    lv_obj_set_style_text_color(task_title, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(task_title, &lv_font_montserrat_14, 0);
    lv_obj_align(task_title, LV_ALIGN_TOP_LEFT, 0, 0);

    dashboard_task_label = lv_label_create(center_card);
    lv_label_set_text(dashboard_task_label, "$ --");
    lv_obj_set_style_text_color(dashboard_task_label, COLOR_CYAN, 0);
    lv_obj_set_style_text_font(dashboard_task_label, &lv_font_montserrat_18, 0);
    lv_obj_align(dashboard_task_label, LV_ALIGN_TOP_LEFT, 0, 24);

    lv_obj_t *tool_title = lv_label_create(center_card);
    lv_label_set_text(tool_title, "Active Tool");
    lv_obj_set_style_text_color(tool_title, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(tool_title, &lv_font_montserrat_14, 0);
    lv_obj_align(tool_title, LV_ALIGN_TOP_LEFT, 0, 68);

    dashboard_tool_label = lv_label_create(center_card);
    lv_label_set_text(dashboard_tool_label, "None");
    lv_obj_set_style_text_color(dashboard_tool_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(dashboard_tool_label, &lv_font_montserrat_14, 0);
    lv_obj_align(dashboard_tool_label, LV_ALIGN_TOP_LEFT, 0, 88);

    lv_obj_t *workspace_title = lv_label_create(center_card);
    lv_label_set_text(workspace_title, "Workspace");
    lv_obj_set_style_text_color(workspace_title, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(workspace_title, &lv_font_montserrat_14, 0);
    lv_obj_align(workspace_title, LV_ALIGN_TOP_LEFT, 0, 112);

    dashboard_workspace_label = lv_label_create(center_card);
    lv_label_set_text(dashboard_workspace_label, "None");
    lv_obj_set_style_text_color(dashboard_workspace_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(dashboard_workspace_label, &lv_font_montserrat_14, 0);
    lv_obj_align(dashboard_workspace_label, LV_ALIGN_TOP_LEFT, 0, 132);

    /* Scrollable message log */
    lv_obj_t *scroll_area = lv_obj_create(dashboard_screen);
    lv_obj_set_size(scroll_area, 440, 200);
    lv_obj_align(scroll_area, LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_set_style_bg_color(scroll_area, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(scroll_area, LV_OPA_60, 0);
    lv_obj_set_style_border_width(scroll_area, 1, 0);
    lv_obj_set_style_border_color(scroll_area, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(scroll_area, 8, 0);
    lv_obj_set_style_pad_all(scroll_area, 8, 0);
    lv_obj_set_scroll_dir(scroll_area, LV_DIR_VER);
    lv_obj_add_flag(scroll_area, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_obj_t *scroll_content = lv_label_create(scroll_area);
    lv_label_set_text(scroll_content,
        "Line 01: grep -r TODO src/\n"
        "Line 02: ls -la main/\n"
        "Line 03: git diff HEAD~1\n"
        "Line 04: cargo build\n"
        "Line 05: npm run test\n"
        "Line 06: vim main.c\n"
        "Line 07: cat README\n"
        "Line 08: ssh deploy\n"
        "Line 09: docker compose\n"
        "Line 10: systemctl status\n"
        "Line 11: python train.py\n"
        "Line 12: brew install ffmpeg\n"
        "Line 13: ffmpeg convert\n"
        "Line 14: rm -rf node_modules\n"
        "Line 15: yarn add react\n"
        "Line 16: kubectl get pods\n"
        "Line 17: make clean all\n"
        "Line 18: curl localhost\n");
    lv_obj_set_style_text_color(scroll_content, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(scroll_content, &lv_font_montserrat_14, 0);
    lv_obj_set_width(scroll_content, 420);
    lv_obj_set_style_text_line_space(scroll_content, 4, 0);

    /* Bottom agent info bar */
    lv_obj_t *bottom_bar = lv_obj_create(dashboard_screen);
    lv_obj_set_size(bottom_bar, 440, 60);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(bottom_bar, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(bottom_bar, 1, 0);
    lv_obj_set_style_border_color(bottom_bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(bottom_bar, 12, 0);
    lv_obj_set_style_pad_all(bottom_bar, 12, 0);

    lv_obj_t *agent_title = lv_label_create(bottom_bar);
    lv_label_set_text(agent_title, "Agent");
    lv_obj_set_style_text_color(agent_title, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(agent_title, &lv_font_montserrat_14, 0);
    lv_obj_align(agent_title, LV_ALIGN_TOP_LEFT, 0, 0);

    dashboard_agent_label = lv_label_create(bottom_bar);
    lv_label_set_text(dashboard_agent_label, "Disconnected");
    lv_obj_set_style_text_color(dashboard_agent_label, COLOR_RED, 0);
    lv_obj_set_style_text_font(dashboard_agent_label, &lv_font_montserrat_18, 0);
    lv_obj_align(dashboard_agent_label, LV_ALIGN_TOP_LEFT, 0, 20);

    dashboard_arc = lv_arc_create(bottom_bar);
    lv_obj_set_size(dashboard_arc, 50, 50);
    lv_obj_align(dashboard_arc, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_arc_set_mode(dashboard_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(dashboard_arc, 0, 100);
    lv_arc_set_value(dashboard_arc, 0);
    lv_obj_set_style_arc_color(dashboard_arc, COLOR_CYAN, 0);
    lv_obj_set_style_arc_color(dashboard_arc, COLOR_TEXT_SEC, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(dashboard_arc, 6, 0);
    lv_obj_set_style_arc_width(dashboard_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(dashboard_arc, LV_OPA_TRANSP, 0);
    lv_obj_remove_style(dashboard_arc, NULL, LV_PART_KNOB);
}

static void create_approval_screen(void)
{
    approval_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(approval_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(approval_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(approval_screen, 0, 0);
    lv_obj_set_style_pad_all(approval_screen, 0, 0);

    lv_obj_t *title = lv_label_create(approval_screen);
    lv_label_set_text(title, "CONFIRM COMMAND?");
    lv_obj_set_style_text_color(title, COLOR_AMBER, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    approval_cmd_label = lv_label_create(approval_screen);
    lv_label_set_text(approval_cmd_label, "No command pending");
    lv_obj_set_style_text_color(approval_cmd_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(approval_cmd_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(approval_cmd_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(approval_cmd_label, 440);
    lv_obj_align(approval_cmd_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *btn_approve = lv_obj_create(approval_screen);
    lv_obj_set_size(btn_approve, 180, 60);
    lv_obj_align(btn_approve, LV_ALIGN_BOTTOM_RIGHT, -30, -40);
    lv_obj_set_style_bg_color(btn_approve, COLOR_GREEN, 0);
    lv_obj_set_style_bg_opa(btn_approve, LV_OPA_30, 0);
    lv_obj_set_style_border_color(btn_approve, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(btn_approve, 2, 0);
    lv_obj_set_style_radius(btn_approve, 12, 0);
    lv_obj_add_flag(btn_approve, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_approve, btn_approve_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *approve_label = lv_label_create(btn_approve);
    lv_label_set_text(approve_label, "APPROVE");
    lv_obj_center(approve_label);
    lv_obj_set_style_text_color(approve_label, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(approve_label, &lv_font_montserrat_24, 0);

    lv_obj_t *btn_deny = lv_obj_create(approval_screen);
    lv_obj_set_size(btn_deny, 180, 60);
    lv_obj_align(btn_deny, LV_ALIGN_BOTTOM_LEFT, 30, -40);
    lv_obj_set_style_bg_color(btn_deny, COLOR_RED, 0);
    lv_obj_set_style_bg_opa(btn_deny, LV_OPA_30, 0);
    lv_obj_set_style_border_color(btn_deny, COLOR_RED, 0);
    lv_obj_set_style_border_width(btn_deny, 2, 0);
    lv_obj_set_style_radius(btn_deny, 12, 0);
    lv_obj_add_flag(btn_deny, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_deny, btn_deny_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *deny_label = lv_label_create(btn_deny);
    lv_label_set_text(deny_label, "DENY");
    lv_obj_center(deny_label);
    lv_obj_set_style_text_color(deny_label, COLOR_RED, 0);
    lv_obj_set_style_text_font(deny_label, &lv_font_montserrat_24, 0);
}

static void show_screen(lv_obj_t *screen)
{
    if (active_screen != screen) {
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
        active_screen = screen;
    }
}

void ui_init(void)
{
    create_guide_screen();
    create_dashboard_screen();
    create_approval_screen();

    /* Small touch indicator on top layer */
    extern lv_obj_t *touch_indicator;
    touch_indicator = lv_obj_create(lv_layer_top());
    lv_obj_set_size(touch_indicator, 12, 12);
    lv_obj_align(touch_indicator, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(touch_indicator, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(touch_indicator, LV_OPA_30, 0);
    lv_obj_set_style_border_width(touch_indicator, 0, 0);
    lv_obj_set_style_radius(touch_indicator, LV_RADIUS_CIRCLE, 0);

    lv_scr_load(guide_screen);
    active_screen = guide_screen;
    ESP_LOGI(TAG, "UI screens initialized");
}

void ui_update_telemetry(const ble_telemetry_data_t *data)
{
    if (!data->updated) return;

    if (data->state != current_state) {
        current_state = data->state;
        switch (data->state) {
            case BLE_STATE_DISCONNECTED:
                show_screen(guide_screen);
                break;
            case BLE_STATE_IDLE:
            case BLE_STATE_WORKING:
                show_screen(dashboard_screen);
                break;
            case BLE_STATE_WAIT_APPROVAL:
                show_screen(approval_screen);
                lv_label_set_text(approval_cmd_label, data->active_tool);
                break;
            case BLE_STATE_WAIT_QUESTION:
                show_screen(dashboard_screen);
                break;
        }
    }

    if (current_state == BLE_STATE_DISCONNECTED) {
        return;
    }

    const char *state_text = "IDLE";
    lv_color_t state_color = COLOR_GREEN;
    lv_color_t arc_color = COLOR_GREEN;

    switch (data->state) {
        case BLE_STATE_IDLE:
            state_text = "IDLE";
            state_color = COLOR_GREEN;
            arc_color = COLOR_GREEN;
            break;
        case BLE_STATE_WORKING:
            state_text = "WORKING";
            state_color = COLOR_CYAN;
            arc_color = COLOR_CYAN;
            break;
        case BLE_STATE_WAIT_APPROVAL:
            state_text = "WAITING";
            state_color = COLOR_AMBER;
            arc_color = COLOR_AMBER;
            break;
        case BLE_STATE_WAIT_QUESTION:
            state_text = "QUESTION";
            state_color = COLOR_AMBER;
            arc_color = COLOR_AMBER;
            break;
        default:
            state_text = "IDLE";
            state_color = COLOR_GREEN;
            arc_color = COLOR_GREEN;
            break;
    }

    lv_label_set_text(dashboard_state_label, state_text);
    lv_obj_set_style_text_color(dashboard_state_label, state_color, 0);

    lv_label_set_text(dashboard_agent_label, data->agent_name);
    lv_obj_set_style_text_color(dashboard_agent_label, state_color, 0);

    lv_label_set_text(dashboard_task_label, data->active_tool);
    lv_label_set_text(dashboard_tool_label, data->active_tool);
    lv_label_set_text(dashboard_workspace_label, data->workspace);

    lv_arc_set_value(dashboard_arc, data->progress_current);
    lv_obj_set_style_arc_color(dashboard_arc, arc_color, LV_PART_INDICATOR);
}
