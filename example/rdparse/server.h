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

	enum class HttpParseStatus : uint8_t {
		Initial = 0,
		Header,
		Body
	};

	class EmplaceBuffer : public netknot::RcBuffer {
	public:
		EmplaceBuffer(char *data, size_t size);
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

		HttpAcceptAsyncCallback(HttpServer *httpServer, peff::Alloc *selfAllocator);

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
	};

	class HttpReadAsyncCallback : public netknot::ReadAsyncCallback {
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

		virtual ~HttpReadAsyncCallback();

		void onRefZero() noexcept override;

		static peff::Option<HttpRequestLineView> parseHttpRequestLine(std::string_view requestLine);

		static peff::Option<HttpRequestHeaderView> parseHttpRequestHeader(std::string_view requestHeader, peff::Alloc *allocator);

		virtual netknot::ExceptionPointer onStatusChanged(netknot::ReadAsyncTask *task) noexcept override;
	};

	class HttpWriteAsyncCallback : public netknot::WriteAsyncCallback {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator, allocator;
		Connection *connection;
		HttpServer *httpServer;

		HttpWriteAsyncCallback(HttpServer *httpServer, Connection *connection, peff::Alloc *selfAllocator, peff::Alloc *allocator);

		virtual ~HttpWriteAsyncCallback();

		virtual void onRefZero() noexcept override;

		virtual netknot::ExceptionPointer onStatusChanged(netknot::WriteAsyncTask *task) noexcept override;
	};

	class Connection {
	public:
		HttpServer *httpServer;
		std::unique_ptr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> socket;
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		HttpReadAsyncCallback requestCallback;
		HttpWriteAsyncCallback responseCallback;

		Connection(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket);
		~Connection();

		void dealloc() noexcept;

		static Connection *alloc(peff::Alloc *allocator, HttpServer *httpServer, netknot::Socket *socket);
	};

	struct HttpURLHandlerState {
	};

	class HttpRequestHandler {
	public:
		HttpRequestHandler();
		~HttpRequestHandler();

		virtual void dealloc() = 0;

		virtual netknot::ExceptionPointer handleURL(const HttpURLHandlerState &state) = 0;
	};

	struct HttpRequestHandlerRegistry {
		peff::RcObjectPtr<peff::Alloc> allocator;
		peff::String url;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> getHandler;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> headHandler;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> postHandler;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> putHandler;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> deleteHandler;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> connectHandler;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> optionsHandler;
		peff::UniquePtr<HttpRequestHandler, peff::DeallocableDeleter<HttpRequestHandler>> patchHandler;

		HttpRequestHandlerRegistry(peff::Alloc *allocator);
		~HttpRequestHandlerRegistry();
	};

	class HttpServer {
	private:
		[[nodiscard]] netknot::ExceptionPointer _reserveHandlerRegistry(const std::string_view &name);

	public:
		peff::RcObjectPtr<peff::Alloc> allocator;
		peff::UniquePtr<netknot::Socket, peff::DeallocableDeleter<netknot::Socket>> serverSocket;
		peff::Set<peff::UniquePtr<Connection, peff::DeallocableDeleter<Connection>>> connections;
		peff::HashMap<std::string_view, HttpRequestHandlerRegistry> handlerRegistries;

		HttpServer(peff::Alloc *allocator, netknot::Socket *serverSocket);

		[[nodiscard]] bool addConnection(Connection *conn) noexcept;

		[[nodiscard]] netknot::ExceptionPointer registerGetHandler(const std::string_view &name, HttpRequestHandler *handler);
	};
}

#endif
