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

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
using boost::format;

namespace mcore
{

    MasterParameters::MasterParameters(MSCMasterParameters const &param)
    {
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
		
		/*if (param.packageDownloadPathCallback == nullptr) {
			MSCThrow(InvalidArgumentException("packageDownloadPathCallback"));
		}
		auto *packageDownloadPathCallback = param.packageDownloadPathCallback;
		auto *packageDownloadPathCallbackData = param.packageDownloadPathCallbackUserData;
		getPackageDownloadPathFunction = [=](const std::string& versionName) {
			std::array<char, 512> buffer;
			buffer[511] = 0;
			if (packageDownloadPathCallback(versionName.c_str(), buffer.data(), 511, packageDownloadPathCallbackData) != 0)
				return std::string();
			return std::string(buffer.data());
		};
		
		if (param.deployPackageCallback == nullptr) {
			MSCThrow(InvalidArgumentException("deployPackageCallback"));
		}
		auto *deployPackageCallback = param.deployPackageCallback;
		auto *deployPackageCallbackData = param.deployPackageCallbackUserData;
		deployPackageFunction = [=](const std::string& versionName) {
			if (deployPackageCallback(versionName.c_str(), deployPackageCallbackData)) {
				MSCThrow(InvalidDataException("Deploying package failed."));
			}
		};*/
    }

    Master::Master(const std::shared_ptr<Library>& library, const MasterParameters& parameters):
            _library(library),
            _parameters(parameters),
            nodeAcceptor(library->ioService(), parseTcpEndpoint(parameters.nodeEndpoint)),
            clientAcceptor(library->ioService(), parseTcpEndpoint(parameters.clientEndpoint)),
            heartbeatTimer(library->ioService()),
            heartbeatRunning(true),
            _sslContext(ssl::context::tlsv1),
            disposed(false)
	{
		auto pw = parameters.sslPassword;
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
        waitingNodeConnection = std::make_shared<MasterNodeConnection>(*this);
        waitingClient = std::make_shared<MasterClient>(*this, 1);

        // Ensure Master is not destroyed until async operation completes
        nodeAcceptorMutex.lock();
        clientAcceptorMutex.lock();

        acceptNodeConnectionAsync(true);
        acceptClientAsync(true);

        doHeartbeat(boost::system::error_code());
		
		BOOST_LOG_SEV(log, LogLevel::Info) << "Initialization done.";
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
        if (e) {
            return;
        }

        std::lock_guard<std::recursive_mutex> hLock(heartbeatMutex);
        if (!heartbeatRunning)
            return;
        {
            std::lock_guard<std::recursive_mutex> lock(listenersMutex);
            for (auto *l: listeners)
                l->heartbeat(*this);
        }
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
        nodeAcceptor.async_accept(waitingNodeConnection->tcpSocket(),
        [=](const boost::system::error_code& errorCode) {
            if (errorCode.value() == boost::asio::error::operation_aborted || disposed) {
                nodeAcceptorMutex.unlock();
                return;
            } else if (errorCode) {
				// FIXME: what kind of error occurs?
				if (initial) {
					BOOST_LOG_SEV(log, LogLevel::Info) <<
					format("Accepting node connection failed: %s.") % errorCode;
				}
            } else {
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
        clientAcceptor.async_accept(waitingClient->tcpSocket(),
        [=](const boost::system::error_code& errorCode) {
            if (errorCode.value() == boost::asio::error::operation_aborted || disposed) {
                nodeAcceptorMutex.unlock();
                return;
            } else if (errorCode) {
				// FIXME: what kind of error occurs?
				if (initial) {
					BOOST_LOG_SEV(log, LogLevel::Debug) <<
					format("Accepting client connection failed: %s.") % errorCode;
				}
            } else {
                auto conn = waitingClient;
                {
                    std::lock_guard<std::recursive_mutex> lock(clientsMutex);
                    clients[conn->id()] = std::move(waitingClient);
                    waitingClient = std::make_shared<MasterClient>(*this, conn->id() + 1);
                }
                conn->didAccept();
            }

            // Accept next connection
            acceptClientAsync(false);
        });
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

        nodeAcceptor.close();
        clientAcceptor.close();

        // Wait until node acceptor is closed
        nodeAcceptorMutex.lock();
        clientAcceptorMutex.lock();

        waitingNodeConnection->shutdown();
        waitingNodeConnection.reset();

        // Shut all connections down
        std::vector<MasterNodeConnection *> nodeConns;
        {
            std::lock_guard<std::recursive_mutex> lock(nodeConnectionsMutex);
            for (const auto& c: nodeConnections) {
                c->shutdown();
                nodeConns.push_back(c.get());
            }
        }
        for (auto *c: nodeConns)
            delete c;

        _library = nullptr;
    }

    Master::~Master()
    {
        invalidate();
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
        for (auto *l: listeners) l->versionAdded(*this, name);
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
		
		std::lock_guard<std::recursive_mutex> lock(nodeConnectionsMutex);
		for (const std::shared_ptr<MasterNodeConnection>& c: nodeConnections) {
			auto node = c->masterNode();
			if (node != nullptr) {
				ret.push_back(std::move(node));
			}
		}
		
		return ret;
	}
	
	boost::optional<std::pair<std::shared_ptr<MasterNode>, std::string>>
	Master::bindClientToDomain(const std::string& room)
	{
		using RetType = std::pair<std::shared_ptr<MasterNode>, std::string>;
		
		std::vector<Balancer<RetType>::Item> items;
		{
			std::lock_guard<std::recursive_mutex> lock(nodeConnectionsMutex);
			std::lock_guard<std::recursive_mutex> lock2(nodeThrottlesMutex);
			std::lock_guard<std::recursive_mutex> lock3(versionsMutex);
			for (const std::shared_ptr<MasterNodeConnection>& c: nodeConnections) {
				auto node = c->masterNode();
				if (node != nullptr) {
					auto it = nodeThrottles.find(node->nodeInfo().nodeName);
					if (it == nodeThrottles.end())
						continue;
					
					double nodeThrottle = it->second;
					if (nodeThrottle == 0.0)
						continue;
					
					for (const auto& domain: node->domainStatuses()) {
						auto it2 = versions.find(domain.versionName);
						if (it2 == versions.end())
							continue;
						
						double versionThrottle = it2->second.throttle;
						if (versionThrottle == 0.0)
							continue;
						
						Balancer<RetType>::Item item;
						
						item.key = RetType(node, domain.versionName);
						item.current = static_cast<double>(domain.numClients);
						item.desired = nodeThrottle * versionThrottle;
						
						items.emplace_back(std::move(item));
					}
				}
			}
		}
		
		return Balancer<RetType>().performBalancing(items);
	}
}

extern "C" MSCResult MSCMasterCreate(MSCLibrary library,
        MSCMasterParameters *params, MSCMaster *master)
{
    return mcore::convertExceptionsToResultCode([&] {
        *master = nullptr;

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
        delete mcore::Master::fromHandle(master);
    });
}

extern "C" MSCResult MSCMasterAddVersion(MSCMaster master, const char *versionName)
{
    return mcore::convertExceptionsToResultCode([&] {
        mcore::Master::fromHandle(master)->addVersion(versionName);
    });
}
extern "C" MSCResult MSCMasterRemoveVersion(MSCMaster master, const char *versionName)
{
    return mcore::convertExceptionsToResultCode([&] {
        mcore::Master::fromHandle(master)->removeVersion(versionName);
    });
}
extern "C" MSCResult MSCMasterSetVersionThrottle(MSCMaster master,
												 const char *versionName, double throttle)
{
    return mcore::convertExceptionsToResultCode([&] {
        mcore::Master::fromHandle(master)->setVersionThrottle(versionName, throttle);
    });
}
extern "C" MSCResult MSCMasterSetNodeThrottle(MSCMaster master,
											  const char *nodeName, double throttle)
{
	return mcore::convertExceptionsToResultCode([&] {
		mcore::Master::fromHandle(master)->setNodeThrottle(nodeName, throttle);
	});
}
extern "C" MSCResult MSCMasterEnumerateNodes(MSCMaster master,
									 MSCMasterEnumerateNodesNodeCallback nodeCallback,
									 MSCMasterEnumerateNodesDomainCallback domainCallback,
									 void *userdata)
{
	return mcore::convertExceptionsToResultCode([&] {
		auto *m = mcore::Master::fromHandle(master);
		for (const auto& node: m->getAllNodes()) {
			if (nodeCallback != nullptr) {
				nodeCallback(node->nodeInfo().nodeName.c_str(), userdata);
			}
			if (domainCallback != nullptr) {
				for (const auto& info: node->domainStatuses()) {
					domainCallback(node->nodeInfo().nodeName.c_str(),
								   info.versionName.c_str(),
								   static_cast<std::uint32_t>(info.numClients),
								   static_cast<std::uint32_t>(info.numRooms),
								   userdata);
				}
			}
		}
	});
}
