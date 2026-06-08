//
// Created by Egon on 7/6/26.
//

#ifndef BTMID_WAVETABLE_H
#define BTMID_WAVETABLE_H

#include <cstdint>
#include <cmath>
#include <atomic>
#include <mutex>

class WaveTable {

public:
    static float sin(float freq, float t);
    static float saw(float freq, float t);
    static float square(float freq, float t);

private:

    static void initSinTable();
};


#endif //BTMID_WAVETABLE_H
