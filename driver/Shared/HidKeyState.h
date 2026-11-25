#pragma once
#include "InterceptorProtocol.h"

// Send modifier makes before ordinary makes and modifier breaks after ordinary
// breaks so a chord carried by one HID report retains its class-driver meaning.
static __inline unsigned IcHidCollectKeyChanges(const uint8_t previous[256],
    const uint8_t current[256],int pressed,uint16_t changed[256]) {
    unsigned count=0,pass,usage;
    for(pass=0;pass<2;++pass) {
        const int wantModifier=pressed?(pass==0):(pass==1);
        for(usage=4;usage<256;++usage) {
            const int modifier=usage>=0xe0 && usage<=0xe7;
            if(modifier==wantModifier && previous[usage]!=current[usage] &&
                (current[usage]!=0)==(pressed!=0))changed[count++]=(uint16_t)usage;
        }
    }
    return count;
}

// Merge one HID keyboard report into the union of all report IDs. A usage can
// be present in several IDs, so releasing it in one report must not release it
// globally while another still owns it. All arrays start at zero. The input
// bitmap contains only keyboard usages 4..255 (rollover is handled by caller).
// Returns whether the merged state changed, not merely the report's ownership.
static __inline int IcHidMergeKeyReport(uint8_t previous[32],const uint8_t report[32],
    uint16_t owners[256],uint8_t current[256]) {
    unsigned byte;int changed=0;
    for(byte=0;byte<32;++byte) {
        const unsigned delta=previous[byte]^report[byte];unsigned bit;
        if(!delta)continue;
        previous[byte]=report[byte];
        for(bit=0;bit<8;++bit)if(delta&(1u<<bit)) {
            const unsigned usage=byte*8+bit;
            const uint8_t before=current[usage];
            if(report[byte]&(1u<<bit))++owners[usage];else --owners[usage];
            current[usage]=(uint8_t)(owners[usage]!=0);
            changed|=(before!=current[usage]);
        }
    }
    return changed;
}
