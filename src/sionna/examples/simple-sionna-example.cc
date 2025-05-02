/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/sionna-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NewSionnaExample");

/**
 * @brief 函数：将某个 Node 的位置/速度传给 Sionna
 */
static void
SendLocUpdateToSionna(Ptr<Node> node)
{
  std::string obj_id = "obj" + std::to_string(node->GetId());
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
  Vector pos = mob->GetPosition();
  Vector vel = mob->GetVelocity();

  updateLocationInSionna(obj_id, pos, vel);

  NS_LOG_UNCOND("Sent LOC_UPDATE for " << obj_id
               << " at (" << pos.x << "," << pos.y << "," << pos.z << ")");
}

int main(int argc, char* argv[])
{
  double txPower = 0;    // dBm
  uint16_t earfcn = 100; // LTE频点(对应2.1GHz左右)
  bool sionna = true;
  std::string server_ip = "127.0.0.1";
  bool local_machine = true;
  bool verb = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("txPower", "TX power in dBm", txPower);
  cmd.AddValue("earfcn", "EARFCN (default 100 - ~2.1GHz)", earfcn);
  cmd.AddValue("sionna", "Use and enable Sionna", sionna);
  cmd.AddValue("sionna-server-ip", "Sionna server IP", server_ip);
  cmd.AddValue("sionna-local-machine", "True if local", local_machine);
  cmd.AddValue("sionna-verbose", "Enable Sionna verbose", verb);
  cmd.Parse(argc, argv);

  // 1) 创建 SionnaHelper
  SionnaHelper& sionnaHelper = SionnaHelper::GetInstance();
  if (sionna)
  {
    sionnaHelper.SetSionna(sionna);
    sionnaHelper.SetServerIp(server_ip);
    sionnaHelper.SetLocalMachine(local_machine);
    sionnaHelper.SetVerbose(verb);
  }

  // 2) 创建 LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // 使用 MultiModelSpectrumChannel + SionnaLtePathlossModel
  lteHelper->SetSpectrumChannelType("ns3::MultiModelSpectrumChannel");
  lteHelper->SetSpectrumChannelAttribute("SpectrumPropagationLossModel",
                                         StringValue("ns3::SionnaLtePathlossModel"));

  // 3) EPC helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // 频点: DlEarfcn=100, UlEarfcn=18100
  lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(earfcn));
  lteHelper->SetEnbDeviceAttribute("UlEarfcn", UintegerValue(earfcn + 18000));

  // 4) 建立节点 (1 eNB + 3 UEs)，并定位
  NodeContainer enbNodes, ueNodes;
  enbNodes.Create(1);
  ueNodes.Create(3);

  // eNB位置 (0,0,1.5)
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityEnb.Install(enbNodes);
  enbNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 1.5));

  // UEs位置 (200,0,1.5), (200,10,1.5), (200,-10,1.5)
  MobilityHelper mobilityUe;
  mobilityUe.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityUe.Install(ueNodes);
  ueNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(200.0, 0.0, 1.5));
  ueNodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(200.0, 10.0, 1.5));
  ueNodes.Get(2)->GetObject<MobilityModel>()->SetPosition(Vector(200.0, -10.0, 1.5));

  // 5) 在UE装IP协议栈
  InternetStackHelper internet;
  internet.Install(ueNodes);

  // 6) 创建并安装 LTE NetDevice
  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);
  // 后续只要有 LTE PHY 的传输，就会自动调用这个 pathloss model 进行衰落。

  // 7) 设置 TxPower
  enbDevs.Get(0)->GetObject<LteEnbNetDevice>()->GetPhy()->SetTxPower(txPower);
  for (uint32_t i=0; i<ueDevs.GetN(); ++i)
  {
    ueDevs.Get(i)->GetObject<LteUeNetDevice>()->GetPhy()->SetTxPower(txPower);
  }

  // 8) Attach UEs 到 eNB
  for (uint32_t i=0; i<ueNodes.GetN(); i++)
  {
    lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
  }

  // 9) 产生UDP流量(Echo). 改为固定间隔0.01s, MaxPackets=1000
  uint16_t port = 9;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
  {
    // 在UE i 上安装 UdpEchoServer
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(i));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0)); // 让server在10s结束(也可设20s，但仿真只到10s)

    // 让其他UE给 i 发包
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
      if (i != j)
      {
        UdpEchoClientHelper echoClient(Ipv4Address("7.0.0.1"), port);
        // 发送大量包, 每隔0.01s
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01))); // 10ms
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(j));
        clientApps.Start(Seconds(2.0 + j));
        clientApps.Stop(Seconds(10.0));
      }
    }
  }

  // 10) 启动前，把 eNB/UE 坐标更新到 Sionna
  NS_LOG_UNCOND("=== Sending LOC_UPDATE for eNB and UEs ===");
  SendLocUpdateToSionna(enbNodes.Get(0));
  for (uint32_t i=0; i<ueNodes.GetN(); ++i)
  {
    SendLocUpdateToSionna(ueNodes.Get(i));
  }

  // 11) FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  // 仿真跑到10s
  Simulator::Stop(Seconds(10.0));
  lteHelper->EnablePhyTraces();
  lteHelper->EnableMacTraces();
  lteHelper->EnableRlcTraces();

  Simulator::Run();

  // 打印FlowMonitor统计
  flowmon->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();
  for (auto it = stats.begin(); it != stats.end(); ++it)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);

    NS_LOG_UNCOND("Flow " << it->first << " ( " << t.sourceAddress
                   << " -> " << t.destinationAddress << " )");
    NS_LOG_UNCOND("  TxPackets=" << it->second.txPackets
                   << "  RxPackets=" << it->second.rxPackets
                   << "  LostPackets=" << it->second.lostPackets);

    uint64_t rxBytes   = it->second.rxBytes;
    double firstTxTime = it->second.timeFirstTxPacket.GetSeconds();
    double lastRxTime  = it->second.timeLastRxPacket.GetSeconds();
    double timeDiff    = lastRxTime - firstTxTime;

    NS_LOG_UNCOND("  rxBytes=" << rxBytes
                   << "  firstTxTime=" << firstTxTime
                   << "s  lastRxTime=" << lastRxTime
                   << "s  timeDiff=" << timeDiff << "s");

    double throughputMbps = 0.0;
    if (timeDiff>0)
    {
      throughputMbps = (rxBytes * 8.0) / (timeDiff * 1e6);
    }
    NS_LOG_UNCOND("  Throughput=" << throughputMbps << " Mbps\n");
  }

  // (可选) 打印Sionna PathLoss
  // 你也可像原来那样 getPathGainFromSionna( enbPos, uePos ) 做校验

  Simulator::Destroy();
  sionnaHelper.ShutdownSionna();

  return 0;
}
