/* SPDX-License-Identifier: MIT */
#ifndef STORAGE_IO_FLIGHT_RECORDER_CSDK_H
#define STORAGE_IO_FLIGHT_RECORDER_CSDK_H
#include <stdint.h>
#if !defined(_WIN32) || !defined(_WIN64)
#error This SDK requires Windows x64.
#endif
#ifdef __cplusplus
#define STORAGE_IO_FLIGHT_RECORDER_CSDK_NOEXCEPT noexcept
extern "C" {
#else
#define STORAGE_IO_FLIGHT_RECORDER_CSDK_NOEXCEPT
#endif
#if defined(STORAGE_IO_FLIGHT_RECORDER_CSDK_BUILD)
#define STORAGE_IO_FLIGHT_RECORDER_CSDK_API __declspec(dllexport)
#else
#define STORAGE_IO_FLIGHT_RECORDER_CSDK_API __declspec(dllimport)
#endif
/* open is inert. One active session per loaded product DLL.
   call accepts length-delimited UTF-8 JSON and returns an allocated UTF-8
   response, or NULL on allocation failure. Always free with this same DLL.
   Session/operation handles are scoped to the client that returned them. */
STORAGE_IO_FLIGHT_RECORDER_CSDK_API uint32_t storage_io_flight_recorder_json_open(uint64_t *session) STORAGE_IO_FLIGHT_RECORDER_CSDK_NOEXCEPT;
STORAGE_IO_FLIGHT_RECORDER_CSDK_API uint32_t storage_io_flight_recorder_json_close(uint64_t session) STORAGE_IO_FLIGHT_RECORDER_CSDK_NOEXCEPT;
STORAGE_IO_FLIGHT_RECORDER_CSDK_API char *storage_io_flight_recorder_json_call(uint64_t session,const char *request,uint32_t length) STORAGE_IO_FLIGHT_RECORDER_CSDK_NOEXCEPT;
STORAGE_IO_FLIGHT_RECORDER_CSDK_API void storage_io_flight_recorder_json_free(char *response) STORAGE_IO_FLIGHT_RECORDER_CSDK_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef STORAGE_IO_FLIGHT_RECORDER_CSDK_NOEXCEPT
#endif
