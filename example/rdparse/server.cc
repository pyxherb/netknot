#include "server.h"

using namespace http;

using std::operator""sv;

Connection::Connection(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket) noexcept : selfAllocator(allocator), httpServer(httpServer), socket(socket) {
}
Connection::~Connection() {
}
void Connection::dealloc() noexcept {
	peff::destroyAndRelease<Connection>(selfAllocator.get(), this, alignof(Connection));
}
Connection *Connection::alloc(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket) {
	return peff::allocAndConstruct<Connection>(allocator, alignof(Connection), allocator, httpServer, socket);
}

HandlerURL::HandlerURL(peff::Alloc *selfAllocator) noexcept : selfAllocator(selfAllocator) {
}
HandlerURL::~HandlerURL() {
}
void HandlerURL::dealloc() noexcept {
	peff::destroyAndRelease<HandlerURL>(selfAllocator.get(), this, alignof(HandlerURL));
}

netknot::ExceptionPointer HttpURLHandlerState::writeStatusLine(HttpResponseStatus status) {
	if (this->stage != HttpURLHandlerStateStage::StatusLine)
		std::terminate();

	std::string_view httpVersion = "HTTP/1,1 "sv;

	if (!responseData.build(httpVersion))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append(HttpServer::getHttpResponseMessage(status)))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append("\r\n"))
		return netknot::OutOfMemoryError::alloc();

	this->stage = HttpURLHandlerStateStage::ResponseHeaders;

	return {};
}

netknot::ExceptionPointer HttpURLHandlerState::writeHeader(const std::string_view &name, const std::string_view &value) {
	if (this->stage != HttpURLHandlerStateStage::ResponseHeaders)
		std::terminate();

	if (!responseData.append(name))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append(": "sv))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append(value))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append("\r\n"))
		return netknot::OutOfMemoryError::alloc();

	return {};
}

netknot::ExceptionPointer HttpURLHandlerState::endHeader() {
	if (this->stage != HttpURLHandlerStateStage::ResponseHeaders)
		std::terminate();

	if (!responseData.append("\r\n"))
		return netknot::OutOfMemoryError::alloc();

	this->stage = HttpURLHandlerStateStage::ResponseBody;

	return {};
}

netknot::ExceptionPointer HttpURLHandlerState::writeBody(const std::string_view &data) {
	if (this->stage != HttpURLHandlerStateStage::ResponseBody)
		std::terminate();

	if (!responseData.append(data))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append("\r\n"))
		return netknot::OutOfMemoryError::alloc();

	return {};
}

netknot::ExceptionPointer HttpURLHandlerState::writeResponse(HttpResponseStatus status, const std::string_view &contentType, const std::string_view &body) {
	NETKNOT_RETURN_IF_EXCEPT(writeStatusLine(status));
	NETKNOT_RETURN_IF_EXCEPT(writeHeader("Content-Type", contentType));

	char lenStr[sizeof(size_t) / 3 + 1];

	sprintf(lenStr, "%zu", body.size());

	NETKNOT_RETURN_IF_EXCEPT(writeHeader("Content-Length", lenStr));
	NETKNOT_RETURN_IF_EXCEPT(endHeader());
	NETKNOT_RETURN_IF_EXCEPT(writeBody(body));

	this->stage = HttpURLHandlerStateStage::End;

	return {};
}

HttpRequestHandler::HttpRequestHandler(const std::string_view &methodName) noexcept : _methodName(methodName) {}
HttpRequestHandler::~HttpRequestHandler() {}

HttpRequestHandlerRegistry::HttpRequestHandlerRegistry(peff::Alloc *allocator) : allocator(allocator), baseUrl(allocator), handlers(allocator) {
}

HttpRequestHandlerRegistry::~HttpRequestHandlerRegistry() {
}

HttpAcceptAsyncCallback::HttpAcceptAsyncCallback(HttpServer *httpServer, peff::Alloc *selfAllocator) noexcept : httpServer(httpServer), selfAllocator(selfAllocator) {
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

	if (!(conn->requestCallback = peff::allocAndConstruct<HttpReadAsyncCallback>(
			  httpServer->allocator.get(), alignof(HttpReadAsyncCallback),
			  httpServer,
			  conn.get(),
			  &peff::g_nullAlloc,
			  httpServer->allocator.get())))
		return netknot::OutOfMemoryError::alloc();
	NETKNOT_RETURN_IF_EXCEPT(conn->socket->readAsync(
		selfAllocator.get(),
		bufferRef,
		conn->requestCallback.get(),
		task));

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
						std::string_view rawPath = requestLineView.path;
						std::string_view pathView, queryView, fragmentView;

						HttpURLHandlerState urlHandlerState = {
							httpServer,
							connection,
							pathView,
							queryView,
							fragmentView,
							requestHeaderView,
							peff::String(httpServer->allocator.get())
						};

						size_t offQuery = rawPath.find_first_of('?', 0);
						size_t offFragment = rawPath.find_first_of('#', 0);

						if (offFragment != std::string_view::npos) {
							if (offQuery != std::string_view::npos) {
								if (offQuery > offFragment) {
									urlHandlerState.writeResponse(HttpResponseStatus::BadRequest, "text/plain", "");
									goto writeResponse;
								}
								queryView = rawPath.substr(offQuery, offFragment - offQuery);
								fragmentView = rawPath.substr(offFragment);
								pathView = rawPath.substr(0, offQuery);
							} else {
								pathView = rawPath.substr(0, offFragment);
								fragmentView = rawPath.substr(offFragment);
							}
						} else {
							if (offQuery != std::string_view::npos) {
								pathView = rawPath.substr(0, offQuery);
								queryView = rawPath.substr(offQuery);
							} else {
								pathView = rawPath;
							}
						}

						if (auto it = httpServer->handlerRegistries.find(pathView); it != httpServer->handlerRegistries.end()) {
							const auto &registry = it.value();
							if (auto jt = registry.handlers.find(requestLineView.method); jt != registry.handlers.end())
								NETKNOT_RETURN_IF_EXCEPT(jt.value()->handleURL(urlHandlerState));
							else
								urlHandlerState.writeResponse(HttpResponseStatus::MethodNotAllowed, "text/plain", "");
						} else
							urlHandlerState.writeResponse(HttpResponseStatus::NotFound, "text/plain", "");

					writeResponse:
						if (!(connection->responseCallback = peff::allocAndConstruct<HttpWriteAsyncCallback>(
							httpServer->allocator.get(), alignof(HttpWriteAsyncCallback),
								  httpServer, connection, httpServer->allocator.get(), httpServer->allocator.get())))
							return netknot::OutOfMemoryError::alloc();
						HttpWriteAsyncCallback *callback = connection->responseCallback.get();

						callback->bufferData = std::move(urlHandlerState.responseData);
						callback->buffer = EmplaceBuffer(callback->bufferData.data(), callback->bufferData.size());
						netknot::RcBufferRef bufferRef(&*callback->buffer);

						netknot::WriteAsyncTask *task;
						NETKNOT_RETURN_IF_EXCEPT(connection->socket->writeAsync(httpServer->allocator.get(), bufferRef, callback, task));
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

HttpWriteAsyncCallback::HttpWriteAsyncCallback(HttpServer *httpServer, Connection *connection, peff::Alloc *selfAllocator, peff::Alloc *allocator) : httpServer(httpServer), connection(connection), selfAllocator(selfAllocator), allocator(allocator), bufferData(allocator) {
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

netknot::ExceptionPointer HttpServer::_reserveHandlerRegistry(const std::string_view &name) {
	HttpRequestHandlerRegistry registry(allocator.get());

	if (!registry.baseUrl.build(name))
		return netknot::OutOfMemoryError::alloc();

	if (!handlerRegistries.insert(registry.baseUrl, std::move(registry)))
		return netknot::OutOfMemoryError::alloc();

	return {};
}

void HttpServer::_removeHandlerRegistry(const std::string_view &name) {
	assert(handlerRegistries.contains(name));

	handlerRegistries.removeWithoutResizeBuckets(name);
}

HttpServer::HttpServer(peff::Alloc *allocator, netknot::IOService *ioService, netknot::Socket *serverSocket) : allocator(allocator), ioService(ioService), connections(allocator), serverSocket(serverSocket), handlerRegistries(allocator) {}

std::string_view HttpServer::getHttpResponseMessage(HttpResponseStatus status) {
	switch (status) {
		case HttpResponseStatus::Continue:
			return "100 Continue"sv;
		case HttpResponseStatus::SwitchingProtocols:
			return "101 Switching Protocols"sv;
		case HttpResponseStatus::Processing:
			return "102 Processing"sv;
		case HttpResponseStatus::EarlyHints:
			return "103 Early Hints"sv;
		case HttpResponseStatus::OK:
			return "200 OK"sv;
		case HttpResponseStatus::Created:
			return "201 Created"sv;
		case HttpResponseStatus::Accepted:
			return "202 Accepted"sv;
		case HttpResponseStatus::NonAuthoritativeInformation:
			return "203 Non-Authoritative Information"sv;
		case HttpResponseStatus::NoContent:
			return "204 No Content"sv;
		case HttpResponseStatus::ResetContent:
			return "205 Reset Content"sv;
		case HttpResponseStatus::PartialContent:
			return "206 Partial Content"sv;
		case HttpResponseStatus::MultiStatus:
			return "207 Multi-Status"sv;
		case HttpResponseStatus::AlreadyReported:
			return "208 Already Reported"sv;
		case HttpResponseStatus::IMUsed:
			return "226 IM Used"sv;
		case HttpResponseStatus::MultipleChoice:
			return "300 Multiple Choice"sv;
		case HttpResponseStatus::MovedPermanently:
			return "301 Moved Permanently"sv;
		case HttpResponseStatus::Found:
			return "302 Found"sv;
		case HttpResponseStatus::SeeOther:
			return "303 See Other"sv;
		case HttpResponseStatus::NotModified:
			return "304 Not Modified"sv;
		case HttpResponseStatus::UseProxy:
			return "305 Use Proxy"sv;
		case HttpResponseStatus::TemporaryRedirect:
			return "307 Temporary Redirect"sv;
		case HttpResponseStatus::PermanentRedirect:
			return "308 Permanent Redirect"sv;
		case HttpResponseStatus::BadRequest:
			return "400 Bad Request"sv;
		case HttpResponseStatus::Unauthorized:
			return "401 Unauthorized"sv;
		case HttpResponseStatus::PaymentRequired:
			return "402 Payment Required"sv;
		case HttpResponseStatus::Forbidden:
			return "403 Forbidden"sv;
		case HttpResponseStatus::NotFound:
			return "404 Not Found"sv;
		case HttpResponseStatus::MethodNotAllowed:
			return "405 Method Not Allowed"sv;
		case HttpResponseStatus::NotAcceptable:
			return "406 Not Acceptable"sv;
		case HttpResponseStatus::ProxyAuthenticationRequired:
			return "407 Proxy Authentication Required"sv;
		case HttpResponseStatus::RequestTimeout:
			return "408 Request Timeout"sv;
		case HttpResponseStatus::Conflict:
			return "409 Conflict"sv;
		case HttpResponseStatus::Gone:
			return "410 Gone"sv;
		case HttpResponseStatus::LengthRequired:
			return "411 Length Required"sv;
		case HttpResponseStatus::PreconditionFailed:
			return "412 Precondition Failed"sv;
		case HttpResponseStatus::PayloadTooLarge:
			return "413 Payload Too Large"sv;
		case HttpResponseStatus::URITooLong:
			return "414 URI Too Long"sv;
		case HttpResponseStatus::UnsupportedMediaType:
			return "415 Unsupported Media Type"sv;
		case HttpResponseStatus::RangeNotSatisfiable:
			return "416 Range Not Satisfiable"sv;
		case HttpResponseStatus::ExpectationFailed:
			return "417 Expectation Failed"sv;
		case HttpResponseStatus::ImATeapot:
			return "418 I'm a teapot"sv;
		case HttpResponseStatus::MisdirectedRequest:
			return "421 Misdirected Request"sv;
		case HttpResponseStatus::UnprocessableEntity:
			return "422 Unprocessable Entity"sv;
		case HttpResponseStatus::Locked:
			return "423 Locked"sv;
		case HttpResponseStatus::FailedDependency:
			return "424 Failed Dependency"sv;
		case HttpResponseStatus::TooEarly:
			return "425 Too Early"sv;
		case HttpResponseStatus::UpgradeRequired:
			return "426 Upgrade Required"sv;
		case HttpResponseStatus::PreconditionRequired:
			return "428 Precondition Required"sv;
		case HttpResponseStatus::TooManyRequests:
			return "429 Too Many Requests"sv;
		case HttpResponseStatus::RequestHeaderFieldsTooLarge:
			return "431 Request Header Fields Too Large"sv;
		case HttpResponseStatus::UnavailableForLegalReasons:
			return "451 Unavailable For Legal Reasons"sv;
		case HttpResponseStatus::InternalServerError:
			return "500 Internal Server Error"sv;
		case HttpResponseStatus::NotImplemented:
			return "501 Not Implemented"sv;
		case HttpResponseStatus::BadGateway:
			return "502 Bad Gateway"sv;
		case HttpResponseStatus::ServiceUnavailable:
			return "503 Service Unavailable"sv;
		case HttpResponseStatus::GatewayTimeout:
			return "504 Gateway Timeout"sv;
		case HttpResponseStatus::HTTPVersionNotSupported:
			return "505 HTTP Version Not Supported"sv;
		case HttpResponseStatus::VariantAlsoNegotiates:
			return "506 Variant Also Negotiates"sv;
		case HttpResponseStatus::InsufficientStorage:
			return "507 Insufficient Storage"sv;
		case HttpResponseStatus::LoopDetected:
			return "508 Loop Detected"sv;
		case HttpResponseStatus::NotExtended:
			return "510 Not Extended"sv;
		case HttpResponseStatus::NetworkAuthenticationRequired:
			return "511 Netowkr Authentication Required"sv;
		default:
			std::terminate();
	}
}

bool HttpServer::addConnection(Connection *conn) noexcept {
	if (!connections.insert({ conn }))
		return false;
	return true;
}

netknot::ExceptionPointer HttpServer::registerHandler(const std::string_view &name, HttpRequestHandler *handler) {
	peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> handlerPtr(handler);

	peff::ScopeGuard removeHandlerGuard([this, name]() noexcept {
		_removeHandlerRegistry(name);
	});
	if (!handlerRegistries.contains(name))
		NETKNOT_RETURN_IF_EXCEPT(_reserveHandlerRegistry(name));
	else
		removeHandlerGuard.release();

	if(!handlerRegistries.at(name).handlers.insert(std::string_view(handlerPtr->_methodName), std::move(handlerPtr)))
		return netknot::OutOfMemoryError::alloc();

	removeHandlerGuard.release();

	return {};
}
