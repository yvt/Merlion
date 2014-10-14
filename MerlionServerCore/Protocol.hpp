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
		std::string serverSoftwareName;

        template <class Writer>
        void serialize(Writer& writer)
        {
            writer.writeString(nodeName);
			writer.writeString(serverSoftwareName);
        }

        template <class Reader>
        void deserialize(Reader& reader)
        {
            nodeName = reader.readString();
			serverSoftwareName = reader.readString();
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
