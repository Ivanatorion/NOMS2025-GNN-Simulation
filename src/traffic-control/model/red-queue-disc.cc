// /* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright © 2011 Marcos Talau
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
 * Author: Marcos Talau (talau@users.sourceforge.net)
 *
 * Thanks to: Duy Nguyen<duy@soe.ucsc.edu> by RED efforts in NS3
 *
 *
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *
 * Copyright (c) 1990-1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PORT NOTE: This code was ported from ns-2 (queue/red.cc).  Almost all 
 * comments have also been ported from NS-2
 */

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "red_queue_nn.h"
#include "red-queue-disc.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/string.h"
#include "ns3/gnn-header.h"
#include "ns3/net-device.h"
#include "ns3/node.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RedQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (RedQueueDisc);

//namespace ns3 {

//NS_LOG_COMPONENT_DEFINE ("GNNHeader");

NS_OBJECT_ENSURE_REGISTERED (GNNHeader);

GNNHeader::GNNHeader ()
{
  this->isValid = true;
  this->m_iter = 0;
  this->m_nodeID = 0;
  this->m_identifier = 99;
  for(int i = 0; i < GNN_HEADER_SIZE; i++)
    m_hiddenState[i] = 0.0;
  NS_LOG_FUNCTION (this);
}

void
GNNHeader::SetIter (uint32_t iter)
{
  NS_LOG_FUNCTION (this << iter);
  m_iter = iter;
}
uint32_t
GNNHeader::GetIter (void) const
{
  NS_LOG_FUNCTION (this);
  return m_iter;
}

void
GNNHeader::SetNodeID (uint8_t id)
{
  NS_LOG_FUNCTION (this << id);
  m_nodeID = id;
}
uint8_t
GNNHeader::GetNodeID (void) const
{
  NS_LOG_FUNCTION (this);
  return m_nodeID;
}

void GNNHeader::SetHS(double hs[]){
    for(int i = 0; i < GNN_HEADER_SIZE; i++)
      m_hiddenState[i] = hs[i];
}

double* GNNHeader::GetHS (void){
    return m_hiddenState;
}

TypeId
GNNHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::GNNHeader")
    .SetParent<Header> ()
    .SetGroupName("Applications")
    .AddConstructor<GNNHeader> ()
  ;
  return tid;
}

TypeId
GNNHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
GNNHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "iter=" << m_iter << " hs=[ ";
  for(int i = 0; i < GNN_HEADER_SIZE; i++){
    double d = m_hiddenState[i];
    os << d << " ";
  }
  os << "]";
}

uint32_t
GNNHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return 2 + 4 + sizeof(double) * GNN_HEADER_SIZE;
}

bool
GNNHeader::IsValid (void) const
{
  NS_LOG_FUNCTION (this);
  return this->isValid;
}

void
GNNHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteU8 (m_identifier);
  i.WriteU8 (m_nodeID);
  i.WriteHtonU32 (m_iter);

  for(int x = 0; x < GNN_HEADER_SIZE; x++){
    uint64_t aux;
    memcpy(&aux, &m_hiddenState[x], sizeof(double));
    i.WriteHtonU64 (aux);
  }
}

uint32_t
GNNHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  uint8_t identifier = i.ReadU8 ();
  m_nodeID = i.ReadU8 ();

  isValid = (identifier == m_identifier); //ID number
  m_iter = i.ReadNtohU32 ();
  for(int x = 0; x < GNN_HEADER_SIZE; x++){
    uint64_t aux = i.ReadNtohU64();
    memcpy(&m_hiddenState[x], &aux, sizeof(double));
  }
  return GetSerializedSize ();
}

typedef struct flowdt{
  int nPackets;
  int nPacketsECN;
  std::vector<std::string> queues;
} FlowData;

static std::map<std::string, FlowData> flowDatas;

TypeId RedQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RedQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName("TrafficControl")
    .AddConstructor<RedQueueDisc> ()
    .AddAttribute ("MeanPktSize",
                   "Average of packet size",
                   UintegerValue (500),
                   MakeUintegerAccessor (&RedQueueDisc::m_meanPktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("IdlePktSize",
                   "Average packet size used during idle times. Used when m_cautions = 3",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RedQueueDisc::m_idlePktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Wait",
                   "True for waiting between dropped packets",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RedQueueDisc::m_isWait),
                   MakeBooleanChecker ())
    .AddAttribute ("Gentle",
                   "True to increases dropping probability slowly when average queue exceeds maxthresh",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RedQueueDisc::m_isGentle),
                   MakeBooleanChecker ())
    .AddAttribute ("ARED",
                   "True to enable ARED",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RedQueueDisc::m_isARED),
                   MakeBooleanChecker ())
    .AddAttribute ("AdaptMaxP",
                   "True to adapt m_curMaxP",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RedQueueDisc::m_isAdaptMaxP),
                   MakeBooleanChecker ())
    .AddAttribute ("FengAdaptive",
                   "True to enable Feng's Adaptive RED",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RedQueueDisc::m_isFengAdaptive),
                   MakeBooleanChecker ())
    .AddAttribute ("NLRED",
                   "True to enable Nonlinear RED",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RedQueueDisc::m_isNonlinear),
                   MakeBooleanChecker ())
    .AddAttribute ("MinTh",
                   "Minimum average length threshold in packets/bytes",
                   DoubleValue (5),
                   MakeDoubleAccessor (&RedQueueDisc::m_minTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxTh",
                   "Maximum average length threshold in packets/bytes",
                   DoubleValue (15),
                   MakeDoubleAccessor (&RedQueueDisc::m_maxTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc",
                   QueueSizeValue (QueueSize ("25p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("QW",
                   "Queue weight related to the exponential weighted moving average (EWMA)",
                   DoubleValue (0.002),
                   MakeDoubleAccessor (&RedQueueDisc::m_qW),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("LInterm",
                   "The maximum probability of dropping a packet",
                   DoubleValue (50),
                   MakeDoubleAccessor (&RedQueueDisc::m_lInterm),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("TargetDelay",
                   "Target average queuing delay in ARED",
                   TimeValue (Seconds (0.005)),
                   MakeTimeAccessor (&RedQueueDisc::m_targetDelay),
                   MakeTimeChecker ())
    .AddAttribute ("Interval",
                   "Time interval to update m_curMaxP",
                   TimeValue (Seconds (0.5)),
                   MakeTimeAccessor (&RedQueueDisc::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("Top",
                   "Upper bound for m_curMaxP in ARED",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&RedQueueDisc::m_top),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("Bottom",
                   "Lower bound for m_curMaxP in ARED",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&RedQueueDisc::m_bottom),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("Alpha",
                   "Increment parameter for m_curMaxP in ARED",
                   DoubleValue (0.01),
                   MakeDoubleAccessor (&RedQueueDisc::SetAredAlpha),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("Beta",
                   "Decrement parameter for m_curMaxP in ARED",
                   DoubleValue (0.9),
                   MakeDoubleAccessor (&RedQueueDisc::SetAredBeta),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("FengAlpha",
                   "Decrement parameter for m_curMaxP in Feng's Adaptive RED",
                   DoubleValue (3.0),
                   MakeDoubleAccessor (&RedQueueDisc::SetFengAdaptiveA),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("FengBeta",
                   "Increment parameter for m_curMaxP in Feng's Adaptive RED",
                   DoubleValue (2.0),
                   MakeDoubleAccessor (&RedQueueDisc::SetFengAdaptiveB),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("LastSet",
                   "Store the last time m_curMaxP was updated",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&RedQueueDisc::m_lastSet),
                   MakeTimeChecker ())
    .AddAttribute ("Rtt",
                   "Round Trip Time to be considered while automatically setting m_bottom",
                   TimeValue (Seconds (0.1)),
                   MakeTimeAccessor (&RedQueueDisc::m_rtt),
                   MakeTimeChecker ())
    .AddAttribute ("Ns1Compat",
                   "NS-1 compatibility",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RedQueueDisc::m_isNs1Compat),
                   MakeBooleanChecker ())
    .AddAttribute ("LinkBandwidth", 
                   "The RED link bandwidth",
                   DataRateValue (DataRate ("1.5Mbps")),
                   MakeDataRateAccessor (&RedQueueDisc::m_linkBandwidth),
                   MakeDataRateChecker ())
    .AddAttribute ("LinkDelay", 
                   "The RED link delay",
                   TimeValue (MilliSeconds (20)),
                   MakeTimeAccessor (&RedQueueDisc::m_linkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("UseEcn",
                   "True to use ECN (packets are marked instead of being dropped)",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RedQueueDisc::m_useEcn),
                   MakeBooleanChecker ())
    .AddAttribute ("UseHardDrop",
                   "True to always drop packets above max threshold",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RedQueueDisc::m_useHardDrop),
                   MakeBooleanChecker ())
    .AddAttribute ("UseGNN",
                   "True to use GNN to determine ECN",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RedQueueDisc::m_useGNN),
                   MakeBooleanChecker ())
    .AddAttribute ("TracingName", "A name", StringValue(""), MakeStringAccessor (&RedQueueDisc::m_tracingName), MakeStringChecker())
  ;

  return tid;
}

RedQueueDisc::RedQueueDisc () :
  QueueDisc (QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();

  roNN = NULL;
  aggNN = NULL;
  uptNN = NULL;
  msgNN = NULL;

  gnn_last_queue_size = 0.0;

  gnn_0_hits = 0;
  gnn_0_misses = 0;
  gnn_1_hits = 0;
  gnn_1_misses = 0;
}

RedQueueDisc::~RedQueueDisc ()
{
  NS_LOG_FUNCTION (this);

  if(this->roNN != NULL)
    delete this->roNN;
  if(this->aggNN != NULL)
    delete this->aggNN;
  if(this->uptNN != NULL)
    delete this->msgNN;
  if(this->msgNN != NULL)
    delete this->msgNN;
}

void RedQueueDisc::PrintGNNAcc(){
  if(this->gnn_0_hits + this->gnn_0_misses == 0 || this->gnn_1_hits + this->gnn_1_misses == 0)
    return;

  printf("RED-Q %d GNN: 0 hits = %d / %d (%.4f) ", this->gnn_queue_id, this->gnn_0_hits, this->gnn_0_hits + this->gnn_0_misses, ((double) this->gnn_0_hits) / (this->gnn_0_hits + this->gnn_0_misses));
  printf("- 1 hits = %d / %d (%.4f)\n", this->gnn_1_hits, this->gnn_1_hits + this->gnn_1_misses, ((double) this->gnn_1_hits) / (this->gnn_1_hits + this->gnn_1_misses));
}

double RedQueueDisc::GetGNNAcc(int zeroOne){
  if((this->gnn_0_hits + this->gnn_0_misses) == 0 || (this->gnn_1_hits + this->gnn_1_misses) == 0)
    return -1.0;

  if(zeroOne == 0){
    return ((double) this->gnn_0_hits) / (this->gnn_0_hits + this->gnn_0_misses);
  }
  
  return ((double) this->gnn_1_hits) / (this->gnn_1_hits + this->gnn_1_misses);
}

void RedQueueDisc::SetGNNQueueID(int id){
  this->gnn_queue_id = id;
}

int RedQueueDisc::GetGNNQueueID(){
  return this->gnn_queue_id;
}

void RedQueueDisc::AddGNNNeighbor(const int neighbor){
  this->gnn_neighbors.push_back(neighbor);

  std::vector<double> n_state(this->gnn_hidden_state.size());
  this->gnn_neighbors_messages.push_back(n_state);
  this->gnn_neighbors_received.push_back(false);
}

void RedQueueDisc::AddGNNSwitchQueue(Ptr<RedQueueDisc> qp){
  this->gnn_switch_queues.push_back(qp);
}

std::vector<int> RedQueueDisc::GetGNNNeighbors(){
  return this->gnn_neighbors;
}

std::vector<double> RedQueueDisc::GetGNNHiddenState(){
  return this->gnn_hidden_state;
}

void RedQueueDisc::SetGNNNumIterations(const int num){
  this->gnn_num_iterations = num;
}

void RedQueueDisc::SetGNNHiddenStateDim(int dim){
  this->gnn_hidden_state.resize(dim);
  this->gnn_next_hidden_state.resize(dim);
  for(int i = 0; i < dim; i++){
    this->gnn_hidden_state[i] = 0.0;
    this->gnn_next_hidden_state[i] = 0.0;
  } 
}

void RedQueueDisc::LoadGNNNetworksFromFile(const std::string& file){
  if(this->msgNN != NULL){
    delete this->msgNN;
    this->msgNN = NULL;
  }
  if(this->aggNN != NULL){
    delete this->aggNN;
    this->aggNN = NULL;
  }
  if(this->uptNN != NULL){
    delete this->uptNN;
    this->uptNN = NULL;
  }
  if(this->roNN != NULL){
    delete this->roNN;
    this->roNN = NULL;
  }

  FILE *fp = fopen(file.c_str(), "r");

  if(!fp){
    printf("Could not open file: %s\n", file.c_str());
    exit(1);
  }

  int rb = 0;

  this->msgNN = new RQDNeuralNetwork();
  this->aggNN = new RQDNeuralNetwork();
  this->roNN = new RQDNeuralNetwork();
  this->uptNN = new RQDNeuralNetwork();

  int msgLayers, aggLayers, uptLayers, readoutLayers;
  
  rb += fscanf(fp, "%d %d %d %d\n", &uptLayers, &aggLayers, &msgLayers, &readoutLayers);

  double wgt;

  std::vector<Activation> activations;
  int activationsIndex = 0;
  for(int i = 0; i < msgLayers + aggLayers + uptLayers + readoutLayers; i++){
    char buffer[256];
    rb += fscanf(fp, "%s\n", buffer);
    if(!strcmp(buffer, "None"))
      activations.push_back(NONE);
    else if(!strcmp(buffer, "sigmoid"))
      activations.push_back(SIGMOID);
    else if(!strcmp(buffer, "relu"))
      activations.push_back(RELU);
    else{
      printf("Error: Unknown layer activation \"%s\"\n", buffer);
      exit(1);
    }
  }

  for(int i = 0; i < uptLayers; i++){
    
    int l, c;
    rb += fscanf(fp, "%d %d\n", &l, &c);
    RQDNeuralNetworkMatrix layer(l, c);
    for(int j = 0; j < l; j++){
      for(int k = 0; k < c; k++){
        rb += fscanf(fp, "%lf ", &wgt);
        layer.SetAt(j, k, wgt);
      }
    }

    rb += fscanf(fp, "%d\n", &c);
    RQDNeuralNetworkMatrix bias(1, c);
    for(int k = 0; k < c; k++){
      rb += fscanf(fp, "%lf ", &wgt);
      bias.SetAt(0, k, wgt);
    }

    this->uptNN->AddLayerMatrix(layer, bias, activations[activationsIndex]);
    activationsIndex++;
  }

  for(int i = 0; i < aggLayers; i++){
    
    int l, c;
    rb += fscanf(fp, "%d %d\n", &l, &c);
    RQDNeuralNetworkMatrix layer(l, c);
    for(int j = 0; j < l; j++){
      for(int k = 0; k < c; k++){
        rb += fscanf(fp, "%lf ", &wgt);
        layer.SetAt(j, k, wgt);
      }
    }

    rb += fscanf(fp, "%d\n", &c);
    RQDNeuralNetworkMatrix bias(1, c);
    for(int k = 0; k < c; k++){
      rb += fscanf(fp, "%lf ", &wgt);
      bias.SetAt(0, k, wgt);
    }

    this->aggNN->AddLayerMatrix(layer, bias, activations[activationsIndex]);
    activationsIndex++;
  }

  for(int i = 0; i < msgLayers; i++){
    
    int l, c;
    rb += fscanf(fp, "%d %d\n", &l, &c);
    RQDNeuralNetworkMatrix layer(l, c);
    for(int j = 0; j < l; j++){
      for(int k = 0; k < c; k++){
        rb += fscanf(fp, "%lf ", &wgt);
        layer.SetAt(j, k, wgt);
      }
    }

    rb += fscanf(fp, "%d\n", &c);
    RQDNeuralNetworkMatrix bias(1, c);
    for(int k = 0; k < c; k++){
      rb += fscanf(fp, "%lf ", &wgt);
      bias.SetAt(0, k, wgt);
    }

    this->msgNN->AddLayerMatrix(layer, bias, activations[activationsIndex]);
    activationsIndex++;
  }

  for(int i = 0; i < readoutLayers; i++){
    
    int l, c;
    rb += fscanf(fp, "%d %d\n", &l, &c);
    RQDNeuralNetworkMatrix layer(l, c);
    for(int j = 0; j < l; j++){
      for(int k = 0; k < c; k++){
        rb += fscanf(fp, "%lf ", &wgt);
        layer.SetAt(j, k, wgt);
      }
    }

    rb += fscanf(fp, "%d\n", &c);
    RQDNeuralNetworkMatrix bias(1, c);
    for(int k = 0; k < c; k++){
      rb += fscanf(fp, "%lf ", &wgt);
      bias.SetAt(0, k, wgt);
    }

    this->roNN->AddLayerMatrix(layer, bias, activations[activationsIndex]);
    activationsIndex++;
  }

  fclose(fp);
}

void RedQueueDisc::GNNReceiveMessage(GNNHeader& messageHeader){
  if(!m_useGNN){
    printf("ERROR: Q%d received GNN message while m_useGNN is false\n", this->gnn_queue_id);
    exit(0);
  }

  double *hs = messageHeader.GetHS();
  int senderID = messageHeader.GetNodeID();
  std::vector<double> message(this->gnn_hidden_state.size());
  for(int i = 0; i < (int) this->gnn_hidden_state.size(); i++){
    message[i] = hs[i];
  }

  int nb = 0;
  while(nb < (int) this->gnn_neighbors.size() && this->gnn_neighbors[nb] != senderID){
    nb++;
  }
  if(nb == (int) this->gnn_neighbors.size()){
    printf("ERROR: Q%d received message from non-neighboor %d\n", this->gnn_queue_id, senderID);
    exit(0);
  }

  std::vector<double> inp(this->gnn_hidden_state.size() * 2);

  //If we have a NN, apply
  if(this->msgNN->GetNLayers() > 0){
    for(int i = 0; i < (int) this->gnn_hidden_state.size(); i++)
      inp[i] = message[i];
    for(int i = 0; i < (int) this->gnn_hidden_state.size(); i++)
      inp[i+this->gnn_hidden_state.size()] = this->gnn_hidden_state[i];

    RQDNeuralNetworkMatrix msgR = this->msgNN->Apply(inp);
    for(int i = 0; i < (int) message.size(); i++){
      message[i] = msgR.GetAt(0, i);
    }
  }

  this->gnn_neighbors_messages[nb] = message;
  this->gnn_neighbors_received[nb] = true;

  while((int) messageHeader.GetIter() > this->gnn_iter_n){
    this->gnn_iter_n++;
    this->GNNAggregateAndUpdate();
  }

  bool allRecv = true;
  for(int i = 0; i < (int) this->gnn_neighbors_received.size(); i++)
    if(!this->gnn_neighbors_received[i])
      allRecv = false;
  if(allRecv){
    this->gnn_iter_n++;
    this->GNNAggregateAndUpdate();
  }
}

void RedQueueDisc::SetGNNNetDevice(Ptr<NetDevice> nd){
  this->m_gnnNetDevice = nd;
}

void RedQueueDisc::GNNSendMessages(){
  if(!m_useGNN)
    return;

  for(Ptr<RedQueueDisc> rq : this->gnn_switch_queues){
    std::vector<double> hs = rq->GetGNNHiddenState();
    Ptr<Packet> packet = Create<Packet>();
  
    GNNHeader gnnHeader;
    if(hs.size() > 0)
      gnnHeader.SetHS(&hs[0]);
    gnnHeader.SetNodeID(rq->GetGNNQueueID());
    gnnHeader.SetIter(this->gnn_iter_n);

    packet->AddHeader(gnnHeader);
    
    this->m_gnnNetDevice->Send(packet, Ipv4Address("10.1.1.1"), 0x0801);
  }
}

void RedQueueDisc::GNNUpdateQueueSizeDelta(){
  this->gnn_queue_size_delta = (double) this->GetCurrentSize().GetValue() - this->gnn_last_queue_size;
  this->gnn_last_queue_size = this->GetCurrentSize().GetValue();
}

void RedQueueDisc::GNNAggregateAndUpdate(){
  if(!m_useGNN)
    return;

  for(int i = 0; i < (int) this->gnn_next_hidden_state.size(); i++)
    this->gnn_next_hidden_state[i] = 0.0;

  int numMessages = 0;
  for(int i = 0; i < (int) this->gnn_neighbors_received.size(); i++){
    if(this->gnn_neighbors_received[i]){
      for(int j = 0; j < (int) this->gnn_next_hidden_state.size(); j++)
        this->gnn_next_hidden_state[j] = this->gnn_next_hidden_state[j] + this->gnn_neighbors_messages[i][j];
      this->gnn_neighbors_received[i] = false;
    }
    numMessages++;
  }
  if(numMessages == 0) numMessages = 1;
  for(int j = 0; j < (int) this->gnn_next_hidden_state.size(); j++)
      this->gnn_next_hidden_state[j] = this->gnn_next_hidden_state[j] / numMessages;

  std::vector<double> inp(this->gnn_hidden_state.size() * 2);
    
  for(int i = 0; i < (int) this->gnn_hidden_state.size(); i++){
    inp[i] = this->gnn_next_hidden_state[i];
  }
  for(int i = 0; i < (int) this->gnn_hidden_state.size(); i++){
    inp[i+this->gnn_hidden_state.size()] = this->gnn_hidden_state[i];
  }

  RQDNeuralNetworkMatrix uptR = this->uptNN->Apply(inp);

  for(int i = 0; i < (int) this->gnn_hidden_state.size(); i++)
    this->gnn_hidden_state[i] = uptR.GetAt(0, i);

  if(this->gnn_iter_n % this->gnn_num_iterations == 0){
    RQDNeuralNetworkMatrix forwardRO = this->roNN->Apply(this->gnn_hidden_state);
    gnnDecision = (forwardRO.GetAt(0, 0) > 0.5);

    this->gnn_hidden_state[0] = (double) this->GetCurrentSize().GetValue();
    this->gnn_hidden_state[1] = (double) this->gnn_queue_size_delta;
    for(int j = 2; j < (int) this->gnn_hidden_state.size(); j++)
      this->gnn_hidden_state[j] = 0.0;
  }
}

void RedQueueDisc::OutputFlowDatas(const std::string& file){
  FILE *fp = fopen(file.c_str(), "w");

  fprintf(fp,"FlowSource,Packets,PacketsECN,NQueues\n");
  for(auto v : flowDatas){
    fprintf(fp, "%s,%d,%d,%d\n", v.first.c_str(), v.second.nPackets, v.second.nPacketsECN, (int) v.second.queues.size());
  }

  fclose(fp);
}

void
RedQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  QueueDisc::DoDispose ();
}

void
RedQueueDisc::SetAredAlpha (double alpha)
{
  NS_LOG_FUNCTION (this << alpha);
  m_alpha = alpha;

  if (m_alpha > 0.01)
    {
      NS_LOG_WARN ("Alpha value is above the recommended bound!");
    }
}

double
RedQueueDisc::GetAredAlpha (void)
{
  NS_LOG_FUNCTION (this);
  return m_alpha;
}

void
RedQueueDisc::SetAredBeta (double beta)
{
  NS_LOG_FUNCTION (this << beta);
  m_beta = beta;

  if (m_beta < 0.83)
    {
      NS_LOG_WARN ("Beta value is below the recommended bound!");
    }
}

double
RedQueueDisc::GetAredBeta (void)
{
  NS_LOG_FUNCTION (this);
  return m_beta;
}

void
RedQueueDisc::SetFengAdaptiveA (double a)
{
  NS_LOG_FUNCTION (this << a);
  m_a = a;

  if (m_a != 3)
    {
      NS_LOG_WARN ("Alpha value does not follow the recommendations!");
    }
}

double
RedQueueDisc::GetFengAdaptiveA (void)
{
  NS_LOG_FUNCTION (this);
  return m_a;
}

void
RedQueueDisc::SetFengAdaptiveB (double b)
{
  NS_LOG_FUNCTION (this << b);
  m_b = b;

  if (m_b != 2)
    {
      NS_LOG_WARN ("Beta value does not follow the recommendations!");
    }
}

double
RedQueueDisc::GetFengAdaptiveB (void)
{
  NS_LOG_FUNCTION (this);
  return m_b;
}

void
RedQueueDisc::SetTh (double minTh, double maxTh)
{
  NS_LOG_FUNCTION (this << minTh << maxTh);
  NS_ASSERT (minTh <= maxTh);
  m_minTh = minTh;
  m_maxTh = maxTh;
}

std::string
RedQueueDisc::GetTracingName() {
  return m_tracingName;
}

int64_t 
RedQueueDisc::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uv->SetStream (stream);
  return 1;
}

/*
void a(QueueDisc::InternalQueue *internal, RedQueueDisc *queue, QueueDiscItem *item, bool drop)
{
  if (item->GetProtocol() != 0x0800)
    {
      return;
    }

  auto ip = reinterpret_cast<Ipv4QueueDiscItem*>(item);
  auto congestionEncountered = ip->GetHeader().GetEcn() == Ipv4Header::ECN_CE;

  output <<
      Simulator::Now()
      << "," <<
      queue->GetTracingName()
        << "," <<
      queue->GetNPackets()
         << "," <<
      congestionEncountered
         << "," <<
      std::endl
      ;
}

bool shouldIgnore (QueueDiscItem *item)
{
  return (item->GetProtocol() != 0x0800);
}

bool congestionEncountered(QueueDiscItem *item)
{
  return (item->GetProtocol() == 0x0800)
         && reinterpret_cast<Ipv4QueueDiscItem*>(item)->GetHeader().GetEcn() == Ipv4Header::ECN_CE;
}
*/

bool
RedQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  //if (m_tracingName != "" && !output.is_open())
  //  {
  //    output.open ("output/skynet.dat");
  //  }

  NS_LOG_FUNCTION (this << item);

  uint32_t nQueued = GetInternalQueue (0)->GetCurrentSize ().GetValue ();

  //auto ignorePacket = shouldIgnore(PeekPointer(item));
  //auto ceSet = congestionEncountered(PeekPointer(item));

  // simulate number of packets arrival during idle period
  uint32_t m = 0;

  if (m_idle == 1)
    {
      NS_LOG_DEBUG ("RED Queue Disc is idle.");
      Time now = Simulator::Now ();

      if (m_cautious == 3)
        {
          double ptc = m_ptc * m_meanPktSize / m_idlePktSize;
          m = uint32_t (ptc * (now - m_idleTime).GetSeconds ());
        }
      else
        {
          m = uint32_t (m_ptc * (now - m_idleTime).GetSeconds ());
        }

      m_idle = 0;
    }

  m_qAvg = Estimator (nQueued, m + 1, m_qAvg, m_qW);

  NS_LOG_DEBUG ("\t bytesInQueue  " << GetInternalQueue (0)->GetNBytes () << "\tQavg " << m_qAvg);
  NS_LOG_DEBUG ("\t packetsInQueue  " << GetInternalQueue (0)->GetNPackets () << "\tQavg " << m_qAvg);

  m_count++;
  m_countBytes += item->GetSize ();

  uint32_t dropType = DTYPE_NONE;
  if(!this->m_useGNN){
    if (m_qAvg >= m_minTh && nQueued > 1)
      {
        if ((!m_isGentle && m_qAvg >= m_maxTh) ||
            (m_isGentle && m_qAvg >= 2 * m_maxTh))
          {
            NS_LOG_DEBUG ("adding DROP FORCED MARK");
            dropType = DTYPE_FORCED;
          }
        else if (m_old == 0)
          {
            /* 
            * The average queue size has just crossed the
            * threshold from below to above m_minTh, or
            * from above m_minTh with an empty queue to
            * above m_minTh with a nonempty queue.
            */
            m_count = 1;
            m_countBytes = item->GetSize ();
            m_old = 1;
          }
        else if (DropEarly (item, nQueued))
          {
            NS_LOG_LOGIC ("DropEarly returns 1");
            dropType = DTYPE_UNFORCED;
          }
      }
    else 
      {
        // No packets are being dropped
        m_vProb = 0.0;
        m_old = 0;
      }
  }
  else{
    if(this->roNN != NULL){
      if(this->gnnDecision)
        dropType = DTYPE_UNFORCED;
    }
  }

  if(this->roNN != NULL){
    if(m_qAvg < m_maxTh){
      if(!this->gnnDecision)
        this->gnn_0_hits++;
      else
        this->gnn_0_misses++;
    }
    else{
      if(!this->gnnDecision)
        this->gnn_1_misses++;
      else
        this->gnn_1_hits++;
    }
  }

  if (item->GetProtocol() == 0x0800){
    Ipv4QueueDiscItem* ip = reinterpret_cast<Ipv4QueueDiscItem*>(PeekPointer( item ));

    Ptr<Packet> packet = ip->GetPacket();
    Ipv4Header ipHdr = ip->GetHeader();

    uint32_t srcAddr = ipHdr.GetSource().Get();
    bool congestionEncountered = ipHdr.GetEcn() == Ipv4Header::ECN_CE;
    int mask = 256;

    if(ipHdr.GetPayloadSize()){ //Ignore Acks
      char buffer[64];
      sprintf(buffer, "%d.%d.%d.%d", (srcAddr >> 24) % mask, (srcAddr >> 16) % mask, (srcAddr >> 8) % mask, (srcAddr) % mask);
      std::string str(buffer);

      sprintf(buffer,"Q%d", this->gnn_queue_id);
      std::string qName(buffer);

      auto it = flowDatas.find(str);
      if(it != flowDatas.end()){
        if(it->second.queues[it->second.queues.size() - 1] == qName){
          it->second.nPackets = it->second.nPackets + 1;
          if(congestionEncountered){
            it->second.nPacketsECN = it->second.nPacketsECN + 1;
          }
        }
        else{
          bool queueInList = false;
          for(const std::string& q : it->second.queues){
            if(q == qName)
              queueInList = true;
          }
          if(!queueInList)
            it->second.queues.push_back(qName);
        }
      }
      else{
        FlowData data;
        data.nPackets = 1;
        data.nPacketsECN = (congestionEncountered ? 1 : 0);
        data.queues.push_back(qName);
        flowDatas.insert(std::make_pair(str, data));
      }
    }
  }

  if (dropType == DTYPE_UNFORCED){
    if (!m_useEcn || !Mark (item, UNFORCED_MARK))
    {
      NS_LOG_DEBUG ("\t Dropping due to Prob Mark " << m_qAvg);
      DropBeforeEnqueue (item, UNFORCED_DROP);
      return false;
    }
    NS_LOG_DEBUG ("\t Marking due to Prob Mark " << m_qAvg);
  }
  else if (dropType == DTYPE_FORCED)
    {
      if (m_useHardDrop || !m_useEcn || !Mark (item, FORCED_MARK))
        {
          NS_LOG_DEBUG ("\t Dropping due to Hard Mark " << m_qAvg);
          DropBeforeEnqueue (item, FORCED_DROP);
          if (m_isNs1Compat)
            {
              m_count = 0;
              m_countBytes = 0;
            }
          return false;
        }
      NS_LOG_DEBUG ("\t Marking due to Hard Mark " << m_qAvg);
    }

  bool retval = GetInternalQueue (0)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
  // internal queue because QueueDisc::AddInternalQueue sets the trace callback

  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  /*
  if (m_tracingName != "" && !ignorePacket && !ceSet)
    {
      a (PeekPointer (GetInternalQueue (0)), this, PeekPointer (item), false);
    }
  */

  return retval;
}

/*
 * Note: if the link bandwidth changes in the course of the
 * simulation, the bandwidth-dependent RED parameters do not change.
 * This should be fixed, but it would require some extra parameters,
 * and didn't seem worth the trouble...
 */
void
RedQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Initializing RED params.");

  m_cautious = 0;
  m_ptc = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize);

  if (m_isARED)
    {
      // Set m_minTh, m_maxTh and m_qW to zero for automatic setting
      m_minTh = 0;
      m_maxTh = 0;
      m_qW = 0;

      // Turn on m_isAdaptMaxP to adapt m_curMaxP
      m_isAdaptMaxP = true;
    }

  if (m_isFengAdaptive)
    {
      // Initialize m_fengStatus
      m_fengStatus = Above;
    }

  if (m_minTh == 0 && m_maxTh == 0)
    {
      m_minTh = 5.0;

      // set m_minTh to max(m_minTh, targetqueue/2.0) [Ref: http://www.icir.org/floyd/papers/adaptiveRed.pdf]
      double targetqueue = m_targetDelay.GetSeconds() * m_ptc;

      if (m_minTh < targetqueue / 2.0 )
        {
          m_minTh = targetqueue / 2.0;
        }
      if (GetMaxSize ().GetUnit () == QueueSizeUnit::BYTES)
        {
          m_minTh = m_minTh * m_meanPktSize;
        }

      // set m_maxTh to three times m_minTh [Ref: http://www.icir.org/floyd/papers/adaptiveRed.pdf]
      m_maxTh = 3 * m_minTh;
    }

  NS_ASSERT (m_minTh <= m_maxTh);

  m_qAvg = 0.0;
  m_count = 0;
  m_countBytes = 0;
  m_old = 0;
  m_idle = 1;

  double th_diff = (m_maxTh - m_minTh);
  if (th_diff == 0)
    {
      th_diff = 1.0; 
    }
  m_vA = 1.0 / th_diff;
  m_curMaxP = 1.0 / m_lInterm;
  m_vB = -m_minTh / th_diff;

  if (m_isGentle)
    {
      m_vC = (1.0 - m_curMaxP) / m_maxTh;
      m_vD = 2.0 * m_curMaxP - 1.0;
    }
  m_idleTime = NanoSeconds (0);

/*
 * If m_qW=0, set it to a reasonable value of 1-exp(-1/C)
 * This corresponds to choosing m_qW to be of that value for
 * which the packet time constant -1/ln(1-m)qW) per default RTT 
 * of 100ms is an order of magnitude more than the link capacity, C.
 *
 * If m_qW=-1, then the queue weight is set to be a function of
 * the bandwidth and the link propagation delay.  In particular, 
 * the default RTT is assumed to be three times the link delay and 
 * transmission delay, if this gives a default RTT greater than 100 ms. 
 *
 * If m_qW=-2, set it to a reasonable value of 1-exp(-10/C).
 */
  if (m_qW == 0.0)
    {
      m_qW = 1.0 - std::exp (-1.0 / m_ptc);
    }
  else if (m_qW == -1.0)
    {
      double rtt = 3.0 * (m_linkDelay.GetSeconds () + 1.0 / m_ptc);

      if (rtt < 0.1)
        {
          rtt = 0.1;
        }
      m_qW = 1.0 - std::exp (-1.0 / (10 * rtt * m_ptc));
    }
  else if (m_qW == -2.0)
    {
      m_qW = 1.0 - std::exp (-10.0 / m_ptc);
    }

  if (m_bottom == 0)
    {
      m_bottom = 0.01;
      // Set bottom to at most 1/W, where W is the delay-bandwidth
      // product in packets for a connection.
      // So W = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize * m_rtt.GetSeconds())
      double bottom1 = (8.0 * m_meanPktSize * m_rtt.GetSeconds()) / m_linkBandwidth.GetBitRate();
      if (bottom1 < m_bottom)
        {
          m_bottom = bottom1;
        }
    }

  NS_LOG_DEBUG ("\tm_delay " << m_linkDelay.GetSeconds () << "; m_isWait " 
                             << m_isWait << "; m_qW " << m_qW << "; m_ptc " << m_ptc
                             << "; m_minTh " << m_minTh << "; m_maxTh " << m_maxTh
                             << "; m_isGentle " << m_isGentle << "; th_diff " << th_diff
                             << "; lInterm " << m_lInterm << "; va " << m_vA <<  "; cur_max_p "
                             << m_curMaxP << "; v_b " << m_vB <<  "; m_vC "
                             << m_vC << "; m_vD " <<  m_vD);
}

// Updating m_curMaxP, following the pseudocode
// from: A Self-Configuring RED Gateway, INFOCOMM '99.
// They recommend m_a = 3, and m_b = 2.
void
RedQueueDisc::UpdateMaxPFeng (double newAve)
{
  NS_LOG_FUNCTION (this << newAve);

  if (m_minTh < newAve && newAve < m_maxTh)
    {
      m_fengStatus = Between;
    }
  else if (newAve < m_minTh && m_fengStatus != Below)
    {
      m_fengStatus = Below;
      m_curMaxP = m_curMaxP / m_a;
    }
  else if (newAve > m_maxTh && m_fengStatus != Above)
    {
      m_fengStatus = Above;
      m_curMaxP = m_curMaxP * m_b;
    }
}

// Update m_curMaxP to keep the average queue length within the target range.
void
RedQueueDisc::UpdateMaxP (double newAve)
{
  NS_LOG_FUNCTION (this << newAve);

  Time now = Simulator::Now ();
  double m_part = 0.4 * (m_maxTh - m_minTh);
  // AIMD rule to keep target Q~1/2(m_minTh + m_maxTh)
  if (newAve < m_minTh + m_part && m_curMaxP > m_bottom)
    {
      // we should increase the average queue size, so decrease m_curMaxP
      m_curMaxP = m_curMaxP * m_beta;
      m_lastSet = now;
    }
  else if (newAve > m_maxTh - m_part && m_top > m_curMaxP)
    {
      // we should decrease the average queue size, so increase m_curMaxP
      double alpha = m_alpha;
      if (alpha > 0.25 * m_curMaxP)
        {
          alpha = 0.25 * m_curMaxP;
        }
      m_curMaxP = m_curMaxP + alpha;
      m_lastSet = now;
    }
}

// Compute the average queue size
double
RedQueueDisc::Estimator (uint32_t nQueued, uint32_t m, double qAvg, double qW)
{
  NS_LOG_FUNCTION (this << nQueued << m << qAvg << qW);

  double newAve = qAvg * std::pow (1.0 - qW, m);
  newAve += qW * nQueued;

  Time now = Simulator::Now ();
  if (m_isAdaptMaxP && now > m_lastSet + m_interval)
    {
      UpdateMaxP (newAve);
    }
  else if (m_isFengAdaptive)
    {
      UpdateMaxPFeng (newAve);  // Update m_curMaxP in MIMD fashion.
    }

  return newAve;
}

// Check if packet p needs to be dropped due to probability mark
uint32_t
RedQueueDisc::DropEarly (Ptr<QueueDiscItem> item, uint32_t qSize)
{
  NS_LOG_FUNCTION (this << item << qSize);

  double prob1 = CalculatePNew ();
  m_vProb = ModifyP (prob1, item->GetSize ());

  // Drop probability is computed, pick random number and act
  if (m_cautious == 1)
    {
      /*
       * Don't drop/mark if the instantaneous queue is much below the average.
       * For experimental purposes only.
       * pkts: the number of packets arriving in 50 ms
       */
      double pkts = m_ptc * 0.05;
      double fraction = std::pow ((1 - m_qW), pkts);

      if ((double) qSize < fraction * m_qAvg)
        {
          // Queue could have been empty for 0.05 seconds
          return 0;
        }
    }

  double u = m_uv->GetValue ();

  if (m_cautious == 2)
    {
      /*
       * Decrease the drop probability if the instantaneous
       * queue is much below the average.
       * For experimental purposes only.
       * pkts: the number of packets arriving in 50 ms
       */
      double pkts = m_ptc * 0.05;
      double fraction = std::pow ((1 - m_qW), pkts);
      double ratio = qSize / (fraction * m_qAvg);

      if (ratio < 1.0)
        {
          u *= 1.0 / ratio;
        }
    }

  if (u <= m_vProb)
    {
      NS_LOG_LOGIC ("u <= m_vProb; u " << u << "; m_vProb " << m_vProb);

      // DROP or MARK
      m_count = 0;
      m_countBytes = 0;
      /// \todo Implement set bit to mark

      return 1; // drop
    }

  return 0; // no drop/mark
}

// Returns a probability using these function parameters for the DropEarly function
double
RedQueueDisc::CalculatePNew (void)
{
  NS_LOG_FUNCTION (this);
  double p;

  if (m_isGentle && m_qAvg >= m_maxTh)
    {
      // p ranges from m_curMaxP to 1 as the average queue
      // size ranges from m_maxTh to twice m_maxTh
      p = m_vC * m_qAvg + m_vD;
    }
  else if (!m_isGentle && m_qAvg >= m_maxTh)
    {
      /* 
       * OLD: p continues to range linearly above m_curMaxP as
       * the average queue size ranges above m_maxTh.
       * NEW: p is set to 1.0
       */
      p = 1.0;
    }
  else
    {
      /*
       * p ranges from 0 to m_curMaxP as the average queue size ranges from
       * m_minTh to m_maxTh
       */
      p = m_vA * m_qAvg + m_vB;

      if (m_isNonlinear)
        {
          p *= p * 1.5;
        }

      p *= m_curMaxP;
    }

  if (p > 1.0)
    {
      p = 1.0;
    }

  return p;
}

// Returns a probability using these function parameters for the DropEarly function
double 
RedQueueDisc::ModifyP (double p, uint32_t size)
{
  NS_LOG_FUNCTION (this << p << size);
  double count1 = (double) m_count;

  if (GetMaxSize ().GetUnit () == QueueSizeUnit::BYTES)
    {
      count1 = (double) (m_countBytes / m_meanPktSize);
    }

  if (m_isWait)
    {
      if (count1 * p < 1.0)
        {
          p = 0.0;
        }
      else if (count1 * p < 2.0)
        {
          p /= (2.0 - count1 * p);
        }
      else
        {
          p = 1.0;
        }
    }
  else
    {
      if (count1 * p < 1.0)
        {
          p /= (1.0 - count1 * p);
        }
      else
        {
          p = 1.0;
        }
    }

  if ((GetMaxSize ().GetUnit () == QueueSizeUnit::BYTES) && (p < 1.0))
    {
      p = (p * size) / m_meanPktSize;
    }

  if (p > 1.0)
    {
      p = 1.0;
    }

  return p;
}

Ptr<QueueDiscItem>
RedQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      m_idle = 1;
      m_idleTime = Simulator::Now ();

      return 0;
    }
  else
    {
      m_idle = 0;
      Ptr<QueueDiscItem> item = GetInternalQueue (0)->Dequeue ();

      NS_LOG_LOGIC ("Popped " << item);

      NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
      NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

      return item;
    }
}

Ptr<const QueueDiscItem>
RedQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);
  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<const QueueDiscItem> item = GetInternalQueue (0)->Peek ();

  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  return item;
}

bool
RedQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("RedQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("RedQueueDisc cannot have packet filters");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // add a DropTail queue
      AddInternalQueue (CreateObjectWithAttributes<DropTailQueue<QueueDiscItem> >
                          ("MaxSize", QueueSizeValue (GetMaxSize ())));
    }

  if (GetNInternalQueues () != 1)
    {
      NS_LOG_ERROR ("RedQueueDisc needs 1 internal queue");
      return false;
    }

  if ((m_isARED || m_isAdaptMaxP) && m_isFengAdaptive)
    {
      NS_LOG_ERROR ("m_isAdaptMaxP and m_isFengAdaptive cannot be simultaneously true");
    }

  return true;
}

} // namespace ns3
