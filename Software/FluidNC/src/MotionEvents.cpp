// MotionEvents.cpp
#include "MotionEvents.h"
#include "Config.h" // MAX_N_AXIS
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <cstring>

struct MotionEvent {
    float target[MAX_N_AXIS];
    size_t n_axis;
};

// Small queue to hold recent move submissions. We'll keep it short to avoid memory use.
static QueueHandle_t motion_event_queue = nullptr;
static const size_t MOTION_EVENT_QUEUE_LEN = 8;

static void ensure_queue() {
    if (!motion_event_queue) {
        motion_event_queue = xQueueCreate(MOTION_EVENT_QUEUE_LEN, sizeof(MotionEvent));
    }
}

void motion_event_post(const float* target, size_t n_axis) {
    ensure_queue();
    if (!motion_event_queue) return;

    MotionEvent ev;
    ev.n_axis = (n_axis > MAX_N_AXIS) ? MAX_N_AXIS : n_axis;
    // copy target values (in mm)
    for (size_t i = 0; i < ev.n_axis; ++i) ev.target[i] = target[i];

    // Try to send without blocking. If queue full, drop oldest and push new entry.
    if (xQueueSend(motion_event_queue, &ev, (TickType_t)0) != pdTRUE) {
        // remove one item to make space
        MotionEvent tmp;
        xQueueReceive(motion_event_queue, &tmp, (TickType_t)0);
        xQueueSend(motion_event_queue, &ev, (TickType_t)0);
    }
}

bool motion_event_pop(float* out_target, size_t& out_n_axis) {
    ensure_queue();
    if (!motion_event_queue) return false;

    MotionEvent ev;
    if (xQueueReceive(motion_event_queue, &ev, (TickType_t)0) == pdTRUE) {
        out_n_axis = ev.n_axis;
        for (size_t i = 0; i < ev.n_axis; ++i) out_target[i] = ev.target[i];
        return true;
    }
    return false;
}
