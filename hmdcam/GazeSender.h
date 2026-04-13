#pragma once
// GazeSender.h
// Takes SingleEyeFitter gaze results for left+right eyes and sends them
// to the main ESP32-S3 DevKitC over USB CDC (/dev/ttyACM0).
//
//   [left_pupil_d:    f32]
//   [left_openness:   f32]   // 0=closed, 1=fully open
//   [right_gaze_x:    f32] [right_gaze_y: f32] [right_gaze_z: f32]
//   [right_pupil_d:   f32]
//   [right_openness:  f32]   // 0=closed, 1=fully open
//   [0x55]
// Total: 42 bytes
//
// Gaze vectors are unit vectors in eye-centered world space:
//   +X = right, +Y = up, +Z = forward (toward viewer)
// Pupil diameter is in millimeters.
//
// Call send() from your render loop after SingleEyeFitter updates.
// It's non-blocking — if the serial port isn't open it silently skips.
//
// Usage:
//   GazeSender sender;
//   sender.open("/dev/ttyACM0");
//   ...
//   sender.send(left_gaze_vec, left_pupil_mm,
//               right_gaze_vec, right_pupil_mm);

#include <stdint.h>
#include <stdbool.h>

// Simple 3-component float vector
struct GazeVec3 {
    float x, y, z;
};

class GazeSender {
public:
    GazeSender();
    ~GazeSender();

    // Open the serial port to the ESP32 DevKitC.
    // Returns true on success. Safe to call multiple times.
    bool open(const char* device = "/dev/ttyACM0");
    void close();

    bool isOpen() const { return m_fd >= 0; }

    // Send one gaze packet. Non-blocking — returns false if port not open
    // or if write fails (e.g. ESP32 disconnected).
    // If either eye is invalid, pass {0,0,1} as the gaze vector (center).
    static const int PKT_SIZE = 42;  // was 34

    bool send(GazeVec3 leftGaze,  float leftPupilMm,  float leftOpenness,
              GazeVec3 rightGaze, float rightPupilMm, float rightOpenness);

    bool sendAveraged(GazeVec3 leftGaze,  float leftPupilMm,  float leftOpenness,
                      GazeVec3 rightGaze, float rightPupilMm, float rightOpenness);

private:
    static const uint8_t PKT_START = 0xAA;
    static const uint8_t PKT_END   = 0x55;

    int  m_fd;
    int  m_failCount;
    bool m_warned;

    static int openSerialPort(const char* path);
};