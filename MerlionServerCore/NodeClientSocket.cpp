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
#include "NodeClientSocket.hpp"
#include "Library.hpp"
#include "Exceptions.hpp"

using boost::format;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
using pipes = asio::local::stream_protocol;

namespace mcore
{
	NodeClientSocket::NodeClientSocket(std::shared_ptr<Library> library,
									   const std::string& displayName, int stream) :
	library(library),
	socket(library->ioService()),
	_strand(library->ioService()),
	webSocket(socket)
	{
		log.setChannel(displayName);
		socket.assign(pipes(), stream);
	}
	NodeClientSocket::~NodeClientSocket()
	{
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Finalized.";
	}
	
	void NodeClientSocket::shutdown()
	{
		auto self = shared_from_this();
		volatile bool done = false;
		std::mutex doneMutex;
		std::condition_variable doneCond;
		down_ = true;
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Shutting down.";
		
		_strand.dispatch([self, this, &done, &doneMutex, &doneCond] {
			try {
				decltype(shutdownListeners) listeners;
				listeners.swap(shutdownListeners);
				
				for (auto& f: listeners) {
					try {
						f();
					} catch (...) {
						BOOST_LOG_SEV(log, LogLevel::Error)
						<< "Unhandled exception thrown while doing shutdown clean-up.: "
						<< boost::current_exception_diagnostic_information();
					}
				}
				
				webSocket.async_shutdown(asiows::web_socket_close_status_codes::normal_closure, "", false,
										 _strand.wrap([self, this] (const boost::system::error_code &) {
					boost::system::error_code error;
					socket.close(error);
				}));
				
			} catch (...) {
				BOOST_LOG_SEV(log, LogLevel::Error) <<
				"Unhandled exception thrown in the shutdown procedure.: " <<
				boost::current_exception_diagnostic_information();
			}
			
			std::lock_guard<std::mutex> lock(doneMutex);
			done = true;
			doneCond.notify_all();
		});
		
		std::unique_lock<std::mutex> lock(doneMutex);
		while (!done) {
			doneCond.wait(lock);
		}
		
		BOOST_LOG_SEV(log, LogLevel::Debug) << "Shutdown completed.";
	}
	
	template <class Callback>
	void NodeClientSocket::receive(Callback &&cb)
	{
		struct ReceiveOperation
		{
			enum class State
			{
				ReceiveMessage,
				ReadMessage
			};
			State state = State::ReceiveMessage;
			std::shared_ptr<NodeClientSocket> socket;
			typename std::remove_reference<Callback>::type callback;
			decltype(shutdownListeners)::iterator shutdownListenersIter;
			
			ReceiveOperation(const std::shared_ptr<NodeClientSocket> &socket,
					   Callback &&cb) :
			socket(socket),
			callback(cb)
			{ }
			
			void done()
			{
				socket->receiving_ = false;
				if (!socket->shutdownListeners.empty()) {
					socket->shutdownListeners.erase(shutdownListenersIter);
				}
			}
			
			void operator () (const boost::system::error_code& error, std::size_t count = 0)
			{
				switch (state) {
					case State::ReceiveMessage:
						if (error) {
							socket->receiveBuffer.resize(0);
							done();
							callback(socket->receiveBuffer, error.message());
						} else {
							state = State::ReadMessage;
							perform();
						}
						break;
					case State::ReadMessage:
						if (error) {
							socket->receiveBuffer.resize(0);
							done();
							callback(socket->receiveBuffer, error.message());
						} else if (count == 0) {
							socket->receiveBuffer.resize(socket->receiveBufferLen);
							done();
							
							try {
								if (!socket->down_)
									callback(socket->receiveBuffer, std::string());
							} catch (...) {
								BOOST_LOG_SEV(socket->log, LogLevel::Error)
								<< "Error occured in packet receive handler.: "
								<< boost::current_exception_diagnostic_information();
							}
						} else {
							socket->receiveBufferLen += count;
							
							if (socket->receiveBufferLen > 65536) {
								// Too long.
								socket->receiveBuffer.resize(0);
								auto self = socket;
								socket->webSocket.async_shutdown(asiows::web_socket_close_status_codes::too_big,
																 "Packet size is limited to 65536 bytes.", true,
																 socket->_strand.wrap([self](const boost::system::error_code&){}));
								callback(socket->receiveBuffer, "Received packet is too long.");
								return;
							}
							
							socket->receiveBuffer.resize(socket->receiveBufferLen);
							perform();
						}
						break;
				}
			}
			
			void perform()
			{
				switch (state) {
					case State::ReceiveMessage:
						{
							auto cb = callback;
							socket->shutdownListeners.emplace_back([cb] {
								vslim::vector_slim<char> dummyBuffer;
								cb(dummyBuffer, "Socket is disconnected.");
							});
						}
						shutdownListenersIter = socket->shutdownListeners.begin();
						socket->webSocket.async_receive_message(std::move(*this));
						break;
					case State::ReadMessage:
						socket->receiveBuffer.resize(socket->receiveBufferLen + 4096);
						socket->webSocket.async_read_some(asio::buffer(socket->receiveBuffer.data() + socket->receiveBufferLen,
																	   4096), std::move(*this));
						break;
				}
			}
		};
		
		auto self = shared_from_this();
		
		_strand.dispatch([self, this, cb] () mutable {
			if (receiving_) {
				cb(receiveBuffer, std::string("Cannot perform multiple reads at once."));
				return;
			} else if (down_) {
				cb(receiveBuffer, std::string("Socket is disconnected."));
				return;
			}
			
			receiving_ = true;
			
			receiveBufferLen = 0;
			receiveBuffer.resize(0);
			
			ReceiveOperation op(shared_from_this(), std::move(cb));
			op.perform();
		});
		
	}
	
	template <class Callback>
	void NodeClientSocket::send(const void *data, std::size_t length,
								Callback &&cb)
	{
		if (length > 65536) {
			// Too long. (limitation of Merlion, not WebSocket)
			MSCThrow(InvalidArgumentException("Packet cannot be longer than 65536 bytes.", "length"));
		}
		
		auto self = shared_from_this();
		auto buffer = std::make_shared<vslim::vector_slim<char>>(length);
		std::memcpy(buffer->data(), data, length);
		
		_strand.dispatch([self, buffer, cb, this] () mutable {
			if (down_) {
				cb(std::string("Socket is disconnected."));
				return;
			}
			
			{
				shutdownListeners.emplace_back([cb] {
					cb("Socket is disconnected.");
				});
			}
			auto it = shutdownListeners.begin();
			
			asiows::web_socket_message_header header;
			webSocket.async_send_message(header, asio::buffer(buffer->data(), buffer->size()),
										 _strand.wrap([self, buffer, cb, this, it] (const boost::system::error_code& error) {
				
				if (!shutdownListeners.empty()) {
					shutdownListeners.erase(it);
				}
				
				if (error) {
					cb(error.message());
				} else {
					cb(std::string());
				}
			}));
		});
	}
	
}

extern "C"
{
	
	MSCResult MSCClientSocketDestroy(MSCClientSocket socket)
	{
		return mcore::convertExceptionsToResultCode([&] {
			if (!socket)
				MSCThrow(mcore::InvalidArgumentException("socket"));
			auto &h = mcore::NodeClientSocket::fromHandle(socket);
			h->shutdown();
			delete &h;
		});
	}
	MSCResult MSCClientSocketReceive(MSCClientSocket socket,
								MSCClientSocketReceiveCallback callback,
								void *userdata)
	{
		return mcore::convertExceptionsToResultCode([&] {
			if (!socket)
				MSCThrow(mcore::InvalidArgumentException("socket"));
			auto &h = mcore::NodeClientSocket::fromHandle(socket);
			h->receive([callback, userdata] (const vslim::vector_slim<char>& buffer, const std::string& error) {
				std::uint32_t ret;
				if (error.empty()) {
					ret = callback(buffer.data(), static_cast<std::uint32_t>(buffer.size()),
							 nullptr, userdata);
				} else {
					ret = callback(nullptr, 0,
							 error.c_str(), userdata);
				}
				if (ret) {
					MSCThrow(mcore::InvalidOperationException("MSCClientSocketReceiveCallback failed."));
				}
			});
		});
	}
	MSCResult MSCClientSocketSend(MSCClientSocket socket,
								const void *data, std::uint32_t dataLength,
								MSCClientSocketSendCallback callback,
								void *userdata)
	{
		return mcore::convertExceptionsToResultCode([&] {
			if (!socket)
				MSCThrow(mcore::InvalidArgumentException("socket"));
			if (dataLength > 0 && !data)
				MSCThrow(mcore::InvalidArgumentException("data"));
			auto &h = mcore::NodeClientSocket::fromHandle(socket);
			h->send(data, dataLength, [callback, userdata] (const std::string& error) {
				std::uint32_t ret;
				if (error.empty()) {
					ret = callback(nullptr, userdata);
				} else {
					ret = callback(error.c_str(), userdata);
				}
				if (ret) {
					MSCThrow(mcore::InvalidOperationException("MSCClientSocketSendCallback failed."));
				}
			});
		});
	}
}

