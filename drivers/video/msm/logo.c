/* drivers/video/msm/logo.c
 *
 * Show Logo in RLE 565 format
 *
 * Copyright (C) 2008 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/irq.h>
#include <asm/system.h>

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * 2)
/*
 rgb565 to rgba8888
  - by jollaman999 -
 Referenced : http://hanburn.tistory.com/140

<GET_RGB565_VALUE> (RRRR RGGG GGGB BBBB)
 |      |      |      |      | RRRR | R000 | 0000 | 0000 |
 |      |      |      |      |      |  GGG | GGG0 | 0000 |
 |      |      |      |      |      |      |    B | BBBB |

<SHIFT_TO_END>
 |      |      |      |      |      |      |    R | RRRR | >> 11
 |      |      |      |      |      |      |   GG | GGGG | >> 5
 |      |      |      |      |      |      |    B | BBBB |
 |      |      |      |      |      |      | AAAA | AAAA |

<SHIFT_TO_GET_REAL_VALUE>
 |      |      |      |      |      |      | RRRR | R    | << 3
 |      |      |      |      |      |      | GGGG | GG   | << 2
 |      |      |      |      |      |      | BBBB | B    | << 3
 |      |      |      |      |      |      | AAAA | AAAA |

<REPLACE - RGBA8888>
 | RRRR | R    |      |      |      |      |      |      | << 24
 |      |      | GGGG | GG   |      |      |      |      | << 16
 |      |      |      |      | BBBB | B    |      |      | << 8
 |      |      |      |      |      |      | AAAA | AAAA |
*/
#ifdef CONFIG_FB_MSM_DEFAULT_DEPTH_RGBA8888
int rgb565_to_rgba8888(unsigned short val)
{
	int r = val & 0xf800 >> 11;
	int g = val & 0x07e0 >> 5;
	int b = val & 0x001f;
	int a = val & 0xff;

	r = r << 3;
	g = g << 2;
	b = b << 3;

	return (r << 24) | (g << 16) | (b << 8) | (a);
}

static void memset32(void *_ptr, unsigned short val, unsigned count)
{
	int *ptr = _ptr;
	count >>= 1;
	while (count--)
		*ptr++ = rgb565_to_rgba8888(val);
}
#else
static void memset16(void *_ptr, unsigned short val, unsigned count)
{
	unsigned short *ptr = _ptr;
	count >>= 1;
	while (count--)
		*ptr++ = val;
}
#endif /* CONFIG_FB_MSM_DEFAULT_DEPTH_RGBA8888 */


/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename, bool bf_supported)
{
	struct fb_info *info;
	int fd, count, err = 0;
	unsigned max;
	unsigned short *data, *bits, *ptr;

	info = registered_fb[0];
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}
	count = sys_lseek(fd, (off_t)0, 2);
	if (count <= 0) {
		err = -EIO;
		goto err_logo_close_file;
	}
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if (sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	if (bf_supported && (info->node == 1 || info->node == 2)) {
		err = -EPERM;
		pr_err("%s:%d no info->creen_base on fb%d!\n",
		       __func__, __LINE__, info->node);
		goto err_logo_free_data;
	}
	if (info->screen_base) {
		bits = (unsigned short *)(info->screen_base);
		while (count > 3) {
			unsigned n = ptr[0];
			if (n > max)
				break;
			#ifdef CONFIG_FB_MSM_DEFAULT_DEPTH_RGBA8888
			memset32(bits, ptr[1], n << 1);
			#else
			memset16(bits, ptr[1], n << 1);
			#endif /* CONFIG_FB_MSM_DEFAULT_DEPTH_RGBA8888 */
			bits += n;
			max -= n;
			ptr += 2;
			count -= 4;
		}
	}

err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
EXPORT_SYMBOL(load_565rle_image);
