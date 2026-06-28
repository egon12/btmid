#pragma once

#include "opus.h"
#include <atomic>
#include <memory>
#include <thread>
#include <string>
#include <netinet/in.h>

class MidiEngine;

class WifiOutput {
public:
    WifiOutput(MidiEngine *engine, std::string host, int port);

    ~WifiOutput();

    WifiOutput &operator=(const WifiOutput &) = delete;

    void start();

    void stop();

private:
    void udpRenderLoop();

    MidiEngine *mEngine;
    std::string mHost;
    int mPort;

    int mUdpSocket{-1};
    struct sockaddr_in mUdpDest{};
    std::thread mUdpThread;
    std::atomic<bool> mUdpRunning{false};
    OpusEncoder *mOpusEncoder{nullptr};
};