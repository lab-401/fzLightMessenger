/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401_CONFIG_H_
#define _401_CONFIG_H_

#include "cJSON/cJSON.h"
#include <storage/storage.h>
#include <toolbox/path.h>
// #include "401LightMsg_config.h"
#define LIGHTMSG_VERSION             "1.1"
#define LIGHTMSG_DEFAULT_ORIENTATION 0
#define LIGHTMSG_DEFAULT_BRIGHTNESS  3
#define LIGHTMSG_DEFAULT_COLOR       1
#define LIGHTMSG_DEFAULT_TEXT        "Lab401"
#define LIGHTMSG_DEFAULT_BITMAPPATH  LIGHTMSGCONF_SAVE_FOLDER

#define LIGHTMSG_DEFAULT_MIRROR 0
#define LIGHTMSG_DEFAULT_SPEED  0
#define LIGHTMSG_DEFAULT_WIDTH  1
#define LIGHTMSG_DEFAULT_ACCEL  0
#define LIGHTMSG_DEFAULT_TONE1  0
#define LIGHTMSG_DEFAULT_TONE2  0

#include "401_err.h"
#include "app_params.h"

typedef void (*color_animation_callback)(uint16_t tick, uint32_t* result, void* ctx);
typedef struct {
    char* version;
    char text[LIGHTMSG_MAX_TEXT_LEN + 1];
    char bitmapPath[LIGHTMSG_MAX_BITMAPPATH_LEN + 1];
    uint8_t color;
    uint8_t brightness;
    bool orientation;
    color_animation_callback cb;
    // New features
    bool mirror; // true = mirror, false = no mirror
    uint8_t speed; // speed index
    uint8_t width; // width index
    uint8_t accel; // acceleration index
    uint8_t tone1; // tone index when first interrupt is detected
    uint8_t tone2; // tone index when second interrupt is detected
} Configuration;

l401_err config_alloc(Configuration** config);
void config_default_init(Configuration* config);
l401_err config_to_json(Configuration* config, char** jsontxt);
l401_err json_to_config(char* jsontxt, Configuration* config);
l401_err config_save_json(const char* filename, Configuration* config);
l401_err config_read_json(const char* filename, Configuration* config);
l401_err config_init_dir(const char* filename);
l401_err config_load_json(const char* filename, Configuration* config);

#endif /* end of include guard: 401_CONFIG_H_ */
