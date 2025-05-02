/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "sionna-lte-pathloss-model.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/mobility-model.h"
#include "ns3/spectrum-signal-parameters.h"
#include "ns3/spectrum-value.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SionnaLtePathlossModel");

// 让 ns-3 认识这个类并绑定到 TypeId 系统（可用属性或调试）
NS_OBJECT_ENSURE_REGISTERED(SionnaLtePathlossModel);

TypeId
SionnaLtePathlossModel::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::SionnaLtePathlossModel") // 定义本类在 ns-3 中的唯一标识符（字符串）
    .SetParent<SpectrumPropagationLossModel>()  // 父类是 SpectrumPropagationLossModel
    .SetGroupName("Lte")
    .AddConstructor<SionnaLtePathlossModel>();
  return tid;
}

SionnaLtePathlossModel::SionnaLtePathlossModel()
{
  NS_LOG_FUNCTION(this);
}

SionnaLtePathlossModel::~SionnaLtePathlossModel()
{
  NS_LOG_FUNCTION(this);
}

Ptr<SpectrumValue>
SionnaLtePathlossModel::DoCalcRxPowerSpectralDensity(
    Ptr<const SpectrumSignalParameters> params,
    Ptr<const MobilityModel> aMob,
    Ptr<const MobilityModel> bMob) const
{
  NS_LOG_FUNCTION(this << params << aMob << bMob);

  // 1) 复制一份 PSD，确保不会直接改动原始 params->psd
  // params->psd 通常是发射端的功率谱密度（Tx PSD），但我们不想直接修改它，因为可能后续还要给其他接收端用
  Ptr<SpectrumValue> rxPsd = Copy<SpectrumValue>(params->psd);

  // 2) 获取 Tx / Rx 坐标
  Vector txPos = aMob->GetPosition();
  Vector rxPos = bMob->GetPosition();

  // 3) 从 Sionna 获取 PathLoss (dB)
  // 这里调用了一个自定义的（或者外部）SionnaHelper 单例，计算发射端和接收端之间的路径损耗（path loss）并以 dB 为单位返回
  double pathLossDb = SionnaHelper::GetInstance().GetPathLossFromSionna(txPos, rxPos);

//  std::cout << "[SionnaLtePathlossModel] Tx=(" << txPos.x << "," << txPos.y
//              << ")  Rx=(" << rxPos.x << "," << rxPos.y << ")  pathLoss="
//              << pathLossDb << " dB" << std::endl;

  // 4) 换算成线性倍数
  double pathLossLin = std::pow(10.0, -pathLossDb / 10.0);

  // 5) 将衰减乘到 PSD 上
  // 这样就得到了接收端功率谱密度（Rx PSD）。因为频谱中每个采样点（子载波或频率格）都要受到相同的路径损耗衰减，所以把每个分量都乘以 pathLossLin
  (*rxPsd) *= pathLossLin;

  return rxPsd;
}

} // namespace ns3
