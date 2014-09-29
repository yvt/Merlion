#include "Prefix.pch"
#include <iostream>
#include "Library.hpp"
#include <atomic>
#include "Public.h"
#include <algorithm>
#include "Exceptions.hpp"

namespace mcore
{
    Library::Library():
    _ioService(new boost::asio::io_service()),
    work(new boost::asio::io_service::work(*_ioService))
    {
        std::size_t numWorkers = std::max<std::size_t>(
                std::thread::hardware_concurrency(), 2) * 2;

        for (std::size_t i = 0; i < numWorkers; ++i) {
            workers.emplace_back();
            Worker& worker = workers.back();
            worker.thread.reset(new std::thread([this] {
                workerRunner();
            }));
        }
    }

    void Library::workerRunner()
    {
        _ioService->run();
    }

    void Library::addListener(LibraryListener *listener)
    {
        if (listener == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(manageMutex);

        listeners.insert(listener);
    }

    void Library::removeListener(LibraryListener *listener)
    {
        std::lock_guard<std::mutex> lock(manageMutex);
        listeners.erase(listener);
    }

    Library::~Library()
    {
        _ioService->stop();

        {
            std::lock_guard<std::mutex> lock(manageMutex);
            for (auto *listener: listeners)
                listener->onBeingDestroyed(*this);
        }

        work.reset(nullptr);
        for (Worker& worker: workers)
            worker.thread->join();

        {
            std::lock_guard<std::mutex> lock(manageMutex);
            for (auto *listener: listeners)
                listener->onWasDestroyed(*this);
        }
    }

}

extern "C" MSCResult MSCLibraryCreate(MSCLibrary *libOut)
{
    return mcore::convertExceptionsToResultCode([&] {
        *libOut = nullptr;
		auto library = std::make_shared<mcore::Library>();
        *libOut = library->handle();
    });
}

extern "C" MSCResult MSCLibraryDestroy(MSCLibrary library)
{
    return mcore::convertExceptionsToResultCode([&] {
        delete mcore::Library::fromHandle(library);
    });
}
