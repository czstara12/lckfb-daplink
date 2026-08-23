/*
 * SPDX-License-Identifier: MIT
 * Origin: Simplified from the existing About screen in this repository.
 * Created-By: gpt-5
 */

#include "../ui.h"
#include "screens.h"
#include "dap_main.h"
#include "main.h"

extern lv_group_t *about_group_main;

/**
 * @brief 将 About 页的返回按钮加入输入设备组。
 */
void ui_About_add_group(void)
{
    lv_group_add_obj(about_group_main, ui_AboutHomeB);
    lv_group_set_editing(about_group_main, false);
}

static void ui_about_add_info_label(const char *text)
{
    lv_obj_t *label = lv_label_create(ui_AboutContainerPanel);

    lv_label_set_text(label, text);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &ui_font_jetbrainsMonoMedium20, LV_PART_MAIN | LV_STATE_DEFAULT);
}

/**
 * @brief 显示软件版本。
 */
void ui_about_show_soft_version(void)
{
    char text[30] = {0};

    rt_snprintf(text, sizeof(text), "Version: %c.%c.%c",
                LCKFB_DAPLINK_VERSION_MAJOR,
                LCKFB_DAPLINK_VERSION_MINOR,
                LCKFB_DAPLINK_VERSION_PATCH);
    ui_about_add_info_label(text);
}

/**
 * @brief 显示硬件型号。
 */
void ui_about_show_hard_version(void)
{
    ui_about_add_info_label("STM32F407Vx");
}

/**
 * @brief 显示固件编译时间。
 */
void ui_about_show_date(void)
{
    char text[32] = {0};

    rt_snprintf(text, sizeof(text), "%s %s", __DATE__, __TIME__);
    ui_about_add_info_label(text);
}

/**
 * @brief 初始化 About 页面。
 */
void ui_About_screen_init(void)
{
    current_screen_set(SCREEN_ABOUT);

    if (about_group_main == NULL)
    {
        about_group_main = lv_group_create();
        lv_group_set_editing(about_group_main, false);
    }

    ui_About = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_About, LV_OBJ_FLAG_SCROLLABLE);

    ui_Abouttitle = lv_label_create(ui_About);
    lv_obj_set_width(ui_Abouttitle, 240);
    lv_obj_set_height(ui_Abouttitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_Abouttitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Abouttitle, "\xE5\x85\xB3\xE4\xBA\x8E"); /* 原文：关于 */
    lv_obj_set_style_text_color(ui_Abouttitle, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Abouttitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Abouttitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Abouttitle, &ui_font_PuHuiTi25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Abouttitle, lv_color_hex(0x0091E6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Abouttitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_AboutHomeB = lv_btn_create(ui_About);
    lv_obj_set_height(ui_AboutHomeB, 18);
    lv_obj_set_width(ui_AboutHomeB, lv_pct(96));
    lv_obj_set_x(ui_AboutHomeB, 0);
    lv_obj_set_y(ui_AboutHomeB, -4);
    lv_obj_set_align(ui_AboutHomeB, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_AboutHomeB, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(ui_AboutHomeB, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_AboutHomeB, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_AboutHomeB, lv_color_hex(0x0073FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AboutHomeB, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(ui_AboutHomeB, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(ui_AboutHomeB, 255, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(ui_AboutHomeB, 2, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(ui_AboutHomeB, 2, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

    ui_AboutHomeL = lv_label_create(ui_AboutHomeB);
    lv_obj_set_width(ui_AboutHomeL, lv_pct(100));
    lv_obj_set_height(ui_AboutHomeL, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_AboutHomeL, LV_ALIGN_CENTER);
    lv_label_set_text(ui_AboutHomeL, "Home");
    lv_obj_set_style_text_align(ui_AboutHomeL, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_AboutHomeL, &ui_font_jetbrainsMonoMedium20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_AboutContainerPanel = lv_obj_create(ui_About);
    lv_obj_set_height(ui_AboutContainerPanel, 190);
    lv_obj_set_width(ui_AboutContainerPanel, lv_pct(100));
    lv_obj_set_x(ui_AboutContainerPanel, 0);
    lv_obj_set_y(ui_AboutContainerPanel, 25);
    lv_obj_set_align(ui_AboutContainerPanel, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_AboutContainerPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_AboutContainerPanel, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_AboutContainerPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_AboutHomeB, ui_event_AboutreturnHomeB, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_About, ui_event_AboutreturnHomeB, LV_EVENT_SCREEN_UNLOADED, NULL);

    ui_about_show_soft_version();
    ui_about_show_hard_version();
    ui_about_show_date();
    ui_About_add_group();
}

/**
 * @brief 删除 About 页面。
 */
void ui_ABOUT_screen_del(void)
{
    lv_obj_del(ui_About);
    ui_About = NULL;
}
