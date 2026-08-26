#pragma once

#include <cstdint>

/**
 * True only when *this caller* is inside the handle's event callback.
 * A handle-wide depth counter is not enough: the delivery task can be in
 * emit() while an application task calls set_tuner_gain_mode().
 */
inline bool esp_rtl_sdr_caller_is_event_callback(uint32_t callback_depth,
                                                 const void *callback_task,
                                                 const void *self_task)
{
    return callback_depth > 0 && callback_task != nullptr && self_task != nullptr &&
           callback_task == self_task;
}
