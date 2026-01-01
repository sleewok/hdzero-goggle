#include "ui/ui_main_menu.h"

#include <stdio.h>
#include <stdlib.h>

#include <log/log.h>

#include "../conf/ui.h"

#include "common.hh"
#include "core/app_state.h"
#include "driver/hardware.h"
#include "driver/mcp3021.h"
#include "driver/screen.h"
#include "lang/language.h"
#include "ui/page_analog_rssi.h"
#include "ui/page_autoscan.h"
#include "ui/page_clock.h"
#include "ui/page_common.h"
#include "ui/page_elrs.h"
#include "ui/page_fans.h"
#include "ui/page_focus_chart.h"
#include "ui/page_headtracker.h"
#include "ui/page_imagesettings.h"
#include "ui/page_input.h"
#include "ui/page_osd.h"
#include "ui/page_playback.h"
#include "ui/page_power.h"
#include "ui/page_record.h"
#include "ui/page_scannow.h"
#include "ui/page_scananalog.h"
#include "ui/page_sleep.h"
#include "ui/page_source.h"
#include "ui/page_storage.h"
#include "ui/page_version.h"
#include "ui/page_wifi.h"
#include "ui/ui_image_setting.h"
#include "ui/ui_keyboard.h"
#include "ui/ui_porting.h"
#include "ui/ui_style.h"

LV_IMG_DECLARE(img_arrow);

progress_bar_t progress_bar;

static lv_obj_t *menu;
static lv_obj_t *root_page;
static lv_obj_t *menu_section;
static lv_obj_t *menu_scroll_up;
static lv_obj_t *menu_scroll_down;

/**
 * To contain all menu pages.
 */
#define PAGE_PACK_MAX_NUM 20

static page_pack_t *page_packs[PAGE_PACK_MAX_NUM];
static size_t page_packs_count = 0;
static page_pack_t *post_bootup_actions[PAGE_PACK_MAX_NUM];
static size_t post_bootup_actions_count = 0;
static bool bootup_actions_fired = false;

static page_pack_t *find_pp(lv_obj_t *page) {
    for (uint32_t i = 0; i < page_packs_count; i++) {
        if (page_packs[i]->page == page) {
            return page_packs[i];
        }
    }
    return NULL;
}

static void select_menu_tab(page_pack_t *pp) {
    lv_obj_clear_flag(pp->icon, LV_OBJ_FLAG_HIDDEN);
#ifdef HDZBOXPRO
    lv_obj_set_style_bg_opa(((lv_menu_t *)menu)->selected_tab, LV_OPA_20, LV_STATE_CHECKED);
#else
    lv_obj_set_style_bg_opa(((lv_menu_t *)menu)->selected_tab, LV_OPA_50, LV_STATE_CHECKED);
#endif
}

static void deselect_menu_tab(page_pack_t *pp) {
    // LV_OPA_20 is the default for pressed menu
    // see lv_theme_default.c styles->menu_pressed
    lv_obj_set_style_bg_opa(((lv_menu_t *)menu)->selected_tab, LV_OPA_20, LV_STATE_CHECKED);
    lv_obj_add_flag(pp->icon, LV_OBJ_FLAG_HIDDEN);
}

static void update_menu_scroll_indicators(lv_obj_t *scroll_obj) {
    if (!menu_scroll_up || !menu_scroll_down || !scroll_obj) {
        return;
    }

    lv_obj_update_layout(scroll_obj);
    const lv_coord_t top = lv_obj_get_scroll_top(scroll_obj);
    const lv_coord_t bottom = lv_obj_get_scroll_bottom(scroll_obj);
    const lv_coord_t content_h = lv_obj_get_self_height(scroll_obj);
    const lv_coord_t view_h = lv_obj_get_height(scroll_obj);
    const bool has_overflow = (content_h > view_h + 2) || (top > 0) || (bottom > 0);

    if (!has_overflow) {
        lv_obj_add_flag(menu_scroll_up, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(menu_scroll_down, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (top > 0) {
        lv_obj_clear_flag(menu_scroll_up, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(menu_scroll_up, LV_OBJ_FLAG_HIDDEN);
    }

    if (bottom > 0) {
        lv_obj_clear_flag(menu_scroll_down, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(menu_scroll_down, LV_OBJ_FLAG_HIDDEN);
    }
}

static void position_menu_scroll_indicators() {
    if (!menu_scroll_up || !menu_scroll_down || !menu_section) {
        return;
    }

    lv_area_t area;
    lv_obj_get_coords(menu_section, &area);
    lv_obj_update_layout(menu_scroll_up);
    lv_obj_update_layout(menu_scroll_down);

    const lv_coord_t up_w = lv_obj_get_width(menu_scroll_up);
    const lv_coord_t up_h = lv_obj_get_height(menu_scroll_up);
    const lv_coord_t down_w = lv_obj_get_width(menu_scroll_down);
    const lv_coord_t down_h = lv_obj_get_height(menu_scroll_down);

    lv_coord_t top_y = area.y1 + 2;
    if (top_y < 10) {
        top_y = 10;
    }
    lv_coord_t bottom_y = area.y2 - down_h - 2;
    if (bottom_y > lv_disp_get_ver_res(NULL) - down_h - 10) {
        bottom_y = lv_disp_get_ver_res(NULL) - down_h - 10;
    }

    lv_obj_set_pos(menu_scroll_up, area.x1 - up_w - 10, top_y);
    lv_obj_set_pos(menu_scroll_down, area.x1 - down_w - 10, bottom_y);
}

static void menu_scroll_event_cb(lv_event_t *e) {
    lv_obj_t *scroll_obj = lv_event_get_target(e);
    position_menu_scroll_indicators();
    update_menu_scroll_indicators(scroll_obj);
}

void submenu_enter(void) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    select_menu_tab(pp);

    if (pp->p_arr.max) {
        // if we have selectable entries, select the first selectable one
        for (pp->p_arr.cur = 0; !lv_obj_has_flag(pp->p_arr.panel[pp->p_arr.cur], FLAG_SELECTABLE); ++pp->p_arr.cur)
            ;
        set_select_item(&pp->p_arr, pp->p_arr.cur);
    }

    if (pp->enter) {
        // if your page as a enter event handler, call it
        pp->enter();
    }
}

void submenu_right_button(bool is_short) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->on_right_button) {
        // if your page has a right_button event handler, call it
        pp->on_right_button(is_short);
    }
}

void submenu_roller(uint8_t key) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->p_arr.max) {
        // if we have selectable entries, move selection
        if (key == DIAL_KEY_UP) {
            do {
                if (pp->p_arr.cur < pp->p_arr.max - 1)
                    pp->p_arr.cur++;
                else
                    pp->p_arr.cur = 0;
            } while (!lv_obj_has_flag(pp->p_arr.panel[pp->p_arr.cur], FLAG_SELECTABLE));
        } else if (key == DIAL_KEY_DOWN) {
            do {
                if (pp->p_arr.cur > 0)
                    pp->p_arr.cur--;
                else
                    pp->p_arr.cur = pp->p_arr.max - 1;
            } while (!lv_obj_has_flag(pp->p_arr.panel[pp->p_arr.cur], FLAG_SELECTABLE));
        }
        LOGI("submenu_roller %d, %d", pp->p_arr.cur, pp->p_arr.max - 1);
        set_select_item(&pp->p_arr, pp->p_arr.cur);
    }

    // Allow roller to have latest item selected
    if (pp->on_roller) {
        // if your page as a roller event handler, call it
        pp->on_roller(key);
    }
}

// the submenu pages called on_roller event handler has to update
// the selection by setting pp->p_arr.cur if a selection change is needed
void submenu_roller_no_selection_change(uint8_t key) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->on_roller) {
        // if your page as a roller event handler, call it
        pp->on_roller(key);
    }

    set_select_item(&pp->p_arr, pp->p_arr.cur);
}

void submenu_exit() {
    LOGI("submenu_exit");
    app_state_push(APP_STATE_MAINMENU);

    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    deselect_menu_tab(pp);

    if (pp->exit) {
        // if your page as a exit event handler, call it
        pp->exit();
    }

    if (pp->p_arr.max) {
        // if we have selectable icons, reset the selector
        pp->p_arr.cur = 0;
        set_select_item(&pp->p_arr, -1);
    }
}

void submenu_click(void) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->on_click) {
        // if your page as a click event handler, call it
        pp->on_click(DIAL_KEY_CLICK, pp->p_arr.cur);
    }

    if (pp->p_arr.max && g_app_state != APP_STATE_WIFI) {
        // if we have selectable icons, check if we hit the back button
        if (pp->p_arr.cur == pp->p_arr.max - 1) {
            submenu_exit();
        }
    }
}

void menu_nav(uint8_t key) {
    static int8_t selected = 0;
    LOGI("menu_nav: key = %d,sel = %d", key, selected);
    if (key == DIAL_KEY_DOWN) {
        selected--;
        if (selected < 0)
            selected += page_packs_count;
    } else if (key == DIAL_KEY_UP) {
        selected++;
        if (selected >= page_packs_count)
            selected -= page_packs_count;
    }
    lv_obj_t *item = lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), selected);
    if (item) {
        lv_event_send(item, LV_EVENT_CLICKED, NULL);
        if (menu_section) {
            lv_obj_scroll_to_view(item, LV_ANIM_OFF);
            update_menu_scroll_indicators(menu_section);
        }
    }
}

static void menu_reinit(void) {
    LOGI("menu_reinit");

    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if ((pp == &pp_scannow)) {
        scan_reinit();
    }

    deselect_menu_tab(pp);

    if (pp->p_arr.max) {
        // if we have selectable icons, reset the selector
        pp->p_arr.cur = 0;
        set_select_item(&pp->p_arr, -1);
    }
}

bool main_menu_is_shown(void) {
    return !lv_obj_has_flag(menu, LV_OBJ_FLAG_HIDDEN);
}

void main_menu_show(bool is_show) {
    if (is_show) {
        menu_reinit();
        lv_obj_clear_flag(menu, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
    }
}

static void main_menu_create_entry(lv_obj_t *menu, lv_obj_t *section, page_pack_t *pp) {
    LOGD("creating main menu entry %s", pp->name);

    pp->page = pp->create(menu, &pp->p_arr);

    lv_obj_t *cont = lv_menu_cont_create(section);

    pp->label = lv_label_create(cont);
    lv_label_set_text(pp->label, _lang(pp->name));
    lv_obj_set_style_text_font(pp->label, UI_MENU_ENTRY_FONT, 0);
    lv_obj_set_style_text_color(pp->label, lv_color_hex(TEXT_COLOR_DEFAULT), 0);
    lv_label_set_long_mode(pp->label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    pp->icon = lv_img_create(cont);
    lv_img_set_src(pp->icon, &img_arrow);
    lv_obj_add_flag(pp->icon, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(cont, UI_MENU_ENTRY_FONT, 0);
    lv_menu_set_load_page_event(menu, cont, pp->page);

    if (pp->on_created) {
        pp->on_created();
    }
    LOGD("Done");
}

static int post_bootup_actions_cmp(const void *lhs, const void *rhs) {
    page_pack_t *lpp = *(page_pack_t **)lhs;
    page_pack_t *rpp = *(page_pack_t **)rhs;
    const int32_t lpri = lpp->post_bootup_run_priority;
    const int32_t rpri = rpp->post_bootup_run_priority;

    if (lpri < rpri) {
        return -1;
    } else if (lpri > rpri) {
        return 1;
    }

    return 0;
}

void main_menu_init(void) {
    // Initialize All Pages
    page_packs[page_packs_count++] = &pp_scannow;
    page_packs[page_packs_count++] = &pp_scananalog;
    page_packs[page_packs_count++] = &pp_source;
    page_packs[page_packs_count++] = &pp_imagesettings;
    page_packs[page_packs_count++] = &pp_osd;
    page_packs[page_packs_count++] = &pp_power;
    page_packs[page_packs_count++] = &pp_fans;
    page_packs[page_packs_count++] = &pp_record;
    page_packs[page_packs_count++] = &pp_autoscan;
    if (g_setting.has_all_features) {
        page_packs[page_packs_count++] = &pp_elrs;
        page_packs[page_packs_count++] = &pp_wifi;
    }
    page_packs[page_packs_count++] = &pp_headtracker;
    page_packs[page_packs_count++] = &pp_playback;
    page_packs[page_packs_count++] = &pp_storage;
    page_packs[page_packs_count++] = &pp_version;
    page_packs[page_packs_count++] = &pp_focus_chart;
    page_packs[page_packs_count++] = &pp_clock;
    page_packs[page_packs_count++] = &pp_input;
#if defined(HDZBOXPRO) || defined(HDZGOGGLE2)
    page_packs[page_packs_count++] = &pp_analog_rssi;
#endif
    page_packs[page_packs_count++] = &pp_sleep;

    menu = lv_menu_create(lv_scr_act());
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(menu, lv_color_make(32, 32, 32), 0);
    lv_obj_set_style_border_width(menu, 2, 0);
    lv_obj_set_style_border_color(menu, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_border_side(menu, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_size(menu, UI_MENU_SIZE);
    lv_obj_set_pos(menu, UI_MENU_POSITION);

    root_page = lv_menu_page_create(menu, "aaa");

    menu_section = lv_menu_section_create(root_page);
    lv_obj_add_flag(menu_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(menu_section, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(menu_section, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(menu_section, menu_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(menu_section, menu_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);
    lv_obj_add_event_cb(menu_section, menu_scroll_event_cb, LV_EVENT_SIZE_CHANGED, NULL);

    for (uint32_t i = 0; i < page_packs_count; i++) {
        main_menu_create_entry(menu, menu_section, page_packs[i]);
        if (page_packs[i]->post_bootup_run_priority > 0 && page_packs[i]->post_bootup_run_function != NULL) {
            post_bootup_actions[post_bootup_actions_count++] = page_packs[i];
        }
    }

    // Resort based on priority
    qsort(post_bootup_actions, post_bootup_actions_count, sizeof(post_bootup_actions[0]), post_bootup_actions_cmp);

    lv_obj_add_style(menu_section, &style_rootmenu, LV_PART_MAIN);
    lv_obj_set_size(menu_section, UI_MENU_ROOT_SIZE);
    lv_obj_set_pos(menu_section, 0, 0);

    lv_obj_set_size(root_page, UI_MENU_ROOT_SIZE);
    lv_obj_set_pos(root_page, 0, 0);
    lv_obj_set_style_border_width(root_page, 0, 0);
    lv_obj_set_style_radius(root_page, 0, 0);

    lv_menu_set_sidebar_page(menu, root_page);
    lv_event_send(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(lv_menu_get_sidebar_header(menu), LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lv_menu_get_cur_sidebar_page(menu), LV_OBJ_FLAG_SCROLLABLE);

    menu_scroll_up = lv_img_create(lv_scr_act());
    lv_img_set_src(menu_scroll_up, &img_arrow);
    lv_img_set_angle(menu_scroll_up, 2700);
    lv_obj_set_style_base_dir(menu_scroll_up, LV_BASE_DIR_LTR, 0);
    lv_obj_add_flag(menu_scroll_up, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(menu_scroll_up);

    menu_scroll_down = lv_img_create(lv_scr_act());
    lv_img_set_src(menu_scroll_down, &img_arrow);
    lv_img_set_angle(menu_scroll_down, 900);
    lv_obj_set_style_base_dir(menu_scroll_down, LV_BASE_DIR_LTR, 0);
    lv_obj_add_flag(menu_scroll_down, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(menu_scroll_down);

    lv_obj_update_layout(menu_section);
    position_menu_scroll_indicators();
    update_menu_scroll_indicators(menu_section);

    progress_bar.bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(progress_bar.bar, UI_MENU_PROG_BAR_SIZE);
    lv_obj_align(progress_bar.bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(progress_bar.bar, LV_OBJ_FLAG_HIDDEN);
    progress_bar.start = 0;
    progress_bar.val = 0;

    // Create Keyboard Object
    keyboard_init();
}

static void bootup_action_completed() {
    bootup_actions_fired = false;
}

static void handle_bootup_action() {
    static page_pack_t **next_bootup_action = &post_bootup_actions[0];
    if (next_bootup_action - &post_bootup_actions[0] >= post_bootup_actions_count) {
        return;
    }

    (*next_bootup_action++)->post_bootup_run_function(bootup_action_completed);
}

void main_menu_update() {
    static uint32_t delta_ms = 0;
    uint32_t now_ms = time_ms();
    delta_ms = now_ms - delta_ms;

    for (uint32_t i = 0; i < page_packs_count; i++) {
        if (page_packs[i]->on_update) {
            page_packs[i]->on_update(delta_ms);
        }
    }

    if (!bootup_actions_fired) {
        bootup_actions_fired = true;
        handle_bootup_action();
    }
    delta_ms = now_ms;
}

void progress_bar_update() {
    static uint8_t state = 0; // 0=idle, 1= in process

    switch (state) {
    case 0:
        if (progress_bar.start) { // to start the progress bar
            state = 1;
            lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(progress_bar.bar, LV_OBJ_FLAG_HIDDEN);
            progress_bar.val = 0;
            // LOGI("Progress bar start");
        }
        break;

    case 1:
        if (progress_bar.start == 0) { // to end end progress bar
            state = 0;
            lv_obj_clear_flag(menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(progress_bar.bar, LV_OBJ_FLAG_HIDDEN);
            progress_bar.val = 0;
            // LOGI("Progress bar end");
        }
        break;
    }

    if (state == 1) {
        if (progress_bar.val < 100)
            progress_bar.val += 4;
        lv_bar_set_value(progress_bar.bar, progress_bar.val, LV_ANIM_OFF);
        lv_timer_handler();
    }
}
