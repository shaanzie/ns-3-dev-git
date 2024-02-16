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

#ifndef REPLAY_HEADER_H
#define REPLAY_HEADER_H

#include "ns3/header.h"
#include "ns3/nstime.h"
#include "ns3/replay-clock.h"

namespace ns3
{
/**
 * \ingroup applications
 *
 * \brief Packet header to carry ReplayClock
 *
 */
class ReplayHeader : public Header
{
  public:

    ReplayHeader();
    
    ReplayHeader(ReplayClock rc);

    /**
     * \return the replay clock
     */
    ReplayClock GetReplayClock() const;

    /**
     * Set the ReplayClock
     */
    void SetReplayClock();

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    ReplayClock m_rc;

};

} // namespace ns3

#endif /* REPLAY_HEADER_H */
