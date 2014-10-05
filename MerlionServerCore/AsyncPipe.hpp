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
    
    template <class InStream, class OutStream, class Callback>
    void startAsyncPipe(InStream& inStream, OutStream& outStream, std::size_t bufferSize, const Callback& callback)
    {
        struct State
        {
            std::vector<char> buffer;
            InStream& inStream;
            OutStream& outStream;
            Callback callback;
            std::uint64_t count = 0;
			std::function<void(const std::shared_ptr<State>&)> run;
			
            State(InStream& inStream, OutStream& outStream, std::size_t bufferSize, const Callback& callback):
                inStream(inStream),
                outStream(outStream),
                callback(callback)
            {
                buffer.resize(bufferSize);
            }
			~State()
			{
				
			}
        };
        auto state = std::make_shared<State>(inStream, outStream, bufferSize, callback);
		state->run = [](const std::shared_ptr<State>& state)
        {
            auto& s = *state;
            s.inStream.async_read_some(asio::buffer(s.buffer.data(), s.buffer.size()),
            [state](const boost::system::error_code& error, std::size_t readCount) {
                auto& s = *state;
                
                if (error) {
                    s.callback(error, s.count);
                    return;
                } else if (readCount == 0) {
                    s.callback(boost::system::error_code(), s.count);
                    return;
                }
                s.count += readCount;
                
                async_write(s.outStream, asio::buffer(s.buffer.data(), readCount),
                [state](const boost::system::error_code& error, std::size_t readCount) {
                    auto& s = *state;
                    
                    if (error) {
                        s.callback(error, s.count);
                        return;
                    }
                    
                    s.run(state);
                });
            });
        };
        state->run(state);
    }
}

