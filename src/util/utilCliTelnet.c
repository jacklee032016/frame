/* 
 *
 */

#include <utilCliImp.h>
#include <utilCliTelnet.h>
#include <baseActiveSock.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLog.h>
#include <baseOs.h>
#include <basePool.h>
#include <baseString.h>
#include <baseExcept.h>
#include <utilErrno.h>
#include <utilScanner.h>
#include <baseAddrResolv.h>
#include <compat/socket.h>

#if (defined(BASE_WIN32) && BASE_WIN32!=0) || \
    (defined(BASE_WIN64) && BASE_WIN64!=0) || \
    (defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0)

/* Undefine EADDRINUSE first, we want it equal to WSAEADDRINUSE,
 * while WinSDK 10 defines it to another value.
 */
#undef EADDRINUSE
#define EADDRINUSE WSAEADDRINUSE

#endif

#define CLI_TELNET_BUF_SIZE 256

#define CUT_MSG "<..data truncated..>\r\n"
#define MAX_CUT_MSG_LEN 25


#define MAX_CLI_TELNET_OPTIONS 256
/** Maximum retry on Telnet Restart **/
#define MAX_RETRY_ON_TELNET_RESTART 100
/** Minimum number of millisecond to wait before retrying to re-bind on
 * telnet restart **/
#define MIN_WAIT_ON_TELNET_RESTART 20
/** Maximum number of millisecod to wait before retrying to re-bind on
 *  telnet restart **/
#define MAX_WAIT_ON_TELNET_RESTART 1000

/**
 * This specify the state for the telnet option negotiation.
 */
enum cli_telnet_option_states
{
    OPT_DISABLE,		/* Option disable */
    OPT_ENABLE,			/* Option enable */
    OPT_EXPECT_DISABLE,		/* Already send disable req, expecting resp */
    OPT_EXPECT_ENABLE,		/* Already send enable req, expecting resp */
    OPT_EXPECT_DISABLE_REV,	/* Already send disable req, expecting resp,
				 * need to send enable req */
    OPT_EXPECT_ENABLE_REV	/* Already send enable req, expecting resp,
				 * need to send disable req */
};

/**
 * This structure contains information for telnet session option negotiation.
 * It contains the local/peer option config and the option negotiation state.
 */
typedef struct cli_telnet_sess_option
{
    /**
     * Local setting for the option.
     * Default: FALSE;
     */
    bbool_t local_is_enable;

    /**
     * Remote setting for the option.
     * Default: FALSE;
     */
    bbool_t peer_is_enable;

    /**
     * Local state of the option negotiation.
     */
    enum cli_telnet_option_states local_state;

    /**
     * Remote state of the option negotiation.
     */
    enum cli_telnet_option_states peer_state;
} cli_telnet_sess_option;

/**
 * This specify the state of input character parsing.
 */
typedef enum cmd_parse_state
{
    ST_NORMAL,
    ST_CR,
    ST_ESC,
    ST_VT100,
    ST_IAC,
    ST_DO,
    ST_DONT,
    ST_WILL,
    ST_WONT
} cmd_parse_state;

typedef enum cli_telnet_command
{
    SUBNEGO_END	    = 240,	/* End of subnegotiation parameters. */
    NOP		    = 241,	/* No operation. */
    DATA_MARK	    = 242,	/* Marker for NVT cleaning. */
    BREAK	    = 243,	/* Indicates that the "break" key was hit. */
    INT_PROCESS	    = 244,	/* Suspend, interrupt or abort the process. */
    ABORT_OUTPUT    = 245,	/* Abort output, abort output stream. */
    ARE_YOU_THERE   = 246,	/* Are you there. */
    ERASE_CHAR	    = 247,	/* Erase character, erase the current char. */
    ERASE_LINE	    = 248,	/* Erase line, erase the current line. */
    GO_AHEAD	    = 249,	/* Go ahead, other end can transmit. */
    SUBNEGO_BEGIN   = 250,	/* Subnegotiation begin. */
    WILL	    = 251,	/* Accept the use of option. */
    WONT	    = 252,	/* Refuse the use of option. */
    DO		    = 253,	/* Request to use option. */
    DONT	    = 254,	/* Request to not use option. */
    IAC		    = 255	/* Interpret as command */
} cli_telnet_command;

enum cli_telnet_options
{
    TRANSMIT_BINARY	= 0,	/* Transmit Binary. */
    TERM_ECHO		= 1,	/* Echo. */
    RECONNECT		= 2,	/* Reconnection. */
    SUPPRESS_GA		= 3,	/* Suppress Go Aheah. */
    MESSAGE_SIZE_NEGO	= 4,	/* Approx Message Size Negotiation. */
    STATUS		= 5,	/* Status. */
    TIMING_MARK		= 6,	/* Timing Mark. */
    RTCE_OPTION		= 7,	/* Remote Controlled Trans and Echo. */
    OUTPUT_LINE_WIDTH	= 8,	/* Output Line Width. */
    OUTPUT_PAGE_SIZE	= 9,	/* Output Page Size. */
    CR_DISPOSITION	= 10,	/* Carriage-Return Disposition. */
    HORI_TABSTOPS	= 11,	/* Horizontal Tabstops. */
    HORI_TAB_DISPO	= 12,	/* Horizontal Tab Disposition. */
    FF_DISP0		= 13,	/* Formfeed Disposition. */
    VERT_TABSTOPS	= 14,	/* Vertical Tabstops. */
    VERT_TAB_DISPO	= 15,	/* Vertical Tab Disposition. */
    LF_DISP0		= 16,	/* Linefeed Disposition. */
    BASE_ASCII		= 17, 	/* Extended ASCII. */
    LOGOUT		= 18,	/* Logout. */
    BYTE_MACRO		= 19,	/* Byte Macro. */
    DE_TERMINAL		= 20,	/* Data Entry Terminal. */
    SUPDUP_PROTO	= 21,	/* SUPDUP Protocol. */
    SUPDUP_OUTPUT	= 22,	/* SUPDUP Output. */
    SEND_LOC		= 23,	/* Send Location. */
    TERM_TYPE		= 24,	/* Terminal Type. */
    EOR			= 25,	/* End of Record. */
    TACACS_UID		= 26,	/* TACACS User Identification. */
    OUTPUT_MARKING	= 27,	/* Output Marking. */
    TTYLOC		= 28,	/* Terminal Location Number. */
    USE_3270_REGIME	= 29,	/* Telnet 3270 Regime. */
    USE_X3_PAD		= 30,	/* X.3 PAD. */
    WINDOW_SIZE		= 31,	/* Window Size. */
    TERM_SPEED		= 32,	/* Terminal Speed. */
    REM_FLOW_CONTROL	= 33,	/* Remote Flow Control. */
    LINE_MODE		= 34,	/* Linemode. */
    X_DISP_LOC		= 35,	/* X Display Location. */
    ENVIRONMENT		= 36,	/* Environment. */
    AUTH		= 37,	/* Authentication. */
    ENCRYPTION		= 38, 	/* Encryption Option. */
    NEW_ENVIRONMENT	= 39,	/* New Environment. */
    TN_3270E		= 40,	/* TN3270E. */
    XAUTH		= 41,	/* XAUTH. */
    CHARSET		= 42,	/* CHARSET. */
    REM_SERIAL_PORT	= 43,	/* Telnet Remote Serial Port. */
    COM_PORT_CONTROL	= 44,	/* Com Port Control. */
    SUPP_LOCAL_ECHO	= 45,	/* Telnet Suppress Local Echo. */
    START_TLS		= 46,	/* Telnet Start TLS. */
    KERMIT		= 47,	/* KERMIT. */
    SEND_URL		= 48,	/* SEND-URL. */
    FWD_X		= 49,	/* FORWARD_X. */
    BASE_OPTIONS		= 255	/* Extended-Options-List */
};

enum terminal_cmd
{
    TC_ESC		= 27,
    TC_UP		= 65,
    TC_DOWN		= 66,
    TC_RIGHT		= 67,
    TC_LEFT		= 68,
    TC_END		= 70,
    TC_HOME		= 72,
    TC_CTRL_C		= 3,
    TC_CR		= 13,
    TC_BS		= 8,
    TC_TAB		= 9,
    TC_QM		= 63,
    TC_BELL		= 7,
    TC_DEL		= 127
};

/**
 * This specify the state of output character parsing.
 */
typedef enum out_parse_state
{
    OP_NORMAL,
    OP_TYPE,
    OP_SHORTCUT,
    OP_CHOICE
} out_parse_state;

/**
 * This structure contains the command line shown to the user.
 * The telnet also needs to maintain and manage command cursor position.
 * Due to that reason, the insert/delete character process from buffer will
 * consider its current cursor position.
 */
typedef struct telnet_recv_buf {
    /**
     * Buffer containing the characters, NULL terminated.
     */
    unsigned char	    rbuf[BASE_CLI_MAX_CMDBUF];

    /**
     * Current length of the command line.
     */
    unsigned		    len;

    /**
     * Current cursor position.
     */
    unsigned		    cur_pos;
} telnet_recv_buf;

/**
 * This structure contains the command history executed by user.
 * Besides storing the command history, it is necessary to be able
 * to browse it.
 */
typedef struct cmd_history
{
    BASE_DECL_LIST_MEMBER(struct cmd_history);
    bstr_t command;
} cmd_history;

typedef struct cli_telnet_sess
{
    bcli_sess		    base;
    bpool_t		    *pool;
    bactivesock_t	    *asock;
    bbool_t		    authorized;
    bioqueue_op_key_t	    op_key;
    bmutex_t		    *smutex;
    cmd_parse_state	    parse_state;
    cli_telnet_sess_option  telnet_option[MAX_CLI_TELNET_OPTIONS];
    cmd_history		    *history;
    cmd_history		    *active_history;

    telnet_recv_buf	    *rcmd;
    unsigned char	    buf[CLI_TELNET_BUF_SIZE + MAX_CUT_MSG_LEN];
    unsigned		    buf_len;
} cli_telnet_sess;

typedef struct cli_telnet_fe
{
    bcli_front_end        base;
    bpool_t              *pool;
    bcli_telnet_cfg       cfg;
    bbool_t               own_ioqueue;
    bcli_sess             sess_head;

    bactivesock_t	   *asock;
    bthread_t            *worker_thread;
    bbool_t               is_quitting;
    bmutex_t             *mutex;
} cli_telnet_fe;

/* Forward Declaration */
static bstatus_t telnet_sess_send2(cli_telnet_sess *sess,
                                     const unsigned char *str, int len);

static bstatus_t telnet_sess_send(cli_telnet_sess *sess,
                                    const bstr_t *str);

static bstatus_t telnet_start(cli_telnet_fe *fe);
static bstatus_t telnet_restart(cli_telnet_fe *tfe);

/**
 * Return the number of characters between the current cursor position
 * to the end of line.
 */
static unsigned recv_buf_right_len(telnet_recv_buf *recv_buf)
{
    return (recv_buf->len - recv_buf->cur_pos);
}

/**
 * Insert character to the receive buffer.
 */
static bbool_t recv_buf_insert(telnet_recv_buf *recv_buf,
				 unsigned char *data)
{
    if (recv_buf->len+1 >= BASE_CLI_MAX_CMDBUF) {
	return BASE_FALSE;
    } else {
	if (*data == '\t' || *data == '?' || *data == '\r') {
	    /* Always insert to the end of line */
	    recv_buf->rbuf[recv_buf->len] = *data;
	} else {
	    /* Insert based on the current cursor pos */
	    unsigned cur_pos = recv_buf->cur_pos;
	    unsigned rlen = recv_buf_right_len(recv_buf);
	    if (rlen > 0) {
		/* Shift right characters */
		bmemmove(&recv_buf->rbuf[cur_pos+1],
			   &recv_buf->rbuf[cur_pos],
			   rlen+1);
	    }
	    recv_buf->rbuf[cur_pos] = *data;
	}
	++recv_buf->cur_pos;
	++recv_buf->len;
	recv_buf->rbuf[recv_buf->len] = 0;
    }
    return BASE_TRUE;
}

/**
 * Delete character on the previous cursor position of the receive buffer.
 */
static bbool_t recv_buf_backspace(telnet_recv_buf *recv_buf)
{
    if ((recv_buf->cur_pos == 0) || (recv_buf->len == 0)) {
	return BASE_FALSE;
    } else {
	unsigned rlen = recv_buf_right_len(recv_buf);
	if (rlen) {
	    unsigned cur_pos = recv_buf->cur_pos;
	    /* Shift left characters */
	    bmemmove(&recv_buf->rbuf[cur_pos-1], &recv_buf->rbuf[cur_pos],
		       rlen);
	}
	--recv_buf->cur_pos;
	--recv_buf->len;
	recv_buf->rbuf[recv_buf->len] = 0;
    }
    return BASE_TRUE;
}

static int compare_str(void *value, const blist_type *nd)
{
    cmd_history *node = (cmd_history*)nd;
    return (bstrcmp((bstr_t *)value, &node->command));
}

/**
 * Insert the command to history. If the entered command is not on the list,
 * a new entry will be created. All entered command will be moved to
 * the first entry of the history.
 */
static bstatus_t insert_history(cli_telnet_sess *sess,
				  char *cmd_val)
{
    cmd_history *in_history;
    bstr_t cmd;
    cmd.ptr = cmd_val;
    cmd.slen = bansi_strlen(cmd_val)-1;

    if (cmd.slen == 0)
	return BASE_SUCCESS;

    BASE_ASSERT_RETURN(sess, BASE_EINVAL);

    /* Find matching history */
    in_history = blist_search(sess->history, (void*)&cmd, compare_str);
    if (!in_history) {
	if (blist_size(sess->history) < BASE_CLI_MAX_CMD_HISTORY) {
	    char *data_history;
	    in_history = BASE_POOL_ZALLOC_T(sess->pool, cmd_history);
	    blist_init(in_history);
	    data_history = (char *)bpool_calloc(sess->pool,
			   sizeof(char), BASE_CLI_MAX_CMDBUF);
	    in_history->command.ptr = data_history;
	    in_history->command.slen = 0;
	} else {
	    /* Get the oldest history */
	    in_history = sess->history->prev;
	}
    } else {
	blist_insert_nodes_after(in_history->prev, in_history->next);
    }
    bstrcpy(&in_history->command, bstrtrim(&cmd));
    blist_push_front(sess->history, in_history);
    sess->active_history = sess->history;

    return BASE_SUCCESS;
}

/**
 * Get the next or previous history of the shown/active history.
 */
static bstr_t* get_prev_history(cli_telnet_sess *sess, bbool_t is_forward)
{
    bstr_t *retval;
    bsize_t history_size;
    cmd_history *node;
    cmd_history *root;

    BASE_ASSERT_RETURN(sess, NULL);

    node = sess->active_history;
    root = sess->history;
    history_size = blist_size(sess->history);

    if (history_size == 0) {
	return NULL;
    } else {
	if (is_forward) {
	    node = (node->next==root)?node->next->next:node->next;
	} else {
	    node = (node->prev==root)?node->prev->prev:node->prev;
	}
	retval = &node->command;
	sess->active_history = node;
    }
    return retval;
}

/*
 * This method is used to send option negotiation command.
 * The commands dealing with option negotiation are
 * three byte sequences, the third byte being the code for the option
 * referenced - (RFC-854).
 */
static bbool_t send_telnet_cmd(cli_telnet_sess *sess,
				 cli_telnet_command cmd,
				 unsigned char option)
{
    unsigned char buf[3];
    BASE_ASSERT_RETURN(sess, BASE_FALSE);

    buf[0] = IAC;
    buf[1] = cmd;
    buf[2] = option;
    telnet_sess_send2(sess, buf, 3);

    return BASE_TRUE;
}

/**
 * This method will handle sending telnet's ENABLE option negotiation.
 * For local option: send WILL.
 * For remote option: send DO.
 * This method also handle the state transition of the ENABLE
 * negotiation process.
 */
static bbool_t send_enable_option(cli_telnet_sess *sess,
				    bbool_t is_local,
				    unsigned char option)
{
    cli_telnet_sess_option *sess_option;
    enum cli_telnet_option_states *state;
    BASE_ASSERT_RETURN(sess, BASE_FALSE);

    sess_option = &sess->telnet_option[option];
    state = is_local?(&sess_option->local_state):(&sess_option->peer_state);
    switch (*state) {
	case OPT_ENABLE:
	    /* Ignore if already enabled */
	    break;
	case OPT_DISABLE:
	    *state = OPT_EXPECT_ENABLE;
	    send_telnet_cmd(sess, (is_local?WILL:DO), option);
	    break;
	case OPT_EXPECT_ENABLE:
	    *state = OPT_DISABLE;
	    break;
	case OPT_EXPECT_DISABLE:
	    *state = OPT_EXPECT_DISABLE_REV;
	    break;
	case OPT_EXPECT_ENABLE_REV:
	    *state = OPT_EXPECT_ENABLE;
	    break;
	case OPT_EXPECT_DISABLE_REV:
	    *state = OPT_DISABLE;
	    break;
	default:
	    return BASE_FALSE;
    }
    return BASE_TRUE;
}

static bbool_t send_cmd_do(cli_telnet_sess *sess,
			     unsigned char option)
{
    return send_enable_option(sess, BASE_FALSE, option);
}

static bbool_t send_cmd_will(cli_telnet_sess *sess,
			       unsigned char option)
{
    return send_enable_option(sess, BASE_TRUE, option);
}

/**
 * This method will handle receiving telnet's ENABLE option negotiation.
 * This method also handle the state transition of the ENABLE
 * negotiation process.
 */
static bbool_t receive_enable_option(cli_telnet_sess *sess,
				       bbool_t is_local,
				       unsigned char option)
{
    cli_telnet_sess_option *sess_opt;
    enum cli_telnet_option_states *state;
    bbool_t opt_ena;
    BASE_ASSERT_RETURN(sess, BASE_FALSE);

    sess_opt = &sess->telnet_option[option];
    state = is_local?(&sess_opt->local_state):(&sess_opt->peer_state);
    opt_ena = is_local?sess_opt->local_is_enable:sess_opt->peer_is_enable;
    switch (*state) {
	case OPT_ENABLE:
	    /* Ignore if already enabled */
    	    break;
	case OPT_DISABLE:
	    if (opt_ena) {
		*state = OPT_ENABLE;
		send_telnet_cmd(sess, is_local?WILL:DO, option);
	    } else {
		send_telnet_cmd(sess, is_local?WONT:DONT, option);
	    }
	    break;
	case OPT_EXPECT_ENABLE:
	    *state = OPT_ENABLE;
	    break;
	case OPT_EXPECT_DISABLE:
	    *state = OPT_DISABLE;
	    break;
	case OPT_EXPECT_ENABLE_REV:
	    *state = OPT_EXPECT_DISABLE;
	    send_telnet_cmd(sess, is_local?WONT:DONT, option);
	    break;
	case OPT_EXPECT_DISABLE_REV:
	    *state = OPT_EXPECT_DISABLE;
	    break;
	default:
	    return BASE_FALSE;
    }
    return BASE_TRUE;
}

/**
 * This method will handle receiving telnet's DISABLE option negotiation.
 * This method also handle the state transition of the DISABLE
 * negotiation process.
 */
static bbool_t receive_disable_option(cli_telnet_sess *sess,
					bbool_t is_local,
					unsigned char option)
{
    cli_telnet_sess_option *sess_opt;
    enum cli_telnet_option_states *state;

    BASE_ASSERT_RETURN(sess, BASE_FALSE);

    sess_opt = &sess->telnet_option[option];
    state = is_local?(&sess_opt->local_state):(&sess_opt->peer_state);

    switch (*state) {
	case OPT_ENABLE:
	    /* Disabling option always need to be accepted */
	    *state = OPT_DISABLE;
	    send_telnet_cmd(sess, is_local?WONT:DONT, option);
    	    break;
	case OPT_DISABLE:
	    /* Ignore if already enabled */
	    break;
	case OPT_EXPECT_ENABLE:
	case OPT_EXPECT_DISABLE:
	    *state = OPT_DISABLE;
	    break;
	case OPT_EXPECT_ENABLE_REV:
	    *state = OPT_DISABLE;
	    send_telnet_cmd(sess, is_local?WONT:DONT, option);
	    break;
	case OPT_EXPECT_DISABLE_REV:
	    *state = OPT_EXPECT_ENABLE;
	    send_telnet_cmd(sess, is_local?WILL:DO, option);
	    break;
	default:
	    return BASE_FALSE;
    }
    return BASE_TRUE;
}

static bbool_t receive_do(cli_telnet_sess *sess, unsigned char option)
{
    return receive_enable_option(sess, BASE_TRUE, option);
}

static bbool_t receive_dont(cli_telnet_sess *sess, unsigned char option)
{
    return receive_disable_option(sess, BASE_TRUE, option);
}

static bbool_t receive_will(cli_telnet_sess *sess, unsigned char option)
{
    return receive_enable_option(sess, BASE_FALSE, option);
}

static bbool_t receive_wont(cli_telnet_sess *sess, unsigned char option)
{
    return receive_disable_option(sess, BASE_FALSE, option);
}

static void set_local_option(cli_telnet_sess *sess,
			     unsigned char option,
			     bbool_t enable)
{
    sess->telnet_option[option].local_is_enable = enable;
}

static void set_peer_option(cli_telnet_sess *sess,
			    unsigned char option,
			    bbool_t enable)
{
    sess->telnet_option[option].peer_is_enable = enable;
}

static bbool_t is_local_option_state_ena(cli_telnet_sess *sess,
					   unsigned char option)
{
    return (sess->telnet_option[option].local_state == OPT_ENABLE);
}

static void send_return_key(cli_telnet_sess *sess)
{
    telnet_sess_send2(sess, (unsigned char*)"\r\n", 2);
}

static void send_bell(cli_telnet_sess *sess) {
    static const unsigned char bell = 0x07;
    telnet_sess_send2(sess, &bell, 1);
}

static void send_prompt_str(cli_telnet_sess *sess)
{
    bstr_t send_data;
    char data_str[128];
    cli_telnet_fe *fe = (cli_telnet_fe *)sess->base.fe;

    send_data.ptr = data_str;
    send_data.slen = 0;

    bstrcat(&send_data, &fe->cfg.prompt_str);

    telnet_sess_send(sess, &send_data);
}

/*
 * This method is used to send error message to client, including
 * the error position of the source command.
 */
static void send_err_arg(cli_telnet_sess *sess,
			 const bcli_exec_info *info,
			 const bstr_t *msg,
			 bbool_t with_return,
			 bbool_t with_last_cmd)
{
    bstr_t send_data;
    char data_str[256];
    bsize_t len;
    unsigned i;
    cli_telnet_fe *fe = (cli_telnet_fe *)sess->base.fe;

    send_data.ptr = data_str;
    send_data.slen = 0;

    if (with_return)
	bstrcat2(&send_data, "\r\n");

    len = fe->cfg.prompt_str.slen + info->err_pos;

    /* Set the error pointer mark */
    for (i=0;i<len;++i) {
	bstrcat2(&send_data, " ");
    }
    bstrcat2(&send_data, "^");
    bstrcat2(&send_data, "\r\n");
    bstrcat(&send_data, msg);
    bstrcat(&send_data, &fe->cfg.prompt_str);
    if (with_last_cmd)
	bstrcat2(&send_data, (char *)sess->rcmd->rbuf);

    telnet_sess_send(sess, &send_data);
}

static void send_inv_arg(cli_telnet_sess *sess,
			 const bcli_exec_info *info,
			 bbool_t with_return,
			 bbool_t with_last_cmd)
{
    static const bstr_t ERR_MSG = {"%Error : Invalid Arguments\r\n", 28};
    send_err_arg(sess, info, &ERR_MSG, with_return, with_last_cmd);
}

static void send_too_many_arg(cli_telnet_sess *sess,
			      const bcli_exec_info *info,
			      bbool_t with_return,
			      bbool_t with_last_cmd)
{
    static const bstr_t ERR_MSG = {"%Error : Too Many Arguments\r\n", 29};
    send_err_arg(sess, info, &ERR_MSG, with_return, with_last_cmd);
}

static void send_hint_arg(cli_telnet_sess *sess,
			  bstr_t *send_data,
			  const bstr_t *desc,
			  bssize_t cmd_len,
			  bssize_t max_len)
{
    if ((desc) && (desc->slen > 0)) {
	int j;

	for (j=0;j<(max_len-cmd_len);++j) {
	    bstrcat2(send_data, " ");
	}
	bstrcat2(send_data, "  ");
	bstrcat(send_data, desc);
	telnet_sess_send(sess, send_data);
	send_data->slen = 0;
    }
}

/*
 * This method is used to notify to the client that the entered command
 * is ambiguous. It will show the matching command as the hint information.
 */
static void send_ambi_arg(cli_telnet_sess *sess,
			  const bcli_exec_info *info,
			  bbool_t with_return,
			  bbool_t with_last_cmd)
{
    unsigned i;
    bsize_t len;
    bstr_t send_data;
    char data[1028];
    cli_telnet_fe *fe = (cli_telnet_fe *)sess->base.fe;
    const bcli_hint_info *hint = info->hint;
    out_parse_state parse_state = OP_NORMAL;
    bssize_t max_length = 0;
    bssize_t cmd_length = 0;
    static const bstr_t sc_type = {"sc", 2};
    static const bstr_t choice_type = {"choice", 6};
    send_data.ptr = data;
    send_data.slen = 0;

    if (with_return)
	bstrcat2(&send_data, "\r\n");

    len = fe->cfg.prompt_str.slen + info->err_pos;

    for (i=0;i<len;++i) {
	bstrcat2(&send_data, " ");
    }
    bstrcat2(&send_data, "^");
    /* Get the max length of the command name */
    for (i=0;i<info->hint_cnt;++i) {
	if (hint[i].type.slen > 0) {
	    if (bstricmp(&hint[i].type, &sc_type) == 0) {
		if ((i > 0) && (!bstricmp(&hint[i-1].desc, &hint[i].desc))) {
		    cmd_length += (hint[i].name.slen + 3);
		} else {
		    cmd_length = hint[i].name.slen;
		}
	    } else {
		cmd_length = hint[i].name.slen;
	    }
	} else {
	    cmd_length = hint[i].name.slen;
	}

	if (cmd_length > max_length) {
	    max_length = cmd_length;
	}
    }

    cmd_length = 0;
    /* Build hint information */
    for (i=0;i<info->hint_cnt;++i) {
	if (hint[i].type.slen > 0) {
	    if (bstricmp(&hint[i].type, &sc_type) == 0) {
		parse_state = OP_SHORTCUT;
	    } else if (bstricmp(&hint[i].type, &choice_type) == 0) {
		parse_state = OP_CHOICE;
	    } else {
		parse_state = OP_TYPE;
	    }
	} else {
	    parse_state = OP_NORMAL;
	}

	if (parse_state != OP_SHORTCUT) {
	    bstrcat2(&send_data, "\r\n  ");
	    cmd_length = hint[i].name.slen;
	}

	switch (parse_state) {
	case OP_CHOICE:
	    /* Format : "[Choice Value]  description" */
	    bstrcat2(&send_data, "[");
	    bstrcat(&send_data, &hint[i].name);
	    bstrcat2(&send_data, "]");
	    break;
	case OP_TYPE:
	    /* Format : "<Argument Type>  description" */
	    bstrcat2(&send_data, "<");
	    bstrcat(&send_data, &hint[i].name);
	    bstrcat2(&send_data, ">");
	    break;
	case OP_SHORTCUT:
	    /* Format : "Command | sc |  description" */
	    {
		cmd_length += hint[i].name.slen;
		if ((i > 0) && (!bstricmp(&hint[i-1].desc, &hint[i].desc))) {
		    bstrcat2(&send_data, " | ");
		    cmd_length += 3;
		} else {
		    bstrcat2(&send_data, "\r\n  ");
		}
		bstrcat(&send_data, &hint[i].name);
	    }
	    break;
	default:
	    /* Command */
	    bstrcat(&send_data, &hint[i].name);
	    break;
	}

	if ((parse_state == OP_TYPE) || (parse_state == OP_CHOICE) ||
	    ((i+1) >= info->hint_cnt) ||
	    (bstrncmp(&hint[i].desc, &hint[i+1].desc, hint[i].desc.slen)))
	{
	    /* Add description info */
	    send_hint_arg(sess, &send_data,
			  &hint[i].desc, cmd_length,
			  max_length);

	    cmd_length = 0;
	}
    }
    bstrcat2(&send_data, "\r\n");
    bstrcat(&send_data, &fe->cfg.prompt_str);
    if (with_last_cmd)
	bstrcat2(&send_data, (char *)sess->rcmd->rbuf);

    telnet_sess_send(sess, &send_data);
}

/*
 * This method is to send command completion of the entered command.
 */
static void send_comp_arg(cli_telnet_sess *sess,
			  bcli_exec_info *info)
{
    bstr_t send_data;
    char data[128];

    bstrcat2(&info->hint[0].name, " ");

    send_data.ptr = data;
    send_data.slen = 0;

    bstrcat(&send_data, &info->hint[0].name);

    telnet_sess_send(sess, &send_data);
}

/*
 * This method is to process the alfa numeric character sent by client.
 */
static bbool_t handle_alfa_num(cli_telnet_sess *sess, unsigned char *data)
{
    if (is_local_option_state_ena(sess, TERM_ECHO)) {
	if (recv_buf_right_len(sess->rcmd) > 0) {
	    /* Cursor is not at EOL, insert character */
	    unsigned char echo[5] = {0x1b, 0x5b, 0x31, 0x40, 0x00};
	    echo[4] = *data;
	    telnet_sess_send2(sess, echo, 5);
	} else {
	    /* Append character */
	    telnet_sess_send2(sess, data, 1);
	}
	return BASE_TRUE;
    }
    return BASE_FALSE;
}

/*
 * This method is to process the backspace character sent by client.
 */
static bbool_t handle_backspace(cli_telnet_sess *sess, unsigned char *data)
{
    unsigned rlen = recv_buf_right_len(sess->rcmd);
    if (recv_buf_backspace(sess->rcmd)) {
	if (rlen) {
	    /*
	     * Cursor is not at the end of line, move the characters
	     * after the cursor to left
	     */
	    unsigned char echo[5] = {0x00, 0x1b, 0x5b, 0x31, 0x50};
	    echo[0] = *data;
	    telnet_sess_send2(sess, echo, 5);
	} else {
	    const static unsigned char echo[3] = {0x08, 0x20, 0x08};
	    telnet_sess_send2(sess, echo, 3);
	}
	return BASE_TRUE;
    }
    return BASE_FALSE;
}

/*
 * Syntax error handler for parser.
 */
static void on_syntax_error(bscanner *scanner)
{
    BASE_UNUSED_ARG(scanner);
    BASE_THROW(BASE_EINVAL);
}

/*
 * This method is to process the backspace character sent by client.
 */
static bstatus_t get_last_token(bstr_t *cmd, bstr_t *str)
{
    bscanner scanner;
    BASE_USE_EXCEPTION;
    bscan_init(&scanner, cmd->ptr, cmd->slen, BASE_SCAN_AUTOSKIP_WS,
		 &on_syntax_error);
    BASE_TRY {
	while (!bscan_is_eof(&scanner)) {
	    bscan_get_until_chr(&scanner, " \t\r\n", str);
	}
    }
    BASE_CATCH_ANY {
	bscan_fini(&scanner);
	return BASE_GET_EXCEPTION();
    }
    BASE_END;
    
    bscan_fini(&scanner);
    return BASE_SUCCESS;
}

/*
 * This method is to process the tab character sent by client.
 */
static bbool_t handle_tab(cli_telnet_sess *sess)
{
    bstatus_t status;
    bbool_t retval = BASE_TRUE;
    unsigned len;

    bpool_t *pool;
    bcli_cmd_val *cmd_val;
    bcli_exec_info info;
    pool = bpool_create(sess->pool->factory, "handle_tab",
			  BASE_CLI_TELNET_POOL_SIZE, BASE_CLI_TELNET_POOL_INC,
			  NULL);

    cmd_val = BASE_POOL_ZALLOC_T(pool, bcli_cmd_val);

    status = bcli_sess_parse(&sess->base, (char *)&sess->rcmd->rbuf, cmd_val,
			       pool, &info);

    len = (unsigned)bansi_strlen((char *)sess->rcmd->rbuf);

    switch (status) {
    case BASE_CLI_EINVARG:
	send_inv_arg(sess, &info, BASE_TRUE, BASE_TRUE);
	break;
    case BASE_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, BASE_TRUE, BASE_TRUE);
	break;
    case BASE_CLI_EMISSINGARG:
    case BASE_CLI_EAMBIGUOUS:
	send_ambi_arg(sess, &info, BASE_TRUE, BASE_TRUE);
	break;
    case BASE_SUCCESS:
	if (len > sess->rcmd->cur_pos)
	{
	    /* Send the cursor to EOL */
	    unsigned rlen = len - sess->rcmd->cur_pos+1;
	    unsigned char *data_sent = &sess->rcmd->rbuf[sess->rcmd->cur_pos-1];
	    telnet_sess_send2(sess, data_sent, rlen);
	}
	if (info.hint_cnt > 0) {
	    /* Complete command */
	    bstr_t cmd = bstr((char *)sess->rcmd->rbuf);
	    bstr_t last_token;

	    if (get_last_token(&cmd, &last_token) == BASE_SUCCESS) {
		/* Hint contains the match to the last command entered */
		bstr_t *hint_info = &info.hint[0].name;
		bstrtrim(&last_token);
		if (hint_info->slen >= last_token.slen) {
		    hint_info->slen -= last_token.slen;
		    bmemmove(hint_info->ptr,
			       &hint_info->ptr[last_token.slen],
			       hint_info->slen);
		}
		send_comp_arg(sess, &info);

		bmemcpy(&sess->rcmd->rbuf[len], info.hint[0].name.ptr,
			  info.hint[0].name.slen);

		len += (unsigned)info.hint[0].name.slen;
		sess->rcmd->rbuf[len] = 0;
	    }
	} else {
	    retval = BASE_FALSE;
	}
	break;
    }
    sess->rcmd->len = len;
    sess->rcmd->cur_pos = sess->rcmd->len;

    bpool_release(pool);
    return retval;
}

/*
 * This method is to process the return character sent by client.
 */
static bbool_t handle_return(cli_telnet_sess *sess)
{
    bstatus_t status;
    bbool_t retval = BASE_TRUE;

    bpool_t *pool;
    bcli_exec_info info;

    send_return_key(sess);
    insert_history(sess, (char *)&sess->rcmd->rbuf);

    pool = bpool_create(sess->pool->factory, "handle_return",
			  BASE_CLI_TELNET_POOL_SIZE, BASE_CLI_TELNET_POOL_INC,
			  NULL);

    status = bcli_sess_exec(&sess->base, (char *)&sess->rcmd->rbuf,
			      pool, &info);

    switch (status) {
    case BASE_CLI_EINVARG:
	send_inv_arg(sess, &info, BASE_FALSE, BASE_FALSE);
	break;
    case BASE_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, BASE_FALSE, BASE_FALSE);
	break;
    case BASE_CLI_EAMBIGUOUS:
    case BASE_CLI_EMISSINGARG:
	send_ambi_arg(sess, &info, BASE_FALSE, BASE_FALSE);
	break;
    case BASE_CLI_EEXIT:
	retval = BASE_FALSE;
	break;
    case BASE_SUCCESS:
	send_prompt_str(sess);
	break;
    }
    if (retval) {
	sess->rcmd->rbuf[0] = 0;
	sess->rcmd->len = 0;
	sess->rcmd->cur_pos = sess->rcmd->len;
    }

    bpool_release(pool);
    return retval;
}

/*
 * This method is to process the right key character sent by client.
 */
static bbool_t handle_right_key(cli_telnet_sess *sess)
{
    if (recv_buf_right_len(sess->rcmd)) {
	unsigned char *data = &sess->rcmd->rbuf[sess->rcmd->cur_pos++];
	telnet_sess_send2(sess, data, 1);
	return BASE_TRUE;
    }
    return BASE_FALSE;
}

/*
 * This method is to process the left key character sent by client.
 */
static bbool_t handle_left_key(cli_telnet_sess *sess)
{
    static const unsigned char move_cursor_left = 0x08;
    if (sess->rcmd->cur_pos) {
	telnet_sess_send2(sess, &move_cursor_left, 1);
	--sess->rcmd->cur_pos;
	return BASE_TRUE;
    }
    return BASE_FALSE;
}

/*
 * This method is to process the up/down key character sent by client.
 */
static bbool_t handle_up_down(cli_telnet_sess *sess, bbool_t is_up)
{
    bstr_t *history;

    BASE_ASSERT_RETURN(sess, BASE_FALSE);

    history = get_prev_history(sess, is_up);
    if (history) {
	bstr_t send_data;
	char str[BASE_CLI_MAX_CMDBUF];
	enum {
	    MOVE_CURSOR_LEFT = 0x08,
	    CLEAR_CHAR = 0x20
	};
	send_data.ptr = str;
	send_data.slen = 0;

	/* Move cursor position to the beginning of line */
	if (sess->rcmd->cur_pos > 0) {
	    bmemset(send_data.ptr, MOVE_CURSOR_LEFT, sess->rcmd->cur_pos);
	    send_data.slen = sess->rcmd->cur_pos;
	}

	if (sess->rcmd->len > (unsigned)history->slen) {
	    /* Clear the command currently shown*/
	    unsigned buf_len = sess->rcmd->len;
	    bmemset(&send_data.ptr[send_data.slen], CLEAR_CHAR, buf_len);
	    send_data.slen += buf_len;

	    /* Move cursor position to the beginning of line */
	    bmemset(&send_data.ptr[send_data.slen], MOVE_CURSOR_LEFT,
		      buf_len);
	    send_data.slen += buf_len;
	}
	/* Send data */
	bstrcat(&send_data, history);
	telnet_sess_send(sess, &send_data);
	bansi_strncpy((char*)&sess->rcmd->rbuf, history->ptr, history->slen);
	sess->rcmd->rbuf[history->slen] = 0;
	sess->rcmd->len = (unsigned)history->slen;
	sess->rcmd->cur_pos = sess->rcmd->len;
	return BASE_TRUE;
    }
    return BASE_FALSE;
}

static bstatus_t process_vt100_cmd(cli_telnet_sess *sess,
				     unsigned char *cmd)
{
    bstatus_t status = BASE_TRUE;
    switch (*cmd) {
	case TC_ESC:
	    break;
	case TC_UP:
	    status = handle_up_down(sess, BASE_TRUE);
	    break;
	case TC_DOWN:
	    status = handle_up_down(sess, BASE_FALSE);
	    break;
	case TC_RIGHT:
	    status = handle_right_key(sess);
	    break;
	case TC_LEFT:
	    status = handle_left_key(sess);
	    break;
	case TC_END:
	    break;
	case TC_HOME:
	    break;
	case TC_CTRL_C:
	    break;
	case TC_CR:
	    break;
	case TC_BS:
	    break;
	case TC_TAB:
	    break;
	case TC_QM:
	    break;
	case TC_BELL:
	    break;
	case TC_DEL:
	    break;
    };
    return status;
}

void bcli_telnet_cfg_default(bcli_telnet_cfg *param)
{
    bassert(param);

    bbzero(param, sizeof(*param));
    param->port = BASE_CLI_TELNET_PORT;
    param->log_level = BASE_CLI_TELNET_LOG_LEVEL;
}

/*
 * Send a message to a telnet session
 */
static bstatus_t telnet_sess_send(cli_telnet_sess *sess,
				    const bstr_t *str)
{
    bssize_t sz;
    bstatus_t status = BASE_SUCCESS;

    sz = str->slen;
    if (!sz)
        return BASE_SUCCESS;

    bmutex_lock(sess->smutex);

    if (sess->buf_len == 0)
        status = bactivesock_send(sess->asock, &sess->op_key,
                                    str->ptr, &sz, 0);
    /* If we cannot send now, append it at the end of the buffer
     * to be sent later.
     */
    if (sess->buf_len > 0 ||
        (status != BASE_SUCCESS && status != BASE_EPENDING))
    {
        int clen = (int)sz;

        if (sess->buf_len + clen > CLI_TELNET_BUF_SIZE)
            clen = CLI_TELNET_BUF_SIZE - sess->buf_len;
        if (clen > 0)
            bmemmove(sess->buf + sess->buf_len, str->ptr, clen);
        if (clen < sz) {
            bansi_snprintf((char *)sess->buf + CLI_TELNET_BUF_SIZE,
                             MAX_CUT_MSG_LEN, CUT_MSG);
            sess->buf_len = (unsigned)(CLI_TELNET_BUF_SIZE +
                            bansi_strlen((char *)sess->buf+
				            CLI_TELNET_BUF_SIZE));
        } else
            sess->buf_len += clen;
    } else if (status == BASE_SUCCESS && sz < str->slen) {
        bmutex_unlock(sess->smutex);
        return BASE_CLI_ETELNETLOST;
    }

    bmutex_unlock(sess->smutex);

    return BASE_SUCCESS;
}

/*
 * Send a message to a telnet session with formatted text
 * (add single linefeed character with carriage return)
 */
static bstatus_t telnet_sess_send_with_format(cli_telnet_sess *sess,
						const bstr_t *str)
{
    bscanner scanner;
    bstr_t out_str;
    static const bstr_t CR_LF = {("\r\n"), 2};
    int str_len = 0;
    char *str_begin = 0;

    BASE_USE_EXCEPTION;

    bscan_init(&scanner, str->ptr, str->slen,
	         BASE_SCAN_AUTOSKIP_WS, &on_syntax_error);

    str_begin = scanner.begin;

    BASE_TRY {
	while (!bscan_is_eof(&scanner)) {
	    bscan_get_until_ch(&scanner, '\n', &out_str);
	    str_len = (int)(scanner.curptr - str_begin);
	    if (*scanner.curptr == '\n') {
		if ((str_len > 1) && (out_str.ptr[str_len-2] == '\r'))
		{
		    continue;
		} else {
		    int str_pos = (int)(str_begin - scanner.begin);

		    if (str_len > 0) {
			bstr_t s;
			bstrset(&s, &str->ptr[str_pos], str_len);
			telnet_sess_send(sess, &s);
		    }
		    telnet_sess_send(sess, &CR_LF);

		    if (!bscan_is_eof(&scanner)) {
			bscan_advance_n(&scanner, 1, BASE_TRUE);
			str_begin = scanner.curptr;
		    }
		}
	    } else {
		bstr_t s;
		int str_pos = (int)(str_begin - scanner.begin);

		bstrset(&s, &str->ptr[str_pos], str_len);
		telnet_sess_send(sess, &s);
	    }
	}
    }
    BASE_CATCH_ANY {
	bscan_fini(&scanner);
	return (BASE_GET_EXCEPTION());
    }
    BASE_END;

    bscan_fini(&scanner);
    return BASE_SUCCESS;
}

static bstatus_t telnet_sess_send2(cli_telnet_sess *sess,
                                     const unsigned char *str, int len)
{
    bstr_t s;

    bstrset(&s, (char *)str, len);
    return telnet_sess_send(sess, &s);
}

static void telnet_sess_destroy(bcli_sess *sess)
{
    cli_telnet_sess *tsess = (cli_telnet_sess *)sess;
    bmutex_t *mutex = ((cli_telnet_fe *)sess->fe)->mutex;

    bmutex_lock(mutex);
    blist_erase(sess);
    bmutex_unlock(mutex);

    bmutex_lock(tsess->smutex);
    bmutex_unlock(tsess->smutex);
    bactivesock_close(tsess->asock);
    bmutex_destroy(tsess->smutex);
    bpool_release(tsess->pool);
}

static void telnet_fe_write_log(bcli_front_end *fe, int level,
		                const char *data, bsize_t len)
{
    cli_telnet_fe *tfe = (cli_telnet_fe *)fe;
    bcli_sess *sess;

    bmutex_lock(tfe->mutex);

    sess = tfe->sess_head.next;
    while (sess != &tfe->sess_head) {
        cli_telnet_sess *tsess = (cli_telnet_sess *)sess;

        sess = sess->next;
	if (tsess->base.log_level >= level) {
	    bstr_t s;

	    bstrset(&s, (char *)data, len);
	    telnet_sess_send_with_format(tsess, &s);
	}
    }

    bmutex_unlock(tfe->mutex);
}

static void telnet_fe_destroy(bcli_front_end *fe)
{
    cli_telnet_fe *tfe = (cli_telnet_fe *)fe;
    bcli_sess *sess;

    tfe->is_quitting = BASE_TRUE;
    if (tfe->worker_thread) {
        bthreadJoin(tfe->worker_thread);
    }

    bmutex_lock(tfe->mutex);

    /* Destroy all the sessions */
    sess = tfe->sess_head.next;
    while (sess != &tfe->sess_head) {
        (*sess->op->destroy)(sess);
        sess = tfe->sess_head.next;
    }

    bmutex_unlock(tfe->mutex);

    bactivesock_close(tfe->asock);
    if (tfe->own_ioqueue)
        bioqueue_destroy(tfe->cfg.ioqueue);

    if (tfe->worker_thread) {
	bthreadDestroy(tfe->worker_thread);
	tfe->worker_thread = NULL;
    }

    bmutex_destroy(tfe->mutex);
    bpool_release(tfe->pool);
}

static int poll_worker_thread(void *p)
{
    cli_telnet_fe *fe = (cli_telnet_fe *)p;

    while (!fe->is_quitting) {
	btime_val delay = {0, 50};
        bioqueue_poll(fe->cfg.ioqueue, &delay);
    }

    return 0;
}

static bbool_t telnet_sess_on_data_sent(bactivesock_t *asock,
 				          bioqueue_op_key_t *op_key,
				          bssize_t sent)
{
    cli_telnet_sess *sess = (cli_telnet_sess *)
			    bactivesock_get_user_data(asock);

    BASE_UNUSED_ARG(op_key);

    if (sent <= 0) {
	MTRACE( "Error On data send");
        bcli_sess_end_session(&sess->base);
        return BASE_FALSE;
    }

    bmutex_lock(sess->smutex);

    if (sess->buf_len) {
        int len = sess->buf_len;

        sess->buf_len = 0;
        if (telnet_sess_send2(sess, sess->buf, len) != BASE_SUCCESS) {
            bmutex_unlock(sess->smutex);
            bcli_sess_end_session(&sess->base);
            return BASE_FALSE;
        }
    }

    bmutex_unlock(sess->smutex);

    return BASE_TRUE;
}

static bbool_t telnet_sess_on_data_read(bactivesock_t *asock,
		                          void *data,
			                  bsize_t size,
			                  bstatus_t status,
			                  bsize_t *remainder)
{
    cli_telnet_sess *sess = (cli_telnet_sess *)
                            bactivesock_get_user_data(asock);
    cli_telnet_fe *tfe = (cli_telnet_fe *)sess->base.fe;
    unsigned char *cdata = (unsigned char*)data;
    bstatus_t is_valid = BASE_TRUE;

    BASE_UNUSED_ARG(size);
    BASE_UNUSED_ARG(remainder);

    if (tfe->is_quitting)
        return BASE_FALSE;

    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
	MTRACE( "Error on data read %d", status);
        return BASE_FALSE;
    }

    bmutex_lock(sess->smutex);

    switch (sess->parse_state) {
	case ST_CR:
	    sess->parse_state = ST_NORMAL;
	    if (*cdata == 0 || *cdata == '\n') {		
		bmutex_unlock(sess->smutex);
		is_valid = handle_return(sess);
		if (!is_valid)
		    return BASE_FALSE;
		bmutex_lock(sess->smutex);
	    }
	    break;
	case ST_NORMAL:
	    if (*cdata == IAC) {
		sess->parse_state = ST_IAC;
	    } else if (*cdata == 127) {
		is_valid = handle_backspace(sess, cdata);
	    } else if (*cdata == 27) {
		sess->parse_state = ST_ESC;
	    } else {
		if (recv_buf_insert(sess->rcmd, cdata)) {
		    if (*cdata == '\r') {
			sess->parse_state = ST_CR;
		    } else if ((*cdata == '\t') || (*cdata == '?')) {
			is_valid = handle_tab(sess);
		    } else if (*cdata > 31 && *cdata < 127) {
			is_valid = handle_alfa_num(sess, cdata);
		    }
		} else {
		    is_valid = BASE_FALSE;
		}
	    }
	    break;
	case ST_ESC:
	    if (*cdata == 91) {
		sess->parse_state = ST_VT100;
	    } else {
		sess->parse_state = ST_NORMAL;
	    }
	    break;
	case ST_VT100:
	    sess->parse_state = ST_NORMAL;
	    is_valid = process_vt100_cmd(sess, cdata);
	    break;
	case ST_IAC:
	    switch ((unsigned) *cdata) {
		case DO:
		    sess->parse_state = ST_DO;
		    break;
		case DONT:
		    sess->parse_state = ST_DONT;
		    break;
		case WILL:
		    sess->parse_state = ST_WILL;
		    break;
		case WONT:
		    sess->parse_state = ST_WONT;
		    break;
		default:
		    sess->parse_state = ST_NORMAL;
		    break;
	    }
	    break;
	case ST_DO:
	    receive_do(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	case ST_DONT:
	    receive_dont(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	case ST_WILL:
	    receive_will(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	case ST_WONT:
	    receive_wont(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	default:
	    sess->parse_state = ST_NORMAL;
	    break;
    }
    if (!is_valid) {
	send_bell(sess);
    }

    bmutex_unlock(sess->smutex);

    return BASE_TRUE;
}

static bbool_t telnet_fe_on_accept(bactivesock_t *asock,
				     bsock_t newsock,
				     const bsockaddr_t *src_addr,
				     int src_addr_len,
				     bstatus_t status)
{
    cli_telnet_fe *fe = (cli_telnet_fe *) bactivesock_get_user_data(asock);

    bstatus_t sstatus;
    bpool_t *pool;
    cli_telnet_sess *sess = NULL;
    bactivesock_cb asock_cb;

    BASE_UNUSED_ARG(src_addr);
    BASE_UNUSED_ARG(src_addr_len);

    if (fe->is_quitting)
        return BASE_FALSE;

    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
	MTRACE( "Error on data accept %d", status);
	if (status == BASE_ESOCKETSTOP)
	    telnet_restart(fe);

        return BASE_FALSE;
    }

    /* An incoming connection is accepted, create a new session */
    pool = bpool_create(fe->pool->factory, "telnet_sess",
                          BASE_CLI_TELNET_POOL_SIZE, BASE_CLI_TELNET_POOL_INC,
                          NULL);
    if (!pool) {
        MTRACE("Not enough memory to create a new telnet session");
        return BASE_TRUE;
    }

    sess = BASE_POOL_ZALLOC_T(pool, cli_telnet_sess);
    sess->pool = pool;
    sess->base.fe = &fe->base;
    sess->base.log_level = fe->cfg.log_level;
    sess->base.op = BASE_POOL_ZALLOC_T(pool, struct bcli_sess_op);
    sess->base.op->destroy = &telnet_sess_destroy;
    bbzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = &telnet_sess_on_data_read;
    asock_cb.on_data_sent = &telnet_sess_on_data_sent;
    sess->rcmd = BASE_POOL_ZALLOC_T(pool, telnet_recv_buf);
    sess->history = BASE_POOL_ZALLOC_T(pool, struct cmd_history);
    blist_init(sess->history);
    sess->active_history = sess->history;

    sstatus = bmutex_create_recursive(pool, "mutex_telnet_sess",
                                        &sess->smutex);
    if (sstatus != BASE_SUCCESS)
        goto on_exit;

    sstatus = bactivesock_create(pool, newsock, bSOCK_STREAM(),
                                   NULL, fe->cfg.ioqueue,
			           &asock_cb, sess, &sess->asock);
    if (sstatus != BASE_SUCCESS) {
        MTRACE("Failure creating active socket");
        goto on_exit;
    }

    bmemset(sess->telnet_option, 0, sizeof(sess->telnet_option));
    set_local_option(sess, TRANSMIT_BINARY, BASE_TRUE);
    set_local_option(sess, STATUS, BASE_TRUE);
    set_local_option(sess, SUPPRESS_GA, BASE_TRUE);
    set_local_option(sess, TIMING_MARK, BASE_TRUE);
    set_local_option(sess, TERM_SPEED, BASE_TRUE);
    set_local_option(sess, TERM_TYPE, BASE_TRUE);

    set_peer_option(sess, TRANSMIT_BINARY, BASE_TRUE);
    set_peer_option(sess, SUPPRESS_GA, BASE_TRUE);
    set_peer_option(sess, STATUS, BASE_TRUE);
    set_peer_option(sess, TIMING_MARK, BASE_TRUE);
    set_peer_option(sess, TERM_ECHO, BASE_TRUE);

    send_cmd_do(sess, SUPPRESS_GA);
    send_cmd_will(sess, TERM_ECHO);
    send_cmd_will(sess, STATUS);
    send_cmd_will(sess, SUPPRESS_GA);

    /* Send prompt string */
    telnet_sess_send(sess, &fe->cfg.prompt_str);

    /* Start reading for input from the new telnet session */
    sstatus = bactivesock_start_read(sess->asock, pool, 1, 0);
    if (sstatus != BASE_SUCCESS) {
        MTRACE( "Failure reading active socket");
        goto on_exit;
    }

    bioqueue_op_key_init(&sess->op_key, sizeof(sess->op_key));
    bmutex_lock(fe->mutex);
    blist_push_back(&fe->sess_head, &sess->base);
    bmutex_unlock(fe->mutex);

    return BASE_TRUE;

on_exit:
    if (sess->asock)
        bactivesock_close(sess->asock);
    else
        bsock_close(newsock);

    if (sess->smutex)
        bmutex_destroy(sess->smutex);

    bpool_release(pool);

    return BASE_TRUE;
}

bstatus_t bcli_telnet_create(bcli_t *cli,
					 bcli_telnet_cfg *param,
					 bcli_front_end **p_fe)
{
    cli_telnet_fe *fe;
    bpool_t *pool;
    bstatus_t status;

    BASE_ASSERT_RETURN(cli, BASE_EINVAL);

    pool = bpool_create(bcli_get_param(cli)->pf, "telnet_fe",
                          BASE_CLI_TELNET_POOL_SIZE, BASE_CLI_TELNET_POOL_INC,
                          NULL);
    fe = BASE_POOL_ZALLOC_T(pool, cli_telnet_fe);
    if (!fe)
        return BASE_ENOMEM;

    fe->base.op = BASE_POOL_ZALLOC_T(pool, struct bcli_front_end_op);

    if (!param)
        bcli_telnet_cfg_default(&fe->cfg);
    else
        bmemcpy(&fe->cfg, param, sizeof(*param));

    blist_init(&fe->sess_head);
    fe->base.cli = cli;
    fe->base.type = BASE_CLI_TELNET_FRONT_END;
    fe->base.op->on_write_log = &telnet_fe_write_log;
    fe->base.op->on_destroy = &telnet_fe_destroy;
    fe->pool = pool;

    if (!fe->cfg.ioqueue) {
        /* Create own ioqueue if application doesn't supply one */
        status = bioqueue_create(pool, 8, &fe->cfg.ioqueue);
        if (status != BASE_SUCCESS)
            goto on_exit;
        fe->own_ioqueue = BASE_TRUE;
    }

    status = bmutex_create_recursive(pool, "mutex_telnet_fe", &fe->mutex);
    if (status != BASE_SUCCESS)
        goto on_exit;

    /* Start telnet daemon */
    status = telnet_start(fe);
    if (status != BASE_SUCCESS)
	goto on_exit;

    bcli_register_front_end(cli, &fe->base);

    if (p_fe)
        *p_fe = &fe->base;

    return BASE_SUCCESS;

on_exit:
    if (fe->own_ioqueue)
        bioqueue_destroy(fe->cfg.ioqueue);

    if (fe->mutex)
        bmutex_destroy(fe->mutex);

    bpool_release(pool);
    return status;
}

static bstatus_t telnet_start(cli_telnet_fe *fe)
{
    bsock_t sock = BASE_INVALID_SOCKET;
    bactivesock_cb asock_cb;
    bsockaddr_in addr;
    bstatus_t status;
    int val;
    int restart_retry;
    unsigned msec;

    /* Start telnet daemon */
    status = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &sock);

    if (status != BASE_SUCCESS)
        goto on_exit;

    bsockaddr_in_init(&addr, NULL, fe->cfg.port);

    val = 1;
    status = bsock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				&val, sizeof(val));

    if (status != BASE_SUCCESS) {
	BASE_PERROR(3, (THIS_FILE, status, "Failed setting socket options"));
    }

    /* The loop is silly, but what else can we do? */
    for (msec=MIN_WAIT_ON_TELNET_RESTART, restart_retry=0;
	 restart_retry < MAX_RETRY_ON_TELNET_RESTART;
	 ++restart_retry, msec=(msec<MAX_WAIT_ON_TELNET_RESTART?
		          msec*2 : MAX_WAIT_ON_TELNET_RESTART))
    {
	status = bsock_bind(sock, &addr, sizeof(addr));
	if (status != BASE_STATUS_FROM_OS(EADDRINUSE))
	    break;
	BASE_ERROR("Address is still in use, retrying..");
	bthreadSleepMs(msec);
    }

    if (status == BASE_SUCCESS) {
	int addr_len = sizeof(addr);

	status = bsock_getsockname(sock, &addr, &addr_len);
	if (status != BASE_SUCCESS)
	    goto on_exit;

        fe->cfg.port = bsockaddr_in_get_port(&addr);

	if (fe->cfg.prompt_str.slen == 0) {
	    bstr_t prompt_sign = {"> ", 2};
	    char *prompt_data = bpool_alloc(fe->pool,
					      bgethostname()->slen+2);
	    fe->cfg.prompt_str.ptr = prompt_data;

	    bstrcpy(&fe->cfg.prompt_str, bgethostname());
	    bstrcat(&fe->cfg.prompt_str, &prompt_sign);
	}
    } else {
        BASE_PERROR(3, (THIS_FILE, status, "Failed binding the socket"));
        goto on_exit;
    }

    status = bsock_listen(sock, 4);
    if (status != BASE_SUCCESS)
        goto on_exit;

    bbzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_accept_complete2 = &telnet_fe_on_accept;
    status = bactivesock_create(fe->pool, sock, bSOCK_STREAM(),
                                  NULL, fe->cfg.ioqueue,
		                  &asock_cb, fe, &fe->asock);
    if (status != BASE_SUCCESS)
        goto on_exit;

    status = bactivesock_start_accept(fe->asock, fe->pool);
    if (status != BASE_SUCCESS)
        goto on_exit;

    if (fe->own_ioqueue) {
        /* Create our own worker thread */
        status = bthreadCreate(fe->pool, "worker_telnet_fe",
                                  &poll_worker_thread, fe, 0, 0,
                                  &fe->worker_thread);
        if (status != BASE_SUCCESS)
            goto on_exit;
    }

    return BASE_SUCCESS;

on_exit:
    if (fe->cfg.on_started) {
	(*fe->cfg.on_started)(status);
    }

    if (fe->asock)
        bactivesock_close(fe->asock);
    else if (sock != BASE_INVALID_SOCKET)
        bsock_close(sock);

    if (fe->own_ioqueue)
        bioqueue_destroy(fe->cfg.ioqueue);

    if (fe->mutex)
        bmutex_destroy(fe->mutex);

    bpool_release(fe->pool);
    return status;
}

static bstatus_t telnet_restart(cli_telnet_fe *fe)
{
    bstatus_t status;
    bcli_sess *sess;

    fe->is_quitting = BASE_TRUE;
    if (fe->worker_thread) {
	bthreadJoin(fe->worker_thread);
    }

    bmutex_lock(fe->mutex);

    /* Destroy all the sessions */
    sess = fe->sess_head.next;
    while (sess != &fe->sess_head) {
	(*sess->op->destroy)(sess);
	sess = fe->sess_head.next;
    }

    bmutex_unlock(fe->mutex);

    /** Close existing activesock **/
    status = bactivesock_close(fe->asock);
    if (status != BASE_SUCCESS)
	goto on_exit;

    if (fe->worker_thread) {
	bthreadDestroy(fe->worker_thread);
	fe->worker_thread = NULL;
    }

    fe->is_quitting = BASE_FALSE;

    /** Start Telnet **/
    status = telnet_start(fe);
    if (status == BASE_SUCCESS) {
	if (fe->cfg.on_started) {
	    (*fe->cfg.on_started)(status);
	}
	MTRACE( "Telnet Restarted");
    }
on_exit:
    return status;
}

bstatus_t bcli_telnet_get_info(bcli_front_end *fe,
					   bcli_telnet_info *info)
{
    bsockaddr hostip;
    bstatus_t status;
    cli_telnet_fe *tfe = (cli_telnet_fe*) fe;

    BASE_ASSERT_RETURN(fe && (fe->type == BASE_CLI_TELNET_FRONT_END) && info,
		     BASE_EINVAL);

    bstrset(&info->ip_address, info->buf_, 0);

    status = bgethostip(bAF_INET(), &hostip);
    if (status != BASE_SUCCESS)
	return status;

    bsockaddr_print(&hostip, info->buf_, sizeof(info->buf_), 0);
    bstrset2(&info->ip_address, info->buf_);

    info->port = tfe->cfg.port;

    return BASE_SUCCESS;
}
