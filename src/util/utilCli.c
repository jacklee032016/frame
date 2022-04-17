/* 
 *
 */

#include <utilCliImp.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseExcept.h>
#include <baseHash.h>
#include <baseOs.h>
#include <basePool.h>
#include <baseString.h>
#include <utilErrno.h>
#include <utilScanner.h>
#include <utilXml.h>

#define CMD_HASH_TABLE_SIZE 63	/* Hash table size */

#define CLI_CMD_CHANGE_LOG  30000
#define CLI_CMD_EXIT        30001

#define MAX_CMD_HASH_NAME_LENGTH BASE_CLI_MAX_CMDBUF
#define MAX_CMD_ID_LENGTH 16


/**
 * This structure describes the full specification of a CLI command. A CLI
 * command mainly consists of the name of the command, zero or more arguments,
 * and a callback function to be called to execute the command.
 *
 * Application can create this specification by forming an XML document and
 * calling bcli_add_cmd_from_xml() to instantiate the spec. A sample XML
 * document containing a command spec is as follows:
 *
 \verbatim
  <CMD name='makecall' id='101' sc='m,mc' desc='Make outgoing call'>
      <ARGS>
	  <ARG name='target' type='text' desc='The destination'/>
      </ARGS>
  </CMD>
 \endverbatim
 */
struct bcli_cmd_spec
{
    /**
     * To make list of child cmds.
     */
    BASE_DECL_LIST_MEMBER(struct bcli_cmd_spec);

    /**
     * Command ID assigned to this command by the application during command
     * creation. If this value is BASE_CLI_CMD_ID_GROUP (-2), then this is
     * a command group and it can't be executed.
     */
    bcli_cmd_id id;

    /**
     * The command name.
     */
    bstr_t name;

    /**
     * The full description of the command.
     */
    bstr_t desc;

    /**
     * Number of optional shortcuts
     */
    unsigned sc_cnt;

    /**
     * Optional array of shortcuts, if any. Shortcut is a short name version
     * of the command. If the command doesn't have any shortcuts, this
     * will be initialized to NULL.
     */
    bstr_t *sc;

    /**
     * The command handler, to be executed when a command matching this command
     * specification is invoked by the end user. The value may be NULL if this
     * is a command group.
     */
    bcli_cmd_handler handler;

    /**
     * Number of arguments.
     */
    unsigned arg_cnt;

    /**
     * Array of arguments.
     */
    bcli_arg_spec *arg;

    /**
     * Child commands, if any. A command will only have subcommands if it is
     * a group. If the command doesn't have subcommands, this field will be
     * initialized with NULL.
     */
    bcli_cmd_spec *sub_cmd;
};

struct bcli_t
{
    bpool_t	       *pool;           /* Pool to allocate memory from */
    bcli_cfg          cfg;            /* CLI configuration */
    bcli_cmd_spec     root;           /* Root of command tree structure */
    bcli_front_end    fe_head;        /* List of front-ends */
    bhash_table_t    *cmd_name_hash;  /* Command name hash table, this will 
					   include the command name and shortcut 
					   as hash key */
    bhash_table_t    *cmd_id_hash;    /* Command id hash table */

    bbool_t           is_quitting;
    bbool_t           is_restarting;
};

/**
 * Reserved command id constants.
 */
typedef enum bcli_std_cmd_id
{
    /**
     * Constant to indicate an invalid command id.
     */
    BASE_CLI_INVALID_CMD_ID = -1,

    /**
     * A special command id to indicate that a command id denotes
     * a command group.
     */
    BASE_CLI_CMD_ID_GROUP = -2

} bcli_std_cmd_id;

/**
 * This describes the type of an argument (bcli_arg_spec).
 */
typedef enum bcli_arg_type
{
    /**
     * Unformatted string.
     */
    BASE_CLI_ARG_TEXT,

    /**
     * An integral number.
     */
    BASE_CLI_ARG_INT,

    /**
     * Choice type
    */
    BASE_CLI_ARG_CHOICE

} bcli_arg_type;

struct arg_type
{
    const bstr_t msg;
} arg_type[3] = 
{
    {{"Text", 4}},
    {{"Int", 3}},
    {{"Choice", 6}}
};

/**
 * This structure describe the specification of a command argument.
 */
struct bcli_arg_spec
{
    /**
     * Argument id
     */
    bcli_arg_id id;

    /**
     * Argument name.
     */
    bstr_t name;

    /**
     * Helpful description of the argument. This text will be used when
     * displaying help texts for the command/argument.
     */
    bstr_t desc;

    /**
     * Argument type, which will be used for rendering the argument and
     * to perform basic validation against an input value.
     */
    bcli_arg_type type;

    /**
     * Argument status
     */
    bbool_t optional;

    /**
     * Validate choice values
     */
    bbool_t validate;

    /**
     * Static Choice Values count
     */
    unsigned stat_choice_cnt; 

    /**
     * Static Choice Values
     */
    bcli_arg_choice_val *stat_choice_val; 

    /**
     * Argument callback to get the valid values
     */
    bcli_get_dyn_choice get_dyn_choice;

};

/**
 * This describe the parse mode of the command line
 */
typedef enum bcli_parse_mode {
    PARSE_NONE,
    PARSE_COMPLETION,	/* Complete the command line */
    PARSE_NBASE_AVAIL,   /* Find the next available command line */
    PARSE_EXEC		/* Exec the command line */
} bcli_parse_mode;

/** 
 * This is used to get the matched command/argument from the 
 * command/argument structure.
 * 
 * @param sess		The session on which the command is execute on.
 * @param cmd		The active command.
 * @param cmd_val	The command value to match.
 * @param argc		The number of argument that the 
 *			current active command have.
 * @param pool		The memory pool to allocate memory.
 * @param get_cmd	Set true to search matching command from sub command.
 * @param parse_mode	The parse mode.
 * @param p_cmd		The command that mathes the command value.
 * @param info		The output information containing any hints for 
 *			matching command/arg.
 * @return		This function return the status of the 
 *			matching process.Please see the return value
 * 			of bcli_sess_parse() for possible return values.
 */
static bstatus_t get_available_cmds(bcli_sess *sess,
				      bcli_cmd_spec *cmd, 
				      bstr_t *cmd_val,
				      unsigned argc,
				      bpool_t *pool,
				      bbool_t get_cmd,
				      bcli_parse_mode parse_mode,
				      bcli_cmd_spec **p_cmd,
				      bcli_exec_info *info);

bcli_cmd_id bcli_get_cmd_id(const bcli_cmd_spec *cmd)
{
    return cmd->id;
}

void bcli_cfg_default(bcli_cfg *param)
{
    bassert(param);
    bbzero(param, sizeof(*param));
    bstrset2(&param->name, "");
}

void bcli_exec_info_default(bcli_exec_info *param)
{
    bassert(param);
    bbzero(param, sizeof(*param));
    param->err_pos = -1;
    param->cmd_id = BASE_CLI_INVALID_CMD_ID;
    param->cmd_ret = BASE_SUCCESS;
}

void bcli_write_log(bcli_t *cli,
                              int level,
                              const char *buffer,
                              int len)
{
    struct bcli_front_end *fe;

    bassert(cli);

    fe = cli->fe_head.next;
    while (fe != &cli->fe_head) {
        if (fe->op && fe->op->on_write_log)
            (*fe->op->on_write_log)(fe, level, buffer, len);
        fe = fe->next;
    }
}

void bcli_sess_write_msg(bcli_sess *sess,                               
				   const char *buffer,
				   bsize_t len)
{
    struct bcli_front_end *fe;

    bassert(sess);

    fe = sess->fe;
    if (fe->op && fe->op->on_write_log)
        (*fe->op->on_write_log)(fe, 0, buffer, len);
}

/* Command handler */
static bstatus_t cmd_handler(bcli_cmd_val *cval)
{
    unsigned level;

    switch(cval->cmd->id) {
    case CLI_CMD_CHANGE_LOG:
        level = bstrtoul(&cval->argv[1]);
        if (!level && cval->argv[1].slen > 0 && (cval->argv[1].ptr[0] < '0' ||
            cval->argv[1].ptr[0] > '9'))
	{
            return BASE_CLI_EINVARG;
	}
        cval->sess->log_level = level;
        return BASE_SUCCESS;
    case CLI_CMD_EXIT:
        bcli_sess_end_session(cval->sess);
        return BASE_CLI_EEXIT;
    default:
        return BASE_SUCCESS;
    }
}

bstatus_t bcli_create(bcli_cfg *cfg,
                                  bcli_t **p_cli)
{
    bpool_t *pool;
    bcli_t *cli;    
    unsigned i;

    /* This is an example of the command structure */
    char* cmd_xmls[] = {
     "<CMD name='log' id='30000' sc='' desc='Change log level'>"
     "    <ARG name='level' type='int' optional='0' desc='Log level'/>"
     "</CMD>",     
     "<CMD name='exit' id='30001' sc='' desc='Exit session'>"     
     "</CMD>",
    };

    BASE_ASSERT_RETURN(cfg && cfg->pf && p_cli, BASE_EINVAL);

    pool = bpool_create(cfg->pf, "cli", BASE_CLI_POOL_SIZE, 
                          BASE_CLI_POOL_INC, NULL);
    if (!pool)
        return BASE_ENOMEM;
    cli = BASE_POOL_ZALLOC_T(pool, struct bcli_t);

    bmemcpy(&cli->cfg, cfg, sizeof(*cfg));
    cli->pool = pool;
    blist_init(&cli->fe_head);

    cli->cmd_name_hash = bhash_create(pool, CMD_HASH_TABLE_SIZE);
    cli->cmd_id_hash = bhash_create(pool, CMD_HASH_TABLE_SIZE);

    cli->root.sub_cmd = BASE_POOL_ZALLOC_T(pool, bcli_cmd_spec);
    blist_init(cli->root.sub_cmd);

    /* Register some standard commands. */
    for (i = 0; i < sizeof(cmd_xmls)/sizeof(cmd_xmls[0]); i++) {
        bstr_t xml = bstr(cmd_xmls[i]);

        if (bcli_add_cmd_from_xml(cli, NULL, &xml, 
				    &cmd_handler, NULL, NULL) != BASE_SUCCESS) 
	{
            MTRACE( "Failed to add command #%d", i);
	}
    }

    *p_cli = cli;

    return BASE_SUCCESS;
}

bcli_cfg* bcli_get_param(bcli_t *cli)
{
    BASE_ASSERT_RETURN(cli, NULL);

    return &cli->cfg;
}

void bcli_register_front_end(bcli_t *cli,
                                       bcli_front_end *fe)
{
    blist_push_back(&cli->fe_head, fe);
}

void bcli_quit(bcli_t *cli, bcli_sess *req,
			 bbool_t restart)
{
    bcli_front_end *fe;

    bassert(cli);
    if (cli->is_quitting)
	return;

    cli->is_quitting = BASE_TRUE;
    cli->is_restarting = restart;

    fe = cli->fe_head.next;
    while (fe != &cli->fe_head) {
        if (fe->op && fe->op->on_quit)
            (*fe->op->on_quit)(fe, req);
        fe = fe->next;
    }
}

bbool_t bcli_is_quitting(bcli_t *cli)
{
    BASE_ASSERT_RETURN(cli, BASE_FALSE);

    return cli->is_quitting;
}

bbool_t bcli_is_restarting(bcli_t *cli)
{
    BASE_ASSERT_RETURN(cli, BASE_FALSE);

    return cli->is_restarting;
}

void bcli_destroy(bcli_t *cli)
{
    bcli_front_end *fe;

    if (!cli)
	return;

    if (!bcli_is_quitting(cli))
        bcli_quit(cli, NULL, BASE_FALSE);

    fe = cli->fe_head.next;
    while (fe != &cli->fe_head) {
        blist_erase(fe);
	if (fe->op && fe->op->on_destroy)
	    (*fe->op->on_destroy)(fe);

        fe = cli->fe_head.next;
    }
    cli->is_quitting = BASE_FALSE;
    bpool_release(cli->pool);
}

void bcli_sess_end_session(bcli_sess *sess)
{
    bassert(sess);

    if (sess->op && sess->op->destroy)
        (*sess->op->destroy)(sess);
}

/* Syntax error handler for parser. */
static void on_syntax_error(bscanner *scanner)
{
    BASE_UNUSED_ARG(scanner);
    BASE_THROW(BASE_EINVAL);
}

/* Get the command from the command hash */
static bcli_cmd_spec *get_cmd_name(const bcli_t *cli, 
				     const bcli_cmd_spec *group, 
				     const bstr_t *cmd)
{
    bstr_t cmd_val;
    char cmd_ptr[MAX_CMD_HASH_NAME_LENGTH];

    cmd_val.ptr = cmd_ptr;
    cmd_val.slen = 0;

    if (group) {
	char cmd_str[MAX_CMD_ID_LENGTH];
	bansi_sprintf(cmd_str, "%d", group->id);
	bstrcat2(&cmd_val, cmd_str);	
    }
    bstrcat(&cmd_val, cmd);
    return (bcli_cmd_spec *)bhash_get(cli->cmd_name_hash, cmd_val.ptr, 
					  (unsigned)cmd_val.slen, NULL);
}

/* Add command to the command hash */
static void add_cmd_name(bcli_t *cli, bcli_cmd_spec *group, 
			 bcli_cmd_spec *cmd, bstr_t *cmd_name)
{
    bstr_t cmd_val;
    bstr_t add_cmd;
    char cmd_ptr[MAX_CMD_HASH_NAME_LENGTH];

    cmd_val.ptr = cmd_ptr;
    cmd_val.slen = 0;

    if (group) {
	char cmd_str[MAX_CMD_ID_LENGTH];
	bansi_sprintf(cmd_str, "%d", group->id);
	bstrcat2(&cmd_val, cmd_str);	
    }
    bstrcat(&cmd_val, cmd_name);
    bstrdup(cli->pool, &add_cmd, &cmd_val);
    
    bhash_set(cli->pool, cli->cmd_name_hash, cmd_val.ptr, 
		(unsigned)cmd_val.slen, 0, cmd);
}

/**
 * This method is to parse and add the choice type 
 * argument values to command structure.
 **/
static bstatus_t add_choice_node(bcli_t *cli,
				   bxml_node *xml_node,
				   bcli_arg_spec *arg,
				   bcli_get_dyn_choice get_choice)
{
    bxml_node *choice_node;
    bxml_node *sub_node;
    bcli_arg_choice_val choice_values[BASE_CLI_MAX_CHOICE_VAL];
    bstatus_t status = BASE_SUCCESS;

    sub_node = xml_node;
    arg->type = BASE_CLI_ARG_CHOICE;
    arg->get_dyn_choice = get_choice;						

    choice_node = sub_node->node_head.next;
    while (choice_node != (bxml_node*)&sub_node->node_head) {
	bxml_attr *choice_attr;
	unsigned *stat_cnt = &arg->stat_choice_cnt;
	bcli_arg_choice_val *choice_val = &choice_values[*stat_cnt];		     
	bbzero(choice_val, sizeof(*choice_val));

	choice_attr = choice_node->attr_head.next;
	while (choice_attr != &choice_node->attr_head) {
	    if (!bstricmp2(&choice_attr->name, "value")) {
		bstrassign(&choice_val->value, &choice_attr->value);
	    } else if (!bstricmp2(&choice_attr->name, "desc")) {
		bstrassign(&choice_val->desc, &choice_attr->value);
	    }
	    choice_attr = choice_attr->next;
	}		
	if (++(*stat_cnt) >= BASE_CLI_MAX_CHOICE_VAL)
	    break;
	choice_node = choice_node->next;
    }    
    if (arg->stat_choice_cnt > 0) {
        unsigned i;

	arg->stat_choice_val = (bcli_arg_choice_val *)
				bpool_zalloc(cli->pool, 
					       arg->stat_choice_cnt *
					       sizeof(bcli_arg_choice_val));

        for (i = 0; i < arg->stat_choice_cnt; i++) {
	    bstrdup(cli->pool, &arg->stat_choice_val[i].value, 
		      &choice_values[i].value);
            bstrdup(cli->pool, &arg->stat_choice_val[i].desc, 
		      &choice_values[i].desc);            
        }
    }
    return status;
}

/**
 * This method is to parse and add the argument attribute to command structure.
 **/
static bstatus_t add_arg_node(bcli_t *cli,
				bxml_node *xml_node,
				bcli_cmd_spec *cmd,
				bcli_arg_spec *arg,
				bcli_get_dyn_choice get_choice)
{    
    bxml_attr *attr;
    bstatus_t status = BASE_SUCCESS;
    bxml_node *sub_node = xml_node;

    if (cmd->arg_cnt >= BASE_CLI_MAX_ARGS)
	return BASE_CLI_ETOOMANYARGS;
    
    bbzero(arg, sizeof(*arg));
    attr = sub_node->attr_head.next;
    arg->optional = BASE_FALSE;
    arg->validate = BASE_TRUE;
    while (attr != &sub_node->attr_head) {	
	if (!bstricmp2(&attr->name, "name")) {
	    bstrassign(&arg->name, &attr->value);
	} else if (!bstricmp2(&attr->name, "id")) {
	    arg->id = bstrtol(&attr->value);
	} else if (!bstricmp2(&attr->name, "type")) {
	    if (!bstricmp2(&attr->value, "text")) {
		arg->type = BASE_CLI_ARG_TEXT;
	    } else if (!bstricmp2(&attr->value, "int")) {
		arg->type = BASE_CLI_ARG_INT;
	    } else if (!bstricmp2(&attr->value, "choice")) {
		/* Get choice value */
		add_choice_node(cli, xml_node, arg, get_choice);
	    } 
	} else if (!bstricmp2(&attr->name, "desc")) {
	    bstrassign(&arg->desc, &attr->value);
	} else if (!bstricmp2(&attr->name, "optional")) {
	    if (!bstrcmp2(&attr->value, "1")) {
		arg->optional = BASE_TRUE;
	    }
	} else if (!bstricmp2(&attr->name, "validate")) {
	    if (!bstrcmp2(&attr->value, "1")) {
		arg->validate = BASE_TRUE;
	    } else {
		arg->validate = BASE_FALSE;
	    }	
	} 
	attr = attr->next;
    }
    cmd->arg_cnt++;
    return status;
}

/**
 * This method is to parse and add the command attribute to command structure.
 **/
static bstatus_t add_cmd_node(bcli_t *cli,				  
				bcli_cmd_spec *group,					 
				bxml_node *xml_node,
				bcli_cmd_handler handler,
				bcli_cmd_spec **p_cmd,
				bcli_get_dyn_choice get_choice)
{
    bxml_node *root = xml_node;
    bxml_attr *attr;
    bxml_node *sub_node;
    bcli_cmd_spec *cmd;
    bcli_arg_spec args[BASE_CLI_MAX_ARGS];
    bstr_t sc[BASE_CLI_MAX_SHORTCUTS];
    bstatus_t status = BASE_SUCCESS;

    if (bstricmp2(&root->name, "CMD"))
        return BASE_EINVAL;

    /* Initialize the command spec */
    cmd = BASE_POOL_ZALLOC_T(cli->pool, struct bcli_cmd_spec);
    
    /* Get the command attributes */
    attr = root->attr_head.next;
    while (attr != &root->attr_head) {
        if (!bstricmp2(&attr->name, "name")) {
            bstrltrim(&attr->value);
            if (!attr->value.slen || 
		(get_cmd_name(cli, group, &attr->value)))                
            {
                return BASE_CLI_EBADNAME;
            }
            bstrdup(cli->pool, &cmd->name, &attr->value);
        } else if (!bstricmp2(&attr->name, "id")) {	    
	    bbool_t is_valid = BASE_FALSE;
            if (attr->value.slen) {		
		bcli_cmd_id cmd_id = bstrtol(&attr->value);
		if (!bhash_get(cli->cmd_id_hash, &cmd_id, 
		                 sizeof(bcli_cmd_id), NULL))
		    is_valid = BASE_TRUE;
	    } 
	    if (!is_valid)
		return BASE_CLI_EBADID;
            cmd->id = (bcli_cmd_id)bstrtol(&attr->value);
        } else if (!bstricmp2(&attr->name, "sc")) {
            bscanner scanner;
            bstr_t str;

            BASE_USE_EXCEPTION;

	    /* The buffer passed to the scanner is not NULL terminated,
	     * but should be safe. See ticket #2063.
	     */
            bscan_init(&scanner, attr->value.ptr, attr->value.slen,
                         BASE_SCAN_AUTOSKIP_WS, &on_syntax_error);

            BASE_TRY {
                while (!bscan_is_eof(&scanner)) {
                    bscan_get_until_ch(&scanner, ',', &str);
                    bstrrtrim(&str);
                    if (!bscan_is_eof(&scanner))
                        bscan_advance_n(&scanner, 1, BASE_TRUE);
                    if (!str.slen)
                        continue;

                    if (cmd->sc_cnt >= BASE_CLI_MAX_SHORTCUTS) {
                        BASE_THROW(BASE_CLI_ETOOMANYARGS);
                    }
                    /* Check whether the shortcuts are already used */
                    if (get_cmd_name(cli, &cli->root, &str)) {
                        BASE_THROW(BASE_CLI_EBADNAME);
                    }

                    bstrassign(&sc[cmd->sc_cnt++], &str);
                }
            }
            BASE_CATCH_ANY {
                bscan_fini(&scanner);
                return (BASE_GET_EXCEPTION());
            }
            BASE_END;

            bscan_fini(&scanner);

        } else if (!bstricmp2(&attr->name, "desc")) {
            bstrdup(cli->pool, &cmd->desc, &attr->value);
        }
        attr = attr->next;
    }

    /* Get the command childs/arguments */
    sub_node = root->node_head.next;
    while (sub_node != (bxml_node*)&root->node_head) {
	if (!bstricmp2(&sub_node->name, "CMD")) {
	    status = add_cmd_node(cli, cmd, sub_node, handler, NULL, 
				  get_choice);
	    if (status != BASE_SUCCESS)
		return status;
	} else if (!bstricmp2(&sub_node->name, "ARG")) {
	    /* Get argument attribute */
	    status = add_arg_node(cli, sub_node, 
			          cmd, &args[cmd->arg_cnt], 
		                  get_choice);

	    if (status != BASE_SUCCESS)
		return status;
        }
        sub_node = sub_node->next;
    }

    if (!cmd->name.slen)
        return BASE_CLI_EBADNAME;

    if (!cmd->id)
        return BASE_CLI_EBADID;

    if (cmd->arg_cnt) {
        unsigned i;

        cmd->arg = (bcli_arg_spec *)bpool_zalloc(cli->pool, cmd->arg_cnt *
                                                     sizeof(bcli_arg_spec));
	
        for (i = 0; i < cmd->arg_cnt; i++) {
            bstrdup(cli->pool, &cmd->arg[i].name, &args[i].name);
            bstrdup(cli->pool, &cmd->arg[i].desc, &args[i].desc);
	    cmd->arg[i].id = args[i].id;
            cmd->arg[i].type = args[i].type;
	    cmd->arg[i].optional = args[i].optional;
	    cmd->arg[i].validate = args[i].validate;
	    cmd->arg[i].get_dyn_choice = args[i].get_dyn_choice;
	    cmd->arg[i].stat_choice_cnt = args[i].stat_choice_cnt;
	    cmd->arg[i].stat_choice_val = args[i].stat_choice_val;
        }
    }

    if (cmd->sc_cnt) {
        unsigned i;

        cmd->sc = (bstr_t *)bpool_zalloc(cli->pool, cmd->sc_cnt *
                                             sizeof(bstr_t));
        for (i = 0; i < cmd->sc_cnt; i++) {
            bstrdup(cli->pool, &cmd->sc[i], &sc[i]);	
	    /** Add shortcut to root command **/
	    add_cmd_name(cli, &cli->root, cmd, &sc[i]);
        }
    }
    
    add_cmd_name(cli, group, cmd, &cmd->name);    
    bhash_set(cli->pool, cli->cmd_id_hash, 
		&cmd->id, sizeof(bcli_cmd_id), 0, cmd);

    cmd->handler = handler;

    if (group) {
	if (!group->sub_cmd) {
	    group->sub_cmd = BASE_POOL_ALLOC_T(cli->pool, struct bcli_cmd_spec);
	    blist_init(group->sub_cmd);
	}
        blist_push_back(group->sub_cmd, cmd);
    } else {
        blist_push_back(cli->root.sub_cmd, cmd);
    }

    if (p_cmd)
        *p_cmd = cmd;

    return status;
}

bstatus_t bcli_add_cmd_from_xml(bcli_t *cli,
					    bcli_cmd_spec *group,
                                            const bstr_t *xml,
                                            bcli_cmd_handler handler,
                                            bcli_cmd_spec **p_cmd, 
					    bcli_get_dyn_choice get_choice)
{ 
    bpool_t *pool;
    bxml_node *root;
    bstatus_t status = BASE_SUCCESS;
    
    BASE_ASSERT_RETURN(cli && xml, BASE_EINVAL);

    /* Parse the xml */
    pool = bpool_create(cli->cfg.pf, "xml", 1024, 1024, NULL);
    if (!pool)
        return BASE_ENOMEM;

    root = bxml_parse(pool, xml->ptr, xml->slen);
    if (!root) {
	MTRACE( "Error: unable to parse XML");
	bpool_release(pool);
	return BASE_CLI_EBADXML;
    }    
    status = add_cmd_node(cli, group, root, handler, p_cmd, get_choice);
    bpool_release(pool);
    return status;
}

bstatus_t bcli_sess_parse(bcli_sess *sess,
				      char *cmdline,
				      bcli_cmd_val *val,
				      bpool_t *pool,
				      bcli_exec_info *info)
{    
    bscanner scanner;
    bstr_t str;
    bsize_t len;    
    bcli_cmd_spec *cmd;
    bcli_cmd_spec *nbcmd;
    bstatus_t status = BASE_SUCCESS;
    bcli_parse_mode parse_mode = PARSE_NONE;    

    BASE_USE_EXCEPTION;

    BASE_ASSERT_RETURN(sess && cmdline && val, BASE_EINVAL);

    BASE_UNUSED_ARG(pool);

    str.slen = 0;
    bcli_exec_info_default(info);

    /* Set the parse mode based on the latest char.
     * And NULL terminate the buffer for the scanner.
     */
    len = bansi_strlen(cmdline);
    if (len > 0 && ((cmdline[len - 1] == '\r')||(cmdline[len - 1] == '\n'))) {
        cmdline[--len] = 0;
	parse_mode = PARSE_EXEC;
    } else if (len > 0 && 
	       (cmdline[len - 1] == '\t' || cmdline[len - 1] == '?')) 
    {
	cmdline[--len] = 0;
	if (len == 0) {
	    parse_mode = PARSE_NBASE_AVAIL;
	} else {
	    if (cmdline[len - 1] == ' ') 
		parse_mode = PARSE_NBASE_AVAIL;
	    else 
		parse_mode = PARSE_COMPLETION;
	}
    }
    val->argc = 0;
    info->err_pos = 0;
    cmd = &sess->fe->cli->root;
    if (len > 0) {
	bscan_init(&scanner, cmdline, len, BASE_SCAN_AUTOSKIP_WS, 
		     &on_syntax_error);
	BASE_TRY {
	    val->argc = 0;	    
	    while (!bscan_is_eof(&scanner)) {
		info->err_pos = (int)(scanner.curptr - scanner.begin);
		if (*scanner.curptr == '\'' || *scanner.curptr == '"' ||
		    *scanner.curptr == '{')
		{
		    bscan_get_quotes(&scanner, "'\"{", "'\"}", 3, &str);
		    /* Remove the quotes */
		    str.ptr++;
		    str.slen -= 2;
		} else {
		    bscan_get_until_chr(&scanner, " \t\r\n", &str);
		}
		++val->argc;
		if (val->argc == BASE_CLI_MAX_ARGS)
		    BASE_THROW(BASE_CLI_ETOOMANYARGS);		
		
		status = get_available_cmds(sess, cmd, &str, val->argc-1, 
					    pool, BASE_TRUE, parse_mode, 
					    &nbcmd, info);

		if (status != BASE_SUCCESS)
		    BASE_THROW(status);
		
		if (cmd != nbcmd) {
		    /* Found new command, set it as the active command */
		    cmd = nbcmd;
		    val->argc = 1;
		    val->cmd = cmd;
		}
		if (parse_mode == PARSE_EXEC) 
		    bstrassign(&val->argv[val->argc-1], &info->hint->name);
		else 
		    bstrassign(&val->argv[val->argc-1], &str);

	    }            
	    if (!bscan_is_eof(&scanner)) 
		BASE_THROW(BASE_CLI_EINVARG);

	}
	BASE_CATCH_ANY {
	    bscan_fini(&scanner);
	    return BASE_GET_EXCEPTION();
	}
	BASE_END;
	
	bscan_fini(&scanner);
    } 
    
    if ((parse_mode == PARSE_NBASE_AVAIL) || (parse_mode == PARSE_EXEC)) {
	/* If exec mode, just get the matching argument */
	status = get_available_cmds(sess, cmd, NULL, val->argc, pool, 
				    (parse_mode==PARSE_NBASE_AVAIL), 
				    parse_mode,
				    NULL, info);
	if ((status != BASE_SUCCESS) && (status != BASE_CLI_EINVARG)) {
	    bstr_t data = bstr(cmdline);
	    bstrrtrim(&data);
	    data.ptr[data.slen] = ' ';
	    data.ptr[data.slen+1] = 0;

	    info->err_pos = (int)bansi_strlen(cmdline);
	}
    } 
   
    val->sess = sess;
    return status;
}

bstatus_t bcli_sess_exec(bcli_sess *sess,
				      char *cmdline,
				      bpool_t *pool,
				      bcli_exec_info *info)
{
    bcli_cmd_val val;
    bstatus_t status;
    bcli_exec_info einfo;
    bstr_t cmd;

    BASE_ASSERT_RETURN(sess && cmdline, BASE_EINVAL);

    BASE_UNUSED_ARG(pool);

    cmd.ptr = cmdline;
    cmd.slen = bansi_strlen(cmdline);

    if (bstrtrim(&cmd)->slen == 0)
	return BASE_SUCCESS;

    if (!info)
        info = &einfo;
	
    status = bcli_sess_parse(sess, cmdline, &val, pool, info);
    if (status != BASE_SUCCESS)
        return status;

    if ((val.argc > 0) && (val.cmd->handler)) {
        info->cmd_ret = (*val.cmd->handler)(&val);
        if (info->cmd_ret == BASE_CLI_EINVARG ||
            info->cmd_ret == BASE_CLI_EEXIT)
	{
            return info->cmd_ret;
	}
    }

    return BASE_SUCCESS;
}

static bbool_t hint_inserted(const bstr_t *name, 
			       const bstr_t *desc, 
			       const bstr_t *type, 
			       bcli_exec_info *info)
{
    unsigned i;
    for(i=0; i<info->hint_cnt; ++i) {
	bcli_hint_info *hint = &info->hint[i];
	if ((!bstrncmp(&hint->name, name, hint->name.slen)) &&
	    (!bstrncmp(&hint->desc, desc, hint->desc.slen)) &&
	    (!bstrncmp(&hint->type, type, hint->type.slen)))
	{
	    return BASE_TRUE;
	}
    }
    return BASE_FALSE;
}

/** This will insert new hint with the option to check for the same 
    previous entry **/
static bstatus_t insert_new_hint2(bpool_t *pool, 
				    bbool_t unique_insert,
				    const bstr_t *name, 
				    const bstr_t *desc, 
				    const bstr_t *type, 
				    bcli_exec_info *info)
{
    bcli_hint_info *hint;
    BASE_ASSERT_RETURN(pool && info, BASE_EINVAL);
    BASE_ASSERT_RETURN((info->hint_cnt < BASE_CLI_MAX_HINTS), BASE_EINVAL);

    if ((unique_insert) && (hint_inserted(name, desc, type, info)))
	return BASE_SUCCESS;

    hint = &info->hint[info->hint_cnt];

    bstrdup(pool, &hint->name, name);

    if (desc && (desc->slen > 0))  {
	bstrdup(pool, &hint->desc, desc);
    } else {
	hint->desc.slen = 0;
    }
    
    if (type && (type->slen > 0)) {
	bstrdup(pool, &hint->type, type);
    } else {
	hint->type.slen = 0;
    }

    ++info->hint_cnt;    
    return BASE_SUCCESS;
}

/** This will insert new hint without checking for the same previous entry **/
static bstatus_t insert_new_hint(bpool_t *pool, 
				   const bstr_t *name, 
				   const bstr_t *desc, 
				   const bstr_t *type, 
				   bcli_exec_info *info)
{        
    return insert_new_hint2(pool, BASE_FALSE, name, desc, type, info);
}

/** This will get a complete/exact match of a command from the cmd hash **/
static bstatus_t get_comp_match_cmds(const bcli_t *cli, 
				       const bcli_cmd_spec *group,
				       const bstr_t *cmd_val,	
				       bpool_t *pool, 
				       bcli_cmd_spec **p_cmd,
				       bcli_exec_info *info)
{
    bcli_cmd_spec *cmd;    
    BASE_ASSERT_RETURN(cli && group && cmd_val && pool && info, BASE_EINVAL);   

    cmd = get_cmd_name(cli, group, cmd_val);

    if (cmd) {
	bstatus_t status;    
	status = insert_new_hint(pool, cmd_val, &cmd->desc, NULL, info);

	if (status != BASE_SUCCESS)
	    return status;

	*p_cmd = cmd;
    }

    return BASE_SUCCESS;
}

/** This method will search for any shortcut with pattern match to the input
    command. This method should be called from root command, as shortcut could
    only be executed from root **/
static bstatus_t get_pattern_match_shortcut(const bcli_t *cli,
					      const bstr_t *cmd_val,
					      bpool_t *pool, 
					      bcli_cmd_spec **p_cmd,
					      bcli_exec_info *info)
{
    bhash_iterator_t it_buf, *it;
    bstatus_t status;
    BASE_ASSERT_RETURN(cli && pool && cmd_val && info, BASE_EINVAL);
  
    it = bhash_first(cli->cmd_name_hash, &it_buf);
    while (it) {
	unsigned i;
	bcli_cmd_spec *cmd = (bcli_cmd_spec *)
			       bhash_this(cli->cmd_name_hash, it);

	BASE_ASSERT_RETURN(cmd, BASE_EINVAL);	    
	
	for (i=0; i < cmd->sc_cnt; ++i) {
	    static const bstr_t SHORTCUT = {"SC", 2};
	    bstr_t *sc = &cmd->sc[i];
	    BASE_ASSERT_RETURN(sc, BASE_EINVAL);

	    if (!bstrncmp(sc, cmd_val, cmd_val->slen)) {
		/** Unique hints needed because cmd hash contain command name
		    and shortcut referencing to the same command **/
		status = insert_new_hint2(pool, BASE_TRUE, sc, &cmd->desc, 
					  &SHORTCUT, info);
		if (status != BASE_SUCCESS)
		    return status;

		if (p_cmd)
		    *p_cmd = cmd;
	    }
	}
	
	it = bhash_next(cli->cmd_name_hash, it);
    }

    return BASE_SUCCESS;
}

/** This method will search a pattern match to the input command from the child
    command list of the current/active command. **/
static bstatus_t get_pattern_match_cmds(bcli_cmd_spec *cmd, 
					  const bstr_t *cmd_val,				      
					  bpool_t *pool, 
					  bcli_cmd_spec **p_cmd, 
					  bcli_parse_mode parse_mode,
					  bcli_exec_info *info)
{
    bstatus_t status;
    BASE_ASSERT_RETURN(cmd && pool && info && cmd_val, BASE_EINVAL);   

    /* Get matching command */
    if (cmd->sub_cmd) {
	bcli_cmd_spec *child_cmd = cmd->sub_cmd->next;
	while (child_cmd != cmd->sub_cmd) {
	    bbool_t found = BASE_FALSE;
	    if (!bstrncmp(&child_cmd->name, cmd_val, cmd_val->slen)) {		
		status = insert_new_hint(pool, &child_cmd->name, 
					 &child_cmd->desc, NULL, info);
		if (status != BASE_SUCCESS)
		    return status;

		found = BASE_TRUE;
	    }
	    if (found) {
		if (parse_mode == PARSE_NBASE_AVAIL) {
		    /** Only insert shortcut on next available commands mode **/
		    unsigned i;
		    for (i=0; i < child_cmd->sc_cnt; ++i) {
			static const bstr_t SHORTCUT = {"SC", 2};
			bstr_t *sc = &child_cmd->sc[i];
			BASE_ASSERT_RETURN(sc, BASE_EINVAL);

			status = insert_new_hint(pool, sc, 
						 &child_cmd->desc, &SHORTCUT, 
						 info);
			if (status != BASE_SUCCESS)
			    return status;
		    }
		}

		if (p_cmd)
		    *p_cmd = child_cmd;			    
	    }	
	    child_cmd = child_cmd->next;
	}
    }
    return BASE_SUCCESS;
}

/** This will match the arguments passed to the command with the argument list
    of the specified command list. **/
static bstatus_t get_match_args(bcli_sess *sess,
				  bcli_cmd_spec *cmd, 
				  const bstr_t *cmd_val,
				  unsigned argc,
				  bpool_t *pool, 
				  bcli_parse_mode parse_mode,
				  bcli_exec_info *info)
{
    bcli_arg_spec *arg;
    bstatus_t status = BASE_SUCCESS;

    BASE_ASSERT_RETURN(cmd && pool && cmd_val && info, BASE_EINVAL);

    if ((argc > cmd->arg_cnt) && (!cmd->sub_cmd)) {
	if (cmd_val->slen > 0)
	    return BASE_CLI_ETOOMANYARGS;
	else
	    return BASE_SUCCESS;
    }

    if (cmd->arg_cnt > 0) {
	arg = &cmd->arg[argc-1];
	BASE_ASSERT_RETURN(arg, BASE_EINVAL);
	if (arg->type == BASE_CLI_ARG_CHOICE) {	    
	    unsigned j;	    	    

	    if ((parse_mode == PARSE_EXEC) && (!arg->validate)) {
		/* If no validation needed, then insert the values */
		status = insert_new_hint(pool, cmd_val, NULL, NULL, info);		
		return status;
	    }

	    for (j=0; j < arg->stat_choice_cnt; ++j) {
		bcli_arg_choice_val *choice_val = &arg->stat_choice_val[j];		
    	    
		BASE_ASSERT_RETURN(choice_val, BASE_EINVAL);		
    	    
		if (!bstrncmp(&choice_val->value, cmd_val, cmd_val->slen)) {		    
		    status = insert_new_hint(pool, 
					     &choice_val->value, 
					     &choice_val->desc, 
					     &arg_type[BASE_CLI_ARG_CHOICE].msg, 
					     info);
		    if (status != BASE_SUCCESS)
			return status;		    
		}
	    }
	    if (arg->get_dyn_choice) {
		bcli_dyn_choice_param dyn_choice_param;
		static bstr_t choice_str = {"choice", 6};

		/* Get the dynamic choice values */	    
		dyn_choice_param.sess = sess;
		dyn_choice_param.cmd = cmd;
		dyn_choice_param.arg_id = arg->id;
		dyn_choice_param.max_cnt = BASE_CLI_MAX_CHOICE_VAL;
		dyn_choice_param.pool = pool;
		dyn_choice_param.cnt = 0;	    

		(*arg->get_dyn_choice)(&dyn_choice_param);
		for (j=0; j < dyn_choice_param.cnt; ++j) {
		    bcli_arg_choice_val *choice = &dyn_choice_param.choice[j];
		    if (!bstrncmp(&choice->value, cmd_val, cmd_val->slen)) {
			bstrassign(&info->hint[info->hint_cnt].name, 
				     &choice->value);
			bstrassign(&info->hint[info->hint_cnt].type, 
				     &choice_str);
			bstrassign(&info->hint[info->hint_cnt].desc, 
				     &choice->desc);
			if ((++info->hint_cnt) >= BASE_CLI_MAX_HINTS)
			    break;
		    }
		}
		if ((info->hint_cnt == 0) && (!arg->optional))
		    return BASE_CLI_EMISSINGARG;
	    }
	} else {
	    if (cmd_val->slen == 0) {
		if (info->hint_cnt == 0) {
		    if (!((parse_mode == PARSE_EXEC) && (arg->optional))) {
			/* For exec mode,no need to insert hint if optional */
			status = insert_new_hint(pool, 
						 &arg->name, 
				  	         &arg->desc, 
						 &arg_type[arg->type].msg,
						 info);
			if (status != BASE_SUCCESS)
			    return status;
		    }
		    if (!arg->optional)
			return BASE_CLI_EMISSINGARG;
		} 
	    } else {
		return insert_new_hint(pool, cmd_val, NULL, NULL, info);
	    }
	}
    } 
    return status;
}

/** This will check for a match of the commands/arguments input. Any match 
    will be inserted to the hint data. **/
static bstatus_t get_available_cmds(bcli_sess *sess,
				      bcli_cmd_spec *cmd, 
				      bstr_t *cmd_val,
				      unsigned argc,
				      bpool_t *pool,
				      bbool_t get_cmd,
				      bcli_parse_mode parse_mode,
				      bcli_cmd_spec **p_cmd,
				      bcli_exec_info *info)
{
    bstatus_t status = BASE_SUCCESS;
    bstr_t *prefix;
    bstr_t EMPTY_STR = {NULL, 0};

    prefix = cmd_val?(bstrtrim(cmd_val)):(&EMPTY_STR);

    info->hint_cnt = 0;    

    if (get_cmd) {
	status = get_comp_match_cmds(sess->fe->cli, cmd, prefix, pool, p_cmd, 
				     info);
	if (status != BASE_SUCCESS)
	    return status;
	
	/** If exact match found, then no need to search for pattern match **/
	if (info->hint_cnt == 0) {
	    if ((parse_mode != PARSE_NBASE_AVAIL) && 
		(cmd == &sess->fe->cli->root)) 
	    {
		/** Pattern match for shortcut needed on root command only **/
		status = get_pattern_match_shortcut(sess->fe->cli, prefix, pool,
						    p_cmd, info);

		if (status != BASE_SUCCESS)
		    return status;
	    } 
	    
	    status = get_pattern_match_cmds(cmd, prefix, pool, p_cmd, 
					    parse_mode, info);
	}

	if (status != BASE_SUCCESS)
	    return status;
    }

    if (argc > 0)
	status = get_match_args(sess, cmd, prefix, argc, 
			        pool, parse_mode, info);

    if (status == BASE_SUCCESS) {	
	if (prefix->slen > 0) {
	    /** If a command entered is not a an empty command, and have a 
		single match in the command list then it is a valid command **/
	    if (info->hint_cnt == 0) {
		status = BASE_CLI_EINVARG;
	    } else if (info->hint_cnt > 1) {
		status = BASE_CLI_EAMBIGUOUS;
	    }
	} else {
	    if (info->hint_cnt > 0)
		status = BASE_CLI_EAMBIGUOUS;
	}
    } 

    return status;
}

