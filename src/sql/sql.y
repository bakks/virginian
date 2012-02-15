%{

/**
 * @file
 * @section Description
 *
 * This file is processed by Bison and used in conjunction with the lexical
 * analyzer produced with sql.l to generate a SQL parser. Basic and incomplete
 * SQL syntax is represented below. Running the parser creates an abstract
 * syntax tree (AST) of structures defined in node.h, which is used by the code
 * generator to produce opcodes used to execute queries using the database VM.
 *
 * This file also contains the virg_sql() function used to call the parser and
 * code generator.
 */


#include <stdio.h>
#include <stdlib.h>
#include "virginian.h"
#include "node.h"

/// root node of AST
virg_node_root *query_root;

/// created by Flex from sql.l
extern int yylex();

/// function called on a syntax error
void yyerror(char *s);

/// defined when Bison is run on this file
extern void yy_scan_string(const char*);
/// defined when Bison is run on this file
extern void yylex_destroy();

%}

 /* this parser currently has 4 shift/reduce conflicts */
%expect 4

 /* union representing all possible nodes of the AST */
%union {
    int i;
    float f;
    char *s;
	node_select *select;
	node_resultcol *resultcol;
	node_condition *condition;
	node_expr *expr;
	int token;
}

 /* tokens defined in sql.l */
%token TSELECT TFROM TWHERE TAS TCOMMA TLP TRP TAND TOR
%token <i> TINT
%token <f> TFLOAT
%token <s> TSTRING
%token <token> TPLUS TMINUS TMUL TDIV
%token <token> TCEQ TCNE TCLT TCLE TCGT TCGE

 /* creates an order of operations, the last defined being the highest order */
%left TOR
%left TAND
%left TPLUS TMINUS
%left TMUL TDIV

 /* AST node types of the syntax elements below */
%type <select> select
%type <resultcol> resultcols resultcol
%type <condition> conditions condition
%type <expr> expr
%type <i> operator conditionop

%%

 /* beginning of basic query */
query:
    select { 
		query_root = node_root_build(QUERY_TYPE_SELECT, $1);
	}
	;

 /* select statement syntax, currently parsing basic statement or basic
  * statement with where conditions. note that only a single table is currently
  * supported */
select:
    TSELECT resultcols TFROM TSTRING {
		$$ = node_select_build($4, $2, NULL);
		free($4);
	}
    | TSELECT resultcols TFROM TSTRING TWHERE conditions {
		$$ = node_select_build($4, $2, $6);
		free($4);
	}
	;

 /* one or more select result columns */
resultcols:
	resultcol {
		$1->next = NULL;
		$$ = $1;
	}
	| resultcol TCOMMA resultcols {
		$1->next = $3;
		$$ = $1;
	}
	;

 /* individual result column, allowing "column", "column label", and "column as
  * label" syntaxes. result columns are simply expressions with output labels */
resultcol:
	expr {
		$$ = node_resultcol_build($1);
	}
	| expr TSTRING {
		$$ = node_resultcol_buildas($1, $2);
	}
	| expr TAS TSTRING {
		$$ = node_resultcol_buildas($1, $3);
	}
	;

 /* list of one or more select column WHERE conditions. conditions can be linked
  * with AND or OR, and surrounded by parentheses to explicitly group. */
conditions:
	condition {
		$$ = $1;
	}
	| condition TAND conditions {
		$1->andcond = $3;
		$$ = $1;
	}
	| TLP condition TAND conditions TRP {
		$2->andcond = $4;
		$$ = $2;
	}
	| condition TOR conditions {
		$1->orcond = $3;
		$$ = $1;
	}
	/* conditions linked by OR and explicitly grouped with parens, explicitly
	 * set orfirst because we violate the usual order of operations */
	| TLP condition TOR conditions TRP {
		$2->orcond = $4;
		$2->orfirst = 1;
		$$ = $2;
	}
	;

 /* a WHERE condition consists of two expressions compared with an operator,
  * sometimes explicitly surrounded with parens */
condition:
	expr conditionop expr {
		$$ = node_condition_build($2, $1, $3);
	}
	| TLP expr conditionop expr TRP {
		$$ = node_condition_build($3, $2, $4);
	}
	;

 /* all possible comparison operators used in a WHERE condition */
conditionop:
	TCEQ { $$ = NODE_COND_EQ; }
	| TCNE { $$ = NODE_COND_NE; }
	| TCLT { $$ = NODE_COND_LT; }
	| TCLE { $$ = NODE_COND_LE; }
	| TCGT { $$ = NODE_COND_GT; }
	| TCGE { $$ = NODE_COND_GE; }
	;

 /* basic expression, used with output result columns and conditions. an
  * expression can be a numerical value, the name of a column pulled from the
  * table during the query, or an operation between two sub-expressions */
expr:
	TLP expr TRP {
		$$ = $2;
	}
	| expr operator expr {
		$$ = node_expr_buildop($2, $1, $3);
	}
	| TINT {
		$$ = node_expr_buildint($1);
	}
	| TFLOAT {
		$$ = node_expr_buildfloat($1);
	}
	/* name of a column accessed at runtime, not a string constant */
	| TSTRING {
		$$ = node_expr_buildcolumn($1);
	}
	;

 /* operators used to combine two expressions */
operator:
	TPLUS { $$ = NODE_OP_PLUS; }
	| TMINUS { $$ = NODE_OP_MINUS; }
	| TMUL { $$ = NODE_OP_MUL; }
	| TDIV { $$ = NODE_OP_DIV; }
	;


%%

/// prints out syntax errors during query parsing
void yyerror(char *s) {
    fprintf(stderr, "Parse Error :: %s\n", s);
    exit(-1);
}

/**
 * @ingroup virginian
 * @brief Parses a SQL query.
 *
 * This function parses a SQL query passed as a string, runs a code generator to
 * produce the approprate opcodes, and return a virtual machine ready to execute
 * the query. This function hides all the complexity of the lexer, parser, and
 * code generator.
 *
 * @param v Pointer to the state struct of the database system
 * @param querystr SQL query
 * @param vm Allocated virtual machine which will be prepared to execute the
 * passed query
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */

int virg_sql(virginian *v, const char *querystr, virg_vm *vm)
{
	parse_virg = v;

	yy_scan_string(querystr);
	yyparse();
	yylex_destroy();

	virg_sql_generate(query_root, vm);
	node_root_free(query_root);

	return VIRG_SUCCESS;
}

