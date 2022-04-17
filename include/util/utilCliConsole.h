/*
 *
 */
#ifndef __UTIL_CLI_CONSOLE_H__
#define __UTIL_CLI_CONSOLE_H__

/**
 * @brief Command Line Interface Console Front End API
 */

#include <utilCliImp.h>


BASE_BEGIN_DECL

/**
 * @ingroup UTIL_CLI_IMP
 * @{
 *
 */


/**
 * This structure contains various options for CLI console front-end.
 * Application must call bcli_console_cfg_default() to initialize
 * this structure with its default values.
 */
typedef struct bcli_console_cfg
{
    /**
     * Default log verbosity level for the session.
     *
     * Default value: BASE_CLI_CONSOLE_LOG_LEVEL
     */
    int log_level;

    /**
     * Specify text message as a prompt string to user.
     *
     * Default: empty
     */
    bstr_t prompt_str;

    /**
     * Specify the command to quit the application.
     *
     * Default: empty
     */
    bstr_t quit_command;

} bcli_console_cfg;


/**
 * Initialize bcli_console_cfg with its default values.
 *
 * @param param		The structure to be initialized.
 */
void bcli_console_cfg_default(bcli_console_cfg *param);


/**
 * Create a console front-end for the specified CLI application, and return
 * the only session instance for the console front end. On Windows operating
 * system, this may open a new console window.
 *
 * @param cli		The CLI application instance.
 * @param param		Optional console CLI parameters. If this value is
 * 			NULL, default parameters will be used.
 * @param p_sess	Pointer to receive the session instance for the
 * 			console front end.
 * @param p_fe		Optional pointer to receive the front-end instance
 * 			of the console front-end just created.
 *
 * @return		BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bcli_console_create(bcli_t *cli,
					   const bcli_console_cfg *param,
					   bcli_sess **p_sess,
					   bcli_front_end **p_fe);

/**
 * Retrieve a cmdline from console stdin and process the input accordingly.
 *
 * @param sess		The CLI session.
 * @param buf		Pointer to receive the buffer.
 * @param maxlen	Maximum length to read.
 *
 * @return		BASE_SUCCESS if an input was read
 */
bstatus_t bcli_console_process(bcli_sess *sess, 
					    char *buf,
					    unsigned maxlen);

BASE_END_DECL

#endif

