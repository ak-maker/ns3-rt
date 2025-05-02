// sionna-helper.h

#ifndef SIONNA_HELPER_H
#define SIONNA_HELPER_H

#include <string>
#include "ns3/vector.h"
#include "ns3/sionna-connection-handler.h"

namespace ns3 {

class SionnaHelper
{
public:
  static SionnaHelper& GetInstance();

  void SetSionna(bool sionna);
  bool GetSionna() const;

  void SetVerbose(bool verbose);
  bool GetVerbose() const;

  void SetServerIp(std::string serverIp);
  std::string GetServerIp() const;

  void SetLocalMachine(bool localMachine);
  bool GetLocalMachine() const;

  bool Initialize();
  void ShutdownSionna();

  double GetPathLossFromSionna(Vector txPos, Vector rxPos);
  double GetPropagationDelayFromSionna(Vector txPos, Vector rxPos);
  std::string GetLosStatusFromSionna(Vector txPos, Vector rxPos);
  void UpdateLocationInSionna(std::string nodeName, Vector pos, Vector vel);

private:
  // 不要在头文件中 = default；只声明即可
  SionnaHelper();
  ~SionnaHelper();

  SionnaHelper(const SionnaHelper&) = delete;
  SionnaHelper& operator=(const SionnaHelper&) = delete;

  bool m_sionna = false;
  bool m_verbose = false;
  bool m_localMachine = true;
  std::string m_serverIp = "127.0.0.1";
};

} // namespace ns3

#endif /* SIONNA_HELPER_H */
