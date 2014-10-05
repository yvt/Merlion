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

namespace mcore
{
    
    class Master;
	class MasterNode;
	class MasterClientResponse;
	class LikeMatcher;
	
    class MasterClient :
	public std::enable_shared_from_this<MasterClient>,
	boost::noncopyable
    {
    public:
        using socketType = boost::asio::ip::tcp::socket;
        using sslSocketType = boost::asio::ssl::stream<socketType>;
        using ioMutexType = std::recursive_mutex;
		using ptr = std::shared_ptr<MasterClient>;
    private:
        friend class Master;
		friend class MasterClientResponse;
		
		TypedLogger<MasterClient> log;
		
		bool allowSpecifyVersion;
        
        std::uint64_t clientId;

        boost::asio::io_service& service;
        bool accepted;
        volatile bool disposed;
		
		ioMutexType mutex;

        boost::asio::ssl::context& sslContext;
        sslSocketType sslSocket;
        
        boost::asio::deadline_timer timeoutTimer;
		
		std::string version;
		std::string _room;
		std::unique_ptr<LikeMatcher> versionRequest;
		
        void handshakeDone(const boost::system::error_code&);
		
		template <class Callback>
		void respondStatus(ClientResponse resp, Callback callback);
		
		void connectionApproved(std::function<void(sslSocketType&, ioMutexType&)> onsuccess,
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
		
        void didAccept();
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
		
		void accept(std::function<void(MasterClient::sslSocketType&, MasterClient::ioMutexType&)> onsuccess,
					std::function<void()> onfail,
					const std::string& version);
		void reject(const std::string& reason);
		
		bool isResponded();
		
		boost::signals2::signal<void(bool)> onResponded;
	};
}

