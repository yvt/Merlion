#include "Prefix.pch"
#include "MasterNodeConnection.hpp"
#include "Exceptions.hpp"
#include "Master.hpp"
#include "Library.hpp"
#include "Protocol.hpp"
#include <boost/format.hpp>
#include <cassert>
#include "MasterNode.hpp"
#include "MasterNodeClientStream.hpp"

namespace asio = boost::asio;
using boost::format;

namespace mcore
{
    MasterNodeConnection::MasterNodeConnection(Master &master):
        _master(master),
        service(master.ioService()),
        accepted(false),
        disposed(false)
    {
        socket = std::make_shared<socketType>(master.library()->ioService());
    }

    MasterNodeConnection::~MasterNodeConnection()
    {
    }
	
	std::shared_ptr<MasterNode> MasterNodeConnection::masterNode()
	{
		std::lock_guard<ioMutexType> lock(mutex);
		auto ret = std::dynamic_pointer_cast<MasterNode>(handler);
		if (ret != nullptr && !ret->isConnected())
			ret.reset();
		return ret;
	}

    boost::asio::ip::tcp::socket &MasterNodeConnection::tcpSocket()
    {
        if (isBuffered())
            return stream->next_layer();
        else
            return *socket;
    }

    void MasterNodeConnection::enableBuffering()
    {
        // FIXME: enable buffering
        /*
        if (isBuffered())
            return;
        stream.reset(new boost::asio::buffered_stream<socketType>
                (*socket));*/
    }
	
	void MasterNodeConnection::setChannelName(const std::string &name)
	{
		log.setChannel(name);
	}

    void MasterNodeConnection::didAccept()
	{
		auto self = shared_from_this();
        std::lock_guard<ioMutexType> lock(mutex);

        accepted = true;
		log.setChannel(str(format("Unknown [%s]") % tcpSocket().remote_endpoint()));

        auto headerBuffer = std::make_shared<std::uint32_t>();

        readAsync(asio::buffer(headerBuffer.get(), 4),
        [this, headerBuffer]
        (const boost::system::error_code& error, std::size_t readCount){
            try {
                if (error) {
                    MSCThrow(boost::system::system_error(error));
                }

                std::uint32_t header = *headerBuffer;

                if (header == ControlStreamHeaderMagic) {
                    handler = std::make_shared<MasterNode>(*this);
                } else if (header == VersionDownloadRequestMagic) {
                    MSCThrow(NotImplementedException());
				} else if (header == DataStreamMagic) {
					handler = std::make_shared<MasterNodeClientStream>(*this);
                } else {
                    MSCThrow(InvalidOperationException(
                            str(format("Invalid header magic 0x%08x received.") % header)));
                }

                assert(handler != nullptr);

				handler->service();

            } catch (std::exception& ex) {
                performShutdownByError(ex);
            }
        });

    }

    void MasterNodeConnection::performShutdownByError(
            const std::exception& ex)
	{
		auto self = shared_from_this();
		
		BOOST_LOG_SEV(log, LogLevel::Warn) << "Shutting down because of an error: " << boost::diagnostic_information(ex);
        shutdown();
    }

    void MasterNodeConnection::shutdown()
	{
		auto self = shared_from_this();
		
        std::lock_guard<ioMutexType> lock(mutex);
		if (disposed)
			return;
		
		
		BOOST_LOG_SEV(log, LogLevel::Info) << "Shutting down.";
		
        if (handler != nullptr)
            handler->connectionShutdown();

		try { tcpSocket().shutdown(socketType::shutdown_both); } catch(...) { }
        tcpSocket().cancel();

        if (handler != nullptr)
            handler->connectionShutdownDone();

        if (accepted) {
            _master.removeNodeConnection(iter);
            accepted = false;
        }

        disposed = true;
    }

}
