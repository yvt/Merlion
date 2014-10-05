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
	class MasterClient;
	
	class MasterNodeClientStream :
	public MasterNodeConnectionHandler,
	public std::enable_shared_from_this<MasterNodeClientStream>
	{
		TypedLogger<MasterNodeClientStream> log;
		
		std::shared_ptr<MasterNodeConnection> connection;
		std::shared_ptr<MasterClient> client;
		
		void shutdown();
	public:
		MasterNodeClientStream(const std::shared_ptr<MasterNodeConnection>& connection);
		~MasterNodeClientStream();
		
		void service() override;
		void connectionShutdown() override;
	};
}
