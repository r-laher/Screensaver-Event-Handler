#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/mutex.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <linux/uaccess.h> // copy_from/to_user
#include <asm/uaccess.h> // ^same
#include <linux/sched.h> // for timers
#include <linux/jiffies.h> // for jiffies global variable
#include <linux/string.h> // for string manipulation functions
#include <linux/ctype.h> // for isdigit

#include <linux/delay.h> //for msleep()
#include <linux/input.h> //for input_register_handler()


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elise DeCarli and Rebecca Laher");
MODULE_DESCRIPTION("EC535 Intro to Embedded Systems Lab5: CITGO Sign Screensaver");

//graphics colors
#define CYG_FB_DEFAULT_PALETTE_BLUE         0x01
#define CYG_FB_DEFAULT_PALETTE_RED          0x04
#define CYG_FB_DEFAULT_PALETTE_WHITE        0x0F
#define CYG_FB_DEFAULT_PALETTE_LIGHTBLUE    0x09
#define CYG_FB_DEFAULT_PALETTE_BLACK        0x00


// CHARACTER DEVICE VARIABLES
static int myscreensaver_major = 61;

// GRAPHICS VARIABLES
struct fb_info *info;
struct fb_fillrect *blank;
struct fb_fillrect *background;
struct fb_fillrect *triangle[288];
struct fb_fillrect *CITGO[15];
struct fb_fillrect *offs[376];
struct fb_fillrect *ons[376];
struct fb_fillrect *trioff[288];
static int sleeping = 1;
void Activate_Screensaver(void);
void build_arrays(void);


// TIMER VARIABLES
static struct timer_list Timer;
int del_timer_sync(struct timer_list *Timer);
void timer_function(struct timer_list *Timer)
{
	sleeping = 1;
	Activate_Screensaver();
}


// FILE OPERATION FUNCTIONS
static int myscreensaver_open(struct inode *inode, struct file *filp);
static int myscreensaver_release(struct inode *inode, struct file *filp);
struct file_operations myscreensaver_fops = {
	open: myscreensaver_open,
	release: myscreensaver_release,
};


/*---------------------------------------------------The following functions borrowed from drivers/linux/evdev.c and keyboard.c----------------------------------------------------*/
static struct input_device_id screenev_ids[2];
static int screenev_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id);
static void screenev_disconnect(struct input_handle *handle);
static void screenev_event(struct input_handle *handle, unsigned int type, unsigned int code, int value);

/* from keyboard.c */
static int screenev_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
  		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "kbd";

	error = input_register_handle(handle);
	if (error)
  		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
  		goto err_unregister_handle;

	return 0;

err_unregister_handle:
	input_unregister_handle(handle);
err_free_handle:
	kfree(handle);
	return error;
}

/* from keyboard.c */
static void screenev_disconnect(struct input_handle *handle)
{
  	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

MODULE_DEVICE_TABLE(input, screenev_ids);

/* from evdev.d */
static struct input_device_id screenev_ids[2] = {
	{ .driver_info = 1 },	// Matches all devices
	{ },			// Terminating zero entry
};

static void screenev_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	// Runs when an event occurs
	sleeping = 0;
	mod_timer(&Timer,  jiffies + 15 * HZ);
}

/* modified from evdev.c */
static struct input_handler screenev_handler = {
	.event		= screenev_event,
	.connect	= screenev_connect,
	.disconnect	= screenev_disconnect,
	.legacy_minors	= true,
	.name		= "screenev",
	.id_table	= screenev_ids,
};

/*---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* Helper function borrowed from drivers/video/fbdev/core/fbmem.c */
static struct fb_info *get_fb_info(unsigned int idx)
{
	struct fb_info *fb_info;

	if (idx >= FB_MAX)
        	return ERR_PTR(-ENODEV);

	fb_info = registered_fb[idx];
	if (fb_info)
        	atomic_inc(&fb_info->count);

	return fb_info;
}

/***********************************************************************************************************************************************************************************/

static int __init myscreensaver_init(void)
{
	int result, ret;

	printk(KERN_INFO "Hello framebuffer!\n");

	// Register device
	result = register_chrdev(myscreensaver_major, "myscreensaver", &myscreensaver_fops);
	if (result < 0)
	{
        	printk(KERN_ALERT "Cannot obtain major number");
		return result;
    	}


	// Register input handler
	ret = input_register_handler(&screenev_handler);
	if (ret < 0)
	{
		printk(KERN_ALERT "Unable to register input handler");
		return ret;
	}


	// Initialize all graphics arrays
	build_arrays();

	// Blank the background
	blank = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
        blank->dx = 0;
        blank->dy = 0;
        blank->width = 1050;
        blank->height = 750;
        blank->color = CYG_FB_DEFAULT_PALETTE_BLACK;
        blank->rop = ROP_COPY;
	info = get_fb_info(0);
        lock_fb_info(info);
	sys_fillrect(info, blank);
	unlock_fb_info(info);

	// Start 15 second timer
	timer_setup(&Timer, timer_function, 0);
        mod_timer(&Timer, jiffies + 15 * HZ);

	printk(KERN_INFO "Module init complete\n");

    	return 0;
}

static void __exit myscreensaver_exit(void) {
	//will run when call rmmod myscreensaver
	//may cause page domain fault or kernel oops if screensaver is currently drawing - needs to be off in order to not cause error and remove successfully
	// Cleanup framebuffer graphics
	int i;
	kfree(blank);
        kfree(background);
        for(i=0; i<288; i++)
        {
                kfree(triangle[i]);
                kfree(trioff[i]);
        }
        for(i=0; i<15; i++)
        {
                kfree(CITGO[i]);
        }
	for(i=0; i<376; i++)
        {
                kfree(offs[i]);
                kfree(ons[i]);
        }

	// Unregister evdev handler
	input_unregister_handler(&screenev_handler);

	// Delete remaining timers
	del_timer_sync(&Timer);

	// Freeing the major number
        unregister_chrdev(myscreensaver_major, "myscreensaver");

    	printk(KERN_INFO "Goodbye framebuffer!\n");
    	printk(KERN_INFO "Module exiting\n");
}

module_init(myscreensaver_init);
module_exit(myscreensaver_exit);


static int myscreensaver_open(struct inode *inode, struct file *filp)
{
        /* Success */
        return 0;
}

static int myscreensaver_release(struct inode *inode, struct file *filp)
{
        /* Success */
        return 0;
}

void build_arrays(void)
{
	int i, j;
    	// Set up white background
    	background = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
	background->dx = 150;
	background->dy = 35;
    	background->width = 700;
    	background->height = 700;
    	background->color = CYG_FB_DEFAULT_PALETTE_WHITE;
    	background->rop = ROP_COPY;


    	// Set up triangle
    	for(i=0; i<288; i++)
    	{
        	triangle[i] = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
        	triangle[i]->color = CYG_FB_DEFAULT_PALETTE_RED;
                triangle[i]->rop = ROP_COPY;
        	triangle[i]->dx = 150 + (349 - i);

        	if(i%2==0){
        		triangle[i]->dy = 44 + 3*(i/2);
        	    	triangle[i]->width = 2*(i+1);
            		triangle[i]->height = 2;
        	}else{
            		triangle[i]->dy = triangle[i-1]->dy + 2;
            		triangle[i]->width = triangle[i-1]->width + 2;
            		triangle[i]->height = 1;
        	}
    	}


    	// Set up CITGO
    	for(i=0; i<15; i++)
    	{
        	CITGO[i] = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
        	CITGO[i]->color = CYG_FB_DEFAULT_PALETTE_BLUE;
        	CITGO[i]->rop = ROP_COPY;
    	}

             //the "C"
    	CITGO[0]->dx = 233;
    	CITGO[0]->dy = 550;
    	CITGO[0]->width = 75;
    	CITGO[0]->height = 20;

	CITGO[1]->dx = 233;
        CITGO[1]->dy = 550;
        CITGO[1]->width = 20;
        CITGO[1]->height = 100;

    	CITGO[2]->dx = 233;
        CITGO[2]->dy = 630;
        CITGO[2]->width = 75;
        CITGO[2]->height = 20;

             //the "I"
    	CITGO[3]->dx = 358;
        CITGO[3]->dy = 550;
        CITGO[3]->width = 20;
        CITGO[3]->height = 100;

             //the "T"
    	CITGO[4]->dx = 428;
        CITGO[4]->dy = 550;
        CITGO[4]->width = 80;
        CITGO[4]->height = 20;

        CITGO[5]->dx = 458;
        CITGO[5]->dy = 550;
        CITGO[5]->width = 20;
        CITGO[5]->height = 100;

             //the "G"
    	CITGO[6]->dx = 558;
        CITGO[6]->dy = 550;
        CITGO[6]->width = 80;
        CITGO[6]->height = 20;

        CITGO[7]->dx = 558;
        CITGO[7]->dy = 550;
        CITGO[7]->width = 20;
        CITGO[7]->height = 100;

        CITGO[8]->dx = 558;
        CITGO[8]->dy = 630;
        CITGO[8]->width = 80;
        CITGO[8]->height = 20;

    	CITGO[9]->dx = 618;
        CITGO[9]->dy = 600;
        CITGO[9]->width = 20;
        CITGO[9]->height = 30;

        CITGO[10]->dx = 598;
        CITGO[10]->dy = 590;
        CITGO[10]->width = 40;
        CITGO[10]->height = 20;

             //the "O"
    	CITGO[11]->dx = 688;
        CITGO[11]->dy = 550;
        CITGO[11]->width = 75;
        CITGO[11]->height = 20;

        CITGO[12]->dx = 688;
        CITGO[12]->dy = 550;
        CITGO[12]->width = 20;
        CITGO[12]->height = 100;

        CITGO[13]->dx = 688;
        CITGO[13]->dy = 630;
        CITGO[13]->width = 75;
        CITGO[13]->height = 20;

        CITGO[14]->dx = 743;
        CITGO[14]->dy = 550;
        CITGO[14]->width = 20;
        CITGO[14]->height = 100;


    	// Set up background off array
    	for(i=0; i<230; i++)
    	{
        	offs[i] = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
                offs[i]->dx = 150;
        	offs[i]->dy = 731 - 3*i;
                offs[i]->width = 700;
        	offs[i]->height = 1;
                offs[i]->color = CYG_FB_DEFAULT_PALETTE_BLACK;
                offs[i]->rop = ROP_COPY;
    	}
    	for(i=86; i<230; i++)
    	{
        	j = i-86;
                offs[i]->width = (700 - triangle[287-2*j]->width)/2;
    	}
    	for(i=230; i<374; i++)
    	{
        	offs[i] = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
        	offs[i]->height = 1;
                offs[i]->color = CYG_FB_DEFAULT_PALETTE_BLACK;
                offs[i]->rop = ROP_COPY;

        	j = i - 230;
        	offs[i]->width = (700 - triangle[287-2*j]->width)/2;
                offs[i]->dx = 850 - offs[i]->width;
        	offs[i]->dy = 731 - 3*(86 + j);
    	}
    	for(i=374; i<376; i++)
    	{
        	offs[i] = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
                offs[i]->dx = 150;
                offs[i]->dy = 38 + 3*(375-i);
                offs[i]->width = 700;
                offs[i]->height = 1;
                offs[i]->color = CYG_FB_DEFAULT_PALETTE_BLACK;
                offs[i]->rop = ROP_COPY;
    	}


    	// Set up background on array
    	for(i=0; i<376; i++)
    	{
        	ons[i] = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
                ons[i]->dx = offs[i]->dx;
                ons[i]->dy = offs[i]->dy;
                ons[i]->width = offs[i]->width;
                ons[i]->height = 1;
                ons[i]->color = CYG_FB_DEFAULT_PALETTE_WHITE;
                ons[i]->rop = ROP_COPY;
    	}


    	// Set up triagle off array
    	for(i=0; i<288; i++)
    	{
        	trioff[i] = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
                trioff[i]->dx = triangle[i]->dx;
                trioff[i]->dy = triangle[i]->dy;
                trioff[i]->width = triangle[i]->width;
                trioff[i]->height = triangle[i]->height;
		trioff[i]->rop = ROP_COPY;
        	if(i%2==0){
                	trioff[i]->color = CYG_FB_DEFAULT_PALETTE_BLACK;
        	}else{
            		trioff[i]->color = CYG_FB_DEFAULT_PALETTE_RED;
                }
    	}
}



void Activate_Screensaver(void)
{
	int i, j;
	info = get_fb_info(0);
        lock_fb_info(info);

	// Initial Setup
	background->color = CYG_FB_DEFAULT_PALETTE_WHITE;
	sys_fillrect(info, background);
        for(i=0; i<288; i++)
        {
        	sys_fillrect(info, triangle[i]);
        }

        for(i=0; i<15; i++)
        {
                sys_fillrect(info, CITGO[i]);
        }
	if(sleeping==0)
		return;
	msleep(1500);

	while(sleeping==1)
	{
                // background off up
                for(i=0; i<233; i++)
                {
                        if(i<28){
                                sys_fillrect(info, offs[i]);
			}else if(i>=28 && i<86){
				sys_fillrect(info, offs[i]);
				for(j=0; j<15; j++)
				{
					sys_fillrect(info, CITGO[j]);
				}
			}else if(i>=86 && i<230){
                                j = i - 86;
                                sys_fillrect(info, offs[i]);
                                sys_fillrect(info, offs[230+j]);
                        }else{
                              	sys_fillrect(info, offs[i+143]);
                        }
			if(sleeping==0)
                        	break;
			msleep(100);
                }
		if(sleeping==0)
                	break;
                msleep(200);

		// background on down
                for(i=232; i>=0; i--)
                {
                        if(i<28){
                                sys_fillrect(info, ons[i]);
                        }else if(i>=28 && i<86){
				sys_fillrect(info, ons[i]);
				for(j=0; j<15; j++)
                                {
                                        sys_fillrect(info, CITGO[j]);
                                }
			}else if(i>=86 && i<230){
                                j = i - 86;
                                sys_fillrect(info, ons[i]);
                                sys_fillrect(info, ons[230+j]);
                        }else{
                              	sys_fillrect(info, ons[i+143]);
                        }
			if(sleeping==0)
				break;
			msleep(100);
                }
		if(sleeping==0)
			break;
                msleep(100);

                // triangle off up
                for(i=287; i>=0; i--)
                {
                        sys_fillrect(info, trioff[i]);
                        if(sleeping==0)
				break;
			msleep(50);
                }
		if(sleeping==0)
			break;
                msleep(100);

		// triangle on down
                for(i=0; i<288; i++)
                {
                        sys_fillrect(info, triangle[i]);
                        if(sleeping==0)
				break;
			msleep(50);
                }
		if(sleeping==0)
			break;
                msleep(100);

                // CITGO on else off
                for(i=0; i<15; i++)
                {
                        sys_fillrect(info, CITGO[i]);
                }
                for(i=0; i<288; i++)
                {
                        sys_fillrect(info, trioff[i]);
                }
                for(i=0; i<376; i++)
                {
                        sys_fillrect(info, offs[i]);
                }
		if(sleeping==0)
			break;
                msleep(1500);

		// all on
                for(i=0; i<288; i++)
                {
                        sys_fillrect(info, triangle[i]);
                }
                for(i=0; i<376; i++)
                {
                        sys_fillrect(info, ons[i]);
                }
                for(i=0; i<15; i++)
                {
                        sys_fillrect(info, CITGO[i]);
                }
		if(sleeping==0)
			break;
                msleep(1500);

                // CITGO on else off
                for(i=0; i<288; i++)
                {
                        sys_fillrect(info, trioff[i]);
                }
                for(i=0; i<376; i++)
                {
                        sys_fillrect(info, offs[i]);
                }
                for(i=0; i<15; i++)
                {
                        sys_fillrect(info, CITGO[i]);
                }
		if(sleeping==0)
			break;
                msleep(1500);

		// all on
                for(i=0; i<288; i++)
                {
                        sys_fillrect(info, triangle[i]);
                }
                for(i=0; i<376; i++)
                {
                        sys_fillrect(info, ons[i]);
                }
                for(i=0; i<15; i++)
                {
                        sys_fillrect(info, CITGO[i]);
                }
		if(sleeping==0)
			break;
        	msleep(1500);
	}
	background->color = CYG_FB_DEFAULT_PALETTE_BLACK;
	sys_fillrect(info, background);
	unlock_fb_info(info);
}
