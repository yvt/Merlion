#pragma once

#include "Logging.hpp"

namespace mcore
{
	class Library;
	
	class NodeVersionLoader:
	public std::enable_shared_from_this<NodeVersionLoader>,
	boost::noncopyable
	{
	public:
		enum class LoadResult
		{
			Downloaded = 0,
			Cancelled,
			Failed
		};
		
	private:
		TypedLogger<NodeVersionLoader> log;
		boost::asio::io_service service;
		std::unique_ptr<boost::asio::io_service::work> work;
		std::thread thread;
		
		struct DownloadItem
		{
			std::string version;
			volatile int retryCount;
		};
		
		std::list<DownloadItem> downloadQueue;
		std::mutex downloadQueueMutex;
		
		volatile bool disposed;
		
		enum class State
		{
			Idle,
			Connecting,
			SendingHeader,
			ReceivingHeader,
			ReceivingData,
			
			Waiting
		};
		
		volatile State state;
		volatile std::uint64_t remainingBytes;
		std::unique_ptr<std::ofstream> file;
		std::vector<char> readBuffer;
		boost::posix_time::ptime lastReceiveTime;
		
		boost::asio::ip::tcp::endpoint endpoint;
		boost::asio::ip::tcp::socket socket;
		boost::asio::deadline_timer timer;
		
		DownloadItem& currentDownloadItem();
		void checkThread();
		bool isDownloading();
		void startDownload();
		void sendHeader();
		void receiveHeader();
		void receiveData(std::uint64_t size);
		void receiveChunk();
		void setWatchDogTimer();
		void cancelAll();
		void downloadCancelled();
		void downloadFailed(const std::string& msg);
		
		void worker();
		
		void checkQueue();
	public:
		NodeVersionLoader(const boost::asio::ip::tcp::endpoint &ep);
		~NodeVersionLoader();
		
		void download(const std::string& version);
		void shutdown();
		
		boost::signals2::signal<void(const std::string&, bool& cancel)> onVersionAboutToBeDownloaded;
		boost::signals2::signal<void(const std::string&)> onVersionDownloaded;
		boost::signals2::signal<void(const std::string&)> onVersionDownloadFailed;
		boost::signals2::signal<void(const std::string&, std::string&)> onNeedsDownloadFileName;
	};
}
