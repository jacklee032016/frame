/* 
 *
 */
#include <baseSockSelect.h>
#include <compat/socket.h>
#include <baseOs.h>
#include <baseAssert.h>
#include <baseErrno.h>

#if defined(BASE_HAS_STRING_H) && BASE_HAS_STRING_H!=0
#   include <string.h>
#endif

#if defined(BASE_HAS_SYS_TIME_H) && BASE_HAS_SYS_TIME_H!=0
#   include <sys/time.h>
#endif

#ifdef _MSC_VER
#   pragma warning(disable: 4018)    // Signed/unsigned mismatch in FD_*
#   pragma warning(disable: 4389)    // Signed/unsigned mismatch in FD_*
#endif

#define PART_FDSET(ps)		((fd_set*)&ps->data[1])
#define PART_FDSET_OR_NULL(ps)	(ps ? PART_FDSET(ps) : NULL)
#define PART_COUNT(ps)		(ps->data[0])

void BASE_FD_ZERO(bfd_set_t *fdsetp)
{
    BASE_CHECK_STACK();
    bassert(sizeof(bfd_set_t)-sizeof(bsock_t) >= sizeof(fd_set));

    FD_ZERO(PART_FDSET(fdsetp));
    PART_COUNT(fdsetp) = 0;
}


void BASE_FD_SET(bsock_t fd, bfd_set_t *fdsetp)
{
    BASE_CHECK_STACK();
    bassert(sizeof(bfd_set_t)-sizeof(bsock_t) >= sizeof(fd_set));

    if (!BASE_FD_ISSET(fd, fdsetp))
        ++PART_COUNT(fdsetp);
    FD_SET(fd, PART_FDSET(fdsetp));
}


void BASE_FD_CLR(bsock_t fd, bfd_set_t *fdsetp)
{
    BASE_CHECK_STACK();
    bassert(sizeof(bfd_set_t)-sizeof(bsock_t) >= sizeof(fd_set));

    if (BASE_FD_ISSET(fd, fdsetp))
        --PART_COUNT(fdsetp);
    FD_CLR(fd, PART_FDSET(fdsetp));
}


bbool_t BASE_FD_ISSET(bsock_t fd, const bfd_set_t *fdsetp)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sizeof(bfd_set_t)-sizeof(bsock_t) >= sizeof(fd_set),
                     0);

    return FD_ISSET(fd, PART_FDSET(fdsetp));
}

bsize_t BASE_FD_COUNT(const bfd_set_t *fdsetp)
{
    return PART_COUNT(fdsetp);
}

int bsock_select( int n, 
			    bfd_set_t *readfds, 
			    bfd_set_t *writefds,
			    bfd_set_t *exceptfds, 
			    const btime_val *timeout)
{
    struct timeval os_timeout, *p_os_timeout;

    BASE_CHECK_STACK();

    BASE_ASSERT_RETURN(sizeof(bfd_set_t)-sizeof(bsock_t) >= sizeof(fd_set),
                     BASE_EBUG);

    if (timeout) {
	os_timeout.tv_sec = timeout->sec;
	os_timeout.tv_usec = timeout->msec * 1000;
	p_os_timeout = &os_timeout;
    } else {
	p_os_timeout = NULL;
    }

    return select(n, PART_FDSET_OR_NULL(readfds), PART_FDSET_OR_NULL(writefds),
		  PART_FDSET_OR_NULL(exceptfds), p_os_timeout);
}

