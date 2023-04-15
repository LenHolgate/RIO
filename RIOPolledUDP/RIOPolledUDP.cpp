#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   if (argc > 2)
   {
      cout << "Usage: RIOPolledUDP [workLoad]" << endl;
   }

   if (argc > 1)
   {
      g_workIterations = _ttol(argv[1]);
   }

   SetupTiming("RIO polled UDP");

   InitialiseWinsock();

   CreateRIOSocket();

   g_queue = g_rio.RIOCreateCompletionQueue(RIO_PENDING_RECVS, 0);

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

   bool done = false;

   DWORD recvFlags = 0;

   RIORESULT results[RIO_MAX_RESULTS];

   ULONG numResults = 0;

   do
   {
      numResults = g_rio.RIODequeueCompletion(
         g_queue,
         results,
         RIO_MAX_RESULTS);

      if (0 == numResults)
      {
         YieldProcessor();
      }
      else if (RIO_CORRUPT_CQ == numResults)
      {
         ErrorExit("RIODequeueCompletion");
      }
   }
   while (0 == numResults);

   StartTiming();

   int workValue = 0;

   bool running = true;

   do
   {
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
         do
         {
            numResults = g_rio.RIODequeueCompletion(
               g_queue,
               results,
               RIO_MAX_RESULTS);

            if (0 == numResults)
            {
               YieldProcessor();
            }
            else if (RIO_CORRUPT_CQ == numResults)
            {
               ErrorExit("RIODequeueCompletion");
            }
         }
         while (0 == numResults);
      }
   }
   while (!done);

   StopTiming();

   PrintTimings();

   return workValue;
}

