#pragma once

#if defined(_MSC_VER)
#define CLU_API_IMPORT __declspec(dllimport)
#define CLU_API_EXPORT __declspec(dllexport)
#define CLU_SUPPRESS_EXPORT_WARNING __pragma(warning(push)) __pragma(warning(disable : 4251 4275))
#define CLU_RESTORE_EXPORT_WARNING __pragma(warning(pop))
#elif defined(__GNUC__)
#define CLU_API_IMPORT
#define CLU_API_EXPORT __attribute__((visibility("default")))
#define CLU_SUPPRESS_EXPORT_WARNING
#define CLU_RESTORE_EXPORT_WARNING
#else
#define CLU_API_IMPORT
#define CLU_API_EXPORT
#define CLU_SUPPRESS_EXPORT_WARNING
#define CLU_RESTORE_EXPORT_WARNING
#endif

#if defined(CLU_BUILD_SHARED)
#ifdef CLU_EXPORT_SHARED
#define CLU_API CLU_API_EXPORT
#else
#define CLU_API CLU_API_IMPORT
#endif
#else
#define CLU_API
#endif
