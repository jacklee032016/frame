/*
 *
 */
#ifndef __UTIL_CLI_TELNET_H__
#define __UTIL_CLI_TELNET_H__

/**
 * @brief Command Line Interface Telnet Front End API
 */

#include <utilCliImp.h>

BASE_BEGIN_DECL

/**
 * @ingroup UTIL_CLI_IMP
 * @{
 *
 */

 /**
 * This structure contains the information about the telnet.
 * Application will get updated information each time the telnet is started/
 * restarted.
 */
typedef struct bcli_telnet_info
{
    /**
     * The telnet's ip address.
     */
    bstr_t	ip_address;

    /**
     * The telnet's port number.
     */
    buint16_t port;

    /* Internal buffer for IP address */
    char buf_[32];

} bcli_telnet_info;

/**
 * This specifies the callback called when telnet is started
 *
 * @param status	The status of telnet startup process.
 *
 */
typedef void (*bcli_telnet_on_started)(bstatus_t status);

/**
 * This structure contains various options to instantiate the telnet daemon.
 * Application must call bcli_telnet_cfg_default() to initialize
 * this structure with its default values.
 */
typedef struct bcli_telnet_cfg
{
    /**
     * Listening port number. The value may be 0 to let the system choose
     * the first available port.
     *
     * Default value: BASE_CLI_TELNET_PORT
     */
    buint16_t port;

    /**
     * Ioqueue instance to be used. If this field is NULL, an internal
     * ioqueue and worker thread will be created.
     */
    bioqueue_t *ioqueue;

    /**
     * Default log verbosity level for the session.
     *
     * Default value: BASE_CLI_TELNET_LOG_LEVEL
     */
    int log_level;

    /**
     * Specify a password to be asked to the end user to access the
     * application. Currently this is not implemented yet.
     *
     * Default: empty (no password)
     */
    bstr_t passwd;

    /**
     * Specify text message to be displayed to newly connected users.
     * Currently this is not implemented yet.
     *
     * Default: empty
     */
    bstr_t welcome_msg;

    /**
     * Specify text message as a prompt string to user.
     *
     * Default: empty
     */
    bstr_t prompt_str;

    /**
     * Specify the bcli_telnet_on_started callback.
     *
     * Default: empty
     */
    bcli_telnet_on_started on_started;

} bcli_telnet_cfg;

/**
 * Initialize bcli_telnet_cfg with its default values.
 *
 * @param param		The structure to be initialized.
 */
void bcli_telnet_cfg_default(bcli_telnet_cfg *param);


/**
 * Create, initialize, and start a telnet daemon for the application.
 *
 * @param cli		The CLI application instance.
 * @param param		Optional parameters for creating the telnet daemon.
 * 			If this value is NULL, default parameters will be used.
 * @param p_fe		Optional pointer to receive the front-end instance
 * 			of the telnet front-end just created.
 *
 * @return		BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bcli_telnet_create(bcli_t *cli,
					  bcli_telnet_cfg *param,
					  bcli_front_end **p_fe);


/**
 * Retrieve cli telnet info.
 *
 * @param telnet_info   The telnet runtime information.
 *
 * @return		BASE_SUCCESS on success.
 */
bstatus_t bcli_telnet_get_info(bcli_front_end *fe, 
					    bcli_telnet_info *info); 

BASE_END_DECL

#endif

