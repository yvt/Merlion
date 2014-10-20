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

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include "Protocol.hpp"
#include "Logging.hpp"
#include <atomic>
#include "Utils.hpp"
#include "Exceptions.hpp"
#include "AsyncPipe.hpp"

namespace mcore
{
    
    class Master;
	class MasterNode;
	class MasterClientResponse;
	class LikeMatcher;
	class BaseMasterClientHandler;
	
    class MasterClient :
	public std::enable_shared_from_this<MasterClient>,
	boost::noncopyable
    {
    public:
        using socketType = boost::asio::ip::tcp::socket;
        using sslSocketType = boost::asio::ssl::stream<socketType>;
		using strandType = boost::asio::strand;
		using ptr = std::shared_ptr<MasterClient>;
		using ioMutexType = std::recursive_mutex;
    private:
        friend class Master;
		friend class MasterClientResponse;
		template <class T>
		friend class MasterClientHandler;
		
		TypedLogger<MasterClient> log;
		
		bool allowSpecifyVersion;
        
        std::uint64_t clientId;

        boost::asio::io_service& service;
        bool accepted;
        volatile bool disposed;
		
		boost::asio::strand _strand;
		
		ioMutexType mutex;

        boost::asio::ssl::context& sslContext;
        sslSocketType sslSocket;
		
		std::shared_ptr<BaseMasterClientHandler> handler;
        
        boost::asio::deadline_timer timeoutTimer;
		
		std::string version;
		std::string _room;
		std::unique_ptr<LikeMatcher> versionRequest;
		
        void handshakeDone(const boost::system::error_code&);
		
		template <class Callback>
		void respondStatus(ClientResponse resp, Callback callback);
		
		void connectionApproved(std::function<std::shared_ptr<BaseMasterClientHandler>()> onsuccess,
								std::function<void()> onfail,
								const std::string& version);
		void connectionRejected();
		
		
    public:
        MasterClient(Master& master, std::uint64_t id,
					 bool allowSpecifyVersion);
        ~MasterClient();
        
        std::uint64_t id() const { return clientId; }
		
        socketType& tcpSocket() { return sslSocket.next_layer(); }
		
		const std::string& room() const { return _room; }
		
		bool doesAcceptVersion(const std::string&);
		
        void handleNewClient();
        void shutdown();
		
		boost::signals2::signal<void(const std::shared_ptr<MasterClientResponse>&)> onNeedsResponse;
		boost::signals2::signal<void()> onShutdown;
    };
	
	class MasterClientResponse :
	public std::enable_shared_from_this<MasterClientResponse>,
	boost::noncopyable
	{
		friend class MasterClient;
		
		MasterClient::ptr _client;
		std::atomic<bool> done;
		
		MasterClientResponse(const MasterClient::ptr&);
	public:
		~MasterClientResponse();
		
		const MasterClient::ptr &client() const { return _client; }
		
		void accept(std::function<std::shared_ptr<BaseMasterClientHandler>()> onsuccess,
					std::function<void()> onfail,
					const std::string& version);
		void reject(const std::string& reason);
		
		bool isResponded();
		
		boost::signals2::signal<void(bool)> onResponded;
	};
	
	class BaseMasterClientHandler :
	boost::noncopyable
	{
		friend class MasterClient;
		template <class T>
		friend class MasterClientHandler;
		virtual void handleClient(std::shared_ptr<MasterClient>) = 0;
		virtual void shutdown() = 0;
	};
	
	/** For an implementation of T, see
	 * <MasterNodeClientStream::ClientHandler>. */
	template <class T>
	class MasterClientHandler :
	boost::noncopyable,
	public std::enable_shared_from_this<MasterClientHandler<T>>,
	public BaseMasterClientHandler
	{
		T baseHandler;
		std::atomic<bool> handled;
		std::shared_ptr<MasterClient> client;
		
		struct ClientInputOutput
		{
			std::shared_ptr<MasterClient> client;
			
			ClientInputOutput(const std::shared_ptr<MasterClient>& client):
			client(client)
			{ }
			
			template <class Buffer, class Callback>
			void async_read_some(Buffer&& buffer,
								 Callback&& callback)
			{
				auto& strand = client->_strand;
				auto client = this->client;
				strand.dispatch([buffer, callback, client, this] {
					auto& strand = client->_strand;
					auto& sslSocket = client->sslSocket;
					sslSocket.async_read_some
					(std::move(buffer),
					 strand.wrap(std::move(callback)));
				});
			}
			template <class Buffer, class Callback>
			void async_write_some(Buffer&& buffer,
								  Callback&& callback)
			{
				auto& strand = client->_strand;
				auto client = this->client;
				strand.dispatch([buffer, callback, client, this] {
					auto& strand = client->_strand;
					auto& sslSocket = client->sslSocket;
					sslSocket.async_write_some
					(std::move(buffer),
					 strand.wrap(std::move(callback)));
				});
			}
			
		};
		
		void handleClient(std::shared_ptr<MasterClient> client) override final
		{
			if (handled.exchange(true)) {
				MSCThrow(InvalidOperationException("handleClient was called twice."));
			}
			
			auto self = this->shared_from_this();
			std::shared_ptr<ClientInputOutput> io
			(new ClientInputOutput(client));
			
			// Start downstream
			startAsyncPipe(*io, *baseHandler, 4096, [self, this, client, io] (const boost::system::error_code& error, std::size_t count) {
				if (error) {
					BOOST_LOG_SEV(client->log, LogLevel::Debug) <<
					"Downstream error.: " << error;
				} else {
					BOOST_LOG_SEV(client->log, LogLevel::Debug) <<
					"Downstream reached EOF.";
				}
				client->shutdown();
			});
			
			// Start upstream
			startAsyncPipe(*baseHandler, *io, 4096, [self, this, client, io] (const boost::system::error_code& error, std::size_t count) {
				if (error) {
					BOOST_LOG_SEV(client->log, LogLevel::Debug) <<
					"Upstream error.: " << error;
				} else {
					BOOST_LOG_SEV(client->log, LogLevel::Debug) <<
					"Upstream reached EOF.";
				}
				client->shutdown();
			});
		}
		
		void shutdown()
		{
			baseHandler->shutdown();
		}
	public:
		MasterClientHandler(const T& baseHandler):
		baseHandler(baseHandler),
		handled(false)
		{ }
		MasterClientHandler(T&& baseHandler):
		baseHandler(baseHandler),
		handled(false)
		{ }
	};
}

