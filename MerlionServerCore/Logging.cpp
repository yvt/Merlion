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

#include <boost/log/core.hpp>
#include <boost/log/sinks.hpp>
#if BOOST_VERSION >= 105600
#include <boost/core/demangle.hpp>
#define MSC_BOOST_DEMANGLE boost::core::demangle
#else
#include <boost/units/detail/utility.hpp>
#define MSC_BOOST_DEMANGLE boost::units::detail::demangle
#endif
#include "Logging.hpp"
#include "Utils.hpp"
#include "Exceptions.hpp"

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace sinks = boost::log::sinks;

namespace mcore
{
	Logger::Logger(const std::string &source)
	{
		add_attribute("Source", attrs::constant<std::string>(source));
	}
	Logger::Logger(const std::type_info &source):
	Logger(MSC_BOOST_DEMANGLE(source.name()))
	{
		setHost("Local");
	}
	
	void Logger::setChannel(const std::string &channel)
	{
		auto r = add_attribute("Channel", attrs::constant<std::string>(channel));
		r.first->second = attrs::constant<std::string>(channel);
	}
	
	void Logger::setHost(const std::string &host)
	{
		auto r = add_attribute("Host", attrs::constant<std::string>(host));
		r.first->second = attrs::constant<std::string>(host);
	}
	
	void LogEntry::log()
	{
		auto core = logging::core::get();
		logging::attribute_set attrs;
		attrs.insert("Channel", attrs::constant<std::string>(channel));
		attrs.insert("Host", attrs::constant<std::string>(host));
		attrs.insert("Source", attrs::constant<std::string>(source));
		attrs.insert("Message", attrs::constant<std::string>(message));
		attrs.insert("Severity", attrs::constant<LogLevel>(level));
		auto rec = core->open_record(attrs);
		if (rec) {
			core->push_record(std::move(rec));
		}
	}
	
	class CustomLogSink: public boost::log::sinks::basic_sink_backend
	<sinks::combine_requirements<
	sinks::concurrent_feeding
	>::type>,
	public boost::enable_shared_from_this<CustomLogSink>
	{
		struct Sink
		{
			MSCLogSink func;
			void *userdata;
		};
		std::list<Sink> sinks;
		std::mutex mutex;
	public:
		CustomLogSink()
		{
			
			
		}
		void consume(boost::log::record_view const &rec)
		{
			std::string def = "(null)";
			const auto& values = rec.attribute_values();
			auto sev =
			logging::extract_or_default<LogLevel>(values["Severity"], LogLevel::Info);
			auto source =
			logging::extract_or_default<std::string>(values["Source"], def);
			auto channel =
			logging::extract_or_default<std::string>(values["Channel"], def);
			auto host =
			logging::extract_or_default<std::string>(values["Host"], def);
			
			auto msg =
			logging::extract_or_default<std::string>(values["Message"], def);
			
			LogEntry entry;
			entry.level = sev;
			entry.source = source;
			entry.host = host;
			entry.message = msg;
			entry.channel = channel;
			
			MSCLogEntry centry = entry;
			
			std::lock_guard<std::mutex> lock(mutex);
			for (const auto& s: sinks)
				s.func(&centry,
					   s.userdata);
		}
		MSCLogSinkHandle addSink(MSCLogSink sink, void *userdata)
		{
			std::lock_guard<std::mutex> lock(mutex);
			Sink s = {sink, userdata};
			sinks.emplace_front(s);
			return reinterpret_cast<MSCLogSinkHandle>(new std::list<Sink>::iterator(sinks.begin()));
		}
		void removeSink(MSCLogSinkHandle handle)
		{
			if (handle == nullptr) {
				MSCThrow(InvalidArgumentException("handle"));
			}
			
			auto *it = reinterpret_cast<std::list<Sink>::iterator *>(handle);
			
			std::lock_guard<std::mutex> lock(mutex);
			sinks.erase(*it);
			
			delete it;
		}
	};
	
	
	static boost::shared_ptr<CustomLogSink> sink =
	boost::make_shared<CustomLogSink>();
	
	struct CustomLogSinkInitializer
	{
		CustomLogSinkInitializer()
		{
			auto core = boost::log::core::get();
			if (sink == nullptr) {
				sink = boost::make_shared<CustomLogSink>();
			}
			auto s = boost::make_shared<sinks::unlocked_sink<CustomLogSink>>(sink);
			core->add_sink(s);
		}
	};
	
}

extern "C" MSCResult MSCAddLogSink(MSCLogSink sink, void *userdata, MSCLogSinkHandle *outHandle)
{
	return mcore::convertExceptionsToResultCode([&] {
		static mcore::CustomLogSinkInitializer initializer;
		
		if (sink == nullptr) {
			MSCThrow(mcore::InvalidArgumentException("sink"));
		}
		
		auto h = mcore::sink->addSink(sink, userdata);
		if (outHandle)
			*outHandle = h;
	});
}
extern "C" MSCResult MSCRemoveLogSink(MSCLogSinkHandle handle)
{
	return mcore::convertExceptionsToResultCode([&] {
		mcore::sink->removeSink(handle);
	});
}
