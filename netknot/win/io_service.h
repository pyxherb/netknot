#ifndef _NETKNOT_WIN_IO_SERVICE_H_
#define _NETKNOT_WIN_IO_SERVICE_H_

#include "../io_service.h"

namespace netknot {
	class Win32IOService : public IOService {
	public:
		NETKNOT_API Win32IOService();
		NETKNOT_API ~Win32IOService();

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual ExceptionPointer createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType) override;
	};
}

#endif
