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

    MasterClient::MasterClient(Master& master, std::uint64_t id,
							   bool allowSpecifyVersion):
	clientId(id),
	service(master.library()->ioService()),
	sslContext(master.sslContext()),
	sslSocket(service, sslContext),
	disposed(false),
	timeoutTimer(service),
	allowSpecifyVersion(allowSpecifyVersion)
    {
    }

    MasterClient::~MasterClient()
    {
    }

    void MasterClient::didAccept()
    {
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
        accepted = true;
		
		log.setChannel(str(format("Client:%d [%s]") % clientId % tcpSocket().remote_endpoint()));
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Client connected from " << tcpSocket().remote_endpoint();
		
        sslSocket.async_handshake(sslSocketType::server, 
        [this, self](const boost::system::error_code& error) {
            handshakeDone(error);
        });
        
        timeoutTimer.expires_from_now(boost::posix_time::seconds(5));
        timeoutTimer.async_wait([this, self](const boost::system::error_code& error) {
            if (error)
                return;
			BOOST_LOG_SEV(log, LogLevel::Debug) << "Timed out. Disconnecting.";
            shutdown();
        });
    }
    
    void MasterClient::handshakeDone(const boost::system::error_code &error)
	{
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
		
		if (error) {
			BOOST_LOG_SEV(log, LogLevel::Debug) << "TLS handshake failed. Disconnecting.: " << error;
            shutdown();
            return;
        }
        
        timeoutTimer.cancel();
        
        // Read prologue
        auto buf = std::make_shared<std::array<char, 2048>>();
        asio::async_read(sslSocket, asio::buffer(buf->data(), buf->size()),
        [this, buf, self](const boost::system::error_code& error, std::size_t readCount) {
			
			auto self = shared_from_this();
			std::lock_guard<std::recursive_mutex> lock(mutex);
			
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Debug) << "Reading prologue failed. Disconnecting.: " << error;
                shutdown();
                return;
            }
            
            try {
				// Read prologue.
                PacketReader reader {buf->data(), buf->size()};
                auto magic = reader.read<std::uint32_t>();
                if (magic != ClientHeaderMagic) {
                    MSCThrow(InvalidDataException(str(format("Invalid header magic 0x%08x.") % magic)));
                }
				
				_room = reader.readString();
				
				std::string requestedVersion = reader.readString();
				
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
				
				// Request a node to start a connection.
				timeoutTimer.expires_from_now(boost::posix_time::seconds(5));
				timeoutTimer.async_wait([resp](const boost::system::error_code& error) {
					if (error)
						return;
					
					resp->reject("Timed out.");
				});
				
				onNeedsResponse(resp);
				
				// Wait for response...
				
			} catch (const InvalidDataException& ex) {
				BOOST_LOG_SEV(log, LogLevel::Debug) << "Protocol error. Disconnecting.: " <<
				boost::current_exception_diagnostic_information();
				
				respondStatus(ClientResponse::ProtocolError,
							  [this, self](const boost::system::error_code&) {
								  shutdown();
							  });
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Warn) << "Internal server error. Disconnecting.: " <<
				boost::current_exception_diagnostic_information();
				
				respondStatus(ClientResponse::InternalServerError,
							  [this, self](const boost::system::error_code&) {
								  shutdown();
							  });
			}
        });
    }
	
	void MasterClient::connectionRejected()
	{
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
		
		BOOST_LOG_SEV(log, LogLevel::Warn) << "Rejecting the client.";
		
		respondStatus(ClientResponse::ServerFull,
					  [this, self](const boost::system::error_code&) {
						  shutdown();
					  });
	}
	
	void MasterClient::connectionApproved(std::function<void(sslSocketType&, ioMutexType&)> onsuccess,
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
		
		respondStatus(ClientResponse::Success,
		  [this, self, onsuccess, onfail](const boost::system::error_code &error) {
			  if (error) {
				  BOOST_LOG_SEV(log, LogLevel::Info) << "Failed to send 'Success' response. Disconnecting.";
				  onfail();
				  shutdown();
			  } else {
				  onsuccess(sslSocket, mutex);
				  timeoutTimer.cancel();
			  }
		  });
	}
	
	template <class Callback>
	void MasterClient::respondStatus(ClientResponse resp, Callback callback)
	{
		auto self = shared_from_this();
		std::lock_guard<ioMutexType> lock(mutex);
		
		auto buf = std::make_shared<ClientResponse>(resp);
		asio::async_write(sslSocket, asio::buffer(buf.get(), sizeof(ClientResponse)),
		[this, buf, self, callback](const boost::system::error_code& error, std::size_t) {
			callback(error);
		});
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
		std::lock_guard<ioMutexType> lock(mutex);
		
        if (disposed)
            return;
        disposed = true;
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Shutting down.";
		
		onShutdown();
		
		try { sslSocket.shutdown(); } catch (...) { }
		try { tcpSocket().shutdown(socketType::shutdown_both); } catch (...) { }
        tcpSocket().close();
		
		timeoutTimer.cancel();
    }
	
	MasterClientResponse::MasterClientResponse(const MasterClient::ptr &client):
	_client(client)
	{ }
	
	MasterClientResponse::~MasterClientResponse()
	{
		reject("No one responded to MasterClientResponse.");
	}
	
	void MasterClientResponse::accept(std::function<void (MasterClient::sslSocketType &, MasterClient::ioMutexType&)> onsuccess,
									  std::function<void ()> onfail,
									  const std::string& version)
	{
		auto d = done.exchange(true);
		if (d) {
			onfail();
			return;
		}
		_client->connectionApproved(onsuccess, onfail, version);
		
		onResponded(true);
	}
	
	void MasterClientResponse::reject(const std::string& reason)
	{
		auto d = done.exchange(true);
		if (d) {
			return;
		}
		
		BOOST_LOG_SEV(_client->log, LogLevel::Warn) <<
		reason;
		_client->connectionRejected();
		
		onResponded(false);
	}
	
	bool MasterClientResponse::isResponded()
	{
		return done;
	}
	
}

