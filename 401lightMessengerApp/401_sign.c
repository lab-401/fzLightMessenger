/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "./401_sign.h"
#include "401_err.h"
static void l401_sign_input_callback(InputEvent* input, void* ctx) {
    FuriSemaphore* semaphore = ctx;
    if((input->type == InputTypeShort) && (input->key == InputKeyBack)) {
        furi_semaphore_release(semaphore);
    }
}

static void l401_sign_render_callback(Canvas* canvas, void* status) {
    l401_err* err = (l401_err*)status;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 128 / 2, 10, AlignLeft, AlignTop, "ERROR!");
    switch(*err) {
    case L401_ERR_STORAGE:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_storage);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "Storage error");
        break;
    case L401_ERR_PARSE:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_parse);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "Parsing error");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "you got an F");
        break;
    case L401_ERR_INTERNAL:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_unknown2);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "Internal error");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "something not cool");
        break;
    case L401_ERR_FILESYSTEM:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_storage);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "Filesystem stuff");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "went wrong x_x");
        break;
    case L401_ERR_MALFORMED:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_malformed);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "Configuration file");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "malformed :(");
        break;
    case L401_ERR_HARDWARE:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_hw);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "Hardware module");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "not found :(");
        break;
    case L401_ERR_FILE_DOESNT_EXISTS:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_search);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "File missing");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "or not found");
        break;
    case L401_ERR_BMP_FILE:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_bitmap);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 20, AlignLeft, AlignTop, "bitmap files");
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "must be 16px tall");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "max 100px wide");
        canvas_draw_str_aligned(canvas, 50, 50, AlignLeft, AlignTop, "and 1bpp only.");
        break;
    default:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_err_unknown);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 50, 30, AlignLeft, AlignTop, "Unknown stuff");
        canvas_draw_str_aligned(canvas, 50, 40, AlignLeft, AlignTop, "happened :'(");
        break;
    }
}

l401_err check_hat(void* app); // Implemented in 401LightMsg_main.c

static void on_tick(void* ctx) {
    SignContext* sign_context = ctx;
    l401_err err = check_hat(sign_context->app_context);
    if (err == L401_OK) {
        sign_context->status_code = 0;
        furi_semaphore_release(sign_context->semaphore);
    }
}

int32_t l401_sign_app(l401_err err, void* app_context) {
    FuriSemaphore* semaphore = furi_semaphore_alloc(1, 0);
    furi_assert(semaphore);
    SignContext sign_context = { semaphore, app_context, (err == L401_ERR_HARDWARE) ? 1 : 0 };
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, l401_sign_render_callback, &err);
    view_port_input_callback_set(view_port, l401_sign_input_callback, semaphore);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FuriTimer* timer = NULL;
    if (err == L401_ERR_HARDWARE) {
        timer = furi_timer_alloc(on_tick, FuriTimerTypePeriodic, &sign_context);
        furi_timer_start(timer, 1000);
    }
    view_port_update(view_port);

    furi_check(furi_semaphore_acquire(semaphore, FuriWaitForever) == FuriStatusOk);
    if (timer) {
        furi_timer_stop(timer);
        furi_timer_free(timer);
    }
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_semaphore_free(semaphore);
    return sign_context.status_code;
}
