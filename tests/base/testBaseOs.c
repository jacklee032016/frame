/* 
 *
 */
#include "testBaseTest.h"
#include <baseLog.h>
#include <baseOs.h>

#if INCLUDE_OS_TEST
static int _endiannessTest32(void)
{
	union t
	{
		buint32_t	u32;
		buint16_t	u16[2];
		buint8_t	u8[4];
	} t;

	BASE_INFO(" Testing endianness..");

	t.u32 = 0x11223344;

#if defined(BASE_IS_LITTLE_ENDIAN) && BASE_IS_LITTLE_ENDIAN
	BASE_INFO("   Library is set to little endian");

#if defined(BASE_IS_BIG_ENDIAN) && BASE_IS_BIG_ENDIAN
	#error Error: Both BASE_IS_LITTLE_ENDIAN and BASE_IS_BIG_ENDIAN are set!
#endif

	if ((t.u16[0] & 0xFFFF) != 0x3344 || (t.u16[1] & 0xFFFF) != 0x1122)
	{
		BASE_ERROR("   Error: wrong 16bit values 0x%x and 0x%x", (t.u16[0] & 0xFFFF), (t.u16[1] & 0xFFFF));
		return 10;
	}

	if ((t.u8[0] & 0xFF) != 0x44 ||(t.u8[1] & 0xFF) != 0x33 ||
		(t.u8[2] & 0xFF) != 0x22 ||	(t.u8[3] & 0xFF) != 0x11)
	{
		BASE_ERROR("   Error: wrong 8bit values");
		return 12;
	}

#elif defined(BASE_IS_BIG_ENDIAN) && BASE_IS_BIG_ENDIAN
	BASE_INFO("   Library is set to big endian");

	if ((t.u16[0] & 0xFFFF) != 0x1122 ||(t.u16[1] & 0xFFFF) != 0x3344)
	{
		BASE_ERROR("   Error: wrong 16bit values 0x%x and 0x%x", (t.u16[0] & 0xFFFF), (t.u16[1] & 0xFFFF));
		return 20;
	}

	if ((t.u8[0] & 0xFF) != 0x11 ||(t.u8[1] & 0xFF) != 0x22 ||
		(t.u8[2] & 0xFF) != 0x33 ||	(t.u8[3] & 0xFF) != 0x44)
	{
		BASE_ERROR("   Error: wrong 8bit values");
		return 22;
	}

#if defined(BASE_IS_LITTLE_ENDIAN) && BASE_IS_LITTLE_ENDIAN
#error Error: Both BASE_IS_LITTLE_ENDIAN and BASE_IS_BIG_ENDIAN are set!
#endif


#else
#error Error: Endianness is not set properly!
#endif

	return 0;
}

int testBaseOs(void)
{
	const bsys_info *si;
	int rc = 0;

	BASE_INFO(" Sys info:");
	si = bget_sys_info();
	BASE_INFO("   machine:  %s", si->machine.ptr);
	BASE_INFO("   os_name:  %s", si->os_name.ptr);
	BASE_INFO("   os_ver:   0x%x", si->os_ver);
	BASE_INFO("   sdk_name: %s", si->sdk_name.ptr);
	BASE_INFO("   sdk_ver:  0x%x", si->sdk_ver);
	BASE_INFO("   info:     %s", si->info.ptr);

	rc = _endiannessTest32();

	bConfigDump();
	return rc;
}

#else
int dummy_os_var;
#endif

