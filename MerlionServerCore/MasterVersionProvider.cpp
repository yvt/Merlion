#include "Prefix.pch"
#include "MasterVersionProvider.hpp"
#include <iostream>
#include <fstream>

namespace asio = boost::asio;
using boost::format;

namespace mcore
{
	MasterVersionProvider::MasterVersionProvider(MasterNodeConnection& connection):
	connection(connection)
	{
		
	}
	
	MasterVersionProvider::~MasterVersionProvider()
	{
		
	}
	
	void MasterVersionProvider::service()
	{
		getVersionStringLength();
	}
	
	void MasterVersionProvider::getVersionStringLength()
	{
		// Read version name string length
		auto buf = std::make_shared<std::uint32_t>();
		connection.readAsync(asio::buffer(buf.get(), 4),
		[this, buf](const boost::system::error_code &error, std::size_t)
		{
			if (error) {
				connection.shutdown();
				return;
			}
			
			std::size_t len = static_cast<std::size_t>(*buf);
			if (len > 1024) {
				connection.shutdown();
				return;
			}
			
			getVersionString(len);
		});
	}
	
	void MasterVersionProvider::getVersionString(std::size_t len)
	{
		// Read version name string length
		auto buf = std::make_shared<std::vector<char>>();
		buf->resize(len);
		connection.readAsync(asio::buffer(buf->data(), buf->size()),
		[this, buf](const boost::system::error_code &error, std::size_t)
		{
			if (error) {
				connection.shutdown();
				return;
			}

			std::string ver {buf->data(), buf->size()};
			MSCThrow(NotImplementedException());
		});
	}
	
	void MasterVersionProvider::connectionShutdown()
	{
		
	}
}
