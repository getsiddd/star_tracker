#include "camera.hpp"

#include <math.h>
#include <assert.h>
#include <iostream>

#include "attitude-utils.hpp"


#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <string.h> // strerrno
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <stdexcept>

#if defined(__linux__) // Or #if __linux__
#include <linux/videodev2.h>
#include <fcntl.h>
#endif

#define CLEAR(x) memset(&(x), 0, sizeof(x))

using namespace std;

namespace lost {

    /**
     * Converts from a 3D point in space to a 2D point on the camera sensor.
     * Assumes that X is the depth direction and that it points away from the center of the sensor, i.e., any vector (x, 0, 0) will be at (xResolution/2, yResolution/2) on the sensor.
     */
    Vec2 Camera::SpatialToCamera(const Vec3 &vector) const {
        // can't handle things behind the camera.
        assert(vector.x > 0);
        // TODO: is there any sort of accuracy problem when vector.y and vector.z are small?

        float focalFactor = focalLength/vector.x;

        float yPixel = vector.y*focalFactor;
        float zPixel = vector.z*focalFactor;

        return { -yPixel + xCenter, -zPixel + yCenter };
    }

    /**
     * Gives a point in 3d space that could correspond to the given vector, using the same
     * coordinate system described for SpatialToCamera.
     * Not all vectors returned by this function will necessarily have the same magnitude.
     * @return A vector in 3d space corresponding to the given vector, with x-component equal to 1
     * @warning Other functions rely on the fact that returned vectors are placed one unit away (x-component equal to 1). Don't change this behavior!
     */
    Vec3 Camera::CameraToSpatial(const Vec2 &vector) const {
        assert(InSensor(vector));

        // isn't it interesting: To convert from center-based to left-corner-based coordinates is the
        // same formula; f(x)=f^{-1}(x) !
        float xPixel = -vector.x + xCenter;
        float yPixel = -vector.y + yCenter;

        return {
            1,
            xPixel / focalLength,
            yPixel / focalLength,
        };
    }

    /// Returns whether a given pixel is actually in the camera's field of view
    bool Camera::InSensor(const Vec2 &vector) const {
        // if vector.x == xResolution, then it is at the leftmost point of the pixel that's "hanging
        // off" the edge of the image, so vector is still in the image.
        return vector.x >= 0 && vector.x <= xResolution
            && vector.y >= 0 && vector.y <= yResolution;
    }

    float FovToFocalLength(float xFov, float xResolution) {
        return xResolution / 2.0 / std::tan(xFov/2);
    }

    float FocalLengthToFov(float focalLength, float xResolution, float pixelSize) {
        return std::atan(xResolution/2 * pixelSize / focalLength) * 2;
    }

    float Camera::Fov() const {
        return FocalLengthToFov(focalLength, xResolution, 1.0);
    }

    Image* Camera::readImageFromV4L(int timeout) {
        return &frame(timeout);
    }

    bool Camera::open() {
        return open_device();
    }


    static int xioctl(int fh, unsigned long int request, void *arg)
    {
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
    }


    #define CLIP(color) (unsigned char)(((color) > 0xFF) ? 0xff : (((color) < 0) ? 0 : (color)))

    static void v4lconvert_yuyv_to_rgb24(const unsigned char *src, 
                                        unsigned char *dest,
                                        int width, int height, 
                                        int stride)
    {
        int j;

        while (--height >= 0) {
            for (j = 0; j + 1 < width; j += 2) {
                int u = src[1];
                int v = src[3];
                int u1 = (((u - 128) << 7) +  (u - 128)) >> 6;
                int rg = (((u - 128) << 1) +  (u - 128) +
                        ((v - 128) << 2) + ((v - 128) << 1)) >> 3;
                int v1 = (((v - 128) << 1) +  (v - 128)) >> 1;

                *dest++ = CLIP(src[0] + v1);
                *dest++ = CLIP(src[0] - rg);
                *dest++ = CLIP(src[0] + u1);

                *dest++ = CLIP(src[2] + v1);
                *dest++ = CLIP(src[2] - rg);
                *dest++ = CLIP(src[2] + u1);
                src += 4;
            }
            src += stride - (width * 2);
        }
    }
    /*******************************************************************/

    Camera::Camera(float focalLength, float xCenter, float yCenter, int xResolution, int yResolution, std::string cameraID) 
    : focalLength(focalLength), xCenter(xCenter), yCenter(yCenter), 
    xResolution(xResolution), yResolution(yResolution), cameraID(cameraID)
    {
        open_device();
        init_device();
        // xres and yres are set to the actual resolution provided by the cam

        // frame stored as RGB888 (ie, RGB24)
        rgb_frame.width = xResolution;
        rgb_frame.height = yResolution;
        rgb_frame.size = xResolution * yResolution * 3;
        start_capturing();
    }

    Camera::~Camera()
    {
        stop_capturing();
        uninit_device();
        close_device();

        free(rgb_frame.image);
    }

    Image& Camera::frame(int timeout)
    {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            /* Timeout. */
            tv.tv_sec = timeout;
            tv.tv_usec = 0;

            r = select(fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                BOOST_LOG_TRIVIAL(error) << "select";
                return rgb_frame;
            }

            if (0 == r) {
                BOOST_LOG_TRIVIAL(error) << cameraID << ": select timeout!";
                return rgb_frame;
            }
            if (read_frame()) {
                return rgb_frame;
            }
            /* EAGAIN - continue select loop. */
        }

    }

    bool Camera::read_frame()
    {
        #if defined(__linux__) // Or #if __linux__

        struct v4l2_buffer buf;
        unsigned int i;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
            switch (errno) {
                case EAGAIN:
                    return false;

                case EIO:
                    /* Could ignore EIO, see spec. */

                    /* fall through */

                default:
                BOOST_LOG_TRIVIAL(error) << "VIDIOC_DQBUF";
                return false;
            }
        }

        assert(buf.index < n_buffers);

        v4lconvert_yuyv_to_rgb24((unsigned char *) buffers[buf.index].data,
                                rgb_frame.image,
                                xResolution,
                                yResolution,
                                stride);

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
            BOOST_LOG_TRIVIAL(error) << "VIDIOC_QBUF";
            return false;
        
        return true;

        #elif defined(__APPLE__)
        return false;
        #else
        return false;
        #endif

        return false;
    }

    bool Camera::open_device(void)
    {   
        #if defined(__linux__) // Or #if __linux__
        struct stat st;

        if (-1 == stat(cameraID.c_str(), &st)) {
            BOOST_LOG_TRIVIAL(error) << cameraID << ": cannot identify! " + to_string(errno) +  ": " + strerror(errno);
            return false;
        }

        if (!S_ISCHR(st.st_mode)) {
            BOOST_LOG_TRIVIAL(error) << cameraID << " is no device!";
            return false;
        }

        fd = open(cameraID.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
            BOOST_LOG_TRIVIAL(error) << cameraID << ": cannot open!";
            return false;
        }
        else {
            BOOST_LOG_TRIVIAL(error) << cameraID << ": opened!";
            return true;
        }
        #elif defined(__APPLE__) // Or #if __linux__
        return false;
        #else
        //Using default opencv;
        cameraDevice.open(cameraID);
        if (isCameraOpened()) {
            std::cerr << "Camera is opened" << std::endl;
            return true;
        }
        else {
            std::cerr << "Real Time Read status: ERROR! Unable to open camera" << std::endl;
            return false;
        }
        #endif
        BOOST_LOG_TRIVIAL(error) << cameraID << ": cannot identify and Operating System issue ";
        return false;
    }


    void Camera::init_mmap(void)
    {
        #if defined(__linux__) // Or #if __linux__
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
            if (EINVAL == errno) {
                BOOST_LOG_TRIVIAL(error) << cameraID << ": does not support memory mapping!";
                return false;
            } else {
                BOOST_LOG_TRIVIAL(error) << "VIDIOC_REQBUFS";
                return false;
            }
        }

        if (req.count < 2) {
            BOOST_LOG_TRIVIAL(error) << cameraID << ": Insufficient buffer memory !";
            return false;
        }

        buffers = (buffer*) calloc(req.count, sizeof(*buffers));

        if (!buffers) {
            BOOST_LOG_TRIVIAL(error) << "Out of memory";
            return false;
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
            struct v4l2_buffer buf;

            CLEAR(buf);

            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = n_buffers;

            if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)){
                BOOST_LOG_TRIVIAL(error) << "VIDIOC_QUERYBUF";
                return false;
            }

            buffers[n_buffers].size = buf.length;
            buffers[n_buffers].data =
                    mmap(NULL /* start anywhere */,
                        buf.length,
                        PROT_READ | PROT_WRITE /* required */,
                        MAP_SHARED /* recommended */,
                        fd, buf.m.offset);

            if (MAP_FAILED == buffers[n_buffers].data){
                BOOST_LOG_TRIVIAL(error) << "mmap";
                return false;
            }
        }
        #endif
    }

    void Camera::close_device(void)
    {
        #if defined(__linux__) // Or #if __linux__
        if (-1 == close(fd)){
            BOOST_LOG_TRIVIAL(error) << "close";
            return false;
        }
        fd = -1;
        #endif
    }

    bool Camera::init_device(void)
    {
        #if defined(__linux__) // Or #if __linux__
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;
        unsigned int min;

        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
            if (EINVAL == errno) {
                BOOST_LOG_TRIVIAL(error) << cameraID << ": is no V4L2 device";
                return false;
            } else {
                BOOST_LOG_TRIVIAL(error) << cameraID << ": VIDIOC_QUERYCAP";
                return false;
            }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                BOOST_LOG_TRIVIAL(error) << cameraID << ": is no video capture device";
                return false;
        }

        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                BOOST_LOG_TRIVIAL(error) << cameraID << ": does not support streaming i/o";
                return false;
        }

        /* Select video input, video standard and tune here. */


        CLEAR(cropcap);

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
            crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            crop.c = cropcap.defrect; /* reset to default */

            if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
                switch (errno) {
                    case EINVAL:
                        /* Cropping not supported. */
                        break;
                    default:
                        /* Errors ignored. */
                        break;
                }
            }
        } else {
            /* Errors ignored. */
        }


        CLEAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (force_format) {
            fmt.fmt.pix.width       = xResolution;
            fmt.fmt.pix.height      = yResolution;
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
            fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

            if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                BOOST_LOG_TRIVIAL(error) << "VIDIOC_S_FMT";
                return false;

            if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
                // note that libv4l2 (look for 'v4l-utils') provides helpers
                // to manage conversions
                BOOST_LOG_TRIVIAL(error) << " : Webcam does not support YUYV format. Support for more format need to be added!";
                return false;

            /* Note VIDIOC_S_FMT may change width and height. */
            xResolution = fmt.fmt.pix.width;
            yResolution = fmt.fmt.pix.height;

            stride = fmt.fmt.pix.bytesperline;


        } else {
            /* Preserve original settings as set by v4l2-ctl for example */
            if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
                BOOST_LOG_TRIVIAL(error) << "VIDIOC_G_FMT";
                return false;
        }
        init_mmap();
        return true;
        #endif
        return false;
    }


    bool Camera::uninit_device(void)
    {   
        #if defined(__linux__) // Or #if __linux__
        unsigned int i;

        for (i = 0; i < n_buffers; ++i) {
            if (-1 == munmap(buffers[i].data, buffers[i].size)) {
                BOOST_LOG_TRIVIAL(error) << "munmap";
                return false;
            }
        }

        free(buffers);
        return true;
        #endif

        return false;
    }

    void Camera::start_capturing(void)
    {
        #if defined(__linux__) // Or #if __linux__
        unsigned int i;
        enum v4l2_buf_type type;

        for (i = 0; i < n_buffers; ++i) {
            struct v4l2_buffer buf;

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)){
                BOOST_LOG_TRIVIAL(error) << "munmap";
                return false;
            }
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)){
            BOOST_LOG_TRIVIAL(error) << "VIDIOC_STREAMON";
            return false;
        }
        #endif
    }

    bool Camera::stop_capturing(void)
    {
        #if defined(__linux__) // Or #if __linux__
        enum v4l2_buf_type type;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)){
            BOOST_LOG_TRIVIAL(error) << "VIDIOC_STREAMOFF";
            return false;
        }
        return true;
        #endif
        return false;
    }

}
