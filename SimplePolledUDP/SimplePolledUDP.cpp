#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int _tmain(int argc, _TCHAR* argv[])
{
   if (argc > 2)
   {
      cout << "Usage: SimplePolledUDP [workLoad]" << endl;
   }

   if (argc > 1)
   {
      g_workIterations = _ttol(argv[1]);
   }

   SetupTiming("Simple polled UDP");

   InitialiseWinsock();

   SOCKET s = CreateSocket();

   Bind(s, PORT);

   SetSocketRecvBufferToMaximum(s);

   bool done = false;

   CHAR buffer[RECV_BUFFER_SIZE];

   WSABUF buf;

   buf.buf = buffer;
   buf.len = RECV_BUFFER_SIZE;

   DWORD bytesRecvd = 0;

   DWORD flags = 0;

   if (SOCKET_ERROR == ::WSARecv(
      s,
      &buf,
      1,
      &bytesRecvd,
      &flags,
      0,
      0))
   {
      ErrorExit("WSARecv");
   }

   g_packets++;

   StartTiming();

   int workValue = 0;

   do
   {
      workValue += DoWork(g_workIterations);

      if (SOCKET_ERROR == ::WSARecv(
         s,
         &buf,
         1,
         &bytesRecvd,
         &flags,
         0,
         0))
      {
         ErrorExit("WSARecv");
      }

      if (bytesRecvd == EXPECTED_DATA_SIZE)
      {
         g_packets++;
      }
      else
      {
         done = true;
      }
   }
   while (!done);

   StopTiming();

   PrintTimings();

   return workValue;
}

