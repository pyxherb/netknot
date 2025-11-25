#ifndef _NETKNOT_IO_SERVICE_H_
#define _NETKNOT_IO_SERVICE_H_

#include "socket.h"

namespace netknot {
	struct IOServiceCreationParams {
		peff::RcObjectPtr<peff::Alloc> allocator;
		size_t nWorkerThreads = 0;
		size_t szWorkerThreadStackSize = 0;

		NETKNOT_API IOServiceCreationParams(peff::Alloc *paramsAllocator, peff::Alloc *allocator);
		NETKNOT_API ~IOServiceCreationParams();
	};

	typedef ExceptionPointer (*AddressCompiler)(peff::Alloc *allocator, const Address &address, char *&bufferOut, size_t &szBufferOut);

	class IOService {
	public:
		NETKNOT_API IOService();
		NETKNOT_API ~IOService();

		virtual void dealloc() noexcept = 0;

		[[nodiscard]] virtual ExceptionPointer run() = 0;
		[[nodiscard]] virtual ExceptionPointer stop() = 0;

		virtual ExceptionPointer postAsyncTask(AsyncTask *task) noexcept = 0;

		virtual ExceptionPointer createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType, Socket *&socketOut) noexcept = 0;

		virtual ExceptionPointer translateAddress(peff::Alloc *allocator, const Address *address, TranslatedAddress **compiledAddressOut, size_t *compiledAddressSizeOut = nullptr) noexcept = 0;
		virtual ExceptionPointer detranslateAddress(peff::Alloc *allocator, const peff::UUID &addressFamily, const TranslatedAddress *address, Address &addressOut) noexcept = 0;
	};

	ExceptionPointer createDefaultIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept;
}

#endif
