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
	
	namespace
	{
		template <class TStream, class TLock>
		struct synchornized_stream
		{
			TStream& stream;
			TLock& lock;
			
			synchornized_stream(TStream& stream, TLock& lock):
			stream(stream), lock(lock)
			{ }
			
			template <class... TArgs>
			void async_read_some(TArgs&& ...args)
			{
				std::lock_guard<TLock> guard(lock);
				stream.async_read_some(std::forward<TArgs>(args)...);
			}
			template <class... TArgs>
			void async_write_some(TArgs&& ...args)
			{
				std::lock_guard<TLock> guard(lock);
				stream.async_write_some(std::forward<TArgs>(args)...);
			}
		};
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
			
			auto req = connection->master().dequePendingClient(clientId);
			
			if (!req) {
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				"Client ID was not found in the master pending client table.";
				connection->shutdown();
				return;
			}
			
			client = req->response->client();
			
			auto c = connection;
			
			req->response->accept
			([this, self, c](ssl::stream<ip::tcp::socket>& stream, std::recursive_mutex& ioMutex) {
				
				auto syncStream = std::make_shared
				<synchornized_stream<ssl::stream<ip::tcp::socket>, std::recursive_mutex>>
				(stream, ioMutex);
				
				startAsyncPipe(*syncStream, *connection, 4096, [this, self, c, syncStream](const boost::system::error_code &error, std::uint64_t) {
					if (error) {
						BOOST_LOG_SEV(log, LogLevel::Debug) <<
						"Downstream error.: " << error;
					} else {
						BOOST_LOG_SEV(log, LogLevel::Debug) <<
						"End of downstream.";
				   }
				   shutdown();
			   });
				startAsyncPipe(*connection, *syncStream, 4096, [this, self, c, syncStream](const boost::system::error_code &error, std::uint64_t) {
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
			},
			req->version);
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
