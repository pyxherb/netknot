#ifndef _NETKNOT_BASE_BASEDEFS_H_
#define _NETKNOT_BASE_BASEDEFS_H_

#include <peff/base/basedefs.h>

#if defined(_MSC_VER)
	#define NETKNOT_DLLEXPORT __declspec(dllexport)
	#define NETKNOT_DLLIMPORT __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
	#define NETKNOT_DLLEXPORT __attribute__((__visibility__("default")))
	#define NETKNOT_DLLIMPORT __attribute__((__visibility__("default")))
#endif

#define NETKNOT_FORCEINLINE PEFF_FORCEINLINE

#if defined(_MSC_VER)
	#define NETKNOT_DECL_EXPLICIT_INSTANTIATED_CLASS(apiModifier, name, ...) \
		apiModifier extern template class name<__VA_ARGS__>;
	#define NETKNOT_DEF_EXPLICIT_INSTANTIATED_CLASS(apiModifier, name, ...) \
		apiModifier template class name<__VA_ARGS__>;
#elif defined(__GNUC__) || defined(__clang__)
	#define NETKNOT_DECL_EXPLICIT_INSTANTIATED_CLASS(apiModifier, name, ...) \
		extern template class apiModifier name<__VA_ARGS__>;
	#define NETKNOT_DEF_EXPLICIT_INSTANTIATED_CLASS(apiModifier, name, ...) \
		template class name<__VA_ARGS__>;
#else
	#define NETKNOT_DECL_EXPLICIT_INSTANTIATED_CLASS(apiModifier, name, ...)
	#define NETKNOT_DEF_EXPLICIT_INSTANTIATED_CLASS(apiModifier, name, ...)
#endif

#if NETKNOT_STATIC_LINK
	#define NETKNOT_API
#else
	#if IS_NETKNOT_BUILDING
		#define NETKNOT_API NETKNOT_DLLEXPORT
	#else
		#define NETKNOT_API NETKNOT_DLLIMPORT
	#endif
#endif

#endif
