#include "node.h"

/// allocate and return node_expr struct
node_expr *node_expr_build()
{
    node_expr *x = (node_expr*)malloc(sizeof(node_expr));
	x->iskey = 0;
	x->lhs = NULL;
	x->rhs = NULL;
    return x;
}

/// allocate and return node_expr struct for data column given its name
node_expr *node_expr_buildcolumn(char *val)
{
    node_expr *x = node_expr_build();
    x->type = NODE_EXPR_COLUMN;
    x->val.s = val;
    return x;
}

/// allocate and return node_expr struct for constant integer
node_expr *node_expr_buildint(int val)
{
    node_expr *x = node_expr_build();
    x->type = NODE_EXPR_INT;
    x->val.i = val;
    return x;
}

/// allocate and return node_expr struct for constant float
node_expr *node_expr_buildfloat(float val)
{
    node_expr *x = node_expr_build();
    x->type = NODE_EXPR_FLOAT;
    x->val.f = val;
    return x;
}

/// allocate and return node_expr struct for operator of two sub expressions
node_expr *node_expr_buildop(int op, node_expr *lhs, node_expr *rhs)
{
    node_expr *x = (node_expr*)malloc(sizeof(node_expr));
    x->type = NODE_EXPR_OP;
    x->val.i = op;
	x->lhs = lhs;
	x->rhs = rhs;
    return x;
}

char *ts_buffer;
int ts_count;

/// recurse through expression true, appending to ts_buffer
void node_expr_tostringrecurse(node_expr *x)
{
	size_t left = VIRG_MAX_COLUMN_NAME - ts_count;

	switch(x->type) {
		case NODE_EXPR_COLUMN :
			strncpy(&ts_buffer[ts_count], x->val.s, left);
			ts_count += strlen(x->val.s);
			break;

		case NODE_EXPR_INT :
			ts_count += snprintf(&ts_buffer[ts_count], left, "%i", x->val.i);
			break;

		case NODE_EXPR_FLOAT :
			ts_count += snprintf(&ts_buffer[ts_count], left, "%f", x->val.f);
			break;

		case NODE_EXPR_OP :
			ts_count += snprintf(&ts_buffer[ts_count], left, "(");
			left = VIRG_MAX_COLUMN_NAME - ts_count;

			node_expr_tostringrecurse(x->lhs);

			char op;
			switch(x->val.i) {
				case NODE_OP_PLUS	: op = '+'; break;
				case NODE_OP_MINUS	: op = '-'; break;
				case NODE_OP_MUL	: op = '*'; break;
				case NODE_OP_DIV	: op = '/'; break;
				default : assert(0);
			}

			ts_count += snprintf(&ts_buffer[ts_count], left, "%c", op);

			node_expr_tostringrecurse(x->rhs);

			left = VIRG_MAX_COLUMN_NAME - ts_count;
			ts_count += snprintf(&ts_buffer[ts_count], left, ")");
			break;

		default :
			assert(0);
	}
}

/// return string representation of an expression tree
char *node_expr_tostring(node_expr *x)
{
	ts_buffer = (char*)malloc(VIRG_MAX_COLUMN_NAME);
	ts_count = 0;

	node_expr_tostringrecurse(x);

	return ts_buffer;
}

/// recursively deallocate expression tree
void node_expr_free(node_expr *x)
{
	if(x->lhs != NULL)
		node_expr_free(x->lhs);

	if(x->rhs != NULL)
		node_expr_free(x->rhs);

	free(x);
}

/// allocate and return resultcol struct with no label
node_resultcol *node_resultcol_build(node_expr *expr)
{
    node_resultcol *x = (node_resultcol*)malloc(sizeof(node_resultcol));
    x->expr = expr;
    x->output_name = node_expr_tostring(expr);

    return x;
}

/// allocate and return resultcol struct with a label
node_resultcol *node_resultcol_buildas(node_expr *expr, char *output_name)
{
    node_resultcol *x = (node_resultcol*)malloc(sizeof(node_resultcol));
    x->expr = expr;
    x->output_name = output_name;

    return x;
}

/// recursively deallocate a list of resultcolumns
void node_resultcol_free(node_resultcol *x)
{
	node_expr_free(x->expr);
	free(x->output_name);

	if(x->next != NULL)
		node_resultcol_free(x->next);

	free(x);
}

/// allocate and return new condition node
node_condition *node_condition_build(int type, node_expr *lhs, node_expr *rhs)
{
	assert(lhs != NULL);
	assert(rhs != NULL);
	assert(type >= NODE_COND_EQ && type <= NODE_COND_GE);

	node_condition *x = (node_condition*)malloc(sizeof(node_condition));
	x->type = type;
	x->orfirst = 0;
	x->lhs = lhs;
	x->rhs = rhs;

	x->andcond = NULL;
	x->orcond = NULL;

	return x;
}

/// recursively deallocate tree of conditions
void node_condition_free(node_condition *x)
{
	node_expr_free(x->lhs);
	node_expr_free(x->rhs);

	if(x->andcond != NULL)
		node_condition_free(x->andcond);

	if(x->orcond != NULL)
		node_condition_free(x->orcond);

	free(x);
}

/// allocate and return new SELECT AST base
node_select *node_select_build(char *tablename,
    node_resultcol *resultcols, node_condition *conditions)
{
    node_select *x = (node_select*)malloc(sizeof(node_select));

	// locate table id given its name, store only that id
    unsigned table_id;
    int r = virg_table_getid(parse_virg, tablename, &table_id);

    if(r == VIRG_FAIL) {
        free(x);
        char buff[64];
        sprintf(buff, "could not find table %s", tablename);
        yyerror(buff);
    }

    x->table_id = table_id;
    x->resultcols = resultcols;
    x->conditions = conditions;

    return x;
}

/// free select node and its trees of resultcolumns and conditions
void node_select_free(node_select *x)
{
	node_resultcol_free(x->resultcols);

	if(x->conditions != NULL)
		node_condition_free(x->conditions);

	free(x);
}

/// allocate and return root node of the SQL AST
virg_node_root *node_root_build(int query_type, void *query)
{
    virg_node_root *x = (virg_node_root*)malloc(sizeof(virg_node_root));
    x->query_type = query_type;

    switch(query_type) {
        case QUERY_TYPE_SELECT :
            x->query.select = (node_select*)query;
            break;

		default :
			assert(0);
    }

    return x;
}

/// deallocate SQL statement abstract syntax tree
void node_root_free(virg_node_root *x)
{
	switch(x->query_type) {
		case QUERY_TYPE_SELECT :
			node_select_free(x->query.select);
			break;

		default :
			assert(0);
	}

	free(x);
}

