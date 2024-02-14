/*
 * Copyright (c) 2007 INRIA, Gustavo Carneiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * Authors: Ishaan Lagwankar (lagwanka@msu.edu)
 *          
 */

#include "replay-clock.h"
#include "log.h"

#include <vector>
#include <iostream>
#include <math.h>

/**
 * \file
 * \ingroup ReplayClock
 * ns3::ReplayClock implementations.
 */

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ReplayClock");

uint32_t 
ReplayClock::GetOffsetSize()
{
    return (offset_bitmap.count()*32)/8;
}

uint32_t 
ReplayClock::GetCounterSize()
{
    return sizeof(counters);
}

uint32_t 
ReplayClock::GetClockSize()
{
    return GetOffsetSize() + GetCounterSize() + sizeof(hlc);
}

void 
ReplayClock::SendLocal(uint32_t node_hlc)
{

    uint32_t new_hlc = std::max(hlc, node_hlc);
    uint32_t new_offset = new_hlc - node_hlc;

    uint32_t offset_at_pid = GetOffsetAtIndex(nodeId);

    // Make sure this case does not happen
    if (new_hlc == hlc && offset_at_pid <= new_offset)
    {
        counters++;
    }
    else if(new_hlc == hlc)
    {
        new_offset = std::min(new_offset, offset_at_pid);
        SetOffsetAtIndex(nodeId, new_offset);
        counters = 0;
    }
    else
    {    
        counters = 0;
        Shift(new_hlc);
        SetOffsetAtIndex(nodeId, 0);
    }
}

void 
ReplayClock::Recv(ReplayClock m_ReplayClock, uint32_t node_hlc)
{
    uint32_t new_hlc = std::max(hlc, m_ReplayClock.hlc);
    new_hlc = std::max(new_hlc, node_hlc);

    ReplayClock a = *this;
    ReplayClock b = m_ReplayClock;

    a.Shift(new_hlc);

    b.Shift(new_hlc);

    a.MergeSameEpoch(b);

    if(EqualOffset(a) && m_ReplayClock.EqualOffset(a))
    {
        a.counters = std::max(a.counters, m_ReplayClock.counters);
        a.counters++;
    }
    else if(EqualOffset(a) && !m_ReplayClock.EqualOffset(a))
    {
        a.counters++;
    }
    else if(!EqualOffset(a) && m_ReplayClock.EqualOffset(a))
    {
        a.counters = m_ReplayClock.counters;
        a.counters++;
    }
    else
    {
        a.counters = 0;
    }

    *this = a;
}

void 
ReplayClock::Shift(uint32_t new_hlc)
{
    uint32_t bitmap = offset_bitmap.to_ulong();
    uint32_t index = 0;
    while(bitmap > 0)
    {

        uint32_t offset_at_index = GetOffsetAtIndex(index);

        uint32_t pos = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);

        uint32_t new_offset = std::min(new_hlc - (hlc - offset_at_index), epsilon);

        if(new_offset >= epsilon)
        {    
            offset_bitmap[pos] = 0;
            RemoveOffsetAtIndex(index);
        }
        else
        {
            SetOffsetAtIndex(index, new_offset);
        }

        bitmap = bitmap & (bitmap - 1);

        index++;
    }
    hlc = new_hlc;
}

void 
ReplayClock::MergeSameEpoch(ReplayClock m_ReplayClock)
{
    
    offset_bitmap = offset_bitmap | m_ReplayClock.offset_bitmap;

    uint32_t bitmap = offset_bitmap.to_ulong();
    uint32_t index = 0;

    while(bitmap > 0)
    {
        uint32_t pos = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);
        
        uint32_t new_offset = std::min(GetOffsetAtIndex(pos), m_ReplayClock.GetOffsetAtIndex(pos));
        SetOffsetAtIndex(index, new_offset);

        if(GetOffsetAtIndex(pos) >= epsilon)
        {
            offset_bitmap[pos] = 0;
            RemoveOffsetAtIndex(index); 
        }
        bitmap = bitmap & (bitmap - 1);

        index++;
    }
}


bool 
ReplayClock::EqualOffset(ReplayClock a) const
{
    if(a.hlc != hlc || a.offset_bitmap != offset_bitmap || a.offsets != offsets)
    {
        return false;
    }
    return true;
}

// Helper Function to extract k bits from p position
// and returns the extracted value as integer
uint32_t 
extract(int number, int k, int p)
{
    return (((1 << k) - 1) & (number >> (p)));
}

uint32_t 
ReplayClock::GetOffsetAtIndex(uint32_t index)
{
    
    // cout << endl << "Extract " << 8 << " bits from position " << 4*index << endl;

    std::bitset<32> offset(extract(offsets.to_ulong(), 8, 4*index));
    
    // cout << "Offset: " << offset << endl;

    return offset.to_ulong();

}

void 
ReplayClock::SetOffsetAtIndex(uint32_t index, uint8_t new_offset)
{
    // Convert new offset to bitset and add bitset at appropriate position
    
    std::bitset<32> offset(new_offset);

    std::bitset<32> res(extract(offsets.to_ulong(), 4*index, 0));

    res |= offset.to_ulong() << index*4;

    std::bitset<32> lastpart(extract(offsets.to_ulong(), 32 - (4*(index + 1)), 4*(index + 1)));

    res |= lastpart << ((index+1)*4);

    offsets = res;

}

void 
ReplayClock::RemoveOffsetAtIndex(uint32_t index)
{
    // Remove and squash the bitset of given index through index + 4
    std::bitset<32> res(extract(offsets.to_ulong(), 4*index, 0));
    
    std::bitset<32> lastpart(extract(offsets.to_ulong(), 32 - (4*(index + 1)), 4*(index + 1)));

    res |= lastpart << ((index+1)*4);

    offsets = res;

}

bool 
ReplayClock::IsEqual(ReplayClock f)
{
    if(hlc != f.GetHLC() || offset_bitmap != f.GetBitmap() || offsets != f.GetOffsets() || counters != f.GetCounters())
    {
        return false;
    }
    return true;
}

void 
ReplayClock::Serialize(uint8_t* buffer)
{
    
    uint32_t integers[4] = {
        hlc, 
        static_cast<uint32_t>(offset_bitmap.to_ulong()),
        static_cast<uint32_t>(offsets.to_ulong()), 
        counters
    };

    // Convert each uint32_t integer into four uint8_t values and concatenate into a single array
    for (int i = 0; i < 4; ++i) {
        buffer[i * 16] = (integers[i] >> 24) & 0xFF;
        buffer[i * 16 + 1] = (integers[i] >> 16) & 0xFF;
        buffer[i * 16 + 2] = (integers[i] >> 8) & 0xFF;
        buffer[i * 16 + 3] = integers[i] & 0xFF;

        buffer[i * 16 + 4] = (integers[i] >> 24) & 0xFF;
        buffer[i * 16 + 5] = (integers[i] >> 16) & 0xFF;
        buffer[i * 16 + 6] = (integers[i] >> 8) & 0xFF;
        buffer[i * 16 + 7] = integers[i] & 0xFF;

        buffer[i * 16 + 8] = (integers[i] >> 24) & 0xFF;
        buffer[i * 16 + 9] = (integers[i] >> 16) & 0xFF;
        buffer[i * 16 + 10] = (integers[i] >> 8) & 0xFF;
        buffer[i * 16 + 11] = integers[i] & 0xFF;

        buffer[i * 16 + 12] = (integers[i] >> 24) & 0xFF;
        buffer[i * 16 + 13] = (integers[i] >> 16) & 0xFF;
        buffer[i * 16 + 14] = (integers[i] >> 8) & 0xFF;
        buffer[i * 16 + 15] = integers[i] & 0xFF;
    }

}

void
ReplayClock::Deserialize(uint8_t* buffer, uint32_t* integers)
{

    // Convert each 4 uint8_t values into a uint32_t integer
    for (int i = 0; i < 4; ++i) {
        integers[i] = (static_cast<uint32_t>(buffer[i * 16]) << 24) |
                      (static_cast<uint32_t>(buffer[i * 16 + 1]) << 16) |
                      (static_cast<uint32_t>(buffer[i * 16 + 2]) << 8) |
                      static_cast<uint32_t>(buffer[i * 16 + 3]);
    }

}

}   // namespace ns3

/**@}*/ // \ingroup ReplayClock