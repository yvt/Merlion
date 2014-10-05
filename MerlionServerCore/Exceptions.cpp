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
#include "Exceptions.hpp"
#include <boost/thread/tss.hpp>

namespace mcore
{
	// 1. For unknown reason, boost::thread_specific_ptr<std::string>::
	// ~boost::thread_specific_ptr() crashes on exit.
	// So leave it leaked...
	// 2. Initialized static (non-local) global variable is
	// likely to crash the shared library
	namespace
	{
		boost::thread_specific_ptr<std::string>& lastErrorMessage()
		{
			static boost::thread_specific_ptr<std::string> *lastErrorMessage = nullptr;
			if (lastErrorMessage == nullptr) {
				lastErrorMessage = new boost::thread_specific_ptr<std::string>();
			}
			return *lastErrorMessage;
		}
	}

    void setLastErrorMessage(const std::string& s)
    {
        lastErrorMessage().reset(new std::string(s));
    }
    const char *getLastErrorMessage()
    {
        std::string *msg = lastErrorMessage().get();
        if (msg == nullptr) {
            return "No error occured yet.";
        } else {
            return msg->c_str();
        }
    }

}

extern "C" const char *MSCGetLastErrorMessage()
{
    return mcore::getLastErrorMessage();
}
