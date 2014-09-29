#pragma once

#include <cstdint>
#include <string>

namespace mcore
{
    static constexpr std::uint32_t ControlStreamHeaderMagic = 0x129a1234;
    static constexpr std::uint32_t VersionDownloadRequestMagic = 0x229b1234;
    static constexpr std::uint32_t DataStreamMagic = 0x3183a443;

    enum class MasterCommand : std::uint8_t
    {
        Nop = 0,
        RejectClient,
        VersionLoaded,
        VersionUnloaded,
        BindRoom,
        UnbindRoom,
        Log
    };

    enum class NodeCommand : std::uint8_t
    {
        Nop = 0,
        LoadVersion,
        UnloadVersion,
        Connect
    };
    
    struct NodeInfo
    {
        std::string nodeName;

        template <class Writer>
        void serialize(Writer& writer)
        {
            writer.writeString(nodeName);
        }

        template <class Reader>
        void deserialize(Reader& reader)
        {
            nodeName = reader.readString();
        }
    };
    
    static constexpr std::uint32_t ClientHeaderMagic = 0x918123ab;
    
    enum class ClientResponse : std::uint8_t
    {
        Success = 0,
		ServerFull,
		ProtocolError,
		InternalServerError
    };
}
