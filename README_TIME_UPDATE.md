# Đồng Hồ Điện Tử với Cập Nhật Thời Gian qua UART

## Mô tả

Đây là bài tập nâng cấp từ lab 4 với chức năng cập nhật thời gian qua giao tiếp RS232.

## Tính năng

1. **Hiển thị thời gian thực**: Hiển thị đồng hồ điện tử trên màn hình LCD
2. **Bài 1 - Gửi thời gian qua UART**: Nhấn nút 12 để gửi thời gian hiện tại qua UART
3. **Bài 1 - Cập nhật thời gian qua UART**: Nhấn nút 13 để cập nhật giờ, phút, giây
4. **Bài 2 - Chức năng mở rộng**: Nhấn nút 14 để kích hoạt bài 2 (chưa implement)

## Cách sử dụng

### Chế độ hiển thị bình thường

- LCD hiển thị giao diện đơn giản:
  - Tiêu đề: "DIGITAL CLOCK" (màu trắng, căn giữa)
  - Thời gian: HH:MM:SS (màu vàng, font lớn, căn giữa)

**Lưu ý**: Timer 10s sẽ reset mỗi khi gửi request mới (kể cả khi validation fail)

### 📊 Timeout Flow Chart (Bài 2):

```
START → Send "Hours" → Timer = 0
   ↓
Input received?
   ├─ YES → Valid data?
   │    ├─ YES → Timer = 0 → Send "Minutes"
   │    └─ NO  → Timer = 0 → Send "Hours" (retry)
   │
   └─ NO → Timer >= 10s?
        ├─ NO  → Continue waiting
        └─ YES → Retry count++
             ├─ < 3 → Timer = 0 → Send "Hours"
             └─ ≥ 3 → ERROR → Exit to normal mode
```

**Các trigger reset timer:**

1. 🟢 Gửi request mới ("Hours", "Minutes", "Seconds")
2. 🟡 Validation fail → gửi lại request
3. 🔵 Timeout → retry → gửi lại request
4. ✅ Valid input → chuyển bước tiếp theo

### Bài 1 - Gửi thời gian qua UART (Nút 12)

- Nhấn nút 12 để gửi thời gian hiện tại qua UART
- Format: "HH:MM:SS"

### Bài 1 - Cập nhật thời gian với Timeout cơ bản (Nút 13)

**✅ Đã cập nhật: Thêm timeout cho mỗi bước!**

1. Nhấn nút 13 để bắt đầu chế độ cập nhật (chỉ giờ, phút, giây)
2. LCD sẽ hiển thị "TIME UPDATE MODE"
3. Hệ thống sẽ lần lượt yêu cầu với **timeout 10s mỗi bước**:

   - "Hours" - Nhập giờ (0-23) → **10s timeout**
   - "Minutes" - Nhập phút (0-59) → **10s timeout**
   - "Seconds" - Nhập giây (0-59) → **10s timeout**

4. **Xử lý timeout mới:**

   - Nếu không nhập trong 10s → "Timeout! Please enter input (X/3)"
   - Sau 3 lần timeout → "Timeout exceeded! Returning to normal mode..."
   - Tự động quay về đồng hồ bình thường

5. Sau khi nhập xong giây, hệ thống sẽ:
   - Cập nhật giờ, phút, giây vào DS3231 (giữ nguyên ngày tháng năm)
   - Hiển thị "Time update completed!"
   - Tự động quay về chế độ hiển thị bình thường sau 2 giây

## Sử dụng với phần mềm Hercules

1. Mở Hercules Terminal
2. Kết nối với cổng COM tương ứng
3. Cấu hình: 115200 baud, 8N1
4. Khi hệ thống gửi request (ví dụ: "Hours"), nhập giá trị và nhấn Enter
5. Tiếp tục cho đến khi hoàn thành

## Ví dụ cập nhật thời gian

```
Hệ thống gửi: "Hours"
Bạn nhập: "14" + Enter
Hệ thống phản hồi: "Received: 14"

Hệ thống gửi: "Minutes"
Bạn nhập: "30" + Enter
Hệ thống phản hồi: "Received: 30"

Hệ thống gửi: "Seconds"
Bạn nhập: "45" + Enter
Hệ thống phản hồi: "Received: 45"

Hệ thống gửi: "Time update completed!"
```

**Lưu ý:** Chỉ cập nhật giờ, phút, giây. Ngày tháng năm được giữ nguyên.

## So sánh Bài 1 vs Bài 2

| Tính năng                | Bài 1 (Button 13)                   | Bài 2 (Button 14)                  |
| ------------------------ | ----------------------------------- | ---------------------------------- |
| **Giao diện**            | Đơn giản + timeout message          | Hiển thị timeout/retry info        |
| **Timeout**              | ✅ 10 giây (mới cập nhật)           | ✅ 10 giây                         |
| **Retry**                | ✅ Tối đa 3 lần → quit (mới)        | ✅ Tối đa 3 lần                    |
| **Validation**           | ✅ Nghiêm ngặt (mới cập nhật)       | ✅ Nghiêm ngặt (chỉ số hợp lệ)     |
| **Xử lý lỗi**            | ✅ Báo lỗi qua UART (mới)           | ✅ Báo lỗi chi tiết trên LCD       |
| **Response khi invalid** | ✅ Reset timer + yêu cầu nhập lại   | ✅ Yêu cầu nhập lại                |
| **Response khi timeout** | ✅ "Please enter input (X/3)" (mới) | ✅ Auto retry và báo lỗi sau 3 lần |
| **Timeout message**      | Simple UART only                    | Detailed LCD + UART messages       |
| **Phù hợp cho**          | Demo với timeout cơ bản             | Ứng dụng thực tế với UI đầy đủ     |

## Bài 2 - Cập nhật thời gian với Timeout và Retry (Button 14)

**Trạng thái: ✅ Đã implement**

Button 14 kích hoạt **Bài 2** - phiên bản nâng cao của chức năng cập nhật thời gian với các tính năng:

### Tính năng Bài 2:

1. **Timeout 10 giây**: Nếu sau 10s không có phản hồi từ máy tính, hệ thống sẽ gửi lại request
2. **Retry tối đa 3 lần**: Sau 3 lần timeout, hệ thống báo lỗi và quay về chế độ bình thường
3. **Validation dữ liệu**: Kiểm tra dữ liệu đầu vào hợp lệ, nếu không hợp lệ sẽ yêu cầu nhập lại
4. **Hiển thị trạng thái**: LCD hiển thị thông tin timeout, retry count, và lỗi validation

### Cách sử dụng Bài 2:

1. **Nhấn Button 14** để bắt đầu chế độ cập nhật nâng cao
2. LCD hiển thị "** EXERCISE 2 **" với thông tin timeout và retry
3. Quy trình tương tự Bài 1 nhưng có thêm:
   - Đếm ngược timeout 10s
   - Hiển thị số lần retry
   - Validation nghiêm ngặt input
   - Báo lỗi chi tiết

### Các tình huống xử lý:

#### Timeout:

```
Hệ thống: "Hours"
(Không có phản hồi trong 10s)
LCD: "Timeout! Retry 1/3"
Hệ thống: "Hours" (gửi lại)
```

#### Dữ liệu không hợp lệ:

```
Hệ thống: "Hours"
Người dùng: "abc" + Enter
LCD: "Invalid data! Retry..."
Hệ thống: "ERROR: Invalid data! Hours must be 0-23."
Hệ thống: "Hours" (yêu cầu nhập lại - timer reset)

Người dùng: "25" + Enter
Hệ thống: "ERROR: Invalid data! Hours must be 0-23."
Hệ thống: "Hours" (yêu cầu nhập lại - timer reset)

Người dùng: "14" + Enter
Hệ thống: "Received: 14"
Hệ thống: "Minutes" (chuyển bước tiếp theo - timer reset)
```

#### Quá số lần retry:

```
(Sau 3 lần timeout)
LCD: "ERROR: No response!"
LCD: "Returning to normal..."
Hệ thống: "ERROR: Timeout after 3 retries!"
(Quay về chế độ bình thường sau 3s)
```

### Button Control Summary

| Button | Bài tập   | Chức năng                                |
| ------ | --------- | ---------------------------------------- |
| 12     | Bài 1     | Gửi thời gian qua UART                   |
| 13     | Bài 1     | Cập nhật thời gian (cơ bản)              |
| **14** | **Bài 2** | **Cập nhật thời gian (timeout + retry)** |

## Khắc phục sự cố

### Vấn đề: Chậm chuyển sang mục tiếp theo khi cập nhật thời gian

**Nguyên nhân:**

- Logic xử lý UART chỉ hoạt động khi có dữ liệu mới
- Ring buffer có thể bị đầy hoặc xử lý không kịp thời

**Giải pháp đã implement:**

1. **Tối ưu hóa logic xử lý UART**: Xử lý liên tục trong chế độ cập nhật thời gian
2. **Cải thiện hàm đọc dòng**: Sử dụng static buffer để lưu trạng thái
3. **Thêm feedback**: Echo lại giá trị nhận được để xác nhận
4. **Hiển thị hướng dẫn**: Hiển thị rõ ràng khoảng giá trị cần nhập

**Cách sử dụng đúng:**

- Nhập số và nhấn **Enter** ngay lập tức
- Chờ thông báo "Received: [số]" để xác nhận
- Hệ thống sẽ tự động chuyển sang bước tiếp theo

### Vấn đề: Nhập giá trị 12 nhưng nhận được 255

**Nguyên nhân có thể:**

- Lỗi parsing trong hàm uart_ParseNumber()
- Buffer không được kết thúc đúng cách (null terminator)
- Nhập ký tự không hợp lệ hoặc có ký tự đặc biệt

**Giải pháp đã implement:**

1. **Thêm debug output**: Hiển thị raw input để kiểm tra dữ liệu nhận được
2. **Cải thiện filter input**: Chỉ chấp nhận ký tự số (0-9) và Enter
3. **Validation khoảng giá trị**: Kiểm tra Hours (0-23), Minutes/Seconds (0-59)
4. **Null terminator**: Đảm bảo buffer được kết thúc đúng

**Debug output sẽ hiển thị:**

```
Raw input: [12]
Parsed value: 12
Valid hours: 12
```

Nếu vẫn thấy giá trị 255, hãy kiểm tra:

- Có nhập đúng chỉ số không?
- Có ký tự đặc biệt nào trong input không?
- Terminal setting có đúng (115200, 8N1) không?

### Vấn đề: Input bị concat với dữ liệu cũ

**Triệu chứng:**

```
Input: "12"
Output: "Raw input: [12abc45]" (có ký tự lạ từ lần trước)
```

**Nguyên nhân:** Buffer không được clear khi bắt đầu mới

**Giải pháp đã implement:**

- ✅ Clear ring buffer và line buffer khi bắt đầu update
- ✅ Reset buffer ngay khi có ký tự không hợp lệ
- ✅ Memset buffer về 0 sau mỗi lần sử dụng
- ✅ Overflow protection cho line buffer

**Code đã sửa:**

- `uart_ClearInputBuffer()`: Clear tất cả buffer khi start
- `uart_ResetLineBuffer()`: Reset static line buffer
- Improved `uart_ReadLine()`: Better garbage character handling

### Vấn đề: Timeout không hoạt động

**Triệu chứng:** Nhấn Button 14, chờ 10s nhưng không thấy thông báo timeout

**Debug steps:**

1. Nhấn Button 14 → Kiểm tra UART output có thông báo:

   ```
   DEBUG: Started Exercise 2 (timeout mode)
   DEBUG: Processing in advanced mode (Ex2)
   DEBUG: Entered uart_ProcessTimeUpdateEx()
   ```

2. Trên LCD có hiển thị đồng hồ đếm:
   ```
   Waiting... 1s
   Waiting... 2s
   ...
   Waiting... 10s
   Timeout! Retry 1/3
   ```

**Nếu không thấy debug messages:** Main loop không gọi `uart_ProcessTimeUpdate()`
**Nếu thấy debug nhưng không timeout:** Logic timeout có vấn đề

## Lưu ý

- **✅ CẢ HAI BÀI ĐỀU CÓ VALIDATION**: Chỉ chấp nhận số nguyên trong khoảng hợp lệ
- **✅ XỬ LÝ LỖI THÔNG MINH**: Khi nhập sai sẽ báo lỗi và yêu cầu nhập lại
- **✅ TIMER RESET ĐÚNG**: Mỗi khi gửi request mới, timer sẽ reset về 0
- Thời gian được lưu vào IC DS3231 để giữ chính xác khi mất điện
- Màn hình LCD sẽ cập nhật mỗi giây
- **Nhập số và nhấn Enter ngay, không chờ đợi**

### 🔧 Cập nhật mới:

- **✅ FIX INPUT BUFFER**: Xóa dữ liệu cũ khi bắt đầu, tránh concat chuỗi lạ
- **✅ TIMEOUT CHO BÀI 1**: Thêm timeout 10s + LCD error display
- **✅ LCD ERROR MESSAGES**: Hiển thị chi tiết lỗi timeout trên LCD
- **Bài 1 (Button 13)**: Timeout + validation + LCD error display
- **Bài 2 (Button 14)**: Timer 10s reset đúng cách, validation chi tiết
- **Cả hai bài**: Thông báo lỗi rõ ràng khi nhập sai format hoặc khoảng giá trị

### 🕒 Cách hoạt động của Timeout (Bài 2):

**Timer bắt đầu đếm khi nào?**

- Mỗi khi hệ thống gửi request ("Hours", "Minutes", "Seconds")
- Timer reset về 0 khi: gửi request mới, validation fail, chuyển bước

**Ví dụ chi tiết:**

```
00:00s - Hệ thống: "Hours" (timer bắt đầu đếm từ 0)
00:05s - Người dùng: "abc" → ERROR → "Hours" (timer reset về 0)
00:08s - Người dùng: "14" → OK → "Minutes" (timer reset về 0)
00:15s - (Không response) → TIMEOUT → "Minutes" (timer reset về 0)
00:23s - (Không response) → TIMEOUT → "Minutes" (timer reset về 0)
00:31s - (Không response) → ERROR: 3 retries → Quit
```

### 📺 LCD Error Display cho Bài 1:

**Khi timeout (mỗi lần):**

```
LCD Line 1: "TIMEOUT 1/3!"     (màu đỏ)
LCD Line 2: "Please enter input..."  (màu vàng)
```

**Khi 3 lần timeout (final error):**

```
LCD Line 1: "ERROR: 3x TIMEOUT!"     (màu đỏ, size 16)
LCD Line 2: "No input received"      (màu trắng)
LCD Line 3: "Returning to clock..."  (màu vàng)
LCD Line 4: "Failed at: Hours step"  (màu cyan, tùy step)
```

**Các step có thể fail:**

- "Failed at: Hours step"
- "Failed at: Minutes step"
- "Failed at: Seconds step"
