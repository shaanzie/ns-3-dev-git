/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */
#include "replay-client.h"

#include "replay-header.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"

#include <cstdio>
#include <cstdlib>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ReplayClient");

NS_OBJECT_ENSURE_REGISTERED(ReplayClient);

void 
ReplayClient::PrintClock(ReplayClock rc)
{
    NS_LOG_INFO("-----------------------------------------------------------");
    NS_LOG_INFO("HLC: " << rc.GetHLC());
    NS_LOG_INFO("Bitmap: " << rc.GetBitmap());
    NS_LOG_INFO("Offsets: " << rc.GetOffsets());
    NS_LOG_INFO("Counters: " << rc.GetCounters());
}

TypeId
ReplayClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ReplayClient")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<ReplayClient>()
            .AddAttribute(
                "MaxPackets",
                "The maximum number of packets the application will send (zero means infinite)",
                UintegerValue(100),
                MakeUintegerAccessor(&ReplayClient::m_count),
                MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval",
                          "The time to wait between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&ReplayClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&ReplayClient::m_peerAddress),
                          MakeAddressChecker())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(100),
                          MakeUintegerAccessor(&ReplayClient::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketSize",
                          "Size of packets generated. The minimum packet size is 12 bytes which is "
                          "the size of the header carrying the sequence number and the time stamp.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&ReplayClient::m_size),
                          MakeUintegerChecker<uint32_t>(12, 65507));
    return tid;
}

ReplayClient::ReplayClient()
{
    NS_LOG_FUNCTION(this);
    m_sent = 0;
    m_totalTx = 0;
    m_socket = nullptr;
    m_sendEvent = EventId();
}

ReplayClient::~ReplayClient()
{
    NS_LOG_FUNCTION(this);
}

void
ReplayClient::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddress = ip;
    m_peerPort = port;
}

void
ReplayClient::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddress = addr;
}

void
ReplayClient::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

void
ReplayClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        if (Ipv4Address::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (Ipv6Address::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
        }
    }

#ifdef NS3_LOG_ENABLE
    std::stringstream peerAddressStringStream;
    if (Ipv4Address::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Ipv4Address::ConvertFrom(m_peerAddress);
    }
    else if (Ipv6Address::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Ipv6Address::ConvertFrom(m_peerAddress);
    }
    else if (InetSocketAddress::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4();
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetIpv6();
    }
    m_peerAddressString = peerAddressStringStream.str();
#endif // NS3_LOG_ENABLE

    m_socket->SetRecvCallback(MakeCallback(&ReplayClient::HandleRead, this));
    m_socket->SetAllowBroadcast(true);
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &ReplayClient::Send, this);
}

void
ReplayClient::StopApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(m_sendEvent);
}

void
ReplayClient::Send()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_sendEvent.IsExpired());
    
    Address from;
    Address to;
    m_socket->GetSockName(from);
    m_socket->GetPeerName(to);

    ReplayHeader repheader = CreateReplayHeader();

    NS_ABORT_IF(m_size < repheader.GetSerializedSize());
    Ptr<Packet> p = Create<Packet>(m_size - repheader.GetSerializedSize());

    p->AddHeader(repheader);

    if ((m_socket->Send(p)) >= 0)
    {
        ++m_sent;
        m_totalTx += p->GetSize();
        
        NS_LOG_INFO("TraceDelay TX " << p->GetSize() << " bytes to " << m_peerAddress << " Uid: "
                                     << p->GetUid() << " from " 
                                     << GetNode()->GetId() << " at Time: " << (Simulator::Now()).As(Time::S));
                                     
    }
    
    else
    {
        NS_LOG_INFO("Error while sending " << m_size << " bytes to " << m_peerAddress);
    }
    

    if (m_sent < m_count || m_count == 0)
    {
        m_sendEvent = Simulator::Schedule(m_interval, &ReplayClient::Send, this);
    }

}

void
ReplayClient::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {

        socket->GetSockName(localAddress);

        if (packet->GetSize() > 0)
        {
            uint32_t receivedSize = packet->GetSize();
                NS_LOG_INFO("TraceDelay: RX " << receivedSize << " bytes from "
                                              << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                              << " Uid: " << packet->GetUid() << " RXtime: " << Simulator::Now());

            ProcessPacket(packet);

            ReplayHeader repheader = CreateReplayHeader();

            Ptr<Packet> p = Create<Packet>(m_size - repheader.GetSerializedSize());

            p->AddHeader(repheader);

            if ((socket->SendTo(p, 0, from)) >= 0)
            {
                
                NS_LOG_INFO("TraceDelay TX " << p->GetSize() << " bytes to " << from << " Uid: "
                                            << p->GetUid() << " from node " << GetNode()->GetId() 
                                            << " at Time: " << (Simulator::Now()).As(Time::S));
                                            
            }
            
            else
            {
                NS_LOG_INFO("Error while sending " << m_size << " bytes to " << from);
            }

        }
    }
}

ReplayHeader
ReplayClient::CreateReplayHeader()
{

    Ptr<Node> client = GetNode();

    ReplayClock client_rc = client->GetReplayClock();

    client_rc.SendLocal(GetNode()->GetNodeLocalClock());
    
    client->SetReplayClock(client_rc);

    ReplayHeader repheader(client_rc);

    // repheader.Print(std::cout);

    return repheader;

}

void
ReplayClient::ProcessPacket(Ptr<Packet> packet)
{

    Ptr<Node> server = GetNode();
    ReplayClock server_rc = server->GetReplayClock();

    ReplayHeader repheader;
    packet->RemoveHeader(repheader);

    // repheader.Print(std::cout);

    ReplayClock m_rc(
        repheader.GetReplayClock().GetHLC(),
        repheader.GetReplayClock().GetNodeId(),
        repheader.GetReplayClock().GetBitmap(),
        repheader.GetReplayClock().GetOffsets(),
        repheader.GetReplayClock().GetCounters(),
        EPSILON,
        INTERVAL
    );

    server_rc.Recv(m_rc, GetNode()->GetNodeLocalClock());

    server->SetReplayClock(server_rc);
}

uint64_t
ReplayClient::GetTotalTx() const
{
    return m_totalTx;
}



} // Namespace ns3
