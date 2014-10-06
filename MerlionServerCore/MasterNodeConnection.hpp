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

#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <list>
#include "Logging.hpp"

namespace mcore
{
    class Master;
	class MasterNode;
    class MasterNodeConnectionHandler;

    class MasterNodeConnection :
	public std::enable_shared_from_this<MasterNodeConnection>,
	boost::noncopyable
    {
    public:
        using socketType = boost::asio::ip::tcp::socket;
        using ioMutexType = std::recursive_mutex;
		using ptr = std::shared_ptr<MasterNodeConnection>;
    private:
        friend class Master;
        Master&_master;
        std::list<std::shared_ptr<MasterNodeConnection>>::iterator iter;
		TypedLogger<MasterNodeConnection> log;
		
        boost::asio::io_service& service;
        bool accepted;
        volatile bool disposed;

        std::shared_ptr<socketType> socket;
        std::shared_ptr<boost::asio::buffered_stream<socketType>> stream;

        std::weak_ptr<MasterNodeConnectionHandler> handler;

        ioMutexType mutex;


    public:
        MasterNodeConnection(Master& master);
        ~MasterNodeConnection();

        Master& master() const { return _master; }
		
		// tired to add "nodes" to Master:)
		std::shared_ptr<MasterNode> masterNode();

        void didAccept();
        void shutdown();
		
		void setChannelName(const std::string&); // For logging
		
		void performShutdownByError(const std::string&);

        socketType& tcpSocket();
        template <class Buffer, class Callback>
        void readAsync(const Buffer& buffer, Callback cb)
        {
            std::lock_guard<ioMutexType> lock(mutex);
            auto self = shared_from_this();
            async_read(tcpSocket(), buffer,
            [self, cb]
            (const boost::system::error_code& error, std::size_t readCount) {
                std::lock_guard<ioMutexType> lock(self->mutex);

                if (self->disposed || error == boost::asio::error::operation_aborted) {
                    return;
                }

                cb(error, readCount);
            });
		}

        template <class Buffer, class Callback>
        void writeAsync(const Buffer& buffer, Callback cb)
        {
            std::lock_guard<ioMutexType> lock(mutex);
            auto self = shared_from_this();
            async_write(tcpSocket(), buffer,
            [self, cb]
                    (const boost::system::error_code& error, std::size_t readCount) {
                std::lock_guard<ioMutexType> lock(self->mutex);

                if (self->disposed || error == boost::asio::error::operation_aborted) {
                    return;
                }

                cb(error, readCount);
            });
		}
		
		// For compatibility with AsyncPipe.hpp
		template <class Buffer, class Callback>
		void async_read_some(const Buffer& buffer, Callback cb)
		{
			std::lock_guard<ioMutexType> lock(mutex);
			auto self = shared_from_this();
			tcpSocket().async_read_some(buffer,
			[self, cb]
			(const boost::system::error_code& error, std::size_t readCount) mutable {
				std::lock_guard<ioMutexType> lock(self->mutex);
				
				if (self->disposed || error == boost::asio::error::operation_aborted) {
					return;
				}
				
				cb(error, readCount);
			});
		}
		template <class Buffer, class Callback>
		void async_write_some(const Buffer& buffer, Callback cb)
		{
			std::lock_guard<ioMutexType> lock(mutex);
			auto self = shared_from_this();
			tcpSocket().async_write_some(buffer,
			[self, cb]
			(const boost::system::error_code& error, std::size_t readCount) mutable {
				std::lock_guard<ioMutexType> lock(self->mutex);
				
				if (self->disposed || error == boost::asio::error::operation_aborted) {
					return;
				}
				
				cb(error, readCount);
			});
		}

        ioMutexType& ioMutex() { return mutex; }
        bool isDisposed() const { return disposed; }

        bool isBuffered() const { return stream != nullptr; }
        void enableBuffering();
    };

    class MasterNodeConnectionHandler
    {
    public:
        virtual ~MasterNodeConnectionHandler() { }
        virtual void service() = 0;
        virtual void connectionShutdown() { }
        virtual void connectionShutdownDone() { }
    };

}
