#ifndef COMPATIBILITY
#define COMPATIBILITY

#include <inttypes.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>

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
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/param.h>

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
#endif

#endif