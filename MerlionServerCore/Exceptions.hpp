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

#include <stdexcept>
#include <string>
#include "Public.h"
#include <cassert>

namespace mcore
{
	class Exception : virtual public boost::exception, virtual public std::exception
    {
        std::string message;
	public:
		/*Exception():
		Exception("Unknown error.")
		{ }*/
        Exception(const std::string& message):
                message(message)
        { }
        virtual MSCResult code() const = 0;

        virtual const char *what() const throw() override { return message.c_str(); }
    };

    class SystemErrorException : virtual public Exception
    {
    public:
        SystemErrorException():
                Exception("The operating system returned an error.")
        { }
        SystemErrorException(const std::string& message):
                Exception(message)
        { }
        SystemErrorException(const boost::system::error_code& boostErrorCode):
                Exception(boostErrorCode.message())
        { }
        SystemErrorException(const boost::system::system_error& boostError):
                Exception(boostError.what())
        { }
        virtual MSCResult code() const { return MSCR_SystemError; }
    };

    class InvalidOperationException : virtual public Exception
    {
    public:
        InvalidOperationException():
                Exception("Performed operation is invalid for the current state of the object.")
        { }
        InvalidOperationException(const std::string& message):
                Exception(message)
        { }
        virtual MSCResult code() const { return MSCR_InvalidOperation; }
    };

    class InvalidArgumentException : virtual public Exception
    {
    public:
        InvalidArgumentException():
                Exception("Invalid argument.")
        { }
        InvalidArgumentException(const std::string& parameterName):
                Exception("Invalid argument.: " + parameterName)
        { }
        InvalidArgumentException(
                const std::string& message,
                const std::string& parameterName):
                Exception("Invalid argument.: " + parameterName + " " + message)
        { }
        virtual MSCResult code() const { return MSCR_InvalidArgument; }
    };

    class InvalidFormatException : virtual public Exception
    {
    public:
        InvalidFormatException():
                Exception("Invalid format.")
        { }
        InvalidFormatException(const std::string& message):
                Exception(message)
        { }
        virtual MSCResult code() const { return MSCR_InvalidFormat; }
    };

    class InvalidDataException : virtual public Exception
    {
    public:
        InvalidDataException():
                Exception("Invalid data.")
        { }
        InvalidDataException(const std::string& message):
                Exception(message)
        { }
        virtual MSCResult code() const { return MSCR_InvalidData; }
    };

    class NotImplementedException : virtual public Exception
    {
    public:
        NotImplementedException():
                Exception("Requested feature is not implemented.")
        { }
        NotImplementedException(const std::string& message):
                Exception(message)
        { }
        virtual MSCResult code() const { return MSCR_NotImplemented; }
    };

    void setLastErrorMessage(const std::string&);
    const char *getLastErrorMessage();

    template <class F>
    MSCResult convertExceptionsToResultCode(F functor)
    {
        try {
            functor();
            return MSCR_Success;
        } catch (mcore::Exception& ex) {
            setLastErrorMessage(ex.what());
            return ex.code();
        } catch (std::exception& ex) {
            setLastErrorMessage(ex.what());
            return MSCR_Unknown;
        } catch (...) {
            setLastErrorMessage("Unknown exception thrown.");
            return MSCR_Unknown;
        }
    }

#define MSCThrow(ex) throw (boost::enable_error_info(ex)<<\
::boost::throw_function(BOOST_THROW_EXCEPTION_CURRENT_FUNCTION) <<\
::boost::throw_file(__FILE__) <<\
::boost::throw_line((int)__LINE__) )
}
