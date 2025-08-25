#include <netknot/io_service.h>
#include <peff/base/deallocable.h>
#include <peff/advutils/unique_ptr.h>

class HttpServer;

class Connection {
public:
	HttpServer *httpServer;
	peff::RcObjectPtr<peff::Alloc> selfAllocator;

	Connection(peff::Alloc *allocator, HttpServer *httpServer) : selfAllocator(allocator), httpServer(httpServer) {
	}

	~Connection() {
	}

	void dealloc() noexcept {
		peff::destroyAndRelease<Connection>(selfAllocator.get(), this, alignof(Connection));
	}
};

class HttpServer {
public:
};

class HttpAcceptAsyncCallback : public netknot::AcceptAsyncCallback {
public:
	HttpServer *httpServer;

	HttpAcceptAsyncCallback(HttpServer *httpServer): httpServer(httpServer) {
	}

	virtual ~HttpAcceptAsyncCallback() {
	}

	virtual void dealloc() noexcept override {
	}

	virtual netknot::ExceptionPointer onAccepted(netknot::Socket *socket) {
	}
};

int main() {
	netknot::ExceptionPointer e;
	{
		peff::UniquePtr<netknot::IOService, peff::DeallocableDeleter<netknot::IOService>> ioService;
		{
			netknot::IOServiceCreationParams params(peff::getDefaultAlloc(), peff::getDefaultAlloc());

			if ((e = netknot::createDefaultIOService(ioService.getRef(), params))) {
				std::terminate();
			}
		}

		peff::UniquePtr<netknot::CompiledAddress, peff::DeallocableDeleter<netknot::CompiledAddress>> compiledAddr;
		{
			netknot::IPv4Address addr(127, 0, 0, 0, 8080);

			if ((e = ioService->compileAddress(peff::getDefaultAlloc(), &addr, compiledAddr.getRef()))) {
				std::terminate();
			}
		}

		peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> socket;
		{
			if ((e = ioService->createSocket(peff::getDefaultAlloc(), netknot::ADDRFAM_IPV4, netknot::SOCKET_TCP, socket.getRef()))) {
				std::terminate();
			}
		}

		if ((e = socket->bind(compiledAddr.get()))) {
			std::terminate();
		}

		HttpServer httpServer;

		HttpAcceptAsyncCallback callback(&httpServer);
		peff::RcObjectPtr<netknot::AcceptAsyncTask> acceptAsyncTask;
		if ((e = socket->acceptAsync(peff::getDefaultAlloc(), &callback, acceptAsyncTask.getRef()))) {
			std::terminate();
		}

		ioService->run();
	}
}
