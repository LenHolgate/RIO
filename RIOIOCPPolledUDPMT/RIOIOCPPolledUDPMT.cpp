#include "stdafx.h"

#define THREAD_STATS_SHOW_DEQUE
#define THREAD_STATS_SHOW_NOTIFY
#define THREAD_STATS_SHOW_MIN_MAX

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   if (argc > 2)
   {
      cout << "Usage: RIOIOCPPolledUDPMT [workLoad]" << endl;
   }

   if (argc > 1)
   {
      g_workIterations = _ttol(argv[1]);
   }

   SetupTiming("RIO IOCP Polled MT UDP");

   InitialiseWinsock();

   CreateRIOSocket();

   HANDLE hIOCP = CreateIOCP();

   OVERLAPPED overlapped;

   RIO_NOTIFICATION_COMPLETION completionType;

   completionType.Type = RIO_IOCP_COMPLETION;
   completionType.Iocp.IocpHandle = hIOCP;
   completionType.Iocp.CompletionKey = (void*)1;
   completionType.Iocp.Overlapped = &overlapped;

   g_queue = g_rio.RIOCreateCompletionQueue(RIO_PENDING_RECVS, &completionType);

   if (g_queue == RIO_INVALID_CQ)
   {
      ErrorExit("RIOCreateCompletionQueue");
   }

   ULONG maxOutstandingReceive = RIO_PENDING_RECVS;
   ULONG maxReceiveDataBuffers = 1;
   ULONG maxOutstandingSend = 0;
   ULONG maxSendDataBuffers = 1;

   void *pContext = 0;

   g_requestQueue = g_rio.RIOCreateRequestQueue(g_s, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, g_queue, g_queue, pContext);

   if (g_requestQueue == RIO_INVALID_RQ)
   {
      ErrorExit("RIOCreateRequestQueue");
   }

   PostRIORecvs(RECV_BUFFER_SIZE, RIO_PENDING_RECVS);

   CreateIOCPThreads(NUM_IOCP_THREADS);

   INT notifyResult = g_rio.RIONotify(g_queue);

   if (notifyResult != ERROR_SUCCESS)
   {
      ErrorExit("RIONotify", notifyResult);
   }

   WaitForProcessingStarted();

   WaitForProcessingStopped();

   StopIOCPThreads();

   PrintTimings();

   return 0;
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

   OVERLAPPED *pOverlapped = 0;

   DWORD recvFlags = 0;

   if (!::GetQueuedCompletionStatus(g_hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
   {
      ErrorExit("GetQueuedCompletionStatus");
   }

   int workValue = 0;

   if (completionKey == 1)
   {
      RIORESULT results[RIO_MAX_RESULTS];

      bool done = false;

      ::SetEvent(g_hStartedEvent);

      ULONG numResults = g_rio.RIODequeueCompletion(g_queue, results, RIO_MAX_RESULTS);

      if (0 == numResults || RIO_CORRUPT_CQ == numResults)
      {
         ErrorExit("RIODequeueCompletion");
      }

      bool waitForCompletion = (numResults >= RIO_RESULTS_THRESHOLD);

      if (waitForCompletion)
      {
#ifdef TRACK_THREAD_STATS
         threadData.notifyCalled++;
#endif

         INT notifyResult = g_rio.RIONotify(g_queue);

         if (notifyResult != ERROR_SUCCESS)
         {
            ErrorExit("RIONotify", notifyResult);
         }
      }

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

         DWORD packets = 0;

         for (DWORD i = 0; i < numResults; ++i)
         {
            EXTENDED_RIO_BUF *pBuffer = reinterpret_cast<EXTENDED_RIO_BUF *>(results[i].RequestContext);

            if (results[i].BytesTransferred == EXPECTED_DATA_SIZE)
            {
               workValue += DoWork(g_workIterations);

               packets++;

               ::EnterCriticalSection(&g_criticalSection);

               if (!g_rio.RIOReceive(g_requestQueue, pBuffer, 1, recvFlags, pBuffer))
               {
                  ErrorExit("RIOReceive");
               }

               ::LeaveCriticalSection(&g_criticalSection);

               done = false;
            }
            else
            {
               done = true;
            }
         }

         ::InterlockedAdd(&g_packets, packets);

         if (!done)
         {
            if (waitForCompletion)
            {
               if (!::GetQueuedCompletionStatus(g_hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
               {
                  ErrorExit("GetQueuedCompletionStatus");
               }

               if (completionKey == 0)
               {
                  done = true;
               }
            }

            if (!done)
            {
               // could spin here on 0, with a spin count

               numResults = g_rio.RIODequeueCompletion(g_queue, results, RIO_MAX_RESULTS);

               if (RIO_CORRUPT_CQ == numResults)
               {
                  ErrorExit("RIODequeueCompletion");
               }

               if (numResults == 0)
               {
                  if (!waitForCompletion)
                  {
#ifdef TRACK_THREAD_STATS
                     threadData.notifyCalled++;
#endif

                     INT notifyResult = g_rio.RIONotify(g_queue);

                     if (notifyResult != ERROR_SUCCESS)
                     {
                        ErrorExit("RIONotify", notifyResult);
                     }
                  }
                  else
                  {
                     ErrorExit("RIODequeueCompletion");
                  }

                  if (!::GetQueuedCompletionStatus(g_hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
                  {
                     ErrorExit("GetQueuedCompletionStatus");
                  }

                  if (completionKey == 0)
                  {
                     done = true;
                  }
                  else
                  {
                     numResults = g_rio.RIODequeueCompletion(g_queue, results, RIO_MAX_RESULTS);

                     if (0 == numResults || RIO_CORRUPT_CQ == numResults)
                     {
                        ErrorExit("RIODequeueCompletion");
                     }
                  }
               }

               waitForCompletion = (numResults >= RIO_RESULTS_THRESHOLD);

               if (waitForCompletion)
               {
#ifdef TRACK_THREAD_STATS
                  threadData.notifyCalled++;
#endif

                  INT notifyResult = g_rio.RIONotify(g_queue);

                  if (notifyResult != ERROR_SUCCESS)
                  {
                     ErrorExit("RIONotify", notifyResult);
                  }
               }
            }
         }
      }
      while (!done);
   }

   ::SetEvent(g_hStoppedEvent);

   return workValue;
}

