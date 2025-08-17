#include <netknot/io_service.h>
#include <peff/base/deallocable.h>
#include <peff/advutils/unique_ptr.h>

int main() {
	{
		peff::UniquePtr<netknot::IOService, peff::DeallocableDeleter<netknot::IOService>> ioService;

		netknot::IOServiceCreationParams params(peff::getDefaultAlloc(), peff::getDefaultAlloc());

		netknot::ExceptionPointer e = netknot::createDefaultIOService(ioService.getRef(), params);

		if (e) {
			std::terminate();
		}
	}
}
