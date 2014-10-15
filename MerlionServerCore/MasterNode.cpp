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
#include "MasterNode.hpp"
#include "Protocol.hpp"
#include "Exceptions.hpp"
#include <boost/format.hpp>
#include "Packet.hpp"
#include "MasterClient.hpp"
#include <sstream>

namespace asio = boost::asio;
using boost::format;

namespace mcore
{

	MasterNode::MasterNode(const MasterNodeConnection::ptr&connection):
        connection(connection),
        connected(false),
        sendReady(true)
	{
		channel = str(format("Unknown Node [%s]") % connection->tcpSocket().remote_endpoint());
		log.setChannel(channel);
		connection->setChannelName(channel);
		
		{
			std::stringstream ss;
			ss << connection->tcpSocket().remote_endpoint();
			_hostNameString = ss.str();
		}
		
		// Placeholder for packet size
		sendBuffer.write<std::uint32_t>(0);
    }

    MasterNode::~MasterNode()
    {
        master().removeListener(this);
		connection->shutdown();
    }

    Master& MasterNode::master()
    {
        return connection->master();
    }

    void MasterNode::service()
    {
        connection->enableBuffering();
		

		auto self = shared_from_this();
        auto headerBuffer = std::make_shared<std::uint32_t>(0);

        connection->readAsync(asio::buffer(headerBuffer.get(), 4),
        [this, self, headerBuffer]
        (const boost::system::error_code& error, std::size_t readCount){
            try {
                if (error) {
                    MSCThrow(boost::system::system_error(error));
                }

                std::uint32_t dataSize = *headerBuffer;
                if (dataSize == 0 || dataSize > 1024 * 1024) {
                    MSCThrow(InvalidDataException("Header is too big."));
                }

                receiveHeader(dataSize);
			} catch (...) {
				connection->performShutdownByError(boost::current_exception_diagnostic_information());
            }
        });
    }

    void MasterNode::receiveHeader(std::size_t size)
	{
		auto self = shared_from_this();
        auto headerBuffer = std::make_shared<std::vector<char>>();
        headerBuffer->resize(size);

        connection->readAsync(asio::buffer(headerBuffer->data(), size),
        [this, self, headerBuffer]
        (const boost::system::error_code& error, std::size_t readCount){
            try {
                if (error) {
                    MSCThrow(boost::system::system_error(error));
                }

                PacketReader reader(headerBuffer->data(), readCount);
                _nodeInfo.deserialize(reader);
				
				auto nodeUptime = reader.read<std::uint64_t>();
				startTime = boost::posix_time::second_clock::universal_time()
				- boost::posix_time::seconds(static_cast<long>(nodeUptime));
				
				channel = str(format("Node:%s [%s]") % _nodeInfo.nodeName % connection->tcpSocket().remote_endpoint());
				log.setChannel(channel);
				connection->setChannelName(channel);
				
				BOOST_LOG_SEV(log, LogLevel::Info) << "Connected. Requesting to load versions.";

				
                // Request to load all versions
                auto vers = master().getAllVersionNames();
                for (const auto& ver: vers)
                    sendLoadVersion(ver, false);
                flushSendBuffer();

				master().addListener(this);
				master().addNode(shared_from_this());

                connected = true;
				
				BOOST_LOG_SEV(log, LogLevel::Info) << "Started receiving commands.";
                receiveCommand();
			} catch (...) {
				connection->performShutdownByError(boost::current_exception_diagnostic_information());
			}
        });
    }

    void MasterNode::receiveCommand()
	{
		auto self = shared_from_this();
        auto sizeBuffer = std::make_shared<std::uint32_t>();

        connection->readAsync(asio::buffer(sizeBuffer.get(), 4),
        [this, self, sizeBuffer]
        (const boost::system::error_code& error, std::size_t readCount){
            try {
                if (error) {
                    MSCThrow(boost::system::system_error(error));
                }

                std::uint32_t dataSize = *sizeBuffer;
                if (dataSize == 0 || dataSize > 1024 * 1024) {
                    MSCThrow(InvalidDataException("Command packet is too big."));
                }


                receiveCommandBody(dataSize);
			} catch (...) {
				connection->performShutdownByError(boost::current_exception_diagnostic_information());
			}
        });
    }

    void MasterNode::receiveCommandBody(std::size_t size)
	{
		auto self = shared_from_this();
        auto cmdBuffer = std::make_shared<std::vector<char>>();
        cmdBuffer->resize(size);

        connection->readAsync(asio::buffer(cmdBuffer->data(), size),
        [this, self, cmdBuffer]
        (const boost::system::error_code& error, std::size_t readCount){
            try {
                if (error) {
                    MSCThrow(boost::system::system_error(error));
                }

                PacketReader reader(cmdBuffer->data(), readCount);
				
				while (reader.canReadBytes(1)) {
					MasterCommand cmd = reader.read<MasterCommand>();
					
					switch (cmd) {
					case MasterCommand::Nop:
						break;

					case MasterCommand::RejectClient:
						{
							auto clientId = reader.read<std::uint64_t>();
							
							auto req = master().dequePendingClient(clientId);
							if (!req) {
								BOOST_LOG_SEV(log, LogLevel::Debug)
								<< format("Requested to reject client #%d that doesn't exist.") % clientId;
								break;
							}
							
							req->response->reject(std::string());
							break;
						}

					case MasterCommand::VersionLoaded:
						{
							auto version = reader.readString();
							std::lock_guard<std::recursive_mutex> lock(domainsMutex);
							Domain& domain = domains[version];
							domain.versionName = version;
							domain.startTime = boost::posix_time::second_clock::universal_time();
							break;
						}

					case MasterCommand::VersionUnloaded:
						{
							auto version = reader.readString();
							std::lock_guard<std::recursive_mutex> lock(domainsMutex);
							domains.erase(version);
							break;
						}

					case MasterCommand::BindRoom:
						{
							auto room = reader.readString();
							auto version = reader.readString();
							std::lock_guard<std::recursive_mutex> lock(domainsMutex);
							auto it = domains.find(version);
							if (it != domains.end()) {
								Domain& domain = it->second;
								domain.rooms.insert(room);
							}
							break;
						}

					case MasterCommand::UnbindRoom:
						{
							auto room = reader.readString();
							std::lock_guard<std::recursive_mutex> lock(domainsMutex);
							for (auto& d: domains) {
								d.second.rooms.erase(room);
							}
							break;
						}

					case MasterCommand::Log:
						{
							LogEntry entry;
							entry.level = reader.read<LogLevel>();
							entry.source = reader.readString();
							entry.channel = reader.readString();
							entry.message = reader.readString();
							entry.host = channel;
							try {
								entry.log();
							} catch (...) {
								BOOST_LOG_SEV(log, LogLevel::Warn) <<
								"Error while recording the forwarded log. Ignoring.: " <<
								boost::current_exception_diagnostic_information();
							}
							break;
						}
					default:
						MSCThrow(InvalidDataException(str(format("Invalid command 0x%02x received.") %
								static_cast<std::uint8_t>(cmd))));
					}
				}

                receiveCommand();
            } catch (...) {
				connection->performShutdownByError
				(boost::current_exception_diagnostic_information());
			}
        });
    }

    void MasterNode::sendLoadVersion(const std::string& version, bool flush)
	{
		auto self = shared_from_this();
        std::lock_guard<std::recursive_mutex> lock(sendMutex);
        sendBuffer.write(NodeCommand::LoadVersion);
        sendBuffer.writeString(version);

        if (flush)
            flushSendBuffer();
    }

    void MasterNode::sendUnloadVersion(const std::string& version, bool flush)
	{
		auto self = shared_from_this();
        std::lock_guard<std::recursive_mutex> lock(sendMutex);
        sendBuffer.write(NodeCommand::UnloadVersion);
        sendBuffer.writeString(version);

        if (flush)
            flushSendBuffer();
    }
	
	void MasterNode::sendClientConnected(std::uint64_t clientId,
										 const std::string &version,
										 const std::string &room)
	{
		auto self = shared_from_this();
		{
			std::lock_guard<std::recursive_mutex> lock(sendMutex);
			sendBuffer.write(NodeCommand::Connect);
			sendBuffer.write(clientId);
			sendBuffer.writeString(version);
			sendBuffer.writeString(room);
			
			flushSendBuffer();
		}
	}

    void MasterNode::sendHeartbeat()
	{
		auto self = shared_from_this();
        std::lock_guard<std::recursive_mutex> lock(sendMutex);
        sendBuffer.write(NodeCommand::Nop);
            flushSendBuffer();
    }

    void MasterNode::flushSendBuffer()
	{
		auto self = shared_from_this();
        std::lock_guard<std::recursive_mutex> lock(sendMutex);
        if (!sendReady)
            return;

        auto sentData = std::make_shared<std::vector<char>>();
        sentData->swap(sendBuffer.vector());
		
		// Placeholder for packet size
		sendBuffer.write<std::uint32_t>(0);
		
		*reinterpret_cast<std::uint32_t *>(sentData->data()) =
		static_cast<std::uint32_t>(sentData->size() - 4);
		
        sendReady = false;
        connection->writeAsync(asio::buffer(sentData->data(), sentData->size()),
        [this, sentData] (const boost::system::error_code& error, std::size_t readCount) {
            try {
                if (error) {
                    MSCThrow(boost::system::system_error(error));
                }

                std::lock_guard<std::recursive_mutex> lock(sendMutex);
                sendReady = true;

                if (sendBuffer.size() > 4) {
                    flushSendBuffer();
                }

			} catch (...) {
				connection->performShutdownByError(boost::current_exception_diagnostic_information());
            }
        });
    }

	void MasterNode::acceptClient(const std::shared_ptr<MasterClientResponse> &response,
								  const std::string &version)
	{
		auto client = response->client();
		
		std::lock_guard<std::recursive_mutex> lock(domainsMutex);
		auto it = domains.find(version);
		if (it == domains.end()) {
			response->reject("Version not found.");
			return;
		}
		
		it->second.clients.emplace(response->client()->id(),
								   response->client());
		
		BOOST_LOG_SEV(log, LogLevel::Debug) <<
		format("Sending client connect request (id = '%d').") % client->id();
		sendClientConnected(client->id(), version, client->room());
		BOOST_LOG_SEV(log, LogLevel::Debug) <<
		format("Client connect request sent (id = '%d').") % client->id();
	}
	
    void MasterNode::versionAdded(Master &, const std::string &version)
    {
        sendLoadVersion(version);
    }

    void MasterNode::versionRemoved(Master &, const std::string &version)
    {
        sendUnloadVersion(version);
    }

    void MasterNode::connectionShutdown()
	{
		auto self = shared_from_this();
		
		connected = false;
        master().removeListener(this);
		master().removeNode(this);
    }

    void MasterNode::heartbeat(Master &param)
    {
        sendHeartbeat();
    }
	
	void MasterNode::clientDisconnected(std::uint64_t cId)
	{
		removeClient(cId);
	}
	
	void MasterNode::removeClient(std::uint64_t clientId)
	{
		std::lock_guard<std::recursive_mutex> lock(domainsMutex);
		for (auto& item: domains)
			item.second.clients.erase(clientId);
	}
	
	std::size_t MasterNode::numClients()
	{
		std::size_t count = 0;
		
		std::lock_guard<std::recursive_mutex> lock(domainsMutex);
		for (const auto& item: domains)
			count += item.second.clients.size();
		
		return count;
	}
	
	std::size_t MasterNode::numRooms()
	{
		std::size_t count = 0;
		
		std::lock_guard<std::recursive_mutex> lock(domainsMutex);
		for (const auto& item: domains)
			count += item.second.rooms.size();
		
		return count;
	}
	
	std::uint64_t MasterNode::uptime()
	{
		return static_cast<std::uint64_t>
		((boost::posix_time::second_clock::universal_time() -
		  startTime).total_seconds());;
	}
	
	boost::optional<std::string> MasterNode::findDomainForRoom(const std::string& room)
	{
		if (room.empty())
			return boost::none;
		
		std::lock_guard<std::recursive_mutex> lock(domainsMutex);
		for (const auto& item: domains) {
			auto it = item.second.rooms.find(room);
			if (it != item.second.rooms.end())
				return item.first;
		}
		
		return boost::none;
	}
	
	std::vector<MasterNode::DomainStatus> MasterNode::domainStatuses()
	{
		std::vector<DomainStatus> ret;
		
		std::lock_guard<std::recursive_mutex> lock(domainsMutex);
		ret.reserve(domains.size());
		for (const auto& item: domains) {
			DomainStatus status;
			status.versionName = item.second.versionName;
			status.uptime = static_cast<std::uint64_t>
			((boost::posix_time::second_clock::universal_time() -
			  item.second.startTime).total_seconds());
			status.numClients = item.second.clients.size();
			status.numRooms = item.second.rooms.size();
			ret.push_back(status);
		}
		
		return ret;
	}
}
