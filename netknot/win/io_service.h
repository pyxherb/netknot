#ifndef _NETKNOT_WIN_IO_SERVICE_H_
#define _NETKNOT_WIN_IO_SERVICE_H_

#include "socket.h"
#include "../io_service.h"
#include <peff/base/deallocable.h>
#include <peff/containers/dynarray.h>
#include <peff/containers/map.h>
#include <Windows.h>

namespace netknot {
	class Win32CompiledAddress : public CompiledAddress {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		char *data = nullptr;
		size_t size = 0;

		NETKNOT_API Win32CompiledAddress(peff::Alloc *selfAllocator);
		NETKNOT_API virtual ~Win32CompiledAddress();

		NETKNOT_API virtual void dealloc() noexcept override;
	};

	class Win32IOService : public IOService {
	public:
		struct IOCPOverlapped : public OVERLAPPED {
		};

		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		HANDLE iocpCompletionPort = INVALID_HANDLE_VALUE;

		NETKNOT_API Win32IOService(peff::Alloc *selfAllocator);
		NETKNOT_API ~Win32IOService();

		NETKNOT_API static Win32IOService *alloc(peff::Alloc *selfAllocator);

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual void run() override;

		NETKNOT_API virtual ExceptionPointer createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType) override;

		NETKNOT_API virtual ExceptionPointer compileAddress(peff::Alloc *allocator, const Address &address, CompiledAddress *&compiledAddressOut) override;
	};
}

#endif
