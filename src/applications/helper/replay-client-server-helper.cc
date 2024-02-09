/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 */
#include "replay-client-server-helper.h"

#include "ns3/string.h"
#include "ns3/replay-client.h"
#include "ns3/replay-server.h"
#include "ns3/uinteger.h"

namespace ns3
{

ReplayServerHelper::ReplayServerHelper()
{
    m_factory.SetTypeId(ReplayServer::GetTypeId());
}

ReplayServerHelper::ReplayServerHelper(uint16_t port)
{
    m_factory.SetTypeId(ReplayServer::GetTypeId());
    SetAttribute("Port", UintegerValue(port));
}

void
ReplayServerHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
ReplayServerHelper::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;

        m_server = m_factory.Create<ReplayServer>();
        node->AddApplication(m_server);
        apps.Add(m_server);
    }
    return apps;
}

Ptr<ReplayServer>
ReplayServerHelper::GetServer()
{
    return m_server;
}

ReplayClientHelper::ReplayClientHelper()
{
    m_factory.SetTypeId(ReplayClient::GetTypeId());
}

ReplayClientHelper::ReplayClientHelper(Address address, uint16_t port)
{
    m_factory.SetTypeId(ReplayClient::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
    SetAttribute("RemotePort", UintegerValue(port));
}

ReplayClientHelper::ReplayClientHelper(Address address)
{
    m_factory.SetTypeId(ReplayClient::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
}

void
ReplayClientHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
ReplayClientHelper::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;
        Ptr<ReplayClient> client = m_factory.Create<ReplayClient>();
        node->AddApplication(client);
        apps.Add(client);
    }
    return apps;
}

} // namespace ns3
