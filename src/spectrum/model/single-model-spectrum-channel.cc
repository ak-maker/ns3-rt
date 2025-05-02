/*
 * Copyright (c) 2009 CTTC
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */

#include "single-model-spectrum-channel.h"

#include "spectrum-phy.h"
#include "spectrum-propagation-loss-model.h"
#include "spectrum-transmit-filter.h"

#include <ns3/angles.h>
#include <ns3/antenna-model.h>
#include <ns3/double.h>
#include <ns3/log.h>
#include <ns3/mobility-model.h>
#include <ns3/net-device.h>
#include <ns3/node.h>
#include <ns3/object.h>
#include <ns3/packet-burst.h>
#include <ns3/packet.h>
#include <ns3/propagation-delay-model.h>
#include <ns3/propagation-loss-model.h>
#include <ns3/simulator.h>

#include <algorithm>
// 类名 SingleModelSpectrumChannel 继承自 SpectrumChannel
// SpectrumChannel 是 ns-3 框架中较高层的一个抽象类，用于描述在频谱维度上如何对数据进行传输、接收和损耗建模（即功率谱密度 PSD 的传播模型）。
// 功率谱密度（PSD）的传播模型”指的是针对信号在“频谱域”进行更精细化、基于频率分量的传播损耗和衰减建模的一种方法。
// 简单来说，与只关心总功率的传统模型不同，PSD 传播模型会将发射信号在频率域上的功率分布（即功率谱密度）拿出来“切片”或“离散”，分别对各个频率分量进行路径损耗、阴影衰落、多径衰落等单独的衰减处理，最终再将各个频率分量在接收端进行合并。
// std::vector<Ptr<SpectrumPhy>> m_phyList, 存储所有要加入当前信道的 SpectrumPhy 对象。每个 SpectrumPhy 通常对应一个“物理层”实例，比如 LTE、Wi-Fi、NR 的物理层收发器等。
// Ptr<const SpectrumModel> m_spectrumModel, 记录当前信道所使用的频谱模型（SpectrumModel）。在首次收到发射信号时会从其 PSD（psd->GetSpectrumModel()）获取并保存，用于确保所有参与通信的 PHY 都在同一频率离散模型下进行计算。
// Ptr<SpectrumPropagationLossModel> m_spectrumPropagationLoss, 一个可选的“频谱级”的传播损耗模型。它可以对每个频率采样点（或子载波）进行更细粒度的处理，也可以统一对所有频点进行同样的处理；具体由所用的模型决定。
// Ptr<PropagationLossModel> m_propagationLoss,一个更通用的传统标量传播损耗模型（CalcRxPower返回一个 dB 值），与 m_spectrumPropagationLoss 不同，它通常只返回一个单值的总衰减。该模型多出现在 StartTx 中，用来计算传统的单值 PathLoss
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SingleModelSpectrumChannel");

NS_OBJECT_ENSURE_REGISTERED(SingleModelSpectrumChannel);

SingleModelSpectrumChannel::SingleModelSpectrumChannel()
{
    NS_LOG_FUNCTION(this); // 构造函数：初始化日志系统（NS_LOG_FUNCTION(this)）
}

void
SingleModelSpectrumChannel::DoDispose() // 清空 m_phyList，置空 m_spectrumModel，并调用父类 DoDispose() 完成释放。这是 ns-3 的对象生命周期管理规定。
{
    NS_LOG_FUNCTION(this);
    m_phyList.clear();
    m_spectrumModel = nullptr;
    m_spectrumPropagationLoss = nullptr; // 释放指针
    SpectrumChannel::DoDispose();
}

TypeId
SingleModelSpectrumChannel::GetTypeId()
{
    NS_LOG_FUNCTION_NOARGS();
    static TypeId tid = TypeId("ns3::SingleModelSpectrumChannel")
                            .SetParent<SpectrumChannel>() // 声明其父类是 SpectrumChannel
                            .SetGroupName("Spectrum") // 归属分组，便于管理
                            .AddConstructor<SingleModelSpectrumChannel>() // 通过无参构造创建对象
                            .AddAttribute("SpectrumPropagationLossModel",
                            "A SpectrumPropagationLossModel to handle pathloss in StartRx()",
                            PointerValue(),
                            MakePointerAccessor(&SingleModelSpectrumChannel::m_spectrumPropagationLoss),
                            MakePointerChecker<SpectrumPropagationLossModel>()); // // 新增 Attribute，让外部可配置 SpectrumPropagationLossModel
    return tid;
}

void
SingleModelSpectrumChannel::RemoveRx(Ptr<SpectrumPhy> phy) // 如果 phy 不在 m_phyList 中，就将其添加进去。意味着该 PHY（接收器）现在加入了本信道，可以接收其他节点发来的数据
{
    NS_LOG_FUNCTION(this << phy);
    auto it = std::find(begin(m_phyList), end(m_phyList), phy);
    if (it != std::end(m_phyList))
    {
        m_phyList.erase(it);
    }
}

void
SingleModelSpectrumChannel::AddRx(Ptr<SpectrumPhy> phy) // 反向操作，从列表中移除对应的 PHY 对象。这两个函数一起管理信道上的收发设备集合。
{
    NS_LOG_FUNCTION(this << phy);
    if (std::find(m_phyList.cbegin(), m_phyList.cend(), phy) == m_phyList.cend())
    {
        m_phyList.push_back(phy);
    }
}

void
SingleModelSpectrumChannel::StartTx(Ptr<SpectrumSignalParameters> txParams)
{
    // 1. 日志 & 参数检查
    NS_LOG_FUNCTION(this << txParams->psd << txParams->duration << txParams->txPhy);
    NS_ASSERT_MSG(txParams->psd, "NULL txPsd");
    NS_ASSERT_MSG(txParams->txPhy, "NULL txPhy");

    // 2. 记录传输信号参数 (trace 回调)
    Ptr<SpectrumSignalParameters> txParamsTrace =
        txParams->Copy(); // copy it since traced value cannot be const (because of potential
                          // underlying DynamicCasts)
    m_txSigParamsTrace(txParamsTrace);

    // just a sanity check routine. We might want to remove it to save some computational load --
    // one "if" statement  ;-)
    // // 3. 初始化或校验 SpectrumModel
    if (!m_spectrumModel)
    {
        // first pak, record SpectrumModel
        m_spectrumModel = txParams->psd->GetSpectrumModel();
        // 当首次发射时记录当前信道的 SpectrumModel
        // 后续若有发射，则要确保它们都使用同一个 SpectrumModel（否则会在此断言报错）
    }
    else
    {
        // all attached SpectrumPhy instances must use the same SpectrumModel
        NS_ASSERT(*(txParams->psd->GetSpectrumModel()) == *m_spectrumModel);
    }

    // 4. 获取发射端的移动模型
    Ptr<MobilityModel> senderMobility = txParams->txPhy->GetMobility();

    // 5. 遍历所有 Rx 列表
    for (auto rxPhyIterator = m_phyList.begin(); rxPhyIterator != m_phyList.end(); ++rxPhyIterator)
    {
        Ptr<NetDevice> rxNetDevice = (*rxPhyIterator)->GetDevice();
        Ptr<NetDevice> txNetDevice = txParams->txPhy->GetDevice();

        if (rxNetDevice && txNetDevice)
        {
            // we assume that devices are attached to a node
            if (rxNetDevice->GetNode()->GetId() == txNetDevice->GetNode()->GetId())
            {
                NS_LOG_DEBUG("Skipping the pathloss calculation among different antennas of the "
                             "same node, not supported yet by any pathloss model in ns-3.");
                continue;
            }
        }

        // 5a. 跳过发射端自身和过滤器不通过的情况
        if (m_filter && m_filter->Filter(txParams, *rxPhyIterator))
        {
            continue;
        }

        if ((*rxPhyIterator) != txParams->txPhy) // 可以计算
        {
            Time delay = MicroSeconds(0);

            Ptr<MobilityModel> receiverMobility = (*rxPhyIterator)->GetMobility();
            NS_LOG_LOGIC("copying signal parameters " << txParams);
            // 5b. 复制一份信号参数给每个接收器
            Ptr<SpectrumSignalParameters> rxParams = txParams->Copy();

            // 5c. 计算路径损耗 (天线增益 / 传播模型等)
            if (senderMobility && receiverMobility)
            {
                double txAntennaGain = 0;
                double rxAntennaGain = 0;
                double propagationGainDb = 0;
                double pathLossDb = 0;
                // 路径损耗
                if (rxParams->txAntenna)
                {
                    Angles txAngles(receiverMobility->GetPosition(), senderMobility->GetPosition());
                    txAntennaGain = rxParams->txAntenna->GetGainDb(txAngles); // 获取发射天线增益 (txAntennaGain)
                    NS_LOG_LOGIC("txAntennaGain = " << txAntennaGain << " dB");
                    pathLossDb -= txAntennaGain;
                }
                Ptr<AntennaModel> rxAntenna =
                    DynamicCast<AntennaModel>((*rxPhyIterator)->GetAntenna());
                if (rxAntenna)
                {
                    Angles rxAngles(senderMobility->GetPosition(), receiverMobility->GetPosition());
                    rxAntennaGain = rxAntenna->GetGainDb(rxAngles); // 接收天线增益 (rxAntennaGain)
                    NS_LOG_LOGIC("rxAntennaGain = " << rxAntennaGain << " dB");
                    pathLossDb -= rxAntennaGain;
                }
                if (m_propagationLoss)
                {
                    propagationGainDb =
                        m_propagationLoss->CalcRxPower(0, senderMobility, receiverMobility); // 得到传播增益（通常就是 -pathLoss dB）
                    NS_LOG_LOGIC("propagationGainDb = " << propagationGainDb << " dB");
                    pathLossDb -= propagationGainDb;
                }
                NS_LOG_LOGIC("total pathLoss = " << pathLossDb << " dB");
                // Gain trace
                m_gainTrace(senderMobility,
                            receiverMobility,
                            txAntennaGain,
                            rxAntennaGain,
                            propagationGainDb,
                            pathLossDb);
                // Pathloss trace
                m_pathLossTrace(txParams->txPhy, *rxPhyIterator, pathLossDb);
                // 最终把所有损耗合并到 pathLossDb
                if (pathLossDb > m_maxLossDb) // 计算总 pathLossDb；如果衰减过大，直接 continue; 跳过（表示无法接收）。
                {
                    // beyond range
                    continue;
                }
                // 将线性倍数乘到 PSD 上
                // 将衰减（线性倍数 pathGainLinear）乘到 rxParams->psd 上，得到衰减后的功率谱密度。
                double pathGainLinear = std::pow(10.0, (-pathLossDb) / 10.0);
//                *(rxParams->psd) *= pathGainLinear;

                // 5d. 计算传播时延 (PropagationDelayModel)
                if (m_propagationDelay)
                {
                    delay = m_propagationDelay->GetDelay(senderMobility, receiverMobility); // 如果存在 m_propagationDelay，计算发射与接收之间的传播延迟 delay = m_propagationDelay->GetDelay(...)。
                }
            }

            // 5e. 调用 ScheduleXXX，把接收事件调度到正确的节点或执行器上
            // Simulator::ScheduleWithContext() 或 Simulator::Schedule() 将 StartRx() 函数在指定延时后调用，实现异步接收。
            if (rxNetDevice)
            {
                // the receiver has a NetDevice, so we expect that it is attached to a Node
                uint32_t dstNode = rxNetDevice->GetNode()->GetId();
                Simulator::ScheduleWithContext(dstNode,
                                               delay,
                                               &SingleModelSpectrumChannel::StartRx,
                                               this,
                                               rxParams,
                                               *rxPhyIterator);
            }
            else
            {
                // the receiver is not attached to a NetDevice, so we cannot assume that it is
                // attached to a node
                Simulator::Schedule(delay,
                                    &SingleModelSpectrumChannel::StartRx,
                                    this,
                                    rxParams,
                                    *rxPhyIterator);
            }
        }
    }
}

// 接收过程：StartRx()
// 当先前调度的事件被触发后，该函数由仿真器（Simulator）在正确的时间点调用。
void
SingleModelSpectrumChannel::StartRx(Ptr<SpectrumSignalParameters> params, Ptr<SpectrumPhy> receiver)
{
    NS_LOG_FUNCTION(this << params);
    if (m_spectrumPropagationLoss)
    {
        params->psd =
            m_spectrumPropagationLoss->CalcRxPowerSpectralDensity(params,
                                                                  params->txPhy->GetMobility(),
                                                                  receiver->GetMobility()); // 如果 m_spectrumPropagationLoss（频谱级的损耗模型）不为空，则再次对 params->psd 做更细的频率相关衰落计算。
    }
    receiver->StartRx(params); // 最终调用 receiver->StartRx(params)，让 PHY 进入“接收”流程，如进一步计算干扰、处理解调、CRC 检查等。
}
//需要注意，这里有两层“衰落或损耗”计算：
//
//    在 StartTx() 中，用传统的标量 m_propagationLoss 先粗略衰减一次。
//
//    在 StartRx() 中，如果设置了 m_spectrumPropagationLoss，再根据频谱做细粒度衰减。
//    这两部分都可选，用户可以根据需要选择用哪一种或两者结合。

std::size_t
SingleModelSpectrumChannel::GetNDevices() const
{
    NS_LOG_FUNCTION(this);
    return m_phyList.size();
}

Ptr<NetDevice>
SingleModelSpectrumChannel::GetDevice(std::size_t i) const
{
    NS_LOG_FUNCTION(this << i);
    return m_phyList.at(i)->GetDevice()->GetObject<NetDevice>();
}

} // namespace ns3
