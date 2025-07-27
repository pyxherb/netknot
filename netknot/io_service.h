#ifndef _NETKNOT_IO_SERVICE_H_
#define _NETKNOT_IO_SERVICE_H_

#include "socket.h"

namespace netknot {
	struct IOServiceCreationParams {
		peff::RcObjectPtr<peff::Alloc> allocator;
		size_t nWorkerThreads = 0;

		NETKNOT_API IOServiceCreationParams(peff::Alloc *paramsAllocator, peff::Alloc *allocator);
		NETKNOT_API ~IOServiceCreationParams();
	};

	class IOService {
	public:
		NETKNOT_API IOService();
		NETKNOT_API ~IOService();

		virtual void dealloc() noexcept = 0;

		virtual void run() = 0;

		virtual ExceptionPointer createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType) = 0;
	};

	ExceptionPointer createDefaultIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept;
}

#endif
