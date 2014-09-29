#pragma once

#include "MasterNode.hpp"
#include "Logging.hpp"

namespace mcore
{
	class MasterClient;
	
	class MasterNodeClientStream :
	public MasterNodeConnectionHandler,
	public std::enable_shared_from_this<MasterNodeClientStream>
	{
		TypedLogger<MasterNodeClientStream> log;
		
		MasterNodeConnection& connection;
		std::shared_ptr<MasterClient> client;
		
		void shutdown();
	public:
		MasterNodeClientStream(MasterNodeConnection& connection);
		~MasterNodeClientStream();
		
		void service() override;
		void connectionShutdown() override;
	};
}
