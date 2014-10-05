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

namespace mcore
{
	class Node;
	class NodeClient;
	
	class NodeDomain :
	public std::enable_shared_from_this<NodeDomain>,
	boost::noncopyable
	{
		Node& _node;
		std::string const _version;
		std::recursive_mutex mutex;
		
		volatile bool acceptsClient;
		std::unordered_map<std::uint64_t, std::shared_ptr<NodeClient>> clients;
		
	public:
		NodeDomain(Node& node, const std::string &name);
		~NodeDomain();
		
		Node& node() const { return _node; }
		const std::string& version() const { return _version; }
		
		void setAcceptsClient(bool);
		
		void acceptClient(const std::shared_ptr<NodeClient>& client);
	};
	
}
