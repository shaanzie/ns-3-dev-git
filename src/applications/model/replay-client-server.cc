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
#include "replay-client-server.h"

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
#include <unistd.h>
#include <random>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ReplayClientServer");

NS_OBJECT_ENSURE_REGISTERED(ReplayClientServer);

TypeId
ReplayClientServer::GetTypeId()
{
  static TypeId tid =
      TypeId("ns3::ReplayClientServer")
          .SetParent<Application>()
          .SetGroupName("Applications")
          .AddConstructor<ReplayClientServer>()
          .AddAttribute(
              "MaxPackets",
              "The maximum number of packets the application will send (zero means infinite)",
              UintegerValue(100),
              MakeUintegerAccessor(&ReplayClientServer::m_count),
              MakeUintegerChecker<uint32_t>())
          .AddAttribute("Interval",
                        "The time to wait between packets",
                        TimeValue(Seconds(1.0)),
                        MakeTimeAccessor(&ReplayClientServer::m_interval),
                        MakeTimeChecker())
          .AddAttribute("RemotePort",
                        "The destination port of the outbound packets",
                        UintegerValue(100),
                        MakeUintegerAccessor(&ReplayClientServer::m_peerPort),
                        MakeUintegerChecker<uint16_t>())
          .AddAttribute("PacketSize",
                        "Size of packets generated. The minimum packet size is 12 bytes which is "
                        "the size of the header carrying the sequence number and the time stamp.",
                        UintegerValue(1024),
                        MakeUintegerAccessor(&ReplayClientServer::m_size),
                        MakeUintegerChecker<uint32_t>(12, 65507));
  return tid;
}

ReplayClientServer::ReplayClientServer()
{
  NS_LOG_FUNCTION(this);
  m_sent = 0;
  m_socket = nullptr;
  m_sendEvent = EventId();
  m_received = 0;
}

ReplayClientServer::~ReplayClientServer()
{
  NS_LOG_FUNCTION(this);
}


/**
 * \brief set the port
 * \param port remote port
 */
void
ReplayClientServer::SetPort(uint16_t port)
{
  NS_LOG_FUNCTION(this << port);
  m_peerPort = port;
}


void 
ReplayClientServer::SetInterval(Time interval)
{
  m_interval = interval;
}

void 
ReplayClientServer::SetSize(uint32_t size)
{
  m_size = size;
}

/**
 * \brief set the remote address
 * \param addr remote address
 */
void 
ReplayClientServer::SetPeerList(std::vector<Address> addr)
{
  m_peerAddresses = addr;
}

void 
ReplayClientServer::DoDispose()
{
  NS_LOG_FUNCTION(this);
  Application::DoDispose();
}

void
ReplayClientServer::StartApplication()
{
  NS_LOG_FUNCTION(this);

  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddresses[GetNode()->GetId()]), m_peerPort);
    uint32_t flag = m_socket->Bind(local);
    
    Address sockaddress;
    m_socket->GetSockName(sockaddress);

    if (flag == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
  }

  m_socket->SetRecvCallback(MakeCallback(&ReplayClientServer::Recv, this));

  m_sendEvent = Simulator::Schedule(Seconds(0.0), &ReplayClientServer::Send, this);

}

void
ReplayClientServer::StopApplication()
{
  NS_LOG_FUNCTION(this);
  Simulator::Cancel(m_sendEvent);
  if (m_socket)
  {
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
  }
}

void 
ReplayClientServer::Send()
{
  NS_LOG_FUNCTION(this);

  Address peerAddress = GetRandomIP();

  if(m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(peerAddress), m_peerPort)) != 0)
  {
    NS_FATAL_ERROR("Connect failed!");
  }

  NS_ASSERT(m_sendEvent.IsExpired());

  Address from;
  Address to;
  m_socket->GetSockName(from);
  m_socket->GetPeerName(to);

  // NS_LOG_INFO(InetSocketAddress::ConvertFrom(to).GetIpv4() << ":" << InetSocketAddress::ConvertFrom(to).GetPort());

  ReplayHeader repheader = CreateReplayHeader(InetSocketAddress::ConvertFrom(from).GetIpv4(), InetSocketAddress::ConvertFrom(to).GetIpv4());

  NS_ABORT_IF(m_size < repheader.GetSerializedSize());
  Ptr<Packet> p = Create<Packet>(m_size - repheader.GetSerializedSize());

  p->AddHeader(repheader);

  if ((m_socket->Send(p)) >= 0)
  {
    ++m_sent;
    m_totalTx += p->GetSize();
    
    NS_LOG_DEBUG
    (                             "Node: " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                  << " sent " << p->GetSize() 
                                  << " bytes to " << InetSocketAddress::ConvertFrom(to).GetIpv4()
                                  << " at Time: " << (Simulator::Now()).As(Time::S)
    );
                                    
  }
  
  else
  {
      NS_LOG_DEBUG("Error while sending " << m_size << " bytes from " << from << " to " << to);
  }
  

  if (m_sent < m_count || m_count == 0)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &ReplayClientServer::Send, this);
  }

}

Address
ReplayClientServer::GetRandomIP()
{
  std::random_device rd; 
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, m_peerAddresses.size() - 1);

  int randomIP = distrib(gen);

  // return m_peerAddresses[(GetNode()->GetId() + 1) % m_peerAddresses.size()];
  return m_peerAddresses[randomIP];

}

void 
ReplayClientServer::Recv(Ptr<Socket> socket)
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
      NS_LOG_DEBUG
      (                             "Node: " << InetSocketAddress::ConvertFrom(localAddress).GetIpv4() 
                                    << " received "       << receivedSize 
                                    << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                    << " RXtime: "    << Simulator::Now()
      );
      m_received++;

      ProcessPacket(packet, InetSocketAddress::ConvertFrom(from).GetIpv4(), InetSocketAddress::ConvertFrom(localAddress).GetIpv4());
    }
  }
}

ReplayHeader
ReplayClientServer::CreateReplayHeader(Ipv4Address send, Ipv4Address recv)
{

  Ptr<Node> client = GetNode();

  ReplayClock client_rc = client->GetReplayClock();

  client_rc.SendLocal(GetNode()->GetNodeLocalClock());
  
  client->SetReplayClock(client_rc);

  ReplayHeader repheader(client_rc);

  // repheader.Print(std::cout);

  NS_LOG_INFO
  (
    "SEND" << "," <<
    send  << "," <<
    recv  << "," <<
    client_rc.GetHLC() << "," <<
    client_rc.GetBitmap() << "," <<
    client_rc.GetOffsets() << "," <<
    client_rc.GetCounters() << "," <<
    NUM_PROCS << ',' <<
    EPSILON << ',' <<
    INTERVAL << ',' <<
    DELTA << "," <<
    ALPHA << ',' <<
    MAX_OFFSET_SIZE << "," <<
    client_rc.GetOffsetSize() << "," <<
    client_rc.GetCounterSize() << "," <<
    client_rc.GetClockSize() << "," <<
    client_rc.GetMaxOffset()
  );

  return repheader;

}

void
ReplayClientServer::ProcessPacket(Ptr<Packet> packet, Ipv4Address send, Ipv4Address recv)
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

  NS_LOG_INFO
  (
    "RECV" << "," <<
    recv  << "," <<
    send << "," <<
    server_rc.GetHLC() << "," <<
    server_rc.GetBitmap() << "," <<
    server_rc.GetOffsets() << "," <<
    server_rc.GetCounters() << "," <<
    NUM_PROCS << ',' <<
    EPSILON << ',' <<
    INTERVAL << ',' <<
    DELTA << "," <<
    ALPHA << ',' <<
    MAX_OFFSET_SIZE << "," <<
    server_rc.GetOffsetSize() << "," <<
    server_rc.GetCounterSize() << "," <<
    server_rc.GetClockSize() << "," <<
    server_rc.GetMaxOffset()
  );

}

} // namespace ns3
