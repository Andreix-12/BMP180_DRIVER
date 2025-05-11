
# Driver BMP180 cho Linux (Raspberry Pi)

## Giới thiệu

Dự án này bao gồm:
- **File `bmp180.c`**: driver kernel space cho cảm biến BMP180 (nhiệt độ và áp suất).
- **File `Test.c`**: chương trình không gian người dùng (user-space) để đọc nhiệt độ, áp suất từ BMP180 thông qua thiết bị `/dev/bmp180`.
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
   ./Test
   ```

   Kết quả ví dụ:
   ```
   Nhiệt độ: 25.3 °C
   Áp suất: 55 Pa
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
## 🌳 Tạo Device Tree Overlay cho BMP180 (Tuỳ chọn nâng cao)

Để tương tác với driver BMP180 bằng Device Tree Overlay, bạn có thể tạo một overlay đơn giản để ánh xạ thiết bị vào hệ thống I2C.

### 📁 Bước 1: Tạo file overlay (.dts)

Tạo một file tên `bmp180-overlay.dts`:

```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "brcm,bcm2835";

    fragment@0 {
        target = <&i2c1>;
        __overlay__ {
            bmp180@77 {
                compatible = "bmp180";
                reg = <0x77>;
            };
        };
    };
};
```

> Lưu ý: `0x77` là địa chỉ I2C mặc định của BMP180.

### ⚙️ Bước 2: Biên dịch Overlay

Dùng `dtc` (Device Tree Compiler) để biên dịch:

```bash
dtc -@ -I dts -O dtb -o bmp180.dtbo bmp180-overlay.dts
```

### 📦 Bước 3: Cài Overlay

Chép file `.dtbo` vào thư mục overlays của boot:

```bash
sudo cp bmp180.dtbo /boot/overlays/
```

Chỉnh sửa file cấu hình `/boot/config.txt` để nạp overlay lúc khởi động:

```bash
sudo nano /boot/config.txt
```

Thêm dòng sau vào cuối file:

```
dtoverlay=bmp180
```

Khởi động lại hệ thống:

```bash
sudo reboot
```

Sau khi khởi động, bạn có thể kiểm tra overlay đã nạp chưa bằng:

```bash
dmesg | grep bmp180
```

> Nếu bạn sử dụng Device Tree, driver kernel có thể tự động tạo `/dev/bmp180` nếu cấu hình đúng.
