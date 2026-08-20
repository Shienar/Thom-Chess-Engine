#ifndef COMPATIBILITY
#define COMPATIBILITY

#include <inttypes.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>

    #define _FILE_OFFSET_BITS 64
    #define _LARGEFILE64_SOURCE 1
    #include <io.h>

    #define resizeFile(file, offset) _chsize_s(_fileno(file), offset)

    #define _strtok(x, y, z) strtok_s(x, y, z)

    #define fseek_64 _fseeki64
    #define ftell_64 _ftelli64

    #define _min min
    #define _max max

    #define THREADTYPE HANDLE
    #define THREAD_INIT NULL
    
    #define GETPID GetCurrentProcessId

    #define THREAD_RETURN DWORD WINAPI
    #define THREAD_PARAM LPVOID

    #define THREAD_WAIT(thread) WaitForSingleObject(thread, INFINITE); \
                                CloseHandle(thread)
    #define THREAD_START(thread, func, arg) thread = CreateThread(NULL, 0, func, arg, 0, NULL)

    #define mutex_t CRITICAL_SECTION 
    #define CREATE_MUTEX(mutex) InitializeCriticalSection(&mutex)
    #define LOCK_MUTEX(mutex) EnterCriticalSection(&mutex)
    #define UNLOCK_MUTEX(mutex) LeaveCriticalSection(&mutex)
    #define DESTROY_MUTEX(mutex) DeleteCriticalSection(&mutex)

    typedef struct {
        HANDLE file_handle;
        HANDLE mapping_handle;
        void* data;
        size_t size;
    } mmap_handle_t;
    
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/param.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <string.h>

    #define resizeFile(file, offset) ftruncate(fileno(file), offset)

    #define _strtok(x, y, z) strtok_r(x, y, z)

    #define fseek_64 fseek
    #define ftell_64 ftell
    
    #define _min MIN
    #define _max MAX

    #define THREADTYPE pthread_t
    #define THREAD_INIT 0

    #define GETPID getpid

    #define THREAD_RETURN void*
    #define THREAD_PARAM void*
    
    #define THREAD_START(thread, func, arg) pthread_create(&thread, NULL, func, (void*)arg)
    #define THREAD_WAIT(thread) pthread_join(thread, NULL)

    #define mutex_t pthread_mutex_t
    #define CREATE_MUTEX(mutex) pthread_mutex_init(&mutex, NULL)
    #define LOCK_MUTEX(mutex) pthread_mutex_lock(&mutex)
    #define UNLOCK_MUTEX(mutex) pthread_mutex_unlock(&mutex)
    #define DESTROY_MUTEX(mutex) pthread_mutex_destroy(&mutex)

    typedef struct {
        int fd;
        void* data;
        size_t size;
    } mmap_handle_t;
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define littleEndian16(x) (x)
    #define littleEndian32(x) (x)
    #define littleEndian64(x) (x)

    #define bigEndian16(x) __builtin_bswap16(x)
    #define bigEndian32(x) __builtin_bswap32(x)
    #define bigEndian64(x) __builtin_bswap64(x)
#else
    #define littleEndian16(x) __builtin_bswap16(x)
    #define littleEndian32(x) __builtin_bswap32(x)
    #define littleEndian64(x) __builtin_bswap64(x)

    #define bigEndian16(x) (x)
    #define bigEndian32(x) (x)
    #define bigEndian64(x) (x)
#endif

#endif