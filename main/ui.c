#include "sdkconfig.h"
#include "ui.h"
#include "ble_server.h"
#include "audio.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
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

/* AMOLED color palette (matching Stitch design system) */
#define COLOR_BG         lv_color_hex(0x000000) // OLED Pure Black
#define COLOR_CARD       lv_color_hex(0x121212) // Level 1 Card Container
#define COLOR_TEXT       lv_color_hex(0xE2E2E2) // Active main text (on-surface)
#define COLOR_TEXT_SEC   lv_color_hex(0xCCC3D8) // Secondary subtext (on-surface-variant)
#define COLOR_CYAN       lv_color_hex(0x00F0FF) // Nexus Blue (Secondary Accent)
#define COLOR_AMBER      lv_color_hex(0xFF4500) // Nexus Orange (Primary Accent)
#define COLOR_GREEN      lv_color_hex(0x10B981)
#define COLOR_RED        lv_color_hex(0xEF4444)

extern const lv_font_t lv_font_montserrat_14;
extern const lv_font_t lv_font_montserrat_18;
extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t lv_font_montserrat_36;
#if CONFIG_LV_FONT_MONTSERRAT_48
extern const lv_font_t lv_font_montserrat_48;
#else
#define lv_font_montserrat_48 lv_font_montserrat_36
#endif

static lv_obj_t *guide_screen = NULL;
static lv_obj_t *dashboard_screen = NULL;
static lv_obj_t *approval_screen = NULL;

static lv_obj_t *approval_cmd_label = NULL;

/* Dashboard sub-views and widgets */
static lv_obj_t *orange_card = NULL;
static lv_obj_t *empty_layout = NULL;
static lv_obj_t *task_layout = NULL;
static lv_obj_t *section_header_lbl = NULL;
static lv_obj_t *footer_progress_lbl = NULL;
static lv_obj_t *footer_progress_pct_lbl = NULL;
static lv_obj_t *footer_time_lbl = NULL;
static lv_obj_t *footer_time_cnt = NULL;

typedef struct {
    lv_obj_t *row_cnt;
    lv_obj_t *name_lbl;
    lv_obj_t *id_lbl;
    lv_obj_t *time_lbl;
    lv_obj_t *status_lbl;
    lv_obj_t *divider;
} task_row_t;

static task_row_t task_rows[3];

typedef struct {
    lv_obj_t *date_lbl;
    lv_obj_t *clock_lbl;
    lv_obj_t *secs_lbl;
    lv_obj_t *wifi_icon;
    lv_obj_t *bt_icon;
} time_header_t;

static time_header_t header_guide;
static time_header_t header_dash;
static time_header_t header_approval;

static ble_monitor_state_t current_state = BLE_STATE_DISCONNECTED;
static lv_obj_t *active_screen = NULL;

/* Common Top Time & Date Header Builder */
static void create_time_header(lv_obj_t *parent, time_header_t *header)
{
    lv_obj_t *cnt = lv_obj_create(parent);
    lv_obj_set_size(cnt, 432, 100);
    lv_obj_align(cnt, LV_ALIGN_TOP_MID, 0, 24); // 24px safe area offset
    lv_obj_set_style_bg_opa(cnt, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cnt, 0, 0);
    lv_obj_set_style_pad_all(cnt, 0, 0);
    lv_obj_remove_flag(cnt, LV_OBJ_FLAG_SCROLLABLE);
    
    // Date Label on the top-left
    header->date_lbl = lv_label_create(cnt);
    lv_label_set_text(header->date_lbl, "WAITING SYNC");
    lv_obj_set_style_text_color(header->date_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(header->date_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(header->date_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Icons container on the top-right
    lv_obj_t *icons_cnt = lv_obj_create(cnt);
    lv_obj_set_size(icons_cnt, 80, 30);
    lv_obj_align(icons_cnt, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(icons_cnt, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icons_cnt, 0, 0);
    lv_obj_set_style_pad_all(icons_cnt, 0, 0);
    lv_obj_set_flex_flow(icons_cnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icons_cnt, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(icons_cnt, 12, 0);
    lv_obj_remove_flag(icons_cnt, LV_OBJ_FLAG_SCROLLABLE);
    
    header->wifi_icon = lv_label_create(icons_cnt);
    lv_label_set_text(header->wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(header->wifi_icon, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(header->wifi_icon, &lv_font_montserrat_18, 0);
    
    header->bt_icon = lv_label_create(icons_cnt);
    lv_label_set_text(header->bt_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(header->bt_icon, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(header->bt_icon, &lv_font_montserrat_18, 0);
    
    // Clock label on the bottom-left
    header->clock_lbl = lv_label_create(cnt);
    lv_label_set_text(header->clock_lbl, "--:--");
    lv_obj_set_style_text_color(header->clock_lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(header->clock_lbl, &lv_font_montserrat_48, 0);
    lv_obj_align(header->clock_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // Seconds label next to clock
    header->secs_lbl = lv_label_create(cnt);
    lv_label_set_text(header->secs_lbl, "--");
    lv_obj_set_style_text_color(header->secs_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(header->secs_lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(header->secs_lbl, LV_ALIGN_BOTTOM_LEFT, 136, -8);
}

/* 250ms System Clock and Spinner Update Timer callback */
static void clock_timer_cb(lv_timer_t *timer)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char date_str[32];
    char time_str[16];
    char secs_str[8];
    
    if (timeinfo.tm_year < 71) {
        strcpy(date_str, "WAITING SYNC");
        strcpy(time_str, "--:--");
        strcpy(secs_str, "--");
    } else {
        const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        int wday = timeinfo.tm_wday;
        if (wday < 0 || wday > 6) wday = 0;
        snprintf(date_str, sizeof(date_str), "%s %d", weekdays[wday], timeinfo.tm_mday);
        snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        snprintf(secs_str, sizeof(secs_str), "%02d", timeinfo.tm_sec);
    }
    
    if (header_guide.date_lbl) {
        lv_label_set_text(header_guide.date_lbl, date_str);
        lv_label_set_text(header_guide.clock_lbl, time_str);
        lv_label_set_text(header_guide.secs_lbl, secs_str);
    }
    if (header_dash.date_lbl) {
        lv_label_set_text(header_dash.date_lbl, date_str);
        lv_label_set_text(header_dash.clock_lbl, time_str);
        lv_label_set_text(header_dash.secs_lbl, secs_str);
    }
    if (header_approval.date_lbl) {
        lv_label_set_text(header_approval.date_lbl, date_str);
        lv_label_set_text(header_approval.clock_lbl, time_str);
        lv_label_set_text(header_approval.secs_lbl, secs_str);
    }
    
    // Cycle text status spinner character for any active "working" tasks
    static int spin_frame = 0;
    const char spin_chars[] = {'|', '/', '-', '\\'};
    char spin_buf[2] = {spin_chars[spin_frame], '\0'};
    spin_frame = (spin_frame + 1) % 4;
    
    for (int i = 0; i < 3; i++) {
        if (task_rows[i].status_lbl) {
            const char *txt = lv_label_get_text(task_rows[i].status_lbl);
            if (txt && strlen(txt) == 1 && 
                (txt[0] == '|' || txt[0] == '/' || txt[0] == '-' || txt[0] == '\\')) {
                lv_label_set_text(task_rows[i].status_lbl, spin_buf);
            }
        }
    }
}

static void create_guide_screen(void)
{
    guide_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(guide_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(guide_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(guide_screen, 0, 0);
    lv_obj_set_style_pad_all(guide_screen, 0, 0);
    lv_obj_remove_flag(guide_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Setup time header
    create_time_header(guide_screen, &header_guide);
    lv_obj_set_style_text_color(header_guide.bt_icon, COLOR_TEXT_SEC, 0); // Bluetooth grayed out
    
    // Solid orange card with black content (matching Empty State design)
    lv_obj_t *guide_card = lv_obj_create(guide_screen);
    lv_obj_set_size(guide_card, 416, 220); // Expanded card size
    lv_obj_align(guide_card, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(guide_card, COLOR_AMBER, 0);
    lv_obj_set_style_border_width(guide_card, 0, 0);
    lv_obj_set_style_radius(guide_card, 32, 0); // rounded-3xl
    lv_obj_set_style_pad_all(guide_card, 20, 0);
    lv_obj_set_flex_flow(guide_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(guide_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(guide_card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *bt_icon = lv_label_create(guide_card);
    lv_label_set_text(bt_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(bt_icon, COLOR_BG, 0); // Black icon
    lv_obj_set_style_text_font(bt_icon, &lv_font_montserrat_48, 0);
    
    lv_obj_t *guide_lbl = lv_label_create(guide_card);
    lv_label_set_text(guide_lbl, "WAITING FOR CONNECTION...");
    lv_obj_set_style_text_color(guide_lbl, COLOR_BG, 0); // Black text
    lv_obj_set_style_text_font(guide_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(guide_lbl, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t *name_lbl = lv_label_create(guide_card);
    lv_label_set_text(name_lbl, "Device: Buddy-AMOLED");
    lv_obj_set_style_text_color(name_lbl, COLOR_BG, 0); // Black monospace style text
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_opa(name_lbl, LV_OPA_80, 0);
}

static void create_dashboard_screen(void)
{
    dashboard_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(dashboard_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(dashboard_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dashboard_screen, 0, 0);
    lv_obj_set_style_pad_all(dashboard_screen, 0, 0);
    lv_obj_remove_flag(dashboard_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Setup time header
    create_time_header(dashboard_screen, &header_dash);
    
    // Section Header label ("CURRENT TASK (X)")
    section_header_lbl = lv_label_create(dashboard_screen);
    lv_label_set_text(section_header_lbl, "CURRENT TASK (0)");
    lv_obj_set_style_text_color(section_header_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(section_header_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(section_header_lbl, LV_ALIGN_TOP_LEFT, 32, 134);
    
    // The unified main Orange Card (bg-nexus-orange) - Expanded size to 220px height
    orange_card = lv_obj_create(dashboard_screen);
    lv_obj_set_size(orange_card, 416, 220);
    lv_obj_align(orange_card, LV_ALIGN_TOP_MID, 0, 156);
    lv_obj_set_style_bg_color(orange_card, COLOR_AMBER, 0);
    lv_obj_set_style_border_width(orange_card, 0, 0);
    lv_obj_set_style_radius(orange_card, 32, 0); // rounded-3xl
    lv_obj_set_style_pad_hor(orange_card, 20, 0);
    lv_obj_set_style_pad_ver(orange_card, 10, 0); // slightly more vertical padding
    lv_obj_remove_flag(orange_card, LV_OBJ_FLAG_SCROLLABLE);
    
    // 2.1 Empty State child layout (transparent background, black text) - Expanded height
    empty_layout = lv_obj_create(orange_card);
    lv_obj_set_size(empty_layout, 376, 200);
    lv_obj_center(empty_layout);
    lv_obj_set_style_bg_opa(empty_layout, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(empty_layout, 0, 0);
    lv_obj_set_style_pad_all(empty_layout, 0, 0);
    lv_obj_set_flex_flow(empty_layout, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(empty_layout, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(empty_layout, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *empty_icon = lv_label_create(empty_layout);
    lv_label_set_text(empty_icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(empty_icon, COLOR_BG, 0); // Black icon
    lv_obj_set_style_text_font(empty_icon, &lv_font_montserrat_24, 0);
    
    lv_obj_t *empty_title = lv_label_create(empty_layout);
    lv_label_set_text(empty_title, "No Active Tasks");
    lv_obj_set_style_text_color(empty_title, COLOR_BG, 0); // Black text
    lv_obj_set_style_text_font(empty_title, &lv_font_montserrat_18, 0);
    
    lv_obj_t *empty_desc = lv_label_create(empty_layout);
    lv_label_set_text(empty_desc, "SYSTEM_IDLE: 000-0");
    lv_obj_set_style_text_color(empty_desc, COLOR_BG, 0);
    lv_obj_set_style_text_font(empty_desc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_opa(empty_desc, LV_OPA_70, 0); // Black with 70% opacity
    
    // 2.2 Refined list child layout (transparent background, black text) - Expanded height
    task_layout = lv_obj_create(orange_card);
    lv_obj_set_size(task_layout, 376, 200);
    lv_obj_center(task_layout);
    lv_obj_set_style_bg_opa(task_layout, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(task_layout, 0, 0);
    lv_obj_set_style_pad_all(task_layout, 0, 0);
    lv_obj_set_flex_flow(task_layout, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(task_layout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(task_layout, 6, 0); // increased row gap
    lv_obj_remove_flag(task_layout, LV_OBJ_FLAG_SCROLLABLE);
    
    // Pre-create 3 task rows exactly inside task_layout - Expanded individual row height to 58px
    for (int i = 0; i < 3; i++) {
        task_rows[i].row_cnt = lv_obj_create(task_layout);
        lv_obj_set_size(task_rows[i].row_cnt, 376, 54);
        lv_obj_set_style_bg_opa(task_rows[i].row_cnt, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(task_rows[i].row_cnt, 0, 0);
        lv_obj_set_style_pad_all(task_rows[i].row_cnt, 0, 0);
        lv_obj_remove_flag(task_rows[i].row_cnt, LV_OBJ_FLAG_SCROLLABLE);
        
        // Top line labels: TASK_ID (left), remaining time (right)
        task_rows[i].id_lbl = lv_label_create(task_rows[i].row_cnt);
        lv_label_set_text(task_rows[i].id_lbl, "TASK_ID: ---");
        lv_obj_set_style_text_color(task_rows[i].id_lbl, COLOR_BG, 0);
        lv_obj_set_style_text_font(task_rows[i].id_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_opa(task_rows[i].id_lbl, LV_OPA_70, 0); // 70% opacity
        lv_obj_align(task_rows[i].id_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
        
        task_rows[i].time_lbl = lv_label_create(task_rows[i].row_cnt);
        lv_label_set_text(task_rows[i].time_lbl, "00:00 REM");
        lv_obj_set_style_text_color(task_rows[i].time_lbl, COLOR_BG, 0);
        lv_obj_set_style_text_font(task_rows[i].time_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(task_rows[i].time_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);
        
        // Bottom line labels: Name (left), status icon (right)
        task_rows[i].name_lbl = lv_label_create(task_rows[i].row_cnt);
        lv_label_set_text(task_rows[i].name_lbl, "Task Name");
        lv_obj_set_style_text_color(task_rows[i].name_lbl, COLOR_BG, 0);
        lv_obj_set_style_text_font(task_rows[i].name_lbl, &lv_font_montserrat_18, 0);
        lv_obj_align(task_rows[i].name_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        
        task_rows[i].status_lbl = lv_label_create(task_rows[i].row_cnt);
        lv_label_set_text(task_rows[i].status_lbl, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(task_rows[i].status_lbl, COLOR_BG, 0);
        lv_obj_set_style_text_font(task_rows[i].status_lbl, &lv_font_montserrat_18, 0);
        lv_obj_align(task_rows[i].status_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        
        // Horizontal line separator (divider)
        task_rows[i].divider = lv_obj_create(task_layout);
        lv_obj_set_size(task_rows[i].divider, 376, 1);
        lv_obj_set_style_bg_color(task_rows[i].divider, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(task_rows[i].divider, LV_OPA_10, 0); // 10% opacity line
        lv_obj_set_style_border_width(task_rows[i].divider, 0, 0);
        lv_obj_set_style_pad_all(task_rows[i].divider, 0, 0);
        lv_obj_remove_flag(task_rows[i].divider, LV_OBJ_FLAG_SCROLLABLE);
    }
    
    // Bottom Footer
    footer_progress_lbl = lv_label_create(dashboard_screen);
    lv_label_set_text(footer_progress_lbl, "0");
    lv_obj_set_style_text_color(footer_progress_lbl, COLOR_TEXT, 0);
    // Adjusted progress number size to Montserrat-36 to scale correctly with clock
    lv_obj_set_style_text_font(footer_progress_lbl, &lv_font_montserrat_36, 0);
    lv_obj_align(footer_progress_lbl, LV_ALIGN_BOTTOM_LEFT, 32, -24);
    
    // Separate smaller '%' symbol label
    footer_progress_pct_lbl = lv_label_create(dashboard_screen);
    lv_label_set_text(footer_progress_pct_lbl, "%");
    lv_obj_set_style_text_color(footer_progress_pct_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(footer_progress_pct_lbl, &lv_font_montserrat_18, 0);
    // Position it dynamically next to the number
    lv_obj_align_to(footer_progress_pct_lbl, footer_progress_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -5);
    
    footer_time_cnt = lv_obj_create(dashboard_screen);
    lv_obj_set_size(footer_time_cnt, 110, 24);
    lv_obj_align(footer_time_cnt, LV_ALIGN_BOTTOM_RIGHT, -32, -28);
    lv_obj_set_style_bg_color(footer_time_cnt, COLOR_AMBER, 0); // Solid orange
    lv_obj_set_style_border_width(footer_time_cnt, 0, 0);
    lv_obj_set_style_radius(footer_time_cnt, 4, 0); // slightly rounded
    lv_obj_set_style_pad_all(footer_time_cnt, 0, 0);
    lv_obj_remove_flag(footer_time_cnt, LV_OBJ_FLAG_SCROLLABLE);
    
    footer_time_lbl = lv_label_create(footer_time_cnt);
    lv_label_set_text(footer_time_lbl, "--/-- --:--");
    lv_obj_set_style_text_color(footer_time_lbl, COLOR_BG, 0); // Black time text
    lv_obj_set_style_text_font(footer_time_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(footer_time_lbl);
}

static void create_approval_screen(void)
{
    approval_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(approval_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(approval_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(approval_screen, 0, 0);
    lv_obj_set_style_pad_all(approval_screen, 0, 0);
    lv_obj_remove_flag(approval_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Setup time header
    create_time_header(approval_screen, &header_approval);
    
    lv_obj_t *title = lv_label_create(approval_screen);
    lv_label_set_text(title, "CONFIRM COMMAND?");
    lv_obj_set_style_text_color(title, COLOR_AMBER, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 96);
    
    approval_cmd_label = lv_label_create(approval_screen);
    lv_label_set_text(approval_cmd_label, "No command pending");
    lv_obj_set_style_text_color(approval_cmd_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(approval_cmd_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(approval_cmd_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(approval_cmd_label, 440);
    lv_obj_align(approval_cmd_label, LV_ALIGN_CENTER, 0, 0);
    
    // Approve button (solid green, black text)
    lv_obj_t *btn_approve = lv_obj_create(approval_screen);
    lv_obj_set_size(btn_approve, 160, 50);
    lv_obj_align(btn_approve, LV_ALIGN_BOTTOM_RIGHT, -44, -50);
    lv_obj_set_style_bg_color(btn_approve, COLOR_GREEN, 0);
    lv_obj_set_style_bg_opa(btn_approve, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_approve, 0, 0);
    lv_obj_set_style_radius(btn_approve, 12, 0);
    lv_obj_add_flag(btn_approve, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_approve, btn_approve_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_remove_flag(btn_approve, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *approve_label = lv_label_create(btn_approve);
    lv_label_set_text(approve_label, "APPROVE");
    lv_obj_center(approve_label);
    lv_obj_set_style_text_color(approve_label, COLOR_BG, 0); // Black text
    lv_obj_set_style_text_font(approve_label, &lv_font_montserrat_18, 0);
    
    // Deny button (solid red, black text)
    lv_obj_t *btn_deny = lv_obj_create(approval_screen);
    lv_obj_set_size(btn_deny, 160, 50);
    lv_obj_align(btn_deny, LV_ALIGN_BOTTOM_LEFT, 44, -50);
    lv_obj_set_style_bg_color(btn_deny, COLOR_RED, 0);
    lv_obj_set_style_bg_opa(btn_deny, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_deny, 0, 0);
    lv_obj_set_style_radius(btn_deny, 12, 0);
    lv_obj_add_flag(btn_deny, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_deny, btn_deny_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_remove_flag(btn_deny, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *deny_label = lv_label_create(btn_deny);
    lv_label_set_text(deny_label, "DENY");
    lv_obj_center(deny_label);
    lv_obj_set_style_text_color(deny_label, COLOR_BG, 0); // Black text
    lv_obj_set_style_text_font(deny_label, &lv_font_montserrat_18, 0);
}

static void show_screen(lv_obj_t *screen)
{
    if (active_screen != screen) {
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
        active_screen = screen;
    }
}

void ui_init(void)
{
    create_guide_screen();
    create_dashboard_screen();
    create_approval_screen();
    
    // Create 250ms interval system clock and spinner timer
    lv_timer_create(clock_timer_cb, 250, NULL);
    
    lv_scr_load(guide_screen);
    active_screen = guide_screen;
    ESP_LOGI(TAG, "Stitch UI screens initialized");
}

void ui_update_telemetry(const ble_telemetry_data_t *data)
{
    if (!data->updated) return;
    
    // 1. Monitor State to screen transition
    if (data->state != current_state) {
        current_state = data->state;
        switch (data->state) {
            case BLE_STATE_DISCONNECTED:
                show_screen(guide_screen);
                break;
            case BLE_STATE_IDLE:
            case BLE_STATE_WORKING:
            case BLE_STATE_WAIT_QUESTION:
                show_screen(dashboard_screen);
                break;
            case BLE_STATE_WAIT_APPROVAL:
                show_screen(approval_screen);
                lv_label_set_text(approval_cmd_label, data->active_tool);
                break;
        }
    }
    
    if (current_state == BLE_STATE_DISCONNECTED) {
        return;
    }
    
    // Update Bluetooth status icon in top-right header (connected = Cyan)
    lv_obj_set_style_text_color(header_dash.bt_icon, COLOR_CYAN, 0);
    lv_obj_set_style_text_color(header_approval.bt_icon, COLOR_CYAN, 0);
    
    // 2. Parse active_tool string thread-safely into up to 3 task rows
    char task_str[256];
    strncpy(task_str, data->active_tool, sizeof(task_str) - 1);
    task_str[sizeof(task_str) - 1] = '\0';
    
    int task_idx = 0;
    
    // Only attempt to parse if we are in WORKING state and the string is not empty or default "None"
    if (data->state == BLE_STATE_WORKING && 
        strlen(task_str) > 0 && 
        strcmp(task_str, "None") != 0 && 
        strcmp(task_str, "idle") != 0) {
        
        char *task_saveptr = NULL;
        char *task_token = strtok_r(task_str, "|", &task_saveptr);
        
        while (task_token != NULL && task_idx < 3) {
            char *field_saveptr = NULL;
            char *name = strtok_r(task_token, ",", &field_saveptr);
            char *id = strtok_r(NULL, ",", &field_saveptr);
            char *time_str = strtok_r(NULL, ",", &field_saveptr);
            char *status = strtok_r(NULL, ",", &field_saveptr);
            
            // Enforce Null Guards
            if (!name) name = "Unknown Task";
            if (!id) id = "---";
            if (!time_str) time_str = "00:00";
            if (!status) status = "idle";
            
            // Apply text parameters
            lv_label_set_text(task_rows[task_idx].name_lbl, name);
            
            char id_buf[32];
            snprintf(id_buf, sizeof(id_buf), "TASK_ID: %s", id);
            lv_label_set_text(task_rows[task_idx].id_lbl, id_buf);
            
            char time_buf[32];
            snprintf(time_buf, sizeof(time_buf), "%s REM", time_str);
            lv_label_set_text(task_rows[task_idx].time_lbl, time_buf);
            
            // Status Icon/Label formatting (using built-in symbols with check_circle and pause_circle metaphors)
            if (strcmp(status, "working") == 0) {
                lv_label_set_text(task_rows[task_idx].status_lbl, "|"); // Starts with spinner character
            } else if (strcmp(status, "done") == 0) {
                lv_label_set_text(task_rows[task_idx].status_lbl, LV_SYMBOL_OK);
            } else if (strcmp(status, "paused") == 0) {
                lv_label_set_text(task_rows[task_idx].status_lbl, LV_SYMBOL_PAUSE);
            } else {
                lv_label_set_text(task_rows[task_idx].status_lbl, LV_SYMBOL_PLAY);
            }
            
            // Show this task slot and divider
            lv_obj_remove_flag(task_rows[task_idx].row_cnt, LV_OBJ_FLAG_HIDDEN);
            if (task_idx < 2) {
                lv_obj_remove_flag(task_rows[task_idx].divider, LV_OBJ_FLAG_HIDDEN);
            }
            
            task_idx++;
            task_token = strtok_r(NULL, "|", &task_saveptr);
        }
    }
    
    // Hide unused task slots and dividers
    for (int i = task_idx; i < 3; i++) {
        lv_obj_add_flag(task_rows[i].row_cnt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(task_rows[i].divider, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 3. Switch dashboard empty vs refined task layout
    if (task_idx > 0) {
        lv_obj_add_flag(empty_layout, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(task_layout, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(empty_layout, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(task_layout, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Update the Section Header text (e.g., "CURRENT TASK (3)")
    char section_hdr[32];
    snprintf(section_hdr, sizeof(section_hdr), "CURRENT TASK (%d)", task_idx);
    lv_label_set_text(section_header_lbl, section_hdr);
    
    // 4. Update Footer Percentage (Number and PCT symbol split)
    uint8_t progress = data->progress_current;
    if (progress > 100) progress = 100;
    
    char progress_buf[16];
    snprintf(progress_buf, sizeof(progress_buf), "%d", progress);
    lv_label_set_text(footer_progress_lbl, progress_buf);
    
    // Force immediate size update of the number label so that the subsequent align call
    // uses the newly rendered text size (prevents overlapping/colliding characters).
    lv_obj_update_layout(footer_progress_lbl);
    
    // Dynamic re-align to keep '%' symbol snapped to the right edge of the shifting numbers
    lv_obj_align_to(footer_progress_pct_lbl, footer_progress_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -5);
    
    // 5. Update last updated time badge from local synchronized RTC
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year >= 71) {
        char time_badge[32];
        snprintf(time_badge, sizeof(time_badge), "%02d/%02d %02d:%02d", 
                 timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
        lv_label_set_text(footer_time_lbl, time_badge);
    } else {
        lv_label_set_text(footer_time_lbl, "No Sync");
    }
}
