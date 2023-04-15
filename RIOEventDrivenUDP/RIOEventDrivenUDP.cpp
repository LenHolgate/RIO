#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   if (argc > 2)
   {
      cout << "Usage: RIOEventDrivenUDP [workLoad]" << endl;
   }

   if (argc > 1)
   {
      g_workIterations = _ttol(argv[1]);
   }

   SetupTiming("RIO Event Driven UDP");

   InitialiseWinsock();

   CreateRIOSocket();

   HANDLE hEvent = WSACreateEvent();

   if (hEvent == WSA_INVALID_EVENT)
   {
      ErrorExit("WSACreateEvent");
   }

   RIO_NOTIFICATION_COMPLETION completionType;

   completionType.Type = RIO_EVENT_COMPLETION;
   completionType.Event.EventHandle = hEvent;
#ifdef WE_RESET_EVENT
   completionType.Event.NotifyReset = FALSE;
#else
   completionType.Event.NotifyReset = TRUE;
#endif

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

   g_requestQueue = g_rio.RIOCreateRequestQueue(
      g_s,
      maxOutstandingReceive,
      maxReceiveDataBuffers,
      maxOutstandingSend,
      maxSendDataBuffers,
      g_queue,
      g_queue,
      pContext);

   if (g_requestQueue == RIO_INVALID_RQ)
   {
      ErrorExit("RIOCreateRequestQueue");
   }

   PostRIORecvs(RECV_BUFFER_SIZE, RIO_PENDING_RECVS);

#ifdef TRACK_THREAD_STATS
   ThreadData &threadData = g_threadData[0];

   threadData.threadId = ::GetCurrentThreadId();
#endif

   bool done = false;

   DWORD recvFlags = 0;

   RIORESULT results[RIO_MAX_RESULTS];

   const INT notifyResult = g_rio.RIONotify(g_queue);

   if (notifyResult != ERROR_SUCCESS)
   {
      ErrorExit("RIONotify");
   }
   
   const DWORD waitResult = WaitForSingleObject(hEvent, INFINITE);

   if (waitResult != WAIT_OBJECT_0)
   {
      ErrorExit("WaitForSingleObject");
   }

   StartTiming();

   ULONG numResults = g_rio.RIODequeueCompletion(g_queue, results, RIO_MAX_RESULTS);

   if (0 == numResults || RIO_CORRUPT_CQ == numResults)
   {
      ErrorExit("RIODequeueCompletion");
   }

   int workValue = 0;

   bool running = true;

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

      for (DWORD i = 0; i < numResults; ++i)
      {
         EXTENDED_RIO_BUF *pBuffer = reinterpret_cast<EXTENDED_RIO_BUF *>(results[i].RequestContext);

         if (results[i].BytesTransferred == EXPECTED_DATA_SIZE)
         {
            g_packets++;

            workValue += DoWork(g_workIterations);

            if (!g_rio.RIOReceive(
               g_requestQueue,
               pBuffer,
               1,
               recvFlags,
               pBuffer))
            {
               ErrorExit("RIOReceive");
            }

            done = false;
         }
         else
         {
            done = true;
         }
      }

      if (!done)
      {
#ifdef WE_RESET_EVENT
         if (!::ResetEvent(hEvent))
         {
            ErrorExit("ResetEvent");
         }
#endif

#ifdef TRACK_THREAD_STATS
         threadData.notifyCalled++;
#endif

         const INT notifyResult = g_rio.RIONotify(g_queue);

         if (notifyResult != ERROR_SUCCESS)
         {
            ErrorExit("RIONotify");
         }

         const DWORD waitResult = WaitForSingleObject(hEvent, INFINITE);

         if (waitResult != WAIT_OBJECT_0)
         {
            ErrorExit("WaitForSingleObject");
         }

         numResults = g_rio.RIODequeueCompletion(g_queue, results, RIO_MAX_RESULTS);

         if (0 == numResults || RIO_CORRUPT_CQ == numResults)
         {
            ErrorExit("RIODequeueCompletion");
         }
      }
   }
   while (!done);

   StopTiming();

   PrintTimings();

   DisplayThreadStats();

   return workValue;
}

