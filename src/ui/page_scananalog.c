#include "page_scananalog.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <log/log.h>
#include <minIni.h>

#include "core/app_state.h"
#include "core/common.hh"
#include "core/defines.h"
#include "core/osd.h"
#include "core/settings.h"
#include "driver/fbtools.h"
#include "driver/hardware.h"
#include "driver/i2c.h"
#include "driver/rtc6715.h"
#include "lang/language.h"
#include "ui/page_common.h"
#include "ui/page_scannow.h"
#include "ui/ui_main_menu.h"
#include "ui/ui_porting.h"
#include "ui/ui_style.h"

LV_IMG_DECLARE(img_signal_status);
LV_IMG_DECLARE(img_signal_status2);
LV_IMG_DECLARE(img_signal_status3);
LV_IMG_DECLARE(img_ant1);
LV_IMG_DECLARE(img_ant2);
LV_IMG_DECLARE(img_ant3);
LV_IMG_DECLARE(img_ant4);
LV_IMG_DECLARE(img_ant5);
LV_IMG_DECLARE(img_ant6);
LV_IMG_DECLARE(img_ant7);
LV_IMG_DECLARE(img_ant8);
LV_IMG_DECLARE(img_ant9);
LV_IMG_DECLARE(img_ant10);
LV_IMG_DECLARE(img_ant11);
LV_IMG_DECLARE(img_ant12);
LV_IMG_DECLARE(img_ant13);
LV_IMG_DECLARE(img_ant14);

typedef struct {
    bool is_valid;
    int gain;
} analog_channel_status_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *img0;
    lv_obj_t *label;
    lv_obj_t *img1;
} analog_channel_t;

static analog_channel_t analog_channel_tb[8];
static analog_channel_status_t analog_status_tb[ANALOG_CHANNEL_NUM];

static lv_obj_t *analog_progressbar;
static lv_obj_t *analog_label;
static lv_obj_t *analog_list;
static lv_obj_t *analog_band_label;
static lv_obj_t *analog_found_label;
static int analog_current_band = 0;
static int analog_user_select_index = -1;
static lv_coord_t analog_col_dsc1[] = {UI_SCANNOW_SCANNER_COLS};
static lv_coord_t analog_row_dsc1[] = {UI_SCANNOW_SCANNER_ROWS};
static lv_coord_t analog_col_dsc2[] = {UI_SCANNOW_SIGNAL_COLS};
static lv_coord_t analog_row_dsc2[] = {UI_SCANNOW_SIGNAL_ROWS};
static const char *analog_band_names[] = {"A", "B", "E", "F", "R", "L"};
static const int analog_band_count = 6;

static void analog_select_signal(int idx_in_band) {
    for (int i = 0; i < 8; i++) {
        lv_img_set_src(analog_channel_tb[i].img0, &img_signal_status);
        int channel_idx = analog_current_band * 8 + i;
        if (analog_status_tb[channel_idx].is_valid) {
            lv_img_set_src(analog_channel_tb[i].img0, &img_signal_status2);
        }
    }
    if (idx_in_band >= 0 && idx_in_band < 8)
        lv_img_set_src(analog_channel_tb[idx_in_band].img0, &img_signal_status3);
}

static void analog_set_signal_bar(analog_channel_t *channel, bool is_valid, int gain) {
    if (!is_valid) {
        lv_img_set_src(channel->img0, &img_signal_status);

        if (gain < 5) {
            lv_img_set_src(channel->img1, &img_ant1);
        } else if (gain < 10) {
            lv_img_set_src(channel->img1, &img_ant3);
        } else if (gain < 15) {
            lv_img_set_src(channel->img1, &img_ant4);
        } else if (gain < 16) {
            lv_img_set_src(channel->img1, &img_ant5);
        } else if (gain < 20) {
            lv_img_set_src(channel->img1, &img_ant6);
        } else if (gain < 30) {
            lv_img_set_src(channel->img1, &img_ant7);
        } else if (gain <= 77) {
            lv_img_set_src(channel->img1, &img_ant7);
        } else {
            lv_img_set_src(channel->img1, &img_ant1);
        }

    } else {
        lv_img_set_src(channel->img0, &img_signal_status2);
        if (gain < 5) {
            lv_img_set_src(channel->img1, &img_ant8);
        } else if (gain < 10) {
            lv_img_set_src(channel->img1, &img_ant10);
        } else if (gain < 15) {
            lv_img_set_src(channel->img1, &img_ant11);
        } else if (gain < 16) {
            lv_img_set_src(channel->img1, &img_ant12);
        } else if (gain < 20) {
            lv_img_set_src(channel->img1, &img_ant13);
        } else if (gain < 30) {
            lv_img_set_src(channel->img1, &img_ant14);
        } else if (gain <= 77) {
            lv_img_set_src(channel->img1, &img_ant14);
        } else {
            lv_img_set_src(channel->img1, &img_ant8);
        }
    }
}

static void create_analog_channel_switch(lv_obj_t *parent, int col, int row, analog_channel_t *channel, const char *name) {
    channel->img0 = lv_img_create(parent);
    lv_img_set_src(channel->img0, &img_signal_status);
    lv_obj_set_size(channel->img0, img_signal_status.header.w, img_signal_status.header.h);
    lv_obj_set_grid_cell(channel->img0, LV_GRID_ALIGN_START, col, 1,
                         LV_GRID_ALIGN_CENTER, row, 1);

    channel->label = lv_label_create(parent);
    lv_obj_set_style_text_font(channel->label, UI_SCANNOW_CHAN_FONT, 0);
    lv_obj_set_style_text_align(channel->label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(channel->label, lv_color_hex(TEXT_COLOR_DEFAULT), 0);
    lv_obj_set_style_pad_top(channel->label, UI_SCANNOW_CHAN_PAD, 0);
    lv_label_set_long_mode(channel->label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(channel->label, name);
    lv_obj_set_grid_cell(channel->label, LV_GRID_ALIGN_START, col + 1, 1,
                         LV_GRID_ALIGN_CENTER, row, 1);

    channel->img1 = lv_img_create(parent);
    lv_img_set_src(channel->img1, &img_ant1);
    lv_obj_set_size(channel->img1, img_ant1.header.w, img_ant1.header.h);
    lv_obj_set_grid_cell(channel->img1, LV_GRID_ALIGN_START, col + 2, 1,
                         LV_GRID_ALIGN_CENTER, row, 1);
}

static void analog_update_band_ui(void) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%s", analog_band_names[analog_current_band]);
    lv_label_set_text(analog_band_label, buf);

    for (int i = 0; i < 8; i++) {
        int ch_idx = analog_current_band * 8 + i;
        const char *name = channel2str(0, 0, ch_idx + 1);
        lv_label_set_text(analog_channel_tb[i].label, name);
        analog_set_signal_bar(&analog_channel_tb[i], analog_status_tb[ch_idx].is_valid, analog_status_tb[ch_idx].gain);
    }
}

static void user_select_first_valid(void) {
    analog_user_select_index = -1;
    for (int i = 0; i < 8; i++) {
        int ch_idx = analog_current_band * 8 + i;
        if (analog_status_tb[ch_idx].is_valid) {
            analog_user_select_index = i;
            break;
        }
    }
    if (analog_user_select_index >= 0)
        analog_select_signal(analog_user_select_index);
    else
        analog_select_signal(-1);
}

static void user_clear_signal(void) {
    analog_user_select_index = -1;
    for (int i = 0; i < 8; i++) {
        lv_img_set_src(analog_channel_tb[i].img0, &img_signal_status);
        lv_img_set_src(analog_channel_tb[i].img1, &img_ant1);
    }
}

static void analog_change_band(int delta) {
    analog_current_band += delta;
    if (analog_current_band < 0)
        analog_current_band += analog_band_count;
    if (analog_current_band >= analog_band_count)
        analog_current_band -= analog_band_count;
    user_clear_signal();
    analog_update_band_ui();
    analog_user_select_index = 0;
    analog_select_signal(analog_user_select_index);
}

static void scan_channel_analog(uint8_t channel, uint8_t *gain_ret, bool *valid) {
    int rssi_volt_mv;

    rtc6715.set_ch(channel);
    usleep(120000);

    rssi_volt_mv = rtc6715.rssi;
    rssi_volt_mv -= 1600;
    rssi_volt_mv = (rssi_volt_mv < 0) ? 0 : rssi_volt_mv;
    rssi_volt_mv /= 6;
    if (rssi_volt_mv > 100)
        rssi_volt_mv = 100;

    *gain_ret = (uint8_t)rssi_volt_mv;
    *valid = rssi_volt_mv > 5;

    LOGI("Analog scan ch:%d valid:%d gain:%d", channel + 1, *valid, *gain_ret);
}

static int8_t scan_analog_now(void) {
    uint8_t gain;
    bool valid;
    uint8_t valid_index;
    char buf[128];
    int found_count = 0;

#ifdef EMULATOR_BUILD
    snprintf(buf, sizeof(buf), "%s...", _lang("Scanning"));
    lv_label_set_text(analog_label, buf);
    lv_bar_set_range(analog_progressbar, 0, ANALOG_CHANNEL_NUM + 6);
    lv_bar_set_value(analog_progressbar, 0, LV_ANIM_OFF);
    lv_timer_handler();

    for (int ch = 0; ch < ANALOG_CHANNEL_NUM; ch++) {
        analog_status_tb[ch].is_valid = 0;
        analog_status_tb[ch].gain = 0;
    }
    analog_update_band_ui();

    // Populate a few dummy channels for emulator use.
    int dummy_channels[] = {0, 7, 15, 23};
    for (size_t i = 0; i < sizeof(dummy_channels) / sizeof(dummy_channels[0]); i++) {
        int ch = dummy_channels[i];
        analog_status_tb[ch].is_valid = 1;
        analog_status_tb[ch].gain = 60 - (int)(i * 10);
    }

    analog_update_band_ui();
    lv_bar_set_value(analog_progressbar, ANALOG_CHANNEL_NUM + 5, LV_ANIM_OFF);
    analog_user_select_index = 0;
    analog_select_signal(analog_user_select_index);
    lv_label_set_text(analog_label, _lang("Scanning done"));
    snprintf(buf, sizeof(buf), "%s: %d", _lang("Found"), (int)(sizeof(dummy_channels) / sizeof(dummy_channels[0])));
    lv_label_set_text(analog_found_label, buf);
    return (int)(sizeof(dummy_channels) / sizeof(dummy_channels[0]));
#endif

    snprintf(buf, sizeof(buf), "%s...", _lang("Scanning"));
    lv_label_set_text(analog_label, buf);
    lv_bar_set_range(analog_progressbar, 0, ANALOG_CHANNEL_NUM + 6);
    lv_bar_set_value(analog_progressbar, 0, LV_ANIM_OFF);
    lv_timer_handler();

    // clear
    for (int ch = 0; ch < ANALOG_CHANNEL_NUM; ch++) {
        analog_status_tb[ch].is_valid = 0;
        analog_status_tb[ch].gain = 0;
    }
    analog_update_band_ui();

    for (int ch = 0; ch < ANALOG_CHANNEL_NUM; ch++) {
        scan_channel_analog(ch, &gain, &valid);
        if (valid) {
            analog_status_tb[ch].is_valid = 1;
            analog_status_tb[ch].gain = gain;
        }
        if ((ch / 8) == analog_current_band) {
            int idx = ch % 8;
            analog_set_signal_bar(&analog_channel_tb[idx], analog_status_tb[ch].is_valid, analog_status_tb[ch].gain);
        }
        lv_bar_set_value(analog_progressbar, ch + 5, LV_ANIM_OFF);
        lv_timer_handler();
    }
    lv_bar_set_value(analog_progressbar, ANALOG_CHANNEL_NUM + 5, LV_ANIM_OFF);

    valid_index = 0;
    for (int ch = 0; ch < ANALOG_CHANNEL_NUM; ch++) {
        if (analog_status_tb[ch].is_valid) {
            valid_index++;
            found_count++;
        }

        lv_timer_handler();
    }

    analog_update_band_ui();
    analog_user_select_index = 0;
    analog_select_signal(analog_user_select_index);
    lv_label_set_text(analog_label, _lang("Scanning done"));
    snprintf(buf, sizeof(buf), "%s: %d", _lang("Found"), found_count);
    lv_label_set_text(analog_found_label, buf);
    if (!valid_index)
        return -1;
    else
        return valid_index;
}

static lv_obj_t *page_scananalog_create(lv_obj_t *parent, panel_arr_t *arr) {
    char buf[256];

    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, UI_PAGE_VIEW_SIZE);
    lv_obj_add_style(page, &style_scan, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, UI_SCANNOW_PAGE_PAD, 0);

    lv_obj_t *cont1 = lv_obj_create(page);
    lv_obj_set_size(cont1, UI_SCANNOW_SCANNER_SIZE);
    lv_obj_set_layout(cont1, LV_LAYOUT_GRID);
    lv_obj_clear_flag(cont1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(cont1, &style_scan, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont1, 0, 0);
    lv_obj_set_style_grid_column_dsc_array(cont1, analog_col_dsc1, 0);
    lv_obj_set_style_grid_row_dsc_array(cont1, analog_row_dsc1, 0);

    analog_progressbar = lv_bar_create(cont1);
    lv_obj_set_size(analog_progressbar, UI_SCANNOW_PROG_BAR_SIZE);
    lv_obj_center(analog_progressbar);
    lv_bar_set_value(analog_progressbar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(analog_progressbar, lv_color_make(0xff, 0xff, 0xff), LV_PART_MAIN);
    lv_obj_set_style_radius(analog_progressbar, 0, LV_PART_MAIN);
#ifdef HDZBOXPRO
    lv_obj_set_style_bg_color(analog_progressbar, lv_color_make(0, 0x80, 0), LV_PART_INDICATOR);
#else
    lv_obj_set_style_bg_color(analog_progressbar, lv_color_make(0, 0xff, 0), LV_PART_INDICATOR);
#endif
    lv_obj_set_style_radius(analog_progressbar, 0, LV_PART_INDICATOR);

    lv_obj_set_grid_cell(analog_progressbar, LV_GRID_ALIGN_START, 0, 1,
                         LV_GRID_ALIGN_CENTER, 1, 1);

    lv_bar_set_range(analog_progressbar, 0, 14);

    analog_label = lv_label_create(cont1);
    lv_label_set_text(analog_label, _lang("Scan Ready"));
    lv_obj_set_style_text_font(analog_label, UI_SCANNOW_READY_FONT, 0);
    lv_obj_set_style_text_align(analog_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(analog_label, lv_color_hex(TEXT_COLOR_DEFAULT), 0);
    lv_obj_set_style_pad_top(analog_label, UI_SCANNOW_READY_PAD, 0);
    lv_label_set_long_mode(analog_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_grid_cell(analog_label, LV_GRID_ALIGN_START, 0, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    analog_band_label = lv_label_create(cont1);
    lv_label_set_text(analog_band_label, "A");
    lv_obj_set_style_text_font(analog_band_label, UI_SCANNOW_NOTE_FONT, 0);
    lv_obj_set_style_text_align(analog_band_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(analog_band_label, lv_color_hex(TEXT_COLOR_DEFAULT), 0);
    lv_obj_set_grid_cell(analog_band_label, LV_GRID_ALIGN_END, 2, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    analog_found_label = lv_label_create(cont1);
    lv_label_set_text(analog_found_label, "");
    lv_obj_set_style_text_font(analog_found_label, UI_SCANNOW_NOTE_FONT, 0);
    lv_obj_set_style_text_align(analog_found_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(analog_found_label, lv_color_hex(TEXT_COLOR_DEFAULT), 0);
    lv_obj_set_grid_cell(analog_found_label, LV_GRID_ALIGN_END, 2, 1,
                         LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_t *label2 = lv_label_create(cont1);
    snprintf(buf, sizeof(buf), "%s\n %s\n %s\n %s",
             _lang("When scanning is complete, use the"),
             _lang("dial to select a channel and press"),
             _lang("the Enter button to choose"),
             _lang("Right button switches band"));
    lv_label_set_text(label2, buf);
    lv_obj_set_style_text_font(label2, UI_SCANNOW_NOTE_FONT, 0);
    lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(label2, lv_color_hex(TEXT_COLOR_DEFAULT), 0);
    lv_obj_set_style_pad_top(label2, UI_SCANNOW_NOTE_PAD, 0);
    lv_label_set_long_mode(label2, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(label2, LV_GRID_ALIGN_START, 2, 1,
                         LV_GRID_ALIGN_START, 0, 3);

    analog_list = lv_obj_create(page);
    lv_obj_set_size(analog_list, UI_SCANNOW_FREQ_SIZE);
    lv_obj_set_style_bg_opa(analog_list, 0, 0);
    lv_obj_add_style(analog_list, &style_scan, LV_PART_MAIN);
    lv_obj_set_layout(analog_list, LV_LAYOUT_GRID);
    lv_obj_clear_flag(analog_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(analog_list, 0, 0);
    lv_obj_set_style_grid_column_dsc_array(analog_list, analog_col_dsc2, 0);
    lv_obj_set_style_grid_row_dsc_array(analog_list, analog_row_dsc2, 0);

    const uint8_t col_offset = 1;
    for (int i = 0; i < 8; i++) {
        const char *name = channel2str(0, 0, i + 1);
        create_analog_channel_switch(analog_list, ((i >> 2) << 2) + col_offset, i & 0x03, &analog_channel_tb[i], name);
    }

    analog_update_band_ui();

    return page;
}

static int scan_analog(void) {
    g_scanning = true;
    int8_t ret = scan_analog_now();
    g_scanning = false;
    return ret;
}

static void page_scananalog_enter() {
    int auto_scaned_cnt = scan_analog();
    LOGI("analog scan return :%d", auto_scaned_cnt);

    if (auto_scaned_cnt == 1) {
#ifdef EMULATOR_BUILD
        // In emulator, just show menu again after dummy scan completes.
        submenu_exit();
        return;
#endif
        app_state_push(APP_STATE_VIDEO);
        g_source_info.source = SOURCE_AV_MODULE;
        app_switch_to_analog(0);
    }

    if (auto_scaned_cnt == -1)
        submenu_exit();
}

static void page_scananalog_exit() {
    HDZero_Close();
}

static void page_scananalog_on_roller(uint8_t key) {
    if (analog_user_select_index < 0)
        return;

    if (key == DIAL_KEY_UP) {
        if (analog_user_select_index < 7)
            analog_user_select_index++;
    } else if (key == DIAL_KEY_DOWN) {
        if (analog_user_select_index > 0)
            analog_user_select_index--;
    }
    analog_select_signal(analog_user_select_index);
}

static void page_scananalog_on_click(uint8_t key, int sel) {
    if (analog_user_select_index < 0)
        return;

    int ch = analog_current_band * 8 + analog_user_select_index + 1;
    if (!analog_status_tb[ch - 1].is_valid)
        return;
    g_setting.source.analog_channel = ch;
    ini_putl("source", "analog_channel", g_setting.source.analog_channel, SETTING_INI);
    g_source_info.source = SOURCE_AV_MODULE;
    app_state_push(APP_STATE_VIDEO);
    app_switch_to_analog(0);
}

static void page_scananalog_on_right_button(bool is_short) {
    if (is_short)
        analog_change_band(1);
}

page_pack_t pp_scananalog = {
    .name = ANALOG_SCAN_NAME,
    .create = page_scananalog_create,
    .enter = page_scananalog_enter,
    .exit = page_scananalog_exit,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = page_scananalog_on_roller,
    .on_click = page_scananalog_on_click,
    .on_right_button = page_scananalog_on_right_button,
};
