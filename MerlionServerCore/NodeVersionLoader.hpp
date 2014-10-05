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
