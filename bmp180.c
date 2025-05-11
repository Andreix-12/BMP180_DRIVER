#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h> // For copy_to_user
#include <linux/delay.h>   // For msleep
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>   // For mutex

#define DRIVER_NAME "bmp180_driver"
#define DEVICE_NAME "bmp180"
#define CLASS_NAME  "bmp180_class"

// BMP180 Registers and Commands
#define BMP180_REG_CALIB_START   0xAA   // EEPROM calibration data start register
#define BMP180_CALIB_DATA_LENGTH 22     // Length of calibration data
#define BMP180_REG_CHIP_ID       0xD0   // Chip ID register
#define BMP180_CHIP_ID_VAL       0x55   // Expected chip ID
#define BMP180_REG_CONTROL       0xF4   // Control register
#define BMP180_REG_RESULT        0xF6   // MSB of result
#define BMP180_CMD_TEMP          0x2E   // Command to start temperature measurement
#define BMP180_CMD_PRESSURE_0    0x34   // Command to start pressure measurement (OSS=0)

// IOCTL definitions
#define BMP180_IOCTL_MAGIC     'b'
#define BMP180_IOCTL_READ_TEMP _IOR(BMP180_IOCTL_MAGIC, 1, int)
#define BMP180_IOCTL_READ_PRESS _IOR(BMP180_IOCTL_MAGIC, 2, int)

// Oversampling Setting (OSS)
#define OSS 0

static struct i2c_client *bmp180_client_g;
static struct class *bmp180_class_g;
static struct device *bmp180_device_g;
static int major_number_g;

// Calibration coefficients
static short ac1, ac2, ac3, b1, b2, mb, mc, md;
static unsigned short ac4, ac5, ac6;

// Mutex for protecting sensor access
static DEFINE_MUTEX(bmp180_mutex);

static int bmp180_read_calib_data(struct i2c_client *client)
{
    u8 buf[BMP180_CALIB_DATA_LENGTH];
    int ret;

    ret = i2c_smbus_read_i2c_block_data(client, BMP180_REG_CALIB_START,
                                        BMP180_CALIB_DATA_LENGTH, buf);
    if (ret < 0) {
        pr_err("bmp180: Failed to read calibration block data (err %d)\n", ret);
        return ret;
    }
    // In i2c_smbus_read_i2c_block_data, the return value is the number of bytes read on success.
    if (ret != BMP180_CALIB_DATA_LENGTH) {
        pr_err("bmp180: Read wrong length for calibration data (%d instead of %d)\n",
               ret, BMP180_CALIB_DATA_LENGTH);
        return -EIO;
    }

    ac1 = (buf[0]  << 8) | buf[1];
    ac2 = (buf[2]  << 8) | buf[3];
    ac3 = (buf[4]  << 8) | buf[5];
    ac4 = (buf[6]  << 8) | buf[7];
    ac5 = (buf[8]  << 8) | buf[9];
    ac6 = (buf[10] << 8) | buf[11];
    b1  = (buf[12] << 8) | buf[13];
    b2  = (buf[14] << 8) | buf[15];
    mb  = (buf[16] << 8) | buf[17];
    mc  = (buf[18] << 8) | buf[19];
    md  = (buf[20] << 8) | buf[21];

    pr_info("bmp180: Calibration data: ac1=%d, ac2=%d, ac3=%d, ac4=%u, ac5=%u, ac6=%u, b1=%d, b2=%d, mb=%d, mc=%d, md=%d\n",
            ac1, ac2, ac3, ac4, ac5, ac6, b1, b2, mb, mc, md);
    return 0;
}

static int bmp180_read_raw_temp_unsafe(struct i2c_client *client)
{
    int ret;
    int msb, lsb;

    ret = i2c_smbus_write_byte_data(client, BMP180_REG_CONTROL, BMP180_CMD_TEMP);
    if (ret < 0) {
        pr_err("bmp180: Failed to write temp command (err %d)\n", ret);
        return ret;
    }
    msleep(5);

    msb = i2c_smbus_read_byte_data(client, BMP180_REG_RESULT);
    if (msb < 0) {
        pr_err("bmp180: Failed to read temp MSB (err %d)\n", msb);
        return msb;
    }
    lsb = i2c_smbus_read_byte_data(client, BMP180_REG_RESULT + 1);
    if (lsb < 0) {
        pr_err("bmp180: Failed to read temp LSB (err %d)\n", lsb);
        return lsb;
    }
    return (msb << 8) | lsb;
}

static int bmp180_read_raw_press_unsafe(struct i2c_client *client)
{
    int ret;
    int msb, lsb, xlsb;
    u8 cmd = BMP180_CMD_PRESSURE_0 + (OSS << 6);

    ret = i2c_smbus_write_byte_data(client, BMP180_REG_CONTROL, cmd);
    if (ret < 0) {
        pr_err("bmp180: Failed to write pressure command (err %d)\n", ret);
        return ret;
    }

    switch (OSS) {
    case 0: msleep(5); break;
    case 1: msleep(8); break;
    case 2: msleep(14); break;
    case 3: msleep(26); break;
    default: msleep(5);
    }

    msb = i2c_smbus_read_byte_data(client, BMP180_REG_RESULT);
    if (msb < 0) return msb;
    lsb = i2c_smbus_read_byte_data(client, BMP180_REG_RESULT + 1);
    if (lsb < 0) return lsb;
    xlsb = i2c_smbus_read_byte_data(client, BMP180_REG_RESULT + 2);
    if (xlsb < 0) return xlsb;

    return ((msb << 16) | (lsb << 8) | xlsb) >> (8 - OSS);
}

static int bmp180_get_b5_unsafe(struct i2c_client *client, int *b5_out)
{
    int ut, x1, x2;
    ut = bmp180_read_raw_temp_unsafe(client);
    if (ut < 0) {
        return ut;
    }

    x1 = ((ut - (int)ac6) * (int)ac5) >> 15;
    if ((x1 + md) == 0) {
        pr_err("bmp180: Division by zero in B5 calculation (x1+md is 0)\n");
        return -EINVAL;
    }
    x2 = ((int)mc << 11) / (x1 + md);
    *b5_out = x1 + x2;
    return 0;
}

static int bmp180_read_temp(struct i2c_client *client, int *temp_out)
{
    int b5, temp, ret;

    mutex_lock(&bmp180_mutex);
    ret = bmp180_get_b5_unsafe(client, &b5);
    mutex_unlock(&bmp180_mutex);

    if (ret < 0) {
        return ret;
    }
    temp = (b5 + 8) >> 4;
    *temp_out = temp;
    return 0;
}

static int bmp180_read_press(struct i2c_client *client, int *press_out)
{
    int up, b5, ret;
    long p;
    long b6, x1, x2, x3, b3;
    unsigned long b4, b7;

    mutex_lock(&bmp180_mutex);
    ret = bmp180_get_b5_unsafe(client, &b5);
    if (ret < 0) {
        mutex_unlock(&bmp180_mutex);
        return ret;
    }
    up = bmp180_read_raw_press_unsafe(client);
    mutex_unlock(&bmp180_mutex);

    if (up < 0) {
        return up;
    }

    b6 = b5 - 4000;
    x1 = (b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = (ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = ((((long)ac1 * 4 + x3) << OSS) + 2) >> 2;

    x1 = (ac3 * b6) >> 13;
    x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = (ac4 * (unsigned long)(x3 + 32768)) >> 15;
    if (b4 == 0) {
         pr_err("bmp180: Division by zero in pressure calculation (b4 is 0)\n");
         return -EINVAL;
    }

    b7 = ((unsigned long)up - b3) * (50000 >> OSS);

    if (b7 < 0x80000000) {
        p = (b7 * 2) / b4;
    } else {
        p = (b7 / b4) * 2;
    }

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);

    *press_out = p;
    return 0;
}

static long bmp180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int value = 0;
    int ret;

    switch (cmd) {
    case BMP180_IOCTL_READ_TEMP:
        ret = bmp180_read_temp(bmp180_client_g, &value);
        if (ret < 0)
            return ret;
        break;
    case BMP180_IOCTL_READ_PRESS:
        ret = bmp180_read_press(bmp180_client_g, &value);
        if (ret < 0)
            return ret;
        break;
    default:
        pr_warn("bmp180: Invalid IOCTL cmd: %u\n", cmd);
        return -EINVAL;
    }

    if (copy_to_user((int __user *)arg, &value, sizeof(value))) {
        pr_err("bmp180: Failed to copy data to user space\n");
        return -EFAULT;
    }
    return 0;
}

static int bmp180_open(struct inode *inode, struct file *file)
{
    pr_info("bmp180: Device opened\n");
    return 0;
}

static int bmp180_release(struct inode *inode, struct file *file)
{
    pr_info("bmp180: Device closed\n");
    return 0;
}

static const struct file_operations bmp180_fops = {
    .owner = THIS_MODULE,
    .open = bmp180_open,
    .unlocked_ioctl = bmp180_ioctl,
    .release = bmp180_release,
};

// CHANGED: Probe function signature reverted to single argument
static int bmp180_probe(struct i2c_client *client)
{
    int ret;
    int chip_id_val;

    pr_info("bmp180: Probing for device at I2C address 0x%02x\n", client->addr);

    // 1. Check I2C functionalities
    // CHANGED: I2C_FUNC_SMBUS_READ_I2C_BLOCK_DATA to I2C_FUNC_SMBUS_READ_BLOCK_DATA
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_BYTE_DATA |
                                                  I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
                                                  I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
        pr_err("bmp180: I2C adapter does not support necessary SMBus operations\n");
        return -EIO;
    }

    bmp180_client_g = client; // Store client globally

    // 2. Optional: Check Chip ID
    chip_id_val = i2c_smbus_read_byte_data(client, BMP180_REG_CHIP_ID);
    if (chip_id_val < 0) {
        pr_err("bmp180: Failed to read chip ID (err %d)\n", chip_id_val);
        return chip_id_val;
    }
    if (chip_id_val != BMP180_CHIP_ID_VAL) {
        pr_err("bmp180: Invalid chip ID 0x%02x (expected 0x%02x)\n", chip_id_val, BMP180_CHIP_ID_VAL);
        return -ENODEV;
    }
    pr_info("bmp180: Chip ID 0x%02x verified\n", chip_id_val);

    // 3. Read calibration data
    ret = bmp180_read_calib_data(client);
    if (ret < 0) {
        return ret;
    }

    // 4. Register character device
    major_number_g = register_chrdev(0, DEVICE_NAME, &bmp180_fops);
    if (major_number_g < 0) {
        pr_err("bmp180: Failed to register a major number\n");
        // bmp180_client_g = NULL; // Not strictly necessary on failure here
        return major_number_g;
    }
    pr_info("bmp180: Registered correctly with major number %d\n", major_number_g);

    // 5. Create device class
    bmp180_class_g = class_create(CLASS_NAME);
    if (IS_ERR(bmp180_class_g)) {
        unregister_chrdev(major_number_g, DEVICE_NAME);
        // bmp180_client_g = NULL;
        pr_err("bmp180: Failed to register device class\n");
        return PTR_ERR(bmp180_class_g);
    }
    pr_info("bmp180: Device class registered correctly\n");

    // 6. Create device node
    bmp180_device_g = device_create(bmp180_class_g, NULL, MKDEV(major_number_g, 0), NULL, DEVICE_NAME);
    if (IS_ERR(bmp180_device_g)) {
        class_destroy(bmp180_class_g);
        unregister_chrdev(major_number_g, DEVICE_NAME);
        // bmp180_client_g = NULL;
        pr_err("bmp180: Failed to create the device\n");
        return PTR_ERR(bmp180_device_g);
    }
    pr_info("bmp180: Device created successfully (/dev/%s)\n", DEVICE_NAME);
    pr_info("bmp180: Driver installed.\n");

    return 0;
}

static void bmp180_remove(struct i2c_client *client)
{
    device_destroy(bmp180_class_g, MKDEV(major_number_g, 0));
    class_destroy(bmp180_class_g);
    unregister_chrdev(major_number_g, DEVICE_NAME);
    // bmp180_client_g = NULL; // Clear the global client pointer
    pr_info("bmp180: Driver removed.\n");
}

static const struct i2c_device_id bmp180_id[] = {
    { "bmp180", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bmp180_id);

static struct i2c_driver bmp180_i2c_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe = bmp180_probe, // This should now match the expected type
    .remove = bmp180_remove,
    .id_table = bmp180_id,
};

module_i2c_driver(bmp180_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ban da sua <your.email@example.com>"); // Vui lòng cập nhật tên của bạn
MODULE_DESCRIPTION("BMP180 I2C Pressure/Temperature Sensor Driver with /dev interface (RPi Kernel Fixes)");
