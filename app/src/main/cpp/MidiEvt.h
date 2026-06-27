//
// Created by Egon on 27/6/26.
//

#ifndef BTMID_MIDIEVT_H
#define BTMID_MIDIEVT_H

#include<cstdint>

struct MidiEvt {
    uint8_t channel;
    uint8_t type;
    uint8_t data1;
    uint8_t data2;
};

#endif //BTMID_MIDIEVT_H
