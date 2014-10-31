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
#include "WebSocket.hpp"

namespace asiows
{
	struct TESTSOCK
	{
	public:
		using lowest_layer_type = TESTSOCK;
		template <class Buf, class Fn>
		void async_read_some(Buf &&buf, Fn &&fn)
		{
			fn(boost::system::error_code(), 0);
		}
		template <class Buf, class Fn>
		void async_write_some(Buf &&buf, Fn &&fn)
		{
			fn(boost::system::error_code(), 0);
		}
	};
	
	// makes sure web_socket is valid
	void hoge()
	{
		web_socket_server<TESTSOCK> server;
		server.async_start_handshake([](const boost::system::error_code&){});
		
		auto &ws = server.socket();
		ws.async_shutdown(0, "blah", false,
						  [](const boost::system::error_code&){});
		ws.async_receive_message([](const boost::system::error_code&){});
		
		web_socket_frame_header hdr;
		ws.async_begin_write(hdr, [&](const boost::system::error_code &ec) {
			ws.async_write_some(boost::asio::buffer("hoge", 4), [&](const boost::system::error_code &ec, std::size_t) {
				ws.async_end_write([&](const boost::system::error_code &ec) {});
			});
		});
		ws.async_send_ping("blah", [](const boost::system::error_code &ec){});
		
	}
}
