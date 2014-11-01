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
#include "NodeClient.hpp"
#include "NodeDomain.hpp"
#include "Node.hpp"
#include "Library.hpp"
#include "Exceptions.hpp"
#include "AsyncPipe.hpp"
#include "NodeClientSocket.hpp"

using boost::format;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
using pipes = asio::local::stream_protocol;

namespace mcore
{
	NodeClient::NodeClient(const std::shared_ptr<NodeDomain> &domain,
						   std::uint64_t clientId,
						   const std::string& room):
	domain(domain),
	_clientId(clientId),
	state(State::NotAccepted),
	room(room),
	masterSocket(domain->node().library()->ioService()),
	appSocket(domain->node().library()->ioService()),
	pipe {0, 0},
	_strand {domain->node().library()->ioService()},
	closed {false},
	library {domain->node().library()},
	displayName {str(format("Client:%d") % clientId)}
	{
		log.setChannel(displayName);
	}
	
	bool NodeClient::accept()
	{
		auto self = shared_from_this();
			
		assert(state == State::NotAccepted);
		
		state = State::WaitingForApplication;
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Started accepting client.";
		
		auto dom = domain.lock();
		if (!dom) {
			BOOST_LOG_SEV(log, LogLevel::Debug) << "Domain is already destroyed. Rejecting.";
			reject();
			return false;
		}
		
		Node& node = dom->node();
		const auto& param = node.parameters();
		auto createFunc = param.acceptClientFunction;
		auto destroyFunc = param.destroyClientFunction;
		
		auto lib = node.library();
		assert(lib != nullptr);
		
		lib->ioService().post([this, createFunc, destroyFunc, dom, self]() {
			
			BOOST_LOG_SEV(log, LogLevel::Debug) << "Dispatching the application to accept the client.";
			
			try {
				createFunc(clientId(), dom->version(), room);
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Warn)
				<< format("Could not accept client because of an error.:")
				<< boost::current_exception_diagnostic_information();
				
				_strand.post([this, self] {
					if (state == State::WaitingForApplication) {
						reject();
					} else {
						assert(state == State::Closed);
					}
				});
				return;
			}
			
			_strand.post([this, self, destroyFunc] {
				if (state == State::Closed) {
					// Rollback.
					BOOST_LOG_SEV(log, LogLevel::Debug) << "Cancelling the creation of the client handler.";
					try {
						destroyFunc(clientId());
					} catch (...) {
						// Very problematic...
						BOOST_LOG_SEV(log, LogLevel::Error)
						<< format("ROLLBACK FAILURE: Failed to destroying a client handler after being disconnected.")
						<< boost::current_exception_diagnostic_information();
					}
					return;
				} else {
					assert(state == State::WaitingForApplication);
					
					connectToMaster();
				}
			});
			
		});

		return true;
	}
	
	void NodeClient::reject()
	{
		auto self = shared_from_this();
		_strand.post([this, self] {
			assert(state == State::NotAccepted ||
				   state == State::WaitingForApplication);
			
			state = State::Closed;
			closed.store(true);
			
			onConnectionRejected(self);
			onClosed(self);
		});
	}
	
	void NodeClient::connectToMaster()
	{
		auto self = shared_from_this();
		assert(state == State::WaitingForApplication);
		
		auto dom = domain.lock();
		assert(dom != nullptr);
		
		state = State::ConnectingToMaster;
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Connecting to the master server.";
		
		Node& node = dom->node();
		const auto& param = node.parameters();
		auto setupFunc = param.setupClientFunction;
		auto destroyFunc = param.destroyClientFunction;
		auto endpoint = node.masterEndpoint();
		
		masterSocket.async_connect(endpoint, _strand.wrap([this, self, setupFunc, destroyFunc, dom](const boost::system::error_code& error) {
			
			try {
				if (error) {
					MSCThrow(boost::system::system_error(error));
				}
				
				BOOST_LOG_SEV(log, LogLevel::Debug) << "Connected to the master server.";
				
				asio::write(masterSocket, asio::buffer(&DataStreamMagic, 4));
				asio::write(masterSocket, asio::buffer(&_clientId, 8));
				
				if (state == State::Closed) {
					BOOST_LOG_SEV(log, LogLevel::Debug) << "...but cancelled.";
					masterSocket.shutdown(ip::tcp::socket::shutdown_both);
					masterSocket.close();
					try {
						destroyFunc(clientId());
					} catch (...) {
						// Very problematic...
						BOOST_LOG_SEV(log, LogLevel::Error)
						<< format("ROLLBACK FAILURE: Failed to destroying a client handler after being disconnected.")
						<< boost::current_exception_diagnostic_information();
					}
					
					state = State::Closed;
					return;
				}
				
				assert(state == State::ConnectingToMaster);
				
				state = State::SettingUpApplication;
				
				BOOST_LOG_SEV(log, LogLevel::Debug) << "Creating UNIX pipes.";
				
				if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe))
					throw SystemErrorException(str(format("Error while creating pipe.: %d") % errno));
				
				appSocket.assign(pipes(), pipe[0]);
				pipe[0] = 0; // ownership moved
				
				// Create client socket
				auto csock = std::make_shared<NodeClientSocket>(library, displayName, pipe[1]);
				pipe[1] = 0; // ownership moved
				setupFunc(clientId(), csock);
				
				state = State::Servicing;
				startService();
				
			} catch (...) {
				
				if (state == State::SettingUpApplication) {
					BOOST_LOG_SEV(log, LogLevel::Error) << "Failed to setup a client handler.: " <<
					boost::current_exception_diagnostic_information();
					
					try { masterSocket.shutdown(ip::tcp::socket::shutdown_both); } catch (...) { }
					masterSocket.close();
					
				} else {
					BOOST_LOG_SEV(log, LogLevel::Error) << "Failed to establish a connection.: " <<
					boost::current_exception_diagnostic_information();
					onConnectionRejected(self);
				}
				
				if (pipe[0]) close(pipe[0]);
				if (pipe[1]) close(pipe[1]);
				
				if (appSocket.is_open()) appSocket.close();
				
				try {
					destroyFunc(clientId());
				} catch (...) {
					// Very problematic...
					BOOST_LOG_SEV(log, LogLevel::Error)
					<< format("ROLLBACK FAILURE: Failed to destroying a client handler after being disconnected.")
					<< boost::current_exception_diagnostic_information();
				}
				
				state = State::Closed;
				closed.store(true);
				onClosed(self);
			}
		}));
		
	}
	
	void NodeClient::cancelConnectToMaster()
	{
		auto self = shared_from_this();
		assert(state == State::ConnectingToMaster);
		
		state = State::Closed;
		try { masterSocket.close(); } catch (...) { }
		masterSocket.cancel();
	}
	
	void NodeClient::startService()
	{
		auto self = shared_from_this();
		assert(state == State::Servicing);
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Starting service.";
		
		startAsyncPipe(masterSocket, appSocket, 8192, _strand.wrap([this, self](const boost::system::error_code& error, std::uint64_t) {
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Debug) << "Downstream stream error.: " << error;
				terminateService();
			} else {
				try { appSocket.close(); } catch(...) { }
				socketDown();
			}
		}));
		startAsyncPipe(appSocket, masterSocket, 8192, _strand.wrap([this, self](const boost::system::error_code& error, std::uint64_t) {
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Debug) << "Upstream stream error.: " << error;
				terminateService();
			} else {
				try { appSocket.close(); } catch(...) { }
				socketDown();
			}
		}));
		
	}
	
	void NodeClient::socketDown()
	{
		auto self = shared_from_this();
		if (!appSocket.is_open()) {
			BOOST_LOG_SEV(log, LogLevel::Debug) << "Pipe closed.";
			terminateService();
		}
	}
	
	void NodeClient::terminateService()
	{
		auto self = shared_from_this();
		//assert(state == State::Servicing);
		
		if (appSocket.is_open()) appSocket.close();
		
		try { masterSocket.shutdown(ip::tcp::socket::shutdown_both); } catch (...) { }
		try { masterSocket.close(); } catch(...) { }
		
		closed.store(true);
		onClosed(self);
	}
	
	void NodeClient::drop()
	{
		auto self = shared_from_this();
		_strand.post([this, self] {
			switch (state) {
				case State::NotAccepted:
				case State::WaitingForApplication:
					reject();
					break;
				case State::ConnectingToMaster:
					cancelConnectToMaster();
					break;
				case State::SettingUpApplication:
					assert(false);
					break;
				case State::Servicing:
					terminateService();
					break;
				case State::Closed:
					break;
			}
		});
	}
	
	NodeClient::~NodeClient()
	{
		
	}
	
}
