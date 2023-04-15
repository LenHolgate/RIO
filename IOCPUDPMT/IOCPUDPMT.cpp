#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   if (argc > 2)
   {
      cout << "Usage: IOCPUDPMT [workLoad]" << endl;
   }

   if (argc > 1)
   {
      g_workIterations = _ttol(argv[1]);
   }

   SetupTiming("IOCP UDP MT");

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

   threadData.maxPacketsProcessed = 1;
   threadData.minPacketsProcessed = 1;
#endif

   DWORD numberOfBytes = 0;

   ULONG_PTR completionKey = 0;

   OVERLAPPED *pOverlapped = 0;

   if (!::GetQueuedCompletionStatus(g_hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
   {
      ErrorExit("GetQueuedCompletionStatus");
   }

   int workValue = 0;

   if (completionKey == 1)
   {
      bool done = false;

      ::SetEvent(g_hStartedEvent);

      DWORD bytesRecvd = 0;
      DWORD flags = 0;

      do
      {
#ifdef TRACK_THREAD_STATS
         threadData.dequeueCalled++;

         threadData.packetsProcessed++;
#endif

         if (numberOfBytes == EXPECTED_DATA_SIZE)
         {
            ::InterlockedIncrement(&g_packets);

            workValue += DoWork(g_workIterations);

            EXTENDED_OVERLAPPED *pExtOverlapped = static_cast<EXTENDED_OVERLAPPED *>(pOverlapped);

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

         if (!done)
         {
            if (!::GetQueuedCompletionStatus(g_hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
            {
               const DWORD lastError = ::GetLastError();

               if (lastError != ERROR_OPERATION_ABORTED)
               {
                  ErrorExit("GetQueuedCompletionStatus", lastError);
               }
            }

            if (completionKey == 0)
            {
               done = true;
            }
         }
      }
      while (!done);
   }

   ::SetEvent(g_hStoppedEvent);

   return workValue;
}

