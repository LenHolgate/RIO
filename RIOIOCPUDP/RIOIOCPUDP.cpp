#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   SetupTiming("RIO IOCP UDP");

   InitialiseWinsock();

   CreateRIOSocket();

   g_hIOCP = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);

   OVERLAPPED overlapped;

   RIO_NOTIFICATION_COMPLETION completionType;

   completionType.Type = RIO_IOCP_COMPLETION;
   completionType.Iocp.IocpHandle = g_hIOCP;
   completionType.Iocp.CompletionKey = (void*)0;
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

   bool done = false;

   RIORESULT results[RIO_MAX_RESULTS];

   INT notifyResult = g_rio.RIONotify(g_queue);

   if (notifyResult != ERROR_SUCCESS)
   {
      ErrorExit("RIONotify", notifyResult);
   }

   DWORD numberOfBytes = 0;

   ULONG_PTR completionKey = 0;

   OVERLAPPED *pOverlapped = 0;

   if (!::GetQueuedCompletionStatus(g_hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
   {
      ErrorExit("GetQueuedCompletionStatus");
   }

   ULONG numResults = g_rio.RIODequeueCompletion(g_queue, results, RIO_MAX_RESULTS);

   if (0 == numResults || RIO_CORRUPT_CQ == numResults)
   {
      ErrorExit("RIODequeueCompletion");
   }

   StartTiming();

   DWORD recvFlags = 0;

   int workValue = 0;

   do
   {
      for (DWORD i = 0; i < numResults; ++i)
      {
         EXTENDED_RIO_BUF *pBuffer = reinterpret_cast<EXTENDED_RIO_BUF *>(results[i].RequestContext);

         if (results[i].BytesTransferred == EXPECTED_DATA_SIZE)
         {
            g_packets++;

            workValue += DoWork(g_workIterations);

            if (!g_rio.RIOReceive(g_requestQueue, pBuffer, 1, recvFlags, pBuffer))
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
         const INT notifyResult = g_rio.RIONotify(g_queue);

         if (notifyResult != ERROR_SUCCESS)
         {
            ErrorExit("RIONotify");
         }

         if (!::GetQueuedCompletionStatus(g_hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
         {
            ErrorExit("GetQueuedCompletionStatus");
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

   return workValue;
}

