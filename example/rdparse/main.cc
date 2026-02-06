#include "server.h"
#include <map>

class MyAllocator : public peff::StdAlloc {
public:
	struct AllocRecord {
		size_t size;
		size_t alignment;
	};

	std::map<void *, AllocRecord> allocRecords;

	~MyAllocator() {
		assert(allocRecords.empty());
	}

	virtual void *alloc(size_t size, size_t alignment) noexcept override {
		void *p = this->StdAlloc::alloc(size, alignment);
		if (!p)
			std::terminate();

		allocRecords[p] = { size, alignment };

		return p;
	}

	virtual void *realloc(void *p, size_t size, size_t alignment, size_t newSize, size_t newAlignment) noexcept override {
		void *ptr = this->StdAlloc::realloc(p, size, alignment, newSize, newAlignment);
		if (!ptr)
			return nullptr;

		AllocRecord &allocRecord = allocRecords.at(p);

		assert(allocRecord.size == size);
		assert(allocRecord.alignment == alignment);

		allocRecords.erase(p);

		allocRecords[ptr] = { newSize, newAlignment };

		return ptr;
	}

	virtual void release(void *p, size_t size, size_t alignment) noexcept override {
		AllocRecord &allocRecord = allocRecords.at(p);

		assert(allocRecord.size == size);
		assert(allocRecord.alignment == alignment);

		allocRecords.erase(p);

		this->StdAlloc::release(p, size, alignment);
	}
};

int main() {
#ifdef _MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	peff::StdAlloc myAlloc;
	{
		netknot::ExceptionPointer e;

		peff::UniquePtr<netknot::IOService, peff::DeallocableDeleter<netknot::IOService>> ioService;
		{
			netknot::IOServiceCreationParams params(&myAlloc, &myAlloc);
			params.nWorkerThreads = 1;

			if ((e = netknot::createDefaultIOService(ioService.getRef(), params))) {
				std::terminate();
			}
		}

		peff::UniquePtr<netknot::TranslatedAddress, peff::DeallocableDeleter<netknot::TranslatedAddress>> compiledAddr;
		{
			netknot::IPv4Address addr(0, 0, 0, 0, 8080);

			if ((e = ioService->translateAddress(&myAlloc, &addr, compiledAddr.getAddressOf()))) {
				std::terminate();
			}
		}

		peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> socket;
		{
			if ((e = ioService->createSocket(&myAlloc, netknot::ADDRFAM_IPV4, netknot::SOCKET_TCP, socket.getRef()))) {
				std::terminate();
			}
		}

		if ((e = socket->bind(compiledAddr.get()))) {
			std::terminate();
		}

		if ((e = socket->listen(114514))) {
			std::terminate();
		}

		{
			http::HttpServer httpServer(&myAlloc, ioService.get(), socket.release());

			peff::RcObjectPtr<http::HttpAcceptAsyncCallback> callback;

			if (!(callback = peff::allocAndConstruct<http::HttpAcceptAsyncCallback>(&myAlloc, alignof(http::HttpAcceptAsyncCallback), &httpServer, &myAlloc)))
				std::terminate();

			peff::RcObjectPtr<netknot::AcceptAsyncTask> acceptAsyncTask;
			if ((e = httpServer.serverSocket->acceptAsync(&myAlloc, callback.get(), acceptAsyncTask.getRef()))) {
				std::terminate();
			}

			peff::UniquePtr<http::HttpRequestHandler, peff::DeallocableDeleter<http::HttpRequestHandler>> stopGetHandler = http::allocFnHttpRequestHandler(
				&myAlloc,
				"GET",
				[](const http::HttpURLHandlerState &state) -> netknot::ExceptionPointer {
					return state.httpServer->ioService->stop();
				});
			if (!stopGetHandler)
				std::terminate();

			if ((e = httpServer.registerHandler("/stop", stopGetHandler.release()))) {
				std::terminate();
			}

			if ((e = ioService->run()))
				std::terminate();
		}
	}

	return 0;
}
