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
#include "Public.h"

namespace mcore
{
	
	enum class LogLevel : int
	{
		Debug = static_cast<int>(MSCLS_Debug),
		Info =  static_cast<int>(MSCLS_Info),
		Warn =  static_cast<int>(MSCLS_Warn),
		Error = static_cast<int>(MSCLS_Error),
		Fatal = static_cast<int>(MSCLS_Fatal)
	};
	
	struct LogEntry
	{
		LogLevel level;
		std::string source;
		std::string channel;
		std::string host;
		std::string message;
		
		LogEntry() = default;
		LogEntry(const MSCLogEntry& e)
		{
			level = static_cast<LogLevel>(e.severity);
			source = e.source ? e.source : std::string();
			channel = e.channel ? e.channel : std::string();
			host = e.host ? e.host : std::string();
			message = e.message ? e.message : std::string();
		}
		
		operator MSCLogEntry() const
		{
			MSCLogEntry e;
			e.severity = static_cast<MSCLogSeverity>(level);
			e.source = source.c_str();
			e.channel = channel.c_str();
			e.host = host.c_str();
			e.message = message.c_str();
			return e;
		}
		
		void log();
	};
		
	class Logger:
	public boost::log::sources::severity_logger<LogLevel>
	{
	public:
		Logger(const std::string& source);
		Logger(const std::type_info& source);
		void setChannel(const std::string&);
		void setHost(const std::string&);
	};
	template <class T>
	class TypedLogger: public Logger
	{
		public:
		TypedLogger(): Logger(typeid(T)) { }
	};
}



