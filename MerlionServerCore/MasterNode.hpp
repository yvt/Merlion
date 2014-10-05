#pragma once

#include "MasterNodeConnection.hpp"
#include "Protocol.hpp"
#include "Packet.hpp"
#include "Master.hpp"
#include <unordered_map>
#include "Logging.hpp"

namespace mcore
{
    class Master;
	class MasterClientResponse;
	
    class MasterNode:
	public MasterNodeConnectionHandler, MasterListener,
	public std::enable_shared_from_this<MasterNode>
    {
		TypedLogger<MasterNode> log;
        std::list<std::shared_ptr<MasterNode>>::iterator iter;

		std::shared_ptr<MasterNodeConnection> connection;
        NodeInfo _nodeInfo;

        PacketGenerator sendBuffer;
        std::recursive_mutex sendMutex;
        volatile bool sendReady;
        
        struct Domain
        {
            std::string versionName;
			std::unordered_map<std::uint64_t, std::shared_ptr<MasterClient>> clients;
            std::unordered_set<std::string> rooms;
        };
        std::unordered_map<std::string, Domain> domains;
        std::recursive_mutex domainsMutex;

        volatile bool connected;

        void receiveHeader(std::size_t size);
        void receiveCommand();
        void receiveCommandBody(std::size_t size);

        void flushSendBuffer();

        void versionAdded(Master&, const std::string&) override;
        void versionRemoved(Master&, const std::string&) override;
		void clientDisconnected(std::uint64_t) override;
        void heartbeat(Master&);
        void connectionShutdown();
		
		void addClient(const std::string& version, std::uint64_t);
		
    public:
		MasterNode(const std::shared_ptr<MasterNodeConnection>&);
        ~MasterNode();

        Master& master();
		
		const NodeInfo& nodeInfo() const { return _nodeInfo; }
		
		bool isConnected() const { return connected; }
		
		std::size_t numClients();
		std::size_t numRooms();
		
		boost::optional<std::string> findDomainForRoom(const std::string&);
		
		struct DomainStatus
		{
			std::string versionName;
			std::size_t numClients;
			std::size_t numRooms;
		};
		std::vector<DomainStatus> domainStatuses();
		
		void acceptClient(const std::shared_ptr<MasterClientResponse>& response,
						  const std::string& version);
		
		void removeClient(std::uint64_t);

        void sendLoadVersion(const std::string&, bool flush = true);
        void sendUnloadVersion(const std::string&, bool flush = true);
        void sendHeartbeat();
		void sendClientConnected(std::uint64_t clientId,
								 const std::string& version,
								 const std::string& room);

        void service() override;
    };
}