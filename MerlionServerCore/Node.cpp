#include "Prefix.pch"
#include "Node.hpp"
#include "Exceptions.hpp"
#include "Utils.hpp"
#include "Packet.hpp"
#include "Protocol.hpp"
#include "NodeClient.hpp"
#include "NodeDomain.hpp"

namespace asio = boost::asio;
using boost::format;

namespace mcore
{
	
	NodeParameters::NodeParameters(MSCNodeParameters const &param)
	{
		if (param.nodeName == nullptr) {
			MSCThrow(InvalidArgumentException("nodeName"));
		}
		nodeName = param.nodeName;
		
		if (param.masterEndpoint == nullptr) {
			MSCThrow(InvalidArgumentException("masterEndpoint"));
		}
		masterEndpoint = param.masterEndpoint;
		
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
		
		if (param.packageDownloadPathCallback == nullptr) {
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
				MSCThrow(InvalidOperationException("Deploying package failed."));
			}
		};
		
		if (param.loadVersionCallback == nullptr) {
			MSCThrow(InvalidArgumentException("loadVersionCallback"));
		}
		auto *loadVersionCallback = param.loadVersionCallback;
		auto *loadVersionCallbackData = param.loadVersionCallbackUserData;
		loadVersionFunction = [=](const std::string& versionName) {
			if (loadVersionCallback(versionName.c_str(), loadVersionCallbackData)) {
				MSCThrow(InvalidOperationException("Loading version failed."));
			}
		};
		
		if (param.unloadVersionCallback == nullptr) {
			MSCThrow(InvalidArgumentException("unloadVersionCallback"));
		}
		auto *unloadVersionCallback = param.unloadVersionCallback;
		auto *unloadVersionCallbackData = param.unloadVersionCallbackUserData;
		unloadVersionFunction = [=](const std::string& versionName) {
			if (unloadVersionCallback(versionName.c_str(), unloadVersionCallbackData)) {
				MSCThrow(InvalidOperationException("Unloading version failed."));
			}
		};
		
		if (param.acceptClientCallback == nullptr) {
			MSCThrow(InvalidArgumentException("acceptClientCallback"));
		}
		auto *acceptClientCallback = param.acceptClientCallback;
		auto *acceptClientCallbackData = param.acceptClientCallbackUserData;
		acceptClientFunction = [=](std::uint64_t clientId, const std::string& versionName, const std::string& roomName) {
			if (acceptClientCallback(clientId, versionName.c_str(), roomName.c_str(), roomName.size(), acceptClientCallbackData)) {
				MSCThrow(InvalidOperationException("Accepting client failed."));
			}
		};
		
		if (param.setupClientCallback == nullptr) {
			MSCThrow(InvalidArgumentException("setupClientCallback"));
		}
		auto *setupClientCallback = param.setupClientCallback;
		auto *setupClientCallbackData = param.setupClientCallbackUserData;
		setupClientFunction = [=](std::uint64_t clientId, const ClientSetup& setup) {
			MSCClientSetup set;
			set.readStream = setup.readStream;
			set.writeStream = setup.writeStream;
			if (setupClientCallback(clientId, &set, setupClientCallbackData)) {
				MSCThrow(InvalidOperationException("Setting client up failed."));
			}
		};
		
		if (param.discardClientCallback == nullptr) {
			MSCThrow(InvalidArgumentException("discardClientCallback"));
		}
		auto *discardClientCallback = param.discardClientCallback;
		auto *discardClientCallbackData = param.discardClientCallbackUserData;
		destroyClientFunction = [=](std::uint64_t clientId) {
			if (discardClientCallback(clientId, discardClientCallbackData)) {
				MSCThrow(InvalidOperationException("Discarding client failed."));
			}
		};
	}

	Node::Node(const std::shared_ptr<Library> &library, const NodeParameters &parameters):
	_library(library),
	_parameters(parameters),
	socket(library->ioService()),
	timeoutTimer(library->ioService()),
	reconnectTimer(library->ioService()),
	endpoint(parseTcpEndpoint(parameters.masterEndpoint)),
	down(true)
	{
		BOOST_LOG_SEV(log, LogLevel::Info) << "Starting as '" << parameters.nodeName << "'.";
		
		info.nodeName = parameters.nodeName;
		
		connectAsync();
	}
	
	void Node::onBeingDestroyed(Library &)
	{
		invalidate();
	}
	
	void Node::invalidate()
	{
		if (_library == nullptr) {
			return;
		}
		
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		shutdown();
		
		timeoutTimer.cancel();
		reconnectTimer.cancel();
		
		_library = nullptr;
		
	}
	
	void Node::checkValid() const
	{
		if (_library == nullptr)
			MSCThrow(InvalidOperationException());
	}
	
	void Node::setTimeout()
	{
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		
		timeoutTimer.cancel();
		timeoutTimer.expires_from_now(boost::posix_time::seconds(5));
		timeoutTimer.async_wait([this](const boost::system::error_code& error) {
			if (error) return;
			BOOST_LOG_SEV(log, LogLevel::Error) << "Timed out. Disconnecting.";
			
			std::lock_guard<std::recursive_mutex> lock(socketMutex);
			
			shutdown();
			scheduleReconnect();
		});
	}
	
	void Node::shutdown()
	{
		BOOST_LOG_SEV(log, LogLevel::Info) << "Shutting down.";
		
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		down = true;
		try { socket.close(); } catch (...) { }
		
		// Unload domains
		BOOST_LOG_SEV(log, LogLevel::Info) << "Unloading domains.";
		for (const auto& ver: domains) {
			BOOST_LOG_SEV(log, LogLevel::Debug) <<
			format("Unloading '%s'.") % ver.first;
			_parameters.unloadVersionFunction(ver.first);
		}
		domains.clear();
		
	}
	
	void Node::scheduleReconnect()
	{
		BOOST_LOG_SEV(log, LogLevel::Info) << "Scheduling reconnection.";
		reconnectTimer.expires_from_now(boost::posix_time::seconds(5));
		reconnectTimer.async_wait([this](const boost::system::error_code& error) {
			if (error) return;
			connectAsync();
		});
	}
	
	void Node::connectAsync()
	{
		BOOST_LOG_SEV(log, LogLevel::Info) << "Connecting to " << endpoint << ".";
		
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		
		socket.async_connect(endpoint, [this](const boost::system::error_code& error) {
			std::lock_guard<std::recursive_mutex> lock(socketMutex);
								 
			try {
				if (error) {
					MSCThrow(boost::system::system_error(error));
				}
				
				BOOST_LOG_SEV(log, LogLevel::Info) << "Connected.";
				sendHeader();
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Error) << "Failed to connect.: " <<
				boost::current_exception_diagnostic_information();
				shutdown();
				scheduleReconnect();
			}
		});
	}
	
	void Node::sendHeader()
	{
		PacketGenerator header1, header2;
		header1.write(ControlStreamHeaderMagic);
		info.serialize(header2);
		header1.write(static_cast<std::uint32_t>(header2.size()));
		
		auto buf1 = std::make_shared<std::vector<char>>(std::move(header1.vector()));
		auto buf2 = std::make_shared<std::vector<char>>(std::move(header2.vector()));
		
		std::array<asio::const_buffer, 2> buffers =
		{ asio::buffer(*buf1), asio::buffer(*buf2) };
		
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		
		asio::async_write(socket, buffers, [this, buf1, buf2](const boost::system::error_code& err, std::size_t)
	    {
			std::lock_guard<std::recursive_mutex> lock(socketMutex);
			try {
				if (err) {
					MSCThrow(boost::system::system_error(err));
				}
				
				BOOST_LOG_SEV(log, LogLevel::Info) << "NodeInfo sent. Started listening for events...";
				
				down = false;
				timeoutTimer.cancel();
				receiveCommandHeader();
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Error) << "Failed to send NodeInfo.: " <<
				boost::current_exception_diagnostic_information();
				shutdown();
				scheduleReconnect();
			}
		});
	}
	
	void Node::receiveCommandHeader()
	{
		auto buf = std::make_shared<std::uint32_t>();
		
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		asio::async_read(socket, asio::buffer(buf.get(), 4), [this, buf](const boost::system::error_code& err, std::size_t) {
			std::lock_guard<std::recursive_mutex> lock(socketMutex);
			try {
				if (err) {
					MSCThrow(boost::system::system_error(err));
				}
				
				std::size_t size = static_cast<std::size_t>(*buf);
				if (size > 1024 * 1024) {
					MSCThrow(InvalidDataException("Command is too big."));
				}
				
				receiveCommandBody(size);
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Error) << "Failed to receive command.: " <<
				boost::current_exception_diagnostic_information();
				shutdown();
				scheduleReconnect();
			}
		});
	}
	void Node::receiveCommandBody(std::size_t size)
	{
		auto buf = std::make_shared<std::vector<char>>();
		buf->resize(size);
		
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		asio::async_read(socket, asio::buffer(*buf), [this, buf](const boost::system::error_code& err, std::size_t) {
			std::lock_guard<std::recursive_mutex> lock(socketMutex);
			try {
				if (err) {
					MSCThrow(boost::system::system_error(err));
				}
				
				PacketReader reader(buf->data(), buf->size());
				
				receiveCommandHeader();
				
				while (reader.canReadBytes(1)) {
					auto cmd = reader.read<NodeCommand>();
					
					switch (cmd) {
						case NodeCommand::Nop:
							break;
						case NodeCommand::Connect:
						{
							auto clientId = reader.read<std::uint64_t>();
							auto version = reader.readString();
							auto room = reader.readString();
							
							BOOST_LOG_SEV(log, LogLevel::Debug) <<
							format("Handling client %d. (Version = '%s')") % clientId % version;
							
							try {
								auto it = domains.find(version);
								if (it == domains.end()) {
									MSCThrow(InvalidOperationException
											 (str(format("Version '%s' not found.") % version)));
								}
								auto client = std::make_shared<NodeClient>(it->second, clientId, room);
								client->onConnectionRejected.connect([this](const NodeClient::ptr& client) {
									sendRejectClient(client->clientId());
								});
								
								it->second->acceptClient(client);
							} catch (...) {
								BOOST_LOG_SEV(log, LogLevel::Warn) <<
								format("Rejecting client %d (Version = '%s') because of an error.: ") % clientId % version <<
								boost::current_exception_diagnostic_information();
								sendRejectClient(clientId);
							}
						}
							break;
						case NodeCommand::LoadVersion:
						{
							auto version = reader.readString();
							BOOST_LOG_SEV(log, LogLevel::Info) <<
							format("Loading version '%s'.") % version;
							try {
								if (domains.find(version) != domains.end()) {
									MSCThrow(InvalidOperationException("Already loaded."));
									break;
								}
								auto path = _parameters.getPackagePathFunction(version);
								if (boost::filesystem::exists(path)) {
									_parameters.loadVersionFunction(version);
									domains.emplace(version, std::make_shared<NodeDomain>(*this, version));
									domains[version]->setAcceptsClient(true);
								} else {
									// TODO: download version!
									//       current code assumes version is already downloaded.
									MSCThrow(NotImplementedException("Downloading application is not implemented."));
								}
							} catch (...) {
								BOOST_LOG_SEV(log, LogLevel::Error) <<
								format("Failed to load version '%s'.: ") % version <<
								boost::current_exception_diagnostic_information();
							}
						}
							break;
						case NodeCommand::UnloadVersion:
						{
							auto version = reader.readString();
							BOOST_LOG_SEV(log, LogLevel::Info) <<
							format("Unloading version '%s'.") % version;
							try {
								auto it = domains.find(version);
								if (it != domains.end())
									it->second->setAcceptsClient(false);
								_parameters.unloadVersionFunction(version);
								domains.erase(version);
							} catch (...) {
								BOOST_LOG_SEV(log, LogLevel::Error) <<
								format("Failed to unload version '%s'.: ") % version <<
								boost::current_exception_diagnostic_information();
							}
						}
							
							break;
					}
				}
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Error) << "Failed to send NodeInfo.: " <<
				boost::current_exception_diagnostic_information();
				shutdown();
				scheduleReconnect();
			}
		});
	}

	template <class F>
	void Node::sendCommand(const std::string& name, F generator)
	{
		PacketGenerator gen;
		gen.write<std::uint32_t>(0); // Placeholder for size
		generator(gen);
		*reinterpret_cast<std::uint32_t *>(gen.data()) = static_cast<std::uint32_t>(gen.size() - 4);
		
		auto buf = std::make_shared<std::vector<char>>(std::move(gen.vector()));
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		
		asio::async_write(socket, asio::buffer(*buf), [this, buf, name](const boost::system::error_code& err, std::size_t) {
			std::lock_guard<std::recursive_mutex> lock(socketMutex);
			try {
				if (err) {
					MSCThrow(boost::system::system_error(err));
				}
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Error) << "Failed to send " << name << ".: " <<
				boost::current_exception_diagnostic_information();
				shutdown();
				scheduleReconnect();
			}
		});
	}
	
	void Node::sendHeartbeat()
	{
		sendCommand("Nop", [&](PacketGenerator& gen) {
			gen.write(MasterCommand::Nop);
		});
	}
	void Node::sendRejectClient(std::uint64_t clientId)
	{
		sendCommand("RejectClient", [&](PacketGenerator& gen) {
			gen.write(MasterCommand::RejectClient);
			gen.write(clientId);
		});
	}
	void Node::sendVersionLoaded(const std::string& version)
	{
		sendCommand("VersionLoaded", [&](PacketGenerator& gen) {
			gen.write(MasterCommand::VersionLoaded);
			gen.writeString(version);
		});
	}
	void Node::sendVersionUnloaded(const std::string& version)
	{
		sendCommand("VersionUnloaded", [&](PacketGenerator& gen) {
			gen.write(MasterCommand::VersionUnloaded);
			gen.writeString(version);
		});
	}
	void Node::sendBindRoom(const std::string& room, const std::string& ver)
	{
		sendCommand("BindRoom", [&](PacketGenerator& gen) {
			gen.write(MasterCommand::BindRoom);
			gen.writeString(room);
			gen.writeString(ver);
		});
	}
	void Node::sendUnbindRoom(const std::string& room)
	{
		sendCommand("UnbindRoom", [&](PacketGenerator& gen) {
			gen.write(MasterCommand::UnbindRoom);
			gen.writeString(room);
		});
	}
	void Node::sendLog()
	{
		MSCThrow(NotImplementedException());
	}
	
	void Node::versionLoaded(const std::string &v)
	{
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		if (down)
			return;
		sendVersionLoaded(v);
	}
	void Node::versionUnloaded(const std::string &v)
	{
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		if (down)
			return;
		sendVersionUnloaded(v);
	}
	void Node::bindRoom(const std::string &room, const std::string &version)
	{
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		if (down)
			return;
		sendBindRoom(room, version);
	}
	void Node::unbindRoom(const std::string &room)
	{
		std::lock_guard<std::recursive_mutex> lock(socketMutex);
		if (down)
			return;
		sendUnbindRoom(room);
	}
	
	Node::~Node()
	{
		invalidate();
	}
	
}

extern "C" MSCResult MSCNodeCreate(MSCLibrary library,
								   MSCNodeParameters *params, MSCNode *node)
{
	return mcore::convertExceptionsToResultCode([&] {
		*node = nullptr;
		
		if (library == nullptr) {
			MSCThrow(mcore::InvalidArgumentException("library"));
		}
		if (params == nullptr) {
			MSCThrow(mcore::InvalidArgumentException("params"));
		}
		
		auto *m = new mcore::Node(*mcore::Library::fromHandle(library), *params);
		*node = m->handle();
	});
}

extern "C" MSCResult MSCNodeDestroy(MSCNode node)
{
	return mcore::convertExceptionsToResultCode([&] {
		delete mcore::Node::fromHandle(node);
	});
}


extern "C" MSCResult MSCNodeVersionLoaded(MSCNode node, const char *version)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (version == nullptr) {
			MSCThrow(mcore::InvalidArgumentException("version"));
		}
		auto *n = mcore::Node::fromHandle(node);
		n->versionLoaded(version);
	});
}

extern "C" MSCResult MSCNodeVersionUnloaded(MSCNode node, const char *version)
{
	return mcore::convertExceptionsToResultCode([&] {
		if (version == nullptr) {
			MSCThrow(mcore::InvalidArgumentException("version"));
		}
		auto *n = mcore::Node::fromHandle(node);
		n->versionUnloaded(version);
	});
}

