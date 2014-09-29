#include "Prefix.pch"
#include "Utils.hpp"
#include "Exceptions.hpp"

namespace mcore
{
    std::pair<boost::asio::ip::address, int> parseEndpoint
            (const std::string& text)
    {
        // Find address/port separator
        auto pos = text.rfind(':');
        if (pos == std::string::npos) {
            MSCThrow(InvalidFormatException());
        }

        int port;
        try {
            port = std::stoi(text.substr(pos + 1));
        } catch (...) {
            MSCThrow(InvalidFormatException("Invalid endpoint port number."));
        }

        if (port < 1 || port > 65535) {
            MSCThrow(InvalidFormatException("Port number is out of range."));
        }

        std::string addressPart = text.substr(0, pos);

        try {
            boost::asio::io_service io_service;
            boost::asio::ip::tcp::resolver resolver(io_service);
            boost::asio::ip::tcp::resolver::query query(addressPart, std::string());

            auto result = resolver.resolve(query);
            for (;result != boost::asio::ip::tcp::resolver::iterator(); ++result) {
                auto address = result->endpoint().address();
                return std::make_pair(address, port);
            }

            MSCThrow(SystemErrorException("Specified endpoint could not be converted to an IP address/port pair."));
        } catch (boost::system::system_error& e) {
            MSCThrow(SystemErrorException(e));
        }
    }

    boost::asio::ip::tcp::endpoint parseTcpEndpoint
            (const std::string& text)
    {
        auto result = parseEndpoint(text);
        return boost::asio::ip::tcp::endpoint(result.first, result.second);
    }
}
