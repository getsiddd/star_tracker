#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>              /* low-level i/o */
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

static int xioctl(int fh, int request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && eintr == errno);
    return r;
}

int alloccamera(char* file) 
{
    struct v4l2_capability cap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;

    int camera_fd = open(file, o_rdonly);

    if (-1 == xioctl (camera_fd, vidioc_querycap, &cap)) {
        if (einval == errno) {
            fprintf (stderr, "%s is no v4l2 device\n", file);
            exit (exit_failure);
        } else {
            printf("\nerror in ioctl vidioc_querycap\n\n");
            exit(0);
        }
    }

    if (!(cap.capabilities & v4l2_cap_video_capture)) {
        fprintf (stderr, "%s is no video capture device\n", file);
        exit (exit_failure);
    }

    if (!(cap.capabilities & v4l2_cap_readwrite)) {
        fprintf (stderr, "%s does not support read i/o\n", file);
        exit (exit_failure);
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type    = v4l2_buf_type_video_capture;
    fmt.fmt.pix.width       = 320; 
    fmt.fmt.pix.height      = 240;
    fmt.fmt.pix.pixelformat = v4l2_pix_fmt_yuyv;
    fmt.fmt.pix.field       = v4l2_field_interlaced;
    if (-1 == xioctl(camera_fd, vidioc_s_fmt, &fmt)) {
        printf("vidioc_s_fmt");
    }
    return camera_fd;
}