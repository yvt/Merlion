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
#include "WebSocket.hpp"
#include "Logging.hpp"
#include <list>

namespace mcore
{
	class Library;
	
	class NodeClientSocket :
	boost::noncopyable,
	public std::enable_shared_from_this<NodeClientSocket>
	{
		TypedLogger<NodeClientSocket> log;
		std::shared_ptr<Library> const library;
		
		boost::asio::local::stream_protocol::socket socket;
		
		boost::asio::strand _strand;
		
		asiows::web_socket<boost::asio::local::stream_protocol::socket&> webSocket;
		std::vector<char> receiveBuffer;
		std::size_t receiveBufferLen;
		
		std::list<std::function<void()>> shutdownListeners;
		
		bool receiving_ = false;
		bool down_ = false;
		
	public:
		NodeClientSocket(std::shared_ptr<Library>, const std::string& displayName,
						 int stream);
		virtual ~NodeClientSocket();
		
		MSCClientSocket createHandle()
		{ return reinterpret_cast<MSCClientSocket>
			(new std::shared_ptr<NodeClientSocket>(shared_from_this())); }
		static std::shared_ptr<NodeClientSocket>& fromHandle(MSCClientSocket h)
		{ return *reinterpret_cast<std::shared_ptr<NodeClientSocket> *>(h); }
		
		template <class Callback>
		void receive(Callback&&);
		
		template <class Callback>
		void send(const void *data, std::size_t length,
				  Callback&&);
		
		void shutdown();
	};
	
	
}
