#include "Prefix.pch"
#include "MasterClient.hpp"
#include "AsyncPipe.hpp"
#include "Master.hpp"
#include "Library.hpp"
#include "Packet.hpp"
#include "Protocol.hpp"
#include <boost/format.hpp>
#include "MasterNode.hpp"

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;

using boost::format;

namespace mcore
{

    MasterClient::MasterClient(Master& master, std::uint64_t id):
            _master(master),
            clientId(id),
            service(master.library()->ioService()),
            sslContext(master.sslContext()),
            sslSocket(service, sslContext),
            disposed(false),
            timeoutTimer(service)
    {
    }

    MasterClient::~MasterClient()
    {
    }

    void MasterClient::didAccept()
    {
        auto self = shared_from_this();
		std::lock_guard<std::recursive_mutex> lock(mutex);
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
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
		if (error) {
			BOOST_LOG_SEV(log, LogLevel::Debug) << "TLS handshake failed. Disconnecting.: " << error;
            shutdown();
            return;
        }
        
        timeoutTimer.cancel();
        
        // Read prologue
        auto buf = std::make_shared<std::array<char, 2048>>();
        asio::async_read(sslSocket, asio::buffer(*buf),
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
				
                auto room = reader.readString();
				
				// Use balancer to select a server.
				auto domain = master().bindClientToDomain(room);
				
				if (domain == boost::none) {
					BOOST_LOG_SEV(log, LogLevel::Warn) << "Could not find a suitable domain. Overload possible. Disconnecting.";
					respondStatus(ClientResponse::ServerFull,
					[this, self](const boost::system::error_code&) {
						shutdown();
					});
					return;
				}
				
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				format("Bound to '%s' at '%s'.") % domain->second % domain->first->nodeInfo().nodeName;
				
				// Request a node to start a connection.
				timeoutTimer.expires_from_now(boost::posix_time::seconds(5));
				timeoutTimer.async_wait([this, self](const boost::system::error_code& error) {
					if (error)
						return;
					
					BOOST_LOG_SEV(log, LogLevel::Debug) << "Timed out. Disconnecting.";
					respondStatus(ClientResponse::ServerFull,
								  [this, self](const boost::system::error_code&) {
									  shutdown();
								  });
				});
				
				version = domain->second;
				domain->first->sendClientConnected(clientId,
												   version, room);
				
				// Node will call connectionApproved or connectionRejected...
				
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
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
		BOOST_LOG_SEV(log, LogLevel::Info) << "Rejecting the client.";
		
		respondStatus(ClientResponse::InternalServerError,
					  [this, self](const boost::system::error_code&) {
						  shutdown();
					  });
	}
	
	void MasterClient::connectionApproved(std::function<void(sslSocketType&)> onsuccess,
										  std::function<void()> onfail)
	{
		
		auto self = shared_from_this();
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
		if (disposed) {
			onfail();
			return;
		}
		
		BOOST_LOG_SEV(log, LogLevel::Info) << "Connection approved.";
		
		respondStatus(ClientResponse::Success,
		  [this, self, onsuccess, onfail](const boost::system::error_code &error) {
			  if (error) {
				  BOOST_LOG_SEV(log, LogLevel::Info) << "Failed to send 'Success' response. Disconnecting.";
				  onfail();
				  shutdown();
			  } else {
				  onsuccess(sslSocket);
				  timeoutTimer.cancel();
			  }
		  });
	}
	
	template <class Callback>
	void MasterClient::respondStatus(ClientResponse resp, Callback callback)
	{
		auto self = shared_from_this();
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
		auto buf = std::make_shared<ClientResponse>(resp);
		asio::async_write(sslSocket, asio::buffer(buf.get(), sizeof(ClientResponse)),
		[this, buf, self, callback](const boost::system::error_code& error, std::size_t) {
			callback(error);
		});
	}
    
    void MasterClient::shutdown()
	{
		auto self = shared_from_this();
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
        if (disposed)
            return;
        disposed = true;
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Shutting down.";
		
		master().removeClient(clientId);
		try { sslSocket.shutdown(); } catch (...) { }
		try { tcpSocket().shutdown(socketType::shutdown_both); } catch (...) { }
        tcpSocket().close();
		
		timeoutTimer.cancel();
    }
}

