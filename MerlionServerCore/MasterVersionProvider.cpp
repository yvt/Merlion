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
#include "MasterVersionProvider.hpp"
#include <iostream>
#include <fstream>
#include "Master.hpp"

namespace asio = boost::asio;
using boost::format;

namespace mcore
{
	MasterVersionProvider::MasterVersionProvider(const MasterNodeConnection::ptr &connection):
	connection(connection),
	buffer(16384)
	{
		std::string channel = "Provide:Unknown";
		connection->setChannelName(channel);
		log.setChannel(channel);
		
		// TODO: timeout
	}
	
	MasterVersionProvider::~MasterVersionProvider()
	{
		
	}
	
	void MasterVersionProvider::service()
	{
		auto self = shared_from_this();
		getVersionStringLength();
	}
	
	void MasterVersionProvider::getVersionStringLength()
	{
		// Read version name string length
		auto self = shared_from_this();
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Receiving version string length.";
		
		auto buf = std::make_shared<std::uint32_t>();
		connection->readAsync(asio::buffer(buf.get(), 4),
		[this, self, buf](const boost::system::error_code &error, std::size_t)
		{
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Warn) << "Failed to receive version string length.: " << error;
				connection->shutdown();
				return;
			}
			
			std::size_t len = static_cast<std::size_t>(*buf);
			if (len > 1024) {
				connection->shutdown();
				return;
			}
			
			getVersionString(len);
		});
	}
	
	void MasterVersionProvider::getVersionString(std::size_t len)
	{
		// Read version name string length
		auto self = shared_from_this();
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Receiving version string.";
		
		auto buf = std::make_shared<std::vector<char>>();
		buf->resize(len);
		connection->readAsync(asio::buffer(buf->data(), buf->size()),
		[this, self, buf](const boost::system::error_code &error, std::size_t)
		{
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Warn) << "Failed to receive version string.: " << error;
				connection->shutdown();
				return;
			}

			try {
				std::string ver {buf->data(), buf->size()};
				
				std::string channel = "Provide:" + ver;
				connection->setChannelName(channel);
				log.setChannel(channel);
				
				BOOST_LOG_SEV(log, LogLevel::Debug) << "Got version string.";
				
				auto path = connection->master().parameters().getPackagePathFunction(ver);
				auto bPath = boost::filesystem::path(path);
				auto fileSize = boost::filesystem::file_size(bPath);
				if (fileSize == static_cast<std::size_t>(-1)) {
					MSCThrow(InvalidOperationException
							 (str(format("Cannot determine the size of '%s'.") % bPath)));
				}
				
				if (fileSize == 0) {
					MSCThrow(InvalidOperationException
							 (str(format("File size of '%s' is zero bytes.") % bPath)));
				}
				
				
				file.open(bPath);
				file.exceptions(std::ios::failbit |
								std::ios::badbit);
				
				sendFileLength(static_cast<std::uint64_t>(fileSize));
				
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Warn) << "Failed to supply application package.: " <<
				boost::current_exception_diagnostic_information();
				connection->shutdown();
			}
			MSCThrow(NotImplementedException());
		});
	}
	
	void MasterVersionProvider::sendFileLength(std::uint64_t len)
	{
		auto self = shared_from_this();
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Sending application size.";
		
		remainingLength = len;
		
		auto buf = std::make_shared<std::uint64_t>(len);
		connection->writeAsync(asio::buffer(buf.get(), 8),
		[this, self, buf](const boost::system::error_code &error, std::size_t)
		{
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Warn) << "Failed to send application size.: " << error;
				connection->shutdown();
				return;
			}
			
			BOOST_LOG_SEV(log, LogLevel::Info) << "Sending application package.";
			sendChunk();
		});
	}
	
	void MasterVersionProvider::sendChunk()
	{
		auto self = shared_from_this();
		
		if (remainingLength == 0) {
			// done!
			BOOST_LOG_SEV(log, LogLevel::Info) << "Done.";
			connection->shutdown();
			return;
		}
		
		auto sentLen = static_cast<std::size_t>
		(std::min<std::uintmax_t>(buffer.size(), remainingLength));
		
		try {
			file.read(buffer.data(), sentLen);
		} catch (...) {
			BOOST_LOG_SEV(log, LogLevel::Warn) << "Error occured while reading package file: " <<
			boost::current_exception_diagnostic_information();
			connection->shutdown();
			return;
		}
		
		remainingLength -= static_cast<std::uint64_t>(sentLen);
		
		connection->writeAsync(asio::buffer(buffer.data(), sentLen),
		[this, self](const boost::system::error_code &error, std::size_t)
		{
			if (error) {
				BOOST_LOG_SEV(log, LogLevel::Warn) << "Failed to send application package.: " << error;
				connection->shutdown();
				return;
			}
			
			sendChunk();
		});
		
	}
	
	void MasterVersionProvider::connectionShutdown()
	{
		
	}
}
