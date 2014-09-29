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
