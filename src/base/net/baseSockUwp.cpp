/* $Id: sock_uwp.cpp 5539 2017-01-23 04:32:34Z nanang $ */
/* 
 * Copyright (C) 2016 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <baseAssert.h>
#include <baseErrno.h>
#include <math.h>
#include <baseOs.h>
#include <compat/socket.h>

#include <ppltasks.h>
#include <string>

#include "sock_uwp.h"

 /*
 * Address families conversion.
 * The values here are indexed based on baddr_family.
 */
const buint16_t BASE_AF_UNSPEC	= AF_UNSPEC;
const buint16_t BASE_AF_UNIX	= AF_UNIX;
const buint16_t BASE_AF_INET	= AF_INET;
const buint16_t BASE_AF_INET6	= AF_INET6;
#ifdef AF_PACKET
const buint16_t BASE_AF_PACKET	= AF_PACKET;
#else
const buint16_t BASE_AF_PACKET	= 0xFFFF;
#endif
#ifdef AF_IRDA
const buint16_t BASE_AF_IRDA	= AF_IRDA;
#else
const buint16_t BASE_AF_IRDA	= 0xFFFF;
#endif

/*
* Socket types conversion.
* The values here are indexed based on bsock_type
*/
const buint16_t BASE_SOCK_STREAM= SOCK_STREAM;
const buint16_t BASE_SOCK_DGRAM	= SOCK_DGRAM;
const buint16_t BASE_SOCK_RAW	= SOCK_RAW;
const buint16_t BASE_SOCK_RDM	= SOCK_RDM;

/*
* Socket level values.
*/
const buint16_t BASE_SOL_SOCKET	= SOL_SOCKET;
#ifdef SOL_IP
const buint16_t BASE_SOL_IP	= SOL_IP;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64) 
const buint16_t BASE_SOL_IP	= IPPROTO_IP;
#else
const buint16_t BASE_SOL_IP	= 0;
#endif /* SOL_IP */

#if defined(SOL_TCP)
const buint16_t BASE_SOL_TCP	= SOL_TCP;
#elif defined(IPPROTO_TCP)
const buint16_t BASE_SOL_TCP	= IPPROTO_TCP;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64)
const buint16_t BASE_SOL_TCP	= IPPROTO_TCP;
#else
const buint16_t BASE_SOL_TCP	= 6;
#endif /* SOL_TCP */

#ifdef SOL_UDP
const buint16_t BASE_SOL_UDP	= SOL_UDP;
#elif defined(IPPROTO_UDP)
const buint16_t BASE_SOL_UDP	= IPPROTO_UDP;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64)
const buint16_t BASE_SOL_UDP	= IPPROTO_UDP;
#else
const buint16_t BASE_SOL_UDP	= 17;
#endif /* SOL_UDP */

#ifdef SOL_IPV6
const buint16_t BASE_SOL_IPV6	= SOL_IPV6;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64)
#   if defined(IPPROTO_IPV6) || (_WIN32_WINNT >= 0x0501)
const buint16_t BASE_SOL_IPV6	= IPPROTO_IPV6;
#   else
const buint16_t BASE_SOL_IPV6	= 41;
#   endif
#else
const buint16_t BASE_SOL_IPV6	= 41;
#endif /* SOL_IPV6 */

/* IP_TOS */
#ifdef IP_TOS
const buint16_t BASE_IP_TOS	= IP_TOS;
#else
const buint16_t BASE_IP_TOS	= 1;
#endif


/* TOS settings (declared in netinet/ip.h) */
#ifdef IPTOS_LOWDELAY
const buint16_t BASE_IPTOS_LOWDELAY	= IPTOS_LOWDELAY;
#else
const buint16_t BASE_IPTOS_LOWDELAY	= 0x10;
#endif
#ifdef IPTOS_THROUGHPUT
const buint16_t BASE_IPTOS_THROUGHPUT	= IPTOS_THROUGHPUT;
#else
const buint16_t BASE_IPTOS_THROUGHPUT	= 0x08;
#endif
#ifdef IPTOS_RELIABILITY
const buint16_t BASE_IPTOS_RELIABILITY	= IPTOS_RELIABILITY;
#else
const buint16_t BASE_IPTOS_RELIABILITY	= 0x04;
#endif
#ifdef IPTOS_MINCOST
const buint16_t BASE_IPTOS_MINCOST	= IPTOS_MINCOST;
#else
const buint16_t BASE_IPTOS_MINCOST	= 0x02;
#endif


/* optname values. */
const buint16_t BASE_SO_TYPE    = SO_TYPE;
const buint16_t BASE_SO_RCVBUF  = SO_RCVBUF;
const buint16_t BASE_SO_SNDBUF  = SO_SNDBUF;
const buint16_t BASE_TCP_NODELAY= TCP_NODELAY;
const buint16_t BASE_SO_REUSEADDR= SO_REUSEADDR;
#ifdef SO_NOSIGPIPE
const buint16_t BASE_SO_NOSIGPIPE = SO_NOSIGPIPE;
#else
const buint16_t BASE_SO_NOSIGPIPE = 0xFFFF;
#endif
#if defined(SO_PRIORITY)
const buint16_t BASE_SO_PRIORITY = SO_PRIORITY;
#else
/* This is from Linux, YMMV */
const buint16_t BASE_SO_PRIORITY = 12;
#endif

const buint16_t	BASE_SO_BOARDCAST	= SO_BROADCAST;

/* Multicasting is not supported e.g. in PocketPC 2003 SDK */
#ifdef IP_MULTICAST_IF
const buint16_t BASE_IP_MULTICAST_IF    = IP_MULTICAST_IF;
const buint16_t BASE_IP_MULTICAST_TTL   = IP_MULTICAST_TTL;
const buint16_t BASE_IP_MULTICAST_LOOP  = IP_MULTICAST_LOOP;
const buint16_t BASE_IP_ADD_MEMBERSHIP  = IP_ADD_MEMBERSHIP;
const buint16_t BASE_IP_DROP_MEMBERSHIP = IP_DROP_MEMBERSHIP;
#else
const buint16_t BASE_IP_MULTICAST_IF    = 0xFFFF;
const buint16_t BASE_IP_MULTICAST_TTL   = 0xFFFF;
const buint16_t BASE_IP_MULTICAST_LOOP  = 0xFFFF;
const buint16_t BASE_IP_ADD_MEMBERSHIP  = 0xFFFF;
const buint16_t BASE_IP_DROP_MEMBERSHIP = 0xFFFF;
#endif

/* recv() and send() flags */
const int BASE_MSG_OOB		= MSG_OOB;
const int BASE_MSG_PEEK		= MSG_PEEK;
const int BASE_MSG_DONTROUTE	= MSG_DONTROUTE;



using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Networking;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;


ref class PjUwpSocketDatagramRecvHelper sealed
{
internal:
    PjUwpSocketDatagramRecvHelper(PjUwpSocket* uwp_sock_) :
	uwp_sock(uwp_sock_), avail_data_len(0), recv_args(nullptr)
    {
	recv_wait = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	event_token = uwp_sock->datagram_sock->MessageReceived += 
	    ref new TypedEventHandler<DatagramSocket^, 
				      DatagramSocketMessageReceivedEventArgs^>
		    (this, &PjUwpSocketDatagramRecvHelper::OnMessageReceived);
    }

    void OnMessageReceived(DatagramSocket ^sender,
			   DatagramSocketMessageReceivedEventArgs ^args)
    {
	try {
	    if (uwp_sock->sock_state >= SOCKSTATE_DISCONNECTED)
		return;

	    recv_args = args;
	    avail_data_len = args->GetDataReader()->UnconsumedBufferLength;

	    if (uwp_sock->cb.on_read) {
		(*uwp_sock->cb.on_read)(uwp_sock, avail_data_len);
	    }

	    WaitForSingleObjectEx(recv_wait, INFINITE, false);
	} catch (...) {}
    }

    bstatus_t ReadDataIfAvailable(void *buf, bssize_t *len,
				    bsockaddr_t *from)
    {
	if (avail_data_len <= 0)
	    return BASE_ENOTFOUND;

	if (*len < avail_data_len)
	    return BASE_ETOOSMALL;

	// Read data
	auto reader = recv_args->GetDataReader();
	auto buffer = reader->ReadBuffer(avail_data_len);
	unsigned char *p;
	GetRawBufferFromIBuffer(buffer, &p);
	bmemcpy((void*) buf, p, avail_data_len);
	*len = avail_data_len;

	// Get source address
	wstr_addr_to_sockaddr(recv_args->RemoteAddress->CanonicalName->Data(),
			      recv_args->RemotePort->Data(), from);

	// finally
	avail_data_len = 0;
	SetEvent(recv_wait);

	return BASE_SUCCESS;
    }

private:

    ~PjUwpSocketDatagramRecvHelper()
    {
	if (uwp_sock->datagram_sock)
	    uwp_sock->datagram_sock->MessageReceived -= event_token;

	SetEvent(recv_wait);
	CloseHandle(recv_wait);
    }

    PjUwpSocket* uwp_sock;
    DatagramSocketMessageReceivedEventArgs^ recv_args;
    EventRegistrationToken event_token;
    HANDLE recv_wait;
    int avail_data_len;
};


ref class PjUwpSocketListenerHelper sealed
{
internal:
    PjUwpSocketListenerHelper(PjUwpSocket* uwp_sock_) :
	uwp_sock(uwp_sock_), conn_args(nullptr)
    {
	conn_wait = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	event_token = uwp_sock->listener_sock->ConnectionReceived +=
	    ref new TypedEventHandler<StreamSocketListener^, StreamSocketListenerConnectionReceivedEventArgs^>
	    (this, &PjUwpSocketListenerHelper::OnConnectionReceived);
    }

    void OnConnectionReceived(StreamSocketListener ^sender,
	StreamSocketListenerConnectionReceivedEventArgs ^args)
    {
	try {
	    conn_args = args;

	    if (uwp_sock->cb.on_accept) {
		(*uwp_sock->cb.on_accept)(uwp_sock);
	    }

	    WaitForSingleObjectEx(conn_wait, INFINITE, false);
	} catch (Exception^ e) {}
    }

    bstatus_t GetAcceptedSocket(StreamSocket^& stream_sock)
    {
	if (conn_args == nullptr)
	    return BASE_ENOTFOUND;

	stream_sock = conn_args->Socket;

	// finally
	conn_args = nullptr;
	SetEvent(conn_wait);

	return BASE_SUCCESS;
    }

private:

    ~PjUwpSocketListenerHelper()
    {
	if (uwp_sock->listener_sock)
	    uwp_sock->listener_sock->ConnectionReceived -= event_token;

	SetEvent(conn_wait);
	CloseHandle(conn_wait);
    }

    PjUwpSocket* uwp_sock;
    StreamSocketListenerConnectionReceivedEventArgs^ conn_args;
    EventRegistrationToken event_token;
    HANDLE conn_wait;
};


PjUwpSocket::PjUwpSocket(int af_, int type_, int proto_) :
    af(af_), type(type_), proto(proto_),
    sock_type(SOCKTYPE_UNKNOWN),
    sock_state(SOCKSTATE_NULL),
    is_blocking(BASE_TRUE),
    has_pending_bind(BASE_FALSE),
    has_pending_send(BASE_FALSE),
    has_pending_recv(BASE_FALSE)
{
    bsockaddr_init(bAF_INET(), &local_addr, NULL, 0);
    bsockaddr_init(bAF_INET(), &remote_addr, NULL, 0);
}

PjUwpSocket::~PjUwpSocket()
{
    DeinitSocket();
}

PjUwpSocket* PjUwpSocket::CreateAcceptSocket(Windows::Networking::Sockets::StreamSocket^ stream_sock_)
{
    PjUwpSocket *new_sock = new PjUwpSocket(bAF_INET(), bSOCK_STREAM(), 0);
    new_sock->stream_sock = stream_sock_;
    new_sock->sock_type = SOCKTYPE_STREAM;
    new_sock->sock_state = SOCKSTATE_CONNECTED;
    new_sock->socket_reader = ref new DataReader(new_sock->stream_sock->InputStream);
    new_sock->socket_writer = ref new DataWriter(new_sock->stream_sock->OutputStream);
    new_sock->socket_reader->InputStreamOptions = InputStreamOptions::Partial;
    new_sock->send_buffer = ref new Buffer(SEND_BUFFER_SIZE);
    new_sock->is_blocking = is_blocking;

    // Update local & remote address
    wstr_addr_to_sockaddr(stream_sock_->Information->RemoteAddress->CanonicalName->Data(),
			  stream_sock_->Information->RemotePort->Data(),
			  &new_sock->remote_addr);
    wstr_addr_to_sockaddr(stream_sock_->Information->LocalAddress->CanonicalName->Data(),
			  stream_sock_->Information->LocalPort->Data(),
			  &new_sock->local_addr);

    return new_sock;
}


bstatus_t PjUwpSocket::InitSocket(enum PjUwpSocketType sock_type_)
{
    BASE_ASSERT_RETURN(sock_type_ > SOCKTYPE_UNKNOWN, BASE_EINVAL);

    sock_type = sock_type_;
    if (sock_type == SOCKTYPE_LISTENER) {
	listener_sock = ref new StreamSocketListener();
    } else if (sock_type == SOCKTYPE_STREAM) {
	stream_sock = ref new StreamSocket();
    } else if (sock_type == SOCKTYPE_DATAGRAM) {
	datagram_sock = ref new DatagramSocket();
    } else {
	bassert(!"Invalid socket type");
	return BASE_EINVAL;
    }

    if (sock_type == SOCKTYPE_DATAGRAM || sock_type == SOCKTYPE_STREAM) {
	send_buffer = ref new Buffer(SEND_BUFFER_SIZE);
    }

    sock_state = SOCKSTATE_INITIALIZED;

    return BASE_SUCCESS;
}


void PjUwpSocket::DeinitSocket()
{
    if (stream_sock) {
	concurrency::create_task(stream_sock->CancelIOAsync()).wait();
    }
    if (datagram_sock && has_pending_send) {
	concurrency::create_task(datagram_sock->CancelIOAsync()).wait();
    }
    if (listener_sock) {
	concurrency::create_task(listener_sock->CancelIOAsync()).wait();
    }
    while (has_pending_recv) bthreadSleepMs(10);

    stream_sock = nullptr;
    datagram_sock = nullptr;
    dgram_recv_helper = nullptr;
    listener_sock = nullptr;
    listener_helper = nullptr;
    socket_writer = nullptr;
    socket_reader = nullptr;
    sock_state = SOCKSTATE_NULL;
}

bstatus_t PjUwpSocket::Bind(const bsockaddr_t *addr)
{
    /* Not initialized yet, socket type is perhaps TCP, just not decided yet
     * whether it is a stream or a listener.
     */
    if (sock_state < SOCKSTATE_INITIALIZED) {
	bsockaddr_cp(&local_addr, addr);
	has_pending_bind = BASE_TRUE;
	return BASE_SUCCESS;
    }
    
    BASE_ASSERT_RETURN(sock_state == SOCKSTATE_INITIALIZED, BASE_EINVALIDOP);
    if (sock_type != SOCKTYPE_DATAGRAM && sock_type != SOCKTYPE_LISTENER)
	return BASE_EINVALIDOP;

    if (has_pending_bind) {
	has_pending_bind = BASE_FALSE;
	if (!addr)
	    addr = &local_addr;
    }

    /* If no bound address is set, just return */
    if (!bsockaddr_has_addr(addr) && !bsockaddr_get_port(addr))
	return BASE_SUCCESS;

    if (addr != &local_addr)
	bsockaddr_cp(&local_addr, addr);

    HRESULT err = 0;
    concurrency::create_task([this, addr, &err]() {
	HostName ^host;
	int port;

	sockaddr_to_hostname_port(addr, host, &port);
	if (bsockaddr_has_addr(addr)) {
	    if (sock_type == SOCKTYPE_DATAGRAM)
		return datagram_sock->BindEndpointAsync(host, port.ToString());
	    else
		return listener_sock->BindEndpointAsync(host, port.ToString());
	} else /* if (bsockaddr_get_port(addr) != 0) */ {
	    if (sock_type == SOCKTYPE_DATAGRAM)
		return datagram_sock->BindServiceNameAsync(port.ToString());
	    else
		return listener_sock->BindServiceNameAsync(port.ToString());
	}
    }).then([this, &err](concurrency::task<void> t)
    {
	try {
	    if (!err)
		t.get();
	} catch (Exception^ e) {
	    err = e->HResult;
	}
    }).get();

    return (err? BASE_RETURN_OS_ERROR(err) : BASE_SUCCESS);
}


bstatus_t PjUwpSocket::SendImp(const void *buf, bssize_t *len)
{
    if (has_pending_send)
	return BASE_RETURN_OS_ERROR(BASE_BLOCKING_ERROR_VAL);

    if (*len > (bssize_t)send_buffer->Capacity)
	return BASE_ETOOBIG;

    CopyToIBuffer((unsigned char*)buf, *len, send_buffer);
    send_buffer->Length = *len;
    socket_writer->WriteBuffer(send_buffer);

    /* Blocking version */
    if (is_blocking) {
	bstatus_t status = BASE_SUCCESS;
	concurrency::cancellation_token_source cts;
	auto cts_token = cts.get_token();
	auto t = concurrency::create_task(socket_writer->StoreAsync(),
					  cts_token);
	*len = cancel_after_timeout(t, cts, (unsigned int)WRITE_TIMEOUT).
	    then([cts_token, &status](concurrency::task<unsigned int> t_)
	{
	    int sent = 0;
	    try {
		if (cts_token.is_canceled())
		    status = BASE_ETIMEDOUT;
		else
		    sent = t_.get();
	    } catch (Exception^ e) {
		status = BASE_RETURN_OS_ERROR(e->HResult);
	    }
	    return sent;
	}).get();

	return status;
    } 

    /* Non-blocking version */
    has_pending_send = BASE_TRUE;
    concurrency::create_task(socket_writer->StoreAsync()).
	then([this](concurrency::task<unsigned int> t_)
    {
	try {
	    unsigned int l = t_.get();
	    has_pending_send = BASE_FALSE;

	    // invoke callback
	    if (cb.on_write) {
		(*cb.on_write)(this, l);
	    }
	} catch (...) {
	    has_pending_send = BASE_FALSE;
	    sock_state = SOCKSTATE_ERROR;
	    DeinitSocket();

	    // invoke callback
	    if (cb.on_write) {
		(*cb.on_write)(this, -BASE_EUNKNOWN);
	    }
	}
    });

    return BASE_SUCCESS;
}


bstatus_t PjUwpSocket::Send(const void *buf, bssize_t *len)
{
    if ((sock_type!=SOCKTYPE_STREAM && sock_type!=SOCKTYPE_DATAGRAM) ||
	(sock_state!=SOCKSTATE_CONNECTED))
    {
	return BASE_EINVALIDOP;
    }

    /* Sending for SOCKTYPE_DATAGRAM is implemented in bsock_sendto() */
    if (sock_type == SOCKTYPE_DATAGRAM) {
	return SendTo(buf, len, &remote_addr);
    }

    return SendImp(buf, len);
}


bstatus_t PjUwpSocket::SendTo(const void *buf, bssize_t *len,
				const bsockaddr_t *to)
{
    if (sock_type != SOCKTYPE_DATAGRAM || sock_state < SOCKSTATE_INITIALIZED
	|| sock_state >= SOCKSTATE_DISCONNECTED)
    {
	return BASE_EINVALIDOP;
    }

    if (has_pending_send)
	return BASE_RETURN_OS_ERROR(BASE_BLOCKING_ERROR_VAL);

    if (*len > (bssize_t)send_buffer->Capacity)
	return BASE_ETOOBIG;

    HostName ^hostname;
    int port;
    sockaddr_to_hostname_port(to, hostname, &port);

    concurrency::cancellation_token_source cts;
    auto cts_token = cts.get_token();
    auto t = concurrency::create_task(datagram_sock->GetOutputStreamAsync(
				      hostname, port.ToString()), cts_token);
    bstatus_t status = BASE_SUCCESS;

    cancel_after_timeout(t, cts, (unsigned int)WRITE_TIMEOUT).
	then([this, cts_token, &status](concurrency::task<IOutputStream^> t_)
    {
	try {
	    if (cts_token.is_canceled()) {
		status = BASE_ETIMEDOUT;
	    } else {
		IOutputStream^ outstream = t_.get();
		socket_writer = ref new DataWriter(outstream);
	    }
	} catch (Exception^ e) {
	    status = BASE_RETURN_OS_ERROR(e->HResult);
	}
    }).get();

    if (status != BASE_SUCCESS)
	return status;

    status = SendImp(buf, len);
    if ((status == BASE_SUCCESS || status == BASE_EPENDING) &&
	sock_state < SOCKSTATE_CONNECTED)
    {
	sock_state = SOCKSTATE_CONNECTED;
    }

    return status;
}


int PjUwpSocket::ConsumeReadBuffer(void *buf, int max_len)
{
    if (socket_reader->UnconsumedBufferLength == 0)
	return 0;

    int read_len = BASE_MIN((int)socket_reader->UnconsumedBufferLength,max_len);
    IBuffer^ buffer = socket_reader->ReadBuffer(read_len);
    read_len = buffer->Length;
    CopyFromIBuffer((unsigned char*)buf, read_len, buffer);
    return read_len;
}


bstatus_t PjUwpSocket::Recv(void *buf, bssize_t *len)
{
    /* Only for TCP, at least for now! */
    if (sock_type == SOCKTYPE_DATAGRAM)
	return BASE_ENOTSUP;

    if (sock_type != SOCKTYPE_STREAM || sock_state != SOCKSTATE_CONNECTED)
	return BASE_EINVALIDOP;

    if (has_pending_recv)
	return BASE_RETURN_OS_ERROR(BASE_BLOCKING_ERROR_VAL);

    /* First check if there is already some data in the read buffer */
    if (buf) {
	int avail_len = ConsumeReadBuffer(buf, *len);
	if (avail_len > 0) {
	    *len = avail_len;
	    return BASE_SUCCESS;
	}
    }

    /* Blocking version */
    if (is_blocking) {
	bstatus_t status = BASE_SUCCESS;
	concurrency::cancellation_token_source cts;
	auto cts_token = cts.get_token();
	auto t = concurrency::create_task(socket_reader->LoadAsync(*len),
					  cts_token);
	*len = cancel_after_timeout(t, cts, READ_TIMEOUT)
	    .then([this, len, buf, cts_token, &status]
				    (concurrency::task<unsigned int> t_)
	{
	    try {
		if (cts_token.is_canceled()) {
		    status = BASE_ETIMEDOUT;
		    return 0;
		}
		t_.get();
	    } catch (Exception^) {
		status = BASE_ETIMEDOUT;
		return 0;
	    }

	    *len = ConsumeReadBuffer(buf, *len);
	    return (int)*len;
	}).get();

	return status;
    }

    /* Non-blocking version */

    concurrency::cancellation_token_source cts;
    auto cts_token = cts.get_token();

    has_pending_recv = BASE_TRUE;
    concurrency::create_task(socket_reader->LoadAsync(*len), cts_token)
	.then([this, cts_token](concurrency::task<unsigned int> t_)
    {
	try {
	    if (cts_token.is_canceled()) {
		has_pending_recv = BASE_FALSE;

		// invoke callback
		if (cb.on_read) {
		    (*cb.on_read)(this, -BASE_EUNKNOWN);
		}
		return;
	    }

	    t_.get();
	    has_pending_recv = BASE_FALSE;

	    // invoke callback
	    int read_len = socket_reader->UnconsumedBufferLength;
	    if (read_len > 0 && cb.on_read) {
		(*cb.on_read)(this, read_len);
	    }
	} catch (...) {
	    has_pending_recv = BASE_FALSE;

	    // invoke callback
	    if (cb.on_read) {
		(*cb.on_read)(this, -BASE_EUNKNOWN);
	    }
	}
    });

    return BASE_RETURN_OS_ERROR(BASE_BLOCKING_ERROR_VAL);
}


bstatus_t PjUwpSocket::RecvFrom(void *buf, bssize_t *len,
				  bsockaddr_t *from)
{
    if (sock_type != SOCKTYPE_DATAGRAM || sock_state < SOCKSTATE_INITIALIZED
	|| sock_state >= SOCKSTATE_DISCONNECTED)
    {
	return BASE_EINVALIDOP;
    }

    /* Start receive, if not yet */
    if (dgram_recv_helper == nullptr) {
	dgram_recv_helper = ref new PjUwpSocketDatagramRecvHelper(this);
    }

    /* Try to read any available data first */
    if (buf || is_blocking) {
	bstatus_t status;
	status = dgram_recv_helper->ReadDataIfAvailable(buf, len, from);
	if (status != BASE_ENOTFOUND)
	    return status;
    }

    /* Blocking version */
    if (is_blocking) {
	int max_loop = 0;
	bstatus_t status = BASE_ENOTFOUND;
	while (status == BASE_ENOTFOUND && sock_state <= SOCKSTATE_CONNECTED)
	{
	    status = dgram_recv_helper->ReadDataIfAvailable(buf, len, from);
	    if (status != BASE_SUCCESS)
		bthreadSleepMs(100);

	    if (++max_loop > 10)
		return BASE_ETIMEDOUT;
	}
	return status;
    }

    /* For non-blocking version, just return BASE_EPENDING */
    return BASE_RETURN_OS_ERROR(BASE_BLOCKING_ERROR_VAL);
}


bstatus_t PjUwpSocket::Connect(const bsockaddr_t *addr)
{
    bstatus_t status;

    BASE_ASSERT_RETURN((sock_type == SOCKTYPE_UNKNOWN && sock_state == SOCKSTATE_NULL) ||
		     (sock_type == SOCKTYPE_DATAGRAM && sock_state == SOCKSTATE_INITIALIZED),
		     BASE_EINVALIDOP);

    if (sock_type == SOCKTYPE_UNKNOWN) {
	InitSocket(SOCKTYPE_STREAM);
	// No need to check pending bind, no bind for TCP client socket
    }

    bsockaddr_cp(&remote_addr, addr);

    auto t = concurrency::create_task([this, addr]()
    {
	HostName ^hostname;
	int port;
	sockaddr_to_hostname_port(&remote_addr, hostname, &port);
	if (sock_type == SOCKTYPE_STREAM)
	    return stream_sock->ConnectAsync(hostname, port.ToString(),
				      SocketProtectionLevel::PlainSocket);
	else
	    return datagram_sock->ConnectAsync(hostname, port.ToString());
    }).then([=](concurrency::task<void> t_)
    {
	try {
	    t_.get();

	    sock_state = SOCKSTATE_CONNECTED;

	    // Update local & remote address
	    HostName^ local_address;
	    String^ local_port;

	    if (sock_type == SOCKTYPE_STREAM) {
		local_address = stream_sock->Information->LocalAddress;
		local_port = stream_sock->Information->LocalPort;

		socket_reader = ref new DataReader(stream_sock->InputStream);
		socket_writer = ref new DataWriter(stream_sock->OutputStream);
		socket_reader->InputStreamOptions = InputStreamOptions::Partial;
	    } else {
		local_address = datagram_sock->Information->LocalAddress;
		local_port = datagram_sock->Information->LocalPort;
	    }
	    if (local_address && local_port) {
		wstr_addr_to_sockaddr(local_address->CanonicalName->Data(),
		    local_port->Data(),
		    &local_addr);
	    }

	    if (!is_blocking && cb.on_connect) {
		(*cb.on_connect)(this, BASE_SUCCESS);
	    }
	    return (bstatus_t)BASE_SUCCESS;

	} catch (Exception^ ex) {

	    SocketErrorStatus status = SocketError::GetStatus(ex->HResult);

	    switch (status)
	    {
	    case SocketErrorStatus::UnreachableHost:
		break;
	    case SocketErrorStatus::ConnectionTimedOut:
		break;
	    case SocketErrorStatus::ConnectionRefused:
		break;
	    default:
		break;
	    }

	    if (!is_blocking && cb.on_connect) {
		(*cb.on_connect)(this, BASE_EUNKNOWN);
	    }

	    return (bstatus_t)BASE_EUNKNOWN;
	}
    });

    if (!is_blocking)
	return BASE_RETURN_OS_ERROR(BASE_BLOCKING_CONNECT_ERROR_VAL);

    try {
	status = t.get();
    } catch (Exception^) {
	return BASE_EUNKNOWN;
    }
    return status;
}

bstatus_t PjUwpSocket::Listen()
{
    BASE_ASSERT_RETURN((sock_type == SOCKTYPE_UNKNOWN) ||
		     (sock_type == SOCKTYPE_LISTENER &&
		      sock_state == SOCKSTATE_INITIALIZED),
		     BASE_EINVALIDOP);

    if (sock_type == SOCKTYPE_UNKNOWN)
	InitSocket(SOCKTYPE_LISTENER);

    if (has_pending_bind)
	Bind();

    /* Start listen */
    if (listener_helper == nullptr) {
	listener_helper = ref new PjUwpSocketListenerHelper(this);
    }

    return BASE_SUCCESS;
}

bstatus_t PjUwpSocket::Accept(PjUwpSocket **new_sock)
{
    if (sock_type != SOCKTYPE_LISTENER || sock_state != SOCKSTATE_INITIALIZED)
	return BASE_EINVALIDOP;

    StreamSocket^ accepted_sock;
    bstatus_t status = listener_helper->GetAcceptedSocket(accepted_sock);
    if (status == BASE_ENOTFOUND)
	return BASE_RETURN_OS_ERROR(BASE_BLOCKING_ERROR_VAL);

    if (status != BASE_SUCCESS)
	return status;

    *new_sock = CreateAcceptSocket(accepted_sock);
    return BASE_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////////
//
// 's sock.h implementation
//

/*
 * Convert 16-bit value from network byte order to host byte order.
 */
buint16_t bntohs(buint16_t netshort)
{
    return ntohs(netshort);
}

/*
 * Convert 16-bit value from host byte order to network byte order.
 */
buint16_t bhtons(buint16_t hostshort)
{
    return htons(hostshort);
}

/*
 * Convert 32-bit value from network byte order to host byte order.
 */
buint32_t bntohl(buint32_t netlong)
{
    return ntohl(netlong);
}

/*
 * Convert 32-bit value from host byte order to network byte order.
 */
buint32_t bhtonl(buint32_t hostlong)
{
    return htonl(hostlong);
}

/*
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 */
char* binet_ntoa(bin_addr inaddr)
{
    return inet_ntoa(*(struct in_addr*)&inaddr);
}

/*
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
int binet_aton(const bstr_t *cp,  bin_addr *inp)
{
    char tempaddr[BASE_INET_ADDRSTRLEN];

    /* Initialize output with BASE_INADDR_NONE.
    * Some apps relies on this instead of the return value
    * (and anyway the return value is quite confusing!)
    */
    inp->s_addr = BASE_INADDR_NONE;

    /* Caution:
    *	this function might be called with cp->slen >= 16
    *  (i.e. when called with hostname to check if it's an IP addr).
    */
    BASE_ASSERT_RETURN(cp && cp->slen && inp, 0);
    if (cp->slen >= BASE_INET_ADDRSTRLEN) {
	return 0;
    }

    bmemcpy(tempaddr, cp->ptr, cp->slen);
    tempaddr[cp->slen] = '\0';

#if defined(BASE_SOCK_HAS_INET_ATON) && BASE_SOCK_HAS_INET_ATON != 0
    return inet_aton(tempaddr, (struct in_addr*)inp);
#else
    inp->s_addr = inet_addr(tempaddr);
    return inp->s_addr == BASE_INADDR_NONE ? 0 : 1;
#endif
}

/*
 * Convert text to IPv4/IPv6 address.
 */
bstatus_t binet_pton(int af, const bstr_t *src, void *dst)
{
    char tempaddr[BASE_INET6_ADDRSTRLEN];

    BASE_ASSERT_RETURN(af == BASE_AF_INET || af == BASE_AF_INET6, BASE_EAFNOTSUP);
    BASE_ASSERT_RETURN(src && src->slen && dst, BASE_EINVAL);

    /* Initialize output with BASE_IN_ADDR_NONE for IPv4 (to be
    * compatible with binet_aton()
    */
    if (af == BASE_AF_INET) {
	((bin_addr*)dst)->s_addr = BASE_INADDR_NONE;
    }

    /* Caution:
    *	this function might be called with cp->slen >= 46
    *  (i.e. when called with hostname to check if it's an IP addr).
    */
    if (src->slen >= BASE_INET6_ADDRSTRLEN) {
	return BASE_ENAMETOOLONG;
    }

    bmemcpy(tempaddr, src->ptr, src->slen);
    tempaddr[src->slen] = '\0';

#if defined(BASE_SOCK_HAS_INET_PTON) && BASE_SOCK_HAS_INET_PTON != 0
    /*
    * Implementation using inet_pton()
    */
    if (inet_pton(af, tempaddr, dst) != 1) {
	bstatus_t status = bget_netos_error();
	if (status == BASE_SUCCESS)
	    status = BASE_EUNKNOWN;

	return status;
    }

    return BASE_SUCCESS;

#elif defined(BASE_WIN32) || defined(BASE_WIN64) || defined(BASE_WIN32_WINCE)
    /*
    * Implementation on Windows, using WSAStringToAddress().
    * Should also work on Unicode systems.
    */
    {
	BASE_DECL_UNICODE_TEMP_BUF(wtempaddr, BASE_INET6_ADDRSTRLEN)
	    bsockaddr sock_addr;
	int addr_len = sizeof(sock_addr);
	int rc;

	sock_addr.addr.sa_family = (buint16_t)af;
	rc = WSAStringToAddress(
	    BASE_STRING_TO_NATIVE(tempaddr, wtempaddr, sizeof(wtempaddr)),
	    af, NULL, (LPSOCKADDR)&sock_addr, &addr_len);
	if (rc != 0) {
	    /* If you get rc 130022 Invalid argument (WSAEINVAL) with IPv6,
	    * check that you have IPv6 enabled (install it in the network
	    * adapter).
	    */
	    bstatus_t status = bget_netos_error();
	    if (status == BASE_SUCCESS)
		status = BASE_EUNKNOWN;

	    return status;
	}

	if (sock_addr.addr.sa_family == BASE_AF_INET) {
	    bmemcpy(dst, &sock_addr.ipv4.sin_addr, 4);
	    return BASE_SUCCESS;
	}
	else if (sock_addr.addr.sa_family == BASE_AF_INET6) {
	    bmemcpy(dst, &sock_addr.ipv6.sin6_addr, 16);
	    return BASE_SUCCESS;
	}
	else {
	    bassert(!"Shouldn't happen");
	    return BASE_EBUG;
	}
    }
#elif !defined(BASE_HAS_IPV6) || BASE_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return BASE_EIPV6NOTSUP;
#else
    bassert(!"Not supported");
    return BASE_EIPV6NOTSUP;
#endif
}

/*
 * Convert IPv4/IPv6 address to text.
 */
bstatus_t binet_ntop(int af, const void *src,
				 char *dst, int size)

{
    BASE_ASSERT_RETURN(src && dst && size, BASE_EINVAL);
    BASE_ASSERT_RETURN(af == BASE_AF_INET || af == BASE_AF_INET6, BASE_EAFNOTSUP);

    *dst = '\0';

#if defined(BASE_SOCK_HAS_INET_NTOP) && BASE_SOCK_HAS_INET_NTOP != 0
    /*
    * Implementation using inet_ntop()
    */
    if (inet_ntop(af, src, dst, size) == NULL) {
	bstatus_t status = bget_netos_error();
	if (status == BASE_SUCCESS)
	    status = BASE_EUNKNOWN;

	return status;
    }

    return BASE_SUCCESS;

#elif defined(BASE_WIN32) || defined(BASE_WIN64) || defined(BASE_WIN32_WINCE)
    /*
    * Implementation on Windows, using WSAAddressToString().
    * Should also work on Unicode systems.
    */
    {
	BASE_DECL_UNICODE_TEMP_BUF(wtempaddr, BASE_INET6_ADDRSTRLEN)
	    bsockaddr sock_addr;
	DWORD addr_len, addr_str_len;
	int rc;

	bbzero(&sock_addr, sizeof(sock_addr));
	sock_addr.addr.sa_family = (buint16_t)af;
	if (af == BASE_AF_INET) {
	    if (size < BASE_INET_ADDRSTRLEN)
		return BASE_ETOOSMALL;
	    bmemcpy(&sock_addr.ipv4.sin_addr, src, 4);
	    addr_len = sizeof(bsockaddr_in);
	    addr_str_len = BASE_INET_ADDRSTRLEN;
	}
	else if (af == BASE_AF_INET6) {
	    if (size < BASE_INET6_ADDRSTRLEN)
		return BASE_ETOOSMALL;
	    bmemcpy(&sock_addr.ipv6.sin6_addr, src, 16);
	    addr_len = sizeof(bsockaddr_in6);
	    addr_str_len = BASE_INET6_ADDRSTRLEN;
	}
	else {
	    bassert(!"Unsupported address family");
	    return BASE_EAFNOTSUP;
	}

#if BASE_NATIVE_STRING_IS_UNICODE
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
	    NULL, wtempaddr, &addr_str_len);
	if (rc == 0) {
	    bunicode_to_ansi(wtempaddr, wcslen(wtempaddr), dst, size);
	}
#else
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
	    NULL, dst, &addr_str_len);
#endif

	if (rc != 0) {
	    bstatus_t status = bget_netos_error();
	    if (status == BASE_SUCCESS)
		status = BASE_EUNKNOWN;

	    return status;
	}

	return BASE_SUCCESS;
    }

#elif !defined(BASE_HAS_IPV6) || BASE_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return BASE_EIPV6NOTSUP;
#else
    bassert(!"Not supported");
    return BASE_EIPV6NOTSUP;
#endif
}

/*
 * Get hostname.
 */
const bstr_t* bgethostname(void)
{
    static char buf[BASE_MAX_HOSTNAME];
    static bstr_t hostname;

    BASE_CHECK_STACK();

    if (hostname.ptr == NULL) {
	hostname.ptr = buf;
	if (gethostname(buf, sizeof(buf)) != 0) {
	    hostname.ptr[0] = '\0';
	    hostname.slen = 0;
	}
	else {
	    hostname.slen = strlen(buf);
	}
    }
    return &hostname;
}

/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
bstatus_t bsock_socket(int af, 
				   int type, 
				   int proto,
				   bsock_t *p_sock)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(p_sock!=NULL, BASE_EINVAL);

    PjUwpSocket *s = new PjUwpSocket(af, type, proto);
    
    /* Init UDP socket here */
    if (type == bSOCK_DGRAM()) {
	s->InitSocket(SOCKTYPE_DATAGRAM);
    }

    *p_sock = (bsock_t)s;
    return BASE_SUCCESS;
}


/*
 * Bind socket.
 */
bstatus_t bsock_bind( bsock_t sock, 
				  const bsockaddr_t *addr,
				  int len)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock, BASE_EINVAL);
    BASE_ASSERT_RETURN(addr && len>=(int)sizeof(bsockaddr_in), BASE_EINVAL);
    PjUwpSocket *s = (PjUwpSocket*)sock;
    return s->Bind(addr);
}


/*
 * Bind socket.
 */
bstatus_t bsock_bind_in( bsock_t sock, 
				     buint32_t addr32,
				     buint16_t port)
{
    bsockaddr_in addr;

    BASE_CHECK_STACK();

    bbzero(&addr, sizeof(addr));
    addr.sin_family = BASE_AF_INET;
    addr.sin_addr.s_addr = bhtonl(addr32);
    addr.sin_port = bhtons(port);

    return bsock_bind(sock, &addr, sizeof(bsockaddr_in));
}


/*
 * Close socket.
 */
bstatus_t bsock_close(bsock_t sock)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock, BASE_EINVAL);

    if (sock == BASE_INVALID_SOCKET)
	return BASE_SUCCESS;

    PjUwpSocket *s = (PjUwpSocket*)sock;
    delete s;

    return BASE_SUCCESS;
}

/*
 * Get remote's name.
 */
bstatus_t bsock_getpeername( bsock_t sock,
					 bsockaddr_t *addr,
					 int *namelen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock && addr && namelen && 
		     *namelen>=(int)sizeof(bsockaddr_in), BASE_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    bsockaddr_cp(addr, s->GetRemoteAddr());
    *namelen = bsockaddr_get_len(addr);

    return BASE_SUCCESS;
}

/*
 * Get socket name.
 */
bstatus_t bsock_getsockname( bsock_t sock,
					 bsockaddr_t *addr,
					 int *namelen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock && addr && namelen && 
		     *namelen>=(int)sizeof(bsockaddr_in), BASE_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    bsockaddr_cp(addr, s->GetLocalAddr());
    *namelen = bsockaddr_get_len(addr);

    return BASE_SUCCESS;
}


/*
 * Send data
 */
bstatus_t bsock_send(bsock_t sock,
				 const void *buf,
				 bssize_t *len,
				 unsigned flags)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock && buf && len, BASE_EINVAL);
    BASE_UNUSED_ARG(flags);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    return s->Send(buf, len);
}


/*
 * Send data.
 */
bstatus_t bsock_sendto(bsock_t sock,
				   const void *buf,
				   bssize_t *len,
				   unsigned flags,
				   const bsockaddr_t *to,
				   int tolen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock && buf && len, BASE_EINVAL);
    BASE_UNUSED_ARG(flags);
    BASE_UNUSED_ARG(tolen);
    
    PjUwpSocket *s = (PjUwpSocket*)sock;
    return s->SendTo(buf, len, to);
}


/*
 * Receive data.
 */
bstatus_t bsock_recv(bsock_t sock,
				 void *buf,
				 bssize_t *len,
				 unsigned flags)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock && len && *len > 0, BASE_EINVAL);
    
    BASE_UNUSED_ARG(flags);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    return s->Recv(buf, len);
}

/*
 * Receive data.
 */
bstatus_t bsock_recvfrom(bsock_t sock,
				     void *buf,
				     bssize_t *len,
				     unsigned flags,
				     bsockaddr_t *from,
				     int *fromlen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock && buf && len && from && fromlen, BASE_EINVAL);
    BASE_ASSERT_RETURN(*len > 0, BASE_EINVAL);
    BASE_ASSERT_RETURN(*fromlen >= (int)sizeof(bsockaddr_in), BASE_EINVAL);

    BASE_UNUSED_ARG(flags);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    bstatus_t status = s->RecvFrom(buf, len, from);
    if (status == BASE_SUCCESS)
	*fromlen = bsockaddr_get_len(from);
    return status;
}

/*
 * Get socket option.
 */
bstatus_t bsock_getsockopt( bsock_t sock,
					buint16_t level,
					buint16_t optname,
					void *optval,
					int *optlen)
{
    // Not supported for now.
    BASE_UNUSED_ARG(sock);
    BASE_UNUSED_ARG(level);
    BASE_UNUSED_ARG(optname);
    BASE_UNUSED_ARG(optval);
    BASE_UNUSED_ARG(optlen);
    return BASE_ENOTSUP;
}


/*
 * Set socket option.
 */
bstatus_t bsock_setsockopt( bsock_t sock,
					buint16_t level,
					buint16_t optname,
					const void *optval,
					int optlen)
{
    // Not supported for now.
    BASE_UNUSED_ARG(sock);
    BASE_UNUSED_ARG(level);
    BASE_UNUSED_ARG(optname);
    BASE_UNUSED_ARG(optval);
    BASE_UNUSED_ARG(optlen);
    return BASE_ENOTSUP;
}


/*
* Set socket option.
*/
bstatus_t bsock_setsockopt_params( bsock_t sockfd,
    const bsockopt_params *params)
{
    unsigned int i = 0;
    bstatus_t retval = BASE_SUCCESS;
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(params, BASE_EINVAL);

    for (;i<params->cnt && i<BASE_MAX_SOCKOPT_PARAMS;++i) {
	bstatus_t status = bsock_setsockopt(sockfd, 
				    (buint16_t)params->options[i].level,
				    (buint16_t)params->options[i].optname,
				    params->options[i].optval, 
				    params->options[i].optlen);
	if (status != BASE_SUCCESS) {
	    retval = status;
	    BASE_PERROR(4,(THIS_FILE, status,
			 "Warning: error applying sock opt %d",
			 params->options[i].optname));
	}
    }

    return retval;
}


/*
 * Connect socket.
 */
bstatus_t bsock_connect( bsock_t sock,
				     const bsockaddr_t *addr,
				     int namelen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock && addr, BASE_EINVAL);
    BASE_UNUSED_ARG(namelen);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    return s->Connect(addr);
}


/*
 * Shutdown socket.
 */
#if BASE_HAS_TCP
bstatus_t bsock_shutdown( bsock_t sock,
				      int how)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sock, BASE_EINVAL);
    BASE_UNUSED_ARG(how);

    return bsock_close(sock);
}

/*
 * Start listening to incoming connections.
 */
bstatus_t bsock_listen( bsock_t sock,
				    int backlog)
{
    BASE_CHECK_STACK();
    BASE_UNUSED_ARG(backlog);
    BASE_ASSERT_RETURN(sock, BASE_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    return s->Listen();
}

/*
 * Accept incoming connections
 */
bstatus_t bsock_accept( bsock_t serverfd,
				    bsock_t *newsock,
				    bsockaddr_t *addr,
				    int *addrlen)
{
    bstatus_t status;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(serverfd && newsock, BASE_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)serverfd;
    PjUwpSocket *new_uwp_sock;

    status = s->Accept(&new_uwp_sock);
    if (status != BASE_SUCCESS)
	return status;
    if (newsock == NULL)
	return BASE_ENOTFOUND;

    *newsock = (bsock_t)new_uwp_sock;

    if (addr)
	bsockaddr_cp(addr, new_uwp_sock->GetRemoteAddr());

    if (addrlen)
	*addrlen = bsockaddr_get_len(addr);

    return BASE_SUCCESS;
}
#endif	/* BASE_HAS_TCP */
