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
#include <algorithm>
#include <unistd.h>

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
    return ((offset_bitmap.count() * MAX_OFFSET_SIZE) + 1) / 8;
}

uint32_t 
ReplayClock::GetMaxOffset()
{   
    uint32_t max_offset = 0;
    
    int index = 0;
    uint32_t bitmap = offset_bitmap.to_ulong();
    while(bitmap > 0)
    {

        uint32_t process_id = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);

        max_offset = std::max(max_offset, GetOffsetAtIndex(index));

        bitmap = bitmap & (bitmap - 1);
        index++;
    }

    return max_offset;
}

uint32_t 
ReplayClock::GetCounterSize()
{
    return (log2(counters) + 1) / 8;
}

uint32_t 
ReplayClock::GetClockSize()
{
    return GetOffsetSize() + GetCounterSize() + ((log2(hlc) + 1) / 8);
}

void 
ReplayClock::SendLocal(uint32_t node_hlc)
{

    // std::cout << "--------------------------SEND--------------------------" << std::endl;
    // PrintClock();

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
        
        int index = 0;
        uint32_t bitmap = offset_bitmap.to_ulong();
        while(bitmap > 0)
        {

            uint32_t process_id = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);
            if(process_id == nodeId)
            {
                SetOffsetAtIndex(index, new_offset);
                offset_bitmap[process_id] = 1;
            }

            bitmap = bitmap & (bitmap - 1);
            index++;
        }

        counters = 0;
        offset_bitmap[nodeId] = 1;
    }
    else
    {    
        counters = 0;
        Shift(new_hlc);
        
        int index = 0;
        uint32_t bitmap = offset_bitmap.to_ulong();
        while(bitmap > 0)
        {

            uint32_t process_id = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);
            if(process_id == nodeId)
            {
                SetOffsetAtIndex(index, 0);
                offset_bitmap[process_id] = 1;
            }

            bitmap = bitmap & (bitmap - 1);
            index++;
        }

        offset_bitmap[nodeId] = 1;
    }

    // std::cout << "--------------------------SEND DONE!--------------------------" << std::endl;
    // PrintClock();
    // std::cout << "==============================================================" << std::endl;
    // sleep(2);
}

void 
ReplayClock::Recv(ReplayClock m_ReplayClock, uint32_t node_hlc)
{

    // std::cout << "--------------------------RECV--------------------------" << std::endl;

    // std::cout << "--------------------------NODE CLOCK--------------------------" << std::endl;

    // PrintClock();

    // std::cout << "--------------------------MESSAGE CLOCK--------------------------" << std::endl;

    // m_ReplayClock.PrintClock();

    uint32_t new_hlc = std::max(hlc, m_ReplayClock.hlc);
    new_hlc = std::max(new_hlc, node_hlc);

    ReplayClock a = *this;
    ReplayClock b = m_ReplayClock;

    a.Shift(new_hlc);

    // std::cout << "--------------------------A SHIFTED CLOCK--------------------------" << std::endl;

    // a.PrintClock();

    b.Shift(new_hlc);

    // std::cout << "--------------------------B SHIFTED CLOCK--------------------------" << std::endl;

    // b.PrintClock();

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

    offset_bitmap[nodeId] = 1;

//     std::cout << "--------------------------FINAL CLOCK--------------------------" << std::endl;

//     PrintClock();

//     std::cout << "--------------------------RECV DONE!--------------------------" << std::endl;

//     sleep(2);
}

void 
ReplayClock::Shift(uint32_t new_hlc)
{

    int index = 0;

    // std::cout << "Shifting from " << hlc << " to " << new_hlc << std::endl;

    uint32_t bitmap = offset_bitmap.to_ulong();
    while(bitmap > 0)
    {

        uint32_t process_id = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);
        
        uint32_t offset_at_index = GetOffsetAtIndex(index);

        uint32_t new_offset = std::min(new_hlc - (hlc - offset_at_index), epsilon);

        // std::cout   << "Old offset for process ID: " << process_id << " and index " << index <<  ": " 
        //             << offset_at_index << ", new offset: " << new_offset << std::endl;
        // std::cout << "Epsilon: " << epsilon << std::endl;

        if(new_offset >= epsilon)
        {    
            RemoveOffsetAtIndex(index);
            offset_bitmap[process_id] = 0;
        }
        else
        {
            SetOffsetAtIndex(index, new_offset);
            offset_bitmap[process_id] = 1;
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
        
        uint32_t new_offset = std::min(GetOffsetAtIndex(index), m_ReplayClock.GetOffsetAtIndex(index));

        if(new_offset >= epsilon)
        {
            RemoveOffsetAtIndex(index); 
            offset_bitmap[pos] = 0;
        }
        else
        {
            SetOffsetAtIndex(index, new_offset);
            offset_bitmap[pos] = 1;
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

    std::bitset<MAX_OFFSET_SIZE> offset(extract(offsets.to_ulong(), MAX_OFFSET_SIZE, MAX_OFFSET_SIZE * index));
    
    // cout << "Offset: " << offset << endl;

    return offset.to_ulong();

}

void 
ReplayClock::SetOffsetAtIndex(uint32_t index, uint32_t new_offset)
{
    // Convert new offset to bitset and add bitset at appropriate position

    // std::cout << "To insert " << new_offset << " at index " << index << std::endl;

    // std::cout << "Original bitmap: " << offset_bitmap << std::endl;
    
    std::bitset<MAX_OFFSET_SIZE> offset(new_offset);

    // std::cout << "New offset: " << offset << std::endl;

    std::bitset<64> res(extract(offsets.to_ulong(), MAX_OFFSET_SIZE*index, 0));

    // std::cout << "First part: " << res << std::endl;

    res |= offset.to_ulong() << index*MAX_OFFSET_SIZE;

    // std::cout << "Inserted offset: " << res << std::endl;

    std::bitset<64> lastpart(extract(offsets.to_ulong(), 
                                                    64 - (MAX_OFFSET_SIZE*(index + 1)), 
                                                    MAX_OFFSET_SIZE*(index + 1)));

    // std::cout << "Last part: " << lastpart << std::endl;

    res |= lastpart << ((index+1)*MAX_OFFSET_SIZE);

    // std::cout << "Final offsets: " << res << std::endl;

    offsets = res;

}

void 
ReplayClock::RemoveOffsetAtIndex(uint32_t index)
{
    // Remove and squash the bitset of given index through index + 4
    std::bitset<64> res(extract(offsets.to_ulong(), MAX_OFFSET_SIZE*index, 0));
    
    std::bitset<64> lastpart(extract(offsets.to_ulong(),
                                                        64 - (MAX_OFFSET_SIZE*(index + 1)),
                                                        MAX_OFFSET_SIZE*(index + 1)));

    res |= lastpart << ((index+1)*MAX_OFFSET_SIZE);

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

    // std::cout << "Serialized: " << std::endl;
    // std::cout << "HLC: " << integers[0] << std::endl;
    // std::cout << "Bitmap: " << integers[1] << std::endl;
    // std::cout << "Offsets: " << integers[2] << std::endl;
    // std::cout << "Counters: " << integers[3] << std::endl;

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

    // std::cout << "Deserialized: " << std::endl;
    // std::cout << "HLC: " << integers[0] << std::endl;
    // std::cout << "Bitmap: " << integers[1] << std::endl;
    // std::cout << "Offsets: " << integers[2] << std::endl;
    // std::cout << "Counters: " << integers[3] << std::endl;

}

void 
ReplayClock::PrintClock()
{
    std::cout << "NodeID: " << nodeId << std::endl;

    std::cout << "HLC: " << hlc << std::endl;

    uint32_t bitmap = offset_bitmap.to_ulong();
    int index = 0;
    while(bitmap > 0)   
    {

        uint32_t process_id = log2((~(bitmap ^ (~(bitmap - 1))) + 1) >> 1);

        std::cout << "Offset for process " << process_id << ": " << GetOffsetAtIndex(index) << std::endl;

        bitmap = bitmap & (bitmap - 1);
        index++;
    }

    std::cout << "Counters: " << counters << std::endl;


}

}   // namespace ns3

/**@}*/ // \ingroup ReplayClock