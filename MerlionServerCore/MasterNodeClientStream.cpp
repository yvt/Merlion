#include "Prefix.pch"
#include "MasterNodeClientStream.hpp"
#include "Master.hpp"
#include "MasterClient.hpp"
#include "AsyncPipe.hpp"

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
namespace ip = boost::asio::ip;
using boost::format;

namespace mcore
{
	MasterNodeClientStream::MasterNodeClientStream(const MasterNodeConnection::ptr &connection):
	connection(connection)
	{
		std::string channel = "Unknown Client Data";
		log.setChannel(channel);
		connection->setChannelName(channel);
	}
	
	MasterNodeClientStream::~MasterNodeClientStream()
	{
		
	}
	
	void MasterNodeClientStream::service()
	{
		// Read clientId
		auto self = shared_from_this();
		auto buf = std::make_shared<std::uint64_t>();
		connection->readAsync(asio::buffer(buf.get(), 8),
		[this, self, buf](const boost::system::error_code &error, std::size_t)
		{
			if (error) {
				connection->shutdown();
				return;
			}
			
			std::uint64_t clientId = *buf;
			
			std::string channel = str(format("Client:%d") % clientId);
			log.setChannel(channel);
			connection->setChannelName(channel);
			
			client = connection->master().getClient(clientId);
			
			if (client == nullptr) {
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				"Client ID was not found.";
				connection->shutdown();
				return;
			}
			
			auto c = connection;
			
			client->connectionApproved
			([this, self, c](ssl::stream<ip::tcp::socket>& stream) {
				startAsyncPipe(stream, *connection, 4096, [this, self, c](const boost::system::error_code &error, std::uint64_t) {
					if (error) {
						BOOST_LOG_SEV(log, LogLevel::Debug) <<
						"Downstream error.: " << error;
					} else {
						BOOST_LOG_SEV(log, LogLevel::Debug) <<
						"End of downstream.";
				   }
				   shutdown();
			   });
				startAsyncPipe(*connection, stream, 4096, [this, self, c](const boost::system::error_code &error, std::uint64_t) {
					if (error) {
						BOOST_LOG_SEV(log, LogLevel::Debug) <<
						"Upstream error.: " << error;
					} else {
						BOOST_LOG_SEV(log, LogLevel::Debug) <<
						"End of upstream.";
					}
				   shutdown();
			   });
			},
			[this, self]() {
				shutdown();
			});
		});
	}
	
	void MasterNodeClientStream::connectionShutdown()
	{
		
	}
	
	void MasterNodeClientStream::shutdown()
	{
		auto self = shared_from_this();
		
		if (client) {
			client->shutdown();
			client.reset();
		}
		
		connection->shutdown();
	}
}
