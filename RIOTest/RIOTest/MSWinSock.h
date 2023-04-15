#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef JETBYTE_TOOLS_SOCKET_MS_WINSOCK_EXTENSIONS_INCLUDED__
#define JETBYTE_TOOLS_SOCKET_MS_WINSOCK_EXTENSIONS_INCLUDED__
///////////////////////////////////////////////////////////////////////////////
// File: MSWinSock.h
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

#include "UsesWinsock.h"

// We have our own copy of mswsock.h so that we can compile with no platform SDK

#include <MSWsock.h>

///////////////////////////////////////////////////////////////////////////////
// Namespace: JetByteTools::Socket
///////////////////////////////////////////////////////////////////////////////

namespace JetByteTools {
namespace Socket {

///////////////////////////////////////////////////////////////////////////////
// CUsesMSWinSockExtensions
///////////////////////////////////////////////////////////////////////////////

/// This class acts as a dynamic function loader for the MS Winsock extension
/// functions via a WSAIoctl() call with SIO_GET_EXTENSION_FUNCTION_POINTER.
/// The assumption is made that all sockets passed to this class will be from
/// the same underlying provider (or, at the very least, all sockets passed
/// to a particular extension function will be from the same provider).
/// \ingroup SocketsUtils
/// \ingroup RAII

class CUsesMSWinSockExtensions : public CUsesWinsock
{
   public:

      CUsesMSWinSockExtensions();

      bool LoadRIOAPI(
         SOCKET s);
	
      bool RIOReceive(
         RIO_RQ SocketQueue,
         PRIO_BUF pData,
         ULONG DataBufferCount,
         DWORD Flags,
         PVOID RequestContext) const;

      bool RIOSend(
         RIO_RQ SocketQueue,
         PRIO_BUF pData,
         ULONG DataBufferCount,
         DWORD Flags,
         PVOID RequestContext) const;

      RIO_CQ RIOCreateCompletionQueue(
         DWORD QueueSize,
         PRIO_NOTIFICATION_COMPLETION NotificationCompletion) const;

      void RIOCloseCompletionQueue(
         RIO_CQ completionQueue) const;

      RIO_RQ RIOCreateRequestQueue(
         SOCKET Socket,
         ULONG MaxOutstandingReceive,
         ULONG MaxReceiveDataBuffers,
         ULONG MaxOutstandingSend,
         ULONG MaxSendDataBuffers,
         RIO_CQ ReceiveCQ,
         RIO_CQ SendCQ,
         PVOID SocketContext) const;

      RIO_BUFFERID RIORegisterBuffer(
         PCHAR DataBuffer,
         DWORD DataLength);

      void RIODeregisterBuffer(
         RIO_BUFFERID id);
      

   private :

      mutable RIO_EXTENSION_FUNCTION_TABLE *m_pRIOAPI;

      /// No copies do not implement
      CUsesMSWinSockExtensions(const CUsesMSWinSockExtensions &rhs);
      /// No copies do not implement
      CUsesMSWinSockExtensions &operator=(const CUsesMSWinSockExtensions &rhs);
};

///////////////////////////////////////////////////////////////////////////////
// Namespace: JetByteTools::Socket
///////////////////////////////////////////////////////////////////////////////

} // End of namespace Socket
} // End of namespace JetByteTools

#endif // JETBYTE_TOOLS_SOCKET_MS_WINSOCK_EXTENSIONS_INCLUDED__

///////////////////////////////////////////////////////////////////////////////
// End of file: MSWinSock.h
///////////////////////////////////////////////////////////////////////////////

