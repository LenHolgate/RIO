#include "stdafx.h"

#include "..\Shared\Constants.h"
#include "..\Shared\Shared.h"

int main(int argc, char* argv[])
{
   if (argc < 2 || argc > 4)
   {
      cout << "Error: no remote address specified" << endl;
      cout << "Usage: SimpleUDPTrafficGeneraor <RemoteIP> [datagrams to send] [shutdown datagrams to send]" << endl;
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

   SetupTiming("Simple UDP Traffic generator", false);

   cout << "Send to: " << pAddress << " - " << PORT << endl;
   cout << "Datagrams to send: " << datagramsToSend << endl;
   cout << "Shutdown datagrams: " << shutdownDatagramsToSend << endl;


   InitialiseWinsock();

   SOCKET s = CreateSocket();

   SetSocketSendBufferToMaximum(s);

   sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(PORT);
   addr.sin_addr.s_addr = inet_addr(pAddress);

   bool done = false;

   CHAR buffer[SEND_BUFFER_SIZE];

   WSABUF buf;

   buf.buf = buffer;
   buf.len = SEND_BUFFER_SIZE;

   DWORD bytesSent = 0;

   DWORD flags = 0;

   StartTiming();

   for (long i = 0; i < datagramsToSend; ++i)
   {
      if (SOCKET_ERROR == ::WSASendTo(
         s,
         &buf,
         1,
         &bytesSent,
         flags,
         reinterpret_cast<sockaddr *>(&addr),
         sizeof(addr),
         0,
         0))
      {
         ErrorExit("WSASend");
      }
   }

   StopTiming();

   buf.len = SEND_BUFFER_SIZE / 2;

   for (long i = 0; i < shutdownDatagramsToSend; ++i)
   {
      if (SOCKET_ERROR == ::WSASendTo(
         s,
         &buf,
         1,
         &bytesSent,
         flags,
         reinterpret_cast<sockaddr *>(&addr),
         sizeof(addr), 0, 0))
      {
         ErrorExit("WSASend");
      }
      Sleep(100);
   }

   g_packets = datagramsToSend;

   PrintTimings("Sent ");

   return 0;
}

