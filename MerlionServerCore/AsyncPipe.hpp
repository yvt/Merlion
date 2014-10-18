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
#include <vector>
#include <functional>

namespace mcore
{
    namespace asio = boost::asio;
	
	namespace detail
	{
		template <class InStream, class OutStream, class Callback>
		struct AsyncPipeState: public std::enable_shared_from_this<AsyncPipeState<InStream, OutStream, Callback>>
		{
			using State = AsyncPipeState;
			std::vector<char> buffer;
			InStream& inStream;
			OutStream& outStream;
			Callback callback;
			std::uint64_t count = 0;
			
			AsyncPipeState(InStream& inStream, OutStream& outStream, std::size_t bufferSize, const Callback& callback):
			inStream(inStream),
			outStream(outStream),
			callback(callback)
			{
				buffer.resize(bufferSize);
			}
			~AsyncPipeState() = default;
			
			void run()
			{
				inStream.async_read_some(asio::buffer(buffer.data(), buffer.size()),
										 ReadOperation(this->shared_from_this()));
			}
			
			struct Operation
			{
				static constexpr bool IsAsyncPipeStateOperation = true;
				std::shared_ptr<State> state;
				Operation(const std::shared_ptr<State>& state):
				state(state) {}
			};
			
			struct ReadOperation: public Operation
			{
				ReadOperation(const std::shared_ptr<State>& state):
				Operation(state) {}
				
				void operator ()
				(const boost::system::error_code& error, std::size_t count) const
				{
					auto& s = *this->state;
					
					if (error) {
						s.callback(error, s.count);
						return;
					} else if (count == 0) {
						s.callback(boost::system::error_code(), s.count);
						return;
					}
					s.count += count;
					
					async_write(s.outStream, asio::buffer(s.buffer.data(), count), WriteOperation(this->state));
				}
			};
			
			struct WriteOperation: public Operation
			{
				WriteOperation(const std::shared_ptr<State>& state):
				Operation(state) {}
				
				void operator ()
				(const boost::system::error_code& error, std::size_t count) const
				{
					auto& s = *this->state;
					
					if (error) {
						s.callback(error, s.count);
						return;
					}
					
					s.run();
				}
			};
			
		};
		
		/* won't work for some reason...
		template <typename Function, class InStream, class OutStream, class Callback>
		inline void asio_handler_invoke
		(Function function,
		 typename AsyncPipeState<InStream, OutStream, Callback>::Operation *thisHandler)
		{
			boost_asio_handler_invoke_helpers::invoke
			(function, thisHandler->state->callback);
		}
		*/
		
		template <typename Function, typename Operation>
		inline typename
		std::enable_if<Operation::IsAsyncPipeStateOperation>::type
		asio_handler_invoke
		(Function function, Operation *thisHandler)
		{
			boost_asio_handler_invoke_helpers::invoke
			(function, thisHandler->state->callback);
		}
		
	}
	
    template <class InStream, class OutStream, class Callback>
    void startAsyncPipe(InStream& inStream, OutStream& outStream, std::size_t bufferSize, const Callback& callback)
    {
		using State = detail::AsyncPipeState<InStream, OutStream, Callback>;
		
        auto state = std::make_shared<State>(inStream, outStream, bufferSize, callback);
        state->run();
    }
}