/* 
 *
 */
#ifndef __UTIL_CLI_H__
#define __UTIL_CLI_H__

/**
 * @brief Command Line Interface
 */

#include <utilTypes.h>
#include <baseList.h>


BASE_BEGIN_DECL

/**
 * @defgroup UTIL_CLI Command Line Interface Framework
 * @{
 * A CLI framework features an interface for defining command specification, 
 * parsing, and executing a command. 
 * It also features an interface to communicate with various front-ends, 
 * such as console, telnet.
 * Application normally needs only one CLI instance to be created. 
 * On special cases, application could also create multiple CLI 
 * instances, with each instance has specific command structure.
 *
\verbatim
| vid help                  Show this help screen                             |
| vid enable|disable        Enable or disable video in next offer/answer      |
| vid call add              Add video stream for current call                 |
| vid call cap N ID         Set capture dev ID for stream #N in current call  |
| disable_codec g711|g722   Show this help screen                             |
<CMD name='vid' id='0' desc="">
       <CMD name='help' id='0' desc='' />
       <CMD name='enable' id='0' desc='' />
       <CMD name='disable' id='0' desc='' />
       <CMD name='call' id='0' desc='' >
                <CMD name='add' id='101' desc='...' />
                <CMD name='cap' id='102' desc='...' >
                   <ARG name='streamno' type='int' desc='...' id='1'/>
                   <ARG name='devid' type='int' optional='1' id='2'/>
                </CMD>
       </CMD>
</CMD>
<CMD name='disable_codec' id=0 desc="">
	<ARG name='codec_list' type='choice' id='3'>
	    <CHOICE value='g711'/>
	    <CHOICE value='g722'/>
	</ARG>
</CMD>
\endverbatim 
 */

/**
 * This opaque structure represents a CLI application. A CLI application is
 * the root placeholder of other CLI objects. In an application, one (and
 * normally only one) CLI application instance needs to be created.
 */
typedef struct bcli_t bcli_t;

/**
 * Type of command id.
 */
typedef int bcli_cmd_id;

/**
 * This describes the parameters to be specified when creating a CLI
 * application with bcli_create(). Application MUST initialize this
 * structure by calling bcli_cfg_default() before using it.
 */
typedef struct bcli_cfg
{
    /**
     * The application name, which will be used in places such as logs.
     * This field is mandatory.
     */
    bstr_t name;

    /**
     * Optional application title, which will be used in places such as
     * window title. If not specified, the application name will be used
     * as the title.
     */
    bstr_t title;

    /**
     * The pool factory where all memory allocations will be taken from.
     * This field is mandatory.
     */
    bpool_factory *pf;

} bcli_cfg;

/**
 * Type of argument id.
 */
typedef int bcli_arg_id;

/**
 * Forward declaration of bcli_cmd_spec structure.
 */
typedef struct bcli_cmd_spec bcli_cmd_spec;

/**
 * Forward declaration for bcli_sess, which will be declared in cli_imp.h.
 */
typedef struct bcli_sess bcli_sess;

/**
 * Forward declaration for CLI front-end.
 */
typedef struct bcli_front_end bcli_front_end;

/**
 * Forward declaration for CLI argument spec structure.
 */
typedef struct bcli_arg_spec bcli_arg_spec;

/**
 * This structure contains the command to be executed by command handler.
 */
typedef struct bcli_cmd_val
{
    /** The session on which the command was executed on. */
    bcli_sess *sess;

    /** The command specification being executed. */
    const bcli_cmd_spec *cmd;

    /** Number of argvs. */
    int argc;

    /** Array of args, with argv[0] specifies the  name of the cmd. */
    bstr_t argv[BASE_CLI_MAX_ARGS];

} bcli_cmd_val;

/**
 * This structure contains the hints information for the end user. 
 * This structure could contain either command or argument information.
 * The front-end will format the information and present it to the user.
 */
typedef struct bcli_hint_info
{
    /**
     * The hint value.
     */
    bstr_t name;

    /**
     * The hint type.
     */
    bstr_t type;

    /**
     * Helpful description of the hint value. 
     */
    bstr_t desc;

} bcli_hint_info;

/**
 * This structure contains extra information returned by bcli_sess_exec()/
 * bcli_sess_parse().
 * Upon return from the function, various other fields in this structure will
 * be set by the function.
 */
typedef struct bcli_exec_info
{
    /**
     * If command parsing failed, on return this will point to the location
     * where the failure occurs, otherwise the value will be set to -1.
     */
    int err_pos;

    /**
     * If a command matching the command in the cmdline was found, on return
     * this will be set to the command id of the command, otherwise it will be
     * set to BASE_CLI_INVALID_CMD_ID.
     */
    bcli_cmd_id cmd_id;

    /**
     * If a command was executed, on return this will be set to the return
     * value of the command, otherwise it will contain BASE_SUCCESS.
     */
    bstatus_t cmd_ret;

    /**
     * The number of hint elements
     **/
    unsigned hint_cnt;

    /**
     * If bcli_sess_parse() fails because of a missing argument or ambigous 
     * command/argument, the function returned BASE_CLI_EMISSINGARG or 
     * BASE_CLI_EAMBIGUOUS error. 
     * This field will contain the hint information. This is useful to give 
     * helpful information to the end_user.
     */
    bcli_hint_info hint[BASE_CLI_MAX_HINTS];

} bcli_exec_info;

/**
 * This structure contains the information returned from the dynamic 
 * argument callback.
 */
typedef struct bcli_arg_choice_val
{
    /**
     * The argument choice value
     */
    bstr_t value;

    /**
     * Helpful description of the choice value. This text will be used when
     * displaying the help texts for the choice value
     */
    bstr_t desc;

} bcli_arg_choice_val;

/**
 * This structure contains the parameters for bcli_get_dyn_choice
 */
typedef struct bcli_dyn_choice_param
{
    /**
     * The session on which the command was executed on.
     */
    bcli_sess *sess;

    /**
     * The command being processed.
     */
    bcli_cmd_spec *cmd;

    /**
     * The argument id.
     */
    bcli_arg_id arg_id;

    /**
     * The maximum number of values that the choice can hold.
     */
    unsigned max_cnt;

    /**
     * The pool to allocate memory from.
     */
    bpool_t *pool;

    /**
     * The choice values count.
     */
    unsigned cnt;

    /**
     * Array containing the valid choice values.
     */
    bcli_arg_choice_val choice[BASE_CLI_MAX_CHOICE_VAL];
} bcli_dyn_choice_param;

/**
 * This specifies the callback type for argument handlers, which will be
 * called to get the valid values of the choice type arguments.
 */
typedef void (*bcli_get_dyn_choice) (bcli_dyn_choice_param *param);

/**
 * This specifies the callback type for command handlers, which will be
 * executed when the specified command is invoked.
 *
 * @param cmd_val   The command that is specified by the user.
 *
 * @return          Return the status of the command execution.
 */
typedef bstatus_t (*bcli_cmd_handler)(bcli_cmd_val *cval);

/**
 * Write a log message to the CLI application. The CLI application
 * will send the log message to all the registered front-ends.
 *
 * @param cli		The CLI application instance.
 * @param level	        Verbosity level of this message message.
 * @param buffer        The message itself.
 * @param len 	        Length of this message.
 */
void bcli_write_log(bcli_t *cli,
                               int level,
                               const char *buffer,
                               int len);

/**
 * Create a new CLI application instance.
 *
 * @param cfg		CLI application creation parameters.
 * @param p_cli		Pointer to receive the returned instance.
 *
 * @return		BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bcli_create(bcli_cfg *cfg,
                                   bcli_t **p_cli);

/**
 * This specifies the function to get the id of the specified command
 * 
 * @param cmd		The specified command.
 *
 * @return		The command id
 */
bcli_cmd_id bcli_get_cmd_id(const bcli_cmd_spec *cmd);

/**
 * Get the internal parameter of the CLI instance.
 *
 * @param cli		The CLI application instance.
 *
 * @return		CLI parameter instance.
 */
bcli_cfg* bcli_get_param(bcli_t *cli);

/**
 * Call this to signal application shutdown. Typically application would
 * call this from it's "Quit" menu or similar command to quit the
 * application.
 *
 * See also bcli_sess_end_session() to end a session instead of quitting the
 * whole application.
 *
 * @param cli		The CLI application instance.
 * @param req		The session on which the shutdown request is
 * 			received.
 * @param restart	Indicate whether application restart is wanted.
 */
void bcli_quit(bcli_t *cli, bcli_sess *req,
			  bbool_t restart);
/**
 * Check if application shutdown or restart has been requested.
 *
 * @param cli		The CLI application instance.
 *
 * @return		BASE_TRUE if bcli_quit() has been called.
 */
bbool_t bcli_is_quitting(bcli_t *cli);

/**
 * Check if application restart has been requested.
 *
 * @param cli		The CLI application instance.
 *
 * @return		BASE_TRUE if bcli_quit() has been called with
 * 			restart parameter set.
 */
bbool_t bcli_is_restarting(bcli_t *cli);

/**
 * Destroy a CLI application instance. This would also close all sessions
 * currently running for this CLI application.
 *
 * @param cli		The CLI application.
 */
void bcli_destroy(bcli_t *cli);

/**
 * Initialize a bcli_cfg with its default values.
 *
 * @param param  The instance to be initialized.
 */
void bcli_cfg_default(bcli_cfg *param);

/**
 * Register a front end to the CLI application.
 *
 * @param CLI		The CLI application.
 * @param fe		The CLI front end to be registered.
 */
void bcli_register_front_end(bcli_t *cli,
                                        bcli_front_end *fe);

/**
 * Create a new complete command specification from an XML node text and
 * register it to the CLI application.
 *
 * Note that the input string MUST be NULL terminated.
 *
 * @param cli		The CLI application.
 * @param group		Optional group to which this command will be added
 * 			to, or specify NULL if this command is a root
 * 			command.
 * @param xml		Input string containing XML node text for the
 * 			command, MUST be NULL terminated.
 * @param handler	Function handler for the command. This must be NULL
 * 			if the command specifies a command group.
 * @param p_cmd		Optional pointer to store the newly created
 * 			specification.
 * @param get_choice	Function handler for the argument. Specify this for 
 *			dynamic choice type arguments.
 *
 * @return		BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bcli_add_cmd_from_xml(bcli_t *cli,
					     bcli_cmd_spec *group,
                                             const bstr_t *xml,
                                             bcli_cmd_handler handler,
                                             bcli_cmd_spec **p_cmd, 
					     bcli_get_dyn_choice get_choice);
/**
 * Initialize bcli_exec_info with its default values.
 *
 * @param param		The param to be initialized.
 */
void bcli_exec_info_default(bcli_exec_info *param);

/**
 * Write a log message to the specific CLI session. 
 *
 * @param sess		The CLI active session.
 * @param buffer        The message itself.
 * @param len 	        Length of this message.
 */
void bcli_sess_write_msg(bcli_sess *sess,                               
				    const char *buffer,
				    bsize_t len);

/**
 * Parse an input cmdline string. The first word of the command line is the
 * command itself, which will be matched against command  specifications
 * registered in the CLI application.
 *
 * Zero or more arguments follow the command name. Arguments are separated by
 * one or more whitespaces. Argument may be placed inside a pair of quotes,
 * double quotes, '{' and '}', or '[' and ']' pairs. This is useful when the
 * argument itself contains whitespaces or other restricted characters. If
 * the quote character itself is to appear in the argument, the argument then
 * must be quoted with different quote characters. There is no character
 * escaping facility provided by this function (such as the use of backslash
 * '\' character).
 *
 * The cmdline may be followed by an extra newline (LF or CR-LF characters),
 * which will be removed by the function. However any more characters
 * following this newline will cause an error to be returned.
 *
 * @param sess		The CLI session.
 * @param cmdline	The command line string to be parsed.
 * @param val		Structure to store the parsing result.
 * @param pool		The pool to allocate memory from.
 * @param info		Additional info to be returned regarding the parsing.
 *
 * @return		This function returns the status of the parsing,
 * 			which can be one of the following :
 *			  - BASE_SUCCESS: a command was executed successfully.
 *			  - BASE_EINVAL: invalid parameter to this function.
 *			  - BASE_ENOTFOUND: command is not found.
 *			  - BASE_CLI_EAMBIGUOUS: command/argument is ambiguous.
 *			  - BASE_CLI_EMISSINGARG: missing argument.
 *			  - BASE_CLI_EINVARG: invalid command argument.
 *			  - BASE_CLI_EEXIT: "exit" has been called to end
 *			      the current session. This is a signal for the
 *			      application to end it's main loop.
 */
bstatus_t bcli_sess_parse(bcli_sess *sess,
				       char *cmdline,
				       bcli_cmd_val *val,
				       bpool_t *pool,
				       bcli_exec_info *info);

/**
 * End the specified session, and destroy it to release all resources used
 * by the session.
 *
 * See also bcli_sess and bcli_front_end for more info regarding the 
 * creation process.
 * See also bcli_quit() to quit the whole application instead.
 *
 * @param sess		The CLI session to be destroyed.
 */
void bcli_sess_end_session(bcli_sess *sess);

/**
 * Execute a command line. This function will parse the input string to find
 * the appropriate command and verify whether the string matches the command
 * specifications. If matches, the command will be executed, and the return
 * value of the command will be set in the \a cmd_ret field of the \a info
 * argument, if specified.
 *
 * See also bcli_sess_parse() for more info regarding the cmdline format.
 *
 * @param sess		The CLI session.
 * @param cmdline	The command line string to be executed. 
 * @param pool		The pool to allocate memory from.
 * @param info		Optional pointer to receive additional information
 * 			related to the execution of the command (such as
 * 			the command return value).
 *
 * @return		This function returns the status of the command
 * 			parsing and execution (note that the return value
 * 			of the handler itself will be returned in \a info
 * 			argument, if specified). Please see the return value
 * 			of bcli_sess_parse() for possible return values.
 */
bstatus_t bcli_sess_exec(bcli_sess *sess,
				      char *cmdline,
				      bpool_t *pool,
				      bcli_exec_info *info);

BASE_END_DECL

#endif

