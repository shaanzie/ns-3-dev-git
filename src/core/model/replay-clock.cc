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

int 
ReplayClock::GetOffsetSize()
{
    return (offset_bitmap.count()*64)/8;
}

int 
ReplayClock::GetCounterSize()
{
    return sizeof(counters);
}

int 
ReplayClock::GetClockSize()
{
    return GetOffsetSize() + GetCounterSize() + sizeof(hlc);
}

void 
ReplayClock::SendLocal(int node_hlc)
{

    int new_hlc = std::max(hlc, node_hlc);
    int new_offset = new_hlc - node_hlc;

    int offset_at_pid = GetOffsetAtIndex(nodeId);

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
ReplayClock::Recv(ReplayClock m_ReplayClock, int node_hlc)
{
    int new_hlc = std::max(hlc, m_ReplayClock.hlc);
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
ReplayClock::Shift(int new_hlc)
{
    int bitmap = offset_bitmap.to_ulong();
    int index = 0;
    while(bitmap > 0)
    {

        int offset_at_index = GetOffsetAtIndex(index);

        int pos = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);

        int new_offset = std::min(new_hlc - (hlc - offset_at_index), epsilon);

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

    int bitmap = offset_bitmap.to_ulong();
    int index = 0;

    while(bitmap > 0)
    {
        int pos = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);
        
        int new_offset = std::min(GetOffsetAtIndex(pos), m_ReplayClock.GetOffsetAtIndex(pos));
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
int 
extract(int number, int k, int p)
{
    return (((1 << k) - 1) & (number >> (p)));
}

int 
ReplayClock::GetOffsetAtIndex(int index)
{
    
    // cout << endl << "Extract " << 64 << " bits from position " << 64*index << endl;

    std::bitset<64> offset(extract(offsets.to_ulong(), 64, 64*index));
    
    // cout << "Offset: " << offset << endl;

    return offset.to_ulong();

}

void 
ReplayClock::SetOffsetAtIndex(int index, int new_offset)
{
    // Convert new offset to bitset and add bitset at appropriate position
    
    std::bitset<64> offset(new_offset);

    std::bitset<64*64> res(extract(offsets.to_ulong(), 64*index, 0));

    res |= offset.to_ulong() << index*64;

    std::bitset<64*64> lastpart(extract(offsets.to_ulong(), 64*64 - (64*(index + 1)), 64*(index + 1)));

    res |= lastpart << ((index+1)*64);

    offsets = res;

}

void 
ReplayClock::RemoveOffsetAtIndex(int index)
{
    // Remove and squash the bitset of given index through index + 4
    std::bitset<64*64> res(extract(offsets.to_ulong(), 64*index, 0));
    
    std::bitset<64*64> lastpart(extract(offsets.to_ulong(), 64*64 - (64*(index + 1)), 64*(index + 1)));

    res |= lastpart << ((index+1)*64);

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

}   // namespace ns3

/**@}*/ // \ingroup ReplayClock