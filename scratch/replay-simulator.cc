/*
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
 */

// Network topology
//
//       n0            n1          n2
//         \           |           /
//          \          |          /
//           \         |         /
//            \        |        /
//             \       |       /
//              \      |      /
//               \     |     /
//                \    |    /
//       n4 ---------  n9 --------- n8
// - Common echo sink at n9

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/config-store.h"

#include <fstream>
#include <unistd.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ReplaySimulator");

int
main(int argc, char* argv[])
{
    //
    // Users may find it convenient to turn on explicit debugging
    // for selected modules; the below lines suggest how to do this
    //
    LogComponentEnable ("ReplaySimulator", LOG_LEVEL_INFO);
    LogComponentEnable ("ReplayClientServer", LOG_LEVEL_INFO);

    NS_LOG_INFO
    (
        "Configuration Params: " << 
        "N." << NUM_PROCS << 
        ".E." << EPSILON << 
        ".I." << INTERVAL << 
        ".D." << DELTA << 
        ".A." << ALPHA <<
        ".MAXOFF." << MAX_OFFSET_SIZE
    );
    sleep(2);

    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer n;

    n.Create(NUM_PROCS);
    NS_LOG_INFO("Created " << NUM_PROCS << " nodes.");

    InternetStackHelper internet;
    internet.Install(n);

    NS_LOG_INFO("Create channels.");
    //
    // Explicitly create the channels required by the topology (shown above).
    //
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));

    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(DELTA)));

    NetDeviceContainer d = csma.Install(n);

    //
    // We've got the "hardware" in place.  Now we need to add IP addresses.
    //
    NS_LOG_INFO("Assign IP Addresses.");
    
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(d);

    std::vector<Address> peerAddresses;

    for(int p = 0; p < NUM_PROCS; ++ p)
    {
        peerAddresses.push_back(Address(i.GetAddress(p)));
    }

    NS_LOG_INFO("Create Applications.");
    //
    // Create a ReplayClientServer application on node 0.
    //
    uint16_t port = 9; // well-known echo port number

    ApplicationContainer apps;

    Time interPacketInterval = MilliSeconds(ALPHA);

    for(int i = 0; i < NUM_PROCS; i++)
    {
        Ptr<ReplayClientServer> client = new ReplayClientServer();
        client->SetPort(port);
        client->SetPeerList(peerAddresses);
        client->SetSize(1024);
        client->SetInterval(interPacketInterval);

        Ptr<Node> node = n.Get(i);

        node->AddApplication(client);

        apps.Add(client);

    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(100.0));

    AsciiTraceHelper ascii;
    
    std::stringstream filename;

    filename    << "replay-sim-"
                << "N." << NUM_PROCS
                << "E." << EPSILON
                << "I." << INTERVAL
                << "D." << DELTA
                << "A." << ALPHA
                << "MAXOFFSETSIZE." << MAX_OFFSET_SIZE
                << ".tr"; 


    csma.EnableAsciiAll(ascii.CreateFileStream(filename.str()));

    std::stringstream pcapfile;

    pcapfile    << "PCAP-"
                << "N." << NUM_PROCS
                << "E." << EPSILON
                << "I." << INTERVAL
                << "D." << DELTA
                << "A." << ALPHA
                << "MAXOFFSETSIZE." << MAX_OFFSET_SIZE
                << ".replay-sim";

    // csma.EnablePcapAll(pcapfile.str(), true);

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    NS_LOG_INFO("MSG_TYPE,NODE_1,NODE_2,HLC,BITMAP,OFFSETS,COUNTERS,NUM_PROCS,EPSILON,INTERVAL,DELTA,ALPHA,MAX_OFFSET_SIZE,OFFSET_SIZE,COUNTER_SIZE,CLOCK_SIZE,MAX_OFFSET");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
