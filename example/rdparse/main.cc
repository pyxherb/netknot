#include "server.h"

int main() {
	netknot::ExceptionPointer e;
	{
		peff::UniquePtr<netknot::IOService, peff::DeallocableDeleter<netknot::IOService>> ioService;
		{
			netknot::IOServiceCreationParams params(peff::getDefaultAlloc(), peff::getDefaultAlloc());
			params.nWorkerThreads = 16;

			if ((e = netknot::createDefaultIOService(ioService.getRef(), params))) {
				std::terminate();
			}
		}

		peff::UniquePtr<netknot::TranslatedAddress, peff::DeallocableDeleter<netknot::TranslatedAddress>> compiledAddr;
		{
			netknot::IPv4Address addr(0, 0, 0, 0, 8080);

			if ((e = ioService->translateAddress(peff::getDefaultAlloc(), &addr, compiledAddr.getAddressOf()))) {
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

		if ((e = socket->listen(114514))) {
			std::terminate();
		}

		http::HttpServer httpServer(peff::getDefaultAlloc(), ioService.get(), socket.release());

		peff::RcObjectPtr<http::HttpAcceptAsyncCallback> callback;

		if (!(callback = peff::allocAndConstruct<http::HttpAcceptAsyncCallback>(peff::getDefaultAlloc(), alignof(http::HttpAcceptAsyncCallback), & httpServer, peff::getDefaultAlloc())))
			std::terminate();

		peff::RcObjectPtr<netknot::AcceptAsyncTask> acceptAsyncTask;
		if ((e = httpServer.serverSocket->acceptAsync(peff::getDefaultAlloc(), callback.get(), acceptAsyncTask.getRef()))) {
			std::terminate();
		}

		peff::UniquePtr<http::HttpRequestHandler, peff::DeallocableDeleter<http::HttpRequestHandler>> stopGetHandler = http::allocFnHttpRequestHandler(
			peff::getDefaultAlloc(),
			"GET",
			[](const http::HttpURLHandlerState &state) -> netknot::ExceptionPointer {
				return state.httpServer->ioService->stop();
			});
		if (!stopGetHandler)
			std::terminate();

		if ((e = httpServer.registerHandler("/stop", stopGetHandler.release()))) {
			std::terminate();
		}

		if ((e = ioService->run()))
			std::terminate();
	}
}
