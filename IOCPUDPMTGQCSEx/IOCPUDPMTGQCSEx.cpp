#include "stdafx.h"

#define THREAD_STATS_SHOW_DEQUE
#define THREAD_STATS_SHOW_MIN_MAX

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   if (argc > 2)
   {
      cout << "Usage: IOCPUDPMTGQCSEx [workLoad]" << endl;
   }

   if (argc > 1)
   {
      g_workIterations = _ttol(argv[1]);
   }

   SetupTiming("IOCP UDP MT GQCSEx");

   InitialiseWinsock();

   SOCKET s = CreateSocket(WSA_FLAG_OVERLAPPED);

   HANDLE hIOCP = CreateIOCP();

   if (0 == ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(s), hIOCP, 1, 0))
   {
      ErrorExit("CreateIoCompletionPort");
   }

   Bind(s, PORT);

   PostIOCPRecvs(RECV_BUFFER_SIZE, IOCP_PENDING_RECVS);

   CreateIOCPThreads(NUM_IOCP_THREADS);

   WaitForProcessingStarted();

   WaitForProcessingStopped();

   StopIOCPThreads();

   PrintTimings();
}

unsigned int __stdcall ThreadFunction(
   void *pV)
{
#ifdef TRACK_THREAD_STATS
   const DWORD index = (DWORD)(ULONG_PTR)pV;

   ThreadData &threadData = g_threadData[index];

   threadData.threadId = ::GetCurrentThreadId();
#endif

   DWORD numberOfBytes = 0;

   ULONG_PTR completionKey = 0;

   OVERLAPPED_ENTRY results[GQCSEX_MAX_RESULTS];

   ULONG numResults = 0;

   if (!::GetQueuedCompletionStatusEx(
         g_hIOCP,
         results,
         GQCSEX_MAX_RESULTS,
         &numResults,
         INFINITE,
         FALSE))
   {
      ErrorExit("GetQueuedCompletionStatusEx");
   }

   ::SetEvent(g_hStartedEvent);

   int workValue = 0;

   bool running = true;

   bool done = false;

   do
   {
#ifdef TRACK_THREAD_STATS
      threadData.dequeueCalled++;

      threadData.packetsProcessed += numResults;

      if (numResults > threadData.maxPacketsProcessed)
      {
         threadData.maxPacketsProcessed = numResults;
      }

      if (numResults < threadData.minPacketsProcessed)
      {
         threadData.minPacketsProcessed = numResults;
      }
#endif

      for (DWORD i = 0; !done && i < numResults; ++i)
      {
         if (results[i].lpCompletionKey == 1)
         {
            if (results[i].dwNumberOfBytesTransferred == EXPECTED_DATA_SIZE)
            {
               ::InterlockedIncrement(&g_packets);

               workValue += DoWork(g_workIterations);

               EXTENDED_OVERLAPPED *pExtOverlapped = static_cast<EXTENDED_OVERLAPPED *>(results[i].lpOverlapped);

               DWORD bytesRecvd = 0;

               DWORD flags = 0;

               if (SOCKET_ERROR == ::WSARecv(g_s, &(pExtOverlapped->buf), 1, &bytesRecvd, &flags, pExtOverlapped, 0))
               {
                  const DWORD lastError = ::GetLastError();

                  if (lastError != ERROR_IO_PENDING)
                  {
                     ErrorExit("WSARecv", lastError);
                  }
               }

               done = false;
            }
            else
            {
               done = true;
            }
         }
         else
         {
            done = true;
         }
      }

      if (!done)
      {
         numResults = 0;

         if (!::GetQueuedCompletionStatusEx(
               g_hIOCP,
               results,
               GQCSEX_MAX_RESULTS,
               &numResults,
               INFINITE,
               FALSE))
         {
            ErrorExit("GetQueuedCompletionStatusEx");
         }
      }
   }
   while (!done);

   ::SetEvent(g_hStoppedEvent);

   return workValue;
}

