#pragma comment(lib, "ws2_32.lib")

#include <WS2tcpip.h>

#include <iostream>

#include <deque>
#include <string>
#include <process.h>
#include <algorithm>

using std::cout;
using std::endl;
using std::deque;
using std::string;

RIO_EXTENSION_FUNCTION_TABLE g_rio;
RIO_CQ g_queue = 0;
RIO_RQ g_requestQueue = 0;

HANDLE g_hIOCP = 0;

HANDLE g_hStartedEvent = 0;
HANDLE g_hStoppedEvent = 0;

HANDLE g_hReadsToProcessEvent = 0;
HANDLE g_hShutdownReaderThreadEvent = 0;

SOCKET g_s;

volatile long g_packets = 0;

LARGE_INTEGER g_frequency;
LARGE_INTEGER g_startCounter;
LARGE_INTEGER g_stopCounter;

typedef std::deque<HANDLE> Threads;

HANDLE g_hReaderThread = 0;

Threads g_threads;

long g_workIterations = 0;

volatile long g_pendingRecvs = 0;

typedef std::deque<RIO_BUFFERID> BufferList;

BufferList g_buffers;

CRITICAL_SECTION g_criticalSection;

struct ThreadData
{
   ThreadData()
      : threadId(0),
        packetsProcessed(0),
        minPacketsProcessed(std::numeric_limits<size_t>::max()),
        maxPacketsProcessed(0),
        notifyCalled(0),
        dequeueCalled(0)
   {
   }

   DWORD threadId;

   size_t packetsProcessed;
   size_t minPacketsProcessed;
   size_t maxPacketsProcessed;
   size_t notifyCalled;
   size_t dequeueCalled;
};

ThreadData g_threadData[NUM_IOCP_THREADS];

struct EXTENDED_RIO_BUF : public RIO_BUF
{
   DWORD operation;

   EXTENDED_RIO_BUF *pNext;
};

volatile PVOID g_pReadList = 0;

struct EXTENDED_OVERLAPPED : public OVERLAPPED
{
   WSABUF buf;
};

unsigned int __stdcall ThreadFunction(
   void *pV);

unsigned int __stdcall ReaderThreadFunction(
   void *pV);

inline string GetLastErrorMessage(
   DWORD last_error,
   bool stripTrailingLineFeed = true)
{
   CHAR errmsg[512];

   if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      0,
      last_error,
      0,
      errmsg,
      511,
      NULL))
   {
      // if we fail, call ourself to find out why and return that error

      const DWORD thisError = ::GetLastError();

      if (thisError != last_error)
      {
         return GetLastErrorMessage(thisError, stripTrailingLineFeed);
      }
      else
      {
         // But don't get into an infinite loop...

         return "Failed to obtain error string";
      }
   }

   if (stripTrailingLineFeed)
   {
      const size_t length = strlen(errmsg);

      if (errmsg[length-1] == '\n')
      {
         errmsg[length-1] = 0;

         if (errmsg[length-2] == '\r')
         {
            errmsg[length-2] = 0;
         }
      }
   }

   return errmsg;
}

inline void ErrorExit(
   const char *pFunction,
   const DWORD lastError)
{
   cout << "Error: " << pFunction << " failed: " << lastError << endl;
   cout << GetLastErrorMessage(lastError) << endl;

   exit(0);
}

inline void ErrorExit(
   const char *pFunction)
{
   const DWORD lastError = ::GetLastError();

   ErrorExit(pFunction, lastError);
}

template <typename TV, typename TM>
inline TV RoundDown(TV Value, TM Multiple)
{
   return((Value / Multiple) * Multiple);
}

template <typename TV, typename TM>
inline TV RoundUp(TV Value, TM Multiple)
{
   return(RoundDown(Value, Multiple) + (((Value % Multiple) > 0) ? Multiple : 0));
}

inline void SetupTiming(
   const char *pProgramName,
   const bool lockToThreadForTiming = true)
{
   cout << pProgramName << endl;
   cout << "Work load: " << g_workIterations << endl;
   cout << "Max results: " << RIO_MAX_RESULTS << endl;

   if (lockToThreadForTiming)
   {
	   HANDLE hThread = ::GetCurrentThread();

	   if (0 == ::SetThreadAffinityMask(hThread, TIMING_THREAD_AFFINITY_MASK))
	   {
		  ErrorExit("SetThreadAffinityMask");
	   }
   }

   //if (!::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
   //{
   //   ErrorExit("SetThreadPriority");
   //}

   if (!::QueryPerformanceFrequency(&g_frequency))
   {
      ErrorExit("QueryPerformanceFrequency");
   }
}

inline void InitialiseWinsock()
{
   WSADATA data;

   WORD wVersionRequested = 0x202;

   if (0 != ::WSAStartup(wVersionRequested, &data))
   {
      ErrorExit("WSAStartup");
   }

   if (USE_LARGE_PAGES)
   {
      // check that we have SeLockMemoryPrivilege and enable it

      ErrorExit("TODO - USE_LARGE_PAGES");
   }
}

inline SOCKET CreateSocket(
   const DWORD flags = 0)
{
   g_s = ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, flags);

   if (g_s == INVALID_SOCKET)
   {
      ErrorExit("WSASocket");
   }

   return g_s;
}

inline HANDLE CreateIOCP()
{
   g_hIOCP = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);

   if (0 == g_hIOCP)
   {
      ErrorExit("CreateIoCompletionPort");
   }

   return g_hIOCP;
}

inline void InitialiseRIO(
   SOCKET s)
{
   GUID functionTableId = WSAID_MULTIPLE_RIO;

   DWORD dwBytes = 0;

   bool ok = true;

   if (0 != WSAIoctl(
      s,
      SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
      &functionTableId,
      sizeof(GUID),
      (void**)&g_rio,
      sizeof(g_rio),
      &dwBytes,
      0,
      0))
   {
      ErrorExit("WSAIoctl");
   }
}

inline void Bind(
   SOCKET s,
   const unsigned short port)
{
   sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(port);
   addr.sin_addr.s_addr = INADDR_ANY;

   if (SOCKET_ERROR == ::bind(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)))
   {
      ErrorExit("bind");
   }
}

inline void CreateRIOSocket(
   const bool bind = true)
{
   g_s = CreateSocket(WSA_FLAG_REGISTERED_IO);

   if (bind)
   {
      Bind(g_s, PORT);
   }

   InitialiseRIO(g_s);
}

inline void SetSocketSendBufferToMaximum(
   SOCKET s)
{
   int soRecvBufSize = 0;

   int optLen = sizeof(soRecvBufSize);

   if (SOCKET_ERROR == ::getsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char *>(&soRecvBufSize), &optLen))
   {
      ErrorExit("setsockopt");
   }

   cout << "Send size = " << soRecvBufSize << endl;

   soRecvBufSize = std::numeric_limits<int>::max();

   cout << "Try to set Send buf to " << soRecvBufSize << endl;

   if (SOCKET_ERROR == ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char *>(&soRecvBufSize), sizeof(soRecvBufSize)))
   {
      ErrorExit("setsockopt");
   }

   if (SOCKET_ERROR == ::getsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char *>(&soRecvBufSize), &optLen))
   {
      ErrorExit("setsockopt");
   }

   cout << "Send buf actually set to " << soRecvBufSize << endl;
}

inline void SetSocketRecvBufferToMaximum(
   SOCKET s)
{
   int soRecvBufSize = 0;

   int optLen = sizeof(soRecvBufSize);

   if (SOCKET_ERROR == ::getsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&soRecvBufSize), &optLen))
   {
      ErrorExit("setsockopt");
   }

   cout << "Recv size = " << soRecvBufSize << endl;

   soRecvBufSize = std::numeric_limits<int>::max();

//   0x3FFFFFFF - possible max?;

   cout << "Try to set recv buf to " << soRecvBufSize << endl;

   if (SOCKET_ERROR == ::setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&soRecvBufSize), sizeof(soRecvBufSize)))
   {
      ErrorExit("setsockopt");
   }

   if (SOCKET_ERROR == ::getsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&soRecvBufSize), &optLen))
   {
      ErrorExit("setsockopt");
   }

   cout << "Recv buf actually set to " << soRecvBufSize << endl;
}

inline char *AllocateBufferSpace(
   const DWORD recvBufferSize,
   const DWORD pendingRecvs,
   DWORD &bufferSize,
   DWORD &receiveBuffersAllocated)
{
   const DWORD preferredNumaNode = 0;

   const SIZE_T largePageMinimum = USE_LARGE_PAGES ? ::GetLargePageMinimum() : 0;

   SYSTEM_INFO systemInfo;

   ::GetSystemInfo(&systemInfo);

   systemInfo.dwAllocationGranularity;
   
   const unsigned __int64 granularity = (largePageMinimum == 0 ? systemInfo.dwAllocationGranularity : largePageMinimum);

   const unsigned __int64 desiredSize = recvBufferSize * pendingRecvs;

   unsigned __int64 actualSize = RoundUp(desiredSize, granularity);

   if (actualSize > std::numeric_limits<DWORD>::max())
   {
      actualSize = (std::numeric_limits<DWORD>::max() / granularity) * granularity;
   }

   receiveBuffersAllocated = std::min<DWORD>(pendingRecvs, static_cast<DWORD>(actualSize / recvBufferSize));

   bufferSize = static_cast<DWORD>(actualSize);

   char *pBuffer = reinterpret_cast<char *>(VirtualAllocExNuma(GetCurrentProcess(), 0, bufferSize, MEM_COMMIT | MEM_RESERVE  | (largePageMinimum != 0 ? MEM_LARGE_PAGES : 0), PAGE_READWRITE, preferredNumaNode));

   if (pBuffer == 0)
   {
      ErrorExit("VirtualAlloc");
   }

   return pBuffer;
}

inline char *AllocateBufferSpace(
   const DWORD recvBufferSize,
   const DWORD pendingRecvs,
   DWORD &receiveBuffersAllocated)
{
   DWORD notUsed;

   return AllocateBufferSpace(recvBufferSize, pendingRecvs, notUsed, receiveBuffersAllocated);
}

inline void PostIOCPRecvs(
   const DWORD recvBufferSize,
   const DWORD pendingRecvs)
{
   DWORD totalBuffersAllocated = 0;

   while (totalBuffersAllocated < pendingRecvs)
   {
      DWORD receiveBuffersAllocated = 0;

      char *pBuffer = AllocateBufferSpace(recvBufferSize, pendingRecvs, receiveBuffersAllocated);

      totalBuffersAllocated += receiveBuffersAllocated;

      DWORD offset = 0;

      const DWORD recvFlags = 0;

      EXTENDED_OVERLAPPED *pBufs = new EXTENDED_OVERLAPPED[receiveBuffersAllocated];

      DWORD bytesRecvd = 0;
      DWORD flags = 0;

      for (DWORD i = 0; i < receiveBuffersAllocated; ++i)
      {
         EXTENDED_OVERLAPPED *pOverlapped = pBufs + i;

         ZeroMemory(pOverlapped, sizeof(EXTENDED_OVERLAPPED));

         pOverlapped->buf.buf = pBuffer + offset;
         pOverlapped->buf.len = recvBufferSize;

         offset += recvBufferSize;

         if (SOCKET_ERROR == ::WSARecv(g_s, &(pOverlapped->buf), 1, &bytesRecvd, &flags, static_cast<OVERLAPPED *>(pOverlapped), 0))
         {
            const DWORD lastError = ::GetLastError();

            if (lastError != ERROR_IO_PENDING)
            {
               ErrorExit("WSARecv", lastError);
            }
         }
      }

      if (totalBuffersAllocated != pendingRecvs)
      {
         cout << pendingRecvs << " receives pending" << endl;
      }
   }

   cout << totalBuffersAllocated << " total receives pending" << endl;
}

inline void PostRIORecvs(
   const DWORD recvBufferSize,
   const DWORD pendingRecvs)
{
   DWORD totalBuffersAllocated = 0;

   while (totalBuffersAllocated < pendingRecvs)
   {
      DWORD bufferSize = 0;
   
      DWORD receiveBuffersAllocated = 0;

      char *pBuffer = AllocateBufferSpace(recvBufferSize, pendingRecvs, bufferSize, receiveBuffersAllocated);

      totalBuffersAllocated += receiveBuffersAllocated;

      RIO_BUFFERID id = g_rio.RIORegisterBuffer(pBuffer, static_cast<DWORD>(bufferSize));

      g_buffers.push_back(id);

      if (id == RIO_INVALID_BUFFERID)
      {
         ErrorExit("RIORegisterBuffer");
      }

      DWORD offset = 0;

      DWORD recvFlags = 0;

      EXTENDED_RIO_BUF *pBufs = new EXTENDED_RIO_BUF[receiveBuffersAllocated];

      for (DWORD i = 0; i < receiveBuffersAllocated; ++i)
      {
         // now split into buffer slices and post our recvs

         EXTENDED_RIO_BUF *pBuffer = pBufs + i;

         pBuffer->operation = 0;
         pBuffer->BufferId = id;
         pBuffer->Offset = offset;
         pBuffer->Length = recvBufferSize;

         offset += recvBufferSize;

         g_pendingRecvs++;

         if (!g_rio.RIOReceive(g_requestQueue, pBuffer, 1, recvFlags, pBuffer))
         {
            ErrorExit("RIOReceive");
         }
      }

      if (totalBuffersAllocated != pendingRecvs)
      {
         cout << pendingRecvs << " receives pending" << endl;
      }
   }

   cout << totalBuffersAllocated << " total receives pending" << endl;
}

inline void CreateIOCPThreads(
   const DWORD numThreads)
{
   ::InitializeCriticalSectionAndSpinCount(&g_criticalSection, SPIN_COUNT);

   g_hStartedEvent = ::CreateEvent(0, TRUE, FALSE, 0);

   if (0 == g_hStartedEvent)
   {
      ErrorExit("CreateEvent");
   }

   g_hStoppedEvent = ::CreateEvent(0, TRUE, FALSE, 0);

   if (0 == g_hStoppedEvent)
   {
      ErrorExit("CreateEvent");
   }

   // Start our worker threads

   for (DWORD i = 0; i < numThreads; ++i)
   {
      unsigned int notUsed;

      const uintptr_t result = ::_beginthreadex(0, 0, ThreadFunction, (void*)i, 0, &notUsed);

      if (result == 0)
      {
         ErrorExit("_beginthreadex", errno);
      }

      g_threads.push_back(reinterpret_cast<HANDLE>(result));
   }

   cout << numThreads << " threads running" << endl;
}

inline void CreateReaderThread()
{
   g_hReadsToProcessEvent = ::CreateEvent(0, FALSE, FALSE, 0);

   if (0 == g_hReadsToProcessEvent)
   {
      ErrorExit("CreateEvent");
   }

   g_hShutdownReaderThreadEvent = ::CreateEvent(0, TRUE, FALSE, 0);

   if (0 == g_hShutdownReaderThreadEvent)
   {
      ErrorExit("CreateEvent");
   }

   unsigned int notUsed;

   const uintptr_t result = ::_beginthreadex(0, 0, ReaderThreadFunction, 0, 0, &notUsed);

   if (result == 0)
   {
      ErrorExit("_beginthreadex", errno);
   }

   g_hReaderThread = (HANDLE)result;
}

inline void StartTiming()
{
   if (!::QueryPerformanceCounter(&g_startCounter))
   {
      ErrorExit("QueryPerformanceCounter");
   }

   cout << "Timing started" << endl;
}

inline void StopTiming()
{
   if (!::QueryPerformanceCounter(&g_stopCounter))
   {
      ErrorExit("QueryPerformanceCounter");
   }

   cout << "Timing stopped" << endl;
}

inline void WaitForProcessingStarted()
{
   if (WAIT_OBJECT_0 != ::WaitForSingleObject(g_hStartedEvent, INFINITE))
   {
      ErrorExit("WaitForSingleObject");
   }

   StartTiming();
}

inline void WaitForProcessingStopped()
{
   if (WAIT_OBJECT_0 != ::WaitForSingleObject(g_hStoppedEvent, INFINITE))
   {
      ErrorExit("WaitForSingleObject");
   }

   StopTiming();
}

inline void DisplayThreadStats(
   const size_t index = 0)
{
#ifdef TRACK_THREAD_STATS
   if (g_threadData[index].packetsProcessed != 0)
   {
      cout << "Thread Id: " << g_threadData[index].threadId << endl;

      #ifdef THREAD_STATS_SHOW_DEQUE
	   cout << "  Dequeue: " << g_threadData[index].dequeueCalled << endl;
      #endif

      #ifdef THREAD_STATS_SHOW_NOTIFY
      cout << "   Notify: " << g_threadData[index].notifyCalled << endl;
      #endif

      cout << "  Packets: " << g_threadData[index].packetsProcessed << endl;

      #ifdef THREAD_STATS_SHOW_MIN_MAX
      cout << "      Min: " << (g_threadData[index].packetsProcessed > 0 ? g_threadData[index].minPacketsProcessed : 0);
      cout << " - Max: " << g_threadData[index].maxPacketsProcessed;
      cout << " - Average: " << (g_threadData[index].dequeueCalled == 0 ? 0 : g_threadData[index].packetsProcessed / g_threadData[index].dequeueCalled) << endl;
      #endif

      cout << endl;
   }
#endif
}

inline size_t CountActiveThreads()
{
   size_t activeThreads = 0;

   for (size_t i = 0; i < NUM_IOCP_THREADS; i++)
   {
      if (g_threadData[i].packetsProcessed != 0)
      {
         activeThreads++;
      }
   }

   return activeThreads;
}

inline void StopIOCPThreads()
{
   // Tell all threads to exit

   for (Threads::const_iterator it = g_threads.begin(), end = g_threads.end(); it != end; ++it)
   {
      if (0 == ::PostQueuedCompletionStatus(g_hIOCP, 0, 0, 0))
      {
         ErrorExit("PostQueuedCompletionStatus");
      }
   }

   cout << "Threads stopping" << endl;

   // Wait for all threads to exit

   for (Threads::const_iterator it = g_threads.begin(), end = g_threads.end(); it != end; ++it)
   {
      HANDLE hThread = *it;

      if (WAIT_OBJECT_0 != ::WaitForSingleObject(hThread, INFINITE))
      {
         ErrorExit("WaitForSingleObject");
      }

      ::CloseHandle(hThread);
   }

   cout << "Threads stopped" << endl;

   const size_t activeThreads = CountActiveThreads();

   cout << activeThreads << " threads processed datagrams" << endl;

   for (size_t i = 0; i < NUM_IOCP_THREADS; i++)
   {
      DisplayThreadStats(i);
   }

   ::DeleteCriticalSection(&g_criticalSection);
}

inline void StopReaderThread()
{
   if (!::SetEvent(g_hShutdownReaderThreadEvent))
   {
      ErrorExit("SetEvent");
   }

   if (WAIT_OBJECT_0 != ::WaitForSingleObject(g_hReaderThread, INFINITE))
   {
      ErrorExit("WaitForSingleObject");
   }

   ::CloseHandle(g_hReaderThread);
}

inline void PrintTimings(
   const char *pDirection = "Received ")
{
   LARGE_INTEGER elapsed;

   elapsed.QuadPart = (g_stopCounter.QuadPart - g_startCounter.QuadPart) / (g_frequency.QuadPart / 1000);

   cout << "Complete in " << elapsed.QuadPart << "ms" << endl;
   cout << pDirection << g_packets << " datagrams" << endl;

   if (elapsed.QuadPart != 0)
   {
      const double perSec = g_packets / elapsed.QuadPart * 1000.00;

      cout << perSec << " datagrams per second" << endl;
   }
}

inline void CleanupRIO()
{
   if (SOCKET_ERROR == ::closesocket(g_s))
   {
      const DWORD lastError = ::GetLastError();

      cout << "error closing socket: " << GetLastErrorMessage(lastError);
   }

   g_rio.RIOCloseCompletionQueue(g_queue);

   for (BufferList::const_iterator it = g_buffers.begin(), end = g_buffers.end(); it != end; ++it)
   {
      g_rio.RIODeregisterBuffer(*it);
   }
}

inline int DoWork(
   const size_t iterations)
{
   int result = rand();

   for (size_t i = 0; i < iterations; ++i)
   {
      result += rand();
   }

   return result % 223;
}

