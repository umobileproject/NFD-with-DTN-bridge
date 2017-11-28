/*
 * dtn-transport.hpp
 */

#ifndef NDN_DAEMON_FACE_DTN_TRANSPORT_HPP_
#define NDN_DAEMON_FACE_DTN_TRANSPORT_HPP_

#include "transport.hpp"
#include "core/global-io.hpp"

namespace nfd {
namespace face {

class DtnTransport : public Transport
{
public:
  DtnTransport(std::string localEndpoint, std::string remoteEndpoint, std::string ibrdtndHost, uint16_t ibrdtndPort);

  void
  receiveBundle(uint8_t* payload, int len);


protected:
  virtual void
  beforeChangePersistency(ndn::nfd::FacePersistency newPersistency);

  virtual void
  doClose() override;

private:
  virtual void
  doSend(Transport::Packet&& packet) override;

  std::string m_ibrdtndHost;
  uint16_t m_ibrdtndPort;
};

} // namespace face
} // namespace nfd


#endif /* NDN_DAEMON_FACE_DTN_TRANSPORT_HPP_ */
