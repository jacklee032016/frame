/* 
 *
 */

#include <utilCliImp.h>
#include <utilCliConsole.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLog.h>
#include <baseOs.h>
#include <basePool.h>
#include <baseString.h>
#include <utilErrno.h>

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

struct cli_console_fe
{
    bcli_front_end    base;
    bpool_t          *pool;
    bcli_sess        *sess;
    bthread_t        *input_thread;
    bbool_t           thread_quit;
    bsem_t           *thread_sem;
    bcli_console_cfg  cfg;

    struct async_input_t
    {
        char       *buf;
        unsigned    maxlen;
        bsem_t   *sem;
    } input;
};

static void console_write_log(bcli_front_end *fe, int level,
		              const char *data, bsize_t len)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    if (cfe->sess->log_level > level)
        printf("%.*s", (int)len, data);
}

static void console_quit(bcli_front_end *fe, bcli_sess *req)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    BASE_UNUSED_ARG(req);

    bassert(cfe);
    if (cfe->input_thread) {
        cfe->thread_quit = BASE_TRUE;
        bsem_post(cfe->input.sem);
        bsem_post(cfe->thread_sem);
    }
}

static void console_destroy(bcli_front_end *fe)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    bassert(cfe);
    console_quit(fe, NULL);

    if (cfe->input_thread)
        bthreadJoin(cfe->input_thread);

    if (cfe->input_thread) {
        bthreadDestroy(cfe->input_thread);
	cfe->input_thread = NULL;
    }

    bsem_destroy(cfe->thread_sem);
    bsem_destroy(cfe->input.sem);
    bpool_release(cfe->pool);
}

void bcli_console_cfg_default(bcli_console_cfg *param)
{
    bassert(param);

    param->log_level = BASE_CLI_CONSOLE_LOG_LEVEL;
    param->prompt_str.slen = 0;
    param->quit_command.slen = 0;
}

bstatus_t bcli_console_create(bcli_t *cli,
					  const bcli_console_cfg *param,
					  bcli_sess **p_sess,
					  bcli_front_end **p_fe)
{
    bcli_sess *sess;
    struct cli_console_fe *fe;
    bcli_console_cfg cfg;
    bpool_t *pool;
    bstatus_t status;

    BASE_ASSERT_RETURN(cli && p_sess, BASE_EINVAL);

    pool = bpool_create(bcli_get_param(cli)->pf, "console_fe",
                          BASE_CLI_CONSOLE_POOL_SIZE, BASE_CLI_CONSOLE_POOL_INC,
                          NULL);
    if (!pool)
        return BASE_ENOMEM;

    sess = BASE_POOL_ZALLOC_T(pool, bcli_sess);
    fe = BASE_POOL_ZALLOC_T(pool, struct cli_console_fe);

    if (!param) {
        bcli_console_cfg_default(&cfg);
        param = &cfg;
    }
    sess->fe = &fe->base;
    sess->log_level = param->log_level;
    sess->op = BASE_POOL_ZALLOC_T(pool, struct bcli_sess_op);
    fe->base.op = BASE_POOL_ZALLOC_T(pool, struct bcli_front_end_op);
    fe->base.cli = cli;
    fe->base.type = BASE_CLI_CONSOLE_FRONT_END;
    fe->base.op->on_write_log = &console_write_log;
    fe->base.op->on_quit = &console_quit;
    fe->base.op->on_destroy = &console_destroy;
    fe->pool = pool;
    fe->sess = sess;
    status = bsem_create(pool, "console_fe", 0, 1, &fe->thread_sem);
    if (status != BASE_SUCCESS)
	return status;

    status = bsem_create(pool, "console_fe", 0, 1, &fe->input.sem);
    if (status != BASE_SUCCESS)
	return status;

    bcli_register_front_end(cli, &fe->base);
    if (param->prompt_str.slen == 0) {
	bstr_t prompt_sign = bstr(">>> ");
	fe->cfg.prompt_str.ptr = bpool_alloc(fe->pool, prompt_sign.slen+1);
	bstrcpy(&fe->cfg.prompt_str, &prompt_sign);
    } else {
	fe->cfg.prompt_str.ptr = bpool_alloc(fe->pool,
					       param->prompt_str.slen+1);
	bstrcpy(&fe->cfg.prompt_str, &param->prompt_str);
    }
    fe->cfg.prompt_str.ptr[fe->cfg.prompt_str.slen] = 0;

    if (param->quit_command.slen)
	bstrdup(fe->pool, &fe->cfg.quit_command, &param->quit_command);

    *p_sess = sess;
    if (p_fe)
        *p_fe = &fe->base;

    return BASE_SUCCESS;
}

static void send_prompt_str(bcli_sess *sess)
{
    bstr_t send_data;
    char data_str[128];
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    send_data.ptr = data_str;
    send_data.slen = 0;

    bstrcat(&send_data, &fe->cfg.prompt_str);
    send_data.ptr[send_data.slen] = 0;

    printf("%s", send_data.ptr);
}

static void send_err_arg(bcli_sess *sess,
			 const bcli_exec_info *info,
			 const bstr_t *msg,
			 bbool_t with_return)
{
    bstr_t send_data;
    char data_str[256];
    bsize_t len;
    unsigned i;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    send_data.ptr = data_str;
    send_data.slen = 0;

    if (with_return)
	bstrcat2(&send_data, "\r\n");

    len = fe->cfg.prompt_str.slen + info->err_pos;

    for (i=0;i<len;++i) {
	bstrcat2(&send_data, " ");
    }
    bstrcat2(&send_data, "^");
    bstrcat2(&send_data, "\r\n");
    bstrcat(&send_data, msg);
    bstrcat(&send_data, &fe->cfg.prompt_str);

    send_data.ptr[send_data.slen] = 0;
    printf("%s", send_data.ptr);
}

static void send_inv_arg(bcli_sess *sess,
			 const bcli_exec_info *info,
			 bbool_t with_return)
{
    static const bstr_t ERR_MSG = {"%Error : Invalid Arguments\r\n", 28};
    send_err_arg(sess, info, &ERR_MSG, with_return);
}

static void send_too_many_arg(bcli_sess *sess,
			      const bcli_exec_info *info,
			      bbool_t with_return)
{
    static const bstr_t ERR_MSG = {"%Error : Too Many Arguments\r\n", 29};
    send_err_arg(sess, info, &ERR_MSG, with_return);
}

static void send_hint_arg(bstr_t *send_data,
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
	send_data->ptr[send_data->slen] = 0;
	printf("%s", send_data->ptr);
	send_data->slen = 0;
    }
}

static void send_ambi_arg(bcli_sess *sess,
			  const bcli_exec_info *info,
			  bbool_t with_return)
{
    unsigned i;
    bsize_t len;
    bstr_t send_data;
    char data[1028];
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
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
	    bstrcat2(&send_data, "[");
	    bstrcat(&send_data, &hint[i].name);
	    bstrcat2(&send_data, "]");
	    break;
	case OP_TYPE:
	    bstrcat2(&send_data, "<");
	    bstrcat(&send_data, &hint[i].type);
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
	    bstrcat(&send_data, &hint[i].name);
	    break;
	}

	if ((parse_state == OP_TYPE) || (parse_state == OP_CHOICE) ||
	    ((i+1) >= info->hint_cnt) ||
	    (bstrncmp(&hint[i].desc, &hint[i+1].desc, hint[i].desc.slen)))
	{
	    /* Add description info */
	    send_hint_arg(&send_data, &hint[i].desc, cmd_length, max_length);

	    cmd_length = 0;
	}
    }
    bstrcat2(&send_data, "\r\n");
    bstrcat(&send_data, &fe->cfg.prompt_str);
    send_data.ptr[send_data.slen] = 0;
    printf("%s", send_data.ptr);
}

static bbool_t handle_hint(bcli_sess *sess)
{
    bstatus_t status;
    bbool_t retval = BASE_TRUE;

    bpool_t *pool;
    bcli_cmd_val *cmd_val;
    bcli_exec_info info;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    char *recv_buf = fe->input.buf;
    bcli_t *cli = sess->fe->cli;

    pool = bpool_create(bcli_get_param(cli)->pf, "handle_hint",
                          BASE_CLI_CONSOLE_POOL_SIZE, BASE_CLI_CONSOLE_POOL_INC,
                          NULL);

    cmd_val = BASE_POOL_ZALLOC_T(pool, bcli_cmd_val);

    status = bcli_sess_parse(sess, recv_buf, cmd_val,
			       pool, &info);

    switch (status) {
    case BASE_CLI_EINVARG:
	send_inv_arg(sess, &info, BASE_TRUE);
	break;
    case BASE_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, BASE_TRUE);
	break;
    case BASE_CLI_EMISSINGARG:
    case BASE_CLI_EAMBIGUOUS:
	send_ambi_arg(sess, &info, BASE_TRUE);
	break;
    case BASE_SUCCESS:
	if (info.hint_cnt > 0) {
	    /* Compelete command */
	    send_ambi_arg(sess, &info, BASE_TRUE);
	} else {
	    retval = BASE_FALSE;
	}
	break;
    }

    bpool_release(pool);
    return retval;
}

static bbool_t handle_exec(bcli_sess *sess)
{
    bstatus_t status;
    bbool_t retval = BASE_TRUE;

    bpool_t *pool;
    bcli_exec_info info;
    bcli_t *cli = sess->fe->cli;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    char *recv_buf = fe->input.buf;

    printf("\r\n");

    pool = bpool_create(bcli_get_param(cli)->pf, "handle_exec",
			  BASE_CLI_CONSOLE_POOL_SIZE, BASE_CLI_CONSOLE_POOL_INC,
			  NULL);

    status = bcli_sess_exec(sess, recv_buf,
			      pool, &info);

    switch (status) {
    case BASE_CLI_EINVARG:
	send_inv_arg(sess, &info, BASE_FALSE);
	break;
    case BASE_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, BASE_FALSE);
	break;
    case BASE_CLI_EAMBIGUOUS:
    case BASE_CLI_EMISSINGARG:
	send_ambi_arg(sess, &info, BASE_FALSE);
	break;
    case BASE_CLI_EEXIT:
	retval = BASE_FALSE;
	break;
    case BASE_SUCCESS:
	send_prompt_str(sess);
	break;
    }

    bpool_release(pool);
    return retval;
}

static int readline_thread(void * p)
{
    struct cli_console_fe * fe = (struct cli_console_fe *)p;

    printf("%s", fe->cfg.prompt_str.ptr);

    while (!fe->thread_quit) {
	bsize_t input_len = 0;
	bstr_t input_str;
	char *recv_buf = fe->input.buf;
	bbool_t is_valid = BASE_TRUE;

	if (fgets(recv_buf, fe->input.maxlen, stdin) == NULL) {
	    /*
	     * Be friendly to users who redirect commands into
	     * program, when file ends, resume with kbd.
	     * If exit is desired end script with q for quit
	     */
 	    /* Reopen stdin/stdout/stderr to /dev/console */
#if ((defined(BASE_WIN32) && BASE_WIN32!=0) || \
     (defined(BASE_WIN64) && BASE_WIN64!=0)) && \
     (!defined(BASE_WIN32_WINCE) || BASE_WIN32_WINCE==0)
	    if (freopen ("CONIN$", "r", stdin) == NULL) {
#else
	    if (1) {
#endif
		puts("Cannot switch back to console from file redirection");
		if (fe->cfg.quit_command.slen) {
		    bmemcpy(recv_buf, fe->cfg.quit_command.ptr,
			      fe->input.maxlen);
		}
		recv_buf[fe->cfg.quit_command.slen] = '\0';
	    } else {
		puts("Switched back to console from file redirection");
		continue;
	    }
	}

	input_str.ptr = recv_buf;
	input_str.slen = bansi_strlen(recv_buf);
	bstrrtrim(&input_str);
	recv_buf[input_str.slen] = '\n';
	recv_buf[input_str.slen+1] = 0;
	if (fe->thread_quit) {
	    break;
	}
	input_len = bansi_strlen(fe->input.buf);
	if ((input_len > 1) && (fe->input.buf[input_len-2] == '?')) {
	    fe->input.buf[input_len-1] = 0;
	    is_valid = handle_hint(fe->sess);
	    if (!is_valid)
		printf("%s", fe->cfg.prompt_str.ptr);
	} else {
	    is_valid = handle_exec(fe->sess);
	}

        bsem_post(fe->input.sem);
        bsem_wait(fe->thread_sem);
    }

    return 0;
}

bstatus_t bcli_console_process(bcli_sess *sess,
					   char *buf,
					   unsigned maxlen)
{
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    BASE_ASSERT_RETURN(sess, BASE_EINVAL);

    fe->input.buf = buf;
    fe->input.maxlen = maxlen;

    if (!fe->input_thread) {
        bstatus_t status;

        status = bthreadCreate(fe->pool, NULL, &readline_thread, fe,
                                  0, 0, &fe->input_thread);
        if (status != BASE_SUCCESS)
            return status;
    } else {
        /* Wake up readline thread */
        bsem_post(fe->thread_sem);
    }

    bsem_wait(fe->input.sem);

    return (bcli_is_quitting(fe->base.cli)? BASE_CLI_EEXIT : BASE_SUCCESS);
}
