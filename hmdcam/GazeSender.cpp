// GazeSender.cpp
#include "GazeSender.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

GazeSender::GazeSender() : m_fd(-1), m_failCount(0), m_warned(false) {}

GazeSender::~GazeSender() { close(); }

int GazeSender::openSerialPort(const char* path) {
    int fd = ::open(path, O_WRONLY | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(fd, &tty);
    cfmakeraw(&tty);
    cfsetospeed(&tty, B115200); // irrelevant for USB CDC but required by termios
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &tty);

    // Switch back to blocking writes
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    return fd;
}

bool GazeSender::open(const char* device) {
    if (m_fd >= 0) return true;
    m_fd = openSerialPort(device);
    if (m_fd >= 0) {
        fprintf(stderr, "[GazeSender] Opened %s\n", device);
        m_failCount = 0;
        m_warned    = false;
        return true;
    }
    if (!m_warned) {
        fprintf(stderr, "[GazeSender] Could not open %s: %s\n",
                device, strerror(errno));
        fprintf(stderr, "[GazeSender] Eye data will not reach ESP32 displays.\n");
        m_warned = true;
    }
    return false;
}

void GazeSender::close() {
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
}

bool GazeSender::send(GazeVec3 leftGaze,  float leftPupilMm,  float leftOpenness,
                      GazeVec3 rightGaze, float rightPupilMm, float rightOpenness) {
    if (m_fd < 0) return false;

    uint8_t pkt[PKT_SIZE];
    pkt[0] = PKT_START;

    memcpy(&pkt[1],  &leftGaze.x,     4);
    memcpy(&pkt[5],  &leftGaze.y,     4);
    memcpy(&pkt[9],  &leftGaze.z,     4);
    memcpy(&pkt[13], &leftPupilMm,    4);
    memcpy(&pkt[17], &leftOpenness,   4);
    memcpy(&pkt[21], &rightGaze.x,    4);
    memcpy(&pkt[25], &rightGaze.y,    4);
    memcpy(&pkt[29], &rightGaze.z,    4);
    memcpy(&pkt[33], &rightPupilMm,   4);
    memcpy(&pkt[37], &rightOpenness,  4);

    pkt[41] = PKT_END;

    ssize_t n = write(m_fd, pkt, PKT_SIZE);
    if (n != PKT_SIZE) {
        m_failCount++;
        if (m_failCount % 100 == 1) {
            fprintf(stderr, "[GazeSender] Write failed (%zd/%d): %s\n",
                    n, PKT_SIZE, strerror(errno));
        }
        if (m_failCount > 200) {
            fprintf(stderr, "[GazeSender] Too many failures, closing port\n");
            close();
            m_failCount = 0;
        }
        return false;
    }

    m_failCount = 0;
    return true;
}

bool GazeSender::sendAveraged(GazeVec3 leftGaze,  float leftPupilMm,  float leftOpenness,
                               GazeVec3 rightGaze, float rightPupilMm, float rightOpenness) {
    GazeVec3 avg = {
        (leftGaze.x + rightGaze.x) * 0.5f,
        (leftGaze.y + rightGaze.y) * 0.5f,
        (leftGaze.z + rightGaze.z) * 0.5f,
    };
    float avgDiam     = (leftPupilMm    + rightPupilMm)    * 0.5f;
    float avgOpenness = (leftOpenness   + rightOpenness)   * 0.5f;

    float mag = sqrtf(avg.x*avg.x + avg.y*avg.y + avg.z*avg.z);
    if (mag > 0.001f) { avg.x /= mag; avg.y /= mag; avg.z /= mag; }

    return send(avg, avgDiam, avgOpenness, avg, avgDiam, avgOpenness);
}