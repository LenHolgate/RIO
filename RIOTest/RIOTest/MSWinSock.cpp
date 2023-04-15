///////////////////////////////////////////////////////////////////////////////
// File: MSWinSock.cpp
///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2002 JetByte Limited.
//
// This software is provided "as is" without a warranty of any kind. All
// express or implied conditions, representations and warranties, including
// any implied warranty of merchantability, fitness for a particular purpose
// or non-infringement, are hereby excluded. JetByte Limited and its licensors
// shall not be liable for any damages suffered by licensee as a result of
// using the software. In no event will JetByte Limited be liable for any
// lost revenue, profit or data, or for direct, indirect, special,
// consequential, incidental or punitive damages, however caused and regardless
// of the theory of liability, arising out of the use of or inability to use
// software, even if JetByte Limited has been advised of the possibility of
// such damages.
//
///////////////////////////////////////////////////////////////////////////////

//#include "JetByteTools\Admin\Admin.h"

#include "MSWinSock.h"
#include "stdafx.h"

//#include "JetByteTools\Win32Tools\Exception.h"
//#include "JetByteTools\Win32Tools\Utils.h"

#pragma hdrstop

///////////////////////////////////////////////////////////////////////////////
// Using directives
///////////////////////////////////////////////////////////////////////////////

//using JetByteTools::Win32::ToBool;
//using JetByteTools::Win32::CException;

///////////////////////////////////////////////////////////////////////////////
// Namespace: JetByteTools::Socket
///////////////////////////////////////////////////////////////////////////////

namespace JetByteTools {
namespace Socket {

///////////////////////////////////////////////////////////////////////////////
// Static helper functions
///////////////////////////////////////////////////////////////////////////////

static bool LoadExtensionFunction(
   SOCKET s,
   GUID functionID,
   void **ppFunc);

///////////////////////////////////////////////////////////////////////////////
// CUsesMSWinSockExtensions
///////////////////////////////////////////////////////////////////////////////

CUsesMSWinSockExtensions::CUsesMSWinSockExtensions()
   :  m_pRIOAPI(new RIO_EXTENSION_FUNCTION_TABLE())
{
   //memset(m_pRIOAPI, 0, sizeof(RIO_EXTENSION_FUNCTION_TABLE));
   //m_pRIOAPI->cbSize = sizeof(RIO_EXTENSION_FUNCTION_TABLE);

}

bool CUsesMSWinSockExtensions::LoadRIOAPI(
   SOCKET s)
{
   GUID functionTableId = WSAID_MULTIPLE_RIO;

   DWORD dwBytes = 0;

   bool ok = true;

   if (0 != WSAIoctl(
      s,
      SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
      &functionTableId,
      sizeof(GUID),
      (void**)m_pRIOAPI,
      sizeof(RIO_EXTENSION_FUNCTION_TABLE),
      &dwBytes,
      0,
      0))
   {
      const DWORD lastError = ::GetLastError();

      ok = false;
   }

   return ok && m_pRIOAPI;
}
	
bool CUsesMSWinSockExtensions::RIOReceive(
   RIO_RQ socketQueue,
   PRIO_BUF pData,
   ULONG dataBufferCount,
   DWORD flags,
   PVOID requestContext) const
{
   if (!m_pRIOAPI)
   {
      throw "TODO";
   }

   if (TRUE != m_pRIOAPI->RIOReceive(socketQueue, pData, dataBufferCount, flags, requestContext))
   {
      const DWORD lastError = ::GetLastError();

   }

   return true;
}

bool CUsesMSWinSockExtensions::RIOSend(
   RIO_RQ socketQueue,
   PRIO_BUF pData,
   ULONG dataBufferCount,
   DWORD flags,
   PVOID requestContext) const
{
   if (!m_pRIOAPI)
   {
      throw "TODO";
   }

   if (TRUE != m_pRIOAPI->RIOSend(socketQueue, pData, dataBufferCount, flags, requestContext))
   {
      const DWORD lastError = ::GetLastError();

   }

   return true;
}

RIO_RQ CUsesMSWinSockExtensions::RIOCreateRequestQueue(
   SOCKET socket,
   ULONG maxOutstandingReceive,
   ULONG maxReceiveDataBuffers,
   ULONG maxOutstandingSend,
   ULONG maxSendDataBuffers,
   RIO_CQ receiveCQ,
   RIO_CQ sendCQ,
   PVOID socketContext) const
{
   if (!m_pRIOAPI)
   {
      throw "TODO";
   }

   RIO_RQ requestQueue = m_pRIOAPI->RIOCreateRequestQueue(socket, maxOutstandingReceive, maxReceiveDataBuffers, maxOutstandingSend, maxSendDataBuffers, receiveCQ, sendCQ, socketContext);

   if (requestQueue == RIO_INVALID_RQ)
   {
      const DWORD lastError = ::GetLastError();

      throw "TODO";
   }

   return requestQueue;
}

RIO_CQ CUsesMSWinSockExtensions::RIOCreateCompletionQueue(
   DWORD queueSize,
   PRIO_NOTIFICATION_COMPLETION notificationCompletion) const
{
   if (!m_pRIOAPI)
   {
      throw "TODO";
   }

   const RIO_CQ completionQueue = m_pRIOAPI->RIOCreateCompletionQueue(queueSize, notificationCompletion);

   if (completionQueue == RIO_INVALID_CQ)
   {
      const DWORD lastError = ::GetLastError();

      throw "TODO";
   }

   return completionQueue;
}

void CUsesMSWinSockExtensions::RIOCloseCompletionQueue(
   RIO_CQ completionQueue) const
{
   if (!m_pRIOAPI)
   {
      throw "TODO";
   }

   m_pRIOAPI->RIOCloseCompletionQueue(completionQueue);
}

RIO_BUFFERID CUsesMSWinSockExtensions::RIORegisterBuffer(
   PCHAR DataBuffer,
   DWORD DataLength)
{
   if (!m_pRIOAPI)
   {
      throw "TODO";
   }

   RIO_BUFFERID id = m_pRIOAPI->RIORegisterBuffer(DataBuffer, DataLength);

   if (id == RIO_INVALID_BUFFERID)
   {
      const DWORD lastError = ::GetLastError();

      throw "TODO";
   }

   return id;

}

void CUsesMSWinSockExtensions::RIODeregisterBuffer(
   RIO_BUFFERID id)
{
   if (!m_pRIOAPI)
   {
      throw "TODO";
   }

   m_pRIOAPI->RIODeregisterBuffer(id);
}

///////////////////////////////////////////////////////////////////////////////
// Namespace: JetByteTools::Socket
///////////////////////////////////////////////////////////////////////////////

} // End of namespace Socket
} // End of namespace JetByteTools

///////////////////////////////////////////////////////////////////////////////
// End of file: MSWinSock.cpp
///////////////////////////////////////////////////////////////////////////////

