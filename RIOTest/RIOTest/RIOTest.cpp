// RIOTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "MSWinSock.h"

#pragma comment(lib, "ws2_32.lib")

#include <WS2tcpip.h>

using JetByteTools::Socket::CUsesMSWinSockExtensions;

void ErrorExit()
{
   const DWORD lastError = ::GetLastError();

   exit(0);
}

struct MyRIO_BUF : public RIO_BUF
{
   DWORD operation;
};

template <typename TV, typename TM>
inline TV RoundDown(TV Value, TM Multiple)
{
   return((Value / Multiple) * Multiple);
}

template <typename TV, typename TM>
inline TV RoundUp(TV Value, TM Multiple)
{
   return(RoundDown(Value, Multiple) + (((Value % Multiple) > 0) ? Multiple : 0));
}

int _tmain(int argc, _TCHAR* argv[])
{
   WSADATA data;

   WORD wVersionRequested = 0x202;

   if (0 != ::WSAStartup(wVersionRequested, &data))
   {
      ErrorExit();
   }

   SOCKET s = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);

   if (s == INVALID_SOCKET)
   {
      ErrorExit();
   }

   RIO_EXTENSION_FUNCTION_TABLE rio;

   GUID functionTableId = WSAID_MULTIPLE_RIO;

   DWORD dwBytes = 0;

   bool ok = true;

   if (0 != WSAIoctl(
      s,
      SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
      &functionTableId,
      sizeof(GUID),
      (void**)&rio,
      sizeof(rio),
      &dwBytes,
      0,
      0))
   {
      ErrorExit();
   }

   const DWORD queueSize = 10;

   HANDLE hEvent = WSACreateEvent();

   HANDLE hIOCP = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
   OVERLAPPED overlapped;

   ZeroMemory(&overlapped, sizeof(overlapped));

   RIO_NOTIFICATION_COMPLETION completionType;

   completionType.Type = RIO_IOCP_COMPLETION;
   completionType.Iocp.IocpHandle = hIOCP;
   completionType.Iocp.CompletionKey = (void*)1;
   completionType.Iocp.Overlapped = &overlapped;

   RIO_CQ queue = rio.RIOCreateCompletionQueue(queueSize, &completionType);

   if (queue == RIO_INVALID_CQ)
   {
      ErrorExit();
   }

   sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(5050);
   addr.sin_addr.s_addr = INADDR_ANY;

   if (SOCKET_ERROR == ::bind(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)))
   {
      ErrorExit();
   }

   if (SOCKET_ERROR == ::listen(s, 5))
   {
      ErrorExit();
   }

   SOCKET acceptedSocket = ::WSAAccept(
      s,
      0,
      0,
      0,
      0);

   if (SOCKET_ERROR == acceptedSocket)
   {
      ErrorExit();
   }

   ULONG maxOutstandingReceive = 4;
   ULONG maxReceiveDataBuffers = 1;
   ULONG maxOutstandingSend = 4;
   ULONG maxSendDataBuffers = 1;

   void *pContext = 0;

   RIO_RQ requestQueue = rio.RIOCreateRequestQueue(acceptedSocket, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, queue, queue, pContext);

   if (requestQueue == RIO_INVALID_RQ)
   {
      ErrorExit();
   }

   const size_t largePageMin = 0;//GetLargePageMinimum();

   SYSTEM_INFO systemInfo;

   GetSystemInfo(&systemInfo);

   systemInfo.dwAllocationGranularity;

   size_t bytesAllocated = 0;

   //while (1)
   {
      const DWORD minSize = 4096 * 1000;

      const DWORD buffer1Size = 2 ;//RoundUp(minSize, max(systemInfo.dwAllocationGranularity, largePageMin));

      char *pBuffer1 = reinterpret_cast<char *>(VirtualAlloc(0, buffer1Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

   //   char *pBuffer1 = reinterpret_cast<char *>(VirtualAllocEx(GetCurrentProcess(), 0, buffer1Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE | (largePageMin ? MEM_LARGE_PAGES : 0)));

      if (pBuffer1 == 0)
      {
         ErrorExit();
      }

      RIO_BUFFERID id = rio.RIORegisterBuffer(pBuffer1, buffer1Size);

      if (id == RIO_INVALID_BUFFERID)
      {
         ErrorExit();
      }

      bytesAllocated+= buffer1Size;

      printf("Bytes allocated: %i\n", bytesAllocated);
   }

   RIO_BUFFERID id = 0;

   MyRIO_BUF buffer1;

   buffer1.operation = 1;
   buffer1.BufferId = id;
   buffer1.Offset = 0;
   buffer1.Length = 10;

   MyRIO_BUF buffer2;

   buffer2.operation = 1;
   buffer2.BufferId = id;
   buffer2.Offset = 10;
   buffer2.Length = 10;

   MyRIO_BUF buffer3;

   buffer3.operation = 1;
   buffer3.BufferId = id;
   buffer3.Offset = 20;
   buffer3.Length = 10;

   MyRIO_BUF buffer4;

   buffer4.operation = 1;
   buffer4.BufferId = id;
   buffer4.Offset = 30;
   buffer4.Length = 10;

   DWORD flags = 0;

   INT notifyResult = rio.RIONotify(queue);

   if (!rio.RIOReceive(requestQueue, &buffer1, 1, flags, &buffer1))
   {
      ErrorExit();
   }

   if (!rio.RIOReceive(requestQueue, &buffer2, 1, flags, &buffer2))
   {
      ErrorExit();
   }

   if (!rio.RIOReceive(requestQueue, &buffer3, 1, flags, &buffer3))
   {
      ErrorExit();
   }

   if (!rio.RIOReceive(requestQueue, &buffer4, 1, flags, &buffer4))
   {
      ErrorExit();
   }

   while(1)
   {
      printf("Loop\n");

      DWORD numberOfBytes = 0;

      ULONG_PTR completionKey = 0;

      OVERLAPPED *pOverlapped = 0;

      if (::GetQueuedCompletionStatus(hIOCP, &numberOfBytes, &completionKey, &pOverlapped, 10000))
      {
         const DWORD lastError = ::GetLastError();

         printf("IOCP - OK - %d - %d\n", lastError, numberOfBytes);

         const DWORD numResults = 10;

         RIORESULT results[numResults];

         ULONG numCompletions = rio.RIODequeueCompletion(queue, results, numResults);

         while (numCompletions)
         {
            printf("Got results: %d\n", numCompletions);

            for (ULONG i = 0; i < numCompletions; ++i)
            {
               MyRIO_BUF *pBuffer = reinterpret_cast<MyRIO_BUF*>(results[i].RequestContext);

               if (pBuffer->operation == 1)
               {
                  printf("Got data %d bytes\n", results[i].BytesTransferred);

                  pBuffer->operation = 0;
                  pBuffer->Length = results[i].BytesTransferred;

                  if (!rio.RIOSend(requestQueue, pBuffer, 1, flags, pBuffer))
                  {
                     ErrorExit();
                  }
               }
               else
               {
                  printf("write completed: %d bytes sent\n", results[i].BytesTransferred);

                  MyRIO_BUF *pBuffer = reinterpret_cast<MyRIO_BUF*>(results[i].RequestContext);

                  pBuffer->operation = 1;
                  pBuffer->Length = 10;

                  if (!rio.RIOReceive(requestQueue,pBuffer, 1, flags, pBuffer))
                  {
                     ErrorExit();
                  }
               }
            }

            numCompletions = rio.RIODequeueCompletion(queue, results, numResults);
         }

         printf("Notify\n");

         INT notifyResult = rio.RIONotify(queue);
      }
      else
      {
         const DWORD lastError = ::GetLastError();
        
         printf("IOCP - Failed - %d\n", lastError);
      }

   }

   return 0;
}

/*
int _tmain(int argc, _TCHAR* argv[])
{
   WSADATA data;

   WORD wVersionRequested = 0x202;

   if (0 != ::WSAStartup(wVersionRequested, &data))
   {
      ErrorExit();
   }

   SOCKET s = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);

   if (s == INVALID_SOCKET)
   {
      ErrorExit();
   }

   RIO_EXTENSION_FUNCTION_TABLE rio;

   GUID functionTableId = WSAID_MULTIPLE_RIO;

   DWORD dwBytes = 0;

   bool ok = true;

   if (0 != WSAIoctl(
      s,
      SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
      &functionTableId,
      sizeof(GUID),
      (void**)&rio,
      sizeof(rio),
      &dwBytes,
      0,
      0))
   {
      ErrorExit();
   }

   const DWORD queueSize = 10;

   HANDLE hEvent = WSACreateEvent();

   RIO_NOTIFICATION_COMPLETION completionType;

   completionType.Type = RIO_EVENT_COMPLETION;
   completionType.Event.EventHandle = hEvent;
   completionType.Event.NotifyReset = TRUE;

   //RIO_CQ queue = rio.RIOCreateCompletionQueue(queueSize, &completionType);
   RIO_CQ queue = rio.RIOCreateCompletionQueue(queueSize, 0);

   if (queue == RIO_INVALID_CQ)
   {
      ErrorExit();
   }

   sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(5050);
   addr.sin_addr.s_addr = INADDR_ANY;

   if (SOCKET_ERROR == ::bind(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)))
   {
      ErrorExit();
   }

   if (SOCKET_ERROR == ::listen(s, 5))
   {
      ErrorExit();
   }

   SOCKET acceptedSocket = ::WSAAccept(
      s,
      0,
      0,
      0,
      0);

   if (SOCKET_ERROR == acceptedSocket)
   {
      ErrorExit();
   }

   ULONG maxOutstandingReceive = 2;
   ULONG maxReceiveDataBuffers = 1;
   ULONG maxOutstandingSend = 1;
   ULONG maxSendDataBuffers = 1;

   void *pContext = 0;

   RIO_RQ requestQueue = rio.RIOCreateRequestQueue(acceptedSocket, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, queue, queue, pContext);

   if (requestQueue == RIO_INVALID_RQ)
   {
      ErrorExit();
   }

   const DWORD buffer1Size = 4096;

   char *pBuffer1 = new char[buffer1Size];

   RIO_BUFFERID id = rio.RIORegisterBuffer(pBuffer1, buffer1Size);

   if (id == RIO_INVALID_BUFFERID)
   {
      ErrorExit();
   }

   RIO_BUF buffer1;

   buffer1.BufferId = id;
   buffer1.Offset = 0;
   buffer1.Length = 10;

   RIO_BUF buffer2;

   buffer2.BufferId = id;
   buffer2.Offset = 10;
   buffer2.Length = 10;

   DWORD flags = 0;

   void *pOperationContext = (void*)1;       // indicate a read

   INT notifyResult = rio.RIONotify(queue);

   {
      if (!rio.RIOReceive(requestQueue, &buffer1, 1, flags, pOperationContext))
      {
         ErrorExit();
      }

      if (!rio.RIOReceive(requestQueue, &buffer2, 1, flags, pOperationContext))
      {
         ErrorExit();
      }

      //const DWORD waitResult = WaitForSingleObject(hEvent, INFINITE);

      //if (waitResult != WAIT_OBJECT_0)
      //{
      //   ErrorExit();
      //}

      RIORESULT result;

      while (0 == rio.RIODequeueCompletion(queue, &result, 1));

      //ULONG numResults = rio.RIODequeueCompletion(queue, &result, 1);

      //if (numResults != 1)
      //{
      //   ErrorExit();
      //}

      printf("Got data %d bytes", result.BytesTransferred);

      RIO_BUF sendBuffer;

      sendBuffer.BufferId = buffer1.BufferId;
      sendBuffer.Offset = buffer1.Offset;
      sendBuffer.Length = result.BytesTransferred;

      if (!rio.RIOSend(requestQueue, &sendBuffer, 1, flags, 0))
      {
         ErrorExit();
      }

      INT notifyResult = rio.RIONotify(queue);
   }

   {
      RIO_BUF buffer[2];

      buffer[0].BufferId = id;
      buffer[0].Offset = 0;
      buffer[0].Length = 10;

      buffer[1].BufferId = id;
      buffer[1].Offset = 10;
      buffer[1].Length = 10;

      if (!rio.RIOReceive(requestQueue, buffer, 2, RIO_MSG_WAITALL, pOperationContext))
      {
         ErrorExit();
      }

      DWORD waitResult = WaitForSingleObject(hEvent, INFINITE);

      if (waitResult != WAIT_OBJECT_0)
      {
         ErrorExit();
      }

      RIORESULT result;

      ULONG numResults = rio.RIODequeueCompletion(queue, &result, 1);

      if (numResults != 1)
      {
         ErrorExit();
      }

      if (result.RequestContext == 1)
      {
         printf("Got data %d bytes\n", result.BytesTransferred);
      }
      else
      {
         printf("write completed: %d bytes sent\n", result.BytesTransferred);
      }

      INT notifyResult = rio.RIONotify(queue);

      waitResult = WaitForSingleObject(hEvent, INFINITE);

      if (waitResult != WAIT_OBJECT_0)
      {
         ErrorExit();
      }

      RIORESULT results[2];

      numResults = rio.RIODequeueCompletion(queue, results, 2);

      if (numResults == 0)
      {
         ErrorExit();
      }

      for (ULONG i = 0; i < numResults; ++i)
      {
         if (results[i].RequestContext == 1)
         {
            printf("Got data %d bytes\n", results[i].BytesTransferred);
         }
         else
         {
            printf("write completed: %d bytes sent\n", results[i].BytesTransferred);
         }
      }

      notifyResult = rio.RIONotify(queue);

      waitResult = WaitForSingleObject(hEvent, INFINITE);

      if (waitResult != WAIT_OBJECT_0)
      {
         ErrorExit();
      }

      numResults = rio.RIODequeueCompletion(queue, results, 2);

      if (numResults == 0)
      {
         ErrorExit();
      }

      for (ULONG i = 0; i < numResults; ++i)
      {
         if (results[i].RequestContext == 1)
         {
            printf("Got data %d bytes\n", results[i].BytesTransferred);
         }
         else
         {
            printf("write completed: %d bytes sent\n", results[i].BytesTransferred);
         }
      }

       notifyResult = rio.RIONotify(queue);

   }


   exit(1);


   CUsesMSWinSockExtensions extensions;

   if (s != INVALID_SOCKET)
   {
      if (extensions.LoadRIOAPI(s))
      {
         const DWORD queueSize = 10;
         
         {
            RIO_CQ queue = extensions.RIOCreateCompletionQueue(queueSize, 0);

            extensions.RIOCloseCompletionQueue(queue);
         }

         {
            RIO_NOTIFICATION_COMPLETION completionType;

            completionType.Type = RIO_EVENT_COMPLETION;
            completionType.Event.EventHandle = CreateEvent(0, TRUE, FALSE, 0);
            completionType.Event.NotifyReset = TRUE;

            RIO_CQ queue = extensions.RIOCreateCompletionQueue(queueSize, &completionType);

            extensions.RIOCloseCompletionQueue(queue);

            CloseHandle(completionType.Event.EventHandle);
         }

         {
            RIO_NOTIFICATION_COMPLETION completionType;

            completionType.Type = RIO_EVENT_COMPLETION;
            completionType.Event.EventHandle = CreateEvent(0, TRUE, FALSE, 0);
            completionType.Event.NotifyReset = FALSE;

            RIO_CQ queue = extensions.RIOCreateCompletionQueue(queueSize, &completionType);

            extensions.RIOCloseCompletionQueue(queue);

            CloseHandle(completionType.Event.EventHandle);
         }

         {
            HANDLE hIOCP = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
            OVERLAPPED overlapped;

            RIO_NOTIFICATION_COMPLETION completionType;

            completionType.Type = RIO_IOCP_COMPLETION;
            completionType.Iocp.IocpHandle = hIOCP;
            completionType.Iocp.CompletionKey = (void*)0;
            completionType.Iocp.Overlapped = &overlapped;

            RIO_CQ queue = extensions.RIOCreateCompletionQueue(queueSize, &completionType);

            extensions.RIOCloseCompletionQueue(queue);

            CloseHandle(completionType.Event.EventHandle);
         }

         RIO_CQ recvQueue = extensions.RIOCreateCompletionQueue(queueSize, 0);
         RIO_CQ sendQueue = extensions.RIOCreateCompletionQueue(queueSize, 0);

         SOCKET s1 = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);
         SOCKET s2 = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);

         try
         {
            ULONG maxOutstandingReceive = 10;
            ULONG maxReceiveDataBuffers = 1;
            ULONG maxOutstandingSend = 10;
            ULONG maxSendDataBuffers = 2;

            void *pContext = 0;

            RIO_RQ requestQueue1 = extensions.RIOCreateRequestQueue(s1, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, recvQueue, sendQueue, pContext);
            RIO_RQ requestQueue2 = extensions.RIOCreateRequestQueue(s2, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, recvQueue, sendQueue, pContext);


         }
         catch(...)
         {
         }

         closesocket(s1);
         closesocket(s2);

         {
            ULONG maxOutstandingReceive = 1;
            ULONG maxReceiveDataBuffers = 1;
            ULONG maxOutstandingSend = 1;
            ULONG maxSendDataBuffers = 2;

            void *pContext = 0;

            SOCKET s = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);

            RIO_RQ requestQueue = extensions.RIOCreateRequestQueue(s, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, recvQueue, sendQueue, pContext);


//            extensions.RIOReceive(requestQueue, 


            closesocket(s);

         }
         
         const DWORD buffer1Size = 4096;

         char *pBuffer1 = new char[buffer1Size];

         RIO_BUFFERID id = extensions.RIORegisterBuffer(pBuffer1, buffer1Size);

         ULONG maxOutstandingReceive = 1;
         ULONG maxReceiveDataBuffers = 1;
         ULONG maxOutstandingSend = 1;
         ULONG maxSendDataBuffers = 2;

         void *pConnectionContext = 0;

         RIO_RQ requestQueue = extensions.RIOCreateRequestQueue(s, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, recvQueue, sendQueue, pConnectionContext);

         RIO_BUF buffer;

         buffer.BufferId = id;
         buffer.Offset = 0;
         buffer.Length = buffer1Size;

         DWORD flags = 0;

         void *pOperationContext = 0;

         extensions.RIOReceive(requestQueue, &buffer, 1, flags, pOperationContext);

         extensions.RIOSend(requestQueue, &buffer, 1, flags, pOperationContext);

         extensions.RIODeregisterBuffer(id);

         extensions.RIOCloseCompletionQueue(recvQueue);
         extensions.RIOCloseCompletionQueue(sendQueue);


      }
   
      closesocket(s);
   }

	return 0;
}
*/
