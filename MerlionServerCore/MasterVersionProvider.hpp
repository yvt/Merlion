#pragma once

#include "MasterNode.hpp"
#include "Logging.hpp"

namespace mcore
{
	class MasterVersionProvider :
	public MasterNodeConnectionHandler,
	public std::enable_shared_from_this<MasterVersionProvider>,
	boost::noncopyable
	{
		TypedLogger<MasterVersionProvider> log;
		std::shared_ptr<MasterNodeConnection> connection;
		
		boost::filesystem::ifstream file;
		std::vector<char> buffer;
		std::uint64_t remainingLength;
		
		void getVersionStringLength();
		void getVersionString(std::size_t);
		void sendFileLength(std::uint64_t);
		void sendChunk();
		
	public:
		MasterVersionProvider(const std::shared_ptr<MasterNodeConnection> &connection);
		~MasterVersionProvider();
		
		void service() override;
		void connectionShutdown() override;
	};
}
