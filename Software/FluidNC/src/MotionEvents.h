// MotionEvents.h
#pragma once
#include <cstddef>

// Post a motion event (target positions in millimeters). This function is non-blocking
// and safe to call from the protocol/planner task. If the internal queue is full,
// the oldest event will be dropped to make room for the new one.
void motion_event_post(const float* target, size_t n_axis);

// Pop a motion event. Returns true if an event was retrieved. The caller should
// provide a buffer of at least MAX_N_AXIS floats.
bool motion_event_pop(float* out_target, size_t& out_n_axis);
