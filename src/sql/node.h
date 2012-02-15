#pragma once

/**
 * @file
 * @section Description
 *
 * This file describes the structures and associated functions for the abstract
 * syntax tree created in the SQL parsing process. This file is used by both the
 * parser and the code generator.
 */

#include "virginian.h"

/// possible datatypes for an expression
#define NODE_EXPR_INT		1
#define NODE_EXPR_FLOAT		2
#define NODE_EXPR_OP		3
#define NODE_EXPR_STRING	4
#define NODE_EXPR_COLUMN	5

/// possible expression oprators
#define NODE_OP_PLUS		1
#define NODE_OP_MINUS		2
#define NODE_OP_MUL			3
#define NODE_OP_DIV			4

/// possible comparison operators used in a query condition
#define NODE_COND_EQ		1
#define NODE_COND_NE		2
#define NODE_COND_LT		3
#define NODE_COND_LE		4
#define NODE_COND_GT		5
#define NODE_COND_GE		6

/// possible query types
#define QUERY_TYPE_SELECT	1
#define QUERY_TYPE_INSERT	2

/// virginian object visible to the parser
virginian *parse_virg;

/// called on a syntax error
void yyerror(char *s);

/** 
 * @brief Defines an expression node of the AST.
 *
 * Expressions are how values are
 * represented and combined through mathematical operators. A value can be
 * either a constant or drawn from a data record. Expression nodes can represent
 * a constant value, a value drawn from a data record, or an operation used to
 * combine two sub expressions, thus a tree of expressions can be built
 */
typedef struct node_expr {
	/// a constant, a column from a data record, or an operation
	int type;
	/// datatype derived from the constant type, column type, or operation type
	virg_t datatype;
	/// left hand side of operation
	struct node_expr *lhs;
	/// right hand side of operation
	struct node_expr *rhs;
	/// if this expression is a column type, note whether that column is id
	int iskey;

	/// possible payload data types
	union {
		int i;
		float f;
		char *s;
		unsigned u;
	} val;
} node_expr;

/// allocate and return expression given a column name
node_expr *node_expr_buildcolumn(char *val);
/// allocate and return expression given a constant integer
node_expr *node_expr_buildint(int val);
/// allocate and return expression given a constant float
node_expr *node_expr_buildfloat(float val);
/// allocate and return expression given operation and two sub expressions
node_expr *node_expr_buildop(int op, node_expr *lhs, node_expr *rhs);
/// return string representation of expression
char *node_expr_tostring(node_expr *x);

/**
 * @brief Represents a result column used in a SELECT statement.
 *
 * Result columns are just expressions with a label used in the output of
 * results. This structure stores these and a pointer to another result column
 * to create a linked list of result columns.
 */
typedef struct node_resultcol {
	/// expression used as result value
	node_expr *expr;
	/// label for result column
	char *output_name;
	/// register holding the result expression value
	int output_reg;
	/// pointer to next resultcol in list
	struct node_resultcol *next;
} node_resultcol;

/// allocate and return result column with label
node_resultcol *node_resultcol_buildas(node_expr *expr, char *output_name);
/// allocate and return result column without label
node_resultcol *node_resultcol_build(node_expr *expr);

/**
 * @brief Represents a conditional SQL statement.
 *
 * Elements in SQL such as the WHERE clause use conditional statements to filter
 * results. Each condition is two expressions compared with a conditional
 * operator. Conditions can also be paired through ANDs and ORs with other
 * conditions
 */
typedef struct node_condition {
	/// comparison operater
	int type;
	/// boolean true ifOR condition has a higher precedence than AND condition
	int orfirst;
	/// AND condition
	struct node_condition *andcond;
	/// OR condition
	struct node_condition *orcond;
	/// left hand expression for comparison
	node_expr *lhs;
	/// right hand expression for comparison
	node_expr *rhs;
} node_condition;

/// allocate and return a condition node
node_condition *node_condition_build(int type, node_expr *lhs, node_expr *rhs);

/**
 * @brief Base of a SELECT statement AST
 */
typedef struct {
	/// list of result columns, of which there must be at least one
	node_resultcol *resultcols;
	/// list of conditions, can be null
	node_condition *conditions;
	/// id of table for select statement
	unsigned table_id;
} node_select;

/// allocate and return new select statement node
node_select *node_select_build(char *tablename,
	node_resultcol *resultcols, node_condition *conditions);

/**
 * @brief Base of an INSERT statement AST
 */
typedef struct {
	int x;
} node_insert;

/**
 * @brief Base of a SQL statement, serves as the root of the AST
 */
typedef struct {
	/// type of query for statement
	int query_type;

	/// various types of queries
	union {
		node_select *select;
	} query;
} virg_node_root;

/// allocate and return root of AST
virg_node_root *node_root_build(int query_type, void *query);
/// deallocate and cleanup AST
void node_root_free(virg_node_root *x);

/// generate SQL opcode statement given an AST
int virg_sql_generate(virg_node_root *root, virg_vm *vm);


