// SPDX-License-Identifier: GPL-2.0
#include "channel-routing.h"
#include <cassert>
#include <iostream>
int main()
{
    using namespace lbm;
    // Music assigned to editing tracks must not survive a record exclusion.
    assert(routedMask({15,true,false},1,14,2)==1);
    // Adding a main game mix preserves its dedicated track and unrelated bits.
    assert(routedMask({40,true,true},1,14,2)==43);
    // Monitor exclusion removes current outputs without muting/deleting input.
    assert(routedMask({15,false,false},1,14,2)==0);
    // A broadcast-only capture must gain a distinct recording output.
    assert(distinctRecordingMask(1,1,0)==2);
    // Service archive output must never be appropriated for a new local mix.
    assert(distinctRecordingMask(1,1,2)==4);
    assert(distinctRecordingMask(7,1,2)==4);
    // Existing main and isolated mic/game recordings retain their selection.
    assert(distinctRecordingMask(14,1,0)==14);
    // All selected recording outputs are removed; unrelated routing survives.
    assert(routedMask({63,true,false},1,14,2)==49);
    std::cout<<"PASS: music exclusion across recordings, isolated tracks, monitor, stream/VOD collision, existing selection\n";
}
