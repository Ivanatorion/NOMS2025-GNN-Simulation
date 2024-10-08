#include <iostream>
#include <iomanip>
#include <ctime>
#include <cstdio>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/gnn-header.h"

#define OUT_FILE_NAME "Throughput.txt"

using namespace ns3;

typedef struct lnk{
  int n1;
  int n2;
  std::string dataRate;
} LinkDef;

std::ofstream rxThroughput;
std::ofstream dataset;

bool countersInitialized = false;
std::vector<uint64_t> rxBytes;
std::vector<std::vector<uint64_t>> rxBytesPerInterval; //for std dev

uint64_t rxBytesGNNRound = 0;
uint64_t rxBytesInterval = 0;

void UpdateGNNs(std::vector<Ptr<RedQueueDisc>>& r_queues, const Time gnnUpdatePeriod){
  for(Ptr<RedQueueDisc> p : r_queues)
    p->GNNSendMessages();

  Simulator::Schedule(gnnUpdatePeriod, &UpdateGNNs, r_queues, gnnUpdatePeriod);
}

void UpdateGNNQueueSizeDeltas(std::vector<Ptr<RedQueueDisc>>& r_queues){
  for(Ptr<RedQueueDisc> p : r_queues){
    p->GNNUpdateQueueSizeDelta();
  }
  Simulator::Schedule(MicroSeconds(100), &UpdateGNNQueueSizeDeltas, r_queues);
}

void OutDataSet(std::vector<Ptr<RedQueueDisc>>& r_queues){
   dataset << (int) r_queues.size() << std::endl;
   for(Ptr<RedQueueDisc> p : r_queues){
    int qs = (int) p->GetCurrentSize().GetValue();
    dataset << p->GetTracingName() << "," << qs << "," << ((qs > 30) ? "1" : "0") << std::endl;
   }
   Simulator::Schedule(MicroSeconds(100), &OutDataSet, r_queues);
}

void
PrintProgress (std::vector<Ptr<RedQueueDisc>>& r_queues, Time interval, Time flowStartupTime, Time stopTime)
{
  static time_t startTime;

  double elapsedSeconds = Simulator::Now().GetSeconds();

  time_t timeNow;
  time(&timeNow);

  double countTime = 2.0 * (stopTime.GetSeconds() - flowStartupTime.GetSeconds()) / 3.0;
  double elapsedWC = (timeNow - startTime);

  int estimatedTime = (int) ((elapsedWC * countTime) / (elapsedSeconds - (stopTime.GetSeconds() - countTime)) - elapsedWC); 

  std::cout << "Progress to " << std::fixed << std::setprecision (1) << elapsedSeconds << " seconds simulation time";
  if(elapsedSeconds > stopTime.GetSeconds() - countTime + 0.0001)
    printf(" - ETA: %02d:%02d:%02d\n", estimatedTime / 3600, (estimatedTime / 60) % 60, estimatedTime % 60);
  else{
    printf("\n");
    time(&startTime);
  }

  const float avgTP = (float) rxBytesInterval / interval.GetSeconds();
  printf("AVG Interval TP: %.2fMB\n", avgTP / 1e6);
  rxBytesInterval = 0.0;

  if(countersInitialized){
    for(int i = 0; i < (int) rxBytesPerInterval.size(); i++){
      int sum = 0;
      for(int j = 0; j < (int) rxBytesPerInterval[i].size(); j++)
        sum = sum + rxBytesPerInterval[i][j];

      rxBytesPerInterval[i].push_back(rxBytes[i] - sum);
    }
  }

  Simulator::Schedule (interval, &PrintProgress, r_queues, interval, flowStartupTime, stopTime);
}

void
TraceRxSink (std::size_t index, Ptr<const Packet> p, const Address& a)
{
  rxBytes[index] += p->GetSize ();
  rxBytesGNNRound += p->GetSize ();
  rxBytesInterval += p->GetSize ();
}

void
InitializeCounters (void)
{
  for (std::size_t i = 0; i < rxBytes.size(); i++){
    rxBytes[i] = 0;
  }

  countersInitialized = true;
}

void
PrintThroughput (Time measurementWindow, const std::vector<int>& clientTargets)
{
  for(std::size_t i = 0; i < rxBytes.size(); i++){
    rxThroughput << measurementWindow.GetSeconds () << "s " << i << " " << (rxBytes[i] * 8) / (measurementWindow.GetSeconds ()) / 1e6 << " " << clientTargets[i] << std::endl;
  }
}

void
PrintFile (Time measurementWindow, std::vector<Ptr<RedQueueDisc>>& r_queues)
{
  double average = 0;
  uint64_t sumSquares = 0;
  uint64_t sum = 0;
  if(rxBytes.size() > 0){
    for (std::size_t i = 0; i < rxBytes.size(); i++)
      {
        sum += rxBytes[i];
        sumSquares += (rxBytes[i] * rxBytes[i]);
      }
    average = ((sum / rxBytes.size()) * 8 / measurementWindow.GetSeconds ()) / 1e6;
    rxThroughput << "Average throughput: " << std::fixed << std::setprecision (2) << average << " Mbps" << std::endl;
  }
  rxThroughput << "Standard dev:" << std::endl;

  for(int i = 0; i < (int) rxBytesPerInterval.size(); i++){
    double avgi = 0.0;
    for(int j = 0; j < (int) rxBytesPerInterval[i].size(); j++)
      avgi = avgi + ((rxBytesPerInterval[i][j] * 8) / 1e6);
    avgi = avgi / rxBytesPerInterval[i].size();

    double vari = 0.0;
    for(int j = 0; j < (int) rxBytesPerInterval[i].size(); j++)
      vari = vari + (((rxBytesPerInterval[i][j] * 8) / 1e6) - avgi) * (((rxBytesPerInterval[i][j] * 8) / 1e6) - avgi);
    vari = vari / rxBytesPerInterval[i].size();

    rxThroughput << std::fixed << std::setprecision (2) << sqrt(vari) << " - ";
    for(int j = 0; j < (int) rxBytesPerInterval[i].size(); j++)
      rxThroughput << std::fixed << std::setprecision (2) << ((rxBytesPerInterval[i][j] * 8) / 1e6) << " ";
    rxThroughput << std::endl;
  }

  for(Ptr<RedQueueDisc> p : r_queues)
    p->PrintGNNAcc();
  
  double gnn_acc_1 = 0.0;
  double gnn_acc_0 = 0.0;
  int qCount = 0;

  for(Ptr<RedQueueDisc> p : r_queues){
    double ac1 = p->GetGNNAcc(1);
    double ac0 = p->GetGNNAcc(0);

    if(ac1 > -0.5 || ac0 > -0.5){
      qCount++;
      gnn_acc_1 = gnn_acc_1 + ac1;
      gnn_acc_0 = gnn_acc_0 + ac0;
    }
  }

  if(qCount > 0){
    gnn_acc_1 = gnn_acc_1 / (double) qCount;
    gnn_acc_0 = gnn_acc_0 / (double) qCount;

    printf("ACC-0: %.4f\nACC-1: %.4f\n", gnn_acc_0, gnn_acc_1);
    rxThroughput << "ACC-0: " << std::fixed << std::setprecision(4) << gnn_acc_0 << " - ACC-1: " << std::fixed << std::setprecision(4) << gnn_acc_1 << std::endl;
  }

  //FlowDatas
  r_queues[0]->OutputFlowDatas("output/FlowDatas.dat");
}

std::string newIPBase(){
    static int n = 0;
    char buffer[128];
    sprintf(buffer, "10.1.%d.0", n);
    std::string result(buffer);
    n++;
    return result;
}

std::string nextTraceName(){
  static int n = 0;
  char buffer[128];
  sprintf(buffer, "T%d", n);
  std::string result(buffer);
  n++;
  return result;
}

void readInputFile(const std::string& inputFile, int &numHosts, int &numSwitches, std::vector<LinkDef>& links, std::vector<int>& hostsPerSwitch, std::string& dataRate1){
  int readB;
  
  FILE *fp = fopen(inputFile.c_str(), "r");
  if(!fp){
    std::cout << "Could not open file: " << inputFile << std::endl;
    exit(1);
  }

  char buffer[256];
  LinkDef tempLink;

  int numLinks;
  readB = fscanf(fp, "%d %d %s\n", &numSwitches, &numLinks, buffer);
  dataRate1 = buffer;
  
  links.reserve(numLinks);
  hostsPerSwitch.reserve(numSwitches);

  for(int i = 0; i < numLinks; i++){
    readB = fscanf(fp, "%d %d %s\n", &tempLink.n1, &tempLink.n2, buffer);
    tempLink.dataRate = buffer;
    links.push_back(tempLink);
  }

  numHosts = 0;
  for(int i = 0; i < numSwitches; i++){
    int x;
    readB = fscanf(fp, "%d\n", &x);
    hostsPerSwitch.push_back(x);
    numHosts = numHosts + x;
  }

  readB = readB;
  fclose(fp);
}

std::map<Node*, std::vector<Ptr<RedQueueDisc>>> switchQDiscs;

void receivePkt(Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from, const Address &to, NetDevice::PacketType packetType){
  GNNHeader gnnHeader;
  p->PeekHeader(gnnHeader);

  for(Ptr<RedQueueDisc> p : switchQDiscs[PeekPointer(device->GetNode())]){
    p->GNNReceiveMessage(gnnHeader);
  }
}

int main (int argc, char *argv[])
{
  Packet::EnablePrinting();

  std::string tcpTypeId = "TcpDctcp";
  Time progressInterval = MilliSeconds (1000);

  std::string inputFile = "";
  std::string gnnWeightsFile = "";
  std::string outputFilePath = ".";
  Time flowStartupWindow = Seconds (3);
  Time convergenceTime = Seconds (12);
  Time measurementWindow = Seconds (15);
  bool enableSwitchEcn = true;
  bool genDataset = false;
  int coreQueueTH = 30;
  int singleFlow = -1;
  bool enableGNN = false;

  Time gnnUpdatePeriod = Time("100us");

  std::string dataRate1 = "1Gbps";

  #ifndef NS_DEPRECATED_3_35
    CommandLine cmd;
  #else
    CommandLine cmd (__FILE__);
  #endif
  cmd.AddValue ("flowStartupWindow", "Startup time window (TCP staggered starts)", flowStartupWindow);
  cmd.AddValue ("convergenceTime", "Convergence time", convergenceTime);
  cmd.AddValue ("measurementWindow", "Measurement window", measurementWindow);
  cmd.AddValue ("enableSwitchEcn", "Enable ECN at switches", enableSwitchEcn);
  cmd.AddValue ("gnnWeightsFile", "File with GNN weights to determine ECN", gnnWeightsFile);
  cmd.AddValue ("gnnUpdatePeriod", "Frequency in which message exchange happens", gnnUpdatePeriod);
  cmd.AddValue ("coreQueueTH", "Switch queue threshold for marking", coreQueueTH);
  cmd.AddValue ("inputFile", "input file", inputFile);
  cmd.AddValue ("outputFilePath", "output file path", outputFilePath);
  cmd.AddValue ("singleFlow", "Specify a number to run only a single flow", singleFlow);
  cmd.AddValue ("genDataset", "Generate ECN dataset", genDataset);
  cmd.Parse (argc, argv);

  if(inputFile == ""){
    std::cout << "No input file defined" << std::endl;
    return 0;
  }

  if(gnnWeightsFile != "" && !genDataset)
    enableGNN = true;

  while(outputFilePath[outputFilePath.size() - 1] == '/')
    outputFilePath.pop_back();

  std::cout << "RngRun = " << RngSeedManager::GetRun() << std::endl;

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));

  Time startTime = Seconds (0);
  Time stopTime = flowStartupWindow + convergenceTime + measurementWindow;

  Time clientStartTime = startTime;

  int numHosts;
  int numSwitches;
  std::vector<LinkDef> coreLinks;
  std::vector<int> hostsPerSwitch;

  readInputFile(inputFile, numHosts, numSwitches, coreLinks, hostsPerSwitch, dataRate1);

  rxBytes.resize(numHosts, 0);
  rxBytesPerInterval.resize(numHosts, {});

  InternetStackHelper internet;

  NodeContainer hosts;
  hosts.Create(numHosts);
  internet.Install(hosts);

  NodeContainer switches;
  switches.Create(numSwitches);
  internet.Install(switches);

  for(int i = 0; i < numSwitches; i++){
    Ptr<Node> n = switches.Get(i);
    std::vector<Ptr<RedQueueDisc>> aux;
    n->RegisterProtocolHandler(MakeCallback(&receivePkt), 0x0801, 0);
    switchQDiscs.emplace(PeekPointer(n), aux);
  }

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  // Set default parameters for RED queue disc
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (enableSwitchEcn));
  // ARED may be used but the queueing delays will increase; it is disabled
  // here because the SIGCOMM paper did not mention it
  // Config::SetDefault ("ns3::RedQueueDisc::ARED", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  Config::SetDefault ("ns3::RedQueueDisc::UseGNN", BooleanValue (enableGNN));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
  // Triumph and Scorpion switches used in DCTCP Paper have 4 MB of buffer
  // If every packet is 1500 bytes, 2666 packets can be stored in 4 MB
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2666p")));
  // DCTCP tracks instantaneous queue length only; so set QW = 1
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (coreQueueTH));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (coreQueueTH));

  Ipv4AddressHelper ipv4;

  int x = 0;
  for(int i = 0; i < numSwitches; i++){
    for(int j = 0; j < hostsPerSwitch[i]; j++){
      PointToPointHelper pointToPoint;
      pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate1));
      pointToPoint.SetChannelAttribute ("Delay", StringValue ("10us"));
      NodeContainer p2pNodes;
      p2pNodes.Add(hosts.Get(x));
      p2pNodes.Add(switches.Get(i));
      NetDeviceContainer devices;
      devices = pointToPoint.Install(p2pNodes);

      TrafficControlHelper tchReceivesFromHost;
      tchReceivesFromHost.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue (dataRate1),
                             "LinkDelay", StringValue ("10us"));

      QueueDiscContainer ct0 = tchReceivesFromHost.Install(devices.Get(0));

      TrafficControlHelper tchSendsToHost;
      tchSendsToHost.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue (dataRate1),
                             "LinkDelay", StringValue ("10us"));

      QueueDiscContainer ct1 = tchSendsToHost.Install(devices.Get(1));

      ipv4.SetBase(newIPBase().c_str(), "255.255.255.0");
      ipv4.Assign(devices);

      x++;
    }
  }

  std::vector<std::vector<int>> neighboorSwitchNums(numSwitches);
  std::vector<std::vector<std::string>> switchQNames(numSwitches);
  std::ofstream queueGraph;
  queueGraph.open (outputFilePath + "/queue_graph.dat", std::ios::out);

  std::vector<Ptr<RedQueueDisc>> r_queues;

  for(LinkDef ln : coreLinks){
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue (ln.dataRate));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("10us"));

    NodeContainer p2pNodes;
    p2pNodes.Add(switches.Get(ln.n1));
    p2pNodes.Add(switches.Get(ln.n2));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(p2pNodes);

    std::string t1 = nextTraceName();
    std::string t2 = nextTraceName();

    TrafficControlHelper tchCore;
    tchCore.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue (ln.dataRate),
                             "LinkDelay", StringValue ("10us"),
                             "TracingName", StringValue(t1));

    QueueDiscContainer ct = tchCore.Install(devices.Get(0));
    for(uint32_t i = 0; i < ct.GetN(); i++){
      Ptr<RedQueueDisc> rqdptr = dynamic_cast<RedQueueDisc *> (PeekPointer (ct.Get(i)));
      r_queues.push_back(rqdptr);
      rqdptr->SetGNNNetDevice(devices.Get(0));

      switchQDiscs[PeekPointer(switches.Get(ln.n1))].push_back(rqdptr);
    }

    TrafficControlHelper tchCore2;
    tchCore2.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue (ln.dataRate),
                             "LinkDelay", StringValue ("10us"),
                             "TracingName", StringValue(t2));

    QueueDiscContainer ct2 = tchCore2.Install(devices.Get(1));
    for(uint32_t i = 0; i < ct2.GetN(); i++){
      Ptr<RedQueueDisc> rqdptr = dynamic_cast<RedQueueDisc *> (PeekPointer (ct2.Get(i)));
      r_queues.push_back(rqdptr);
      rqdptr->SetGNNNetDevice(devices.Get(1));

      switchQDiscs[PeekPointer(switches.Get(ln.n2))].push_back(rqdptr);
    }

    ipv4.SetBase(newIPBase().c_str(), "255.255.255.0");
    ipv4.Assign(devices);

    queueGraph << t1 << " " << t2 << std::endl;
    r_queues[r_queues.size() - 2]->AddGNNNeighbor(std::stoi(t2.substr(1)));
    r_queues[r_queues.size() - 1]->AddGNNNeighbor(std::stoi(t1.substr(1)));
    for(int i : neighboorSwitchNums[ln.n1]){
      for(std::string s : switchQNames[i]){
        queueGraph << t1 << " " << s << std::endl;
        r_queues[r_queues.size() - 2]->AddGNNNeighbor(std::stoi(s.substr(1)));
        r_queues[std::stoi(s.substr(1))]->AddGNNNeighbor(r_queues.size() - 2);
      }
    }
    for(int i : neighboorSwitchNums[ln.n2]){
      for(std::string s : switchQNames[i]){
        queueGraph << t2 << " " << s << std::endl;
        r_queues[r_queues.size() - 1]->AddGNNNeighbor(std::stoi(s.substr(1)));
        r_queues[std::stoi(s.substr(1))]->AddGNNNeighbor(r_queues.size() - 1);
      }
    }
    for(std::string s : switchQNames[ln.n1]){
      queueGraph << t2 << " " << s << std::endl;
      r_queues[std::stoi(t2.substr(1))]->AddGNNNeighbor(std::stoi(s.substr(1)));
      r_queues[std::stoi(s.substr(1))]->AddGNNNeighbor(std::stoi(t2.substr(1)));
    }
    for(std::string s : switchQNames[ln.n2]){
      queueGraph << t1 << " " << s << std::endl;
      r_queues[std::stoi(t1.substr(1))]->AddGNNNeighbor(std::stoi(s.substr(1)));
      r_queues[std::stoi(s.substr(1))]->AddGNNNeighbor(std::stoi(t1.substr(1)));
    }
    for(std::string s : switchQNames[ln.n1]){
      for(std::string s2 : switchQNames[ln.n2]){
        queueGraph << s << " " << s2 << std::endl;
        r_queues[std::stoi(s2.substr(1))]->AddGNNNeighbor(std::stoi(s.substr(1)));
        r_queues[std::stoi(s.substr(1))]->AddGNNNeighbor(std::stoi(s2.substr(1)));
      }
    }

    neighboorSwitchNums[ln.n1].push_back(ln.n2);
    neighboorSwitchNums[ln.n2].push_back(ln.n1);

    switchQNames[ln.n1].push_back(t1);
    switchQNames[ln.n2].push_back(t2);
  }

  queueGraph.close();

  for(int i = 0; i < numSwitches; i++){
    for(std::string s : switchQNames[i]){
      for(std::string s2 : switchQNames[i]){
        r_queues[std::stoi(s.substr(1))]->AddGNNSwitchQueue(r_queues[std::stoi(s2.substr(1))]);
      }
    }
  }

  for(int i = 0; i < (int) r_queues.size(); i++){
    r_queues[i]->SetGNNQueueID(i);
    r_queues[i]->SetGNNHiddenStateDim(2);
    r_queues[i]->SetGNNNumIterations(2);
    if(enableGNN){
      r_queues[i]->LoadGNNNetworksFromFile(gnnWeightsFile);
    }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  std::vector<int> clientTargets(numHosts);
  for(int i = 0; i < numHosts; i++)
    clientTargets[i] = (i+1) % numHosts;
  for(int i = 0; i < 100 * numHosts; i++){
    int a = rand() % numHosts;
    int b = rand() % numHosts;
    if(a != b && clientTargets[b] != a && clientTargets[a] != b){
      int aux = clientTargets[a];
      clientTargets[a] = clientTargets[b];
      clientTargets[b] = aux;
    }
  }

  std::vector<Ptr<PacketSink> > endSinks;
  endSinks.reserve(numHosts);
  for (int i = 0; i < numHosts; i++)
  {
    uint16_t port = 50010;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install (hosts.Get (i));
    Ptr<PacketSink> packetSink = sinkApp.Get (0)->GetObject<PacketSink> ();
    endSinks.push_back (packetSink);
    sinkApp.Start (startTime);
    sinkApp.Stop (stopTime);

    OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", Address ());
    
    clientHelper1.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=2.0|Max=10.0]"));
    clientHelper1.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=8.0]"));
    //clientHelper1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    //clientHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    
    clientHelper1.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate1)));
    clientHelper1.SetAttribute ("PacketSize", UintegerValue (1000));

    ApplicationContainer clientApps1;
    char buffer[256];
    sprintf(buffer, "10.1.%d.1", clientTargets[i]);
    AddressValue remoteAddress (InetSocketAddress (buffer, port));
    clientHelper1.SetAttribute ("Remote", remoteAddress);
    clientApps1.Add (clientHelper1.Install (hosts.Get(i)));
    
    if(singleFlow == -1 || singleFlow == i)
      clientApps1.Start (i * flowStartupWindow / numHosts  + clientStartTime + MilliSeconds (i * 5));
    else
      clientApps1.Start (stopTime);
    
    clientApps1.Stop (stopTime);
    
  }

  char buffer[256];

  if(rxBytes.size() > 0){
    sprintf(buffer, "%s/%s", outputFilePath.c_str(), OUT_FILE_NAME);
    rxThroughput.open (buffer, std::ios::out);
    rxThroughput << "#Parameters:" << std::endl;
    rxThroughput << "RngRun = " << RngSeedManager::GetRun() << std::endl;
    rxThroughput << "enableSwitchEcn = " << (enableSwitchEcn ? "true" : "false") << std::endl;
    rxThroughput << "enableGNN = " << (enableGNN ? "true" : "false") << std::endl;
    rxThroughput << "flowStartupWindow = " << flowStartupWindow.GetSeconds() << "s" << std::endl;
    rxThroughput << "convergenceTime = " << convergenceTime.GetSeconds() << "s" << std::endl;
    rxThroughput << "measurementWindow = " << measurementWindow.GetSeconds() << "s" << std::endl;
    rxThroughput << "hostsDataRates = " << dataRate1 << std::endl;
    rxThroughput << "gnnUpdatePeriod = " << gnnUpdatePeriod.GetMicroSeconds() << "us" << std::endl;
    rxThroughput << "coreQueueTH = " << coreQueueTH << std::endl << std::endl;
    rxThroughput << "#Time(s) - flow - throughput(Mb/s) - host" << std::endl;

    for(std::size_t i = 0; i < rxBytes.size(); i++){
      endSinks[i]->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&TraceRxSink, i));
    }
  }

  time_t startT;
  time(&startT);

  if(enableGNN)
    Simulator::Schedule (MilliSeconds(1), &UpdateGNNs, r_queues, gnnUpdatePeriod);
  if(genDataset)
    Simulator::Schedule (MicroSeconds(100), &OutDataSet, r_queues);
  Simulator::Schedule (MicroSeconds(100), &UpdateGNNQueueSizeDeltas, r_queues);
  Simulator::Schedule (flowStartupWindow + convergenceTime, &InitializeCounters);
  Simulator::Schedule (flowStartupWindow + convergenceTime + measurementWindow, &PrintThroughput, measurementWindow, clientTargets);
  if(!genDataset)
    Simulator::Schedule (flowStartupWindow + convergenceTime + measurementWindow, &PrintFile, measurementWindow, r_queues);
  Simulator::Schedule (progressInterval, &PrintProgress, r_queues, progressInterval, flowStartupWindow, stopTime);
  Simulator::Stop (stopTime + TimeStep (1));

  if(genDataset)
    dataset.open(outputFilePath + "/skynet.dat");
  Simulator::Run ();
  if(genDataset)
    dataset.close();
  if(rxBytes.size() > 0)
    rxThroughput.close();

  Simulator::Destroy ();
  return 0;
}
