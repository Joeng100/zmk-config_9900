/*
 * Copyright (c) 2023 ZitaoTech
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <zmk/backlight.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/capslock_state_changed.h>
#include <zmk/events/backlight_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_TRACKPAD_BACKLIGHT)

#define TRACKPAD_NODE DT_CHOSEN(zmk_trackpad_backlight)

static const struct device *trackpad_dev = DEVICE_DT_GET(TRACKPAD_NODE);
static bool is_trackpad_on = true;
static bool is_capslock_on = false;
static struct k_work_delayable trackpad_blink_work;

// 触控板背光闪烁间隔（毫秒）
#define BLINK_INTERVAL CONFIG_ZMK_TRACKPAD_CAPSLOCK_BLINK_INTERVAL

// 触控板背光亮度（0-100）
#define BRIGHTNESS CONFIG_ZMK_TRACKPAD_BACKLIGHT_BRT

static void trackpad_blink_work_handler(struct k_work *work) {
    if (is_capslock_on) {
        static bool blink_state = false;

        if (blink_state) {
            led_set_brightness(trackpad_dev, 0, BRIGHTNESS);
        } else {
            led_set_brightness(trackpad_dev, 0, 0);
        }

        blink_state = !blink_state;
        k_work_schedule(&trackpad_blink_work, K_MSEC(BLINK_INTERVAL));
    } else {
        // 确保当Caps Lock关闭时，触控板保持常亮状态（如果背光已开启）
        if (is_trackpad_on) {
            led_set_brightness(trackpad_dev, 0, BRIGHTNESS);
        } else {
            led_set_brightness(trackpad_dev, 0, 0);
        }
    }
}

static void update_trackpad_backlight(void) {
    if (is_capslock_on) {
        // 启动闪烁工作队列
        k_work_schedule(&trackpad_blink_work, K_NO_WAIT);
    } else {
        // 取消闪烁工作队列
        k_work_cancel_delayable(&trackpad_blink_work);

        // 恢复常亮状态
        if (is_trackpad_on) {
            led_set_brightness(trackpad_dev, 0, BRIGHTNESS);
        } else {
            led_set_brightness(trackpad_dev, 0, 0);
        }
    }
}

static int trackpad_backlight_listener_capslock(const zmk_event_t *eh) {
    const struct capslock_state_changed *ev = as_capslock_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    is_capslock_on = ev->state;
    update_trackpad_backlight();

    return 0;
}

#if IS_ENABLED(CONFIG_ZMK_TRACKPAD_BACKLIGHT_SYNC_BL)
static int trackpad_backlight_listener_backlight(const zmk_event_t *eh) {
    const struct backlight_state_changed *ev = as_backlight_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    is_trackpad_on = ev->state;
    update_trackpad_backlight();

    return 0;
}
#endif

ZMK_LISTENER(trackpad_backlight, trackpad_backlight_listener_capslock);
ZMK_SUBSCRIPTION(trackpad_backlight, zmk_capslock_state_changed);

#if IS_ENABLED(CONFIG_ZMK_TRACKPAD_BACKLIGHT_SYNC_BL)
ZMK_LISTENER(trackpad_backlight_bl, trackpad_backlight_listener_backlight);
ZMK_SUBSCRIPTION(trackpad_backlight_bl, zmk_backlight_state_changed);
#endif

static int trackpad_backlight_init(const struct device *dev) {
    ARG_UNUSED(dev);

    if (!device_is_ready(trackpad_dev)) {
//        LOG_ERR("Trackpad backlight device not ready");
        return -ENODEV;
    }

    k_work_init_delayable(&trackpad_blink_work, trackpad_blink_work_handler);

    // 初始化时设置触控板背光开启
    led_set_brightness(trackpad_dev, 0, BRIGHTNESS);

//    LOG_INF("Trackpad backlight initialized");
    return 0;
}

SYS_INIT(trackpad_backlight_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif // CONFIG_ZMK_TRACKPAD_BACKLIGHT
