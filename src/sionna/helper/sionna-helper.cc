// sionna-helper.cc
#include "ns3/sionna-helper.h"
#include "ns3/log.h"

// 需要Definition of `SpectrumSignalParameters` for usage in pathloss model?
// That part is for sionna-lte-pathloss-model, not sionna-helper, so no problem here.
namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SionnaHelper");

SionnaHelper::SionnaHelper()
{
  NS_LOG_FUNCTION(this);
}

SionnaHelper::~SionnaHelper()
{
  NS_LOG_FUNCTION(this);
}

SionnaHelper&
SionnaHelper::GetInstance()
{
  static SionnaHelper instance;
  return instance;
}

void
SionnaHelper::SetSionna(bool sionna)
{
  m_sionna = sionna;
}

bool
SionnaHelper::GetSionna() const
{
  return m_sionna;
}

void
SionnaHelper::SetVerbose(bool verbose)
{
  m_verbose = verbose;
  sionna_verbose = verbose;
}

bool
SionnaHelper::GetVerbose() const
{
  return m_verbose;
}

void
SionnaHelper::SetServerIp(std::string ip)
{
  m_serverIp = ip;
  sionna_server_ip = ip;
}

std::string
SionnaHelper::GetServerIp() const
{
  return m_serverIp;
}

void
SionnaHelper::SetLocalMachine(bool localMachine)
{
  m_localMachine = localMachine;
  sionna_local_machine = localMachine;
}

bool
SionnaHelper::GetLocalMachine() const
{
  return m_localMachine;
}

bool
SionnaHelper::Initialize()
{
  if (!m_sionna)
    {
      NS_LOG_INFO("Sionna disabled");
      return false;
    }
  checkConnection(); // use the global function from sionna-connection-handler
  return true;
}

void
SionnaHelper::ShutdownSionna()
{
  if (m_sionna)
    {
      shutdownSionnaServer(); // global function
    }
}

double
SionnaHelper::GetPathLossFromSionna(Vector txPos, Vector rxPos)
{
  if (!m_sionna) {return 0.0;}
  return getPathGainFromSionna(txPos, rxPos);
}

double
SionnaHelper::GetPropagationDelayFromSionna(Vector txPos, Vector rxPos)
{
  if (!m_sionna) {return 0.0;}
  return getPropagationDelayFromSionna(txPos, rxPos);
}

std::string
SionnaHelper::GetLosStatusFromSionna(Vector txPos, Vector rxPos)
{
  if (!m_sionna) {return "Unknown";}
  return getLOSStatusFromSionna(txPos, rxPos);
}

void
SionnaHelper::UpdateLocationInSionna(std::string nodeName, Vector pos, Vector vel)
{
  if (!m_sionna) {return;}
  updateLocationInSionna(nodeName, pos, vel);
}

} // namespace ns3
