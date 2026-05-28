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
	peff::StdAlloc my_allocator;
	{
		netknot::ExceptionPointer e;

		peff::UniquePtr<netknot::IOService, peff::DeallocableDeleter<netknot::IOService>> io_service;
		{
			netknot::IOServiceCreationParams params(&my_allocator, &my_allocator);
			params.nWorkerThreads = 1;

			if ((e = netknot::create_default_io_service(io_service.get_ref(), params))) {
				std::terminate();
			}
		}

		peff::UniquePtr<netknot::TranslatedAddress, peff::DeallocableDeleter<netknot::TranslatedAddress>> compiledAddr;
		{
			netknot::IPv4Address addr(0, 0, 0, 0, 8080);

			if ((e = io_service->translate_addr(&my_allocator, &addr, compiledAddr.get_addr()))) {
				std::terminate();
			}
		}

		peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> socket;
		{
			if ((e = io_service->create_socket(&my_allocator, netknot::ADDRFAM_IPV4, netknot::SOCKET_TCP, socket.get_ref()))) {
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
			http::HttpServer http_server(&my_allocator, io_service.get(), socket.release());

			peff::RcObjectPtr<http::HttpAcceptAsyncCallback> callback;

			if (!(callback = peff::alloc_and_construct<http::HttpAcceptAsyncCallback>(&my_allocator, alignof(http::HttpAcceptAsyncCallback), &http_server, &my_allocator)))
				std::terminate();

			peff::RcObjectPtr<netknot::AcceptAsyncTask> accept_async_task;
			if ((e = http_server.server_socket->accept_async(&my_allocator, callback.get(), accept_async_task.get_ref()))) {
				std::terminate();
			}

			peff::UniquePtr<http::HttpRequestHandler, peff::DeallocableDeleter<http::HttpRequestHandler>> stop_get_handler = http::allocFnHttpRequestHandler(
				&my_allocator,
				"GET",
				[](const http::HttpURLHandlerState &state) -> netknot::ExceptionPointer {
					return state.http_server->io_service->stop();
				});
			if (!stop_get_handler)
				std::terminate();

			if ((e = http_server.register_handler("/stop", stop_get_handler.release()))) {
				std::terminate();
			}

			if ((e = io_service->run()))
				std::terminate();
		}

		puts("");
	}

	return 0;
}
