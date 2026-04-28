#ifndef CAMERA_H
#define CAMERA_H

#include "attitude-utils.hpp"
#include "image.hpp"

#include <string>
#include <memory> // unique_ptr

#include <boost/log/trivial.hpp>

struct buffer {
      void   *data;
      size_t  size;
};

namespace lost {

/// A full description of a camera. Enough information to reconstruct the camera matrix and then some.
class Camera {
public:
    Camera(const Camera &) = default;

    /**
     * @param xCenter,yCenter The "principal point" of the camera. In an ideal camera, just half the resolution, but physical cameras often have a bit of offset.
     */
    Camera(float focalLength, float xCenter, float yCenter, int xResolution, int yResolution, std::string cameraID);

    Camera(float focalLength, int xResolution, int yResolution)
        : Camera(focalLength,
                 xResolution / float(2.0), yResolution / float(2.0),
                 xResolution, yResolution, 
                 "") {};
    
    Camera(float focalLength, int xResolution, int yResolution, std::string cameraID)
        : Camera(focalLength,
                 xResolution / float(2.0), yResolution / float(2.0),
                 xResolution, yResolution, 
                 cameraID) {};

    Vec2 SpatialToCamera(const Vec3 &) const;
    Vec3 CameraToSpatial(const Vec2 &) const;

    // converts from a 2d point in the camera sensor to right ascension and declination relative to
    // the center of the camera.
    // void CoordinateAngles(Vec2 &vector, float *ra, float *de);

    bool InSensor(const Vec2 &vector) const;

    /// Width of the sensor in pixels
    int XResolution() const { return xResolution; };
    /// Height of the sensor in pixels
    int YResolution() const { return yResolution; };
    /// Focal length in pixels
    float FocalLength() const { return focalLength; };
    /// Horizontal field of view in radians
    float Fov() const;

    void SetFocalLength(float focalLength) { this->focalLength = focalLength; }

    std::string CameraID() { return cameraID;}
    Image* readImageFromV4L(int timeout = 1);
    bool isCameraOpened();
    bool open(int index);
    bool open();
    bool close();

    ~Camera();

    /** Captures and returns a frame from the webcam.
     *
     * The returned object contains a field 'data' with the image data in RGB888
     * format (ie, RGB24), as well as 'width', 'height' and 'size' (equal to
     * width * height * 3)
     *
     * This call blocks until a frame is available or until the provided
     * timeout (in seconds). 
     *
     * Throws a runtime_error if the timeout is reached.
     */
    Image& frame(int timeout = 1);

private:
    // TODO: distortion
    float focalLength;
    float xCenter; float yCenter;
    int xResolution; int yResolution;
    std::string cameraID;

    void init_mmap();

    bool open_device();
    void close_device();

    bool init_device();
    bool uninit_device();

    void start_capturing();
    bool stop_capturing();

    bool read_frame();

    int fd;

    Image rgb_frame;
    struct buffer          *buffers;
    unsigned int     n_buffers;

    size_t stride;

    bool force_format = true;

};

float FovToFocalLength(float xFov, float xResolution);

}

#endif