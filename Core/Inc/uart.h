#ifndef INC_UART_H_
#define INC_UART_H_

#include "main.h"
#include "usart.h"
#include <stdint.h>

// ---------------------- Trạng thái cập nhật thời gian ----------------------
typedef enum
{
    TIME_UPDATE_IDLE,
    TIME_UPDATE_HOURS,
    TIME_UPDATE_MINUTES,
    TIME_UPDATE_SECONDS,
    TIME_UPDATE_COMPLETE,
    TIME_UPDATE_ERROR
} TimeUpdateState_t;

// ---------------------- Cấu hình timeout và retry ----------------------
#define TIMEOUT_10S 10000    // 10 giây timeout (ms)
#define MAX_RETRY_COUNT 3    // Số lần retry tối đa
#define INVALID_DATA_RETRY 1 // Retry khi dữ liệu không hợp lệ

// ---------------------- Nguyên mẫu hàm ----------------------
void uart_init_rs232(void);
void uart_Rs232SendString(uint8_t *str);
void uart_Rs232SendNum(uint32_t num);
int uart_read_byte(uint8_t *data);

// Hàm cập nhật thời gian qua UART
void uart_StartTimeUpdate(void);   // Bài 1: Cập nhật thời gian cơ bản
void uart_StartTimeUpdateEx(void); // Bài 2: Cập nhật thời gian với timeout/retry
void uart_ProcessTimeUpdate(void);
void uart_ProcessTimeUpdateEx(void); // Bài 2: Với timeout và retry
int uart_ReadLine(uint8_t *buffer, uint8_t max_len);
uint8_t uart_ParseNumber(uint8_t *str);
uint8_t uart_ValidateInput(uint8_t *str, TimeUpdateState_t state);

// Utility functions
void uart_ClearInputBuffer(void);                                                    // Clear ring buffer và line buffer
void uart_ResetLineBuffer(void);                                                     // Reset static line buffer
void uart_HandleValidationError(TimeUpdateState_t state);                            // Exercise 1 validation error
void uart_HandleValidationErrorEx2(TimeUpdateState_t state);                         // Exercise 2 validation error
void uart_HandleValidationErrorDetailed(uint8_t *input, TimeUpdateState_t state);    // Exercise 1 detailed error
void uart_HandleValidationErrorEx2Detailed(uint8_t *input, TimeUpdateState_t state); // Exercise 2 detailed error

// ---------------------- Biến toàn cục ----------------------
extern volatile uint8_t uart_data_ready;
extern TimeUpdateState_t time_update_state;
extern uint8_t new_time_values[3]; // hours, minutes, seconds

#endif /* INC_UART_H_ */
