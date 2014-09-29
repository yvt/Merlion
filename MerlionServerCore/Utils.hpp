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

}

