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

#ifndef REPCL_H
#define REPCL_H

#include <vector>
#include <iostream>
#include <bitset>
#include <sstream>
#include <string>

/**
 * \file
 * \ingroup ReplayClock
 * Declaration of classes ns3::ReplayClock.
 */


namespace ns3
{

class ReplayClock
{

public:

    /**
     *  Assignment operator
     * \param [in] o ReplayClock to assign.
     * \return The ReplayClock.
     */
    inline ReplayClock &operator=(const ReplayClock &o)
    {
        if (this != &o)
        {
            nodeId          = o.nodeId;
            hlc             = o.hlc;
            offset_bitmap   = o.offset_bitmap;
            counters        = o.counters;
            offsets         = o.offsets;
            epsilon         = o.epsilon;
            interval        = o.interval;
        }
        return *this;
    }


    /** Default constructor, with value 0. */
    inline ReplayClock() : hlc(), offset_bitmap(), offsets(), counters(), nodeId(), epsilon(), interval()
    {
        // Set offset of node to 0
        offset_bitmap[nodeId] = 0;
    } 


    /** Specification constructor. */
    inline ReplayClock(int hlc, int nodeId, int epsilon, int interval) : hlc(hlc), nodeId(nodeId), epsilon(epsilon), interval(interval) 
    {
        offset_bitmap   = 0;
        offsets         = 0;
        counters        = 0;

        // Set offset of node to 0
        offset_bitmap[nodeId] = 0;
    }


    /** Constructor from string representation */
    inline ReplayClock(std::string replayClockString, int nodeId, int epsilon, int interval)
    {
        nodeId      = nodeId;
        epsilon     = epsilon;
        interval    = interval;

        std::istringstream ss(replayClockString);
        ss >> hlc >> offset_bitmap >> offsets >> counters;

    }


    /**
     *  Copy constructor
     *
     * \param [in] o ReplayClock to copy
     */
    inline ReplayClock(const ReplayClock &o)
    {
        nodeId          = o.nodeId;
        hlc             = o.hlc;
        offset_bitmap   = o.offset_bitmap;
        counters        = o.counters;
        offsets         = o.offsets;
        epsilon         = o.epsilon;
        interval        = o.interval;
    }


    /**
     * EqualOffset operation to check whether the two clocks are equal.
     * \param [in] o ReplayClock to check current clock with
    */
    bool IsEqual(ReplayClock o);


    /** Destructor */
    ~ReplayClock() {}    

    
    inline int GetNodeId() const
    {
        return nodeId;
    }


    inline int GetHLC() const
    {
        return hlc;
    }


    inline std::bitset<64> GetBitmap() const 
    {
        return offset_bitmap;
    }


    inline std::bitset<64*64> GetOffsets() const
    {
        return offsets;
    }


    /**
     * GetOffsetAtIndex operation to get only the offset of one index..
     * \param [in] index The index of the offset to get
    */
    int GetOffsetAtIndex(int index);


    inline int GetCounters() const
    {
        return counters;
    }


    inline int GetOffsetSize();


    inline int GetCounterSize();


    inline int GetClockSize();


    static void SetNodeId(int nodeId) 
    {
        nodeId = nodeId;
    }


    static void SetHLC(int hlc)
    {
        hlc = hlc;
    }


    static void SetOffsets(std::bitset<64*64> offsets)
    {
        offsets = offsets;
    }


    /**
     * SetOffsetAtIndex operation to set only the offset of one index.
     * \param [in] index The index of the offset to set
     * \param [in] newOffset The new offset to set the index to
    */
    void SetOffsetAtIndex(int index, int newOffset);


    static void SetCounters(int counters)
    {
        counters = counters;
    }


    static void SetOffsetBitmap(std::bitset<64> offset_bitmap)
    {
        offset_bitmap = offset_bitmap;
    }


    /**@}*/ // Create ReplayClock from Values and Units

    /**
     * \name Get ReplayClock as Strings
     * Get the ReplayClock as Strings.
     *
     * @{
     */
    /**
     * Get the ReplayClock value expressed in a string.
     *
     * \return The ReplayClock expressed in a string
     */
    inline std::string ToString(std::ostringstream ss) const
    {
        ss << hlc << " " << offset_bitmap << " " << offsets << " " << counters;

        return ss.str();
    }


    /**
     * Send/Local operation to advance the ReplayClock.
     * \param [in] node_hlc The local node clock
    */
    void SendLocal(int node_hlc);


    /**
     * Recv operation to process incoming clocks to the ReplayClock.
     * \param [in] m_ReplayClock The event node clock
     * \param [in] node_hlc The local node clock
    */
    void Recv(ReplayClock m_ReplayClock, int node_hlc);


    /**
     * Shift operation to shift the ReplayClock by the local node clock update.
     * \param [in] node_hlc The local node clock
    */
    void Shift(int node_hlc);


    /**
     * MergeSameEpoch operation to shift the ReplayClock by the incoming message's node clock offset.
     * \param [in] m_ReplayClock The event node clock
    */
    void MergeSameEpoch(ReplayClock m_ReplayClock);


    /**
     * EqualOffset operation to check whether the offsets of two clocks are equal.
     * \param [in] o ReplayClock to check current clock with
    */
    bool EqualOffset(ReplayClock o) const;


    /**
     * RemoveOffsetAtIndex operation to remove an offset at an index.
     * \param [in] index Offset to remove by index
    */
    void RemoveOffsetAtIndex(int index);

    /**
     * PrintClock operation to print the clock.
     * \param [in] os OutputStream to print the clock to.
    */
    void PrintClock(std::ostream& os);


private:
    int                 hlc;            //!< Hybrid Logical Clock for topmost level
    std::bitset<64>     offset_bitmap;  //!< Offset bitmap that stores true for nodes whose offsets are being tracked
    std::bitset<64*64>  offsets;        //!< Value of offsets for the nodes where bitmap denotes true
    int                 counters;       //!< Counters for events occuring within an interval
    int                 nodeId;         //!< NodeID of the current node
    int                 epsilon;        //!< Maximum acceptable clock skew
    int                 interval;       //!< Discretizing the standard flow of time to save space

};

}   // namespace ns3

#endif /* REPCL_H */