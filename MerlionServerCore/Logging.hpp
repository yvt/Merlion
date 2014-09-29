#pragma once
#include "Public.h"

namespace mcore
{
	
	enum class LogLevel : int
	{
		Debug = static_cast<int>(MSCLS_Debug),
		Info = static_cast<int>(MSCLS_Info),
		Warn = static_cast<int>(MSCLS_Warn),
		Error = static_cast<int>(MSCLS_Error),
		Fatal = static_cast<int>(MSCLS_Fatal),
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



