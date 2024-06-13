
#include "../ui.h"
#include "screens.h"
#include "dap_main.h"

extern lv_group_t *about_group_main;

void ui_About_add_group(void)
{
	lv_group_add_obj(about_group_main, ui_AboutHomeB);

    lv_group_set_editing(about_group_main, false);   //导航模式
}

void ui_about_show_version(void)
{
    char _temp_text_buf[30 ] =  {0};
    /*Init the style for the default state*/
    static lv_style_t style;
    lv_style_init(&style);

    lv_style_set_radius(&style, 3);

    lv_style_set_bg_opa(&style, LV_OPA_100);
    lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_dir(&style, LV_GRAD_DIR_VER);

    lv_style_set_border_opa(&style, LV_OPA_40);
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_GREY));

    lv_style_set_shadow_width(&style, 8);
    lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_shadow_ofs_y(&style, 8);

    lv_style_set_outline_opa(&style, LV_OPA_COVER);
    lv_style_set_outline_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_pad_all(&style, 10);

    /*Init the pressed style*/
    static lv_style_t style_pr;
    lv_style_init(&style_pr);

    /*Add a large outline when pressed*/
    lv_style_set_outline_width(&style_pr, 30);
    lv_style_set_outline_opa(&style_pr, LV_OPA_TRANSP);

    lv_style_set_translate_y(&style_pr, 5);
    lv_style_set_shadow_ofs_y(&style_pr, 3);
    lv_style_set_bg_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 4));

    /*Add a transition to the outline*/
    static lv_style_transition_dsc_t trans;
    static lv_style_prop_t props[] = {LV_STYLE_OUTLINE_WIDTH, LV_STYLE_OUTLINE_OPA, 0};
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_linear, 300, 0, NULL);

    lv_style_set_transition(&style_pr, &trans);

    lv_obj_t * btn1 = lv_btn_create(ui_About);
    lv_obj_remove_style_all(btn1);                          /*Remove the style coming from the theme*/
    lv_obj_add_style(btn1, &style, 0);
    lv_obj_add_style(btn1, &style_pr, LV_STATE_PRESSED);
    lv_obj_set_size(btn1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_align(btn1, LV_ALIGN_TOP_MID);
    lv_obj_set_y(btn1, 50);


    lv_obj_t * label1 = lv_label_create(btn1);
    rt_snprintf(_temp_text_buf, sizeof(_temp_text_buf), "Version: %c.%c.%c", LCKFB_DAPLINK_VERSION_MAJOR, LCKFB_DAPLINK_VERSION_MINOR, LCKFB_DAPLINK_VERSION_PATCH);
    lv_label_set_text(label1, _temp_text_buf);
    lv_obj_set_style_text_font(label1, &ui_font_jetbrainsMonoMedium20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label1);

}

#define MASK_WIDTH 200
#define MASK_HEIGHT 30

static void add_mask_event_cb(lv_event_t * e)
{
    static lv_draw_mask_map_param_t m;
    static int16_t mask_id;

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_opa_t * mask_map = lv_event_get_user_data(e);
    if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_MASKED);
    }
    else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
        lv_draw_mask_map_init(&m, &obj->coords, mask_map);
        mask_id = lv_draw_mask_add(&m, NULL);

    }
    else if(code == LV_EVENT_DRAW_MAIN_END) {
        lv_draw_mask_free_param(&m);
        lv_draw_mask_remove_id(mask_id);
    }
}

/**
 * Draw label with gradient color
 */
void ui_about_show_date(void)
{
    char _temp_text_buf[30 ] =  {0};
    /* Create the mask of a text by drawing it to a canvas*/
    static lv_opa_t mask_map[MASK_WIDTH * MASK_HEIGHT];

    /*Create a "8 bit alpha" canvas and clear it*/
    lv_obj_t * canvas = lv_canvas_create(ui_About);
    lv_canvas_set_buffer(canvas, mask_map, MASK_WIDTH, MASK_HEIGHT, LV_IMG_CF_ALPHA_8BIT);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    /*Draw a label to the canvas. The result "image" will be used as mask*/
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_white();
    label_dsc.align = LV_TEXT_ALIGN_CENTER;

    rt_snprintf(_temp_text_buf, sizeof(_temp_text_buf), "%s %s", __DATE__,__TIME__);

    lv_canvas_draw_text(canvas, 5, 5, MASK_WIDTH, &label_dsc, _temp_text_buf);

    /*The mask is reads the canvas is not required anymore*/
    lv_obj_del(canvas);

    /* Create an object from where the text will be masked out.
     * Now it's a rectangle with a gradient but it could be an image too*/
    lv_obj_t * grad = lv_obj_create(ui_About);
    lv_obj_set_size(grad, MASK_WIDTH, MASK_HEIGHT);
    lv_obj_center(grad);
    lv_obj_set_style_bg_color(grad, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_grad_color(grad, lv_color_hex(0x0000ff), 0);
    lv_obj_set_style_bg_grad_dir(grad, LV_GRAD_DIR_HOR, 0);
    lv_obj_add_event_cb(grad, add_mask_event_cb, LV_EVENT_ALL, mask_map);

    lv_obj_set_y(grad, 5);
}

void ui_About_screen_init(void)
{
    current_screen_set(SCREEN_ABOUT);

    if(about_group_main == NULL)
    {
        about_group_main = lv_group_create();
        lv_group_set_editing(about_group_main, false);   //导航模式
    }

    ui_About = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_About, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Abouttitle = lv_label_create(ui_About);
    lv_obj_set_width(ui_Abouttitle, 240);
    lv_obj_set_height(ui_Abouttitle, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Abouttitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Abouttitle, "关于");
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
    lv_obj_add_flag(ui_AboutHomeB, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_AboutHomeB, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_AboutHomeB, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_AboutHomeB, lv_color_hex(0x0073FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AboutHomeB, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(ui_AboutHomeB, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(ui_AboutHomeB, 255, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(ui_AboutHomeB, 2, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(ui_AboutHomeB, 2, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

    ui_AboutHomeL = lv_label_create(ui_AboutHomeB);
    lv_obj_set_width(ui_AboutHomeL, lv_pct(100));
    lv_obj_set_height(ui_AboutHomeL, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_AboutHomeL, LV_ALIGN_CENTER);
    lv_label_set_text(ui_AboutHomeL, "Home");
    lv_label_set_recolor(ui_AboutHomeL, "true");
    lv_obj_set_style_text_align(ui_AboutHomeL, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_AboutHomeL, &ui_font_jetbrainsMonoMedium20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_AboutContainerPanel = lv_obj_create(ui_About);
    lv_obj_set_height(ui_AboutContainerPanel, 190);
    lv_obj_set_width(ui_AboutContainerPanel, lv_pct(100));
    lv_obj_set_x(ui_AboutContainerPanel, 0);
    lv_obj_set_y(ui_AboutContainerPanel, 25);
    lv_obj_set_align(ui_AboutContainerPanel, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_AboutContainerPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_AboutContainerPanel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    lv_obj_set_style_radius(ui_AboutContainerPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_AboutHomeB, ui_event_AboutreturnHomeB, LV_EVENT_ALL, NULL);

    ui_about_show_version();
    ui_about_show_date();

    ui_About_add_group();

}

void ui_ABOUT_screen_del(void)
{
    lv_obj_del(ui_About);
    ui_About = NULL;
}