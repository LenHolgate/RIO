#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef JETBYTE_TOOLS_SOCKET_USES_WINSOCK_INCLUDED__
#define JETBYTE_TOOLS_SOCKET_USES_WINSOCK_INCLUDED__
///////////////////////////////////////////////////////////////////////////////
// File: UsesWinsock.h
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

#include <WinSock2.h>

///////////////////////////////////////////////////////////////////////////////
// Namespace: JetByteTools::Socket
///////////////////////////////////////////////////////////////////////////////

namespace JetByteTools {
namespace Socket {

///////////////////////////////////////////////////////////////////////////////
// CUsesWinsock
///////////////////////////////////////////////////////////////////////////////

/// A simple object to support \ref RAII "scope based" Winsock initialisation
/// and uninitialisation. Create an instance of this at the scope where you want
/// Winsock to be available and it will be automatically uninitialised when the
/// scope ends.
/// \ingroup SocketUtils
/// \ingroup RAII

class CUsesWinsock
{
   public :

      /// Note that we don't currently allow you to select the version of
      /// Winsock that you require...

      CUsesWinsock();

      ~CUsesWinsock();

   private :

      WSADATA m_data;

      /// No copies do not implement
      CUsesWinsock(const CUsesWinsock &rhs);
      /// No copies do not implement
      CUsesWinsock &operator=(const CUsesWinsock &rhs);
};

///////////////////////////////////////////////////////////////////////////////
// Namespace: JetByteTools::Socket
///////////////////////////////////////////////////////////////////////////////

} // End of namespace Socket
} // End of namespace JetByteTools

#endif // JETBYTE_TOOLS_SOCKET_USES_WINSOCK_INCLUDED__

///////////////////////////////////////////////////////////////////////////////
// End of file: UsesWinsock.h
///////////////////////////////////////////////////////////////////////////////

