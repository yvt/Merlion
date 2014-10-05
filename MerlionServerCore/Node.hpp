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
#include "Library.hpp"
#include "Public.h"
#include "Logging.hpp"
#include "Protocol.hpp"

namespace mcore
{
	struct ClientSetup
	{
		int readStream;
		int writeStream;
	};
	
	struct NodeParameters
	{
		std::string nodeName;
		std::string masterEndpoint;
		std::function<std::string(const std::string&)> getPackagePathFunction;
		std::function<std::string(const std::string&)> getPackageDownloadPathFunction;
		std::function<void(const std::string&)> deployPackageFunction;
		std::function<void(const std::string&)> loadVersionFunction;
		std::function<void(const std::string&)> unloadVersionFunction;
		std::function<void(std::uint64_t, const std::string&, const std::string&)> acceptClientFunction;
		std::function<void(std::uint64_t, const ClientSetup&)> setupClientFunction;
		std::function<void(std::uint64_t)> destroyClientFunction;
		
		NodeParameters() = default;
		NodeParameters(MSCNodeParameters const &param);
	};
	
	class NodeDomain;
	class NodeClient;
	class NodeVersionLoader;
	
	class Node: boost::noncopyable, LibraryListener
	{
		std::shared_ptr<Library> _library;
		TypedLogger<Node> log;
		
		NodeParameters const _parameters;
		NodeInfo info;
		
		boost::asio::ip::tcp::endpoint endpoint;
		boost::asio::ip::tcp::socket socket;
		std::recursive_mutex socketMutex;
		volatile bool down;
		
		std::unordered_set<std::string> versionsToLoad;
		std::shared_ptr<NodeVersionLoader> versionLoader;
		
		boost::asio::deadline_timer timeoutTimer;
		boost::asio::deadline_timer reconnectTimer;
		
		std::unordered_map<std::string, std::shared_ptr<NodeDomain>> domains;
		
		void onBeingDestroyed(Library&) override;
		void invalidate();
		void checkValid() const;
		
		void setTimeout();
		void shutdown();
		void scheduleReconnect();
		
		void connectAsync();
		void sendHeader();
		void receiveCommandHeader();
		void receiveCommandBody(std::size_t);
		
		void loadDomainIfNotLoaded(const std::string&);
		
		template <class F>
		void sendCommand(const std::string& name, F gen);
		void sendHeartbeat();
		void sendRejectClient(std::uint64_t);
		void sendVersionLoaded(const std::string&);
		void sendVersionUnloaded(const std::string&);
		void sendBindRoom(const std::string& room, const std::string& version);
		void sendUnbindRoom(const std::string& room);
		void sendLog();
	public:
		Node(const std::shared_ptr<Library>&, const NodeParameters&);
		~Node();
		
		const std::shared_ptr<Library>& library() const { return _library; }
		const NodeParameters& parameters() const { return _parameters; }
		const boost::asio::ip::tcp::endpoint& masterEndpoint() const
		{ return endpoint; }
		
		void versionLoaded(const std::string&);
		void versionUnloaded(const std::string&);
		void bindRoom(const std::string& room, const std::string& version);
		void unbindRoom(const std::string& room);
		
		MSCNode handle() { return reinterpret_cast<MSCNode>(this); }
		static Node *fromHandle(MSCNode handle) { return reinterpret_cast<Node *>(handle); }
	};
}