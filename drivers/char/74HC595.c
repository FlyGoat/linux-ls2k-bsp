#include <linux/module.h>
#include <linux/init.h> 
#include <linux/fs.h> 
#include <linux/cdev.h>
#include <asm/uaccess.h> 
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL"); //指定代码使用的许可证



//文件操作函数的声明
int my74hc595_open(struct inode *, struct file *);
int my74hc595_release(struct inode *, struct file *);
ssize_t my74hc595_read(struct file *, char *, size_t, loff_t *);
ssize_t my74hc595_write(struct file *, const char *, size_t, loff_t *);

int dev_major = 1253; //指定主设备号
int dev_minor = 0; //指定次设备号

static struct class *firstdrv_class;
static struct device *firstdrv_class_dev;

struct cdev *my74hc595_cdev; //内核中表示字符设备的结构
int *gp_testdata;//测试用数据

#define DIO0	gpio_set_value(59,0);
#define DIO1 	gpio_set_value(59,1);
#define SCLK0	gpio_set_value(57,0);
#define SCLK1	gpio_set_value(57,1);
#define RCLK0	gpio_set_value(58,0);
#define RCLK1	gpio_set_value(58,1);

unsigned char  LED_0F[] = 
{// 0	 1	  2	   3	4	 5	  6	   7	8	 9	  A	   b	C    d	  E    F    -
	0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90,0x8C,0xBF,0xC6,0xA1,0x86,0xFF,0xbf
};
void LED_OUT(unsigned char X)
{
	unsigned char i;
	for(i=8;i>=1;i--)
	{
		if (X&0x80)
		{ 
			DIO1 
		}else{
			DIO0
		}
		X<<=1;
//		SCLK = 0;
//		SCLK = 1;
		SCLK0
		msleep(300);
		SCLK1
	}
}


struct file_operations my74hc595_fops= //将文件操作与分配的设备号相连
{
	owner: THIS_MODULE, //指向拥有该模块结构的指针
	open: my74hc595_open,
	release: my74hc595_release,
	read: my74hc595_read,
	write: my74hc595_write,
};

static void __exit my74hc595_exit(void) //退出模块时的操作
{
	dev_t devno=MKDEV(dev_major, dev_minor); //dev_t是用来表示设备编号的结构

	cdev_del(my74hc595_cdev); //从系统中移除一个字符设备
	kfree(my74hc595_cdev); //释放自定义的设备结构
	kfree(gp_testdata);
	unregister_chrdev_region(devno, 1); //注销已注册的驱动程序

	device_unregister(firstdrv_class_dev); //删除/dev下对应的字符设备节点
	class_destroy(firstdrv_class);

	printk("my74hc595 unregister success\n");
}

static int __init my74hc595_init(void) //初始化模块的操作
{
	int ret, err;
	dev_t devno;
#if 1
	//动态分配设备号，次设备号已经指定 
	ret=alloc_chrdev_region(&devno, dev_minor, 1, "my74hc595");
	//保存动态分配的主设备号
	dev_major=MAJOR(devno);
#else
	//根据期望值分配设备号
	devno=MKDEV(dev_major, dev_minor);
	ret=register_chrdev_region(devno, 1, "my74hc595");
#endif

	if(ret<0)
	{
		printk("my74hc595 register failure\n");
	//my74hc595_exit(); //如果注册设备号失败就退出系统
		return ret;
	}
	else
	{
		printk("my74hc595 register success\n");
	}

	gp_testdata = kmalloc(sizeof(int), GFP_KERNEL);
#if 0//两种初始化字符设备信息的方法
	my74hc595_cdev = cdev_alloc();//调试时，此中方法在rmmod后会出现异常，原因未知
	my74hc595_cdev->ops = &my74hc595_fops;
#else
	my74hc595_cdev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	cdev_init(my74hc595_cdev, &my74hc595_fops);
#endif

	my74hc595_cdev->owner = THIS_MODULE; //初始化cdev中的所有者字段

	err=cdev_add(my74hc595_cdev, devno, 1); //向内核添加这个cdev结构的信息
	if(err<0)
	printk("add device failure\n"); //如果添加失败打印错误消息

	firstdrv_class = class_create(THIS_MODULE, "my74hc595");
	firstdrv_class_dev = device_create(firstdrv_class, NULL, MKDEV(dev_major, 0), NULL,"my74hc595-%d", 0);//在/dev下创建节点

	printk("register my74hc595 dev OK\n"); 

	return 0;
}
//打开设备文件系统调用对应的操作
int my74hc595_open(struct inode *inode, struct file *filp)
{ 
	//将file结构中的private_data字段指向已分配的设备结构
	filp->private_data = gp_testdata;
	printk("open my74hc595 dev OK\n");
	return 0;
}
//关闭设备文件系统调用对应的操作 
int my74hc595_release(struct inode *inode, struct file *filp)
{
	printk("close my74hc595 dev OK\n");
	return 0;
}
//读设备文件系统调用对应的操作
ssize_t my74hc595_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	//获取指向已分配数据的指针
	unsigned int *p_testdata = filp->private_data;
	//将设备变量值复制到用户空间
	if(copy_to_user(buf, p_testdata, sizeof(int)))
	{
		return -EFAULT;
	}
	printk("read my74hc595 dev OK\n");
	return sizeof(int); //返回读取数据的大小
}
//GPIO 57 sclk
//gpio 58 rclk
//gpio 59 dio
static int write_to_74hc(const char *buf, size_t len)
{
	unsigned int v_len,data,i,v_data;
	unsigned char val,val1;

	for(i=0; i<len; i=i+1)
	{
		printk(">>>>>>>>>>>>>>i=%d<<<<<<<<<<<<\n",i);
	  printk("buf[%d]=0x%x\n", i, buf[i]);

		val = *(LED_0F+buf[i]);
		printk("val=0x%x\n",val);
		LED_OUT(val);

		msleep(400);
	//	val1 = buf[i+1];
		val1 = 1<<i;
		printk("val1=0x%x \n", val1);
		LED_OUT(val1);
		msleep(200);
		RCLK0
		msleep(100);
		RCLK1
	}
	return 0;
}
ssize_t my74hc595_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
	unsigned int *p_testdata = filp->private_data;
	unsigned int i;

//	if(copy_from_user(p_testdata, buf, sizeof(int)))
	if(copy_from_user(p_testdata, buf, 8))
	{
		return -EFAULT;
	}
	
	write_to_74hc(buf, len);
	return sizeof(int); 
}

module_init(my74hc595_init); 
//module_exit(my75hc595_exit); 
