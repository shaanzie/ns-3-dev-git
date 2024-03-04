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
 *
 */

#ifndef REPLAY_CLIENT_SERVER_H
#define REPLAY_CLIENT_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/core-module.h"
#include "replay-header.h"
#include <ns3/traced-callback.h>

#include <vector>

namespace ns3
{

class Socket;
class Packet;

/**
 * \ingroup udpclientserver
 *
 * \brief A Udp client. Sends UDP packet carrying sequence number and time stamp
 *  in their payloads
 *
 */
class ReplayClientServer : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    ReplayClientServer();

    ~ReplayClientServer() override;

    /**
     * \brief set the port
     * \param port remote port
     */
    void SetPort(uint16_t port);

    /**
     * \brief set the remote address
     * \param addr remote address
     */
    void SetPeerList(std::vector<Address> addr);

    /**
     * \brief set interval between packets
     */
    void SetInterval(Time interval);

    /**
     * \brief set size of packets
     */
    void SetSize(uint32_t size);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * \brief Send a packet
     */
    void Send();

    /**
     * \brief Recv a packet from the socket
     * \param socket to recv a packet from
     */
    void Recv(Ptr<Socket> socket);

    /**
     * \brief Obtain replay timestamp from the packet
     * \param packet to get Replay timestamp from
     */
    void ProcessPacket(Ptr<Packet> packet);

    /**
     * \brief Create a Replay timestamp header
     * \return ReplayHeader with the newest clock
     */
    ReplayHeader CreateReplayHeader();

    /**
     * \brief Get a random receiver from list of receivers
     * \return IP of a random node
     */
    Address GetRandomIP();

    /**
     * \brief Print the ReplayClock onto std::out
     * \param rc The Replay Clock to print
     */
    void PrintClock(ReplayClock rc);

    uint32_t    m_count;                        //!< Maximum number of packets the application will send
    Time        m_interval;                     //!< Packet inter-send time
    uint32_t    m_size;                         //!< Size of the sent packet (including the SeqTsHeader)

    uint32_t                  m_sent;           //!< Counter for sent packets
    uint64_t                  m_totalTx;        //!< Total bytes sent
    Ptr<Socket>               m_socket;         //!< Socket
    std::vector<Address>      m_peerAddresses;  //!< Remote peer address
    uint16_t                  m_peerPort;       //!< Remote peer port
    EventId                   m_sendEvent;      //!< Event to send the next packet
    uint64_t                  m_received;       //!< Number of received packets

};

} // namespace ns3

#endif /* REPLAY_CLIENT_SERVER_H */
