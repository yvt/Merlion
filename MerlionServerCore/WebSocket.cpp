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
	namespace detail
	{
		namespace
		{
			web_socket_server_regex_t web_socket_server_regex_;
		}
		const web_socket_server_regex_t& web_socket_server_regex()
		{
			return web_socket_server_regex_;
		}
	}
	const char *http_status_text(int status)
	{
		const char *statusText = "Unknown";
		switch (status) {
			case 400:
				statusText = "Bad Request";
				break;
			case 401:
				statusText = "Unauthorized";
				break;
			case 402:
				statusText = "Payment Required";
				break;
			case 403:
				statusText = "Forbidden";
				break;
			case 404:
				statusText = "Not Found";
				break;
			case 405:
				statusText = "Method Not Allowed";
				break;
			case 406:
				statusText = "Not Acceptable";
				break;
			case 407:
				statusText = "Proxy Authentication Required";
				break;
			case 408:
				statusText = "Request Timeout";
				break;
			case 409:
				statusText = "Conflict";
				break;
			case 410:
				statusText = "Gone";
				break;
			case 411:
				statusText = "Length Required";
				break;
			case 412:
				statusText = "Precondition Failed";
				break;
			case 413:
				statusText = "Request Entity Too Large";
				break;
			case 414:
				statusText = "Request-URI Too Long";
				break;
			case 415:
				statusText = "Unsupported Media Type";
				break;
			case 416:
				statusText = "Requested Range Not Satisfiable";
				break;
			case 417:
				statusText = "Expectation Failed";
				break;
			case 500:
				statusText = "Internal Server Error";
				break;
			case 501:
				statusText = "Not Implemented";
				break;
			case 502:
				statusText = "Bad Gateway";
				break;
			case 503:
				statusText = "Service Unavailable";
				break;
			case 504:
				statusText = "Gateway Timeout";
				break;
			case 505:
				statusText = "HTTP Version Not Supported";
				break;
		}
		return statusText;
	}
	
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
		server.async_accept("hoge", [](const boost::system::error_code&){});
		server.async_reject(0, [](const boost::system::error_code&){});
		
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
