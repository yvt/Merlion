#include "Prefix.pch"
#include "NodeVersionLoader.hpp"
#include "Packet.hpp"
#include "Protocol.hpp"
#include <fstream>

using boost::format;
using boost::asio::ip::tcp;
using boost::system::error_code;
namespace asio = boost::asio;

namespace mcore
{
	
	NodeVersionLoader::NodeVersionLoader(const boost::asio::ip::tcp::endpoint &ep):
	thread([this] { worker(); }),
	service(),
	disposed(false),
	socket(service),
	endpoint(ep),
	state(State::Idle),
	timer(service),
	readBuffer(16384)
	{
		log.setChannel(std::string("(none)"));
		
		work.reset(new boost::asio::io_service::work(service));
		
	}
	
	NodeVersionLoader::~NodeVersionLoader()
	{
		shutdown();
	}
	
	void NodeVersionLoader::shutdown()
	{
		if (disposed)
			return;
		
		disposed = true;
		service.dispatch([this] { cancelAll(); });
		work.reset();
		thread.join();
	}
	
	void NodeVersionLoader::download(const std::string &version)
	{
		auto self = shared_from_this();
		
		DownloadItem item;
		item.version = version;
		item.retryCount = 3;
		{
			std::lock_guard<std::mutex> lock(downloadQueueMutex);
			for (auto& item: downloadQueue) {
				if (item.version == version) {
					item.retryCount = 3;
					return;
				}
			}
			downloadQueue.emplace_back(std::move(item));
		}
		
		service.dispatch([this, self]() {
			checkQueue();
		});
	}
	
	void NodeVersionLoader::checkThread()
	{
		assert(std::this_thread::get_id() == thread.get_id());
	}
	
	bool NodeVersionLoader::isDownloading()
	{
		return state != State::Idle;
	}
	
	void NodeVersionLoader::checkQueue()
	{
		auto self = shared_from_this();
		checkThread();
		
		if (isDownloading())
			return;
		
		while (true) {
			{
				std::lock_guard<std::mutex> lock(downloadQueueMutex);
				if (downloadQueue.empty())
					return;
				
				auto& item = downloadQueue.front();
				bool cancel = false;
				try {
					onVersionAboutToBeDownloaded(item.version, cancel);
				} catch (...) {
					BOOST_LOG_SEV(log, LogLevel::Error) <<
					"onVersionAboutToBeDownloaded failed: " <<
					boost::current_exception_diagnostic_information();
					cancel = true;
				}
				if (cancel) {
					downloadQueue.erase(downloadQueue.begin());
					continue;
				}
				
			}
			
			startDownload();
		}
	}
	
	auto NodeVersionLoader::currentDownloadItem() -> DownloadItem&
	{
		std::lock_guard<std::mutex> lock(downloadQueueMutex);
		auto& item = downloadQueue.front();
		
		return item;
	}
	
	void NodeVersionLoader::startDownload()
	{
		auto self = shared_from_this();
		checkThread();
		
		auto& item = currentDownloadItem();
		{
			std::lock_guard<std::mutex> lock(downloadQueueMutex);
			--item.retryCount;
		}
		
		state = State::Connecting;
		log.setChannel("Download:" + item.version);
		
		std::string fileName;
		auto ver = item.version;
		try {
			onNeedsDownloadFileName(ver, fileName);
			if (fileName.empty()) {
				MSCThrow(InvalidOperationException("File name is empty."));
			}
		} catch (...) {
			downloadFailed("Failed to determine the downloaded file path.: " +
						   boost::current_exception_diagnostic_information());
			return;
		}
		try {
			file.reset(new std::ofstream(fileName.c_str(),
										 std::ios::out |
										 std::ios::trunc |
										 std::ios::binary));
			
			file->exceptions(std::ios::failbit |
							 std::ios::badbit);
		} catch (...) {
			file.reset();
			downloadFailed("Failed to open the destination file '" + fileName + "'.: " +
						   boost::current_exception_diagnostic_information());
			return;
		}
		
		BOOST_LOG_SEV(log, LogLevel::Info) <<
		"Connecting to the master server.";
		
		socket.async_connect(endpoint, [this, self, &item](const error_code& error) {
			checkThread();
			
			if (disposed) {
				downloadCancelled();
				return;
			}
			if (error) {
				downloadFailed(error.message());
				return;
			}
			
			sendHeader();
		});
	}
	
	void NodeVersionLoader::sendHeader()
	{
		auto self = shared_from_this();
		checkThread();
		
		auto& item = currentDownloadItem();
		
		state = State::SendingHeader;
		
		PacketGenerator gen;
		gen.write(VersionDownloadRequestMagic);
		gen.writeString(item.version);
		
		auto buf = std::make_shared<std::vector<char>>(std::move(gen.vector()));
		
		timer.expires_from_now(boost::posix_time::seconds(8));
		timer.async_wait([this, self] (const error_code& error) {
			if (error) {
				return;
			}
			if (disposed) {
				return;
			}
			if (state == State::SendingHeader) {
				BOOST_LOG_SEV(log, LogLevel::Warn) <<
				"Timed out.";
				socket.cancel();
			}
		});
		
		asio::async_write(socket, asio::buffer(buf->data(), buf->size()), [this, self, &item](const error_code& error, std::size_t) {
			checkThread();
			
			if (disposed) {
				downloadCancelled();
				return;
			}
			if (error) {
				downloadFailed(error.message());
				return;
			}
			timer.cancel();
			
			receiveHeader();
		});
	}
	
	void NodeVersionLoader::receiveHeader()
	{
		auto self = shared_from_this();
		checkThread();
		
		auto& item = currentDownloadItem();
		
		state = State::ReceivingHeader;
		
		auto buf = std::make_shared<std::uint64_t>();
		
		timer.expires_from_now(boost::posix_time::seconds(8));
		timer.async_wait([this, self] (const error_code& error) {
			if (error) {
				return;
			}
			if (disposed) {
				return;
			}
			if (state == State::ReceivingHeader) {
				BOOST_LOG_SEV(log, LogLevel::Warn) <<
				"Timed out.";
				socket.cancel();
			}
		});
		
		asio::async_read(socket, asio::buffer(buf.get(), 8), [this, self, &item, buf](const error_code& error, std::size_t) {
			checkThread();
			
			if (disposed) {
				downloadCancelled();
				return;
			}
			if (error) {
				downloadFailed(error.message());
				return;
			}
			timer.cancel();
			
			auto size = *buf;
			if (size == 0) {
				downloadFailed("Version cannot be downloaded for some reason.");
				return;
			}
			
			receiveData(size);
		});
	}
	void NodeVersionLoader::receiveData(std::uint64_t size)
	{
		auto self = shared_from_this();
		checkThread();
		
		state = State::ReceivingData;
		remainingBytes = size;
		lastReceiveTime = boost::posix_time::second_clock::local_time();
		
		setWatchDogTimer();
		receiveChunk();
	}
	
	void NodeVersionLoader::receiveChunk()
	{
		auto self = shared_from_this();
		checkThread();
		
		auto& item = currentDownloadItem();
		auto chunkSize = static_cast<std::size_t>
		(std::min<std::uint64_t>(readBuffer.size(), static_cast<std::uint64_t>(remainingBytes)));
		
		if (chunkSize == 0) {
			// Done!
			BOOST_LOG_SEV(log, LogLevel::Info) <<
			"Download completed.";
			
			auto ver = item.version;
			{
				std::lock_guard<std::mutex> lock(downloadQueueMutex);
				downloadQueue.pop_front();
				// note: `item` is a dangling reference now
			}
			
			file.reset();
			timer.cancel();
			
			onVersionDownloaded(ver);
			
			log.setChannel(std::string("(none)"));
			
			state = State::Waiting;
			timer.expires_from_now(boost::posix_time::seconds(1));
			timer.async_wait([this, self] (const error_code& error) {
				if (error) {
					return;
				}
				if (disposed) {
					BOOST_LOG_SEV(log, LogLevel::Debug) <<
					"Retry timer aborted.: Disposed.";
					return;
				}
				state = State::Idle;
				checkQueue();
			});
			return;
		}
		
		
		asio::async_read(socket, asio::buffer(readBuffer.data(), chunkSize), [this, self, &item, chunkSize](const error_code& error, std::size_t) {
			checkThread();
			
			if (disposed) {
				downloadCancelled();
				return;
			}
			if (error) {
				downloadFailed(error.message());
				return;
			}
			
			lastReceiveTime = boost::posix_time::second_clock::local_time();
			
			try {
				file->write(readBuffer.data(), chunkSize);
			} catch (...) {
				downloadFailed("Failed to write to the downloaded file.: " +
							   boost::current_exception_diagnostic_information());
				return;
			}
			remainingBytes -= static_cast<std::uint64_t>(chunkSize);
			
			receiveChunk();
		});
		
		
	}
	
	void NodeVersionLoader::setWatchDogTimer()
	{
		auto self = shared_from_this();
		checkThread();
		
		timer.expires_from_now(boost::posix_time::seconds(3));
		timer.async_wait([this, self] (const error_code& error) {
			if (error) {
				return;
			}
			if (disposed) {
				return;
			}
			if (state == State::ReceivingData) {
				auto now = boost::posix_time::second_clock::local_time();
				if (now - lastReceiveTime > boost::posix_time::seconds(10)) {
					BOOST_LOG_SEV(log, LogLevel::Warn) <<
					"Timed out.";
					socket.cancel();
				} else {
					setWatchDogTimer();
				}
			}
		});
	}
	
	void NodeVersionLoader::downloadCancelled()
	{
		auto self = shared_from_this();
		
		file.reset();
		
		try { socket.shutdown(tcp::socket::shutdown_both); } catch (...) { }
		try { socket.close(); } catch (...) { }
		log.setChannel(std::string("(none)"));
	}
	
	void NodeVersionLoader::downloadFailed(const std::string& msg)
	{
		auto self = shared_from_this();
		checkThread();
		
		auto& item = currentDownloadItem();
		
		file.reset();
		
		int retryCount;
		{
			std::lock_guard<std::mutex> lock(downloadQueueMutex);
			retryCount = item.retryCount;
		}
		if (retryCount == 0) {
			BOOST_LOG_SEV(log, LogLevel::Error) <<
			"Failed to download.: " << msg;
			
			auto ver = item.version;
			{
				std::lock_guard<std::mutex> lock(downloadQueueMutex);
				downloadQueue.pop_front();
				// note: `item` is a dangling reference now
			}
			
			onVersionDownloadFailed(ver);
			
		} else {
			BOOST_LOG_SEV(log, LogLevel::Warn) <<
			"Failed to download. Retrying.: " << msg;
		}
		
		log.setChannel(std::string("(none)"));
		
		state = State::Waiting;
		timer.expires_from_now(boost::posix_time::seconds(3));
		timer.async_wait([this, self] (const error_code& error) {
			if (error) {
				return;
			}
			if (disposed) {
				BOOST_LOG_SEV(log, LogLevel::Debug) <<
				"Retry timer aborted.: Disposed.";
				return;
			}
			state = State::Idle;
			checkQueue();
		});
		
	}
	
	void NodeVersionLoader::cancelAll()
	{
		assert(disposed);
		
		switch (state) {
			case State::Idle:
				break;
			case State::Connecting:
			case State::SendingHeader:
			case State::ReceivingHeader:
			case State::ReceivingData:
				timer.cancel();
				socket.cancel();
				break;
			case State::Waiting:
				timer.cancel();
				break;
		}
	}
	
	
	
	void NodeVersionLoader::worker()
	{
		service.run();
	}
	
}
