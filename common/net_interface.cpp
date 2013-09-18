// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2013 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//
// Abstraction for low-level network mangement 
//
//-----------------------------------------------------------------------------

#include "net_type.h"
#include "net_log.h"
#include "net_socketaddress.h"
#include "net_interface.h"
#include "net_connection.h"
#include "net_messagecomponent.h"
#include "hashtable.h"

#include "doomdef.h"
#include "m_random.h"
#include "c_cvars.h"
#include "cmdlib.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#ifdef _XBOX
		#include <xtl.h>
	#else
		#include <windows.h>
		#include <winsock2.h>
		#include <ws2tcpip.h>
	#endif // ! _XBOX
#else
	#ifdef GEKKO // Wii/GC
		#include <network.h>
	#else
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <arpa/inet.h>
		#include <netdb.h>
		#include <sys/ioctl.h>
	#endif // ! GEKKO

	#include <sys/types.h>
	#include <errno.h>
	#include <unistd.h>
	#include <sys/time.h>
#endif	// _WIN32

#ifndef _WIN32
	typedef int SOCKET;

	#ifndef GEKKO
		#define SOCKET_ERROR -1
		#define INVALID_SOCKET -1
	#endif

	#define closesocket close
	#define ioctlsocket ioctl
#endif

#ifdef _WIN32
	typedef int socklen_t;
#endif

#ifdef _XBOX
	#include "i_xbox.h"
#endif

#ifdef GEKKO
	#include "i_wii.h"
#endif

#ifdef ODA_HAVE_MINIUPNP
	#define STATICLIB
	#include "miniwget.h"
	#include "miniupnpc.h"
	#include "upnpcommands.h"
#endif	// ODA_HAVE_MINIUPNP

static const uint16_t MAX_INCOMING_SIZE = 8192;

EXTERN_CVAR(net_ip)
EXTERN_CVAR(net_port)
EXTERN_CVAR(net_maxrate)

// ============================================================================
//
// NetInterface Implementation
//
// Abstraction of low-level socket communication. Maintains Connection objects
// for all existing connections.
//
// ============================================================================

// ----------------------------------------------------------------------------
// Public functions
// ----------------------------------------------------------------------------

NetInterface::NetInterface(const std::string& address, uint16_t desired_port) :
	mInitialized(false), mSocket(INVALID_SOCKET)
{
	if (baseapp == client)
		mHostType = HOST_CLIENT;
	else
		mHostType = HOST_SERVER;

	openInterface(address, desired_port);
}


NetInterface::~NetInterface()
{
	closeAllConnections();
	closeInterface();
}


HostType_t NetInterface::getHostType() const
{
	return mHostType;
}


const SocketAddress& NetInterface::getLocalAddress() const
{
	return mLocalAddress;
}


const std::string& NetInterface::getPassword() const
{
	return mPassword;
}


void NetInterface::setPassword(const std::string& password)
{
	mPassword = password;
}


//
// NetInterface::requestConnection
//
// Creates a new Connection object that will request a connection
// from the host at remote_address. Returns the ID of the connection.
//
ConnectionId NetInterface::requestConnection(const SocketAddress& remote_address)
{
	Net_LogPrintf("NetInterface::requestConnection: requesting connection to %s.", std::string(remote_address).c_str());

	Connection* conn = createNewConnection(remote_address);
	conn->openConnection();

	return conn->getConnectionId();
}


//
// NetInterface::closeConnection
//
// 
//
void NetInterface::closeConnection(const ConnectionId& connection_id)
{
	Connection* conn = findConnection(connection_id);
	if (conn == NULL)
		return;	

	const char* remote_address_cstring = conn->getRemoteAddressCString();
	Net_LogPrintf("NetInterface::closeConnection: closing connection to %s.", remote_address_cstring);
	conn->closeConnection();
}


//
// NetInterface::closeAllConnections
//
// Iterates through all Connection objects and frees them, updating the
// connection hash tables and freeing all used connection IDs.
//
void NetInterface::closeAllConnections()
{
	// free all Connection objects
	ConnectionIdTable::const_iterator it;
	for (it = mConnectionsById.begin(); it != mConnectionsById.end(); ++it)
	{
		Connection* conn = it->second;
		conn->closeConnection();
		delete conn;
	}

	mConnectionsById.clear();
	mConnectionsByAddress.clear();
}


//
// NetInterface::isConnected
//
// Returns true if there is a connection that matches connection_id and is
// in a healthy state (not timed-out).
//
bool NetInterface::isConnected(const ConnectionId& connection_id) const
{
	const Connection* conn = findConnection(connection_id);
	if (!conn)
		return false;
	return conn->isConnected();
}


//
// NetInterface::findConnection
//
// Returns a pointer to the Connection object with the connection ID
// matching connection_id, or returns NULL if it was not found.
//
Connection* NetInterface::findConnection(const ConnectionId& connection_id) 
{
	ConnectionIdTable::const_iterator it = mConnectionsById.find(connection_id);
	if (it != mConnectionsById.end())
		return it->second;
	return NULL;
}

const Connection* NetInterface::findConnection(const ConnectionId& connection_id) const
{
	ConnectionIdTable::const_iterator it = mConnectionsById.find(connection_id);
	if (it != mConnectionsById.end())
		return it->second;
	return NULL;
}

//
// NetInterface::sendPacket
//
// Public function to send a packet to a remote host. In actuality, the packet
// is queued for delivery at a later time, dictated by the network effects
// simulation code.
//
void NetInterface::sendPacket(const ConnectionId& connection_id, BitStream& stream)
{
	Connection* conn = findConnection(connection_id);
	if (conn)
	{
		// TODO: insert the data into the packet queue

		uint8_t data[MAX_INCOMING_SIZE];
		uint16_t size = (stream.readSize() + 7) >> 3;
		stream.readBlob(data, size);
		socketSend(conn->getRemoteAddress(), data, size);
	}
}


//
// NetInterface::service
//
// Checks for incoming packets, creating new connections as appropriate, or
// calling an individual connection's service function,
//
void NetInterface::service()
{
//	servicePendingConnections();

	// check for incoming packets
	static uint8_t data[MAX_INCOMING_SIZE];
	uint16_t size = 0;
	SocketAddress source;

	while (socketRecv(source, data, size))
	{
		BitStream stream;
		stream.writeBlob(data, size);
		processPacket(source, stream);
	}

	// iterate through all of the connections and call their service function
	ConnectionIdTable::const_iterator it;
	for (it = mConnectionsById.begin(); it != mConnectionsById.end(); ++it)
	{
		Connection* conn = it->second;
		conn->service();

		// close timed-out connections
		// they will be freed later after a time-out period
		if (!conn->isTerminated() && conn->isTimedOut())
		{
			Net_LogPrintf("NetInterface::service: connection to %s timed-out.", conn->getRemoteAddressCString());
			conn->closeConnection();
		}
	}

	// iterate through the connections and delete any that are in the
	// termination state and have finished their time-out period.
	it = mConnectionsById.begin();
	while (it != mConnectionsById.end())
	{
		bool reiterate = false;

		Connection* conn = it->second;
		if (conn->isTerminated() && conn->isTimedOut())
		{
			Net_LogPrintf("NetInterface::service: removing terminated connection to %s.", conn->getRemoteAddressCString());
			mConnectionsById.erase(conn->getConnectionId());
			mConnectionsByAddress.erase(conn->getRemoteAddress());
			delete conn;
			reiterate = true;
		}

		// if we remove any items from the hash table while iterating,
		// the iterator becomes invalid and we have to start over.
		if (reiterate)
			it = mConnectionsById.begin();
		else
			++it;
	}
}


// ----------------------------------------------------------------------------
// Private low-level socket functions
// ----------------------------------------------------------------------------

//
// NetInterface::openInterface
//
// Binds the interface to a port and opens a socket. If it is not possible to
// bind to desired_port, it will attempt to bind to one of the next 16
// sequential port numbers.
//
void NetInterface::openInterface(const std::string& ip_string, uint16_t desired_port)
{
	if (mInitialized)
		closeInterface();		// already open? close first.

	#ifdef _WIN32
	WSADATA wsad;
	WSAStartup(MAKEWORD(2,2), &wsad);
	#endif

	// determine which interface IP address to bind to
	uint32_t desired_ip = INADDR_ANY;	// use any availible interface IP address
	if (!ip_string.empty() && !iequals(ip_string, "localhost"))
	{
		// resolve 'address' into an integer IP address
		SocketAddress tmpaddr(ip_string + ":1");
		if (tmpaddr.isValid())
			desired_ip = tmpaddr.getIPAddress();
	}

	// retreive hostname
	static const size_t MAX_HOSTNAME_LEN = 256;
	static char buf[MAX_HOSTNAME_LEN];
	gethostname(buf, MAX_HOSTNAME_LEN);
	buf[MAX_HOSTNAME_LEN - 1] = 0;		// ensure properly terminated string

	// Search the availible interface addresses for one matching the desired_ip.
	// If any address is acceptible or a match is not found, the first availible
	// address is used (often 127.0.0.1).
	hostent* ent = gethostbyname(buf);
	if (ent && ent->h_addrtype == AF_INET && ent->h_addr_list[0] != NULL)
	{
		struct sockaddr_in sockaddr;
		*(int *)&sockaddr.sin_addr = *(int *)ent->h_addr_list[0];

		SocketAddress first_address = sockaddr, current_address = sockaddr;

		if (desired_ip != INADDR_ANY)
		{
			for (int i = 1; ent->h_addr_list[i] != NULL; i++)
			{
				*(int *)&sockaddr.sin_addr = *(int *)ent->h_addr_list[i];
				current_address = sockaddr;	
				if (current_address.getIPAddress() == desired_ip)
					break;
				else
					current_address = first_address;
			}
		}

		desired_ip = current_address.getIPAddress();
	}

	// open a socket and bind it to a port and set mLocalAddress
	openSocket(desired_ip, desired_port);
	if (!mLocalAddress.isValid())
	{
     	Net_Error("NetInterface::openInterface: unable to create socket for %s:%u!", ip_string.c_str(), desired_port);
		return;
	}

	// make mSocket non-blocking
	unsigned long ulongtrue = true;
	if (ioctlsocket(mSocket, FIONBIO, &ulongtrue) == -1)
	{
		closeSocket();
		Net_Error("NetInterface::openInterface: ioctl FIONBIO: %s", strerror(errno));
	}

	Net_Printf("NetInterface: Opened socket at %s.", std::string(mLocalAddress).c_str());

	#ifdef ODA_HAVE_MINIUPNP
	// TODO: add UPNP support
/*
	init_upnp();

	std::string str = mLocalAddress;
	std::string ipstr = str.substr(0, str.find(':', 0));
	std::string portstr = str.substr(str.find(':', 0) + 1, npos);

	sv_upnp_internalip.Set(ipstr.c_str());
	upnp_add_redir(ipstr.c_str(), portstr);

	Net_Printf("NetInterface: Initialized uPnP.\n");
*/
	#endif

	mInitialized = true;
}

//
// NetInterace::closeInterface
//
// Releases the binding to a port and closes the socket.
//
void NetInterface::closeInterface()
{
	if (!mInitialized)
		return;

	#ifdef ODA_HAVE_MINIUPNP
	// TODO: add UPNP support
//	upnp_rem_redir(mLocalAddress.getPort());
	#endif

	closeSocket();

	#ifdef _WIN32
	WSACleanup();
	#endif

	mLocalAddress.clear();

	mInitialized = false;
}


//
// NetInterface::openSocket
//
// Binds the interface to a port and opens a socket. If it is not possible to
// bind to desired_port, it will attempt to bind to one of the next 16
// sequential port numbers. mLocalAddress is set to the address of the opened
// socket.
//
void NetInterface::openSocket(uint32_t desired_ip, uint16_t desired_port)
{
	mLocalAddress.clear();

	mSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (mSocket == INVALID_SOCKET)
		return;

	struct sockaddr_in sockaddress;
	memset(&sockaddress, 0, sizeof(sockaddress));
	sockaddress.sin_family = AF_INET;
	sockaddress.sin_addr.s_addr = htonl(desired_ip);

	// try up to 16 port numbers until we find an unbound one
	for (uint16_t port = desired_port; port < desired_port + 16; port++)
	{
		sockaddress.sin_port = htons(port);
		int result = bind(mSocket, (sockaddr *)&sockaddress, sizeof(sockaddress));

		if (result != SOCKET_ERROR)
		{
			mLocalAddress = sockaddress;
			return;
		}
	}

	// unable to bind to a port
	closeSocket();
}


//
// NetInterface::closeSocket
//
// Closes mSocket if it's open.
//
void NetInterface::closeSocket()
{
	if (mSocket != INVALID_SOCKET)
	{
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}
}


//
// NetInterface::socketSend
//
// Low-level function to send data to a remote host.
//
bool NetInterface::socketSend(const SocketAddress& dest, const uint8_t* data, uint16_t size)
{
	Net_LogPrintf("NetInterface::socketSend: sending %u bytes to %s.", size, std::string(dest).c_str());

	if (size == 0 || !dest.isValid())
		return false;

	struct sockaddr_in saddr = dest.toSockAddr();

	// pass the packet data off to the IP stack
	int ret = sendto(mSocket, data, size, 0, (struct sockaddr *)&saddr, sizeof(saddr));

	if (ret == -1)
	{
		Net_Warning("NetInterface::socketSend: Unable to send packet to destination %s", 
				std::string(dest).c_str());

		#ifdef _WIN32
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)		// wouldblock is silent
			return false;
		#else
		if (errno == EWOULDBLOCK)
			return false;
		if (errno == ECONNREFUSED)
			return false;
		#endif
	}

	return true;
}


//
// NetInterface::socketRecv
//
// Low-level function to receive data from a remote host. The source parameter
// is set to the socket address of the sending host. The data buffer must be
// large enough to accomodate MAX_INCOMING_SIZE bytes.
//
bool NetInterface::socketRecv(SocketAddress& source, uint8_t* data, uint16_t& size)
{
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);

	int ret = recvfrom(mSocket, data, MAX_INCOMING_SIZE, 0, (struct sockaddr *)&from, &fromlen);

	source = from;
	size = 0;

	if (ret == -1)
	{
		#ifdef _WIN32
		errno = WSAGetLastError();

		if (errno == WSAEWOULDBLOCK)
			return false;

		if (errno == WSAECONNRESET)
			return false;

		if (errno == WSAEMSGSIZE)
		{
			std::string adrstr(fromadr);
			Net_Warning("NetInterface::socketRecv: Warning: Oversize packet from %s", std::string(source).c_str());
			return false;
		}
		#else
		if (errno == EWOULDBLOCK)
			return false;
		if (errno == ECONNREFUSED)
			return false;
		#endif

		Net_Warning("NetInterface::socketRecv: %s", strerror(errno));
		return false;
	}

	if (ret > 0)
	{
		size = ret;
		Net_LogPrintf("NetInterface::socketRecv: received %u bytes from %s.", size, std::string(source).c_str());
	}

	return (size > 0);
}


// ----------------------------------------------------------------------------
// Private connection handling functions
// ----------------------------------------------------------------------------

//
// NetInterface::findConnection
//
// Returns a pointer to the Connection object with the remote socket address
// matching remote_address, or returns NULL if it was not found.
//
Connection* NetInterface::findConnection(const SocketAddress& remote_address)
{
	ConnectionAddressTable::const_iterator it = mConnectionsByAddress.find(remote_address);
	if (it != mConnectionsByAddress.end())
		return it->second;	
	return NULL;
}

const Connection* NetInterface::findConnection(const SocketAddress& remote_address) const
{
	ConnectionAddressTable::const_iterator it = mConnectionsByAddress.find(remote_address);
	if (it != mConnectionsByAddress.end())
		return it->second;	
	return NULL;
}

//
// NetInterface::createNewConnection
//
// Instantiates a new Connection object with the supplied connection ID
// and socket address. The Connection object is added to the ID and address
// lookup hash tables and a pointer to the object is returned.
//
Connection* NetInterface::createNewConnection(const SocketAddress& remote_address)
{
	ConnectionId connection_id = mConnectionIdGenerator.generate();

	Connection* conn = new Connection(connection_id, this, remote_address);
	mConnectionsById.insert(std::pair<ConnectionId, Connection*>(connection_id, conn));
	mConnectionsByAddress.insert(std::pair<SocketAddress, Connection*>(remote_address, conn));
	return conn;
}


//
// NetInterface::processPacket
//
// Handles the processing and delivery of incoming packets.
//
void NetInterface::processPacket(const SocketAddress& remote_address, BitStream& stream)
{
	Connection* conn = findConnection(remote_address);

	if (conn)
	{


	}
	else if (mHostType == HOST_SERVER)
	{
		// packet from an unknown host
		Net_LogPrintf("NetInterface::processPacket: received packet from an unknown host %s.", std::string(remote_address).c_str());
		conn = createNewConnection(remote_address);
	}
	else
	{
		// clients shouldn't accept packets from unknown sources
		return;
	}

	conn->processPacket(stream);
}


VERSION_CONTROL (net_interface_cpp, "$Id$")

