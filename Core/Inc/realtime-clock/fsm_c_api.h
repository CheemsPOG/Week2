/**
 * @file realtime-clock/fsm_c_api.h
 * @brief C API wrapper for the C++ ClockFSM finite state machine
 */

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the clock finite state machine
 * Must be called before using fsm_run().
 */
void init_fsm();

/**
 * @brief Run one iteration of the FSM
 * This function should be called periodically (e.g., in the main loop)
 * to process button inputs, update the display, and handle state transitions.
 */
void fsm_run();

#ifdef __cplusplus
}
#endif
