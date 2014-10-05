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
