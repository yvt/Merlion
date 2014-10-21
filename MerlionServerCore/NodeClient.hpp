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

#include "Logging.hpp"

namespace mcore
{
	class NodeDomain;
	
	class NodeClient :
	public std::enable_shared_from_this<NodeClient>,
	boost::noncopyable
	{
		TypedLogger<NodeClient> log;
		
		enum class State
		{
			NotAccepted,
			WaitingForApplication,
			ConnectingToMaster,
			SettingUpApplication,
			Servicing,
			
			Closed
		};
		std::weak_ptr<NodeDomain> const domain;
		std::uint64_t const _clientId;
		State state;
		boost::asio::strand _strand;
		std::string const room;
		
		boost::asio::ip::tcp::socket masterSocket;
		int upPipe[2];
		int downPipe[2];
		
		boost::asio::local::stream_protocol::socket upSocket;
		boost::asio::local::stream_protocol::socket downSocket;
		void socketDown();
		
		void connectToMaster();
		void cancelConnectToMaster();
		
		void startService();
		void terminateService();
		
	public:
		using ptr = std::shared_ptr<NodeClient>;
		NodeClient(const std::shared_ptr<NodeDomain> &domain,
				   std::uint64_t clientId,
				   const std::string& room);
		~NodeClient();
		
		std::uint64_t clientId() const { return _clientId; }
		
		// State: Not Accepted
		bool accept();
		void reject();
		
		// Called when connection between app and master
		// will not be created
		boost::signals2::signal<void(const ptr&)> onConnectionRejected;
		
		// For all states
		void drop();
		
		boost::signals2::signal<void(const ptr&)> onClosed;
		
	};
}