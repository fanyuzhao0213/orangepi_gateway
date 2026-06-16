#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

#define FB_DEV      "/dev/fb0"
#define INPUT_DEV   "/dev/input/event1"

static uint16_t *fb_mem;
static int fb_width;
static int fb_height;

static void draw_point(int x,int y,uint16_t color)
{
    if(x < 0 || x >= fb_width)
        return;

    if(y < 0 || y >= fb_height)
        return;

    fb_mem[y * fb_width + x] = color;
}

static void draw_circle(int x0,int y0)
{
    int x,y;

    for(y=-3;y<=3;y++)
    {
        for(x=-3;x<=3;x++)
        {
            if(x*x+y*y<=9)
            {
                draw_point(x0+x,y0+y,0xF800);
            }
        }
    }
}

int main(void)
{
    int fb_fd;
    int input_fd;

    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    fb_fd = open(FB_DEV,O_RDWR);

    if(fb_fd < 0)
    {
        perror("open fb");
        return -1;
    }

    ioctl(fb_fd,FBIOGET_FSCREENINFO,&finfo);
    ioctl(fb_fd,FBIOGET_VSCREENINFO,&vinfo);

    fb_width  = vinfo.xres;
    fb_height = vinfo.yres;

    printf("LCD: %d x %d\n",
           fb_width,
           fb_height);

    fb_mem = mmap(NULL,
                  finfo.smem_len,
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  fb_fd,
                  0);

    if(fb_mem == MAP_FAILED)
    {
        perror("mmap");
        return -1;
    }

    memset(fb_mem,0,finfo.smem_len);

    input_fd = open(INPUT_DEV,O_RDONLY);

    if(input_fd < 0)
    {
        perror("open input");
        return -1;
    }

    struct input_event ev;

    int x = 0;
    int y = 0;

    while(1)
    {
        int ret = read(input_fd,
                       &ev,
                       sizeof(ev));

        if(ret != sizeof(ev))
            continue;

        if(ev.type == EV_ABS)
        {
            if(ev.code == ABS_MT_POSITION_X)
            {
                x = ev.value;
            }

            if(ev.code == ABS_MT_POSITION_Y)
            {
                y = ev.value;
            }
        }

        if(ev.type == EV_SYN &&
           ev.code == SYN_REPORT)
        {
            printf("\rX=%3d Y=%3d",
                   x,
                   y);

            fflush(stdout);

            draw_circle(x,y);
        }
    }

    munmap(fb_mem,finfo.smem_len);

    close(input_fd);
    close(fb_fd);

    return 0;
}

