#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>

#define DRIVER_NAME "bmp180_driver"
#define DEVICE_NAME "bmp180"
#define CLASS_NAME "bmp180_class"

// BMP 
#define bmp180_REG_CONTROL     0xF4   // Control register bmp180
// Thanh ghi chứa kết quả MSB của cảm biến, đối với temp đọc 2 byte 0xF6 và 0xF7, đối với pressure cần 3 byte 0xF6, 0xF7 và 0xF8
#define bmp180_REG_RESULT      0xF6   
#define bmp180_CMD_TEMP        0x2E  // Enable sensor measure temp
#define bmp180_CMD_PRESSURE    0x34  // Enable sensor measure pressure với OS = 0

#define bmp180_IOCTL_MAGIC     'b'
#define bmp180_IOCTL_READ_TEMP _IOR(bmp180_IOCTL_MAGIC, 1, int)
#define bmp180_IOCTL_READ_PRESS _IOR(bmp180_IOCTL_MAGIC, 2, int)
///
static struct i2c_client *bmp180_client;
static struct class *bmp180_class;
static struct device *bmp180_device;
static int major_number;
// Đọc và hiệu chỉnh EEPROM
static short ac1, ac2, ac3, b1, b2, mb, mc, md; // biến hiệu chỉnh theo signed
static unsigned short ac4, ac5, ac6;  // theo unsigned

#define OSS 0 // Độ phân giải 

//
static int bmp180_read_calib_data(void)
{
    u8 calib_reg = 0xAA;
    u8 buf[22];
    int ret = i2c_smbus_read_i2c_block_data(bmp180_client, calib_reg, 22, buf);
    if (ret < 0) return ret;

    ac1 = (buf[0] << 8) | buf[1];
    ac2 = (buf[2] << 8) | buf[3];
    ac3 = (buf[4] << 8) | buf[5];
    ac4 = (buf[6] << 8) | buf[7];
    ac5 = (buf[8] << 8) | buf[9];
    ac6 = (buf[10] << 8) | buf[11];
    b1  = (buf[12] << 8) | buf[13];
    b2  = (buf[14] << 8) | buf[15];
    mb  = (buf[16] << 8) | buf[17];
    mc  = (buf[18] << 8) | buf[19];
    md  = (buf[20] << 8) | buf[21];

    return 0;
}
// đo nhiệt độ cảm biến 
static int bmp180_read_raw_temp(void)
{
    i2c_smbus_write_byte_data(bmp180_client, bmp180_REG_CONTROL, bmp180_CMD_TEMP);
    msleep(5);
    int msb = i2c_smbus_read_byte_data(bmp180_client, bmp180_REG_RESULT);  // LẤY MSB 
    int lsb = i2c_smbus_read_byte_data(bmp180_client, bmp180_REG_RESULT + 1);  // LẤY LSB 
    return (msb << 8) | lsb;  // Ghép bit 
}
// Đo áp suất 
static int bmp180_read_raw_press(void)
{
    i2c_smbus_write_byte_data(bmp180_client, bmp180_REG_CONTROL, bmp180_CMD_PRESSURE + (OSS << 6));  // 
    msleep(8);
    int msb = i2c_smbus_read_byte_data(bmp180_client, bmp180_REG_RESULT);
    int lsb = i2c_smbus_read_byte_data(bmp180_client, bmp180_REG_RESULT + 1);
    int xlsb = i2c_smbus_read_byte_data(bmp180_client, bmp180_REG_RESULT + 2);
    return ((msb << 16) | (lsb << 8) | xlsb) >> (8 - OSS);  // ghép bit theo data sheet 
}
// Tính nhiệt độ 
static int bmp180_get_b5(int *b5_out)
{
    int ut = bmp180_read_raw_temp();
    int x1 = ((ut - (int)ac6) * (int)ac5) >> 15;

    if ((x1 + md) == 0)
    return -EINVAL;
    
    int x2 = ((int)mc << 11) / (x1 + md);
    *b5_out = x1 + x2;
    return 0;
}

static int bmp180_read_temp(int *temp)
{
    int b5;
    bmp180_get_b5(&b5);
    if (temp)*temp = (b5 + 8) >> 4; // Chuyển nhiệt độ về đúng chuẩn 
    return 0;
}
// Tính áp suất 
static int bmp180_read_press(int *press)
{
    int up = bmp180_read_raw_press();
    int b5;
    bmp180_get_b5(&b5);

    int b6 = b5 - 4000;
    int x1 = (b2 * ((b6 * b6) >> 12)) >> 11;
    int x2 = (ac2 * b6) >> 11;
    int x3 = x1 + x2;
    int b3 = ((((int)ac1 * 4 + x3) << OSS) + 2) >> 2;

    x1 = (ac3 * b6) >> 13;
    x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    unsigned int b4 = (ac4 * (unsigned int)(x3 + 32768)) >> 15;
    unsigned int b7 = ((unsigned int)up - b3) * (50000 >> OSS);

    int p;
    if (b7 < 0x80000000)
        p = (b7 << 1) / b4;
    else
        p = (b7 / b4) << 1;

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);
    *press = p;

    return 0;
}
static long bmp180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int value = 0;

    switch (cmd) {
    case bmp180_IOCTL_READ_TEMP:
        bmp180_read_temp(&value);
        break;
    case bmp180_IOCTL_READ_PRESS:
        bmp180_read_press(&value);
        break;
    default:
        return -EINVAL;
    }

    if (copy_to_user((int __user *)arg, &value, sizeof(value)))
        return -EFAULT;

    return 0;
}


// open()
static int bmp180_open(struct inode *inode, struct file *file)
{
    pr_info("bmp180: Device opened\n");
    return 0;
}

// release()
static int bmp180_release(struct inode *inode, struct file *file)
{
    pr_info("bmp180: Device closed\n");
    return 0;
}

// file_operations
static struct file_operations bmp180_fops = {
    .owner = THIS_MODULE,
    .open = bmp180_open,
    .unlocked_ioctl = bmp180_ioctl,
    .release = bmp180_release,
};

// I2C probe
static int bmp180_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

    bmp180_client = client;
        // Đọc EEPROM trước
    if (bmp180_read_calib_data() < 0) {
        pr_err("bmp180: Failed to read calibration data\n");
        return -EIO;
    }

    // Đăng ký major number
    major_number = register_chrdev(0, DEVICE_NAME, &bmp180_fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register a major number\n");
        return major_number;
    }

    // Tạo device class
    bmp180_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(bmp180_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to register device class\n");
        return PTR_ERR(bmp180_class);
    }

    // Tạo device
    bmp180_device = device_create(bmp180_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(bmp180_device)) {
        class_destroy(bmp180_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create the device\n");
        return PTR_ERR(bmp180_device);
    }

    printk(KERN_INFO "bmp180 driver installed\n");
    return 0;
}

static void bmp180_remove(struct i2c_client *client)
{
    device_destroy(bmp180_class, MKDEV(major_number, 0));
    class_destroy(bmp180_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("bmp180: Removed\n");
}

static const struct i2c_device_id bmp180_id[] = {
    { "bmp180", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bmp180_id);

static struct i2c_driver bmp180_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe = bmp180_probe,
    .remove = bmp180_remove,
    .id_table = bmp180_id,
};

module_i2c_driver(bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("bmp180 I2C ADC Driver with /dev interface");