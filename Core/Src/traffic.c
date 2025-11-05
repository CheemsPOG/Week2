#include "traffic.h"
#include "main.h"
#include <stdio.h>

#define RED_COLOR RED
#define GREEN_COLOR GREEN
#define YELLOW_COLOR YELLOW
#define OFF_COLOR GRAY
// Các biến trạng thái
static int mode = 1; // 1: NORMAL, 2: MODIFY RED, 3: MODIFY YELLOW, 4: MODIFY GREEN
static int state = 0; // 0: ĐỎ, 1: VÀNG, 2: XANH
static int counter = 0;
// Chu kỳ đèn (giây)
static int red_time = 5;
static int green_time = 3;
static int yellow_time = 2;
//------------------------------------------------------------
// Hàm vẽ đèn (hình tròn)
void draw_light(int x, int y, int color, int active) {
    uint16_t c = active ? color : OFF_COLOR;
    lcd_DrawCircle(x, y, c, 20, 1);
}
//------------------------------------------------------------
void show_mode(void) {
    lcd_Fill(0, 0, 240, 40, LIGHTGRAY);
    char buf[32];
    switch(mode) {
        case 1: sprintf(buf, "MODE: NORMAL"); break;
        case 2: sprintf(buf, "MODE: MODIFY RED"); break;
        case 3: sprintf(buf, "MODE: MODIFY YELLOW"); break;
        case 4: sprintf(buf, "MODE: MODIFY GREEN"); break;
    }
    lcd_ShowStr(10, 10, buf, BLUE, LIGHTGRAY, 16, 1);
}
void show_value(void) {
    lcd_Fill(0, 220, 240, 240, WHITE);
    char buf[32];
    sprintf(buf, "R:%02d Y:%02d G:%02d", red_time, yellow_time, green_time);
    lcd_ShowStr(30, 222, buf, BLACK, WHITE, 16, 1);
}
//------------------------------------------------------------
// Hiển thị 2 cụm đèn giao thông (2 hướng)
void show_lights(void) {
    lcd_Fill(0, 50, 240, 200, WHITE);
    int x1 = 60, x2 = 180;

    // Vẽ nền các bóng đèn
    draw_light(x1, 80, RED_COLOR, 0);
    draw_light(x1, 130, YELLOW_COLOR, 0);
    draw_light(x1, 180, GREEN_COLOR, 0);
    draw_light(x2, 80, RED_COLOR, 0);
    draw_light(x2, 130, YELLOW_COLOR, 0);
    draw_light(x2, 180, GREEN_COLOR, 0);

    // Hiển thị theo trạng thái (ngược pha)
    switch (state) {
        case 0: // Hướng 1: ĐỎ - Hướng 2: XANH
            draw_light(x1, 80, RED_COLOR, 1);
            draw_light(x2, 180, GREEN_COLOR, 1);
            break;
        case 1: // Hướng 1: XANH - Hướng 2: VÀNG
            draw_light(x1, 180, GREEN_COLOR, 1);
            draw_light(x2, 130, YELLOW_COLOR, 1);
            break;
        case 2: // Hướng 1: VÀNG - Hướng 2: ĐỎ
            draw_light(x1, 130, YELLOW_COLOR, 1);
            draw_light(x2, 80, RED_COLOR, 1);
            break;
    }
}

//------------------------------------------------------------
void traffic_init(void) {
    lcd_Clear(WHITE);
    show_mode();
    show_value();
    show_lights();
}
//------------------------------------------------------------
void traffic_run(void) {
    static int blink = 0;           // Trạng thái chớp (0=OFF,1=ON)
    static int blink_ms = 0;        // Bộ đếm thời gian cho chớp
    static int counter_ms = 0;      // Bộ đếm thời gian pha NORMAL
    static int need_update = 1;     // Cờ yêu cầu cập nhật LCD

    // ===== XỬ LÝ NÚT NHẤN =====
    if (button_count[0] == 1) { // BTN1: đổi mode
        mode++;
        if (mode > 4) mode = 1;
        show_mode();     // Cập nhật text "MODE: ..."
        need_update = 1; // Cập nhật lại giao diện đèn
    }

    // ===== CÁC CHẾ ĐỘ MODIFY =====
    if (mode >= 2 && mode <= 4) {
        blink_ms += 50;              // mỗi vòng gọi ~50 ms
        if (blink_ms >= 250) {       // 0.25 s → 2 Hz
            blink_ms = 0;
            blink = !blink;
            need_update = 1;
        }

        if (need_update) {
            need_update = 0;
            lcd_Fill(0, 50, 240, 200, WHITE);

            int x1 = 60, x2 = 180;
            draw_light(x1, 80, RED_COLOR, 0);
            draw_light(x1, 130, YELLOW_COLOR, 0);
            draw_light(x1, 180, GREEN_COLOR, 0);
            draw_light(x2, 80, RED_COLOR, 0);
            draw_light(x2, 130, YELLOW_COLOR, 0);
            draw_light(x2, 180, GREEN_COLOR, 0);

            // Chớp đèn tương ứng với mode
            if (mode == 2) { // MODIFY RED
                draw_light(x1, 80, RED_COLOR, blink);
                draw_light(x2, 80, RED_COLOR, blink);
            } else if (mode == 3) { // MODIFY YELLOW
                draw_light(x1, 130, YELLOW_COLOR, blink);
                draw_light(x2, 130, YELLOW_COLOR, blink);
            } else if (mode == 4) { // MODIFY GREEN
                draw_light(x1, 180, GREEN_COLOR, blink);
                draw_light(x2, 180, GREEN_COLOR, blink);
            }
        }

        // BTN2 → tăng thời gian
        if (button_count[1] == 1) {
            if (mode == 2) red_time = (red_time % 99) + 1;
            if (mode == 3) yellow_time = (yellow_time % 99) + 1;
            if (mode == 4) green_time = (green_time % 99) + 1;
            show_value(); // Hiển thị lại dòng R:Y:G
        }

        // BTN3 → xác nhận
        if (button_count[2] == 1) {
            lcd_ShowStr(160, 10, "SAVED", GREEN, LIGHTGRAY, 16, 1);
        }
        return; // Không chạy NORMAL khi đang MODIFY
    }

    // ===== CHẾ ĐỘ NORMAL =====
    blink_ms = 0;  // Reset chớp
    blink = 0;

    counter_ms += 50;  // Mỗi vòng = 50 ms thực

    int phase_time = 0;
    switch (state) {
        case 0: phase_time = red_time * 1000; break;
        case 1: phase_time = yellow_time * 1000; break;
        case 2: phase_time = green_time * 1000; break;
    }

    if (counter_ms >= phase_time) {
        counter_ms = 0;
        state++;
        if (state > 2) state = 0;
        need_update = 1;
    }

    if (need_update) {
        need_update = 0;
        show_lights();  // Hiển thị 2 cụm đèn tròn
    }

    // Hiển thị thông tin phía dưới (R:Y:G)
    show_value();
}

