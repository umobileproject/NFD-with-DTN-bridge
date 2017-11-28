/*
 * dtn-transport.cpp
 *
 */

#include "dtn-transport.hpp"


#include "../../../nfd-wrapper.hpp"

#include <time.h>
//#include <android/log.h>

//#define LOG_TAG "DEBFIN"
//#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)


namespace nfd {
namespace face {

NFD_LOG_INIT("DtnTransport");

DtnTransport::DtnTransport(std::string localEndpoint, std::string remoteEndpoint, std::string ibrdtndHost, uint16_t ibrdtndPort):
		  m_ibrdtndHost(ibrdtndHost), m_ibrdtndPort(ibrdtndPort)
{
  this->setLocalUri(FaceUri("dtn-node1",""));
  this->setRemoteUri(FaceUri(remoteEndpoint.substr(6),""));
  this->setScope(ndn::nfd::FACE_SCOPE_NON_LOCAL);
  this->setPersistency(ndn::nfd::FACE_PERSISTENCY_PERSISTENT);
  this->setLinkType(ndn::nfd::LINK_TYPE_POINT_TO_POINT);
  this->setMtu(MTU_UNLIMITED);

  NFD_LOG_FACE_INFO("Creating DTN transport");
}
// ---------------------------------------------------------------------------------------------




// receive bundle ----------called by dtn-channel::processBundle------------------------------------
void
DtnTransport::receiveBundle(uint8_t* payload, int len)
{
	  //NFD_LOG_FACE_INFO("Received: " << b.getPayloadLength() << " payload bytes");
	  bool isOk = false;

	  //TLV block
	  Block element;

	  std::tie(isOk, element) = Block::fromBuffer(payload, len);

	  if (!isOk) {
		NFD_LOG_FACE_WARN("Failed to parse incoming packet");
		// This packet won't extend the face lifetime
		return;
	  }
		// create Packet object with Block element as input
	  Transport::Packet tp(std::move(element));
	  tp.remoteEndpoint = 0;

	  delete[] payload;
	  this->receive(std::move(tp));
}
// ---------------------------------------------------------------------------------------------



void
DtnTransport::beforeChangePersistency(ndn::nfd::FacePersistency newPersistency)
{
}

void
DtnTransport::doClose()
{
}

void
DtnTransport::doSend(Transport::Packet&& packet)
{

    	  std::string localURI = getLocalUri().toString();
    	  std::string localHost = getLocalUri().getHost();
    	  std::string ibrdtnHost = m_ibrdtndHost;



    	  std::string str(packet.packet.begin(),packet.packet.end());

    	  uint8_t* databuf = reinterpret_cast<uint8_t*>(&str[0]);
    	  int len = str.length();

    	  std::string destinationAddress = getRemoteUri().getScheme() + "://" + getRemoteUri().getHost() + getRemoteUri().getPath();

    	  /*// DELAY
    	  struct timespec res;
          clock_gettime(CLOCK_REALTIME, &res);
          double now_ms =  1000.0 * res.tv_sec + (double) res.tv_nsec / 1e6;
          LOGD(",SNDstart,%f",now_ms);
    	  // DELAY*/

    	  sendToIBRDTN(destinationAddress,str);

}

} // namespace face
} // namespace nfd

