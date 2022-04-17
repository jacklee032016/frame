/* 
 *
 */
#ifndef __BASE_EXCEPTION_H__
#define __BASE_EXCEPTION_H__

/**
 * @file except.h
 * @brief Exception Handling in C.
 */

#include <_baseTypes.h>
#include <compat/setjmp.h>
#include <baseLog.h>


BASE_BEGIN_DECL


/**
 * @defgroup BASE_EXCEPT Exception Handling
 * @ingroup BASE_MISC
 * @{
 *
 * \section bexcept_sample_sec Quick Example
 *
 * For the impatient, take a look at some examples:
 *  - @ref page_baselib_samples_except_c
 *  - @ref page_baselib_exception_test
 *
 * \section bexcept_except Exception Handling
 *
 * This module provides exception handling syntactically similar to C++ in
 * C language. In Win32 systems, it uses Windows Structured Exception
 * Handling (SEH) if macro BASE_EXCEPTION_USE_WIN32_SEH is non-zero.
 * Otherwise it will use setjmp() and longjmp().
 *
 * On some platforms where setjmp/longjmp is not available, setjmp/longjmp 
 * implementation is provided. See <compat/setjmp.h> for compatibility.
 *
 * The exception handling mechanism is completely thread safe, so the exception
 * thrown by one thread will not interfere with other thread.
 *
 * The exception handling constructs are similar to C++. The blocks will be
 * constructed similar to the following sample:
 *
 * \verbatim
   #define NO_MEMORY     1
   #define SYNTAX_ERROR  2
  
   int sample1()
   {
      BASE_USE_EXCEPTION;  // declare local exception stack.
  
      BASE_TRY {
        ...// do something..
      }
      BASE_CATCH(NO_MEMORY) {
        ... // handle exception 1
      }
      BASE_END;
   }

   int sample2()
   {
      BASE_USE_EXCEPTION;  // declare local exception stack.
  
      BASE_TRY {
        ...// do something..
      }
      BASE_CATCH_ANY {
         if (BASE_GET_EXCEPTION() == NO_MEMORY)
	    ...; // handle no memory situation
	 else if (BASE_GET_EXCEPTION() == SYNTAX_ERROR)
	    ...; // handle syntax error
      }
      BASE_END;
   }
   \endverbatim
 *
 * The above sample uses hard coded exception ID. It is @b strongly
 * recommended that applications request a unique exception ID instead
 * of hard coded value like above.
 *
 * \section bexcept_reg Exception ID Allocation
 *
 * To ensure that exception ID (number) are used consistently and to
 * prevent ID collisions in an application, it is strongly suggested that 
 * applications allocate an exception ID for each possible exception
 * type. As a bonus of this process, the application can identify
 * the name of the exception when the particular exception is thrown.
 *
 * Exception ID management are performed with the following APIs:
 *  - #bexception_id_alloc().
 *  - #bexception_id_free().
 *  - #bexception_id_name().
 *
 *
 *  itself automatically allocates one exception id, i.e.
 * #BASE_NO_MEMORY_EXCEPTION which is declared in <basePool.h>. This exception
 * ID is raised by default pool policy when it fails to allocate memory.
 *
 * CAVEATS:
 *  - unlike C++ exception, the scheme here won't call destructors of local
 *    objects if exception is thrown. Care must be taken when a function
 *    hold some resorce such as pool or mutex etc.
 *  - You CAN NOT make nested exception in one single function without using
 *    a nested BASE_USE_EXCEPTION. Samples:
  \verbatim
	void wrong_sample()
	{
	    BASE_USE_EXCEPTION;

	    BASE_TRY {
		// Do stuffs
		...
	    }
	    BASE_CATCH_ANY {
		// Do other stuffs
		....
		..

		// The following block is WRONG! You MUST declare 
		// BASE_USE_EXCEPTION once again in this block.
		BASE_TRY {
		    ..
		}
		BASE_CATCH_ANY {
		    ..
		}
		BASE_END;
	    }
	    BASE_END;
	}

  \endverbatim

 *  - You MUST NOT exit the function inside the BASE_TRY block. The correct way
 *    is to return from the function after BASE_END block is executed. 
 *    For example, the following code will yield crash not in this code,
 *    but rather in the subsequent execution of BASE_TRY block:
  \verbatim
        void wrong_sample()
	{
	    BASE_USE_EXCEPTION;

	    BASE_TRY {
		// do some stuffs
		...
		return;	        <======= DO NOT DO THIS!
	    }
	    BASE_CATCH_ANY {
	    }
	    BASE_END;
	}
  \endverbatim
  
 *  - You can not provide more than BASE_CATCH or BASE_CATCH_ANY nor use BASE_CATCH
 *    and BASE_CATCH_ANY for a single BASE_TRY.
 *  - Exceptions will always be caught by the first handler (unlike C++ where
 *    exception is only caught if the type matches.

 * \section BASE_EX_KEYWORDS Keywords
 *
 * \subsection BASE_THROW BASE_THROW(expression)
 * Throw an exception. The expression thrown is an integer as the result of
 * the \a expression. This keyword can be specified anywhere within the 
 * program.
 *
 * \subsection BASE_USE_EXCEPTION BASE_USE_EXCEPTION
 * Specify this in the variable definition section of the function block 
 * (or any blocks) to specify that the block has \a BASE_TRY/BASE_CATCH exception 
 * block. 
 * Actually, this is just a macro to declare local variable which is used to
 * push the exception state to the exception stack.
 * Note: you must specify BASE_USE_EXCEPTION as the last statement in the
 * local variable declarations, since it may evaluate to nothing.
 *
 * \subsection BASE_TRY BASE_TRY
 * The \a BASE_TRY keyword is typically followed by a block. If an exception is
 * thrown in this block, then the execution will resume to the \a BASE_CATCH 
 * handler.
 *
 * \subsection BASE_CATCH BASE_CATCH(expression)
 * The \a BASE_CATCH is normally followed by a block. This block will be executed
 * if the exception being thrown is equal to the expression specified in the
 * \a BASE_CATCH.
 *
 * \subsection BASE_CATCH_ANY BASE_CATCH_ANY
 * The \a BASE_CATCH is normally followed by a block. This block will be executed
 * if any exception was raised in the TRY block.
 *
 * \subsection BASE_END BASE_END
 * Specify this keyword to mark the end of \a BASE_TRY / \a BASE_CATCH blocks.
 *
 * \subsection BASE_GET_EXCEPTION BASE_GET_EXCEPTION(void)
 * Get the last exception thrown. This macro is normally called inside the
 * \a BASE_CATCH or \a BASE_CATCH_ANY block, altough it can be used anywhere where
 * the \a BASE_USE_EXCEPTION definition is in scope.
 *
 * 
 * \section bexcept_examples_sec Examples
 *
 * For some examples on how to use the exception construct, please see:
 *  - @ref page_baselib_samples_except_c
 *  - @ref page_baselib_exception_test
 */

/**
 * Allocate a unique exception id.
 * Applications don't have to allocate a unique exception ID before using
 * the exception construct. However, by doing so it ensures that there is
 * no collisions of exception ID.
 *
 * As a bonus, when exception number is acquired through this function,
 * the library can assign name to the exception (only if 
 * BASE_HAS_EXCEPTION_NAMES is enabled (default is yes)) and find out the
 * exception name when it catches an exception.
 *
 * @param name      Name to be associated with the exception ID.
 * @param id        Pointer to receive the ID.
 *
 * @return          BASE_SUCCESS on success or BASE_ETOOMANY if the library 
 *                  is running out out ids.
 */
bstatus_t bexception_id_alloc(const char *name,
                                           bexception_id_t *id);

/**
 * Free an exception id.
 *
 * @param id        The exception ID.
 *
 * @return          BASE_SUCCESS or the appropriate error code.
 */
bstatus_t bexception_id_free(bexception_id_t id);

/**
 * Retrieve name associated with the exception id.
 *
 * @param id        The exception ID.
 *
 * @return          The name associated with the specified ID.
 */
const char* bexception_id_name(bexception_id_t id);


/** @} */

#if defined(BASE_EXCEPTION_USE_WIN32_SEH) && BASE_EXCEPTION_USE_WIN32_SEH != 0
/*****************************************************************************
 **
 ** IMPLEMENTATION OF EXCEPTION USING WINDOWS SEH
 **
 ****************************************************************************/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void bthrow_exception_(bexception_id_t id) BASE_ATTR_NORETURN
{
    RaiseException(id,1,0,NULL);
}

#define BASE_USE_EXCEPTION    
#define BASE_TRY		    __try
#define BASE_CATCH(id)	    __except(GetExceptionCode()==id ? \
				      EXCEPTION_EXECUTE_HANDLER : \
				      EXCEPTION_CONTINUE_SEARCH)
#define BASE_CATCH_ANY	    __except(EXCEPTION_EXECUTE_HANDLER)
#define BASE_END		    
#define BASE_THROW(id)	    bthrow_exception_(id)
#define BASE_GET_EXCEPTION()  GetExceptionCode()


#elif defined(BASE_SYMBIAN) && BASE_SYMBIAN!=0
/*****************************************************************************
 **
 ** IMPLEMENTATION OF EXCEPTION USING SYMBIAN LEAVE/TRAP FRAMEWORK
 **
 ****************************************************************************/

/* To include this file, the source file must be compiled as
 * C++ code!
 */
#ifdef __cplusplus

class TPjException
{
public:
    int code_;
};

#define BASE_USE_EXCEPTION
#define BASE_TRY			try
//#define BASE_CATCH(id)		
#define BASE_CATCH_ANY		catch (const TPjException & bexcp_)
#define BASE_END
#define BASE_THROW(x_id)		do { TPjException e; e.code_=x_id; throw e;} \
				while (0)
#define BASE_GET_EXCEPTION()	bexcp_.code_

#else

#define BASE_USE_EXCEPTION
#define BASE_TRY				
#define BASE_CATCH_ANY		if (0)
#define BASE_END
#define BASE_THROW(x_id)		do { BASE_LOG(1,("BASE_THROW"," error code = %d",x_id)); } while (0)
#define BASE_GET_EXCEPTION()	0

#endif	/* __cplusplus */

#else
/*****************************************************************************
 **
 ** IMPLEMENTATION OF EXCEPTION USING GENERIC SETJMP/LONGJMP
 **
 ****************************************************************************/

/**
 * This structure (which should be invisible to user) manages the TRY handler
 * stack.
 */
struct bexception_state_t
{    
    bjmp_buf state;                   /**< jmp_buf.                    */
    struct bexception_state_t *prev;  /**< Previous state in the list. */
};

/**
 * Throw exception.
 * @param id    Exception Id.
 */
void bthrow_exception_(bexception_id_t id) BASE_ATTR_NORETURN;

/**
 * Push exception handler.
 */
void bpush_exception_handler_(struct bexception_state_t *rec);

/**
 * Pop exception handler.
 */
void bpop_exception_handler_(struct bexception_state_t *rec);

/**
 * Declare that the function will use exception.
 * @hideinitializer
 */
#define BASE_USE_EXCEPTION    struct bexception_state_t bx_except__; int bx_code__

/**
 * Start exception specification block.
 * @hideinitializer
 */
#define BASE_TRY		    if (1) { \
				bpush_exception_handler_(&bx_except__); \
				bx_code__ = bsetjmp(bx_except__.state); \
				if (bx_code__ == 0)
/**
 * Catch the specified exception Id.
 * @param id    The exception number to catch.
 * @hideinitializer
 */
#define BASE_CATCH(id)	    else if (bx_code__ == (id))

/**
 * Catch any exception number.
 * @hideinitializer
 */
#define BASE_CATCH_ANY	    else

/**
 * End of exception specification block.
 * @hideinitializer
 */
#define BASE_END			bpop_exception_handler_(&bx_except__); \
			    } else {}

/**
 * Throw exception.
 * @param exception_id  The exception number.
 * @hideinitializer
 */
#define BASE_THROW(exception_id)	bthrow_exception_(exception_id)

/**
 * Get current exception.
 * @return      Current exception code.
 * @hideinitializer
 */
#define BASE_GET_EXCEPTION()	(bx_code__)

#endif	/* BASE_EXCEPTION_USE_WIN32_SEH */


BASE_END_DECL

#endif

