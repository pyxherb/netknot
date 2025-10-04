#include <netknot/io_service.h>
#include <peff/base/deallocable.h>
#include <peff/containers/set.h>
#include <peff/containers/string.h>
#include <peff/advutils/unique_ptr.h>
#include <peff/containers/hashmap.h>

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

struct HttpRequestLineView {
	std::string_view method;
	std::string_view path;
	std::string_view version;
};

struct HttpRequestHeaderView {
	peff::HashMap<std::string_view, std::string_view> headers;

	HttpRequestHeaderView(peff::Alloc *allocator) : headers(allocator) {}
};

class HttpReadAsyncCallback : public netknot::ReadAsyncCallback {
public:
	peff::RcObjectPtr<peff::Alloc> selfAllocator;
	HttpServer *httpServer;
	Connection *connection;
	HttpParseStatus parseStatus = HttpParseStatus::Initial;
	peff::String requestLine, requestHeader, body;
	HttpRequestLineView requestLineView;
	HttpRequestHeaderView requestHeaderView;
	size_t expectedBodySize = SIZE_MAX;

	HttpReadAsyncCallback(HttpServer *httpServer, Connection *connectiono, peff::Alloc *selfAllocator) : httpServer(httpServer), connection(connection), selfAllocator(selfAllocator), requestLine(selfAllocator), requestHeader(selfAllocator), body(selfAllocator) {
	}

	virtual ~HttpReadAsyncCallback() {
	}

	virtual void dealloc() noexcept override {
		peff::destroyAndRelease<HttpReadAsyncCallback>(selfAllocator.get(), this, alignof(HttpReadAsyncCallback));
	}

	peff::Option<HttpRequestLineView> parseHttpRequestLine() {
		std::string_view sv = requestLine;
		HttpRequestLineView view;

		size_t separator;

		if ((separator = sv.find(' ')) == std::string_view::npos) {
			return {};
		}
		view.method = sv.substr(0, separator);

		sv = sv.substr(separator + 1);

		if ((separator = sv.find(' ')) == std::string_view::npos) {
			return {};
		}
		view.path = sv.substr(0, separator);

		sv = sv.substr(separator + 1);
		view.method = sv.substr(0, separator);

		return std::move(view);
	}

	peff::Option<HttpRequestHeaderView> parseHttpRequestHeader() {
		std::string_view sv = requestHeader;
		HttpRequestHeaderView view(selfAllocator.get());

		size_t separator;

		std::string_view name;
		std::string_view content;

		while (sv.size()) {
			name = {};
			content = {};

			if ((separator = sv.find(':')) == std::string_view::npos) {
				return {};
			}
			name = sv.substr(0, separator);

			sv = sv.substr(separator + 1);

			if ((separator = sv.find_first_not_of(' ')) == std::string_view::npos) {
				return {};
			}
			sv = sv.substr(separator + 1);

			size_t endOfLine;
			if ((endOfLine = sv.find("\r\n")) == std::string_view::npos) {
				return {};
			}

			content = sv.substr(0, endOfLine);
			sv = sv.substr(endOfLine + 1);

			if (!view.headers.insert(std::move(name), std::move(content)))
				return {};
		}

		return std::move(view);
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
						size_t offSeparator = sv.find("\r\n");

						if (offSeparator != std::string_view::npos) {
							std::string_view requestLine = sv.substr(0, offSeparator + 1);
							if (!this->requestLine.append(requestLine)) {
								return netknot::OutOfMemoryError::alloc();
							}

							offNext = offSeparator + 2;

							if (offNext >= sv.size())
								return readNext();

							sv = sv.substr(offNext + 1);
						} else {
							if (!this->requestLine.append(sv)) {
								return netknot::OutOfMemoryError::alloc();
							}
							std::string_view combinedSv = this->requestLine;
							if ((offSeparator = combinedSv.find("\r\n")) == std::string_view::npos)
								return readNext();
						}

						if (auto result = parseHttpRequestLine(); result.hasValue()) {
							requestLineView = std::move(result.value());
							result.reset();
						} else {
							std::terminate();
						}

						parseStatus = HttpParseStatus::Header;
						[[fallthrough]];
					}
					case HttpParseStatus::Header: {
						size_t offSeparator = sv.find("\r\n\r\n");

						if (offSeparator != std::string_view::npos) {
							std::string_view requestHeaders = sv.substr(offNext, offSeparator + 1);
							if (!requestHeader.append(sv))
								return netknot::OutOfMemoryError::alloc();
							offNext = offSeparator + 4;

							if (offNext >= sv.size())
								return readNext();

							sv = sv.substr(offNext + 1);
						} else {
							if (!requestHeader.append(sv))
								return netknot::OutOfMemoryError::alloc();

							std::string_view combinedSv = requestHeader;

							size_t off = combinedSv.find("\r\n\r\n");
							if (off == std::string_view::npos)
								return readNext();
						}

						if (auto result = parseHttpRequestHeader(); !result.hasValue()) {
							requestHeaderView = std::move(result.value());
							result.reset();
						} else {
							std::terminate();
						}

						if (auto it = requestHeaderView.headers.find("Content-Length"); it != requestHeaderView.headers.end()) {
							size_t contentLength = 0, curDigit;

							for (auto i : it.value()) {
								if ((i < '0' || i > '9'))
									std::terminate();

								if (SIZE_MAX / 10 < contentLength)
									std::terminate();
								contentLength *= 10;

								curDigit = i - '0';
								if (SIZE_MAX - curDigit < contentLength)
									std::terminate();
								contentLength += curDigit;
							}

							expectedBodySize = contentLength;
						}

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
