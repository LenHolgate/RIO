// RIOTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   if (argc > 2)
   {
      cout << "Usage: IOCPUDP [workLoad]" << endl;
   }

   if (argc > 1)
   {
      g_workIterations = _ttol(argv[1]);
   }

   SetupTiming("IOCP UDP");

   InitialiseWinsock();

   SOCKET s = CreateSocket(WSA_FLAG_OVERLAPPED);

   HANDLE hIOCP = CreateIOCP();

   if (0 == ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(s), hIOCP, 0, 0))
   {
      ErrorExit("CreateIoCompletionPort");
   }

   Bind(s, PORT);

   PostIOCPRecvs(RECV_BUFFER_SIZE, IOCP_PENDING_RECVS);

   bool done = false;

   DWORD numberOfBytes = 0;

   ULONG_PTR completionKey = 0;

   OVERLAPPED *pOverlapped = 0;

   if (!::GetQueuedCompletionStatus(hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
   {
      ErrorExit("GetQueuedCompletionStatus");
   }

   StartTiming();

   DWORD bytesRecvd = 0;
   DWORD flags = 0;

   do
   {
      if (numberOfBytes == EXPECTED_DATA_SIZE)
      {
         g_packets++;

         EXTENDED_OVERLAPPED *pExtOverlapped = static_cast<EXTENDED_OVERLAPPED *>(pOverlapped);

         if (SOCKET_ERROR == ::WSARecv(g_s, &(pExtOverlapped->buf), 1, &bytesRecvd, &flags, pExtOverlapped, 0))
         {
            const DWORD lastError = ::GetLastError();

            if (lastError != ERROR_IO_PENDING)
            {
               ErrorExit("WSARecv", lastError);
            }
         }
      }
      else
      {
         StopTiming();

         done = true;
      }

      if (!done)
      {
         if (!::GetQueuedCompletionStatus(hIOCP, &numberOfBytes, &completionKey, &pOverlapped, INFINITE))
         {
            ErrorExit("GetQueuedCompletionStatus");
         }
      }
   }
   while (!done);

   PrintTimings();

   return 0;
}

