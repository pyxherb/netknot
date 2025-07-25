#ifndef _NETKNOT_IO_SERVICE_H_
#define _NETKNOT_IO_SERVICE_H_

#include "socket.h"

namespace netknot {
	class IOService {
	public:
		NETKNOT_API IOService();
		NETKNOT_API ~IOService();

		virtual void dealloc() noexcept = 0;

		virtual ExceptionPointer createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType) = 0;
	};

	ExceptionPointer createDefaultIOService(peff::Alloc *allocator);
}

#endif
