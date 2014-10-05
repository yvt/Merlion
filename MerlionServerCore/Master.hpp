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

#include "Public.h"
#include "Library.hpp"
#include <boost/optional.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <list>
#include <functional>
#include "Logging.hpp"

namespace mcore
{
    class Library;
    class MasterNodeConnection;
    class MasterNode;
    class MasterListener;
    class MasterClient;
	class MasterClientResponse;

    struct MasterParameters
    {
        std::string nodeEndpoint;
        std::string clientEndpoint;
        std::string sslCertificateFile;
        std::string sslPrivateKeyFile;
		std::string sslPassword;
		std::function<std::string(const std::string&)> getPackagePathFunction;
		bool allowVersionSpecification;
		
        MasterParameters() { }
        MasterParameters(const MSCMasterParameters&);
    };

    struct MasterVersion
    {
        std::string name;
        double throttle;
    };

    class Master:
	boost::noncopyable
    {
	public:
		struct PendingClient
		{
			std::string version;
			std::shared_ptr<MasterClientResponse> response;
		};
		
		
	private:
        friend class MasterNodeConnection;

		std::shared_ptr<Library> _library;
		TypedLogger<Master> log;

        MasterParameters _parameters;
        
        volatile bool disposed;

        boost::asio::deadline_timer heartbeatTimer;
        std::recursive_mutex heartbeatMutex;
        volatile bool heartbeatRunning;

        std::shared_ptr<MasterNodeConnection> waitingNodeConnection;
        std::list<std::shared_ptr<MasterNodeConnection>> nodeConnections;
        boost::asio::ip::tcp::acceptor nodeAcceptor;
        std::mutex nodeAcceptorMutex;
        std::recursive_mutex nodeConnectionsMutex;
        void removeNodeConnection(const std::list<std::shared_ptr<MasterNodeConnection>>::iterator&);
        void acceptNodeConnectionAsync(bool initial);
		
		std::unordered_map<MasterNode *, std::shared_ptr<MasterNode>> nodes;
		std::recursive_mutex nodesMutex;
		
        std::shared_ptr<MasterClient> waitingClient;
        std::unordered_map<std::uint64_t, std::shared_ptr<MasterClient>> clients;
        boost::asio::ip::tcp::acceptor clientAcceptor;
        std::mutex clientAcceptorMutex;
        std::recursive_mutex clientsMutex;
		void removeClient(std::uint64_t);
        void acceptClientAsync(bool initial);
		
		std::unordered_map<std::uint64_t, PendingClient> pendingClients;
		std::recursive_mutex pendingClientsMutex;

        std::unordered_set<MasterListener *> listeners;
        std::recursive_mutex listenersMutex;

        std::unordered_map<std::string, MasterVersion> versions;
        std::recursive_mutex versionsMutex;
		
		std::unordered_map<std::string, double> nodeThrottles;
		std::recursive_mutex nodeThrottlesMutex;
		
        boost::asio::ssl::context _sslContext;

        void invalidate();
        void checkValid() const;

        void doHeartbeat(const boost::system::error_code& e);

    public:
        Master(const std::shared_ptr<Library>&, const MasterParameters&);
        ~Master();

		const std::shared_ptr<Library>& library() const { return _library; }
        boost::asio::io_service& ioService() const;
        boost::asio::ssl::context& sslContext() { return _sslContext; }

        const MasterParameters& parameters() const { return _parameters; }

        void addListener(MasterListener *);
        void removeListener(MasterListener *);
		
		void addNode(const std::shared_ptr<MasterNode>&);
		void removeNode(MasterNode *);
		std::vector<std::shared_ptr<MasterNode>> getAllNodes();

        void addVersion(const std::string& name);
        void removeVersion(const std::string& name);
		void setVersionThrottle(const std::string& name, double);
        std::vector<std::string> getAllVersionNames();
		
		boost::optional<PendingClient> dequePendingClient(std::uint64_t);
		std::shared_ptr<MasterClient> getClient(std::uint64_t);
		
		void setNodeThrottle(const std::string& name, double);

        MSCMaster handle() { return reinterpret_cast<MSCMaster>(this); }
        static Master *fromHandle(MSCMaster handle) { return reinterpret_cast<Master *>(handle); }
		
		boost::optional<std::pair<std::shared_ptr<MasterNode>, std::string>>
		bindClientToDomain(const std::shared_ptr<MasterClient>& client);
    };

    class MasterListener
    {
    public:
        virtual void versionAdded(Master&, const std::string&) {}
        virtual void versionRemoved(Master&, const std::string&) {}
        virtual void heartbeat(Master&) {}
		virtual void clientDisconnected(std::uint64_t) {}
    };
}

