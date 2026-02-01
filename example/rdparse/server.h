#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <netknot/io_service.h>
#include <peff/base/deallocable.h>
#include <peff/advutils/unique_ptr.h>
#include <peff/containers/hashmap.h>
#include <peff/containers/string.h>
#include <peff/containers/set.h>

namespace http {
	class HttpServer;
	class Connection;

	enum class HttpResponseStatus : uint16_t {
		Continue = 100,
		SwitchingProtocols = 101,
		Processing = 102,
		EarlyHints = 103,

		OK = 200,
		Created = 201,
		Accepted = 202,
		NonAuthoritativeInformation = 203,
		NoContent = 204,
		ResetContent = 205,
		PartialContent = 206,
		MultiStatus = 207,
		AlreadyReported = 208,
		IMUsed = 226,

		MultipleChoice = 300,
		MovedPermanently = 301,
		Found = 302,
		SeeOther = 303,
		NotModified = 304,
		UseProxy = 305,
		TemporaryRedirect = 307,
		PermanentRedirect = 308,

		BadRequest = 400,
		Unauthorized = 401,
		PaymentRequired = 402,
		Forbidden = 403,
		NotFound = 404,
		MethodNotAllowed = 405,
		NotAcceptable = 406,
		ProxyAuthenticationRequired = 407,
		RequestTimeout = 408,
		Conflict = 409,
		Gone = 410,
		LengthRequired = 411,
		PreconditionFailed = 412,
		PayloadTooLarge = 413,
		URITooLong = 414,
		UnsupportedMediaType = 415,
		RangeNotSatisfiable = 416,
		ExpectationFailed = 417,
		ImATeapot = 418,
		MisdirectedRequest = 421,
		UnprocessableEntity = 422,
		Locked = 423,
		FailedDependency = 424,
		TooEarly = 425,
		UpgradeRequired = 426,
		PreconditionRequired = 428,
		TooManyRequests = 429,
		RequestHeaderFieldsTooLarge = 431,
		UnavailableForLegalReasons = 451,

		InternalServerError = 500,
		NotImplemented = 501,
		BadGateway = 502,
		ServiceUnavailable = 503,
		GatewayTimeout = 504,
		HTTPVersionNotSupported = 505,
		VariantAlsoNegotiates = 506,
		InsufficientStorage = 507,
		LoopDetected = 508,
		NotExtended = 510,
		NetworkAuthenticationRequired = 511
	};

	enum class HttpParseStatus : uint8_t {
		Initial = 0,
		Header,
		Body
	};

	class EmplaceBuffer : public netknot::RcBuffer {
	public:
		EmplaceBuffer(char *data, size_t size);
		EmplaceBuffer(EmplaceBuffer &&) = default;
		~EmplaceBuffer();

		virtual size_t incRef(size_t globalRc) override;
		virtual size_t decRef(size_t globalRc) override;
	};

	class HttpAcceptAsyncCallback : public netknot::AcceptAsyncCallback {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		HttpServer *httpServer;
		char buffer[4096];
		peff::Option<EmplaceBuffer> emplaceBuffer;

		HttpAcceptAsyncCallback(HttpServer *httpServer, peff::Alloc *selfAllocator) noexcept;

		virtual ~HttpAcceptAsyncCallback();

		virtual void onRefZero() noexcept override;

		virtual netknot::ExceptionPointer onAccepted(netknot::Socket *socket) noexcept override;
	};

	struct HttpRequestLineView {
		std::string_view method;
		std::string_view path;
		std::string_view version;
	};

	struct HttpRequestHeaderView {
		peff::HashMap<std::string_view, std::string_view> headers;

		HttpRequestHeaderView(peff::Alloc *allocator);
		HttpRequestHeaderView(HttpRequestHeaderView &&) = default;

		HttpRequestHeaderView &operator=(HttpRequestHeaderView &&) = default;
	};

	class HttpReadAsyncCallback final : public netknot::ReadAsyncCallback {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator, allocator;
		HttpServer *httpServer;
		Connection *connection;
		HttpParseStatus parseStatus = HttpParseStatus::Initial;
		peff::String requestLine, requestHeader;
		HttpRequestLineView requestLineView;
		HttpRequestHeaderView requestHeaderView;

		size_t expectedBodySize = 0;
		size_t szRead = 0;
		std::string_view transferEncoding;
		bool isChunked = false;
		peff::DynArray<char> body;

		peff::Option<EmplaceBuffer> bodyBuffer;
		peff::Option<EmplaceBuffer> responseBuffer;

		HttpReadAsyncCallback(HttpServer *httpServer, Connection *connection, peff::Alloc *selfAllocator, peff::Alloc *allocator);
		HttpReadAsyncCallback(const HttpReadAsyncCallback &) = delete;

		virtual ~HttpReadAsyncCallback();

		void onRefZero() noexcept override;

		static peff::Option<HttpRequestLineView> parseHttpRequestLine(std::string_view requestLine);

		static peff::Option<HttpRequestHeaderView> parseHttpRequestHeader(std::string_view requestHeader, peff::Alloc *allocator);

		virtual netknot::ExceptionPointer onStatusChanged(netknot::ReadAsyncTask *task) noexcept override;
	};

	class HttpWriteAsyncCallback final : public netknot::WriteAsyncCallback {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator, allocator;
		Connection *connection;
		HttpServer *httpServer;
		peff::String bufferData;
		peff::Option<EmplaceBuffer> buffer;

		HttpWriteAsyncCallback(HttpServer *httpServer, Connection *connection, peff::Alloc *selfAllocator, peff::Alloc *allocator);
		HttpWriteAsyncCallback(const HttpWriteAsyncCallback &) = delete;

		virtual ~HttpWriteAsyncCallback();

		virtual void onRefZero() noexcept override;

		virtual netknot::ExceptionPointer onStatusChanged(netknot::WriteAsyncTask *task) noexcept override;
	};

	class Connection {
	public:
		HttpServer *httpServer;
		std::unique_ptr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> socket;
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		peff::RcObjectPtr<HttpReadAsyncCallback> requestCallback;
		peff::RcObjectPtr<HttpWriteAsyncCallback> responseCallback;

		Connection(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket) noexcept;
		~Connection();

		void dealloc() noexcept;

		static Connection *alloc(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket);
	};

	enum class HttpURLHandlerStateStage : uint8_t {
		StatusLine = 0,
		ResponseHeaders,
		ResponseBody,
		End
	};

	struct HttpURLHandlerState {
		HttpServer *httpServer;
		Connection *connection;
		const std::string_view &urlPath;
		const std::string_view &urlQuery;
		const std::string_view &urlFragment;
		const HttpRequestHeaderView &requestHeaderView;
		peff::String responseData;

		HttpURLHandlerStateStage stage = HttpURLHandlerStateStage::StatusLine;

		netknot::ExceptionPointer writeStatusLine(HttpResponseStatus stauts);
		netknot::ExceptionPointer writeHeader(const std::string_view &name, const std::string_view &value);
		netknot::ExceptionPointer endHeader();
		netknot::ExceptionPointer writeBody(const std::string_view &data);
		netknot::ExceptionPointer writeResponse(HttpResponseStatus status, const std::string_view &contentType, const std::string_view &body);
	};

	class HttpRequestHandler {
	private:
		const std::string_view _methodName;
		friend class HttpServer;

	public:
		HttpRequestHandler(const std::string_view &methodName) noexcept;
		virtual ~HttpRequestHandler();

		virtual void dealloc() noexcept = 0;

		virtual netknot::ExceptionPointer handleURL(const HttpURLHandlerState &state) = 0;
	};

	template <typename Fn>
	class FnHttpRequestHandler : public HttpRequestHandler {
	public:
		using ThisType = FnHttpRequestHandler<Fn>;

		peff::RcObjectPtr<peff::Alloc> allocator;
		Fn f;

		static_assert(std::is_invocable_v<Fn, const HttpURLHandlerState &>, "The callback is malformed");
		static_assert(std::is_same_v<std::invoke_result_t<Fn, const HttpURLHandlerState &>, netknot::ExceptionPointer>, "The callback is malformed");

		FnHttpRequestHandler(peff::Alloc *allocator, const std::string_view &methodName, Fn &&f) noexcept : HttpRequestHandler(methodName), allocator(allocator), f(std::move(f)) {
		}
		virtual ~FnHttpRequestHandler() {
		}

		virtual void dealloc() noexcept override {
			peff::destroyAndRelease<ThisType>(allocator.get(), this, alignof(ThisType));
		}

		virtual netknot::ExceptionPointer handleURL(const HttpURLHandlerState &state) override {
			return f(state);
		}
	};

	template <typename Fn>
	FnHttpRequestHandler<Fn> *allocFnHttpRequestHandler(peff::Alloc *allocator, const std::string_view &methodName, Fn &&f) noexcept {
		return peff::allocAndConstruct<FnHttpRequestHandler<Fn>>(allocator, alignof(FnHttpRequestHandler<Fn>), allocator, methodName, std::move(f));
	}

	class HandlerURL {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;

		HandlerURL(peff::Alloc *selfAllocator) noexcept;
		~HandlerURL();

		void dealloc() noexcept;
	};

	struct HttpRequestHandlerRegistry {
		peff::RcObjectPtr<peff::Alloc> allocator;
		peff::String baseUrl;
		peff::HashMap<std::string_view, peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>>> handlers;

		HttpRequestHandlerRegistry(peff::Alloc *allocator);
		HttpRequestHandlerRegistry(HttpRequestHandlerRegistry &&) = default;
		~HttpRequestHandlerRegistry();
	};

	class HttpServer {
	private:
		[[nodiscard]] netknot::ExceptionPointer _reserveHandlerRegistry(const std::string_view &name);
		void _removeHandlerRegistry(const std::string_view &name);

	public:
		peff::RcObjectPtr<peff::Alloc> allocator;
		netknot::IOService *ioService;
		peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> serverSocket;
		peff::Set<peff::UniquePtr<Connection, peff::DeallocableDeleter<Connection>>> connections;
		peff::HashMap<std::string_view, HttpRequestHandlerRegistry> handlerRegistries;

		HttpServer(peff::Alloc *allocator, netknot::IOService *ioService, netknot::Socket *serverSocket);

		static std::string_view getHttpResponseMessage(HttpResponseStatus status);

		[[nodiscard]] bool addConnection(Connection *conn) noexcept;

		[[nodiscard]] netknot::ExceptionPointer registerHandler(const std::string_view &name, HttpRequestHandler *handler);
	};
}

#endif
