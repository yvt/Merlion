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
#include "MasterClient.hpp"
#include "AsyncPipe.hpp"
#include "Master.hpp"
#include "Library.hpp"
#include "Packet.hpp"
#include "Protocol.hpp"
#include <boost/format.hpp>
#include "MasterNode.hpp"
#include "LikeMatcher.hpp"

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;

using boost::format;

namespace mcore
{

	namespace
	{
		std::string decodeRoomName(const std::string& encoded)
		{
			auto hex_decode = [] (char c) -> int
			{
				if (c >= '0' && c <= '9')
					return c - '0';
				else if (c >= 'a' && c <= 'f')
					return c - 'a' + 10;
				else if (c >= 'A' && c <= 'F')
					return c - 'A' + 10;
				else
					return -1;
			};
			
			if (encoded.size() & 1)
				MSCThrow(InvalidFormatException("Length of the encoded room name must be even."));
			
			std::string ret;
			ret.resize(encoded.size() >> 1);
			
			for (std::size_t i = 0; i < ret.size(); ++i) {
				int digit1 = hex_decode(encoded[i * 2]);
				int digit2 = hex_decode(encoded[i * 2 + 1]);
				if (digit1 != -1 && digit2 != -1) {
					char ch = static_cast<char>((digit1 << 4) | digit2);
					ret[i] = ch;
				} else {
					MSCThrow(InvalidFormatException("Encoded room name contains an invalid character."));
				}
			}
			
			return ret;
		}
	}
	
    MasterClient::MasterClient(Master& master, std::uint64_t id,
							   bool allowSpecifyVersion):
	clientId(id),
	service(master.library()->ioService()),
	_strand(service),
	sslContext(master.sslContext()),
	sslSocket(service, sslContext),
	webSocketServer(sslSocket),
	disposed(false),
	timeoutTimer(service),
	allowSpecifyVersion(allowSpecifyVersion)
    {
    }

    MasterClient::~MasterClient()
	{
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Finalized.";
    }

    void MasterClient::handleNewClient()
    {
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
        accepted = true;
		
		log.setChannel(str(format("Client:%d [%s]") % clientId % tcpSocket().remote_endpoint()));
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Client connected from " << tcpSocket().remote_endpoint();
		
        sslSocket.async_handshake(sslSocketType::server, _strand.wrap([this, self](const boost::system::error_code& error) {
            handshakeDone(error);
        }));
        
        timeoutTimer.expires_from_now(boost::posix_time::seconds(5));
        timeoutTimer.async_wait(_strand.wrap([this, self](const boost::system::error_code& error) {
            if (error)
                return;
			BOOST_LOG_SEV(log, LogLevel::Debug) << "Timed out. Disconnecting.";
            shutdown();
        }));
    }
	
	void MasterClient::rejectHandshake(int status)
	{
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
		
		try {
			webSocketServer.async_reject(status,
										 _strand.wrap([this, self] (const boost::system::error_code&) {
				auto self = shared_from_this();
				std::lock_guard<std::recursive_mutex> lock(mutex);
				shutdown();
			}));
		} catch (...) {
			BOOST_LOG_SEV(log, LogLevel::Error) <<
			"Sending error response failed.: " <<
			boost::current_exception_diagnostic_information();
			shutdown();
		}
	}
	
	static std::regex urlRegex {
		"\\/([^/]*)\\/([^/]*)"
	};
	
    void MasterClient::handshakeDone(const boost::system::error_code &error)
	{
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
		
		if (error) {
			BOOST_LOG_SEV(log, LogLevel::Debug) << "TLS handshake failed. Disconnecting.: " << error;
            shutdown();
            return;
        }
		
		webSocketServer.async_start_handshake(_strand.wrap([this, self] (const boost::system::error_code& error) {
			
			auto self = shared_from_this();
			std::lock_guard<std::recursive_mutex> lock(mutex);
			
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Debug) << "WebSocket Connect Handshake failed. Disconnecting.: " << error;
				shutdown();
				return;
			}
			
			try {
				
				// Read subprotocols.
				std::vector<std::string> protos;
				boost::split(protos, webSocketServer.encoded_protocols(),
							 [] (char c) { return c == ','; },
							 boost::token_compress_on);
				
				bool valid = false;
				for (auto& proto: protos) {
					boost::trim(proto);
					if (boost::iequals(proto, "merlion.yvt.jp")) {
						valid = true;
						break;
					}
				}
				
				if (!valid) {
					// Protocol negotiation failed.
					BOOST_LOG_SEV(log, LogLevel::Debug) << "WebSocket subprotocol negotiation failed. Disconnecting.";
					rejectHandshake(asiows::http_status_codes::bad_request);
					return;
				}
				
				std::string path = webSocketServer.http_absolute_path().path;
				std::smatch matches;
				
				if (!std::regex_match(path, matches, urlRegex)) {
					BOOST_LOG_SEV(log, LogLevel::Debug) << "Unrecognizable path format. Disconnecting.: " <<
					boost::current_exception_diagnostic_information();
					rejectHandshake(asiows::http_status_codes::not_found);
					return;
				}
				
				try {
					_room = decodeRoomName(matches[2].str());
				} catch (...) {
					BOOST_LOG_SEV(log, LogLevel::Debug) << "Decoding room name failed. Disconnecting.: " <<
					boost::current_exception_diagnostic_information();
					rejectHandshake(asiows::http_status_codes::bad_request);
				}
				
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				format("Client requests room '%s' (hex encoded).") % matches[2].str();
				
				std::string requestedVersion = matches[1].str();
				
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				format("Client requests version '%s'.") % requestedVersion;
				
				if (allowSpecifyVersion) {
					versionRequest.reset(new LikeMatcher(requestedVersion));
					BOOST_LOG_SEV(log, LogLevel::Debug) <<
					"Version request string parsed.";
				} else {
					BOOST_LOG_SEV(log, LogLevel::Debug) <<
					"Version request is disallowed by the server configuration.";
				}
				
				std::shared_ptr<MasterClientResponse> resp(new MasterClientResponse(shared_from_this()));
				
				timeoutTimer.cancel();
				
				// Request a node to start a connection.
				timeoutTimer.expires_from_now(boost::posix_time::seconds(5));
				timeoutTimer.async_wait([resp](const boost::system::error_code& error) {
					if (error)
						return;
					
					resp->reject("Timed out.",
								 asiows::http_status_codes::request_timeout);
				});
				
				onNeedsResponse(resp);
				
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Warn) << "Internal server error. Disconnecting.: " <<
				boost::current_exception_diagnostic_information();
				rejectHandshake(asiows::http_status_codes::internal_server_error);
			}
			
		}));
    }
	
	void MasterClient::connectionRejected(int statusCode)
	{
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
		
		BOOST_LOG_SEV(log, LogLevel::Warn) << "Rejecting the client.";
		
		rejectHandshake(statusCode);
	}
	
	void MasterClient::connectionApproved(std::function<std::shared_ptr<BaseMasterClientHandler>()> onsuccess,
										  std::function<void()> onfail,
										  const std::string& version)
	{
		
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
		
		if (disposed) {
			onfail();
			return;
		}
		
		this->version = version;
		
		BOOST_LOG_SEV(log, LogLevel::Info) << "Connection approved.";
		
		webSocketServer.async_accept("merlion.yvt.jp", _strand.wrap([this, self, onsuccess, onfail] (const boost::system::error_code &error) {
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Info) << "Failed to send 'Success' response. Disconnecting.";
				onfail();
				shutdown();
			} else {
				auto gotHandler = onsuccess();
				handler = gotHandler;
				try {
					gotHandler->handleClient(shared_from_this());
				} catch (...) {
					BOOST_LOG_SEV(log, LogLevel::Error) << "Error while accepting client.: " <<
					boost::current_exception_diagnostic_information();
					onfail();
					shutdown();
					return;
				}
				timeoutTimer.cancel();
			}
		}));
		
	}
	
	bool MasterClient::doesAcceptVersion(const std::string &ver)
	{
		if (!allowSpecifyVersion)
			return true;
		return versionRequest->match(ver);
	}
    
    void MasterClient::shutdown()
	{
		auto self = shared_from_this();
		_strand.dispatch([self, this] {
			std::lock_guard<ioMutexType> lock(mutex);
			
			if (disposed)
				return;
			disposed = true;
			
			BOOST_LOG_SEV(log, LogLevel::Debug) << "Shutting down.";
			
			onShutdown();
			
			try { sslSocket.shutdown(); } catch (...) { }
			try { tcpSocket().shutdown(socketType::shutdown_both); } catch (...) { }
			try { tcpSocket().close(); } catch (...) { }
			assert(!tcpSocket().is_open());
			
			auto h = handler.lock();
			if (h) {
				h->shutdown();
			}
			handler.reset();
			
			timeoutTimer.cancel();
		});
    }
	
	MasterClientResponse::MasterClientResponse(const MasterClient::ptr &client):
	_client(client),
	done(false)
	{ }
	
	MasterClientResponse::~MasterClientResponse()
	{
		reject("No one responded to MasterClientResponse.");
	}
	
	void MasterClientResponse::accept(std::function<std::shared_ptr<BaseMasterClientHandler>()> onsuccess,
									  std::function<void ()> onfail,
									  const std::string& version)
	{
		auto d = done.exchange(true);
		if (d) {
			onfail();
			return;
		}
		
		auto cli = _client;
		cli->_strand.dispatch([cli, onsuccess, onfail, version] {
			cli->connectionApproved(onsuccess, onfail, version);
		});
		
		onResponded(true);
	}
	
	void MasterClientResponse::reject(const std::string& reason, int statusCode)
	{
		auto d = done.exchange(true);
		if (d) {
			return;
		}
		
		BOOST_LOG_SEV(_client->log, LogLevel::Warn) <<
		reason;
		
		auto cli = _client;
		cli->_strand.dispatch([cli, statusCode] {
			cli->connectionRejected(statusCode);
		});
		
		onResponded(false);
	}
	
	bool MasterClientResponse::isResponded()
	{
		return done;
	}
	
}

