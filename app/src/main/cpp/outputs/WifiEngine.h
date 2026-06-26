#pragma once
#include "opus.h"
#include <atomic>
#include <thread>
#include <string>
#include <netinet/in.h>
#include "../MidiEngine.h"

class WifiEngine : public MidiEngine {
public:
    WifiEngine(std::string host, int port);
    ~WifiEngine() override;

    WifiEngine(const WifiEngine&) = delete;
    WifiEngine& operator=(const WifiEngine&) = delete;

    void start() override;
    void stop() override;

private:
    void udpRenderLoop();

    std::string            mHost;
    int                    mPort;

    int                    mUdpSocket   {-1};
    struct sockaddr_in     mUdpDest     {};
    std::thread            mUdpThread;
    std::atomic<bool>      mUdpRunning  {false};
    OpusEncoder*           mOpusEncoder {nullptr};
};
