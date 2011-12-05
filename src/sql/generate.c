
#include "virginian.h"
#include "node.h"

/**
 * @file
 * @author Peter Bakkum <pbb7c@virginia.edu>
 *
 * @section License
 *
 * This software is provided "as is" and any expressed or implied warranties,
 * including, but not limited to, the implied warranties of merchantibility and
 * fitness for a particular purpose are disclaimed. In no event shall the
 * regents or contributors be liable for any direct, indirect, incidental,
 * special, exemplary, or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *      
 * @section Description 
 *
 * This file contains the code generator used to turn an AST into opcodes that 
 * are executed using the returned virtual machine. It is accessed throug the
 * virg_sql_generate() function. Since this code encapsulates a lot of
 * complexity not touched in any other part of the code its all thrown together
 * and not documented as rigorously as other parts of the codebase. Currently
 * only simple SELECT statements are supported.
 *
 * The code generator has the following passes:
 *
 * - Pass 0: Resolve datatypes of expressions.
 * - Pass 1: Resolve cases where both sides of an operator in an expression is a
 *   constant value, simplify to a single constant expression.
 * - Pass 2: Create the basic structure of the statement, adding opcodes to a
 *   list.
 * - Pass 3: Assign indices to opcodes.
 * - Pass 4: Resolve register indirection and jumps between opcodes.
 * - Pass 5: Output final opcodes.
 */

/// Abstracted virg_op structure with metadata used in code generation.
typedef struct absop {
	/// final fixed index of the operator
	int index;
	/// operator proper
	virg_op op;
	/// pointer to another op used if this op can jump to another location
	struct absop *opptr;
	/// next op in linked list of ops built during code generation
	struct absop *next;
} absop;

/// connects a vm register to an expression
typedef struct reg {
	int index;
	node_expr *expr;
} reg;

/// table of vm registers
reg reg_table[VIRG_REGS];
/// counter of currently used vm registers
int regcounter;

/** Compares two expressions, recursing to sub-expressions if necessary, to
 * determine if they are identical. This is used in register assignment to check
 * if an expression has already been stored in a register.
 */
int expr_equal(node_expr *x1, node_expr *x2)
{
	if(x1->type != x2->type)
		return 0;

	switch(x1->type) {
		case NODE_EXPR_INT :
			return x1->val.i == x2->val.i;

		case NODE_EXPR_FLOAT :
			return x1->val.f == x2->val.f;

		case NODE_EXPR_STRING :
			return strcmp(x1->val.s, x2->val.s) == 0;

		// if both are columns, we compare the column id
		case NODE_EXPR_COLUMN :
			return (x1->iskey == x2->iskey && x1->val.i == x2->val.i);

		// if this is a math op we must compare the op and both sides
		case NODE_EXPR_OP :
			return x1->val.i == x2->val.i &&
				expr_equal(x1->lhs, x2->lhs) &&
				expr_equal(x1->rhs, x2->rhs);

		default :
			assert(0);
	}
}

/// loops through registers to check if the passed expression is identical
int expr_findreg(node_expr *x)
{
	for(int i = 0; i < regcounter; i++) {
		assert(reg_table[i].expr != NULL);

		if(expr_equal(x, reg_table[i].expr))
			return i;
	}

	return -1;
}

/// get first unassigned register and increment counter
int getreg()
{
	assert(regcounter < VIRG_REGS);
	reg_table[regcounter].expr = NULL;

	return regcounter++;
}

/// assigns each registers index based on its position in register table
void regindex()
{
	for(int i = 0; i < regcounter; i++)
		reg_table[i].index = i;
}

/** Initializes an abstract operator object, assigning arguments and allowing a
 * pointer to another op to be added. This pointer is resolved later on.
 */
absop *create_absop(int op, int p1, int p2, int p3,
	absop *opptr)
{
	absop *x = (absop*)malloc(sizeof(absop));
	
	x->op.op = op;
	x->op.p1 = p1;
	x->op.p2 = p2;
	x->op.p3 = p3;
	x->op.p4.i = 0;
	x->opptr = opptr;
	x->next = NULL;

	return x;
}

/// appends absop to a linked list of absops used to build the statement
void append(absop **ops_list, absop *newop)
{
	if(ops_list[0] == NULL) {
		ops_list[0] = newop;
		return;
	}

	absop *x = ops_list[0];
	for(; x->next != NULL; x = x->next) {}

	x->next = newop;
}

/// Used in pass 0 to recurse through expression trees to resolve datatypes
int select_columnpass_recurse(node_expr *x, unsigned table_id)
{
	unsigned u = 0;

	/// switch based on expression type
	switch(x->type) {
		// if the expression is a column, we must look at the table metadata to
		// determine what type it is
		case NODE_EXPR_COLUMN :
			x->iskey = (strcmp(x->val.s, "id") == 0);

			// get column id from table
			if(!x->iskey) {
				VIRG_CHECK(virg_table_getcolumn(parse_virg, table_id,
						x->val.s, &u) == VIRG_FAIL,
					"select_columnpass_recurse() could not locate column");
			}

			// free table name now that we have its id
			free(x->val.s);
			x->val.u = x->iskey ? 0 : u;

			// get datatype of column
			virg_t type;
			if(x->iskey)
				virg_table_getkeytype(parse_virg, table_id, &type);
			else
				virg_table_getcolumntype(parse_virg, table_id, u, &type);

			x->datatype = type;
			break;

		// if this expression is an operation we must recurse down each side,
		// then set this node's datatype to the more general type
		case NODE_EXPR_OP :
			VIRG_CHECK(select_columnpass_recurse(x->lhs, table_id) == VIRG_FAIL,
				"select_columnpass_recurse() failure");
			VIRG_CHECK(select_columnpass_recurse(x->rhs, table_id) == VIRG_FAIL,
				"select_columnpass_recurse() failure");

			// get the more general of the two datatypes
			x->datatype = virg_generalizetype(x->lhs->datatype,
				x->rhs->datatype);
			break;

		// this expression is a constant int
		case NODE_EXPR_INT :
			x->datatype = VIRG_INT;
			break;

		// this expression is a constant float
		case NODE_EXPR_FLOAT :
			x->datatype = VIRG_FLOAT;
			break;

		default: assert(0);
	}

	return VIRG_SUCCESS;
}

/** This is used in pass 0 to recurse through a tree of condition nodes, calling
 * another function for each expression found
 */
void select_columnpass_condrecurse(node_condition *x, unsigned table_id)
{
	// call on left and right hand expressions
	select_columnpass_recurse(x->lhs, table_id);
	select_columnpass_recurse(x->rhs, table_id);

	// recurse through and condition
	if(x->andcond != NULL)
		select_columnpass_condrecurse(x->andcond, table_id);

	// recurse through or condition
	if(x->orcond != NULL)
		select_columnpass_condrecurse(x->orcond, table_id);
}

/** Pass 0
 * This is used to resolve the datatypes of expressions, including columns.
 * Doing this requires iterating through each result column and condition, and
 * recursing through trees of expressions
 */
int select_columnpass(node_select *root)
{
	// must have at least output col
	assert(root->resultcols != NULL);

	// iterate through each result column
	node_resultcol *col = root->resultcols;
	for(; col != NULL; col = col->next) {
		VIRG_CHECK(
			select_columnpass_recurse(col->expr, root->table_id) == VIRG_FAIL,
			"select_columnpass() failure");
	}

	// recurse through condition tree
	if(root->conditions != NULL)
		select_columnpass_condrecurse(root->conditions, root->table_id);

	return VIRG_SUCCESS;
}

/// maps AST node types to Virginian node types
static const virg_t nodetotype[3] = { -1, VIRG_INT, VIRG_FLOAT };

// cast an expression's value to int
int expr_valint(node_expr *x)
{
	switch(x->type) {
		case NODE_EXPR_INT :
			return x->val.i;

		case NODE_EXPR_FLOAT :
			return (int)x->val.f;

		default : assert(0);
	}
}

// cast an expression's value to float
float expr_valfloat(node_expr *x)
{
	switch(x->type) {
		case NODE_EXPR_INT :
			return (float)x->val.i;

		case NODE_EXPR_FLOAT :
			return x->val.f;

		default : assert(0);
	}
}

// macro used to duplicate switch to perform math op
#define RUNOP(op)				\
	switch(op) {				\
		case NODE_OP_PLUS : result = op1 + op2; break;	\
		case NODE_OP_MINUS : result = op1 - op2; break;	\
		case NODE_OP_MUL : result = op1 * op2; break;	\
		case NODE_OP_DIV : result = op1 / op2; break;	\
		default : assert(0);							\
	}

/** Recurse through a tree of expressions, checking if both sides of an operator
 * are constants. If that is the case, perform the math necessary to simplyify
 * the operation to a single constant
 */
void select_resolveopspass_recurse(node_expr *x)
{
	// if this expression is not an operation, we can't go any deeper
	if(x->type != NODE_EXPR_OP)
		return;

	// recurse down left and right sides
	select_resolveopspass_recurse(x->lhs);
	select_resolveopspass_recurse(x->rhs);
	
	// both left and right are constants
	if((x->lhs->type != NODE_EXPR_INT && x->lhs->type != NODE_EXPR_FLOAT) ||
		(x->rhs->type != NODE_EXPR_INT && x->rhs->type != NODE_EXPR_FLOAT))
		return;

	assert(nodetotype[NODE_EXPR_INT] == VIRG_INT);
	assert(nodetotype[NODE_EXPR_FLOAT] == VIRG_FLOAT);

	// since the constants may be of different types, we must output the result
	// in the more general type
	virg_t targettype = virg_generalizetype(
		nodetotype[x->lhs->type], nodetotype[x->rhs->type]);

	switch(targettype) {
		case VIRG_INT :
			{
				// cast both values
				int op1 = expr_valint(x->lhs);
				int op2 = expr_valint(x->rhs);
				int result;
				// perform the operation
				RUNOP(x->val.i);
				// set this node to be a constant of the result value
				x->type = NODE_EXPR_INT;
				x->val.i = result;
			}
			break;

		case VIRG_FLOAT :
			{
				float op1 = expr_valint(x->lhs);
				float op2 = expr_valint(x->rhs);
				float result;
				RUNOP(x->val.i);
				x->type = NODE_EXPR_FLOAT;
				x->val.f = result;
			}
			break;

		default : assert(0);
	}

	x->datatype = targettype;
}

/** This recurses through the tree of conditions, calling another function for
 * any expressions found
 */
void select_resolveopspass_condrecurse(node_condition *x)
{
	// go down left and right expression of condition
	select_resolveopspass_recurse(x->lhs);
	select_resolveopspass_recurse(x->rhs);

	// recurse through AND
	if(x->andcond != NULL)
		select_resolveopspass_condrecurse(x->andcond);

	// recurse through OR
	if(x->orcond != NULL)
		select_resolveopspass_condrecurse(x->orcond);
}

// PASS 1
// resolve cases where both sides of an operator are constants
/** Pass 1
 * This pass recurses through all expressions in the queries looking for cases
 * where both sides of an operator are constant values, since these can be
 * simplified to another constant
 */
int select_resolveopspass(node_select *root)
{
	node_resultcol *col = root->resultcols;

	// iterate through result columns
	for(; col != NULL; col = col->next)
		select_resolveopspass_recurse(col->expr);

	// start recursing through any conditions
	if(root->conditions != NULL)
		select_resolveopspass_condrecurse(root->conditions);

	return VIRG_SUCCESS;
}

/** Recursively resolve expressions and assign them to a register so they can be
 * accessed either for a result column or condition.
 */
int select_structurepass_expr(absop *ops_list, node_expr *expr)
{
	// check if a register already contains an identical expression
	// if so, we return that reg
	int reg = expr_findreg(expr);
	if(reg != -1)
		return reg;

	absop *newop;
	int reg1, reg2;

	// switch on the type of the expression
	switch(expr->type) {
		// constant integer
		case NODE_EXPR_INT:
			reg = getreg();
			newop = create_absop(OP_Integer, reg, expr->val.i, 0, NULL);
			append(&ops_list, newop);
			break;

		// constant float
		case NODE_EXPR_FLOAT:
			reg = getreg();
			newop = create_absop(OP_Float, reg, 0, 0, NULL);
			newop->op.p4.f = expr->val.f;
			append(&ops_list, newop);
			break;

		case NODE_EXPR_STRING:
			assert(0);
			break;

		// this node is an operation between two expressions
		case NODE_EXPR_OP:
			// recurse down both expressions
			reg1 = select_structurepass_expr(ops_list, expr->lhs);
			reg2 = select_structurepass_expr(ops_list, expr->rhs);
			// TODO handle runtime type casting
			
			reg = getreg();
			int op;

			// add the math operation opcode
			switch(expr->val.i) {
				case NODE_OP_PLUS : op = OP_Add; break;
				case NODE_OP_MINUS : op = OP_Sub; break;
				case NODE_OP_MUL : op = OP_Mul; break;
				case NODE_OP_DIV : op = OP_Div; break;
				default: assert(0);
			}

			newop = create_absop(op, reg, reg1, reg2, NULL);
			append(&ops_list, newop);
			break;

		// this expression is a value loaded from a column at runtime
		case NODE_EXPR_COLUMN:
			reg = getreg();
			if(expr->iskey)
				newop = create_absop(OP_Rowid, reg, 0, 0, NULL);
			else
				newop = create_absop(OP_Column, reg, expr->val.u, 0, NULL);
			append(&ops_list, newop);
			break;
	}

	// assign the register to this expression
	reg_table[reg].expr = expr;

	return reg;
}

/** This function turns a tree of WHERE clause conditions into opcodes for
 * filtering results. At a high level, this works in the following manner. Each
 * condition is an operator that compares two expressions with a boolean result.
 * Conditions can also have AND and OR conditions, thus forming a tree. If a
 * condition node has an AND condition, we know that failure of this node's
 * condition invalidates the clause, thus we jump forward to the next condition
 * to be evaluated, possibly the end of the conditions. If this node has no
 * children or is group with an OR condition at a higher order of operations
 * than the AND, then we jump when this condition evaluates to true. In this way
 * we recurse down a tree of both AND and OR conditions.
 */
absop *select_structurepass_condrecurse(node_condition *x, absop *ops_list,
	absop *onsuccess, absop *onfailure, absop *newop)
{
	// resolve the left and right side expressions of the condition
	int reg1 = select_structurepass_expr(ops_list, x->lhs);
	int reg2 = select_structurepass_expr(ops_list, x->rhs);

	int op;

	// if this is a leaf node or the OR has higher precedence than the AND
	if(x->andcond == NULL || (x->orcond != NULL && x->orfirst)) {
		
		// jump on success
		switch(x->type) {
			case NODE_COND_EQ: op = OP_Eq; break;
			case NODE_COND_NE: op = OP_Neq; break;
			case NODE_COND_LT: op = OP_Lt; break;
			case NODE_COND_LE: op = OP_Le; break;
			case NODE_COND_GT: op = OP_Gt; break;
			case NODE_COND_GE: op = OP_Ge; break;
			default: assert(0);
		}

		absop *andop = NULL;

		// if there is a lower precedence AND, we know a successful OR jumps to
		// it, so we create it and use it later
		if(x->andcond != NULL) {
			andop = create_absop(OP_Nop, 0, 0, 0, NULL);
			onsuccess = andop;
		}

		// if we have not been handed an op from above, we create it
		if(newop == NULL)
			newop = create_absop(OP_Nop, 0, 0, 0, NULL);

		// these comparisons are of the pattern
		// OP, value 1 register, value 2 register, jump location if comparison
		// evaluates to true, validity if op evaluates to true
		newop->op.op = op;
		newop->op.p1 = reg1;
		newop->op.p2 = reg2;
		newop->op.p4.i = 1;
		newop->opptr = onsuccess;
		append(&ops_list, newop);

		// if this is not a leaf node
		if(x->orcond != NULL)
			select_structurepass_condrecurse(x->orcond, ops_list,
				onsuccess, onfailure, NULL);

		// recurse down lower precedence AND
		if(x->andcond != NULL)
			select_structurepass_condrecurse(x->andcond, ops_list,
				onfailure, onfailure, andop);
	}
	// this must be part of an AND, we fail if any condition fails
	else {
		// jump on failure
		switch(x->type) {
			case NODE_COND_EQ: op = OP_Neq; break;
			case NODE_COND_NE: op = OP_Eq; break;
			case NODE_COND_LT: op = OP_Ge; break;
			case NODE_COND_LE: op = OP_Gt; break;
			case NODE_COND_GT: op = OP_Le; break;
			case NODE_COND_GE: op = OP_Lt; break;
			default: assert(0);
		}

		absop *orop = NULL;

		// if there is an OR, we create it and use it later
		if(x->orcond != NULL) {
			orop = create_absop(OP_Nop, 0, 0, 0, NULL);
			onfailure = orop;
		}

		// if we have not been handed an op from above, we create it
		if(newop == NULL)
			newop = create_absop(OP_Nop, 0, 0, 0, NULL);

		// same pattern as above
		newop->op.op = op;
		newop->op.p1 = reg1;
		newop->op.p2 = reg2;
		newop->op.p4.i = 0;
		newop->opptr = onfailure;
		append(&ops_list, newop);

		// recurse down other AND, jumping to exit or possible the OR from above
		if(x->andcond != NULL)
			select_structurepass_condrecurse(x->andcond, ops_list,
				onsuccess, onfailure, NULL);

		// if we have an OR condition, recurse to it
		if(x->orcond != NULL)
			select_structurepass_condrecurse(x->orcond, ops_list,
				onsuccess, onfailure, orop);
	}
	
	return newop;
}

/** Pass 2
 * This pass creates the basic structure of a select statement expressed in
 * opcodes. Select statements do the following:
 * - Choose a certain table
 * - Initialize result columns
 * - Begin the parallel section
 * - Resolve the expressions that represent each result column
 * - Resolve all the conditions that filter the results of the select
 * - Output result rows to the results tablet
 * - Exit
 */
int select_structurepass(node_select *root, absop **ops)
{
	regcounter = 0;
	// linked list representing the statement
	absop *ops_list = NULL;
	absop *newop;

	// add table initialization
	newop = create_absop(OP_Table, root->table_id, 0, 0, NULL);
	append(&ops_list, newop);

	// add result column setup
	node_resultcol *currcol = root->resultcols;
	assert(currcol != NULL);

	// iterate through each result column
	for(; currcol != NULL; currcol = currcol->next) {
		// create buffer for this column's name
		char *buff = malloc(VIRG_MAX_COLUMN_NAME);
		strcpy(buff, currcol->output_name);

		// create resultcolumn op
		newop = create_absop(OP_ResultColumn,
			currcol->expr->datatype, 0, 0, NULL);
		newop->op.p4.s = buff;
		append(&ops_list, newop);
	}

	// Begin parallel section, with opcodes for outputting results and exiting
	// the parallel section which we will later append to the statement.
	// We add them now so that forward pointing opcodes can use them
	absop *result = create_absop(OP_Result, 0, 0, 0, NULL);
	absop *converge = create_absop(OP_Converge, 0, 0, 0, NULL);
	newop = create_absop(OP_Parallel, 0, 0, 0, converge);
	append(&ops_list, newop);

	// resolve conditions
	if(root->conditions != NULL) {
		absop *stub = create_absop(OP_Nop, 0, 0, 0, NULL);

		// recurse through condition tree, adding opcodes as necessary
		select_structurepass_condrecurse(root->conditions, ops_list,
			stub, result, NULL);

		// since conditions jump forward to output results, anything that doesnt
		// jump is assumed to be invalid
		newop = create_absop(OP_Invalid, 0, 0, 0, NULL);
		append(&ops_list, newop);

		append(&ops_list, stub);
	}

	// resolve result column expressions
	currcol = root->resultcols;
	for(; currcol != NULL; currcol = currcol->next) {
		int reg = select_structurepass_expr(ops_list, currcol->expr);
		currcol->output_reg = reg;
	}

	// rearrange registers so output columns are contiguous
	// TODO check to make sure resultcols dont use the same reg
	// 		copy if they do
	regindex();
	currcol = root->resultcols;
	int numrescols = 0;

	for(; currcol != NULL; currcol = currcol->next) {
		numrescols++;
		int i = currcol->output_reg;

		// already at back of registers
		if(reg_table[i].index == regcounter - 1)
			continue;

		int oldindex = reg_table[i].index;
		int oldi = i;
		reg_table[i].index = regcounter - 1;

		//printf("column %i old %i new %i\n", numrescols - 1, oldindex, regcounter - 1);

		for(i = 0; i < regcounter; i++)
			if(i != oldi && reg_table[i].index > oldindex)
				reg_table[i].index--;
	}

	// output result columns
	result->op.p1 = root->resultcols->output_reg;
	result->op.p2 = numrescols;
	append(&ops_list, result);

	// finish up parallel section
	append(&ops_list, converge);

	// add finish op
	newop = create_absop(OP_Finish, 0, 0, 0, NULL);
	append(&ops_list, newop);

	ops[0] = ops_list;
	return VIRG_SUCCESS;
}

/** Pass 3
 * This pass just assigns the proper index to each op
 */
void select_opplacepass(absop *aop)
{
	for(int i = 0; aop != NULL; aop = aop->next) {
		if(aop->op.op != OP_Nop)
			aop->index = i++;
		else
			aop->index = i;
	}
}

/** Pass 4
 * This pass resolves the actual location of registers, since they've been
 * rearranged in the original array, and also handles cases where an op jumps to
 * another op. We must replace the pointer with the index to that other op
 */
void select_registerpass(absop *aop)
{
	// iterate through all ops
	for(; aop != NULL; aop = aop->next) {
		//fprintf(stderr, "::%i\n", aop->op.op);
		switch(aop->op.op) {
			case OP_Parallel :
				// resolve index of forward op we are pointing to with opptr
				aop->op.p3 = aop->opptr->index + 1;
				break;

			case OP_Integer :
			case OP_Column :
			case OP_Rowid :
			case OP_Result :
			case OP_Float :
				// resolve actual register index
				aop->op.p1 = reg_table[aop->op.p1].index;
				break;

			// these ops resolve 3 registers
			case OP_Add :
			case OP_Sub :
			case OP_Mul :
			case OP_Div :
				aop->op.p1 = reg_table[aop->op.p1].index;
				aop->op.p2 = reg_table[aop->op.p2].index;
				aop->op.p3 = reg_table[aop->op.p3].index;
				break;

			// resolve 2 registers and forward pointing jump location
			case OP_Eq :
			case OP_Neq :
			case OP_Le :
			case OP_Lt :
			case OP_Ge :
			case OP_Gt :
				aop->op.p1 = reg_table[aop->op.p1].index;
				aop->op.p2 = reg_table[aop->op.p2].index;
				aop->op.p3 = aop->opptr->index;
				break;

			// no register use
			case OP_Table :
			case OP_Invalid :
			case OP_ResultColumn :
			case OP_Converge :
			case OP_Finish :
			case OP_Nop :
				break;

			default :
				assert(0);
		}
	}
}

/** Pass 5
 * Copy all ops from the linked list to a vm
 */
void select_outputpass(absop *aop, virg_vm *vm)
{
	for(; aop != NULL; aop = aop->next)
		if(aop->op.op != OP_Nop)
			virg_vm_addop(vm, aop->op.op, aop->op.p1, aop->op.p2,
				aop->op.p3, aop->op.p4);
}

/** Pass 6
 * Cleans up all the abstract ops we had in our linked list
 */
void select_cleanuppass(absop *aop)
{
	absop *temp;

	for(; aop != NULL; aop = temp) {
		temp = aop->next;
		free(aop);
	}
}

/** Generate a select statement. This function just calls all the passes in
 * order, passing around the AST and a linked list of opcodes that we output to
 * a virtual machine.
 */
int virg_sql_genselect(node_select *root, virg_vm *vm)
{
	select_columnpass(root);
	absop *ops;
	select_resolveopspass(root);
	select_structurepass(root, &ops);
	select_opplacepass(ops);
	select_registerpass(ops);
	select_outputpass(ops, vm);
	select_cleanuppass(ops);

	return VIRG_SUCCESS;
}
        

/**
 * @ingroup sql
 * @brief 
 *
 * Generate opcodes from parsed sql tree
 *
 * @param v		Pointer to the state struct of the database system
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_sql_generate(virg_node_root *root, virg_vm *vm)
{
	switch(root->query_type) {
		case QUERY_TYPE_SELECT:
			virg_sql_genselect(root->query.select, vm);
			break;
	}

	return VIRG_SUCCESS;
}

