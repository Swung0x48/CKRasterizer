#ifndef VXMATHCOMPILER_H
#define VXMATHCOMPILER_H

// Compiler-specific macros for VxMath.

#ifndef __cplusplus
#   error "C++ compiler required."
#endif

#if defined(_MSC_VER)
#   define VX_MSVC _MSC_VER
#elif defined(__GNUC__)
#   define VX_GCC __GNUC__
#endif

#if defined(_MSC_VER) // Microsoft Visual C++
#   if _MSC_VER < 1200 // Visual Studio 6.0
#       error "Unsupported compiler."
#   endif
#   if _MSC_VER >= 1400 // .Net 2005 and higher
#       ifndef _CRT_SECURE_NO_WARNINGS
#           define _CRT_SECURE_NO_WARNINGS
#       endif
#   endif
#endif

#ifndef VX_EXPORT
#   ifdef VX_LIB
#       define VX_EXPORT
#   else
#       ifdef VX_API
#           if defined(WIN32)
#               define VX_EXPORT __declspec(dllexport)
#           else
#               define VX_EXPORT
#           endif
#       else
#           if defined(WIN32)
#               define VX_EXPORT __declspec(dllimport)
#           else
#               define VX_EXPORT
#           endif
#       endif // VX_API
#   endif // VX_LIB
#endif // !VX_EXPORT

// EXPORT DEFINES FOR LIB / DLL VERSIONS
#ifndef CK_LIB
#   ifdef CK_PRIVATE_VERSION_VIRTOOLS
#       define DLL_EXPORT __declspec(dllexport)	// VC++ export option
#   else
#       define DLL_EXPORT
#   endif
#else
#   define DLL_EXPORT
#endif

#ifndef CK_LIB
#   define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#   define PLUGIN_EXPORT
#endif // CK_LIB

#ifdef __cplusplus
#   define BEGIN_CDECLS extern "C" {
#   define END_CDECLS }
#else
#   define BEGIN_CDECLS
#   define END_CDECLS
#endif

#ifndef VX_DEPRECATED
#   if defined(_MSC_VER)
#       define VX_DEPRECATED deprecated
#   elif defined(__GNUC__)
#       define VX_DEPRECATED __attribute__((__deprecated__))
#   else
#       define VX_DEPRECATED
#   endif
#endif

#ifndef VX_NAKED
#   if defined(_MSC_VER)
#       define VX_NAKED __declspec(naked)
#   elif defined(__GNUC__)
#       define VX_NAKED __attribute__((naked))
#   else
#       define VX_NAKED
#   endif
#endif

#ifndef VX_CDECL
#   if defined(_MSC_VER)
#       define VX_CDECL __cdecl
#   elif defined(__GNUC__)
#       define VX_CDECL __attribute__((cdecl))
#   else
#       define VX_CDECL
#   endif
#endif

#ifndef VX_FASTCALL
#   if defined(_MSC_VER)
#       define VX_FASTCALL __fastcall
#   elif defined(__GNUC__)
#       define VX_FASTCALL __attribute__((fastcall))
#   else
#       define VX_FASTCALL
#   endif
#endif

#ifndef VX_STDCALL
#   if defined(_MSC_VER)
#       define VX_STDCALL __stdcall
#   elif defined(__GNUC__)
#       define VX_STDCALL __attribute__((stdcall))
#   else
#       define VX_STDCALL
#   endif
#endif

#ifndef VX_THISCALL
#   if defined(_MSC_VER)
#       define VX_THISCALL __thiscall
#   elif defined(__GNUC__)
#       define VX_THISCALL __attribute__((thiscall))
#   else
#       define VX_THISCALL
#   endif
#endif

#ifndef VX_ALIGN
#   if defined(_MSC_VER)
#       define VX_ALIGN(x) __declspec(align(x))
#   elif defined(__GNUC__)
#       define VX_ALIGN(x) __attribute__((aligned(x)))
#   endif
#endif

#ifndef VX_SECTION
#   if defined(_MSC_VER)
#       define VX_SECTION(x) __declspec(code_seg(x))
#   elif defined(__GNUC__)
#       define VX_SECTION(x) __attribute__(__section__(x)))
#   endif
#endif

#endif // VXMATHCOMPILER_H
