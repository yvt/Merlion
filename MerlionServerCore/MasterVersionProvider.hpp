#pragma once

#include "MasterNode.hpp"

namespace mcore
{
	class MasterVersionProvider :
	public MasterNodeConnectionHandler,
	public std::enable_shared_from_this<MasterVersionProvider>,
	boost::noncopyable
	{
		std::shared_ptr<MasterNodeConnection> connection;
		
		void getVersionStringLength();
		void getVersionString(std::size_t);
		
	public:
		MasterVersionProvider(const std::shared_ptr<MasterNodeConnection> &connection);
		~MasterVersionProvider();
		
		void service() override;
		void connectionShutdown() override;
	};
}
