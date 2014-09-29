#include "Prefix.pch"
#include "Exceptions.hpp"
#include <boost/thread/tss.hpp>

namespace mcore
{
	// For unknown reason, boost::thread_specific_ptr<std::string>::
	// ~boost::thread_specific_ptr() crashes on exit.
	// So leave it leaked...
    static boost::thread_specific_ptr<std::string> *lastErrorMessage =
	new boost::thread_specific_ptr<std::string>();

    void setLastErrorMessage(const std::string& s)
    {
        lastErrorMessage->reset(new std::string(s));
    }
    const char *getLastErrorMessage()
    {
        std::string *msg = lastErrorMessage->get();
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
