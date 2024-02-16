/*
 * Copyright (c) 2009 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "replay-header.h"

#include "ns3/assert.h"
#include "ns3/header.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ReplayHeader");

NS_OBJECT_ENSURE_REGISTERED(ReplayHeader);

ReplayHeader::ReplayHeader()
{
    NS_LOG_FUNCTION(this);
}

ReplayHeader::ReplayHeader(ReplayClock rc)
{
    NS_LOG_FUNCTION(this);
    m_rc = rc;
}

ReplayClock
ReplayHeader::GetReplayClock() const
{
    NS_LOG_FUNCTION(this);
    return m_rc;
}

TypeId
ReplayHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ReplayHeader")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<ReplayHeader>();
    return tid;
}

TypeId
ReplayHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
ReplayHeader::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    std::cout  << "(hlc=" << m_rc.GetHLC() 
        << " nodeId=" << m_rc.GetNodeId() 
        << " offset_bitmap=" << m_rc.GetBitmap() 
        << " offsets=" << m_rc.GetOffsets() 
        << " counters=" << m_rc.GetCounters() 
        << ")"
        << std::endl;
}

uint32_t
ReplayHeader::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 64;
}

void
ReplayHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    i.WriteHtonU32(m_rc.GetHLC());
    i.WriteHtonU32(m_rc.GetNodeId());
    i.WriteHtonU64(m_rc.GetBitmap().to_ullong());
    i.WriteHtonU64(m_rc.GetOffsets().to_ullong());
    i.WriteHtonU32(m_rc.GetCounters());
}

uint32_t
ReplayHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    m_rc.SetHLC(i.ReadNtohU32());
    m_rc.SetNodeId(i.ReadNtohU32());
    m_rc.SetOffsetBitmap(i.ReadNtohU64());
    m_rc.SetOffsets(i.ReadNtohU64());
    m_rc.SetCounters(i.ReadNtohU32());
    return GetSerializedSize();
}

} // namespace ns3
