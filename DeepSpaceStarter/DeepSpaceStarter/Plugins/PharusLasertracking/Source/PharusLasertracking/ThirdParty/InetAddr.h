
/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2009-2020
  ========================================================================*/

#pragma once
///#ifndef _INET_ADDR_H_
///#define _INET_ADDR_H_

#define _WINSOCK_DEPRECATED_NO_WARNINGS // ID NOTE: UGLY HACK
///#ifdef WIN32
#if PLATFORM_WINDOWS // EPIC UBER METHOD
	#include "Windows/AllowWindowsPlatformTypes.h" 
	#include <winsock2.h>
	#include "Windows/HideWindowsPlatformTypes.h" 
#endif
///#else // UNIX
///	#include <sys/types.h>
///	#include <sys/socket.h>
///	#include <netinet/in.h>
///	#include <arpa/inet.h>
///	#define USHORT unsigned short
///	#define ULONG unsigned long
///	#define SOCKET int
///	#define INVALID_SOCKET (-1)
///	#define SOCKET_ERROR (-1)
///#endif

#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

/// Socket constants.
#define SOCKET_TIMEOUT      (SOCKET_ERROR - 1)
#define NO_TIMEOUT          (-1)
#define DEFAULT_TIMEOUT     NO_TIMEOUT

//--------------------------------------------------------------------------------
class InetAddr : public sockaddr_in
{
public:
  // constructors
	InetAddr()
	{
		memset(this, 0, sizeof(InetAddr));
		sin_family= AF_INET;
		sin_port= 0;
		sin_addr.s_addr= 0;
	};

	InetAddr(const struct sockaddr& sockAddr)
	{
		memcpy(this, &sockAddr, sizeof(struct sockaddr));
	};

	InetAddr(const struct sockaddr_in& sin)
	{
		memcpy(this, &sin, sizeof(struct sockaddr_in));
	};

	InetAddr(const ULONG ulAddr, const USHORT ushPort= 0)
	{
		memset(this, 0, sizeof(InetAddr));
		sin_family= AF_INET;
		sin_port= htons(ushPort);
		sin_addr.s_addr= htonl(ulAddr);
	};

	InetAddr(const wchar_t* pStrIP, const USHORT usPort= 0)
	{
		char szStrIP[32];

#ifdef WIN32
		WideCharToMultiByte(CP_ACP, 0, pStrIP, (int)wcslen(pStrIP) + 1, szStrIP, 32, 0, 0);
#else
        wcstombs(szStrIP, pStrIP, sizeof(szStrIP));
#endif
		memset(this, 0, sizeof(InetAddr));
		sin_family= AF_INET;
		sin_port= htons(usPort);
		sin_addr.s_addr= inet_addr(szStrIP);
	}

	InetAddr(const char* pStrIP, const USHORT usPort= 0)
	{
		memset(this, 0, sizeof(InetAddr));
		sin_family= AF_INET;
		sin_port= htons(usPort);
		sin_addr.s_addr= inet_addr(pStrIP);
	}

	//* returns the address in dotted-decimal format
	const char* DottedDecimal() const
	{
		return inet_ntoa(sin_addr);
	}
	USHORT GetPort() const
	{
		return ntohs(sin_port);
	}
	ULONG GetIpAddr() const
	{
		return ntohl(sin_addr.s_addr);
	}

	//* operators added for efficiency
	const InetAddr& operator=(const struct sockaddr& sa)
	{
		memcpy(this, &sa, sizeof(struct sockaddr));
		return *this;
	}
	const InetAddr& operator=(const struct sockaddr_in& sin)
	{
		memcpy(this, &sin, sizeof(struct sockaddr_in));
		return *this;
	}
	operator struct sockaddr() const
	{
		return *((struct sockaddr *)this);
	}
	operator const struct sockaddr() const
	{
		return *((const struct sockaddr *)this);
	}
	operator struct sockaddr *()
	{
		return (struct sockaddr *)this;
	}
	operator const struct sockaddr *() const
	{
		return (const struct sockaddr *)this;
	}
	operator struct sockaddr_in *()
	{
		return (struct sockaddr_in *) this;
	}
	operator const struct sockaddr_in *() const
	{
		return (const struct sockaddr_in *) this;
	}
};

typedef const InetAddr*		LPCINETADDR;
typedef InetAddr*			LPINETADDR;

///#endif // _INET_ADDR_H_
