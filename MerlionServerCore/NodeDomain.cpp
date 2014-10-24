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

#include "Prefix.pch"
#include "NodeDomain.hpp"
#include "NodeClient.hpp"

namespace mcore
{
	
	NodeDomain::NodeDomain(Node& node, const std::string& name):
	_node(node),
	_version(name),
	acceptsClient(false)
	{
		
	}
	
	NodeDomain::~NodeDomain()
	{
	}
	
	void NodeDomain::setAcceptsClient(bool b)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex);
		acceptsClient = b;
		
		if (!b) {
			std::vector<std::shared_ptr<NodeClient>> clis;
			for (const auto& c: clients)
				clis.push_back(c.second);
			for (const auto& c: clis)
				c->drop();
		}
	}
	
	void NodeDomain::acceptClient(const std::shared_ptr<NodeClient> &client)
	{
		auto self = shared_from_this();
		std::lock_guard<std::recursive_mutex> lock(mutex);
		
		if (!acceptsClient) {
			client->reject();
			return;
		}
		
		std::weak_ptr<NodeDomain> weakSelf(self);
		client->onClosed.connect([this, weakSelf](const NodeClient::ptr& client) {
			auto self = weakSelf.lock();
			if (!self)
				return;
			std::lock_guard<std::recursive_mutex> lock(mutex);
			clients.erase(client->clientId());
		});
		
		clients.emplace(client->clientId(), client);
		
		auto success = client->accept();
		if (!success) {
			clients.erase(client->clientId());
			return;
		}
		
		if (!client->isOpen()) {
			clients.erase(client->clientId());
		}
		
	}
	
}
