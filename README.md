
# Driver Kernel cho cảm biến BMP180

Đây là một module kernel đơn giản để giao tiếp với cảm biến BMP180 thông qua giao tiếp I2C trên Raspberry Pi, cung cấp thiết bị `/dev/bmp180` cho ứng dụng người dùng để đọc nhiệt độ và áp suất.

---

## 📂 Các tập tin chính

- `BMP180.c`: Mã nguồn driver nhân Linux cho BMP180.
- `Test.c`: Chương trình mẫu không gian người dùng để đọc nhiệt độ.
- `makefile.txt`: Makefile dùng để build module kernel.

---

## 📋 Yêu cầu hệ thống

- Raspberry Pi với hệ điều hành Linux.
- Đã bật I2C: `sudo raspi-config` → Interface Options → I2C → Enable.
- Đã cài đặt headers của kernel:
  
  ```bash
  sudo apt update
  sudo apt install raspberrypi-kernel-headers build-essential
  ```

⚙️ **Build driver**

Đổi tên makefile.txt thành Makefile (nếu chưa đổi):

```bash
mv makefile.txt Makefile
```

Build module:

```bash
make
```

📦 **Load module và tạo thiết bị**

Nạp module vào kernel:

```bash
sudo insmod bmp18.ko
```

Kiểm tra xem thiết bị /dev/bmp180 có được tạo chưa:

```bash
ls /dev/bmp180
```

Nếu chưa thấy, bạn cần tạo thủ công:

```bash
dmesg | grep bmp180
# Tìm major number (ví dụ: 240)

sudo mknod /dev/bmp180 c 240 0
sudo chmod 666 /dev/bmp180
```

🧪 **Chạy chương trình kiểm tra**

Biên dịch chương trình Test:

```bash
gcc Test.c -o test_bmp180
```

Chạy:

```bash
./test_bmp180
```

Output ví dụ:

```bash
Temp = 25.3 C
```

🧹 **Gỡ module và dọn dẹp**

```bash
sudo rmmod bmp18
make clean
```

⚠️ **Ghi chú**

Cảm biến BMP180 thường có địa chỉ I2C là 0x77.

Dùng i2cdetect -y 1 để kiểm tra địa chỉ I2C và kết nối phần cứng.

Code này hiện chỉ đọc được nhiệt độ. Muốn đọc thêm áp suất, cần mở rộng Test.c với ioctl mã bmp180_IOCTL_READ_PRESS.

📄 **Giấy phép**

MIT / GPL v2 — Tự do sử dụng cho mục đích học tập, nghiên cứu hoặc tích hợp vào hệ thống thực tế.
