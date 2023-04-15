// RIOTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int main(int argc, char* argv[])
{
   if (argc < 2 || argc > 4)
   {
      cout << "Error: no remote address specified" << endl;
      cout << "Usage: RIOUDPTrafficGeneraor <RemoteIP> [datagrams to send] [shutdown datagrams to send]" << endl;
   }

   long datagramsToSend = DATAGRAMS_TO_SEND;

   if (argc > 2)
   {
      datagramsToSend = atol(argv[2]);
   }

   long shutdownDatagramsToSend = SHUTDOWN_DATAGRAMS_TO_SEND;

   if (argc > 3)
   {
      shutdownDatagramsToSend = atol(argv[3]);
   }

   const char *pAddress = argv[1];

   SetupTiming("RIO UDP Traffic generator", false);

   cout << "Datagrams to send: " << datagramsToSend << endl;
   cout << "Shutdown datagrams: " << shutdownDatagramsToSend << endl;

   InitialiseWinsock();

   CreateRIOSocket(false);

   sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(PORT);
   addr.sin_addr.s_addr = inet_addr(pAddress);

   bool done = false;

   bool shutdown = false;

   const DWORD sendFlags = 0;

   DWORD bufferSize = 0;
   
   DWORD sendBuffersAllocated = 0;

   char *pBuffer = AllocateBufferSpace(SEND_BUFFER_SIZE, datagramsToSend, bufferSize, sendBuffersAllocated);

   RIO_BUFFERID id = g_rio.RIORegisterBuffer(pBuffer, static_cast<DWORD>(bufferSize));

   g_buffers.push_back(id);

   if (id == RIO_INVALID_BUFFERID)
   {
      ErrorExit("RIORegisterBuffer");
   }

   DWORD offset = 0;

   DWORD recvFlags = 0;

   EXTENDED_RIO_BUF *pBufs = new EXTENDED_RIO_BUF[sendBuffersAllocated];

   for (DWORD i = 0; i < sendBuffersAllocated; ++i)
   {
      // now split into buffer slices and post our recvs

      EXTENDED_RIO_BUF *pBuffer = pBufs + i;

      pBuffer->operation = 0;
      pBuffer->BufferId = id;
      pBuffer->Offset = offset;
      pBuffer->Length = SEND_BUFFER_SIZE;

      offset += SEND_BUFFER_SIZE;
   }

   g_queue = g_rio.RIOCreateCompletionQueue(sendBuffersAllocated, 0);

   if (g_queue == RIO_INVALID_CQ)
   {
      ErrorExit("RIOCreateCompletionQueue");
   }

   ULONG maxOutstandingReceive = 0;
   ULONG maxReceiveDataBuffers = 1;
   ULONG maxOutstandingSend = sendBuffersAllocated;
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

   if (SOCKET_ERROR == ::connect(g_s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)))
   {
      ErrorExit("connect");
   }

   StartTiming();

   for (size_t i = 0; i < sendBuffersAllocated; ++i)
   {
      ++g_packets;

      if (!shutdown && g_packets > datagramsToSend)
      {
         if (shutdown)
         {
            StopTiming();

            shutdown = true;
         }

         pBufs[i].Length = SEND_BUFFER_SIZE / 2;
      }

      if (g_packets > datagramsToSend + shutdownDatagramsToSend)
      {
         done = true;
      }

      if (!done)
      {
         if (!g_rio.RIOSend(g_requestQueue, &pBufs[i], 1, sendFlags, &pBufs[i]))
         {
            ErrorExit("RIOSend");
         }
      }
   }

   if (!done)
   {
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

      do
      {
         for (DWORD i = 0; i < numResults; ++i)
         {
            EXTENDED_RIO_BUF *pBuffer = reinterpret_cast<EXTENDED_RIO_BUF *>(results[i].RequestContext);

            ++g_packets;

            if (shutdown || g_packets > datagramsToSend)
            {
               if (!shutdown)
               {
                  StopTiming();

                  shutdown = true;
               }

               pBuffer->Length = SEND_BUFFER_SIZE / 2;
            }

            if (g_packets > datagramsToSend + shutdownDatagramsToSend)
            {
               done = true;
            }

            if (!done)
            {
               if (!g_rio.RIOSend(g_requestQueue, pBuffer, 1, sendFlags, pBuffer))
               {
                  ErrorExit("RIOSend");
               }
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
   }

   PrintTimings("Sent ");

   CleanupRIO();

   return 0;
}

