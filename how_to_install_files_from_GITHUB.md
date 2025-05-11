
📘 Hướng Dẫn Cài Đặt Các File Từ Thư Mục `Members` Trên Raspberry Pi

---

## 1. 🛠 Cài Đặt Git (Nếu Chưa Có)

Git giúp bạn tải mã nguồn từ GitHub về Raspberry Pi.

```bash
sudo apt-get update
sudo apt-get install git
```

---

## 2. 🌐 Clone Repository BMP180_DRIVER

Clone mã nguồn từ GitHub:

```bash
git clone https://github.com/Andreix-12/BMP180_DRIVER.git
```

Sau khi hoàn tất, thư mục `BMP180_DRIVER` sẽ xuất hiện.

---

## 3. 📁 Di Chuyển Vào Thư Mục `Members`

```bash
cd BMP180_DRIVER/Members
```

Kiểm tra các file có trong thư mục:

```bash
ls
```

---

## 4. ⚙️ Cài Đặt Các File Trong Thư Mục `Members`

### 🔹 4.1 Nếu Có Makefile

Nếu có file `Makefile`, bạn có thể biên dịch dễ dàng:

```bash
make
```

Cài đặt (nếu Makefile hỗ trợ):

```bash
sudo make install
```

### 🔹 4.2 Nếu Không Có Makefile

Biên dịch thủ công các file `.c`:

Ví dụ:

```bash
gcc -o my_program file1.c file2.c -li2c -lm
```

Sau đó chạy chương trình:

```bash
sudo ./my_program
```

> 💡 *Nếu chương trình yêu cầu giao tiếp I2C, đảm bảo bạn đã bật I2C trên Raspberry Pi bằng `sudo raspi-config`.*

---

## 5. 📦 Cài Đặt Module Kernel (Nếu Có)

Nếu thư viện hoặc file yêu cầu nạp module kernel:

```bash
sudo modprobe <module_name>
```

Kiểm tra log hệ thống:

```bash
dmesg | grep bmp180
```

---

## 6. ⚙️ Cấu Hình Kernel (Nếu Cần)

Một số trường hợp yêu cầu cấu hình kernel hoặc bật các tính năng liên quan I2C. Để cấu hình:

```bash
sudo raspi-config
# Chọn Interface Options → I2C → Enable
```

Hoặc biên dịch lại kernel (nâng cao, không thường cần thiết với BMP180).

---

## 7. 🧹 Một Số Lệnh Hữu Ích Khác

### 🔄 Tải Lại Module Kernel

```bash
sudo rmmod <module_name>
sudo modprobe <module_name>
```

### 🗑 Gỡ Cài Đặt

```bash
sudo rmmod <module_name>
make clean
```

---

## ✅ 8. Tóm Tắt Quy Trình

1. Cài Git
2. Clone repository `BMP180_DRIVER`
3. Vào thư mục `Members`
4. Biên dịch bằng `make` hoặc `gcc`
5. Load module kernel (nếu có)
6. Bật I2C và kiểm tra kết nối thiết bị
7. Cấu hình lại kernel nếu thực sự cần thiết
