/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dtn-channel.hpp"
#include "dtn-transport.hpp"
#include <string>
#include <sys/stat.h> // for chmod()
#include "generic-link-service.hpp"
#include "core/scheduler.hpp"

// TODO
#include "../../../nfd-wrapper.cpp"

//#include <android/log.h>

//#define LOG_TAG2 "DEBFIN"
//#define LOGD2(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG2, __VA_ARGS__)


namespace nfd {

NFD_LOG_INIT("DtnChannel");





//class DtnChannel----------------------------------------------------------------------------------
DtnChannel::DtnChannel(const std::string &endpointPrefix, const std::string &endpointAffix, const std::string &ibrdtnHost, uint16_t ibrdtndPort)
  : m_endpointPrefix(endpointPrefix), m_endpointAffix(endpointAffix), m_ibrdtnHost(ibrdtnHost), m_ibrdtndPort(ibrdtndPort) // "","nfd","localHost",4550"
{
  m_is_open = false;

  setUri(FaceUri(m_endpointPrefix, m_endpointAffix));
}

DtnChannel::~DtnChannel()
{
  if (isListening()) {
    m_is_open = false;
  }
}
//------------------------------------------------------------------------------Class DtnChannel


// called by faceManager after the creation of the channel
void
DtnChannel::listen(const FaceCreatedCallback& onFaceCreated,
                          const FaceCreationFailedCallback& onReceiveFailed)
                          //int backlog/* = acceptor::max_connections*/)
{
  if (isListening()) {
    NFD_LOG_WARN("[" << m_endpointAffix << "] Already listening");
    return;
  }
  m_is_open = true;

  m_onFaceCreated = onFaceCreated;
  m_onReceiveFailed = onReceiveFailed;

  std::string app = m_endpointAffix.substr(1); // Remove leading '/'

  ioService = &getGlobalIoService();
  registerChannelToWrapper(this);
}
//-------------------------------------------------------------------------------------Listen



// called by nfd-wrapper on incoming bundles
void
DtnChannel::queueBundle(std::string remoteEndpoint, uint8_t* payload, int len)
{
        /*// DELAY
        struct timespec res;
        clock_gettime(CLOCK_REALTIME, &res);
        double now_ms =  1000.0 * res.tv_sec + (double) res.tv_nsec / 1e6;
        LOGD2(",RCVend, , , ,%f",now_ms);
        // DELAY*/
        auto f1 = boost::bind(&DtnChannel::processBundle, this, _1, _2, _3);


        uint8_t *payloadHeap = new uint8_t[len];

        std::copy(payload, payload + len, payloadHeap);

        ioService->post(       [=] {f1(remoteEndpoint, payloadHeap, len);}         );



}

// sends bundle to the face for processing
//processBundle---------------------------------------------------------------------------------
void
DtnChannel::processBundle(std::string remoteEndpoint, uint8_t* payload, int len)
{



	NFD_LOG_INFO("DTN bundle received from " << remoteEndpoint);
	NFD_LOG_DEBUG("[" << m_endpointAffix << "] New peer " << remoteEndpoint);

	bool isCreated = false;
	shared_ptr<Face> face = nullptr;


	std::tie(isCreated, face) = createFace(remoteEndpoint, ndn::nfd::FACE_PERSISTENCY_ON_DEMAND);

	if (face == nullptr)
	{
		NFD_LOG_WARN("[" << m_endpointAffix << "] Failed to create face for peer " << remoteEndpoint);
		if (m_onReceiveFailed)
			m_onReceiveFailed(504,remoteEndpoint); // ADDED 504 (random error code) as the callback has changed
		return;
	}


	m_onFaceCreated(face);


	// dispatch the bundle to the face for processing
	static_cast<face::DtnTransport*>(face->getTransport())->receiveBundle(payload,len);
}
//-----------------------------------------------------------------------------------processBundle




//connect---------------------------------------------------------------------------------
// called by dtn-factory::createFace
void
DtnChannel::connect(const std::string &remoteEndpoint,
                    ndn::nfd::FacePersistency persistency,
                    const FaceCreatedCallback& onFaceCreated,
                    const FaceCreationFailedCallback& onConnectFailed)
{

  shared_ptr<Face> face;

  face = createFace(remoteEndpoint, persistency).second;
  if (face == nullptr)
  {
    NFD_LOG_WARN("[" << m_endpointAffix << "] Connect failed");
    if (onConnectFailed)
      //onConnectFailed(remoteEndpoint); //compile fails
    return;
  }

  // Need to invoke the callback regardless of whether or not we had already
  // created the face so that control responses and such can be sent
  onFaceCreated(face);
}
//-----------------------------------------------------------------------------------connect




//createFace---------------------------------------------------------------------------------
std::pair<bool, shared_ptr<Face>>
DtnChannel::createFace(const std::string& remoteEndpoint, ndn::nfd::FacePersistency persistency)
{

  auto it = m_channelFaces.find(remoteEndpoint);
  if (it != m_channelFaces.end()) {
    // we already have a face for this endpoint, just reuse it
	auto face = it->second;

	face->setPersistency(persistency);
    return {false, face};
  }

  // else, create a new face
  // Create Link Service
  auto linkService = make_unique<face::GenericLinkService>();

  // Create Transport
  std::string localEndpoint = m_endpointPrefix + m_endpointAffix;
  auto transport = make_unique<face::DtnTransport>(localEndpoint, remoteEndpoint, m_ibrdtnHost, m_ibrdtndPort );

  // Create Face
  auto face = make_shared<Face>(std::move(linkService), std::move(transport));
  face->setPersistency(persistency);
  m_channelFaces[remoteEndpoint] = face;
  connectFaceClosedSignal(*face,
    [this, remoteEndpoint] {
      NFD_LOG_TRACE("Erasing " << remoteEndpoint << " from channel face map");
      m_channelFaces.erase(remoteEndpoint);
    });
  return {true, face};
}
//-----------------------------------------------------------------------------------createFace


// TODO NOT USED
//accept---------------------------------------------------------------------------------
void
DtnChannel::accept(const FaceCreatedCallback& onFaceCreated,
                          const FaceCreationFailedCallback& onAcceptFailed)
{

}
//-----------------------------------------------------------------------------------accept



//handleAccept---------------------------------------------------------------------------------
void
DtnChannel::handleAccept(const boost::system::error_code& error,
                                const FaceCreatedCallback& onFaceCreated,
                                const FaceCreationFailedCallback& onAcceptFailed)
{
  if (error) {
    if (error == boost::asio::error::operation_aborted) // when the socket is closed by someone
      return;

    NFD_LOG_DEBUG("[] Accept failed: " << error.message());
    if (onAcceptFailed)
    return;
  }

  NFD_LOG_DEBUG("[] Incoming connection");

  accept(onFaceCreated, onAcceptFailed);
}
//-----------------------------------------------------------------------------------handleAccept


} // namespace nfd

