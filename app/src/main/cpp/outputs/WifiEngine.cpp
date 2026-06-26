#include "WifiEngine.h"
#include "../UICallback.h"
#include <android/log.h>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#define LOG_TAG "WifiEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

WifiEngine::WifiEngine(std::string host, int port)
        : mHost(std::move(host)), mPort(port) {}

WifiEngine::~WifiEngine() {
    stop();
    clearOutputPort();
}

void WifiEngine::start() {
    stop();

    int error = 0;
    mOpusEncoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
    if (error != OPUS_OK) {
        LOGE("Opus encoder creation failed: %d", error);
        return;
    }
    opus_encoder_ctl(mOpusEncoder, OPUS_SET_BITRATE(64000));
    opus_encoder_ctl(mOpusEncoder, OPUS_SET_COMPLEXITY(0));

    mUdpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (mUdpSocket < 0) {
        LOGE("UDP socket creation failed: %s", strerror(errno));
        opus_encoder_destroy(mOpusEncoder);
        mOpusEncoder = nullptr;
        return;
    }

    std::memset(&mUdpDest, 0, sizeof(mUdpDest));
    mUdpDest.sin_family = AF_INET;
    mUdpDest.sin_port   = htons(static_cast<uint16_t>(mPort));
    if (inet_pton(AF_INET, mHost.c_str(), &mUdpDest.sin_addr) <= 0) {
        LOGE("Invalid UDP address: %s", mHost.c_str());
        close(mUdpSocket);
        mUdpSocket = -1;
        opus_encoder_destroy(mOpusEncoder);
        mOpusEncoder = nullptr;
        return;
    }

    mUdpRunning.store(true, std::memory_order_relaxed);
    mUdpThread = std::thread(&WifiEngine::udpRenderLoop, this);
    if (mUICallback) mUICallback->start();
    LOGD("UDP audio streaming started — %s:%d", mHost.c_str(), mPort);
}

void WifiEngine::stop() {
    if (mUdpRunning.exchange(false)) {
        if (mUICallback) mUICallback->stop();
        if (mUdpThread.joinable()) mUdpThread.join();
    }
    if (mUdpSocket >= 0) {
        close(mUdpSocket);
        mUdpSocket = -1;
    }
    if (mOpusEncoder) {
        opus_encoder_destroy(mOpusEncoder);
        mOpusEncoder = nullptr;
    }
}

void WifiEngine::udpRenderLoop() {
    constexpr int kFramesPerBuf = 120;  // 10 ms — valid Opus frame size
    float buf[kFramesPerBuf];

    struct timespec nextWake;
    clock_gettime(CLOCK_MONOTONIC, &nextWake);
    const int64_t intervalNs = static_cast<int64_t>(kFramesPerBuf) * 1'000'000'000LL / kSampleRate;

    while (mUdpRunning.load(std::memory_order_acquire)) {
        pollMidi();
        advanceLoop(kFramesPerBuf);

        for (float& s : buf) s = 0.0f;
        render(buf, kFramesPerBuf);

        bool hasData = false;
        for (float s : buf) { if (s != 0.0f) { hasData = true; break; } }

        if (hasData) {
            uint8_t opusBuf[512];
            opus_int32 nBytes = opus_encode_float(mOpusEncoder, buf, kFramesPerBuf,
                                                  opusBuf, sizeof(opusBuf));
            if (nBytes > 0)
                sendto(mUdpSocket, opusBuf, nBytes, 0,
                       reinterpret_cast<struct sockaddr*>(&mUdpDest), sizeof(mUdpDest));
        }

        nextWake.tv_nsec += intervalNs;
        while (nextWake.tv_nsec >= 1'000'000'000L) {
            nextWake.tv_sec  += 1;
            nextWake.tv_nsec -= 1'000'000'000L;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nextWake, nullptr);
    }
}
