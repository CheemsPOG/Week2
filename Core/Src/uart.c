/*
 * uart.c
 * Hoàn thiện theo yêu cầu Bài 1 - Chương 5
 * Sử dụng ring buffer để lưu dữ liệu nhận UART
 * Thêm chức năng cập nhật thời gian qua RS232
 */

#include "uart.h"
#include "lcd.h"
#include "ds3231.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---------------------- Cấu hình bộ đệm vòng ----------------------
#define UART_BUFFER_SIZE 128

uint8_t rx_byte;                       // Byte tạm để nhận ISR
uint8_t uart_buffer[UART_BUFFER_SIZE]; // Bộ đệm vòng
volatile uint16_t uart_head = 0;       // Con trỏ ghi
volatile uint16_t uart_tail = 0;       // Con trỏ đọc
volatile uint8_t uart_data_ready = 0;  // Cờ báo có dữ liệu mới

// ---------------------- Biến cho cập nhật thời gian ----------------------
TimeUpdateState_t time_update_state = TIME_UPDATE_IDLE;
uint8_t new_time_values[3] = {0}; // hours, minutes, seconds
uint8_t response_buffer[32];      // Buffer để lưu response từ máy tính

// ---------------------- Biến cho Bài 2: Timeout và Retry ----------------------
static uint32_t request_start_time = 0; // Thời điểm gửi request
static uint8_t retry_count = 0;         // Số lần retry hiện tại
static uint8_t use_advanced_mode = 0;   // 1 = Bài 2, 0 = Bài 1

// ---------------------- Hàm khởi tạo UART ----------------------
void uart_init_rs232(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1); // Bắt đầu nhận 1 byte đầu tiên
}

// ---------------------- Ngắt UART Receive ----------------------
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint16_t next = (uart_head + 1) % UART_BUFFER_SIZE;
        if (next != uart_tail)
        {                                     // Kiểm tra tràn bộ đệm
            uart_buffer[uart_head] = rx_byte; // Ghi byte nhận vào buffer
            uart_head = next;
            uart_data_ready = 1; // Báo có dữ liệu mới
        }
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1); // Tiếp tục nhận byte kế tiếp
    }
}

// ---------------------- Đọc 1 byte từ ring buffer ----------------------
int uart_read_byte(uint8_t *data)
{
    if (uart_head == uart_tail)
        return 0; // Buffer rỗng
    *data = uart_buffer[uart_tail];
    uart_tail = (uart_tail + 1) % UART_BUFFER_SIZE;
    return 1; // Đọc thành công
}

// ---------------------- Gửi chuỗi ký tự ----------------------
void uart_Rs232SendString(uint8_t *str)
{
    HAL_UART_Transmit(&huart1, str, strlen((char *)str), HAL_MAX_DELAY);
}

// ---------------------- Gửi số nguyên ----------------------
void uart_Rs232SendNum(uint32_t num)
{
    char msg[16];
    sprintf(msg, "%lu", (unsigned long)num);
    uart_Rs232SendString((uint8_t *)msg);
}

// ---------------------- Đọc một dòng từ UART ----------------------
static uint8_t line_buffer[32];
static uint8_t line_index = 0;
static uint8_t line_ready = 0;

// Function to reset static variables
void uart_ResetLineBuffer(void)
{
    // DEBUG: Check if buffer had old data
    if (line_index > 0 || line_buffer[0] != 0)
    {
        char old_debug[50];
        sprintf(old_debug, "DEBUG: Clearing old buffer: [%s] idx=%d\n", line_buffer, line_index);
        uart_Rs232SendString((uint8_t *)old_debug);
    }

    line_index = 0;
    line_ready = 0;
    memset(line_buffer, 0, sizeof(line_buffer));
}

int uart_ReadLine(uint8_t *buffer, uint8_t max_len)
{
    uint8_t byte;

    // Nếu đã có dòng sẵn sàng, trả về nó
    if (line_ready)
    {
        strncpy((char *)buffer, (char *)line_buffer, max_len - 1);
        buffer[max_len - 1] = '\0';
        line_ready = 0;
        line_index = 0;
        memset(line_buffer, 0, sizeof(line_buffer)); // FIX: Clear buffer sau khi dùng
        return 1;
    }

    // Đọc byte mới từ buffer
    while (uart_read_byte(&byte))
    {
        // Bỏ qua ký tự không hợp lệ và reset nếu có ký tự lạ
        if (byte < 0x20 && byte != '\n' && byte != '\r')
        {
            continue;
        }

        if (byte == '\n' || byte == '\r')
        {
            if (line_index > 0)
            {
                line_buffer[line_index] = '\0'; // Đảm bảo null-terminated
                strncpy((char *)buffer, (char *)line_buffer, max_len - 1);
                buffer[max_len - 1] = '\0';

                // FIX: Reset buffer ngay lập tức để tránh concat
                line_index = 0;
                memset(line_buffer, 0, sizeof(line_buffer));

                return 1; // Trả về ngay lập tức
            }
        }
        else if (byte >= 0x20 && byte <= 0x7E) // Accept all printable ASCII characters
        {
            // Accept all printable characters, let validation handle correctness
            if (line_index < sizeof(line_buffer) - 1)
            {
                line_buffer[line_index++] = byte;
            }
            else
            {
                // Buffer đầy, reset để tránh overflow
                line_index = 0;
                memset(line_buffer, 0, sizeof(line_buffer));
                line_buffer[line_index++] = byte;
            }
        }
        // Note: Non-printable characters (except \n, \r) are ignored (continue in while loop)
    }

    return 0; // Chưa đọc đủ một dòng
}

// ---------------------- Validation dữ liệu đầu vào cho Bài 2 ----------------------
// Enhanced validation with specific error types
typedef enum
{
    VALIDATION_OK = 1,
    VALIDATION_EMPTY = 2,
    VALIDATION_NON_NUMERIC = 3,
    VALIDATION_OUT_OF_RANGE = 4
} ValidationResult_t;

ValidationResult_t uart_ValidateInputDetailed(uint8_t *str, TimeUpdateState_t state)
{
    // Kiểm tra chuỗi rỗng
    if (str == NULL || strlen((char *)str) == 0)
    {
        return VALIDATION_EMPTY;
    }

    // Kiểm tra chỉ chứa số
    for (int i = 0; i < strlen((char *)str); i++)
    {
        if (str[i] < '0' || str[i] > '9')
        {
            return VALIDATION_NON_NUMERIC;
        }
    }

    int num = atoi((char *)str);

    // Validation theo trạng thái
    switch (state)
    {
    case TIME_UPDATE_HOURS:
        return (num >= 0 && num <= 23) ? VALIDATION_OK : VALIDATION_OUT_OF_RANGE;
    case TIME_UPDATE_MINUTES:
        return (num >= 0 && num <= 59) ? VALIDATION_OK : VALIDATION_OUT_OF_RANGE;
    case TIME_UPDATE_SECONDS:
        return (num >= 0 && num <= 59) ? VALIDATION_OK : VALIDATION_OUT_OF_RANGE;
    default:
        return VALIDATION_OUT_OF_RANGE;
    }
}

// Backward compatibility function
uint8_t uart_ValidateInput(uint8_t *str, TimeUpdateState_t state)
{
    return (uart_ValidateInputDetailed(str, state) == VALIDATION_OK) ? 1 : 0;
}

// Enhanced validation error handler with specific messages
void uart_HandleValidationErrorDetailed(uint8_t *input, TimeUpdateState_t state)
{
    uart_ClearInputBuffer(); // Clear buffers

    ValidationResult_t result = uart_ValidateInputDetailed(input, state);

    switch (result)
    {
    case VALIDATION_EMPTY:
        uart_Rs232SendString((uint8_t *)"ERROR: Empty input! Please enter a number.\n");
        break;

    case VALIDATION_NON_NUMERIC:
        uart_Rs232SendString((uint8_t *)"ERROR: Non-numeric characters detected! Only numbers (0-9) allowed.\n");
        break;

    case VALIDATION_OUT_OF_RANGE:
        uart_Rs232SendString((uint8_t *)"ERROR: Number out of range! ");
        switch (state)
        {
        case TIME_UPDATE_HOURS:
            uart_Rs232SendString((uint8_t *)"Hours must be 0-23.\n");
            break;
        case TIME_UPDATE_MINUTES:
            uart_Rs232SendString((uint8_t *)"Minutes must be 0-59.\n");
            break;
        case TIME_UPDATE_SECONDS:
            uart_Rs232SendString((uint8_t *)"Seconds must be 0-59.\n");
            break;
        default:
            uart_Rs232SendString((uint8_t *)"Invalid range.\n");
            break;
        }
        break;

    default:
        uart_Rs232SendString((uint8_t *)"ERROR: Invalid input.\n");
        break;
    }

    // Send prompt again
    switch (state)
    {
    case TIME_UPDATE_HOURS:
        uart_Rs232SendString((uint8_t *)"Hours\n");
        break;
    case TIME_UPDATE_MINUTES:
        uart_Rs232SendString((uint8_t *)"Minutes\n");
        break;
    case TIME_UPDATE_SECONDS:
        uart_Rs232SendString((uint8_t *)"Seconds\n");
        break;
    default:
        break;
    }
}

// Main validation error handler (used by both Exercise 1 and 2)
void uart_HandleValidationError(TimeUpdateState_t state)
{
    uart_ClearInputBuffer();
    uart_Rs232SendString((uint8_t *)"ERROR: Invalid data! ");
    switch (state)
    {
    case TIME_UPDATE_HOURS:
        uart_Rs232SendString((uint8_t *)"Hours must be 0-23.\n");
        uart_Rs232SendString((uint8_t *)"Hours\n");
        break;
    case TIME_UPDATE_MINUTES:
        uart_Rs232SendString((uint8_t *)"Minutes must be 0-59.\n");
        uart_Rs232SendString((uint8_t *)"Minutes\n");
        break;
    case TIME_UPDATE_SECONDS:
        uart_Rs232SendString((uint8_t *)"Seconds must be 0-59.\n");
        uart_Rs232SendString((uint8_t *)"Seconds\n");
        break;
    default:
        uart_Rs232SendString((uint8_t *)"Invalid input.\n");
        break;
    }
}

// ---------------------- Chuyển đổi chuỗi thành số với validation ----------------------
uint8_t uart_ParseNumber(uint8_t *str)
{
    int num = atoi((char *)str);

    // Validation theo từng trạng thái
    switch (time_update_state)
    {
    case TIME_UPDATE_HOURS:
        if (num < 0 || num > 23)
            num = 0;
        break;
    case TIME_UPDATE_MINUTES:
    case TIME_UPDATE_SECONDS:
        if (num < 0 || num > 59)
            num = 0;
        break;
    default:
        if (num < 0)
            num = 0;
        if (num > 255)
            num = 255;
        break;
    }

    return (uint8_t)num;
}

// ---------------------- Bắt đầu quá trình cập nhật thời gian (Bài 1) ----------------------
void uart_StartTimeUpdate(void)
{
    use_advanced_mode = 0; // Bài 1: Có timeout nhưng đơn giản hơn bài 2
    retry_count = 0;
    request_start_time = HAL_GetTick(); // FIX: Thêm timer cho bài 1
    time_update_state = TIME_UPDATE_HOURS;

    // Clear all buffers before starting
    uart_ClearInputBuffer(); // This already calls uart_ResetLineBuffer()

    // DEBUG: Thông báo bắt đầu với buffer clean
    uart_Rs232SendString((uint8_t *)"DEBUG: Started Bai1 - Buffer cleared\n");

    lcd_Clear(BLACK);
    lcd_ShowStr(10, 100, "TIME UPDATE MODE", WHITE, BLACK, 16, 0);
    lcd_ShowStr(10, 120, "Updating hours...", WHITE, BLACK, 16, 0);
    lcd_ShowStr(10, 140, "Enter hour (0-23):", YELLOW, BLACK, 14, 0);
    uart_Rs232SendString((uint8_t *)"Hours\n");
}

// ---------------------- Clear UART input buffer ----------------------
void uart_ClearInputBuffer(void)
{
    // Clear ring buffer
    uart_head = uart_tail; // Reset pointers để xóa dữ liệu cũ
    uart_data_ready = 0;

    // Clear line buffer
    uart_ResetLineBuffer();
}

// ---------------------- Bắt đầu quá trình cập nhật thời gian (Bài 2) ----------------------
void uart_StartTimeUpdateEx(void)
{
    use_advanced_mode = 1; // Bài 2: Có timeout/retry
    retry_count = 0;
    request_start_time = HAL_GetTick();
    time_update_state = TIME_UPDATE_HOURS;

    // Clear all buffers before starting
    uart_ClearInputBuffer(); // This already calls uart_ResetLineBuffer()

    // DEBUG: Thông báo đã bắt đầu bài 2
    uart_Rs232SendString((uint8_t *)"DEBUG: Started Exercise 2 - Buffer cleared\n");

    lcd_Clear(BLACK);
    lcd_ShowStr(10, 80, "TIME UPDATE MODE", WHITE, BLACK, 16, 0);
    lcd_ShowStr(10, 100, "** EXERCISE 2 **", CYAN, BLACK, 14, 0);
    lcd_ShowStr(10, 120, "Timeout: 10s", YELLOW, BLACK, 12, 0);
    lcd_ShowStr(10, 135, "Max retry: 3", YELLOW, BLACK, 12, 0);
    lcd_ShowStr(10, 155, "Updating hours...", WHITE, BLACK, 14, 0);
    lcd_ShowStr(10, 170, "Enter hour (0-23):", GREEN, BLACK, 12, 0);
    uart_Rs232SendString((uint8_t *)"Hours\n");
}

// ---------------------- Xử lý quá trình cập nhật thời gian (Bài 1) ----------------------
void uart_ProcessTimeUpdate(void)
{
    // DEBUG: Thêm debug để kiểm tra mode
    static uint8_t debug_sent = 0;
    if (!debug_sent && use_advanced_mode)
    {
        uart_Rs232SendString((uint8_t *)"DEBUG: Processing in advanced mode (Ex2)\n");
        debug_sent = 1;
    }

    if (use_advanced_mode)
    {
        uart_ProcessTimeUpdateEx(); // Chuyển sang Bài 2
        return;
    }

    if (time_update_state == TIME_UPDATE_IDLE || time_update_state == TIME_UPDATE_COMPLETE)
    {
        return;
    }

    // FIX: Thêm timeout cho Bài 1 (đơn giản hơn Bài 2)
    uint32_t current_time = HAL_GetTick();
    uint32_t elapsed_time = current_time - request_start_time;

    // Kiểm tra timeout 10s cho Bài 1
    if (elapsed_time >= TIMEOUT_10S)
    {
        retry_count++;

        // Hiển thị thông báo timeout trên cả UART và LCD
        char timeout_msg[50];
        sprintf(timeout_msg, "Timeout! Please enter input (%d/3)", retry_count);
        uart_Rs232SendString((uint8_t *)timeout_msg);
        uart_Rs232SendString((uint8_t *)"\n");

        // Hiển thị timeout warning trên LCD
        char lcd_timeout_msg[30];
        sprintf(lcd_timeout_msg, "TIMEOUT %d/3!", retry_count);
        lcd_ShowStr(10, 160, lcd_timeout_msg, RED, BLACK, 14, 0);
        lcd_ShowStr(10, 180, "Please enter input...", YELLOW, BLACK, 12, 0);

        if (retry_count >= MAX_RETRY_COUNT)
        {
            // Quá 3 lần, hiển thị lỗi chi tiết trên LCD và UART
            uart_Rs232SendString((uint8_t *)"ERROR: Timeout exceeded! Returning to normal mode...\n");

            // Hiển thị lỗi trên LCD với nhiều dòng thông tin
            lcd_ShowStr(10, 160, "ERROR: 3x TIMEOUT!", RED, BLACK, 16, 0);
            lcd_ShowStr(10, 180, "No input received", WHITE, BLACK, 14, 0);
            lcd_ShowStr(10, 200, "Returning to clock...", YELLOW, BLACK, 12, 0);

            // Hiển thị thông tin trạng thái lỗi
            char error_step[30];
            switch (time_update_state)
            {
            case TIME_UPDATE_HOURS:
                sprintf(error_step, "Failed at: Hours step");
                break;
            case TIME_UPDATE_MINUTES:
                sprintf(error_step, "Failed at: Minutes step");
                break;
            case TIME_UPDATE_SECONDS:
                sprintf(error_step, "Failed at: Seconds step");
                break;
            default:
                sprintf(error_step, "Failed at: Unknown step");
                break;
            }
            lcd_ShowStr(10, 220, error_step, CYAN, BLACK, 12, 0);

            // Clear all buffers before returning to normal mode
            uart_ClearInputBuffer(); // This already calls uart_ResetLineBuffer()

            HAL_Delay(3000); // Tăng thời gian hiển thị để đọc được lỗi
            time_update_state = TIME_UPDATE_IDLE;
            retry_count = 0;
            return;
        }

        // Clear buffers and reset timer for retry
        uart_ClearInputBuffer(); // Clear any partial input that might have caused timeout
        request_start_time = HAL_GetTick();
        switch (time_update_state)
        {
        case TIME_UPDATE_HOURS:
            uart_Rs232SendString((uint8_t *)"Please enter hours (0-23): ");
            break;
        case TIME_UPDATE_MINUTES:
            uart_Rs232SendString((uint8_t *)"Please enter minutes (0-59): ");
            break;
        case TIME_UPDATE_SECONDS:
            uart_Rs232SendString((uint8_t *)"Please enter seconds (0-59): ");
            break;
        default:
            break;
        }
        return;
    }

    if (uart_ReadLine(response_buffer, sizeof(response_buffer)))
    {
        // Debug: Echo chuỗi nhận được trước khi parse
        char debug_msg[60];
        sprintf(debug_msg, "Raw input: [%s] (len=%d)\n", response_buffer, (int)strlen((char *)response_buffer));
        uart_Rs232SendString((uint8_t *)debug_msg);

        // Validation dữ liệu (cũng áp dụng cho Bài 1)
        if (!uart_ValidateInput(response_buffer, time_update_state))
        {
            // Dữ liệu không hợp lệ, reset timer và yêu cầu nhập lại
            request_start_time = HAL_GetTick(); // Reset timer khi validation fail
            uart_HandleValidationErrorDetailed(response_buffer, time_update_state);
            return; // Không xử lý tiếp, chờ input mới
        }

        uint8_t value = uart_ParseNumber(response_buffer);

        // Echo lại giá trị nhận được để xác nhận
        uart_Rs232SendString((uint8_t *)"Received: ");
        uart_Rs232SendNum(value);
        uart_Rs232SendString((uint8_t *)"\n");

        switch (time_update_state)
        {
        case TIME_UPDATE_HOURS:
            new_time_values[0] = value;
            time_update_state = TIME_UPDATE_MINUTES;
            retry_count = 0;                    // Reset retry count cho bước mới
            request_start_time = HAL_GetTick(); // Reset timer cho bước tiếp theo
            lcd_ShowStr(10, 120, "Updating minutes...", WHITE, BLACK, 16, 0);
            lcd_ShowStr(10, 140, "Enter minute (0-59):", YELLOW, BLACK, 14, 0);
            // Clear timeout messages
            lcd_ShowStr(10, 160, "                        ", BLACK, BLACK, 14, 0);
            lcd_ShowStr(10, 180, "                        ", BLACK, BLACK, 12, 0);
            uart_Rs232SendString((uint8_t *)"Minutes\n");
            break;

        case TIME_UPDATE_MINUTES:
            new_time_values[1] = value;
            time_update_state = TIME_UPDATE_SECONDS;
            retry_count = 0;                    // Reset retry count cho bước mới
            request_start_time = HAL_GetTick(); // Reset timer cho bước tiếp theo
            lcd_ShowStr(10, 120, "Updating seconds...", WHITE, BLACK, 16, 0);
            lcd_ShowStr(10, 140, "Enter second (0-59):", YELLOW, BLACK, 14, 0);
            // Clear timeout messages
            lcd_ShowStr(10, 160, "                        ", BLACK, BLACK, 14, 0);
            lcd_ShowStr(10, 180, "                        ", BLACK, BLACK, 12, 0);
            uart_Rs232SendString((uint8_t *)"Seconds\n");
            break;

        case TIME_UPDATE_SECONDS:
            new_time_values[2] = value;

            // Chỉ cập nhật giờ, phút, giây vào DS3231 (giữ nguyên ngày tháng năm)
            ds3231_Write(ADDRESS_HOUR, new_time_values[0]);
            ds3231_Write(ADDRESS_MIN, new_time_values[1]);
            ds3231_Write(ADDRESS_SEC, new_time_values[2]);

            time_update_state = TIME_UPDATE_COMPLETE;
            lcd_ShowStr(10, 120, "Time update completed!", WHITE, BLACK, 16, 0);
            lcd_ShowStr(10, 140, "                        ", WHITE, BLACK, 14, 0);
            uart_Rs232SendString((uint8_t *)"Time update completed!\n");

            // Clear all buffers after completion
            uart_ClearInputBuffer(); // This already calls uart_ResetLineBuffer()

            // Sau 2 giây sẽ quay về chế độ bình thường
            HAL_Delay(2000);
            time_update_state = TIME_UPDATE_IDLE;
            break;

        default:
            time_update_state = TIME_UPDATE_IDLE;
            break;
        }
    }
}

// ---------------------- Xử lý quá trình cập nhật thời gian với Timeout và Retry (Bài 2) ----------------------
void uart_ProcessTimeUpdateEx(void)
{
    if (time_update_state == TIME_UPDATE_IDLE ||
        time_update_state == TIME_UPDATE_COMPLETE ||
        time_update_state == TIME_UPDATE_ERROR)
    {
        return;
    }

    // DEBUG: Thêm debug để xác nhận hàm được gọi
    static uint8_t entry_debug_sent = 0;
    if (!entry_debug_sent)
    {
        uart_Rs232SendString((uint8_t *)"DEBUG: Entered uart_ProcessTimeUpdateEx()\n");
        entry_debug_sent = 1;
    }

    uint32_t current_time = HAL_GetTick();
    uint32_t elapsed_time = current_time - request_start_time;

    // DEBUG: Hiển thị thời gian elapsed mỗi 1 giây
    static uint32_t last_debug_time = 0;
    if (current_time - last_debug_time >= 1000) // Mỗi 1 giây
    {
        last_debug_time = current_time;
        char debug_msg[50];
        sprintf(debug_msg, "Waiting... %lus", elapsed_time / 1000);
        lcd_ShowStr(10, 205, debug_msg, CYAN, BLACK, 10, 0);
    }

    // Kiểm tra timeout (10 giây)
    if (elapsed_time >= TIMEOUT_10S)
    {
        retry_count++;

        // Hiển thị retry trên LCD
        char retry_msg[32];
        sprintf(retry_msg, "Timeout! Retry %d/%d", retry_count, MAX_RETRY_COUNT);
        lcd_ShowStr(10, 190, retry_msg, RED, BLACK, 12, 0);

        // DEBUG: Gửi thông báo qua UART
        uart_Rs232SendString((uint8_t *)"DEBUG: TIMEOUT occurred!\n");

        if (retry_count >= MAX_RETRY_COUNT)
        {
            // Quá số lần retry, báo lỗi
            time_update_state = TIME_UPDATE_ERROR;
            lcd_ShowStr(10, 205, "ERROR: No response!", RED, BLACK, 14, 0);
            lcd_ShowStr(10, 220, "Returning to normal...", WHITE, BLACK, 12, 0);
            uart_Rs232SendString((uint8_t *)"ERROR: Timeout after 3 retries!\n");

            // Clear all buffers after error
            uart_ClearInputBuffer(); // This already calls uart_ResetLineBuffer()

            // Quay về chế độ bình thường sau 3 giây
            HAL_Delay(3000);
            time_update_state = TIME_UPDATE_IDLE;
            retry_count = 0;
            return;
        }

        // Clear buffers and reset timer for retry
        uart_ClearInputBuffer(); // Clear any partial input that might have caused timeout
        request_start_time = HAL_GetTick();
        uart_Rs232SendString((uint8_t *)"TIMEOUT: Resending request...\n");
        switch (time_update_state)
        {
        case TIME_UPDATE_HOURS:
            uart_Rs232SendString((uint8_t *)"Hours\n");
            break;
        case TIME_UPDATE_MINUTES:
            uart_Rs232SendString((uint8_t *)"Minutes\n");
            break;
        case TIME_UPDATE_SECONDS:
            uart_Rs232SendString((uint8_t *)"Seconds\n");
            break;
        default:
            break;
        }
        return;
    }

    // Xử lý response
    if (uart_ReadLine(response_buffer, sizeof(response_buffer)))
    {
        // Reset retry count khi có response
        retry_count = 0;

        // Debug: Echo chuỗi nhận được
        char debug_msg[60];
        sprintf(debug_msg, "Raw input: [%s] (len=%d)\n", response_buffer, (int)strlen((char *)response_buffer));
        uart_Rs232SendString((uint8_t *)debug_msg);

        // Validation dữ liệu
        if (!uart_ValidateInput(response_buffer, time_update_state))
        {
            // Dữ liệu không hợp lệ, gửi lại request
            lcd_ShowStr(10, 190, "Invalid data! Retry...", RED, BLACK, 12, 0);
            uart_HandleValidationErrorDetailed(response_buffer, time_update_state);

            // Reset timer và gửi lại request ngay lập tức
            request_start_time = HAL_GetTick(); // FIX: Reset timer đúng cách
            switch (time_update_state)
            {
            case TIME_UPDATE_HOURS:
                uart_Rs232SendString((uint8_t *)"Hours\n");
                break;
            case TIME_UPDATE_MINUTES:
                uart_Rs232SendString((uint8_t *)"Minutes\n");
                break;
            case TIME_UPDATE_SECONDS:
                uart_Rs232SendString((uint8_t *)"Seconds\n");
                break;
            default:
                break;
            }
            return;
        }

        // Dữ liệu hợp lệ
        uint8_t value = uart_ParseNumber(response_buffer);

        // Echo lại giá trị nhận được
        uart_Rs232SendString((uint8_t *)"Received: ");
        uart_Rs232SendNum(value);
        uart_Rs232SendString((uint8_t *)"\n");

        // Xóa thông báo lỗi trước đó
        lcd_ShowStr(10, 190, "                        ", BLACK, BLACK, 12, 0);

        switch (time_update_state)
        {
        case TIME_UPDATE_HOURS:
            new_time_values[0] = value;
            time_update_state = TIME_UPDATE_MINUTES;
            request_start_time = HAL_GetTick(); // FIX: Reset timer cho bước tiếp theo
            lcd_ShowStr(10, 155, "Updating minutes...", WHITE, BLACK, 14, 0);
            lcd_ShowStr(10, 170, "Enter minute (0-59):", GREEN, BLACK, 12, 0);
            uart_Rs232SendString((uint8_t *)"Minutes\n");
            break;

        case TIME_UPDATE_MINUTES:
            new_time_values[1] = value;
            time_update_state = TIME_UPDATE_SECONDS;
            request_start_time = HAL_GetTick(); // FIX: Reset timer cho bước tiếp theo
            lcd_ShowStr(10, 155, "Updating seconds...", WHITE, BLACK, 14, 0);
            lcd_ShowStr(10, 170, "Enter second (0-59):", GREEN, BLACK, 12, 0);
            uart_Rs232SendString((uint8_t *)"Seconds\n");
            break;

        case TIME_UPDATE_SECONDS:
            new_time_values[2] = value;

            // Cập nhật thời gian vào DS3231
            ds3231_Write(ADDRESS_HOUR, new_time_values[0]);
            ds3231_Write(ADDRESS_MIN, new_time_values[1]);
            ds3231_Write(ADDRESS_SEC, new_time_values[2]);

            time_update_state = TIME_UPDATE_COMPLETE;
            lcd_ShowStr(10, 155, "Time update completed!", GREEN, BLACK, 14, 0);
            lcd_ShowStr(10, 170, "                        ", BLACK, BLACK, 12, 0);
            uart_Rs232SendString((uint8_t *)"Time update completed!\n");

            // Clear all buffers after completion
            uart_ClearInputBuffer(); // This already calls uart_ResetLineBuffer()

            // Quay về chế độ bình thường sau 2 giây
            HAL_Delay(2000);
            time_update_state = TIME_UPDATE_IDLE;
            retry_count = 0;
            break;

        default:
            time_update_state = TIME_UPDATE_IDLE;
            retry_count = 0;
            break;
        }
    }
}

// ==================== COMMENTED OUT DUPLICATE FUNCTIONS ====================
// These functions were commented out to clean up the code while maintaining the
// same functionality. The active functions above provide the same features.

/*
// Enhanced Exercise 2 validation error handler - DUPLICATE FUNCTIONALITY
void uart_HandleValidationErrorEx2Detailed(uint8_t *input, TimeUpdateState_t state)
{
    uart_ClearInputBuffer();

    ValidationResult_t result = uart_ValidateInputDetailed(input, state);

    switch (result)
    {
    case VALIDATION_EMPTY:
        uart_Rs232SendString((uint8_t *)"ERROR: Empty input! Please enter a number.\n");
        break;

    case VALIDATION_NON_NUMERIC:
        uart_Rs232SendString((uint8_t *)"ERROR: Non-numeric characters detected! Only numbers (0-9) allowed.\n");
        break;

    case VALIDATION_OUT_OF_RANGE:
        uart_Rs232SendString((uint8_t *)"ERROR: Number out of range! ");
        switch (state)
        {
        case TIME_UPDATE_HOURS:
            uart_Rs232SendString((uint8_t *)"Hours must be 0-23.\n");
            break;
        case TIME_UPDATE_MINUTES:
            uart_Rs232SendString((uint8_t *)"Minutes must be 0-59.\n");
            break;
        case TIME_UPDATE_SECONDS:
            uart_Rs232SendString((uint8_t *)"Seconds must be 0-59.\n");
            break;
        default:
            uart_Rs232SendString((uint8_t *)"Invalid range.\n");
            break;
        }
        break;

    default:
        uart_Rs232SendString((uint8_t *)"ERROR: Invalid input.\n");
        break;
    }
}

// Helper function for Exercise 2 validation error (no repeat prompt) - DUPLICATE FUNCTIONALITY
void uart_HandleValidationErrorEx2(TimeUpdateState_t state)
{
    uart_ClearInputBuffer(); // Clear buffers
    uart_Rs232SendString((uint8_t *)"ERROR: Invalid data! ");

    switch (state)
    {
    case TIME_UPDATE_HOURS:
        uart_Rs232SendString((uint8_t *)"Hours must be 0-23.\n");
        break;
    case TIME_UPDATE_MINUTES:
        uart_Rs232SendString((uint8_t *)"Minutes must be 0-59.\n");
        break;
    case TIME_UPDATE_SECONDS:
        uart_Rs232SendString((uint8_t *)"Seconds must be 0-59.\n");
        break;
    default:
        uart_Rs232SendString((uint8_t *)"Invalid input.\n");
        break;
    }
}
*/
