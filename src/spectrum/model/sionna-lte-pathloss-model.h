/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef SIONNA_LTE_PATHLOSS_MODEL_H
#define SIONNA_LTE_PATHLOSS_MODEL_H

#include "ns3/spectrum-propagation-loss-model.h"
#include "ns3/vector.h"
#include "ns3/sionna-helper.h"
#include "ns3/object.h"  // 如果你想用 NS_OBJECT_* 宏或者 TypeId, 可以包含
#include <cmath>         // 用到 std::pow

namespace ns3 {

/**
 * \ingroup lte
 * \brief A custom SpectrumPropagationLossModel that queries Sionna for path loss
 *
 * This class overrides DoCalcRxPowerSpectralDensity() to ask Python side (via
 * SionnaHelper) for the path loss in dB, and apply it to the received PSD.
 */
class SionnaLtePathlossModel : public SpectrumPropagationLossModel
{
public:
  static TypeId GetTypeId (void);

  SionnaLtePathlossModel ();
  virtual ~SionnaLtePathlossModel ();

protected:
  /**
   * \brief Implementation of the path loss calculation in Spectrum domain
   *
   * \param params The transmitted signal parameters (includes PSD)
   * \param aMob The MobilityModel of the transmitter
   * \param bMob The MobilityModel of the receiver
   * \return The adjusted PSD after applying path loss
   */
  virtual Ptr<SpectrumValue> DoCalcRxPowerSpectralDensity (
      Ptr<const SpectrumSignalParameters> params,
      Ptr<const MobilityModel> aMob,
      Ptr<const MobilityModel> bMob) const override;
};

} // namespace ns3

#endif /* SIONNA_LTE_PATHLOSS_MODEL_H */
