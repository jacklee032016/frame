/* 
 *
 */
#include <baseConfig.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* PMIB_ICMP_EX is not declared in VC6, causing error.
 * But EVC4, which also claims to be VC6, does have it! 
 */
#if defined(_MSC_VER) && _MSC_VER==1200 && !defined(BASE_WIN32_WINCE)
#   define PMIB_ICMP_EX void*
#endif
#include <winsock2.h>

/* If you encounter error "Cannot open include file: 'Iphlpapi.h' here,
 * you need to install newer Platform SDK. Presumably you're using
 * Microsoft Visual Studio 6?
 */
#include <Iphlpapi.h>

#include <baseIpHelper.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseString.h>

/* Dealing with Unicode quirks:

 There seems to be a difference with GetProcAddress() API signature between
 Windows (i.e. Win32) and Windows CE (e.g. Windows Mobile). On Windows, the
 API is declared as:

   FARPROC GetProcAddress(
     HMODULE hModule,
     LPCSTR lpProcName);
 
 while on Windows CE:

   FARPROC GetProcAddress(
     HMODULE hModule,
     LPCWSTR lpProcName);

 Notice the difference with lpProcName argument type. This means that on 
 Windows, even on Unicode Windows, the lpProcName always takes ANSI format, 
 while on Windows CE, the argument follows the UNICODE setting.

 Because of this, we use a different Unicode treatment here than the usual
 BASE_NATIVE_STRING_IS_UNICODE  setting (<baseUnicode.h>):
   - GPA_TEXT macro: convert literal string to platform's native literal 
         string
   - gpa_char: the platform native character type

 Note that "GPA" and "gpa" are abbreviations for GetProcAddress.
*/
#if defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0
    /* on CE, follow the  Unicode setting */
#   define GPA_TEXT(x)	BASE_T(x)
#   define gpa_char	bchar_t
#else
    /* on non-CE, always use ANSI format */
#   define GPA_TEXT(x)	x
#   define gpa_char	char
#endif


typedef DWORD (WINAPI *PFN_GetIpAddrTable)(PMIB_IPADDRTABLE pIpAddrTable, 
					   PULONG pdwSize, 
					   BOOL bOrder);
typedef DWORD (WINAPI *PFN_GetAdapterAddresses)(ULONG Family,
					        ULONG Flags,
					        PVOID Reserved,
					        PIP_ADAPTER_ADDRESSES AdapterAddresses,
					        PULONG SizePointer);
typedef DWORD (WINAPI *PFN_GetIpForwardTable)(PMIB_IPFORWARDTABLE pIpForwardTable,
					      PULONG pdwSize, 
					      BOOL bOrder);
typedef DWORD (WINAPI *PFN_GetIfEntry)(PMIB_IFROW pIfRow);

static HANDLE s_hDLL;
static PFN_GetIpAddrTable s_pfnGetIpAddrTable;
static PFN_GetAdapterAddresses s_pfnGetAdapterAddresses;
static PFN_GetIpForwardTable s_pfnGetIpForwardTable;
static PFN_GetIfEntry s_pfnGetIfEntry;


static void unload_iphlp_module(void *data)
{
	HANDLE _hDLL = (HANDLE )data;
	FreeLibrary(_hDLL);
	_hDLL = NULL;
	
	s_pfnGetIpAddrTable = NULL;
	s_pfnGetIpForwardTable = NULL;
	s_pfnGetIfEntry = NULL;
	s_pfnGetAdapterAddresses = NULL;
}

static FARPROC GetIpHlpApiProc(gpa_char *lpProcName)
{
	if(NULL == s_hDLL)
	{
		s_hDLL = LoadLibrary(BASE_T("IpHlpApi"));
		if(NULL != s_hDLL)
		{
			libBaseAtExit(&unload_iphlp_module, "WinIpHelpApi", s_hDLL);
		}
	}

	if(NULL != s_hDLL)
		return GetProcAddress(s_hDLL, lpProcName);

	return NULL;
}

static DWORD MyGetIpAddrTable(PMIB_IPADDRTABLE pIpAddrTable, 
			      PULONG pdwSize, 
			      BOOL bOrder)
{
    if(NULL == s_pfnGetIpAddrTable) {
	s_pfnGetIpAddrTable = (PFN_GetIpAddrTable) 
	    GetIpHlpApiProc(GPA_TEXT("GetIpAddrTable"));
    }
    
    if(NULL != s_pfnGetIpAddrTable) {
	return s_pfnGetIpAddrTable(pIpAddrTable, pdwSize, bOrder);
    }
    
    return ERROR_NOT_SUPPORTED;
}

static DWORD MyGetAdapterAddresses(ULONG Family,
				   ULONG Flags,
				   PVOID Reserved,
				   PIP_ADAPTER_ADDRESSES AdapterAddresses,
				   PULONG SizePointer)
{
    if(NULL == s_pfnGetAdapterAddresses) {
	s_pfnGetAdapterAddresses = (PFN_GetAdapterAddresses) 
	    GetIpHlpApiProc(GPA_TEXT("GetAdaptersAddresses"));
    }
    
    if(NULL != s_pfnGetAdapterAddresses) {
	return s_pfnGetAdapterAddresses(Family, Flags, Reserved,
					AdapterAddresses, SizePointer);
    }
    
    return ERROR_NOT_SUPPORTED;
}

#if BASE_IP_HELPER_IGNORE_LOOPBACK_IF
static DWORD MyGetIfEntry(MIB_IFROW *pIfRow)
{
    if(NULL == s_pfnGetIfEntry) {
	s_pfnGetIfEntry = (PFN_GetIfEntry) 
	    GetIpHlpApiProc(GPA_TEXT("GetIfEntry"));
    }
    
    if(NULL != s_pfnGetIfEntry) {
	return s_pfnGetIfEntry(pIfRow);
    }
    
    return ERROR_NOT_SUPPORTED;
}
#endif


static DWORD MyGetIpForwardTable(PMIB_IPFORWARDTABLE pIpForwardTable, 
				 PULONG pdwSize, 
				 BOOL bOrder)
{
    if(NULL == s_pfnGetIpForwardTable) {
	s_pfnGetIpForwardTable = (PFN_GetIpForwardTable) 
	    GetIpHlpApiProc(GPA_TEXT("GetIpForwardTable"));
    }
    
    if(NULL != s_pfnGetIpForwardTable) {
	return s_pfnGetIpForwardTable(pIpForwardTable, pdwSize, bOrder);
    }
    
    return ERROR_NOT_SUPPORTED;
}

/* Enumerate local IP interface using GetIpAddrTable()
 * for IPv4 addresses only.
 */
static bstatus_t enum_ipv4_interface(unsigned *p_cnt, bsockaddr ifs[])
{
    char ipTabBuff[512];
    MIB_IPADDRTABLE *pTab = (MIB_IPADDRTABLE*)ipTabBuff;
    ULONG tabSize = sizeof(ipTabBuff);
    unsigned i, count;
    DWORD rc = NO_ERROR;

    BASE_ASSERT_RETURN(p_cnt && ifs, BASE_EINVAL);

    /* Get IP address table */
    rc = MyGetIpAddrTable(pTab, &tabSize, FALSE);
    if (rc != NO_ERROR) {
	if (rc == ERROR_INSUFFICIENT_BUFFER) {
	    /* Retry with larger buffer */
	    pTab = (MIB_IPADDRTABLE*)malloc(tabSize);
	    if (pTab)
		rc = MyGetIpAddrTable(pTab, &tabSize, FALSE);
	}

	if (rc != NO_ERROR) {
	    if (pTab != (MIB_IPADDRTABLE*)ipTabBuff)
		free(pTab);
	    return BASE_RETURN_OS_ERROR(rc);
	}
    }

    /* Reset result */
    bbzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Now fill out the entries */
    count = (pTab->dwNumEntries < *p_cnt) ? pTab->dwNumEntries : *p_cnt;
    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	MIB_IFROW ifRow;

	/* Ignore 0.0.0.0 address (interface is down?) */
	if (pTab->table[i].dwAddr == 0)
	    continue;

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if ((bntohl(pTab->table[i].dwAddr) >> 24) == 0)
	    continue;

#if BASE_IP_HELPER_IGNORE_LOOPBACK_IF
	/* Investigate the type of this interface */
	bbzero(&ifRow, sizeof(ifRow));
	ifRow.dwIndex = pTab->table[i].dwIndex;
	if (MyGetIfEntry(&ifRow) != 0)
	    continue;

	if (ifRow.dwType == MIB_IF_TYPE_LOOPBACK)
	    continue;
#endif

	ifs[*p_cnt].ipv4.sin_family = BASE_AF_INET;
	ifs[*p_cnt].ipv4.sin_addr.s_addr = pTab->table[i].dwAddr;
	(*p_cnt)++;
    }

    if (pTab != (MIB_IPADDRTABLE*)ipTabBuff)
	free(pTab);

    return (*p_cnt) ? BASE_SUCCESS : BASE_ENOTFOUND;
}

/* Enumerate local IP interface using GetAdapterAddresses(),
 * which works for both IPv4 and IPv6.
 */
static bstatus_t enum_ipv4_ipv6_interface(int af,
					    unsigned *p_cnt,
					    bsockaddr ifs[])
{
    buint8_t buffer[600];
    IP_ADAPTER_ADDRESSES *adapter = (IP_ADAPTER_ADDRESSES*)buffer;
    void *adapterBuf = NULL;
    ULONG size = sizeof(buffer);
    ULONG flags;
    unsigned i;
    DWORD rc;

    flags = GAA_FLAG_SKIP_FRIENDLY_NAME |
	    GAA_FLAG_SKIP_DNS_SERVER |
	    GAA_FLAG_SKIP_MULTICAST;

    rc = MyGetAdapterAddresses(af, flags, NULL, adapter, &size);
    if (rc != ERROR_SUCCESS) {
	if (rc == ERROR_BUFFER_OVERFLOW) {
	    /* Retry with larger memory size */
	    adapterBuf = malloc(size);
	    adapter = (IP_ADAPTER_ADDRESSES*) adapterBuf;
	    if (adapter != NULL)
		rc = MyGetAdapterAddresses(af, flags, NULL, adapter, &size);
	} 

	if (rc != ERROR_SUCCESS) {
	    if (adapterBuf)
		free(adapterBuf);
	    return BASE_RETURN_OS_ERROR(rc);
	}
    }

    /* Reset result */
    bbzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Enumerate interface */
    for (i=0; i<*p_cnt && adapter; adapter = adapter->Next) {
	if (adapter->FirstUnicastAddress) {
	    SOCKET_ADDRESS *pAddr = &adapter->FirstUnicastAddress->Address;

	    /* Ignore address family which we didn't request, just in case */
	    if (pAddr->lpSockaddr->sa_family != BASE_AF_INET &&
		pAddr->lpSockaddr->sa_family != BASE_AF_INET6)
	    {
		continue;
	    }

	    /* Apply some filtering to known IPv4 unusable addresses */
	    if (pAddr->lpSockaddr->sa_family == BASE_AF_INET) {
		const bsockaddr_in *addr_in = 
		    (const bsockaddr_in*)pAddr->lpSockaddr;

		/* Ignore 0.0.0.0 address (interface is down?) */
		if (addr_in->sin_addr.s_addr == 0)
		    continue;

		/* Ignore 0.0.0.0/8 address. This is a special address
		 * which doesn't seem to have practical use.
		 */
		if ((bntohl(addr_in->sin_addr.s_addr) >> 24) == 0)
		    continue;
	    }

#if BASE_IP_HELPER_IGNORE_LOOPBACK_IF
	    /* Ignore loopback interfaces */
	    /* This should have been IF_TYPE_SOFTWARE_LOOPBACK according to
	     * MSDN, and this macro should have been declared in Ipifcons.h, 
	     * but some SDK versions don't have it.
	     */
	    if (adapter->IfType == MIB_IF_TYPE_LOOPBACK)
		continue;
#endif

	    /* Ignore down interface */
	    if (adapter->OperStatus != IfOperStatusUp)
		continue;

	    ifs[i].addr.sa_family = pAddr->lpSockaddr->sa_family;
	    bmemcpy(&ifs[i], pAddr->lpSockaddr, pAddr->iSockaddrLength);
	    ++i;
	}
    }

    if (adapterBuf)
	free(adapterBuf);

    *p_cnt = i;
    return (*p_cnt) ? BASE_SUCCESS : BASE_ENOTFOUND;
}


/*
 * Enumerate the local IP interface currently active in the host.
 */
bstatus_t benum_ip_interface(int af,  unsigned *p_cnt, bsockaddr ifs[])
{
	bstatus_t status = -1;

	BASE_ASSERT_RETURN(p_cnt && ifs, BASE_EINVAL);
	BASE_ASSERT_RETURN(af==BASE_AF_UNSPEC || af==BASE_AF_INET || af==BASE_AF_INET6, BASE_EAFNOTSUP);

	status = enum_ipv4_ipv6_interface(af, p_cnt, ifs);
	if (status != BASE_SUCCESS && (af==BASE_AF_INET || af==BASE_AF_UNSPEC))
		status = enum_ipv4_interface(p_cnt, ifs);
	return status;
}

/*
 * Enumerate the IP routing table for this host.
 */
bstatus_t benum_ip_route(unsigned *p_cnt,
				     bip_route_entry routes[])
{
    char ipTabBuff[1024];
    MIB_IPADDRTABLE *pIpTab;
    char rtabBuff[1024];
    MIB_IPFORWARDTABLE *prTab;
    ULONG tabSize;
    unsigned i, count;
    DWORD rc = NO_ERROR;

    BASE_ASSERT_RETURN(p_cnt && routes, BASE_EINVAL);

    pIpTab = (MIB_IPADDRTABLE *)ipTabBuff;
    prTab = (MIB_IPFORWARDTABLE *)rtabBuff;

    /* First get IP address table */
    tabSize = sizeof(ipTabBuff);
    rc = MyGetIpAddrTable(pIpTab, &tabSize, FALSE);
    if (rc != NO_ERROR)
	return BASE_RETURN_OS_ERROR(rc);

    /* Next get IP route table */
    tabSize = sizeof(rtabBuff);

    rc = MyGetIpForwardTable(prTab, &tabSize, 1);
    if (rc != NO_ERROR)
	return BASE_RETURN_OS_ERROR(rc);

    /* Reset routes */
    bbzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Now fill out the route entries */
    count = (prTab->dwNumEntries < *p_cnt) ? prTab->dwNumEntries : *p_cnt;
    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	unsigned j;

	/* Find interface entry */
	for (j=0; j<pIpTab->dwNumEntries; ++j) {
	    if (pIpTab->table[j].dwIndex == prTab->table[i].dwForwardIfIndex)
		break;
	}

	if (j==pIpTab->dwNumEntries)
	    continue;	/* Interface not found */

	routes[*p_cnt].ipv4.if_addr.s_addr = pIpTab->table[j].dwAddr;
	routes[*p_cnt].ipv4.dst_addr.s_addr = prTab->table[i].dwForwardDest;
	routes[*p_cnt].ipv4.mask.s_addr = prTab->table[i].dwForwardMask;

	(*p_cnt)++;
    }

    return BASE_SUCCESS;
}

