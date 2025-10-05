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

		peff::UniquePtr<netknot::CompiledAddress, peff::DeallocableDeleter<netknot::CompiledAddress>> compiledAddr;
		{
			netknot::IPv4Address addr(0, 0, 0, 0, 8080);

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

		if ((e = socket->listen(114514))) {
			std::terminate();
		}

		http::HttpServer httpServer(peff::getDefaultAlloc());

		http::HttpAcceptAsyncCallback callback(&httpServer, peff::getDefaultAlloc());
		peff::RcObjectPtr<netknot::AcceptAsyncTask> acceptAsyncTask;
		if ((e = socket->acceptAsync(peff::getDefaultAlloc(), &callback, acceptAsyncTask.getRef()))) {
			std::terminate();
		}

		if ((e = ioService->run()))
			std::terminate();
	}
}
