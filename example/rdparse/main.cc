#include <netknot/io_service.h>
#include <peff/base/deallocable.h>
#include <peff/containers/set.h>
#include <peff/containers/string.h>
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
	peff::RcObjectPtr<peff::Alloc> allocator;
	peff::Set<peff::UniquePtr<Connection, peff::DeallocableDeleter<Connection>>> connections;

	HttpServer(peff::Alloc *allocator) : allocator(allocator), connections(allocator) {}

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

enum class HttpParseStatus : uint8_t {
	Initial = 0,
	Header,
	Body
};

class HttpReadAsyncCallback : public netknot::ReadAsyncCallback {
public:
	peff::RcObjectPtr<peff::Alloc> selfAllocator;
	HttpServer *httpServer;
	Connection *connection;
	HttpParseStatus parseStatus = HttpParseStatus::Initial;
	peff::String requestLine, lastReadHeaderLinePart, body;

	HttpReadAsyncCallback(HttpServer *httpServer, Connection *connectiono, peff::Alloc *selfAllocator) : httpServer(httpServer), connection(connection), selfAllocator(selfAllocator), requestLine(selfAllocator), lastReadHeaderLinePart(selfAllocator), body(selfAllocator) {
	}

	virtual ~HttpReadAsyncCallback() {
	}

	virtual void dealloc() noexcept override {
		peff::destroyAndRelease<HttpReadAsyncCallback>(selfAllocator.get(), this, alignof(HttpReadAsyncCallback));
	}

	virtual netknot::ExceptionPointer onStatusChanged(netknot::ReadAsyncTask *task) noexcept override {
		switch (task->getStatus()) {
			case netknot::AsyncTaskStatus::Done: {
				const char *const p = task->getBuffer();
				const size_t realSize = task->getCurrentReadSize();
				std::string_view sv(p, realSize);
				size_t offNext = 0;

				auto readNext = [this]() -> netknot::ExceptionPointer {
					netknot::ReadAsyncTask *task;
					return connection->socket->readAsync(httpServer->allocator.get(), task->getBufferRef(), this, task);
				};

				switch (parseStatus) {
					case HttpParseStatus::Initial: {
						size_t offSeparator = sv.find("\n\n");

						if (offSeparator != std::string_view::npos) {
							std::string_view requestLine = sv.substr(0, offSeparator + 1);
							if (!this->requestLine.append(requestLine)) {
								return netknot::OutOfMemoryError::alloc();
							}
						} else {
							if (!this->requestLine.append(sv)) {
								return netknot::OutOfMemoryError::alloc();
							}

							return readNext();
						}

						offNext = offSeparator + 2;

						if (offNext >= realSize)
							break;
						parseStatus = HttpParseStatus::Header;
						[[fallthrough]];
					}
					case HttpParseStatus::Header: {
						size_t offSeparator = sv.find("\n\n");

						if (offSeparator != std::string_view::npos) {
							std::string_view requestHeaders = sv.substr(offNext, offSeparator + 1);
							if(!lastReadHeaderLinePart.append(sv))
								return netknot::OutOfMemoryError::alloc();
							offNext = offSeparator + 2;
						} else {
							if (!lastReadHeaderLinePart.append(sv))
								return netknot::OutOfMemoryError::alloc();

							return readNext();
						}

						if (offNext >= realSize)
							break;
						parseStatus = HttpParseStatus::Body;
						[[fallthrough]];
					}
					case HttpParseStatus::Body: {
						break;
					}
				}

				break;
			}
			case netknot::AsyncTaskStatus::Interrupted: {
				break;
			}
			default:
				return {};
		}
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
