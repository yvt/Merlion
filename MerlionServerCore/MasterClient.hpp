#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include "Protocol.hpp"
#include "Logging.hpp"

namespace mcore
{
    
    class Master;
	class MasterNode;
	
    class MasterClient : public std::enable_shared_from_this<MasterClient>
    {
    public:
        using socketType = boost::asio::ip::tcp::socket;
        using sslSocketType = boost::asio::ssl::stream<socketType>;
        using ioMutexType = std::recursive_mutex;
    private:
        friend class Master;
        Master& _master;
		TypedLogger<MasterClient> log;
        
        std::uint64_t clientId;

        boost::asio::io_service& service;
        bool accepted;
        volatile bool disposed;
		
		std::recursive_mutex mutex;

        boost::asio::ssl::context& sslContext;
        sslSocketType sslSocket;
        
        boost::asio::deadline_timer timeoutTimer;
		
		std::string version;
		
        void handshakeDone(const boost::system::error_code&);
		
		template <class Callback>
		void respondStatus(ClientResponse resp, Callback callback);
        
    public:
        MasterClient(Master& master, std::uint64_t id);
        ~MasterClient();
        
        std::uint64_t id() const { return clientId; }
        
        socketType& tcpSocket() { return sslSocket.next_layer(); }
        Master& master() const { return _master; }
		
		void connectionApproved(std::function<void(sslSocketType&)> onsuccess,
								std::function<void()> onfail);
		void connectionRejected();
        
        void didAccept();
        void shutdown();
    };
}

