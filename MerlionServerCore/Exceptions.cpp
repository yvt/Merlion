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
