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
		
        void handshakeDone(const boost::system::error_code&);
		
		template <class Callback>
		void respondStatus(ClientResponse resp, Callback callback);
		
		void connectionApproved(std::function<void(sslSocketType&, ioMutexType&)> onsuccess,
								std::function<void()> onfail,
								const std::string& version);
		void connectionRejected();
		
    public:
        MasterClient(Master& master, std::uint64_t id);
        ~MasterClient();
        
        std::uint64_t id() const { return clientId; }
        
        socketType& tcpSocket() { return sslSocket.next_layer(); }
		
		const std::string& room() const { return _room; }
		
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

