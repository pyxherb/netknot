#include "server.h"

using namespace http;

using std::operator""sv;

Connection::Connection(peff::Alloc *allocator, HttpServer *http_server, netknot::Socket *socket) noexcept : self_allocator(allocator), http_server(http_server), socket(socket) {
}
Connection::~Connection() {
}
void Connection::dealloc() noexcept {
	peff::destroy_and_release<Connection>(self_allocator.get(), this, alignof(Connection));
}
Connection *Connection::alloc(peff::Alloc *allocator, HttpServer *http_server, netknot::Socket *socket) {
	return peff::alloc_and_construct<Connection>(allocator, alignof(Connection), allocator, http_server, socket);
}

HandlerURL::HandlerURL(peff::Alloc *self_allocator) noexcept : self_allocator(self_allocator) {
}
HandlerURL::~HandlerURL() {
}
void HandlerURL::dealloc() noexcept {
	peff::destroy_and_release<HandlerURL>(self_allocator.get(), this, alignof(HandlerURL));
}

netknot::ExceptionPointer HttpURLHandlerState::write_status_line(HttpResponseStatus status) {
	if (this->stage != HttpURLHandlerStateStage::StatusLine)
		std::terminate();

	std::string_view httpVersion = "HTTP/1,1 "sv;

	if (!responseData.build(httpVersion))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append(HttpServer::get_http_response_msg(status)))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append("\r\n"))
		return netknot::OutOfMemoryError::alloc();

	this->stage = HttpURLHandlerStateStage::ResponseHeaders;

	return {};
}

netknot::ExceptionPointer HttpURLHandlerState::write_header(const std::string_view &name, const std::string_view &value) {
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

netknot::ExceptionPointer HttpURLHandlerState::end_header() {
	if (this->stage != HttpURLHandlerStateStage::ResponseHeaders)
		std::terminate();

	if (!responseData.append("\r\n"))
		return netknot::OutOfMemoryError::alloc();

	this->stage = HttpURLHandlerStateStage::ResponseBody;

	return {};
}

netknot::ExceptionPointer HttpURLHandlerState::write_body(const std::string_view &data) {
	if (this->stage != HttpURLHandlerStateStage::ResponseBody)
		std::terminate();

	if (!responseData.append(data))
		return netknot::OutOfMemoryError::alloc();

	if (!responseData.append("\r\n"))
		return netknot::OutOfMemoryError::alloc();

	return {};
}

netknot::ExceptionPointer HttpURLHandlerState::write_response(HttpResponseStatus status, const std::string_view &contentType, const std::string_view &body) {
	NETKNOT_RETURN_IF_EXCEPT(write_status_line(status));
	NETKNOT_RETURN_IF_EXCEPT(write_header("Content-Type", contentType));

	char lenStr[sizeof(size_t) / 3 + 1];

	sprintf(lenStr, "%zu", body.size());

	NETKNOT_RETURN_IF_EXCEPT(write_header("Content-Length", lenStr));
	NETKNOT_RETURN_IF_EXCEPT(end_header());
	NETKNOT_RETURN_IF_EXCEPT(write_body(body));

	this->stage = HttpURLHandlerStateStage::End;

	return {};
}

HttpRequestHandler::HttpRequestHandler(const std::string_view &methodName) noexcept : _methodName(methodName) {}
HttpRequestHandler::~HttpRequestHandler() {}

HttpRequestHandlerRegistry::HttpRequestHandlerRegistry(peff::Alloc *allocator) : allocator(allocator), baseUrl(allocator), handlers(allocator) {
}

HttpRequestHandlerRegistry::~HttpRequestHandlerRegistry() {
}

HttpAcceptAsyncCallback::HttpAcceptAsyncCallback(HttpServer *http_server, peff::Alloc *self_allocator) noexcept : http_server(http_server), self_allocator(self_allocator) {
}
HttpAcceptAsyncCallback::~HttpAcceptAsyncCallback() {
}
void HttpAcceptAsyncCallback::on_ref_zero() noexcept {
	peff::destroy_and_release<HttpAcceptAsyncCallback>(self_allocator.get(), this, alignof(HttpAcceptAsyncCallback));
}
netknot::ExceptionPointer HttpAcceptAsyncCallback::on_accepted(netknot::Socket *socket) noexcept {
	peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> s(socket);

	peff::UniquePtr<Connection, peff::DeallocableDeleter<Connection>> conn(Connection::alloc(self_allocator.get(), http_server, socket));

	if (!conn)
		return netknot::OutOfMemoryError::alloc();

	s.release();

	if (!http_server->add_connection(conn.get()))
		return netknot::OutOfMemoryError::alloc();

	peff::RcObjectPtr<netknot::ReadAsyncTask> task;

	EmplaceBuffer bb(buffer, sizeof(buffer));
	netknot::RcBufferRef bufferRef(&*(emplaceBuffer = peff::Option<EmplaceBuffer>(std::move(bb))));

	if (!(conn->request_callback = peff::alloc_and_construct<HttpReadAsyncCallback>(
			  http_server->allocator.get(), alignof(HttpReadAsyncCallback),
			  http_server,
			  conn.get(),
			  &peff::g_null_alloc,
			  http_server->allocator.get())))
		return netknot::OutOfMemoryError::alloc();
	NETKNOT_RETURN_IF_EXCEPT(conn->socket->read_async(
		self_allocator.get(),
		bufferRef,
		conn->request_callback.get(),
		task.get_ref()));

	conn.release();

	peff::RcObjectPtr<netknot::AcceptAsyncTask> accept_async_task;
	if (auto e = http_server->server_socket->accept_async(peff::default_allocator(), this, accept_async_task.get_ref()); e) {
		return e;
	}

	return {};
}

EmplaceBuffer::EmplaceBuffer(char *data, size_t size) : RcBuffer(data, size) {}
EmplaceBuffer::~EmplaceBuffer() {}
size_t EmplaceBuffer::inc_ref(size_t globalRc) {
	return 0;
}
size_t EmplaceBuffer::dec_ref(size_t globalRc) {
	return 0;
}

HttpRequestHeaderView::HttpRequestHeaderView(peff::Alloc *allocator) : headers(allocator) {}

HttpReadAsyncCallback::HttpReadAsyncCallback(HttpServer *http_server, Connection *connection, peff::Alloc *self_allocator, peff::Alloc *allocator) : http_server(http_server), connection(connection), self_allocator(self_allocator), allocator(allocator), request_line(allocator), request_header(allocator), body(allocator), request_header_view(allocator) {
}

HttpReadAsyncCallback::~HttpReadAsyncCallback() {
}

void HttpReadAsyncCallback::on_ref_zero() noexcept {
	peff::destroy_and_release<HttpReadAsyncCallback>(self_allocator.get(), this, alignof(HttpReadAsyncCallback));
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

peff::Option<HttpRequestHeaderView> HttpReadAsyncCallback::parseHttpRequestHeader(std::string_view request_header, peff::Alloc *allocator) {
	std::string_view sv = request_header;
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

netknot::ExceptionPointer HttpReadAsyncCallback::on_status_changed(netknot::ReadAsyncTask *task) noexcept {
	switch (task->get_status()) {
		case netknot::AsyncTaskStatus::Done: {
			const char *const p = task->get_buffer();
			const size_t realSize = task->get_cur_read_size();
			std::string_view sv(p, realSize);
			size_t offNext = 0;

			auto readNext = [this, task]() -> netknot::ExceptionPointer {
				peff::RcObjectPtr<netknot::ReadAsyncTask> taskReceiver;
				return connection->socket->read_async(http_server->allocator.get(), task->get_buffer_ref(), this, taskReceiver.get_ref());
			};

			switch (parse_status) {
				case HttpParseStatus::Initial: {
					size_t offSeparator = sv.find("\r\n");

					if (offSeparator != std::string_view::npos) {
						std::string_view requestLineSv = sv.substr(0, offSeparator);
						if (!request_line.append(requestLineSv)) {
							return netknot::OutOfMemoryError::alloc();
						}

						offNext = offSeparator + 2;

						if (offNext >= sv.size())
							return readNext();

						sv = sv.substr(offNext);
					} else {
						if (!request_line.append(sv)) {
							return netknot::OutOfMemoryError::alloc();
						}
						std::string_view combinedSv = request_line;
						if ((offSeparator = combinedSv.find("\r\n")) == std::string_view::npos)
							return readNext();
					}

					if (auto result = parseHttpRequestLine(request_line); result.has_value()) {
						request_line_view = result.move();
					} else {
						std::terminate();
					}

					parse_status = HttpParseStatus::Header;
					[[fallthrough]];
				}
				case HttpParseStatus::Header: {
					size_t offSeparator = sv.find("\r\n\r\n");

					if (offSeparator != std::string_view::npos) {
						if (!request_header.append(sv.substr(0, offSeparator + 2)))
							return netknot::OutOfMemoryError::alloc();
						offNext = offSeparator + 4;

						sv = sv.substr(offNext);
					} else {
						if (!request_header.append(sv))
							return netknot::OutOfMemoryError::alloc();

						std::string_view combinedSv = request_header;

						size_t off = combinedSv.find("\r\n\r\n");
						if (off == std::string_view::npos)
							return readNext();
					}

					if (auto result = parseHttpRequestHeader(request_header, allocator.get()); result.has_value()) {
						request_header_view = result.move();
					} else {
						std::terminate();
					}

					if (auto it = request_header_view.headers.find("Transfer-Encoding"); it != request_header_view.headers.end()) {
						transfer_encoding = it.value();

						if (transfer_encoding == "chunked") {
							is_chunked = true;
							goto no_content_length;
						}
					}
					if (auto it = request_header_view.headers.find("Content-Length"); it != request_header_view.headers.end()) {
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

						if (!body.resize_uninit(contentLength)) {
							return netknot::OutOfMemoryError::alloc();
						}

						expected_body_size = contentLength;
					}
				no_content_length:

					parse_status = HttpParseStatus::Body;
					if (!is_chunked) {
						if (expected_body_size) {
							EmplaceBuffer bb(body.data(), expected_body_size);
							netknot::RcBufferRef bufferRef(&*(body_buffer = peff::Option<EmplaceBuffer>(std::move(bb))));

							peff::RcObjectPtr<netknot::ReadAsyncTask> task;
							return connection->socket->read_async(http_server->allocator.get(), bufferRef, this, task.get_ref());
						}
					} else {
						std::terminate();
					}
					[[fallthrough]];
				}
				case HttpParseStatus::Body: {
					if (is_chunked) {
						std::terminate();
					} else {
						std::string_view rawPath = request_line_view.path;
						std::string_view pathView, queryView, fragmentView;

						HttpURLHandlerState urlHandlerState = {
							http_server,
							connection,
							pathView,
							queryView,
							fragmentView,
							request_header_view,
							peff::String(http_server->allocator.get())
						};

						size_t offQuery = rawPath.find_first_of('?', 0);
						size_t offFragment = rawPath.find_first_of('#', 0);

						if (offFragment != std::string_view::npos) {
							if (offQuery != std::string_view::npos) {
								if (offQuery > offFragment) {
									urlHandlerState.write_response(HttpResponseStatus::BadRequest, "text/plain", "");
									goto write_response;
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

						if (auto it = http_server->handlerRegistries.find(pathView); it != http_server->handlerRegistries.end()) {
							const auto &registry = it.value();
							if (auto jt = registry.handlers.find(request_line_view.method); jt != registry.handlers.end())
								NETKNOT_RETURN_IF_EXCEPT(jt.value()->handleURL(urlHandlerState));
							else
								urlHandlerState.write_response(HttpResponseStatus::MethodNotAllowed, "text/plain", "");
						} else
							urlHandlerState.write_response(HttpResponseStatus::NotFound, "text/plain", "");

					write_response:
						if (!(connection->response_callback = peff::alloc_and_construct<HttpWriteAsyncCallback>(
							http_server->allocator.get(), alignof(HttpWriteAsyncCallback),
								  http_server, connection, http_server->allocator.get(), http_server->allocator.get())))
							return netknot::OutOfMemoryError::alloc();
						HttpWriteAsyncCallback *callback = connection->response_callback.get();

						callback->bufferData = std::move(urlHandlerState.responseData);
						callback->buffer = EmplaceBuffer(callback->bufferData.data(), callback->bufferData.size());
						netknot::RcBufferRef bufferRef(&*callback->buffer);

						peff::RcObjectPtr<netknot::WriteAsyncTask> task;
						NETKNOT_RETURN_IF_EXCEPT(connection->socket->write_async(http_server->allocator.get(), bufferRef, callback, task.get_ref()));
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

HttpWriteAsyncCallback::HttpWriteAsyncCallback(HttpServer *http_server, Connection *connection, peff::Alloc *self_allocator, peff::Alloc *allocator) : http_server(http_server), connection(connection), self_allocator(self_allocator), allocator(allocator), bufferData(allocator) {
}
HttpWriteAsyncCallback::~HttpWriteAsyncCallback() {
}
void HttpWriteAsyncCallback::on_ref_zero() noexcept {
	peff::destroy_and_release<HttpWriteAsyncCallback>(self_allocator.get(), this, alignof(HttpWriteAsyncCallback));
}
netknot::ExceptionPointer HttpWriteAsyncCallback::on_status_changed(netknot::WriteAsyncTask *task) noexcept {
	switch (task->get_status()) {
		case netknot::AsyncTaskStatus::Done: {
			break;
		}
		case netknot::AsyncTaskStatus::Interrupted: {
			break;
		}
		default:
			std::terminate();
	}

	return {};
}

netknot::ExceptionPointer HttpServer::_reserve_handler_registry(const std::string_view &name) {
	HttpRequestHandlerRegistry registry(allocator.get());

	if (!registry.baseUrl.build(name))
		return netknot::OutOfMemoryError::alloc();

	if (!handlerRegistries.insert(registry.baseUrl, std::move(registry)))
		return netknot::OutOfMemoryError::alloc();

	return {};
}

void HttpServer::_remove_handler_registry(const std::string_view &name) {
	assert(handlerRegistries.contains(name));

	handlerRegistries.remove(name);
}

HttpServer::HttpServer(peff::Alloc *allocator, netknot::IOService *io_service, netknot::Socket *server_socket) : allocator(allocator), io_service(io_service), connections(allocator), server_socket(server_socket), handlerRegistries(allocator) {}

std::string_view HttpServer::get_http_response_msg(HttpResponseStatus status) {
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

bool HttpServer::add_connection(Connection *conn) noexcept {
	if (!connections.insert({ conn }))
		return false;
	return true;
}

netknot::ExceptionPointer HttpServer::register_handler(const std::string_view &name, HttpRequestHandler *handler) {
	peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> handlerPtr(handler);

	peff::ScopeGuard remove_handler_guard([this, name]() noexcept {
		_remove_handler_registry(name);
	});
	if (!handlerRegistries.contains(name))
		NETKNOT_RETURN_IF_EXCEPT(_reserve_handler_registry(name));
	else
		remove_handler_guard.release();

	if(!handlerRegistries.at(name).handlers.insert(std::string_view(handlerPtr->_methodName), std::move(handlerPtr)))
		return netknot::OutOfMemoryError::alloc();

	remove_handler_guard.release();

	return {};
}
