#include <string>
#include <fstream>
#include <cstdlib>
#include <map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"

typedef uint32_t uint;

using namespace ns3;

#define ERROR 0.000001

NS_LOG_COMPONENT_DEFINE("final1");

std::map<std::string, double> mapBytesReceivedIPV4, mapMaxThroughput;
double printGap = 0;

int main(int argc, char *argv[])
{

	uint32_t maxBytes = 1000000000;
	uint32_t port;
	uint32_t packetsize = 100;
	uint32_t run_time = 1;
	uint32_t for_loop = 50;

	bool simultaneously = false;
	std::string prot = "TcpVegas";
	CommandLine cmd;
	cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
	cmd.AddValue("packetsize", "Total number of bytes for application to send", packetsize);
	cmd.AddValue("prot", "Transport protocol to use:TcpHighSpeed, TcpVegas, TcpScalable", prot);
	cmd.AddValue("for_loop", "no of for loop runs", for_loop);
	cmd.AddValue("run_time", "run_time in factor of 5", run_time);
	cmd.AddValue("simultaneously", "run_time in factor of 5", simultaneously);

	cmd.Parse(argc, argv);

	if (prot.compare("TcpScalable") == 0)
	{
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpScalable::GetTypeId()));
	}
	else if (prot.compare("TcpHighSpeed") == 0)
	{
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHighSpeed::GetTypeId()));
	}
	else
	{
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpVegas::GetTypeId()));
	}

	Gnuplot2dDataset dataset_udp;
	Gnuplot2dDataset dataset_tcp;
	Gnuplot2dDataset dataset_udp_delay;
	Gnuplot2dDataset dataset_tcp_delay;

	for (uint i = 0; i < for_loop; ++i)
	{
		uint32_t udpPacketSize = packetsize + 100 * i;
		uint32_t tcpPacketSize = udpPacketSize;

		NodeContainer c;
		c.Create(6);
		NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
		NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
		NodeContainer n2n3 = NodeContainer(c.Get(2), c.Get(3));
		NodeContainer n3n4 = NodeContainer(c.Get(3), c.Get(4));
		NodeContainer n3n5 = NodeContainer(c.Get(3), c.Get(5));

		InternetStackHelper internet;
		internet.Install(c);

		uint32_t queueSizeHR = (80000 * 20) / udpPacketSize;
		uint32_t queueSizeRR = (30000 * 100) / udpPacketSize;

		PointToPointHelper p2p;

		p2p.SetDeviceAttribute("DataRate", StringValue("80Mbps"));
		p2p.SetChannelAttribute("Delay", StringValue("20ms"));
		p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSizeHR)));

		NetDeviceContainer d0d2 = p2p.Install(n0n2);
		NetDeviceContainer d1d2 = p2p.Install(n1n2);
		NetDeviceContainer d3d4 = p2p.Install(n3n4);
		NetDeviceContainer d3d5 = p2p.Install(n3n5);

		p2p.SetDeviceAttribute("DataRate", StringValue("30Mbps"));
		p2p.SetChannelAttribute("Delay", StringValue("100ms"));
		p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSizeRR)));
		NetDeviceContainer d2d3 = p2p.Install(n2n3);

		Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
		em->SetAttribute("ErrorRate", DoubleValue(ERROR));
		d3d4.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
		d3d5.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

		Ipv4AddressHelper ipv4;
		ipv4.SetBase("10.1.0.0", "255.255.255.0");
		Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

		ipv4.SetBase("10.1.1.0", "255.255.255.0");
		Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

		ipv4.SetBase("10.1.3.0", "255.255.255.0");
		Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

		ipv4.SetBase("10.1.4.0", "255.255.255.0");
		Ipv4InterfaceContainer i3i4 = ipv4.Assign(d3d4);

		ipv4.SetBase("10.1.5.0", "255.255.255.0");
		Ipv4InterfaceContainer i3i5 = ipv4.Assign(d3d5);

		Ipv4GlobalRoutingHelper::PopulateRoutingTables();
		Ipv4GlobalRoutingHelper g;
		Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("routing.routes", std::ios::out);
		g.PrintRoutingTableAllAt(Seconds(2), routingStream);

		/*
			**************************		
				CBR traffic on UDP
			**************************
		*/
		port = 9;

		OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(i3i4.GetAddress(1), port)));

		onoff.SetAttribute("PacketSize", UintegerValue(udpPacketSize));

		ApplicationContainer udp_apps_s = onoff.Install(n0n2.Get(0));

		if (simultaneously == false)
		{
			udp_apps_s.Start(Seconds((0.0 + (10 * i)) * run_time));
			udp_apps_s.Stop(Seconds((5.0 + (10 * i)) * run_time));
		}
		else
		{
			udp_apps_s.Start(Seconds((0.0 + (10 * i)) * run_time));
			udp_apps_s.Stop(Seconds((10.0 + (10 * i)) * run_time));
		}

		PacketSinkHelper sink_udp("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));

		ApplicationContainer udp_apps_d = sink_udp.Install(n3n4.Get(1));

		if (simultaneously == false)
		{
			udp_apps_d.Start(Seconds((0.0 + (10 * i)) * run_time));
			udp_apps_d.Stop(Seconds((5.0 + (10 * i)) * run_time));
		}
		else
		{
			udp_apps_d.Start(Seconds((0.0 + (10 * i)) * run_time));
			udp_apps_d.Stop(Seconds((10.0 + (10 * i)) * run_time));
		}

		/*
			**************************		
				FTP traffic on TCP
			**************************
		*/

		port = 12344;
		BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i3i5.GetAddress(1), port));

		source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
		source.SetAttribute("SendSize", UintegerValue(tcpPacketSize));
		ApplicationContainer tcp_apps_s = source.Install(n1n2.Get(0));

		if (simultaneously == false)
		{
			tcp_apps_s.Start(Seconds((5.0 + (10 * i)) * run_time));
			tcp_apps_s.Stop(Seconds((10.0 + (10 * i)) * run_time));
		}
		else
		{
			tcp_apps_s.Start(Seconds((0.0 + (10 * i)) * run_time));
			tcp_apps_s.Stop(Seconds((10.0 + (10 * i)) * run_time));
		}

		PacketSinkHelper sink_tcp("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
		ApplicationContainer tcp_apps_d = sink_tcp.Install(n3n5.Get(1));

		if (simultaneously == false)
		{
			tcp_apps_d.Start(Seconds((5.0 + (10 * i)) * run_time));
			tcp_apps_d.Stop(Seconds((10.0 + (10 * i)) * run_time));
		}
		else
		{
			tcp_apps_d.Start(Seconds((0.0 + (10 * i)) * run_time));
			tcp_apps_d.Stop(Seconds((10.0 + (10 * i)) * run_time));
		}

		/* Flow Monitor */

		Ptr<FlowMonitor> flowmon;
		FlowMonitorHelper flowmonHelper;
		flowmon = flowmonHelper.InstallAll();
		Simulator::Stop(Seconds((10 + (10 * i)) * run_time));
		Simulator::Run();
		flowmon->CheckForLostPackets();

		Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
		std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

		double throughput_udp;
		double throughput_tcp;
		double delay_udp;
		double delay_tcp;

		for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
		{
			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
			std::cout << t.sourceAddress << "\n";
			std::cout << t.destinationAddress << "\n";

			if (t.sourceAddress == "10.1.0.1")
			{
				throughput_udp = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstRxPacket.GetSeconds()) / 1000000;
				delay_udp = i->second.delaySum.GetSeconds() / (i->second.rxPackets);

				dataset_udp.Add(udpPacketSize, throughput_udp);
				dataset_udp_delay.Add(udpPacketSize, delay_udp);

				std::cout << "UDP Flow over CBR " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
				std::cout << "Tx Packets: " << i->second.txPackets << "\n";
				std::cout << "Tx Bytes:" << i->second.txBytes << "\n";
				std::cout << "Rx Packets: " << i->second.rxPackets << "\n";
				std::cout << "Rx Bytes:" << i->second.rxBytes << "\n";
				std::cout << "Net Packet Lost: " << i->second.lostPackets << "\n";
				std::cout << "Lost due to droppackets: " << i->second.packetsDropped.size() << "\n";
				std::cout << "Delay: " << i->second.delaySum.GetSeconds() << std::endl;
				std::cout << "Mean Delay: " << i->second.delaySum.GetSeconds() / (i->second.rxPackets) << std::endl;
				std::cout << "Offered Load: " << i->second.txBytes * 8.0 / (i->second.timeLastTxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
				std::cout << "Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstRxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
				std::cout << "Mean jitter:" << i->second.jitterSum.GetSeconds() / (i->second.rxPackets - 1) << std::endl;
				std::cout << std::endl;
			}
			else if (t.sourceAddress == "10.1.1.1")
			{
				throughput_tcp = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstRxPacket.GetSeconds()) / 1000000;
				delay_tcp = i->second.delaySum.GetSeconds() / (i->second.rxPackets);

				dataset_tcp.Add(tcpPacketSize, throughput_tcp);
				dataset_tcp_delay.Add(tcpPacketSize, delay_tcp);

				std::cout << prot << " Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
				std::cout << "Tx Packets: " << i->second.txPackets << "\n";
				std::cout << "Tx Bytes:" << i->second.txBytes << "\n";
				std::cout << "Rx Packets: " << i->second.rxPackets << "\n";
				std::cout << "Rx Bytes:" << i->second.rxBytes << "\n";
				std::cout << "Net Packet Lost: " << i->second.lostPackets << "\n";
				std::cout << "Lost due to droppackets: " << i->second.packetsDropped.size() << "\n";
				std::cout << "Delay: " << i->second.delaySum.GetSeconds() << std::endl;
				std::cout << "Mean Delay: " << i->second.delaySum.GetSeconds() / (i->second.rxPackets) << std::endl;
				std::cout << "Offered Load: " << i->second.txBytes * 8.0 / (i->second.timeLastTxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
				std::cout << "Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstRxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
				std::cout << "Mean jitter:" << i->second.jitterSum.GetSeconds() / (i->second.rxPackets - 1) << std::endl;
				std::cout << std::endl;
			}
		}
		Simulator::Destroy();
	}

	std::string simultaneously_str = std::to_string(simultaneously);
	std::string fileNameWithNoExtension = prot + simultaneously_str;
	std::string graphicsFileName = fileNameWithNoExtension + ".png";
	std::string plotFileName = fileNameWithNoExtension + ".plt";
	std::string plotTitle = prot + "vs UDP throughput";

	std::string fileNameWithNoExtension_delay = prot + "_delay" + simultaneously_str;
	std::string graphicsFileName_delay = fileNameWithNoExtension_delay + ".png";
	std::string plotFileName_delay = fileNameWithNoExtension_delay + ".plt";
	std::string plotTitle_delay = prot + "vs UDP delay";

	Gnuplot plot(graphicsFileName);
	Gnuplot plot_delay(graphicsFileName_delay);

	plot.SetTitle(plotTitle);
	plot_delay.SetTitle(plotTitle_delay);

	plot.SetTerminal("png");
	plot_delay.SetTerminal("png");

	plot.SetLegend("Packet Size(in Bytes)", "Throughput Values(in mbps)");
	plot_delay.SetLegend("Packet Size(in Bytes)", "Delay(in s)");
	dataset_tcp.SetTitle("Throughput FTP over TCP");
	dataset_tcp.SetStyle(Gnuplot2dDataset::LINES_POINTS);
	dataset_udp.SetTitle("Throughput CBR over UDP");
	dataset_udp.SetStyle(Gnuplot2dDataset::LINES_POINTS);

	dataset_tcp_delay.SetTitle("Delay FTP over TCP");
	dataset_tcp_delay.SetStyle(Gnuplot2dDataset::LINES_POINTS);
	dataset_udp_delay.SetTitle("Delay CBR over UDP");
	dataset_udp_delay.SetStyle(Gnuplot2dDataset::LINES_POINTS);

	plot.AddDataset(dataset_tcp);
	plot.AddDataset(dataset_udp);

	plot_delay.AddDataset(dataset_udp_delay);
	plot_delay.AddDataset(dataset_tcp_delay);
	std::ofstream plotFile(plotFileName.c_str());

	plot.GenerateOutput(plotFile);

	plotFile.close();

	std::ofstream plotFile_delay(plotFileName_delay.c_str());
	plot_delay.GenerateOutput(plotFile_delay);
	plotFile_delay.close();

	return 0;
}