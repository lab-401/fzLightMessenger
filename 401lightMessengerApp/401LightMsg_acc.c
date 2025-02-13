
/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401LightMsg_acc.h"
#include "401LightMsg_config.h"
#include "bmp.h"
#include "font.h"
#include "gui/gui.h"
#include "gui/view_dispatcher.h"
#define PI  3.14159
#define PI3 PI / 3
static const char* TAG = "401_LightMsgAcc";

/**

  Why does this section seems so broken?
  Here, you'll see things we usually don't see around here. Yes, i would have
  liked to use high level API for signal handling, and cool integrated stuff...
  But we're playing with tight timings, very touchy calibrations, and disgusting
  optimisations. Sorry for that, if you can make it work correctly
  with more pretty code, please have a shot at it! :)

*/

/**
 * Creates a bit array representation of the given text using the provided font.
 *
 * @param text The text to convert.
 * @param font The font to use for conversion.
 * @return A pointer to the created text bit array.
 */
static bitmapMatrix* bitMatrix_text_create(const char* text, bitmapMatrixFont* font) {
    furi_assert(text);
    furi_assert(font);
    uint8_t textLen = strlen(text);
    uint8_t fontWidth = font[0];
    uint8_t fontHeight = font[1];
    uint8_t fontOffset = font[2];
    uint8_t totalWidth = (fontWidth + FONT_SPACE_WIDTH) * textLen;
    bitmapMatrix* bitMatrix = NULL;
    uint8_t letter = 0;
    size_t letterPtr = 0;
    size_t bitMatrixOffset = 0;
    bitMatrix = malloc(sizeof(bitmapMatrix));
    if(!bitMatrix) return NULL;

    bitMatrix->width = totalWidth;
    bitMatrix->height = fontHeight;
    bitMatrix->array = NULL;

    bitMatrix->array = malloc(fontHeight * sizeof(uint8_t*));

    if(!bitMatrix->array) {
        free(bitMatrix);
        return NULL;
    }

    for(uint8_t h = 0; h < fontHeight; h++) {
        bitMatrix->array[h] = malloc(totalWidth * sizeof(uint8_t));
        bitMatrix->height = h;
        if(!bitMatrix->array[h]) {
            bitmapMatrix_free(bitMatrix);
            return NULL;
        }
        // Write each letter next to each other, accounting for spaces.
        for(uint8_t t = 0; t < textLen; t++) {
            letter = text[t];
            letterPtr = ((letter - fontOffset) * fontHeight) + 4;
            for(uint8_t w = 0; w < fontWidth; w++) {
                bitMatrixOffset = (t * (FONT_SPACE_WIDTH + fontWidth)) + w;
                bitMatrix->array[h][bitMatrixOffset] =
                    ((font[letterPtr + h] >> (fontWidth - w)) & 0x01) ? 0xff : 0x00;
            }
        }
    }
    // complete the height
    bitMatrix->height++;
    return bitMatrix;
}

/**
 * Input callback for app acceleration.
 *
 * @param input_event The input event.
 * @param ctx The application ctx.
 * @return Always returns false.
 */
static bool app_acc_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    AppContext* app = ctx;
    
    // Handle back button
    if(input_event->type == InputTypeShort && input_event->key == InputKeyBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, AppSceneMainMenu);
        return true;
    }

    return false;
}

/**
 * Handles the swipe tick.
 *
 * @param ctx The ctx.
 */
static void swipes_tick(void* ctx) {
    AppAcc* appAcc = (AppAcc*)ctx;
    appAcc->cycles++;
}

/**
 * Initializes the swipe with the given direction.
 *
 * @param ctx The ctx.
 * @param direction The swipe direction.
 */
static void swipes_init(void* ctx, uint8_t direction) {
    AppContext* app = (AppContext*)ctx;
    AppAcc* appAcc = (AppAcc*)app->sceneAcc;

    if(appAcc->direction != direction) {
        if(appAcc->cyclesAvg != 0)
            appAcc->cyclesAvg = (appAcc->cyclesAvg + appAcc->cycles) / 2;
        else
            appAcc->cyclesAvg = appAcc->cycles;
        // The center is offseted on the first third of the swipe motion to compensate
        // for the acceleration's interractions.
        appAcc->cyclesCenter = (uint16_t)((appAcc->cyclesAvg) / 3);
        appAcc->cycles = 0;
        appAcc->direction = direction;
    }
}

/**
 * Callback for the maximum Z value.
 *
 * @param ctx The ctx.
 */
static void zmax_callback(void* ctx) {
    AppContext* app = (AppContext*)ctx; // Main app struct
    swipes_init(app, 1);
}

/**
 * Callback for the minimum Z value.
 *
 * @param ctx The ctx.
 */
static void zmin_callback(void* ctx) {
    AppContext* app = (AppContext*)ctx; // Main app struct
    swipes_init(app, 0);
}

/**
 * Worker function for the app acceleration.
 *
 * @param ctx The ctx.
 * @return Always returns 0.
 */
static int32_t app_acc_worker(void* ctx) {
    assert(ctx);
    AppContext* app = (AppContext*)ctx; // Main app struct
    AppAcc* appAcc = (AppAcc*)app->sceneAcc;
    AppData* appData = (AppData*)app->data;
    //Canvas* canvas = app->view_dispatcher->gui->canvas;
    Configuration* light_msg_data = (Configuration*)appData->config;
    bitmapMatrix* bitmapMatrix = appAcc->bitmapMatrix;

    uint16_t time = 0; // time ticker
    bool running = true; // Keeps the thread running
    bool is_bitmap_window = 0; // Check if
    // FuriMutex *mutex = appAcc->mutex;

    uint8_t column = 0; // Column of the bitmap array
    uint8_t column_directed = 0; // Column of the bitmap array

    uint8_t row = 0;

    uint8_t r = 0, g = 0, b = 0;
    uint8_t pixel = 0;
    double brightness = lightmsg_brightness_value[light_msg_data->brightness];

    uint32_t color[LIGHTMSG_LED_ROWS] = {0};

    // The shader updating function is the callback associated to the "color"
    color_animation_callback shader = appData->shader;
    lis2dh12_init(&app->data->lis2dh12);

    while(running) {
        // Checks if the thread must be ended.
        if(furi_thread_flags_get()) {
            running = false;
        }
        if(time++ == 4000) {
            notification_message(app->notifications, &sequence_display_backlight_off);
        }

        // Update the cycles counter
        swipes_tick(appAcc);

        /*             Display diagram

                             ╭┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ cyclesCenter
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
            ╎╎╎╎╎╎╎╎╏╏╏╏╏╏╏╏╏║╏╏╏╏╏╏╏╏╎╎╎╎╎╎╎╎
                    └────────────────┘┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ window (=len)
            └────────────────────────────────┘┄┄┄┄┄┄┄ cycles
            ─t⟶
            ⟵t─
        */
        // Get current column to be displayed.
        column = appAcc->cycles - ((appAcc->cyclesCenter) - (bitmapMatrix->width / 2));

        // Swipe direction, according to orientation
        if(appAcc->direction ^ light_msg_data->orientation) {
            column_directed = (column % bitmapMatrix->width) - appAcc->direction;
        } else {
            column_directed =
                bitmapMatrix->width - (column % (bitmapMatrix->width)) - appAcc->direction;
        }

        // Computes the window in which the text is displayed.
        // low limit  = center - len / 2
        // high limit = center + len / 2
        is_bitmap_window =
            (appAcc->cycles >
             (appAcc->cyclesCenter - ((bitmapMatrix->width - appAcc->direction) / 2))) &&
            (appAcc->cycles <=
             (appAcc->cyclesCenter + ((bitmapMatrix->width - appAcc->direction) / 2)));

        // Update the color according to the current shader
        shader(time, color, app);
        // Let the leds shine for a bit
        furi_delay_us(500);

        if(is_bitmap_window) {
            // Draws each rows for each collumns
            for(row = 0; row < LIGHTMSG_LED_ROWS; row++) {
                pixel = (uint8_t)(bitmapMatrix->array[row][column_directed]);
                //  Assign R/G/B on dimmed pixel value
                r = (uint8_t)((pixel & (color[row] >> 16)) * brightness);
                g = (uint8_t)((pixel & (color[row] >> 8)) * brightness);
                b = (uint8_t)((pixel & (color[row] >> 0)) * brightness);
                // Orientation (wheel up/wheel down)
                if(light_msg_data->orientation) {
                    SK6805_set_led_color(row, r, g, b);
                } else {
                    SK6805_set_led_color(LIGHTMSG_LED_ROWS - 1 - row, r, g, b);
                }
            }
        } else {
            for(row = 0; row < LIGHTMSG_LED_ROWS; row++) {
                // Assign brightness accordingly to pixel
                pixel = (uint8_t)(bitmapMatrix->array[row][column_directed - 1] * brightness);
                //  Assign R/G/B on dimmed pixel value
                r = 0x00;
                g = 0x00;
                b = 0x00;

                // Orientation (wheel up/wheel down)
                if(light_msg_data->orientation) {
                    SK6805_set_led_color(row, r, g, b);
                } else {
                    SK6805_set_led_color(LIGHTMSG_LED_ROWS - 1 - row, r, g, b);
                }
            }
        }
        // Stops all OS operation while sending data to LEDs
        SK6805_update();
        furi_delay_us(100);
    }
    SK6805_off();
    bitmapMatrix_free(appAcc->bitmapMatrix);
    return 0;
}

void app_acc_render_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_swipe);
    canvas_draw_icon(canvas, 43, 46, &I_401_lghtmsg_arrow);
    canvas_commit(canvas);
}

/**
 * Allocates an instance of AppAcc.
 *
 * @return A pointer to the allocated AppAcc.
 */
AppAcc* app_acc_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx; // Main app struct
    furi_assert(app->data);
    AppAcc* appAcc = malloc(sizeof(AppAcc)); // app Acc struct

    appAcc->displayMode = APPACC_DISPLAYMODE_TEXT;
    appAcc->counter = 0;
    appAcc->cycles = 0;
    appAcc->cyclesAvg = 0;
    appAcc->cyclesCenter = 0;

    // ISR for maxZ and minZ defining the swipe motion
    furi_hal_gpio_init(LGHTMSG_INT1PIN, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_init(LGHTMSG_INT2PIN, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_add_int_callback(LGHTMSG_INT1PIN, zmax_callback, app);
    furi_hal_gpio_add_int_callback(LGHTMSG_INT2PIN, zmin_callback, app);

    // Configure view
    appAcc->view = view_alloc();

    view_set_context(appAcc->view, app);
    view_set_input_callback(appAcc->view, app_acc_input_callback);
    view_set_draw_callback(appAcc->view, app_acc_render_callback);

    return appAcc;
}

/**
 * Retrieves the view associated with the given AppAcc instance.
 *
 * @param appAcc The AppAcc instance.
 * @return A pointer to the view.
 */
View* app_acc_get_view(AppAcc* appAcc) {
    furi_assert(appAcc);
    return appAcc->view;
}

static bool set_bitmap_dialog(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx; // Main app struct
    AppData* appData = (AppData*)app->data;
    AppAcc* appAcc = (AppAcc*)app->sceneAcc;
    bool set = true;

    Configuration* light_msg_data = (Configuration*)appData->config;
    DialogsFileBrowserOptions browser_options;
    FuriString* bitmapPath;

    appAcc->dialogs = furi_record_open(RECORD_DIALOGS);
    bitmapPath = furi_string_alloc();
    furi_string_set(bitmapPath, light_msg_data->bitmapPath);

    dialog_file_browser_set_basic_options(&browser_options, ".bmp", &I_401_lghtmsg_icon_10px);
    browser_options.base_path = LIGHTMSGCONF_SAVE_FOLDER;
    browser_options.skip_assets = true;

    // Input events and views are managed by file_browser
    if(!dialog_file_browser_show(appAcc->dialogs, bitmapPath, bitmapPath, &browser_options)) {
        set = false;
    } else {
        int res = bmp_header_check_1bpp(furi_string_get_cstr(bitmapPath));
        if(res == 0) {
            strncpy(
                light_msg_data->bitmapPath,
                furi_string_get_cstr(bitmapPath),
                LIGHTMSG_MAX_BITMAPPATH_LEN);
            light_msg_data->bitmapPath[LIGHTMSG_MAX_BITMAPPATH_LEN] = '\0';

            l401_err res = config_save_json(LIGHTMSGCONF_CONFIG_FILE, light_msg_data);

            if(res == L401_OK) {
                FURI_LOG_I(
                    TAG, "Successfully saved configuration to %s.", LIGHTMSGCONF_CONFIG_FILE);
            } else {
                l401_sign_app(res);
                set = false;
            }
        } else {
            l401_sign_app(L401_ERR_BMP_FILE);
            set = false;
        }
    }

    furi_string_free(bitmapPath);
    return set;
}

/**
 * Handles the on-enter event for the app scene acceleration.
 *
 * @param ctx The ctx.
 */
void app_scene_acc_on_enter(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppAcc* appAcc = (AppAcc*)app->sceneAcc;
    AppData* appData = (AppData*)app->data;
    Configuration* light_msg_data = (Configuration*)appData->config;
    // AppBmpEditor* BmpEditor = app->sceneBmpEditor;

    appAcc->accThread = NULL;
    appData->shader = lightmsg_color_value[light_msg_data->color];
    appAcc->bitmapMatrix = NULL;

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewAcc);

    switch(appAcc->displayMode) {
    case APPACC_DISPLAYMODE_TEXT:
        appAcc->bitmapMatrix = bitMatrix_text_create((char*)light_msg_data->text, LightMsgSetFont);
        break;

    case APPACC_DISPLAYMODE_BITMAP:
        if(set_bitmap_dialog(ctx)) {
            appAcc->bitmapMatrix = bmp_to_bitmapMatrix(light_msg_data->bitmapPath);
        } else {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, AppSceneMainMenu);
        }
        break;

    case APPACC_DISPLAYMODE_CUSTOM:
        appAcc->bitmapMatrix = bmp_to_bitmapMatrix(light_msg_data->bitmapPath);
        break;

    default:
        break;
    }

    if(appAcc->bitmapMatrix != NULL) {
        appAcc->accThread = furi_thread_alloc();
        if(appAcc->accThread == NULL) {
            FURI_LOG_E(TAG, "Failed to allocate thread");
            return;
        }

        furi_thread_set_name(appAcc->accThread, "ACC");
        furi_thread_set_stack_size(appAcc->accThread, 2048);
        furi_thread_set_callback(appAcc->accThread, app_acc_worker);
        furi_thread_set_context(appAcc->accThread, app);
        furi_thread_set_priority(appAcc->accThread, FuriThreadPriorityNormal);
        furi_thread_start(appAcc->accThread);
    }
}

/**
 * Handles events for the app scene acceleration.
 *
 * @param ctx The ctx.
 * @param event The scene manager event.
 * @return Always returns false.
 */
bool app_scene_acc_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

/**
 * Handles the on-exit event for the app scene acceleration.
 *
 * @param ctx The ctx.
 */
void app_scene_acc_on_exit(void* ctx) {
    furi_assert(ctx);
    AppContext* app = ctx;
    AppAcc* appAcc = (AppAcc*)app->sceneAcc;
    //Canvas* canvas = app->view_dispatcher->gui->canvas;
    //canvas_reset(canvas);

    if(appAcc->accThread != NULL) {
        furi_thread_flags_set(furi_thread_get_id(appAcc->accThread), 1);
        furi_thread_join(appAcc->accThread);
        furi_thread_free(appAcc->accThread);
    }
}

/**
 * Frees the memory occupied by the given AppAcc instance.
 *
 * @param appAcc The AppAcc instance to be freed.
 */
void app_acc_free(AppAcc* appAcc) {
    furi_assert(appAcc);
    furi_hal_gpio_remove_int_callback(LGHTMSG_INT2PIN);
    furi_hal_gpio_init(LGHTMSG_INT1PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_remove_int_callback(LGHTMSG_INT1PIN);
    furi_hal_gpio_init(LGHTMSG_INT2PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    view_free(appAcc->view);
    free(appAcc);
}
