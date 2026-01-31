#ifndef _NETKNOT_EXCEPT_H_
#define _NETKNOT_EXCEPT_H_

#include "except_base.h"
#include <new>

namespace netknot {
	constexpr static peff::UUID
		EXCEPT_OOM = PEFF_UUID(6e1a12d1, 2a61, 47dd, ac92, afc369290d1b),
		EXCEPT_BUFFER_IS_TOO_BIG = PEFF_UUID(11451419, 1981, 0114, 5141, 919810114514),
		EXCEPT_IO = PEFF_UUID(eed80e28, b8fb, 40de, 9b97, 81a8a6d688f3);

	/// @brief The out of memory error, indicates that a memory allocation has failed.
	class OutOfMemoryError : public Exception {
	public:
		NETKNOT_API OutOfMemoryError() noexcept;
		NETKNOT_API virtual ~OutOfMemoryError();

		NETKNOT_API virtual void dealloc() override;

		NETKNOT_API static OutOfMemoryError *alloc() noexcept;
	};

	extern OutOfMemoryError g_outOfMemoryError;

	class BufferIsTooBigError : public Exception {
	public:
		NETKNOT_API BufferIsTooBigError() noexcept;
		NETKNOT_API virtual ~BufferIsTooBigError();

		NETKNOT_API virtual void dealloc() override;

		NETKNOT_API static BufferIsTooBigError *alloc() noexcept;
	};

	extern BufferIsTooBigError g_bufferIsTooBigError;

	enum class NetworkErrorCode : uint32_t {
		Unknown = 0,
		AddressInUse,			 // Address is already in use
		NetworkIsDown,			 // Network is down
		ConnectionTimedOut,		 // Connection timed out
		SocketIsNotConnected,	 // Socket is not connected.
		AccessDenied,			 // Access denied
		TooManyOpenedFiles,		 // Too many opened files
		MessageSizeIsTooBig,	 // Message size is too big
		ProtocolNotSupported,	 // Protocol not supported
		SocketTypeNotSupported,	 // Socket type not supported
		AddressNotAvailable,	 // Address not available
		NetworkReseted,			 // Network reseted
		NetworkIsUnreachable,	 // Network is unreachable
		ConnectionReseted,		 // Connection reseted
		Shutdown,				 // Shutdown
		TimedOut,				 // Timed out
		ConnectionRefused,		 // Connection refused
		HostIsDown,				 // Host is down
		HostIsUnreachable,		 // Host is unreachable
		ResourceLimitExceeded,	 // Resource limit of local system exceeded
		SystemIsNotReady,		 // System is not ready
		UnsupportedPlatform,	 // Unsupported platform
		ErrorInit,				 // Error initializing
	};

	class NetworkError : public Exception {
	public:
		peff::RcObjectPtr<peff::Alloc> allocator;
		NetworkErrorCode errorCode;

		NETKNOT_API NetworkError(peff::Alloc *allocator, NetworkErrorCode errorCode);
		NETKNOT_API virtual ~NetworkError();

		NETKNOT_API virtual void dealloc() override;

		NETKNOT_API static NetworkError *alloc(peff::Alloc *allocator, NetworkErrorCode errorCode) noexcept;
	};

	NETKNOT_FORCEINLINE ExceptionPointer withOutOfMemoryErrorIfAllocFailed(Exception *exceptionPtr) noexcept {
		if (!exceptionPtr) {
			return OutOfMemoryError::alloc();
		}
		return exceptionPtr;
	}
}

#endif
