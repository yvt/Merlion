#pragma once
#include <boost/asio.hpp>
#include <stdexcept>
#include "Public.h"
#include <list>
#include <memory>
#include <thread>
#include <mutex>
#include <unordered_set>

namespace mcore
{
    class LibraryListener;

	class Library:
	public std::enable_shared_from_this<Library>,
	boost::noncopyable
    {
        std::unordered_set<LibraryListener *> listeners;

        std::unique_ptr<boost::asio::io_service> _ioService;

        struct Worker
        {
            std::unique_ptr<std::thread> thread;
        };

        std::list<Worker> workers;
        std::unique_ptr<boost::asio::io_service::work> work;

        void workerRunner();

        std::mutex manageMutex;

    public:
        Library();
        ~Library();

        void addListener(LibraryListener *);
        void removeListener(LibraryListener *);

		MSCLibrary handle() { return reinterpret_cast<MSCLibrary>(new std::shared_ptr<Library>(shared_from_this())); }
        static std::shared_ptr<Library> *fromHandle(MSCLibrary handle) { return reinterpret_cast<std::shared_ptr<Library> *>(handle); }

        boost::asio::io_service& ioService() const { return *_ioService; }

    };

    class LibraryListener
    {
    public:
        virtual ~LibraryListener() { }
        virtual void onBeingDestroyed(Library&) { }
        virtual void onWasDestroyed(Library&) { }
    };


}



