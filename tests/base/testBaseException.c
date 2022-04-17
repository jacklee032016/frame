/*
 *
 */
#include "testBaseTest.h"


/**
 * \page page_baselib_exception_test Test: Exception Handling
 *
 * This file provides implementation of \b exception_test(). It tests the
 * functionality of the exception handling API.
 *
 * @note This test use static ID not acquired through proper registration.
 * This is not recommended, since it may create ID collissions.
 *
 * \section exception_test_sec Scope of the Test
 *
 * Some scenarios tested:
 *  - no exception situation
 *  - basic TRY/CATCH
 *  - multiple exception handlers
 *  - default handlers
 */


#if INCLUDE_EXCEPTION_TEST

#include <libBase.h>

#ifdef	_MSC_VER
#pragma warning(disable:4702) // warning C4702: unreachable code
#endif

#define	ID_1	1
#define ID_2	2

static int throw_id_1(void)
{
	BASE_THROW( ID_1 );
	BASE_UNREACHED(return -1;)
}

static int throw_id_2(void)
{
    BASE_THROW( ID_2 );
    BASE_UNREACHED(return -1;)
		
}

static int try_catch_test(void)
{
    BASE_USE_EXCEPTION;
    int rc = -200;

    BASE_TRY {
	BASE_THROW(ID_1);
    }
    BASE_CATCH_ANY {
	rc = 0;
    }
    BASE_END;
    return rc;
}

static int throw_in_handler(void)
{
    BASE_USE_EXCEPTION;
    int rc = 0;

    BASE_TRY {
	BASE_THROW(ID_1);
    }
    BASE_CATCH_ANY {
	if (BASE_GET_EXCEPTION() != ID_1)
	    rc = -300;
	else
	    BASE_THROW(ID_2);
    }
    BASE_END;
    return rc;
}

static int return_in_handler(void)
{
    BASE_USE_EXCEPTION;

    BASE_TRY {
	BASE_THROW(ID_1);
    }
    BASE_CATCH_ANY {
	return 0;
    }
    BASE_END;
    return -400;
}


static int test(void)
{
    int rc = 0;
    BASE_USE_EXCEPTION;

    /*
     * No exception situation.
     */
    BASE_TRY {
        BASE_UNUSED_ARG(rc);
    }
    BASE_CATCH_ANY {
        rc = -3;
    }
    BASE_END;

    if (rc != 0)
	return rc;


    /*
     * Basic TRY/CATCH
     */ 
    BASE_TRY {
	rc = throw_id_1();

	// should not reach here.
	rc = -10;
    }
    BASE_CATCH_ANY {
        int id = BASE_GET_EXCEPTION();
	if (id != ID_1) {
	    BASE_ERROR("...error: got unexpected exception %d (%s)", id, bexception_id_name(id));
	    if (!rc) rc = -20;
	}
    }
    BASE_END;

    if (rc != 0)
	return rc;

    /*
     * Multiple exceptions handlers
     */
    BASE_TRY {
	rc = throw_id_2();
	// should not reach here.
	rc = -25;
    }
    BASE_CATCH_ANY {
	switch (BASE_GET_EXCEPTION()) {
	case ID_1:
	    if (!rc) rc = -30;
	    break;
	case ID_2:
	    if (!rc) rc = 0;
	    break;
	default:
	    if (!rc) rc = -40;
	    break;
	}
    }
    BASE_END;

    if (rc != 0)
	return rc;

    /*
     * Test default handler.
     */
    BASE_TRY {
	rc = throw_id_1();
	// should not reach here
	rc = -50;
    }
    BASE_CATCH_ANY {
	switch (BASE_GET_EXCEPTION()) {
	case ID_1:
	    if (!rc) rc = 0;
	    break;
	default:
	    if (!rc) rc = -60;
	    break;
	}
    }
    BASE_END;

    if (rc != 0)
	return rc;

    /*
     * Nested handlers
     */
    BASE_TRY {
	rc = try_catch_test();
    }
    BASE_CATCH_ANY {
	rc = -70;
    }
    BASE_END;

    if (rc != 0)
	return rc;

    /*
     * Throwing exception inside handler
     */
    rc = -80;
    BASE_TRY {
	int rc2;
	rc2 = throw_in_handler();
	if (rc2)
	    rc = rc2;
    }
    BASE_CATCH_ANY {
	if (BASE_GET_EXCEPTION() == ID_2) {
	    rc = 0;
	} else {
	    rc = -90;
	}
    }
    BASE_END;

    if (rc != 0)
	return rc;


    /*
     * Return from handler. Returning from the function inside a handler
     * should be okay (though returning from the function inside the
     * BASE_TRY block IS NOT OKAY!!). We want to test to see if handler
     * is cleaned up properly, but not sure how to do this.
     */
    BASE_TRY {
	int rc2;
	rc2 = return_in_handler();
	if (rc2)
	    rc = rc2;
    }
    BASE_CATCH_ANY {
	rc = -100;
    }
    BASE_END;


    return 0;
}

int testBaseException(void)
{
	int i, rc;
	enum { LOOP = 10 };

	for (i=0; i<LOOP; ++i)
	{
		if ((rc=test()) != 0)
		{
			BASE_ERROR("...failed at i=%d (rc=%d)", i, rc);
			return rc;
		}
	}
	return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_exception_test;
#endif	/* INCLUDE_EXCEPTION_TEST */


