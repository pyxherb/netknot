#ifndef _NETKNOT_EXCEPT_BASE_H_
#define _NETKNOT_EXCEPT_BASE_H_

#include "basedefs.h"
#include <peff/base/uuid.h>
#include <peff/base/alloc.h>

namespace netknot {
	class Exception {
	public:
		peff::UUID kind;

		NETKNOT_API Exception(const peff::UUID &kind);
		NETKNOT_API virtual ~Exception();

		virtual void dealloc() = 0;
	};

	class ExceptionPointer {
	private:
		Exception *_ptr = nullptr;

	public:
		NETKNOT_FORCEINLINE ExceptionPointer() noexcept = default;
		NETKNOT_FORCEINLINE ExceptionPointer(Exception *exception) noexcept : _ptr(exception) {
		}

		NETKNOT_FORCEINLINE ~ExceptionPointer() noexcept {
			unwrap();
			reset();
		}

		ExceptionPointer(const ExceptionPointer &) = delete;
		ExceptionPointer &operator=(const ExceptionPointer &) = delete;
		NETKNOT_FORCEINLINE ExceptionPointer(ExceptionPointer &&other) noexcept {
			_ptr = other._ptr;
			other._ptr = nullptr;
		}
		NETKNOT_FORCEINLINE ExceptionPointer &operator=(ExceptionPointer &&other) noexcept {
			_ptr = other._ptr;
			other._ptr = nullptr;
			return *this;
		}

		NETKNOT_FORCEINLINE Exception *get() noexcept {
			return _ptr;
		}
		NETKNOT_FORCEINLINE const Exception *get() const noexcept {
			return _ptr;
		}

		NETKNOT_FORCEINLINE void reset() noexcept {
			if (_ptr) {
				_ptr->dealloc();
			}
			_ptr = nullptr;
		}

		NETKNOT_FORCEINLINE void unwrap() noexcept {
			if (_ptr) {
				assert(("Unhandled NetKnot exception: ", false));
			}
		}

		NETKNOT_FORCEINLINE explicit operator bool() noexcept {
			return (bool)_ptr;
		}

		NETKNOT_FORCEINLINE Exception *operator->() noexcept {
			return _ptr;
		}

		NETKNOT_FORCEINLINE const Exception *operator->() const noexcept {
			return _ptr;
		}
	};
}

#define NETKNOT_UNWRAP_EXCEPT(expr) (expr).unwrap()
#define NETKNOT_RETURN_IF_EXCEPT(expr)                         \
	if (netknot::ExceptionPointer e = (expr); (bool)e) \
	return e
#define NETKNOT_RETURN_IF_EXCEPT_WITH_LVAR(name, expr) \
	if ((bool)(name = (expr)))                         \
		return name;

#endif
