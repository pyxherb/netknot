#include "server.h"

using namespace http;

Connection::Connection(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket) : selfAllocator(allocator), httpServer(httpServer), socket(socket), requestCallback(httpServer, this, &peff::g_nullAlloc, allocator), responseCallback(httpServer, this, &peff::g_nullAlloc, allocator) {
}
Connection::~Connection() {
}
void Connection::dealloc() noexcept {
	peff::destroyAndRelease<Connection>(selfAllocator.get(), this, alignof(Connection));
}
Connection *Connection::alloc(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket) {
	return peff::allocAndConstruct<Connection>(allocator, alignof(Connection), allocator, httpServer, socket);
}

HttpAcceptAsyncCallback::HttpAcceptAsyncCallback(HttpServer *httpServer, peff::Alloc *selfAllocator) : httpServer(httpServer), selfAllocator(selfAllocator) {
}
HttpAcceptAsyncCallback::~HttpAcceptAsyncCallback() {
}
void HttpAcceptAsyncCallback::onRefZero() noexcept {
	peff::destroyAndRelease<HttpAcceptAsyncCallback>(selfAllocator.get(), this, alignof(HttpAcceptAsyncCallback));
}
netknot::ExceptionPointer HttpAcceptAsyncCallback::onAccepted(netknot::Socket *socket) noexcept {
	peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> s(socket);

	peff::UniquePtr<Connection, peff::DeallocableDeleter<Connection>> conn(Connection::alloc(selfAllocator.get(), httpServer, socket));

	if (!conn)
		return netknot::OutOfMemoryError::alloc();

	s.release();

	if (!httpServer->addConnection(conn.get()))
		return netknot::OutOfMemoryError::alloc();

	netknot::ReadAsyncTask *task;

	EmplaceBuffer bb(buffer, sizeof(buffer));
	netknot::RcBufferRef bufferRef(&*(emplaceBuffer = peff::Option<EmplaceBuffer>(std::move(bb))));
	NETKNOT_RETURN_IF_EXCEPT(conn->socket->readAsync(selfAllocator.get(), bufferRef, &conn->requestCallback, task));

	conn.release();

	peff::RcObjectPtr<netknot::AcceptAsyncTask> acceptAsyncTask;
	if (auto e = httpServer->serverSocket->acceptAsync(peff::getDefaultAlloc(), this, acceptAsyncTask.getRef()); e) {
		return e;
	}

	return {};
}

EmplaceBuffer::EmplaceBuffer(char *data, size_t size) : RcBuffer(data, size) {}
EmplaceBuffer::~EmplaceBuffer() {}
size_t EmplaceBuffer::incRef(size_t globalRc) {
	return 0;
}
size_t EmplaceBuffer::decRef(size_t globalRc) {
	return 0;
}

HttpRequestHeaderView::HttpRequestHeaderView(peff::Alloc *allocator) : headers(allocator) {}

HttpReadAsyncCallback::HttpReadAsyncCallback(HttpServer *httpServer, Connection *connection, peff::Alloc *selfAllocator, peff::Alloc *allocator) : httpServer(httpServer), connection(connection), selfAllocator(selfAllocator), allocator(allocator), requestLine(allocator), requestHeader(allocator), body(allocator), requestHeaderView(allocator) {
}

HttpReadAsyncCallback::~HttpReadAsyncCallback() {
}

void HttpReadAsyncCallback::onRefZero() noexcept {
	peff::destroyAndRelease<HttpReadAsyncCallback>(selfAllocator.get(), this, alignof(HttpReadAsyncCallback));
}

peff::Option<HttpRequestLineView> HttpReadAsyncCallback::parseHttpRequestLine(std::string_view requestLine) {
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
	view.version = sv;

	return std::move(view);
}

peff::Option<HttpRequestHeaderView> HttpReadAsyncCallback::parseHttpRequestHeader(std::string_view requestHeader, peff::Alloc *allocator) {
	std::string_view sv = requestHeader;
	HttpRequestHeaderView view(allocator);

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
		sv = sv.substr(endOfLine + 2);

		if (!view.headers.insert(std::move(name), std::move(content)))
			return {};
	}

	return std::move(view);
}

netknot::ExceptionPointer HttpReadAsyncCallback::onStatusChanged(netknot::ReadAsyncTask *task) noexcept {
	switch (task->getStatus()) {
		case netknot::AsyncTaskStatus::Done: {
			const char *const p = task->getBuffer();
			const size_t realSize = task->getCurrentReadSize();
			std::string_view sv(p, realSize);
			size_t offNext = 0;

			auto readNext = [this, task]() -> netknot::ExceptionPointer {
				peff::RcObjectPtr<netknot::ReadAsyncTask> taskReceiver;
				return connection->socket->readAsync(httpServer->allocator.get(), task->getBufferRef(), this, taskReceiver.getRef());
			};

			switch (parseStatus) {
				case HttpParseStatus::Initial: {
					size_t offSeparator = sv.find("\r\n");

					if (offSeparator != std::string_view::npos) {
						std::string_view requestLineSv = sv.substr(0, offSeparator);
						if (!requestLine.append(requestLineSv)) {
							return netknot::OutOfMemoryError::alloc();
						}

						offNext = offSeparator + 2;

						if (offNext >= sv.size())
							return readNext();

						sv = sv.substr(offNext);
					} else {
						if (!requestLine.append(sv)) {
							return netknot::OutOfMemoryError::alloc();
						}
						std::string_view combinedSv = requestLine;
						if ((offSeparator = combinedSv.find("\r\n")) == std::string_view::npos)
							return readNext();
					}

					if (auto result = parseHttpRequestLine(requestLine); result.hasValue()) {
						requestLineView = result.move();
					} else {
						std::terminate();
					}

					parseStatus = HttpParseStatus::Header;
					[[fallthrough]];
				}
				case HttpParseStatus::Header: {
					size_t offSeparator = sv.find("\r\n\r\n");

					if (offSeparator != std::string_view::npos) {
						if (!requestHeader.append(sv.substr(0, offSeparator + 2)))
							return netknot::OutOfMemoryError::alloc();
						offNext = offSeparator + 4;

						sv = sv.substr(offNext);
					} else {
						if (!requestHeader.append(sv))
							return netknot::OutOfMemoryError::alloc();

						std::string_view combinedSv = requestHeader;

						size_t off = combinedSv.find("\r\n\r\n");
						if (off == std::string_view::npos)
							return readNext();
					}

					if (auto result = parseHttpRequestHeader(requestHeader, allocator.get()); result.hasValue()) {
						requestHeaderView = result.move();
					} else {
						std::terminate();
					}

					if (auto it = requestHeaderView.headers.find("Transfer-Encoding"); it != requestHeaderView.headers.end()) {
						transferEncoding = it.value();

						if (transferEncoding == "chunked") {
							isChunked = true;
							goto noContentLength;
						}
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

						if (!body.resizeUninitialized(contentLength)) {
							return netknot::OutOfMemoryError::alloc();
						}

						expectedBodySize = contentLength;
					}
				noContentLength:

					parseStatus = HttpParseStatus::Body;
					if (!isChunked) {
						if (expectedBodySize) {
							EmplaceBuffer bb(body.data(), expectedBodySize);
							netknot::RcBufferRef bufferRef(&*(bodyBuffer = peff::Option<EmplaceBuffer>(std::move(bb))));

							netknot::ReadAsyncTask *task;
							return connection->socket->readAsync(httpServer->allocator.get(), bufferRef, this, task);
						}
					} else {
						std::terminate();
					}
					[[fallthrough]];
				}
				case HttpParseStatus::Body: {
					if (isChunked) {
						std::terminate();
					} else {
						std::string_view response =
							"HTTP/1.1 200 OK\r\n"
							"Content-Type: text/plain\r\n"
							"Content-Length: 13\r\n"
							"\r\n"
							"Hello, World!";
						EmplaceBuffer rb((char*)response.data(), response.size());
						netknot::RcBufferRef bufferRef(&*(responseBuffer = peff::Option<EmplaceBuffer>(std::move(rb))));

						netknot::WriteAsyncTask *task;
						NETKNOT_RETURN_IF_EXCEPT(connection->socket->writeAsync(httpServer->allocator.get(), bufferRef, &connection->responseCallback, task));
					}
					break;
				}
			}

			break;
		}
		case netknot::AsyncTaskStatus::Interrupted: {
			break;
		}
		default:
			break;
	}

	return {};
}

HttpWriteAsyncCallback::HttpWriteAsyncCallback(HttpServer *httpServer, Connection *connection, peff::Alloc *selfAllocator, peff::Alloc *allocator) : httpServer(httpServer), connection(connection), selfAllocator(selfAllocator), allocator(allocator) {
}
HttpWriteAsyncCallback::~HttpWriteAsyncCallback() {
}
void HttpWriteAsyncCallback::onRefZero() noexcept {
	peff::destroyAndRelease<HttpWriteAsyncCallback>(selfAllocator.get(), this, alignof(HttpWriteAsyncCallback));
}
netknot::ExceptionPointer HttpWriteAsyncCallback::onStatusChanged(netknot::WriteAsyncTask *task) noexcept {
	switch (task->getStatus()) {
		case netknot::AsyncTaskStatus::Done: {
			break;
		}
		case netknot::AsyncTaskStatus::Interrupted: {
			break;
		}
		default:
			break;
	}

	return {};
}
