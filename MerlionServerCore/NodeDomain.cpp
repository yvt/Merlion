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
		
		client->onClosed.connect([this, self](const NodeClient::ptr& client) {
			std::lock_guard<std::recursive_mutex> lock(mutex);
			clients.erase(client->clientId());
		});
		
		clients.emplace(client->clientId(), client);
		
		auto success = client->accept();
		if (!success) {
			clients.erase(client->clientId());
			return;
		}
		
		
		
	}
	
}
