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
#include <string>

namespace mcore
{
    std::pair<boost::asio::ip::address, int> parseEndpoint(const std::string&);

    boost::asio::ip::tcp::endpoint parseTcpEndpoint(const std::string&);

    template <class T>
    void serializeDictionary(std::vector<char> &output, const T& dic)
    {
        for (const auto& ent: dic) {

        };
    }

    template <class T>
    std::vector<char> serializeDictionary(const T& dic)
    {
        std::vector<char> buffer;
        buffer.reserve(512);
        serializeDictionary(buffer, dic);
        return buffer;
    }
	
	class CountingCombiner
	{
	public:
		using result_type = std::size_t;
		
		template <class Iterator>
		result_type operator()(Iterator first, Iterator last) const
		{
			std::size_t count = 0;
			while (first != last)
			{
				try
				{
					*first;
				}
				catch(const boost::signals2::expired_slot &) {}
				++first;
				++count;
			}
			return count;
		}
	};
}

