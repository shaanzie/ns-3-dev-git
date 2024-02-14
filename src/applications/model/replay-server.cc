/*
 *  Copyright (c) 2007,2008,2009 INRIA, UDCAST
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

#include "replay-server.h"

#include "packet-loss-counter.h"
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

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ReplayServer");

NS_OBJECT_ENSURE_REGISTERED(ReplayServer);

TypeId
ReplayServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ReplayServer")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<ReplayServer>()
            .AddAttribute("Port",
                          "Port on which we listen for incoming packets.",
                          UintegerValue(100),
                          MakeUintegerAccessor(&ReplayServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketWindowSize",
                          "The size of the window used to compute the packet loss. This value "
                          "should be a multiple of 8.",
                          UintegerValue(32),
                          MakeUintegerAccessor(&ReplayServer::GetPacketWindowSize,
                                               &ReplayServer::SetPacketWindowSize),
                          MakeUintegerChecker<uint16_t>(8, 256))
            .AddTraceSource("Rx",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&ReplayServer::m_rxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("RxWithAddresses",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&ReplayServer::m_rxTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback");
    return tid;
}

ReplayServer::ReplayServer()
    : m_lossCounter(0)
{
    NS_LOG_FUNCTION(this);
    m_received = 0;
}

ReplayServer::~ReplayServer()
{
    NS_LOG_FUNCTION(this);
}

uint16_t
ReplayServer::GetPacketWindowSize() const
{
    NS_LOG_FUNCTION(this);
    return m_lossCounter.GetBitMapSize();
}

void
ReplayServer::SetPacketWindowSize(uint16_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_lossCounter.SetBitMapSize(size);
}

uint32_t
ReplayServer::GetLost() const
{
    NS_LOG_FUNCTION(this);
    return m_lossCounter.GetLost();
}

uint64_t
ReplayServer::GetReceived() const
{
    NS_LOG_FUNCTION(this);
    return m_received;
}

void
ReplayServer::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

void
ReplayServer::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
    }

    m_socket->SetRecvCallback(MakeCallback(&ReplayServer::HandleRead, this));

    if (!m_socket6)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket6 = Socket::CreateSocket(GetNode(), tid);
        Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(), m_port);
        if (m_socket6->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
    }

    m_socket6->SetRecvCallback(MakeCallback(&ReplayServer::HandleRead, this));
}

void
ReplayServer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void
ReplayServer::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
        socket->GetSockName(localAddress);
        m_rxTrace(packet);
        m_rxTraceWithAddresses(packet, from, localAddress);

        if (packet->GetSize() > 0)
        {
            uint32_t receivedSize = packet->GetSize();
                NS_LOG_INFO("TraceDelay: RX " << receivedSize << " bytes from "
                                              << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                              << " Uid: " << packet->GetUid() << " RXtime: " << Simulator::Now());
            m_received++;

            ProcessPacket(packet);

            uint8_t *repcl_buf = new uint8_t[64];

            uint32_t size = CreateReplayPacket(repcl_buf);
            uint32_t size = sizeof(uint8_t) * 64;

            Ptr<Packet> p = Create<Packet>(repcl_buf, size);

            socket->SendTo(packet, 0, from);

        }
        
    }
}

uint32_t
ReplayServer::CreateReplayPacket(uint8_t* buffer)
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
ReplayServer::ProcessPacket(Ptr<Packet> packet)
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

    server_rc.Recv(m_rc, GetNode()->GetNodeLocalClock());
    server->SetReplayClock(server_rc);

    delete[] buffer;
    delete[] integers;

}

} // Namespace ns3
