/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Prefix.pch"
#include "Master.hpp"
#include "Library.hpp"
#include "Exceptions.hpp"
#include "MasterNodeConnection.hpp"
#include "Utils.hpp"
#include <vector>
#include "MasterNode.hpp"
#include "MasterClient.hpp"
#include "Balancer.hpp"
#include "Version.h"

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
using boost::format;

namespace mcore
{

    MasterParameters::MasterParameters(MSCMasterParameters const &param)
    {
		allowVersionSpecification = (param.flags & MSCMF_DisallowVersionSpecification) == 0;
		
        if (param.nodeEndpoint == nullptr) {
            MSCThrow(InvalidArgumentException("nodeEndpoint"));
        }
        nodeEndpoint = param.nodeEndpoint;

        if (param.clientEndpoint == nullptr) {
            MSCThrow(InvalidArgumentException("clientEndpoint"));
        }
        clientEndpoint = param.clientEndpoint;
        
        if (param.sslCertificateFile == nullptr) {
            MSCThrow(InvalidArgumentException("sslCertificateFile"));
        }
        sslCertificateFile = param.sslCertificateFile;
        
        if (param.sslPrivateKeyFile == nullptr) {
            MSCThrow(InvalidArgumentException("sslPrivateKeyFile"));
        }
        sslPrivateKeyFile = param.sslPrivateKeyFile;
        
        if (param.sslPassword != nullptr) {
            sslPassword = param.sslPassword;
        }
		
		if (param.packagePathCallback == nullptr) {
			MSCThrow(InvalidArgumentException("packagePathCallback"));
		}
		auto *packagePathCallback = param.packagePathCallback;
		auto *packagePathCallbackData = param.packagePathCallbackUserData;
		getPackagePathFunction = [=](const std::string& versionName) {
			std::array<char, 512> buffer;
			buffer[511] = 0;
			if (packagePathCallback(versionName.c_str(), buffer.data(), 511, packagePathCallbackData) != 0)
				return std::string();
			return std::string(buffer.data());
		};
    }

    Master::Master(const std::shared_ptr<Library>& library, const MasterParameters& parameters):
	_library(library),
	_parameters(parameters),
	nodeAcceptor(library->ioService(), parseTcpEndpoint(parameters.nodeEndpoint)),
	clientAcceptor(library->ioService(), parseTcpEndpoint(parameters.clientEndpoint)),
	heartbeatTimer(library->ioService()),
	heartbeatRunning(true),
	_sslContext(ssl::context::tlsv1),
	disposed(false),
	nodeAcceptorRunning(true),
	clientAcceptorRunning(true)
	{
		// Setup SSL
		std::string pw = parameters.sslPassword;
		sslContext().set_password_callback([pw](std::size_t, ssl::context::password_purpose) { return pw; });
		
		try {
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			format("Using %s as the certificate chain file.") % parameters.sslCertificateFile;
			
			sslContext().use_certificate_chain_file(parameters.sslCertificateFile);
		} catch (const std::exception& ex) {
			MSCThrow(InvalidDataException
			(str(format("Error occured while attempting to use '%s' as the "
						"certificate chain file: %s") %
				 parameters.sslCertificateFile % ex.what())));
		}
		try {
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			format("Using %s as the private key file.") % parameters.sslPrivateKeyFile;
			
			sslContext().use_private_key_file(parameters.sslPrivateKeyFile, boost::asio::ssl::context::pem);
		} catch (const std::exception& ex) {
			MSCThrow(InvalidDataException
			(str(format("Error occured while attempting to use '%s' as the "
						"private key file: %s") %
				 parameters.sslPrivateKeyFile % ex.what())));
		}
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "SSL is ready.";
		
		// Prepare to accept the first client and node
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Preparing to accept clients and nodes.";
		waitingNodeConnection = std::make_shared<MasterNodeConnection>(*this);
		waitingClient = std::make_shared<MasterClient>(*this, 1,
													   _parameters.allowVersionSpecification);
		
        acceptNodeConnectionAsync(true);
        acceptClientAsync(true);

		// Start heartbeat
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Starting heartbeat.";
        doHeartbeat(boost::system::error_code());
		
		BOOST_LOG_SEV(log, LogLevel::Info) <<
		format("Merlion Master Server Core (%s) running.") % MSC_VERSION_STRING;
    }

    void Master::checkValid() const
    {
        if (_library == nullptr)
            MSCThrow(InvalidOperationException());
    }

    boost::asio::io_service &Master::ioService() const
    {
        checkValid();
        return _library->ioService();
    }


    void Master::addListener(MasterListener *listener)
    {
        if (listener == nullptr) {
            return;
        }

        std::lock_guard<std::recursive_mutex> lock(listenersMutex);
        listeners.insert(listener);
    }

    void Master::removeListener(MasterListener *listener)
    {
        std::lock_guard<std::recursive_mutex> lock(listenersMutex);
        listeners.erase(listener);
    }

    void Master::doHeartbeat(const boost::system::error_code& e)
    {
		// Cancelled?
        if (e) {
            return;
        }

        std::lock_guard<std::recursive_mutex> hLock(heartbeatMutex);
        if (!heartbeatRunning)
            return;
		
		// Do heartbeat
        {
            std::lock_guard<std::recursive_mutex> lock(listenersMutex);
            for (auto *l: listeners)
                l->heartbeat(*this);
        }
		
		// Setup next heartbeat
        heartbeatTimer.expires_from_now(boost::posix_time::seconds(3));
        heartbeatTimer.async_wait([this]
        (const boost::system::error_code& e){
            doHeartbeat(e);
        });
    }

    void Master::acceptNodeConnectionAsync(bool initial)
    {
		if (initial) {
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			format("Listening for nodes at %s.") % nodeAcceptor.local_endpoint();
		}
		
		try {
			nodeAcceptor.async_accept(waitingNodeConnection->tcpSocket(),
			[=](const boost::system::error_code& errorCode) {
				if (errorCode.value() == boost::asio::error::operation_aborted || disposed) {
					std::lock_guard<std::mutex> guard(nodeAcceptorMutex);
					nodeAcceptorRunning = false;
					nodeAcceptorCV.notify_all();
					return;
				} else if (errorCode) {
					if (initial) {
						BOOST_LOG_SEV(log, LogLevel::Fatal) <<
						format("Accepting node connection failed: %s.") % errorCode;
					}
					std::lock_guard<std::mutex> guard(nodeAcceptorMutex);
					nodeAcceptorRunning = false;
					nodeAcceptorCV.notify_all();
					return;
				} else {
					// New node connected.
					auto conn = waitingNodeConnection;
					{
						std::lock_guard<std::recursive_mutex> lock(nodeConnectionsMutex);
						nodeConnections.push_front(waitingNodeConnection);
						waitingNodeConnection->iter = nodeConnections.begin();
						waitingNodeConnection = std::make_shared<MasterNodeConnection>(*this);
					}
					conn->didAccept();
				}

				// Accept next connection
				acceptNodeConnectionAsync(false);
			});
		} catch (...) {
			std::lock_guard<std::mutex> guard(nodeAcceptorMutex);
			nodeAcceptorRunning = false;
			nodeAcceptorCV.notify_all();
			throw;
		}
    }

    void Master::removeNodeConnection(const std::list<std::shared_ptr<MasterNodeConnection>>::iterator& it)
    {
        std::lock_guard<std::recursive_mutex> lock(nodeConnectionsMutex);
        nodeConnections.erase(it);
    }
    
    void Master::acceptClientAsync(bool initial)
	{
		if (initial) {
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			format("Listening for clients at %s.") % clientAcceptor.local_endpoint();
		}
		try {
			clientAcceptor.async_accept(waitingClient->tcpSocket(),
			[=](const boost::system::error_code& errorCode) {
				if (errorCode.value() == boost::asio::error::operation_aborted || disposed) {
					std::lock_guard<std::mutex> guard(clientAcceptorMutex);
					clientAcceptorRunning = false;
					clientAcceptorCV.notify_all();
					return;
				} else if (errorCode) {
					if (initial) {
						BOOST_LOG_SEV(log, LogLevel::Fatal) <<
						format("Accepting client connection failed: %s.") % errorCode;
					}
					std::lock_guard<std::mutex> guard(clientAcceptorMutex);
					clientAcceptorRunning = false;
					clientAcceptorCV.notify_all();
					return;
				} else {
					// New client connected.
					auto conn = waitingClient;
					
					// Prepare to accept the next client.
					{
						std::lock_guard<std::recursive_mutex> lock(clientsMutex);
						clients[conn->id()] = std::move(waitingClient);
						waitingClient = std::make_shared<MasterClient>(*this, conn->id() + 1,
																	   _parameters.allowVersionSpecification);
					}
					
					std::weak_ptr<MasterClient> weakConn(conn);
					
					// Setup "someone needs to respond to me" handler
					conn->onNeedsResponse.connect([this, weakConn](const std::shared_ptr<MasterClientResponse>& r) {
						
						auto conn = weakConn.lock();
						if (!conn) {
							// Unexpected...
							BOOST_LOG_SEV(log, LogLevel::Error) <<
							"onNeedsResponse raised by unexistent MasterClient.";
							return;
						}
						
						// Use balancer to choose a server.
						auto domain = bindClientToDomain(conn);
						 
						if (!domain) {
							r->reject("Could not find a suitable domain. Overload possible.");
							return;
						}
						
						auto version = domain->second;
						
						BOOST_LOG_SEV(log, LogLevel::Debug) <<
						format("Bound client '%d' to '%s' at '%s'.") % conn->id() %
						version % domain->first->nodeInfo().nodeName;
						
						// Make sure MasterClientResponse is in pendingClients
						// until it is done
						{
							std::lock_guard<std::recursive_mutex> lock(pendingClientsMutex);
							PendingClient pend;
							pend.response = r;
							pend.version = version;
							pendingClients.emplace(r->client()->id(), std::move(pend));
						}
						
						std::function<void(bool)> onResponded = [this, conn](bool) {
							std::lock_guard<std::recursive_mutex> lock(pendingClientsMutex);
							pendingClients.erase(conn->id());
						};
						
						r->onResponded.connect(onResponded);
						
						// It is not impossible that time-out already happened...
						if (r->isResponded()) {
							onResponded(false);
							return;
						}
						
						// Let node process the response.
						domain->first->acceptClient(r, version);
					});
					
					// Register client shutdown handler.
					conn->onShutdown.connect([this, weakConn]() {
						
						auto conn = weakConn.lock();
						if (!conn) {
							// Unexpected...
							BOOST_LOG_SEV(log, LogLevel::Error) <<
							"onShutdown raised by unexistent MasterClient.";
							return;
						}
						
						removeClient(conn->id());
					});
					
					// Start service
					conn->handleNewClient();
				}

				// Accept next connection
				acceptClientAsync(false);
			});
		} catch (...) {
			std::lock_guard<std::mutex> guard(clientAcceptorMutex);
			clientAcceptorRunning = false;
			clientAcceptorCV.notify_all();
			throw;
		}
	}
	
	boost::optional<Master::PendingClient>
	Master::dequePendingClient(std::uint64_t clientId)
	{
		std::lock_guard<std::recursive_mutex> lock(pendingClientsMutex);
		
		auto it = pendingClients.find(clientId);
		if (it == pendingClients.end()) {
			return boost::none;
		}
		
		auto ret = std::move(it->second);
		pendingClients.erase(it);
		
		return std::move(ret);
	}
	
	void Master::removeClient(std::uint64_t clientId)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(clientsMutex);
			clients.erase(clientId);
		}
		{
			std::lock_guard<std::recursive_mutex> lock2(listenersMutex);
			for (auto *l: listeners) l->clientDisconnected(clientId);
		}
	}
	
	std::shared_ptr<MasterClient> Master::getClient(std::uint64_t clientId)
	{
		std::lock_guard<std::recursive_mutex> lock(clientsMutex);
		auto it = clients.find(clientId);
		if (it != clients.end())
			return it->second;
		else
			return nullptr;
	}
	
    void Master::invalidate()
    {
        // Already invalidated?
        if (disposed) {
            return;
		}
		
		BOOST_LOG_SEV(log, LogLevel::Info) << "Master server is going DOWN.";
		
        disposed = true;

        {
            std::lock_guard<std::recursive_mutex> hLock(heartbeatMutex);
            heartbeatRunning = false;
            heartbeatTimer.cancel();
        }
		
		nodeAcceptor.cancel();
		clientAcceptor.cancel();

        // Wait until node/client acceptor is closed
		{
			std::unique_lock<std::mutex> acceptorLock(nodeAcceptorMutex);
			nodeAcceptorCV.wait(acceptorLock, [&]{ return !nodeAcceptorRunning; });
		}
		{
			std::unique_lock<std::mutex> acceptorLock(clientAcceptorMutex);
			clientAcceptorCV.wait(acceptorLock, [&]{ return !clientAcceptorRunning; });
		}

		// Delete node/client acceptor
        waitingNodeConnection->shutdown();
        waitingNodeConnection.reset();
		
		waitingClient->shutdown();
		waitingClient.reset();
		
		nodeAcceptor.close();
		clientAcceptor.close();
		
		// Reject all pending clients
		std::vector<PendingClient> pclientList;
		{
			std::lock_guard<std::recursive_mutex> lock(pendingClientsMutex);
			for (const auto& e: pendingClients)
				pclientList.push_back(e.second);
		}
		for (const auto& c: pclientList)
			c.response->reject("Server is going down.");
		
		// Shut all clients down
		std::vector<std::shared_ptr<MasterClient>> clientList;
		{
			std::lock_guard<std::recursive_mutex> lock(clientsMutex);
			for (const auto& e: clients)
				clientList.push_back(e.second);
		}
		for (const auto& c: clientList)
			c->shutdown();

        // Shut all connections down
        std::vector<std::shared_ptr<MasterNodeConnection>> nodeConns;
        {
            std::lock_guard<std::recursive_mutex> lock(nodeConnectionsMutex);
			std::copy(nodeConnections.begin(), nodeConnections.end(),
					  std::back_inserter(nodeConns));
		}
		
		for (const auto& c: nodeConns)
			c->shutdown();

        _library = nullptr;
    }

    Master::~Master()
    {
        invalidate();
    }
	
	void Master::addNode(const std::shared_ptr<MasterNode>& node)
	{
		std::lock_guard<std::recursive_mutex> lock(nodesMutex);
		nodes[node.get()] = node;
	}
	
	void Master::removeNode(MasterNode *node)
	{
		std::lock_guard<std::recursive_mutex> lock(nodesMutex);
		nodes.erase(node);
	}

    void Master::addVersion(const std::string &name)
    {
        std::lock_guard<std::recursive_mutex> lock(versionsMutex);

        MasterVersion ver;
        ver.name = name;
        ver.throttle = 0.0;
        versions[name] = ver;

        std::lock_guard<std::recursive_mutex> lock2(listenersMutex);
        for (auto *l: listeners) l->versionAdded(*this, name);
    }

    void Master::removeVersion(const std::string &name)
    {
        std::lock_guard<std::recursive_mutex> lock(versionsMutex);
        versions.erase(name);

        std::lock_guard<std::recursive_mutex> lock2(listenersMutex);
        for (auto *l: listeners) l->versionRemoved(*this, name);
    }

    void Master::setVersionThrottle(const std::string &name, double d)
    {
        std::lock_guard<std::recursive_mutex> lock(versionsMutex);
        auto it = versions.find(name);
        if (it != versions.end())
            it->second.throttle = d;
	}
	
	void Master::setNodeThrottle(const std::string &name, double d)
	{
		std::lock_guard<std::recursive_mutex> lock(nodeThrottlesMutex);
		nodeThrottles[name] = d;
	}

    std::vector<std::string> Master::getAllVersionNames()
    {
        std::vector<std::string> ret;

        std::lock_guard<std::recursive_mutex> lock(versionsMutex);
        for (const auto& ver: versions)
            ret.push_back(ver.second.name);

        return ret;
    }
	
	std::vector<std::shared_ptr<MasterNode>> Master::getAllNodes()
	{
		std::vector<std::shared_ptr<MasterNode>> ret;
		
		std::lock_guard<std::recursive_mutex> lock(nodesMutex);
		for (const auto& e: nodes) {
			ret.push_back(e.second);
		}
		
		return ret;
	}
	
	boost::optional<std::pair<std::shared_ptr<MasterNode>, std::string>>
	Master::bindClientToDomain(const std::shared_ptr<MasterClient>& client)
	{
		using RetType = std::pair<std::shared_ptr<MasterNode>, std::string>;
		
		auto room = client->room();
		
		std::vector<Balancer<RetType>::Item> items;
		{
			std::lock_guard<std::recursive_mutex> lock(nodeConnectionsMutex);
			std::lock_guard<std::recursive_mutex> lock2(nodeThrottlesMutex);
			std::lock_guard<std::recursive_mutex> lock3(versionsMutex);
			for (const auto& c: nodes) {
				auto node = c.second;
				if (!node->isConnected())
					continue;
				
				// Has room?
				auto verOrNone = node->findDomainForRoom(room);
				if (verOrNone) {
					auto ver = *verOrNone;
					if (client->doesAcceptVersion(ver))
						return RetType(node, ver);
				}
				
				// Get node throttle value
				auto it = nodeThrottles.find(node->nodeInfo().nodeName);
				if (it == nodeThrottles.end())
					continue;
				
				double nodeThrottle = it->second;
				if (nodeThrottle <= 0.0)
					continue;
				
				// For every domains of the node...
				for (const auto& domain: node->domainStatuses()) {
					auto it2 = versions.find(domain.versionName);
					if (it2 == versions.end())
						continue;
					
					// Get version throttle value
					double versionThrottle = it2->second.throttle;
					if (versionThrottle <= 0.0)
						continue;
					
					// Client accepts the version?
					if (!client->doesAcceptVersion(it2->first))
						continue;
					
					// Add as balancer item
					Balancer<RetType>::Item item;
					
					item.key = RetType(node, domain.versionName);
					item.current = static_cast<double>(domain.numClients);
					item.desired = nodeThrottle * versionThrottle;
					
					items.emplace_back(std::move(item));
				}
			}
		}
		
		// Invoke balancer
		return Balancer<RetType>().performBalancing(items);
	}
}

extern "C" MSCResult MSCMasterCreate(MSCLibrary library,
        MSCMasterParameters *params, MSCMaster *master)
{
    return mcore::convertExceptionsToResultCode([&] {
        *master = nullptr;
		
		if (!master)
			MSCThrow(mcore::InvalidArgumentException("master"));
        if (library == nullptr) {
            MSCThrow(mcore::InvalidArgumentException("library"));
        }
        if (params == nullptr) {
            MSCThrow(mcore::InvalidArgumentException("params"));
        }

        auto *m = new mcore::Master(*mcore::Library::fromHandle(library), *params);
        *master = m->handle();
    });
}

extern "C" MSCResult MSCMasterDestroy(MSCMaster master)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (!master)
			MSCThrow(mcore::InvalidArgumentException("master"));
        delete mcore::Master::fromHandle(master);
    });
}

extern "C" MSCResult MSCMasterAddVersion(MSCMaster master, const char *versionName)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (!master)
			MSCThrow(mcore::InvalidArgumentException("master"));
        mcore::Master::fromHandle(master)->addVersion(versionName);
    });
}
extern "C" MSCResult MSCMasterRemoveVersion(MSCMaster master, const char *versionName)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (!master)
			MSCThrow(mcore::InvalidArgumentException("master"));
        mcore::Master::fromHandle(master)->removeVersion(versionName);
    });
}
extern "C" MSCResult MSCMasterSetVersionThrottle(MSCMaster master,
												 const char *versionName, double throttle)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (!master)
			MSCThrow(mcore::InvalidArgumentException("master"));
        mcore::Master::fromHandle(master)->setVersionThrottle(versionName, throttle);
    });
}
extern "C" MSCResult MSCMasterSetNodeThrottle(MSCMaster master,
											  const char *nodeName, double throttle)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (!master)
			MSCThrow(mcore::InvalidArgumentException("master"));
		mcore::Master::fromHandle(master)->setNodeThrottle(nodeName, throttle);
	});
}
extern "C" MSCResult MSCMasterEnumerateNodes(MSCMaster master,
									 MSCMasterEnumerateNodesNodeCallback nodeCallback,
									 MSCMasterEnumerateNodesDomainCallback domainCallback,
									 void *userdata)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (!master)
			MSCThrow(mcore::InvalidArgumentException("master"));
		auto *m = mcore::Master::fromHandle(master);
		for (const auto& node: m->getAllNodes()) {
			if (nodeCallback != nullptr) {
				const auto& info = node->nodeInfo();
				MSCNodeStatus s;
				s.nodeName = info.nodeName.c_str();
				s.serverSoftwareName = info.serverSoftwareName.c_str();
				s.uptime = node->uptime();
				
				auto hn = node->hostNameString();
				s.hostName = hn.c_str();
				nodeCallback(&s, userdata);
			}
			if (domainCallback != nullptr) {
				for (const auto& info: node->domainStatuses()) {
					MSCDomainStatus s;
					s.nodeName = node->nodeInfo().nodeName.c_str();
					s.versionName = info.versionName.c_str();
					s.numRooms = static_cast<std::uint32_t>(info.numRooms);
					s.numClients = static_cast<std::uint32_t>(info.numClients);
					s.uptime = info.uptime;
					domainCallback(&s,
								   userdata);
				}
			}
		}
	});
}
