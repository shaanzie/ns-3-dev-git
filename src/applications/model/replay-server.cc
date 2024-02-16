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

void 
ReplayServer::PrintClock(ReplayClock rc)
{
    NS_LOG_INFO("-----------------------------------------------------------");
    NS_LOG_INFO("HLC: " << rc.GetHLC());
    NS_LOG_INFO("Bitmap: " << rc.GetBitmap());
    NS_LOG_INFO("Offsets: " << rc.GetOffsets());
    NS_LOG_INFO("Counters: " << rc.GetCounters());
}

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

            ReplayHeader repheader = CreateReplayHeader();

            Ptr<Packet> p = Create<Packet>(repheader.GetSerializedSize());

            if ((socket->SendTo(p, 0, from)) >= 0)
            {
                
                NS_LOG_INFO("TraceDelay TX " << p->GetSize() << " bytes to " << from << " Uid: "
                                     << p->GetUid() << " from " 
                                     << GetNode()->GetId() << " at Time: " << (Simulator::Now()).As(Time::S));
                                            
            }
            
            else
            {
                NS_LOG_INFO("Error while sending " << repheader.GetSerializedSize() << " bytes to " << from);
            }

        }
        
    }
}

ReplayHeader
ReplayServer::CreateReplayHeader()
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
ReplayServer::ProcessPacket(Ptr<Packet> packet)
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

} // Namespace ns3
