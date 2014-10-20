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
	
	class MasterNodeClientStream::ClientHandler :
	boost::noncopyable,
	public std::enable_shared_from_this<ClientHandler>
	{
		std::shared_ptr<MasterNodeClientStream> stream;
	public:
		ClientHandler(const std::shared_ptr<MasterNodeClientStream>& stream):
		stream(stream)
		{
		}
		
		// FIXME: use strand to synchronize access to the tcp socket
		template <class... TArgs>
		void async_read_some(TArgs&& ...args)
		{
			stream->connection->async_read_some(std::forward<TArgs>(args)...);
		}
		template <class... TArgs>
		void async_write_some(TArgs&& ...args)
		{
			stream->connection->async_write_some(std::forward<TArgs>(args)...);
		}
		
		void shutdown()
		{
			stream->connection->shutdown();
		}
	};
	
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
			
			auto cli = req->response->client();
			client = cli;
			
			req->response->accept
			([this, self, cli] {
				std::shared_ptr<ClientHandler> handler(new ClientHandler(self));
				return std::make_shared<MasterClientHandler<std::shared_ptr<ClientHandler>>>(handler);
			}, [this, self, cli] {
				 shutdown();
			}, req->version);
			
		});
	}
	
	void MasterNodeClientStream::connectionShutdown()
	{
		
	}
	
	void MasterNodeClientStream::shutdown()
	{
		auto self = shared_from_this();
		
		auto cli = client.lock();
		client.reset();
		if (cli) {
			cli->shutdown();
		}
		
		connection->shutdown();
	}
}
