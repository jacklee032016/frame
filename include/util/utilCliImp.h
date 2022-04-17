/* 
 *
 */
#ifndef __UTIL_CLI_IMP_H__
#define __UTIL_CLI_IMP_H__

/**
 * @brief Command Line Interface Implementor's API
 */

#include <utilCli.h>


BASE_BEGIN_DECL

/**
 * @defgroup UTIL_CLI_IMP Command Line Interface Implementor's API
 * @ingroup UTIL_CLI
 * @{
 *
 */

/**
 * Default log level for console sessions.
 */
#ifndef BASE_CLI_CONSOLE_LOG_LEVEL
#   define BASE_CLI_CONSOLE_LOG_LEVEL	BASE_LOG_MAX_LEVEL
#endif

/**
 * Default log level for telnet sessions.
 */
#ifndef BASE_CLI_TELNET_LOG_LEVEL
#   define BASE_CLI_TELNET_LOG_LEVEL	4
#endif

/**
 * Default port number for telnet daemon.
 */
#ifndef BASE_CLI_TELNET_PORT
#   define BASE_CLI_TELNET_PORT		0
#endif

/**
 * This enumeration specifies front end types.
 */
typedef enum bcli_front_end_type
{
    BASE_CLI_CONSOLE_FRONT_END,	/**< Console front end.	*/
    BASE_CLI_TELNET_FRONT_END,	/**< Telnet front end.	*/
    BASE_CLI_HTTP_FRONT_END,	/**< HTTP front end.	*/
    BASE_CLI_GUI_FRONT_END	/**< GUI front end.	*/
} bcli_front_end_type;


/**
 * Front end operations. Only the CLI should call these functions
 * directly.
 */
typedef struct bcli_front_end_op
{
    /**
     * Callback to write a log message to the appropriate sessions belonging
     * to this front end. The front end would only send the log message to
     * the session if the session's log verbosity level is greater than the
     * level of this log message.
     *
     * @param fe	The front end.
     * @param level	Verbosity level of this message message.
     * @param data	The message itself.
     * @param len 	Length of this message.
     */
    void (*on_write_log)(bcli_front_end *fe, int level,
		         const char *data, bsize_t len);

    /**
     * Callback to be called when the application is quitting, to signal the
     * front-end to end its main loop or any currently blocking functions,
     * if any.
     *
     * @param fe	The front end.
     * @param req	The session which requested the application quit.
     */
    void (*on_quit)(bcli_front_end *fe, bcli_sess *req);

    /**
     * Callback to be called to close and self destroy the front-end. This
     * must also close any active sessions created by this front-ends.
     *
     * @param fe	The front end.
     */
    void (*on_destroy)(bcli_front_end *fe);

} bcli_front_end_op;


/**
 * This structure describes common properties of CLI front-ends. A front-
 * end is a mean to interact with end user, for example the CLI application
 * may interact with console, telnet, web, or even a GUI.
 *
 * Each end user's interaction will create an instance of bcli_sess.
 *
 * Application instantiates the front end by calling the appropriate
 * function to instantiate them.
 */
struct bcli_front_end
{
    /**
     * Linked list members
     */
    BASE_DECL_LIST_MEMBER(struct bcli_front_end);

    /**
     * Front end type.
     */
    bcli_front_end_type type;

    /**
     * The CLI application.
     */
    bcli_t *cli;

    /**
     * Front end operations.
     */
    bcli_front_end_op *op;
};


/**
 * Session operations.
 */
typedef struct bcli_sess_op
{
    /**
     * Callback to be called to close and self destroy the session.
     *
     * @param sess	The session to destroy.
     */
    void (*destroy)(bcli_sess *sess);

} bcli_sess_op;


/**
 * This structure describes common properties of a CLI session. A CLI session
 * is the interaction of an end user to the CLI application via a specific
 * renderer, where the renderer can be console, telnet, web, or a GUI app for
 * mobile. A session is created by its renderer, and it's creation procedures
 * vary among renderer (for example, a telnet session is created when the
 * end user connects to the application, while a console session is always
 * instantiated once the program is run).
 */
struct bcli_sess
{
    /**
     * Linked list members
     */
    BASE_DECL_LIST_MEMBER(struct bcli_sess);

    /**
     * Pointer to the front-end instance which created this session.
     */
    bcli_front_end *fe;

    /**
     * Session operations.
     */
    bcli_sess_op *op;

    /**
     * Text containing session info, which is filled by the renderer when
     * the session is created.
     */
    bstr_t info;

    /**
     * Log verbosity of this session.
     */
    int log_level;

};

BASE_END_DECL

#endif

