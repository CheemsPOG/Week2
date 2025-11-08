#include "realtime-clock/fsm.hpp"

extern "C" {
#include <stdio.h>

#include "button.h"
#include "ds3231.h"
#include "lcd.h"
#include "software_timer.h"
}

// ============================================================================
// ClockFSM Class Implementation
// ============================================================================

ClockFSM::ClockFSM()
    : current_mode_(Mode::DISPLAY),
      current_field_(TimeField::HOUR),
      blink_counter_(0),
      show_field_(true),
      hold_counter_(0) {
  temp_time_ = {0, 0, 0, 1, 1, 1, 0};
  alarm_     = {0, 0, 0, false, false};
}

void ClockFSM::init() {
  current_mode_    = Mode::DISPLAY;
  current_field_   = TimeField::HOUR;
  blink_counter_   = 0;
  show_field_      = true;
  alarm_.enabled   = false;
  alarm_.triggered = false;
  hold_counter_    = 0;
}

void ClockFSM::run() {
  handleModeButton();
  handleIncrementButton();
  handleSaveButton();

  if (current_mode_ != Mode::SET_ALARM) {
    checkAlarm();
  }

  if (alarm_.triggered) {
    displayAlarmEffect();
    return;
  }
  displayModeStatus();

  if (current_mode_ == Mode::DISPLAY) {
    displayTimeNormal();
  } else {
    displayTimeEdit();
  }
}

// ============================================================================
// Private Methods - Button Handlers
// ============================================================================

void ClockFSM::handleModeButton() {
  if (button_count[BTN_MODE] == 1) {
    lcd_Clear(BLACK);

    switch (current_mode_) {
      case Mode::DISPLAY:
        current_mode_  = Mode::SET_TIME;
        current_field_ = TimeField::HOUR;
        loadCurrentTimeToTemp();
        break;

      case Mode::SET_TIME:
        current_mode_  = Mode::SET_ALARM;
        current_field_ = TimeField::HOUR;
        writeTimeToRTC();
        break;

      case Mode::SET_ALARM:
        current_mode_  = Mode::DISPLAY;
        current_field_ = TimeField::HOUR;
        break;
    }
  }
}

void ClockFSM::handleIncrementButton() {
  if (current_mode_ != Mode::DISPLAY) {
    if (button_count[BTN_INCREMENT] > 0) {
      hold_counter_++;

      // First press or fast increment (2s hold, then every 200ms)
      if (button_count[BTN_INCREMENT] == 1 || (hold_counter_ > 40 && hold_counter_ % 4 == 0)) {
        switch (current_field_) {
          case TimeField::HOUR:
            if (current_mode_ == Mode::SET_TIME) {
              temp_time_.hour = (temp_time_.hour + 1) % 24;
            } else {
              alarm_.hour = (alarm_.hour + 1) % 24;
            }
            break;

          case TimeField::MIN:
            if (current_mode_ == Mode::SET_TIME) {
              temp_time_.min = (temp_time_.min + 1) % 60;
            } else {
              alarm_.min = (alarm_.min + 1) % 60;
            }
            break;

          case TimeField::SEC:
            if (current_mode_ == Mode::SET_TIME) {
              temp_time_.sec = (temp_time_.sec + 1) % 60;
            } else {
              alarm_.sec = (alarm_.sec + 1) % 60;
            }
            break;

          case TimeField::DAY:
            if (current_mode_ == Mode::SET_TIME) {
              temp_time_.day = (temp_time_.day % 7) + 1; // 1-7
            }
            break;

          case TimeField::DATE:
            if (current_mode_ == Mode::SET_TIME) {
              temp_time_.date = (temp_time_.date % 31) + 1; // 1-31
            }
            break;

          case TimeField::MONTH:
            if (current_mode_ == Mode::SET_TIME) {
              temp_time_.month = (temp_time_.month % 12) + 1; // 1-12
            }
            break;

          case TimeField::YEAR:
            if (current_mode_ == Mode::SET_TIME) {
              temp_time_.year = (temp_time_.year + 1) % 100; // 0-99
            }
            break;

          default:
            break;
        }
      }
    } else {
      hold_counter_ = 0;
    }
  }
}

void ClockFSM::handleSaveButton() {
  if (current_mode_ != Mode::DISPLAY) {
    if (button_count[BTN_SAVE] == 1) {
      if (current_mode_ == Mode::SET_ALARM) {
        current_field_   = static_cast<TimeField>((static_cast<int>(current_field_) + 1) % 3);
        alarm_.enabled   = true;
        alarm_.triggered = false;
      } else {
        current_field_ = static_cast<TimeField>((static_cast<int>(current_field_) + 1) %
                                                static_cast<int>(TimeField::COUNT));
      }
    }
  }
}

// ============================================================================
// Private Methods - RTC Operations
// ============================================================================

void ClockFSM::loadCurrentTimeToTemp() {
  temp_time_.hour  = ds3231_hours;
  temp_time_.min   = ds3231_min;
  temp_time_.sec   = ds3231_sec;
  temp_time_.day   = ds3231_day;
  temp_time_.date  = ds3231_date;
  temp_time_.month = ds3231_month;
  temp_time_.year  = ds3231_year;
}

void ClockFSM::writeTimeToRTC() {
  ds3231_Write(ADDRESS_HOUR, temp_time_.hour);
  ds3231_Write(ADDRESS_MIN, temp_time_.min);
  ds3231_Write(ADDRESS_SEC, temp_time_.sec);
  ds3231_Write(ADDRESS_DAY, temp_time_.day);
  ds3231_Write(ADDRESS_DATE, temp_time_.date);
  ds3231_Write(ADDRESS_MONTH, temp_time_.month);
  ds3231_Write(ADDRESS_YEAR, temp_time_.year);
}

// ============================================================================
// Private Methods - Alarm
// ============================================================================

void ClockFSM::checkAlarm() {
  if (alarm_.enabled && !alarm_.triggered) {
    static uint8_t last_second = 255;

    if (last_second != ds3231_sec) {
      last_second = ds3231_sec;

      // Decrement seconds
      if (alarm_.sec > 0) {
        alarm_.sec--;
      } else {
        alarm_.sec = 59;

        if (alarm_.min > 0) {
          alarm_.min--;
        } else {
          alarm_.min = 59;

          if (alarm_.hour > 0) {
            alarm_.hour--;
          } else {
            alarm_.triggered = true;
            alarm_.enabled   = false;
          }
        }
      }
    }
  }
}

void ClockFSM::displayAlarmEffect() {
  static uint8_t alarm_blink = 0;
  alarm_blink                = (alarm_blink + 1) % 10;

  if (alarm_blink < 5) {
    lcd_Fill(0, 0, 240, 320, RED);
    lcd_ShowStr(30, 160, (char*)"!!! TIME UP !!!", WHITE, RED, 24, 1);
  } else {
    lcd_Fill(0, 0, 240, 320, BLACK);
    lcd_ShowStr(30, 160, (char*)"!!! TIME UP !!!", RED, BLACK, 24, 1);
  }

  for (int i = 0; i < 16; i++) {
    if (button_count[i] == 1) {
      alarm_.triggered = false;
      lcd_Clear(BLACK);
      break;
    }
  }
}

// ============================================================================
// Private Methods - Display
// ============================================================================

void ClockFSM::displayModeStatus() {
  char mode_str[20];

  switch (current_mode_) {
    case Mode::DISPLAY:
      sprintf(mode_str, "MODE: DISPLAY   ");
      break;
    case Mode::SET_TIME:
      sprintf(mode_str, "MODE: SET TIME  ");
      break;
    case Mode::SET_ALARM:
      sprintf(mode_str, "MODE: SET TIMER ");
      break;
  }

  lcd_ShowStr(10, 10, mode_str, WHITE, BLACK, 16, 0);
}

void ClockFSM::displayTimeNormal() {
  char           buffer[32];
  static uint8_t last_sec   = 255; // Initialize to invalid value
  static uint8_t last_min   = 255;
  static uint8_t last_hour  = 255;
  static uint8_t last_day   = 255;
  static uint8_t last_date  = 255;
  static uint8_t last_month = 255;
  static uint8_t last_year  = 255;
  static Mode    last_mode  = Mode::DISPLAY;

  // force redraw when returning from another mode
  if (last_mode != current_mode_) {
    last_sec = last_min = last_hour = 255;
    last_day = last_date = last_month = last_year = 255;
    last_mode                                     = current_mode_;
  }

  // clear and redraw time if last values changed
  if (last_sec != ds3231_sec || last_min != ds3231_min || last_hour != ds3231_hours) {
    lcd_Fill(60, 115, 180, 145, BLACK); // Clear time area
    sprintf(buffer, "%02d:%02d:%02d", ds3231_hours, ds3231_min, ds3231_sec);
    lcd_ShowStr(70, 120, buffer, GREEN, BLACK, 24, 1);
    last_sec  = ds3231_sec;
    last_min  = ds3231_min;
    last_hour = ds3231_hours;
  }
  if (last_day != ds3231_day || last_date != ds3231_date || last_month != ds3231_month ||
      last_year != ds3231_year) {
    lcd_Fill(60, 145, 200, 160, BLACK); // Clear date area
    const char* day_names[] = {"", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    sprintf(buffer,
            "%s %02d/%02d/%02d",
            day_names[ds3231_day],
            ds3231_date,
            ds3231_month,
            ds3231_year);
    lcd_ShowStr(70, 150, buffer, YELLOW, BLACK, 16, 1);
    last_day   = ds3231_day;
    last_date  = ds3231_date;
    last_month = ds3231_month;
    last_year  = ds3231_year;
  }
  static uint8_t last_alarm_hour    = 255;
  static uint8_t last_alarm_min     = 255;
  static uint8_t last_alarm_sec     = 255;
  static bool    last_alarm_enabled = false;

  bool timer_changed = (last_alarm_hour != alarm_.hour || last_alarm_min != alarm_.min ||
                        last_alarm_sec != alarm_.sec);

  if (last_alarm_enabled != alarm_.enabled || (alarm_.enabled && timer_changed)) {
    lcd_Fill(10, 235, 220, 255, BLACK);

    if (alarm_.enabled) {
      sprintf(buffer, "TIMER: %02d:%02d:%02d", alarm_.hour, alarm_.min, alarm_.sec);
      lcd_ShowStr(10, 270, buffer, CYAN, BLACK, 16, 0);
    }

    last_alarm_hour    = alarm_.hour;
    last_alarm_min     = alarm_.min;
    last_alarm_sec     = alarm_.sec;
    last_alarm_enabled = alarm_.enabled;
  }
}

void ClockFSM::displayTimeEdit() {
  // Update blink counter (2Hz = toggle every 250ms, 50ms timer * 5 = 250ms)
  blink_counter_ = (blink_counter_ + 1) % 5;
  if (blink_counter_ == 0) {
    show_field_ = !show_field_;
  }

  char     buffer[32];
  uint16_t color_hour = WHITE, color_min = WHITE, color_sec = WHITE;
  uint16_t color_day = WHITE, color_date = WHITE, color_month = WHITE, color_year = WHITE;

  static bool    last_show_field = true;
  static uint8_t last_temp_hour = 255, last_temp_min = 255, last_temp_sec = 255;
  static uint8_t last_temp_day = 255, last_temp_date = 255, last_temp_month = 255,
                 last_temp_year    = 255;
  static uint8_t   last_alarm_hour = 255, last_alarm_min = 255, last_alarm_sec = 255;
  static TimeField last_field = TimeField::COUNT;

  bool need_redraw = (show_field_ != last_show_field) || (current_field_ != last_field);

  if (!show_field_) {
    switch (current_field_) {
      case TimeField::HOUR:
        color_hour = BLACK;
        break;
      case TimeField::MIN:
        color_min = BLACK;
        break;
      case TimeField::SEC:
        color_sec = BLACK;
        break;
      case TimeField::DAY:
        color_day = BLACK;
        break;
      case TimeField::DATE:
        color_date = BLACK;
        break;
      case TimeField::MONTH:
        color_month = BLACK;
        break;
      case TimeField::YEAR:
        color_year = BLACK;
        break;
      default:
        break;
    }
  }

  if (current_mode_ == Mode::SET_TIME) {
    if (need_redraw || last_temp_hour != temp_time_.hour || last_temp_min != temp_time_.min ||
        last_temp_sec != temp_time_.sec) {
      lcd_Fill(75, 95, 180, 125, BLACK);

      sprintf(buffer, "%02d", temp_time_.hour);
      lcd_ShowStr(80, 100, buffer, color_hour, BLACK, 24, 0);
      lcd_ShowStr(104, 100, (char*)":", WHITE, BLACK, 24, 0);
      sprintf(buffer, "%02d", temp_time_.min);
      lcd_ShowStr(116, 100, buffer, color_min, BLACK, 24, 0);
      lcd_ShowStr(140, 100, (char*)":", WHITE, BLACK, 24, 0);
      sprintf(buffer, "%02d", temp_time_.sec);
      lcd_ShowStr(152, 100, buffer, color_sec, BLACK, 24, 0);

      last_temp_hour = temp_time_.hour;
      last_temp_min  = temp_time_.min;
      last_temp_sec  = temp_time_.sec;
    }

    if (need_redraw || last_temp_day != temp_time_.day || last_temp_date != temp_time_.date ||
        last_temp_month != temp_time_.month || last_temp_year != temp_time_.year) {
      lcd_Fill(45, 135, 195, 160, BLACK);

      const char* day_names[] = {"", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
      sprintf(buffer, "%s", day_names[temp_time_.day]);
      lcd_ShowStr(50, 140, buffer, color_day, BLACK, 16, 0);

      sprintf(buffer, "%02d", temp_time_.date);
      lcd_ShowStr(100, 140, buffer, color_date, BLACK, 16, 0);
      lcd_ShowStr(122, 140, (char*)"/", WHITE, BLACK, 16, 0);
      sprintf(buffer, "%02d", temp_time_.month);
      lcd_ShowStr(130, 140, buffer, color_month, BLACK, 16, 0);
      lcd_ShowStr(152, 140, (char*)"/", WHITE, BLACK, 16, 0);
      sprintf(buffer, "%02d", temp_time_.year);
      lcd_ShowStr(160, 140, buffer, color_year, BLACK, 16, 0);

      last_temp_day   = temp_time_.day;
      last_temp_date  = temp_time_.date;
      last_temp_month = temp_time_.month;
      last_temp_year  = temp_time_.year;
    }

  } else if (current_mode_ == Mode::SET_ALARM) {
    if (need_redraw || last_alarm_hour != alarm_.hour || last_alarm_min != alarm_.min ||
        last_alarm_sec != alarm_.sec) {
      lcd_Fill(75, 95, 180, 125, BLACK);

      sprintf(buffer, "%02d", alarm_.hour);
      lcd_ShowStr(80, 100, buffer, color_hour, BLACK, 24, 0);
      lcd_ShowStr(104, 100, (char*)":", WHITE, BLACK, 24, 0);
      sprintf(buffer, "%02d", alarm_.min);
      lcd_ShowStr(116, 100, buffer, color_min, BLACK, 24, 0);
      lcd_ShowStr(140, 100, (char*)":", WHITE, BLACK, 24, 0);
      sprintf(buffer, "%02d", alarm_.sec);
      lcd_ShowStr(152, 100, buffer, color_sec, BLACK, 24, 0);

      last_alarm_hour = alarm_.hour;
      last_alarm_min  = alarm_.min;
      last_alarm_sec  = alarm_.sec;
    }
  }

  last_show_field = show_field_;
  last_field      = current_field_;
}
