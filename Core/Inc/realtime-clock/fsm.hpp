/**
 * @file realtime-clock/fsm.hpp
 * @brief Finite State Machine for a real-time clock with alarm functionality
 */

#pragma once

#include <cstdint>

/**
 * @class ClockFSM
 * @brief Manages clock display, time/alarm editing, and alarm triggering
 * Handles three primary modes: DISPLAY, SET_TIME, and SET_ALARM.
 */
class ClockFSM {
public:
  enum class Mode {
    DISPLAY,
    SET_TIME,
    SET_ALARM
  };
  enum class TimeField {
    HOUR = 0,
    MIN,
    SEC,
    DAY,
    DATE,
    MONTH,
    YEAR,
    COUNT
  };

  ClockFSM();
  ~ClockFSM() = default;

  /**
   * @brief Initialize the FSM to default state
   */
  void init();

  /**
   * @brief Execute one FSM cycle
   */
  void run();

private:
  // ========== Button Indices for Input Handling ===========
  // ========================================================

  static constexpr uint8_t BTN_MODE      = 0;
  static constexpr uint8_t BTN_INCREMENT = 1;
  static constexpr uint8_t BTN_SAVE      = 12;

  // ========== FSM State Variables ===========
  // ==========================================

  Mode      current_mode_;    ///< Current operating mode
  TimeField current_field_;   ///< Currently selected field for editing
  uint8_t   blink_counter_;   ///< Counter for field blinking effect
  bool      show_field_;      ///< Flag to show/hide field during blink
  uint16_t  hold_counter_;    ///< Counter for button hold detection

  struct TempTime {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
  } temp_time_;

  struct Alarm {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    bool    enabled;
    bool    triggered;
  } alarm_;

  /**
   * @brief Handle mode button press
   * Cycles through modes: DISPLAY -> SET_TIME -> SET_ALARM -> DISPLAY
   */
  void handleModeButton();

  /**
   * @brief Handle increment button press
   * Increments the currently selected field in edit modes
   */
  void handleIncrementButton();

  /**
   * @brief Handle save button press
   * Saves current edits and returns to DISPLAY mode
   */
  void handleSaveButton();

  /**
   * @brief Display current mode status on LCD
   */
  void displayModeStatus();

  /**
   * @brief Display time in normal mode
   */
  void displayTimeNormal();

  /**
   * @brief Display time in edit mode with blinking field
   */
  void displayTimeEdit();

  /**
   * @brief Display alarm triggered effect
   */
  void displayAlarmEffect();

  /**
   * @brief Check if alarm conditions are met
   */
  void checkAlarm();

  /**
   * @brief Load current RTC time to temporary buffer
   */
  void loadCurrentTimeToTemp();

  /**
   * @brief Write temporary time buffer to RTC
   */
  void writeTimeToRTC();
};
