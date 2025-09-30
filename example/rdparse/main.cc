#include <netknot/io_service.h>
#include <peff/base/deallocable.h>
#include <peff/containers/set.h>
#include <peff/advutils/unique_ptr.h>

class HttpServer;

class Connection {
public:
	HttpServer *httpServer;
	std::unique_ptr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> socket;
	peff::RcObjectPtr<peff::Alloc> selfAllocator;

	Connection(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket) : selfAllocator(allocator), httpServer(httpServer), socket(socket) {
	}

	~Connection() {
	}

	void dealloc() noexcept {
		peff::destroyAndRelease<Connection>(selfAllocator.get(), this, alignof(Connection));
	}

	static Connection *alloc(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket) {
		return peff::allocAndConstruct<Connection>(allocator, alignof(Connection), allocator, httpServer, socket);
	}
};

class HttpServer {
public:
	peff::Set<peff::UniquePtr<Connection, peff::DeallocableDeleter<Connection>>> connections;

	HttpServer(peff::Alloc *allocator) : connections(allocator) {}

	[[nodiscard]] bool addConnection(Connection *conn) noexcept {
		if (!connections.insert({ conn }))
			return false;
		return true;
	}
};

class HttpAcceptAsyncCallback : public netknot::AcceptAsyncCallback {
public:
	peff::RcObjectPtr<peff::Alloc> selfAllocator;
	HttpServer *httpServer;

	HttpAcceptAsyncCallback(HttpServer *httpServer, peff::Alloc *selfAllocator) : httpServer(httpServer), selfAllocator(selfAllocator) {
	}

	virtual ~HttpAcceptAsyncCallback() {
	}

	virtual void dealloc() noexcept override {
		peff::destroyAndRelease<HttpAcceptAsyncCallback>(selfAllocator.get(), this, alignof(HttpAcceptAsyncCallback));
	}

	virtual netknot::ExceptionPointer onAccepted(netknot::Socket *socket) noexcept override {
		peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> s(socket);

		peff::UniquePtr<Connection, peff::DeallocableDeleter<Connection>> conn(Connection::alloc(selfAllocator.get(), httpServer, socket));

		if (!conn)
			return netknot::OutOfMemoryError::alloc();

		s.release();

		if (!httpServer->addConnection(conn.release()))
			return netknot::OutOfMemoryError::alloc();

		peff::RcObjectPtr<netknot::AcceptAsyncTask> acceptAsyncTask;
		if (auto e = socket->acceptAsync(peff::getDefaultAlloc(), this, acceptAsyncTask.getRef()); e) {
			return e;
		}

		return {};
	}
};

class HttpReadAsyncCallback : public netknot::ReadAsyncCallback {
public:
	peff::RcObjectPtr<peff::Alloc> selfAllocator;
	HttpServer *httpServer;

	HttpReadAsyncCallback(HttpServer *httpServer, peff::Alloc *selfAllocator) : httpServer(httpServer), selfAllocator(selfAllocator) {
	}

	virtual ~HttpReadAsyncCallback() {
	}

	virtual void dealloc() noexcept override {
		peff::destroyAndRelease<HttpReadAsyncCallback>(selfAllocator.get(), this, alignof(HttpReadAsyncCallback));
	}

	virtual netknot::ExceptionPointer onStatusChanged(netknot::ReadAsyncTask *task) noexcept override {
	}
};

class HttpWriteAsyncCallback : public netknot::WriteAsyncCallback {
public:
	peff::RcObjectPtr<peff::Alloc> selfAllocator;
	HttpServer *httpServer;

	HttpWriteAsyncCallback(HttpServer *httpServer, peff::Alloc *selfAllocator) : httpServer(httpServer), selfAllocator(selfAllocator) {
	}

	virtual ~HttpWriteAsyncCallback() {
	}

	virtual void dealloc() noexcept override {
		peff::destroyAndRelease<HttpWriteAsyncCallback>(selfAllocator.get(), this, alignof(HttpWriteAsyncCallback));
	}

	virtual netknot::ExceptionPointer onStatusChanged(netknot::WriteAsyncTask *task) noexcept override {
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

			if ((e = ioService->compileAddress(peff::getDefaultAlloc(), &addr, compiledAddr.getAddressOf()))) {
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

		HttpServer httpServer(peff::getDefaultAlloc());

		HttpAcceptAsyncCallback callback(&httpServer, peff::getDefaultAlloc());
		peff::RcObjectPtr<netknot::AcceptAsyncTask> acceptAsyncTask;
		if ((e = socket->acceptAsync(peff::getDefaultAlloc(), &callback, acceptAsyncTask.getRef()))) {
			std::terminate();
		}

		if ((e = ioService->run()))
			std::terminate();
	}
}
