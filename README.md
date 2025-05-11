
# Driver BMP180 cho Linux (Raspberry Pi)

## Giới thiệu

Dự án này bao gồm:
- **File `BMP180.c`**: driver kernel space cho cảm biến BMP180 (nhiệt độ và áp suất).
- **File `Test.c`**: chương trình không gian người dùng (user-space) để đọc nhiệt độ từ BMP180 thông qua thiết bị `/dev/bmp180`.
- **Makefile**: để biên dịch cả module kernel và chương trình test.

## Yêu cầu hệ thống

- Raspberry Pi hoặc thiết bị Linux có hỗ trợ I2C.
- Đã bật I2C trong `raspi-config`.
- Đã cài đặt các gói:
  ```bash
  sudo apt install raspberrypi-kernel-headers build-essential
  ```

## Cài đặt

1. **Biên dịch module kernel BMP180:**

   ```bash
   make
   ```

2. **Nạp module vào kernel:**

   ```bash
   sudo insmod bmp180.ko
   ```

3. **Tạo node thiết bị nếu chưa có:**

   ```bash
   sudo mknod /dev/bmp180 c 240 0
   sudo chmod 666 /dev/bmp180
   ```

   > *Lưu ý: 240 là số major number tạm thời, bạn có thể kiểm tra lại với `dmesg` nếu cần.*

4. **Chạy chương trình kiểm tra:**

   ```bash
   ./test
   ```

   Kết quả ví dụ:
   ```
   Temp = 25.3 C
   ```

5. **Gỡ module khi không cần nữa:**

   ```bash
   sudo rmmod bmp180
   ```

## Giấy phép (License)

Phần mềm này được phân phối theo **Giấy phép MIT**:

```
MIT License

Copyright (c) 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
IN THE SOFTWARE.
```
