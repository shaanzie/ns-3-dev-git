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

#include "seq-ts-header.h"

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

    uint8_t *repcl_buf = new uint8_t[64];

    uint32_t size = CreateReplayPacket(repcl_buf);

    Ptr<Packet> p = Create<Packet>(repcl_buf, size);

    if ((m_socket->Send(p)) >= 0)
    {
        ++m_sent;
        m_totalTx += p->GetSize();
        
        NS_LOG_INFO("TraceDelay TX " << p->GetSize() << " bytes to " << m_peerAddress << " Uid: "
                                     << p->GetUid() << " Time: " << (Simulator::Now()).As(Time::S));
                                     
    }
    
    else
    {
        NS_LOG_INFO("Error while sending " << m_size << " bytes to " << m_peerAddress);
    }
    

    if (m_sent < m_count || m_count == 0)
    {
        m_sendEvent = Simulator::Schedule(m_interval, &ReplayClient::Send, this);
    }

    delete[] repcl_buf;

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
        }
        
    }
}

uint32_t
ReplayClient::CreateReplayPacket(uint8_t* buffer)
{

    Ptr<Node> client = GetNode();
    ReplayClock client_rc = client->GetReplayClock();

#ifdef REPCL_CONFIG_H
    client_rc.SendLocal(GetNode()->GetNodeLocalClock() / INTERVAL);
#else
    client_rc.SendLocal(GetNode()->GetNodeLocalClock() / 10);
#endif
    
    client->SetReplayClock(client_rc);

    client_rc.Serialize(buffer);

    return client_rc.GetClockSize();

}

void
ReplayClient::ProcessPacket(Ptr<Packet> packet)
{

    uint8_t* buffer = new uint8_t[64];

    packet->CopyData(buffer, 512);

    Ptr<Node> server = GetNode();
    ReplayClock server_rc = server->GetReplayClock();

    uint32_t* integers = new uint32_t[4];

    server_rc.Deserialize(buffer, integers);

#ifdef REPCL_CONFIG_H
    ReplayClock m_rc(integers[0], server->GetId(), integers[1], integers[2], integers[3], EPSILON, INTERVAL);
#else
    ReplayClock m_rc(integers[0], server->GetId(), integers[1], integers[2], integers[3], 20, 10);
#endif

#ifdef REPCL_CONFIG_H
    server_rc.Recv(m_rc, GetNode()->GetNodeLocalClock() / INTERVAL);
#else
    server_rc.Recv(m_rc, GetNode()->GetNodeLocalClock() / 10);
#endif

    server->SetReplayClock(server_rc);

    delete[] buffer;
    delete[] integers;

}

uint64_t
ReplayClient::GetTotalTx() const
{
    return m_totalTx;
}

} // Namespace ns3
