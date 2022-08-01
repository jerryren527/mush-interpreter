#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "mush.h"
#include "debug.h"

/*  Structure of a node in our doubly linked list (with a sentinel node) */
typedef struct stmt_node {
    STMT *statement;
    struct stmt_node *next;
    struct stmt_node *prev;
} STMT_NODE;

/*  Structure of the program store. 'head' is the sentinel node of the doubly linked list */
typedef struct program_store {
    int program_counter;
    int is_initialized;
    struct stmt_node *head;
} PROGRAM_STORE;

PROGRAM_STORE program_store;

/*
 * This is the "program store" module for Mush.
 * It maintains a set of numbered statements, along with a "program counter"
 * that indicates the current point of execution, which is either before all
 * statements, after all statements, or in between two statements.
 * There should be no fixed limit on the number of statements that the program
 * store can hold.
 */

/**
 * @brief  Output a listing of the current contents of the program store.
 * @details  This function outputs a listing of the current contents of the
 * program store.  Statements are listed in increasing order of their line
 * number.  The current position of the program counter is indicated by
 * a line containing only the string "-->" at the current program counter
 * position.
 *
 * @param out  The stream to which to output the listing.
 * @return  0 if successful, -1 if any error occurred.
 */
int prog_list(FILE *out) {
    // TO BE IMPLEMENTED
    
    // Traverse the program store's linked list, printing each node's statements.
    STMT_NODE *node_p = program_store.head->next;       // A node pointer.
    int i = -1;                                         // program counter tracker
    // program_store.program_counter = 2;

    // Show statements, print "-->" when i == program_counter
    while (node_p != program_store.head)
    {
        if (i == program_store.program_counter)
        {
            fprintf(out, "%s\n", "-->");
        }
        // debug("%d\n", program_store.program_counter);
        show_stmt(out, node_p->statement);
        node_p = node_p->next;
        i++;
    }
    return 0;
    // abort();
}

/**
 * @brief  Insert a new statement into the program store.
 * @details  This function inserts a new statement into the program store.
 * The statement must have a line number.  If the line number is the same as
 * that of an existing statement, that statement is replaced.
 * The program store assumes the responsibility for ultimately freeing any
 * statement that is inserted using this function.
 * Insertion of new statements preserves the value of the program counter:
 * if the position of the program counter was just before a particular statement
 * before insertion of a new statement, it will still be before that statement
 * after insertion, and if the position of the program counter was after all
 * statements before insertion of a new statement, then it will still be after
 * all statements after insertion.
 *
 * @param stmt  The statement to be inserted.
 * @return  0 if successful, -1 if any error occurred.
 */
int prog_insert(STMT *stmt) {
    // TO BE IMPLEMENTED
    int curr_node_lineno;       // The lineno of the curr_node.
    STMT_NODE *curr_node = malloc(sizeof(*curr_node));       // Initialize a STMT_NODE instance, curr_node. It represents the current node in the linked list and will contain 'stmt'.
                                                            // Allocate to heap because we want 'curr_node' to persist after leaving this function call.
    if (curr_node == NULL)
    {
        debug("Error calling malloc. prog_insert() is returning -1.\n");
        return -1;
    }

    if (stmt == NULL)
    {
        debug("stmt is NULL. Returning -1.\n");
        return -1;
    }

    // If first call to prog_insert(), initialize program_store (program_counter, is_initialized, and head).
    if (program_store.is_initialized == 0)
    {
        debug("program_store is uninitialized. Initializing it now...\n");
        program_store.program_counter = -1;        // program counter is -1 because -1 is immediately before the 0-th statement in program_store.
        program_store.is_initialized = 1;          // program_store is now initialized.
        
        // Initializing program_store's head (the sentinel node for our linked list).
        STMT_NODE *dummy = malloc(sizeof(*dummy));  // allocate space for program_store's head
        dummy->next = curr_node;
        dummy->prev = curr_node;
        program_store.head = dummy;                 // initialize program_store's head to a sentinel node.

        // Initialize a STMT_NODE instance, curr_node. Have its next and prev point to program_store.head
        curr_node->statement = stmt;
        curr_node->next = program_store.head;
        curr_node->prev = program_store.head;

    }
    else
    {
        // On subsequent calls to prog_insert(), initialize curr_node (statement, next, and prev).
        curr_node->statement = malloc(sizeof(*stmt));   // allocate memory to hold stmt in the node. The program store needs to free this allocated memory when the node gets removed from the program store.
        *(curr_node->statement) = *stmt;                // assign the contents of 'stmt' to the newly allocated memory.
        curr_node_lineno = stmt->lineno;

        
        // Traverse the linked list to find the rightful references for next and prev. This essentially sorts the nodes by lineno.
        STMT_NODE *node_p = program_store.head->next;       // A node pointer.
        while (node_p != program_store.head)
        {
            if (curr_node_lineno < node_p->statement->lineno)   // if stmt's lineno is less than the lineno of the node we are pointing to.
            {
                debug("This node (lineno: %d) needs to be swapped with curr_node (lineno: %d)\n", node_p->statement->lineno, curr_node_lineno);

                // Following algorithm. Inserting curr_node AFTER predecessor
                // current => predecessor
                // x => curr_node
                STMT_NODE *predecessor = node_p->prev;  // node_p is pointing to node will be the successor of curr_node. So predecessor is node_p->prev.
                curr_node->next = predecessor->next;    // Put curr_node directly before node_p.
                curr_node->prev = predecessor;
                predecessor->next->prev = curr_node;
                predecessor->next = curr_node;
                break;
            }
            else if (curr_node_lineno == node_p->statement->lineno) // if stmt's lineno is equal to the lineno of the node we are pointing to.
            {
                debug("This node (lineno: %d) needs to be replaced with curr_node (lineno: %d)\n", curr_node_lineno, node_p->statement->lineno);
                // STMT_NODE *predecessor = node_p->prev;  // node_p is pointing to node will be the successor of curr_node. So predecessor is node_p->prev.

                // detach node_p from the linked list, curr_node takes its place in the linked list.
                curr_node->next = node_p->next;
                curr_node->prev = node_p->prev;
                node_p->prev->next = curr_node;
                node_p->next->prev = curr_node;

                // free node_p and its statement because we do not use it anymore.
                free_stmt(node_p->statement);     // free the memory that currently holds the contents of 'stmt'.
                free(node_p);                     // free the node.
                break;
            }
            if (node_p->next == program_store.head)
            {
                debug("This node (lineno: %d) will be placed at the end of program store\n", curr_node_lineno);
                // STMT_NODE *predecessor = node_p->prev;  // node_p is pointing to node will be the successor of curr_node. So predecessor is node_p->prev.
                curr_node->next = program_store.head;
                curr_node->prev = node_p;
                node_p->next = curr_node;
                node_p = node_p->next;
                break;
            }
            else
                node_p = node_p->next;
        }
    }

    return 0;   // no errors.
    // abort();
}

/**
 * @brief  Delete statements from the program store.
 * @details  This function deletes from the program store statements whose
 * line numbers fall in a specified range.  Any deleted statements are freed.
 * Deletion of statements preserves the value of the program counter:
 * if before deletion the program counter pointed to a position just before
 * a statement that was not among those to be deleted, then after deletion the
 * program counter will still point the position just before that same statement.
 * If before deletion the program counter pointed to a position just before
 * a statement that was among those to be deleted, then after deletion the
 * program counter will point to the first statement beyond those deleted,
 * if such a statement exists, otherwise the program counter will point to
 * the end of the program.
 *
 * @param min  Lower end of the range of line numbers to be deleted.
 * @param max  Upper end of the range of line numbers to be deleted.
 */
int prog_delete(int min, int max) {
    // TO BE IMPLEMENTED
    STMT_NODE *node_p = program_store.head->next;       // A node pointer.

    if (min > max)
    {
        debug("min is greater than max. Returning -1.\n");
        return -1;
    }

    // Traverse the linked list to find a node whose lineno is in the range of [min,max].
    while (node_p != program_store.head)
    {
        if ((node_p->statement->lineno >= min) && (node_p->statement->lineno <= max))
        {
            STMT_NODE *curr_node = node_p;
            debug("This node (lineno %d) is in the range [%d, %d] and will be deleted.\n", node_p->statement->lineno, min, max);
            STMT_NODE *predecessor = curr_node->prev;
            predecessor->next = curr_node->next;   // change references of node_p's predecessor and successor to detach curr_node from linked list
            curr_node->next->prev = predecessor;

            // free curr_node because we do not use it anymore.
            node_p = node_p->next;
            free_stmt(curr_node->statement);        // free the memory that currently holds the contents of 'stmt'.
            free(curr_node);                        // free the node.
        }
        else
            node_p = node_p->next;
    }
    
    return 0;
    // abort();
}

/**
 * @brief  Reset the program counter to the beginning of the program.
 * @details  This function resets the program counter to point just
 * before the first statement in the program.
 */
void prog_reset(void) {
    // TO BE IMPLEMENTED
    debug("Resetting the program counter to -1.\n");
    program_store.program_counter = -1;
    return;
    // abort();
}

/**
 * @brief  Fetch the next program statement.
 * @details  This function fetches and returns the first program
 * statement after the current program counter position.  The program
 * counter position is not modified.  The returned pointer should not
 * be used after any subsequent call to prog_delete that deletes the
 * statement from the program store.
 *
 * @return  The first program statement after the current program
 * counter position, if any, otherwise NULL.
 */
STMT *prog_fetch(void) {
    // TO BE IMPLEMENTED
    /** PC of -1 is immediately before the first (index 0) program statement.
     *  PC of 0 is immediately before the second (index 1) program statement.
     *  So on and so forth.
     * 
     *  If PC=-1, I expect to return the 0th program statement.
     *  If PC=0, I expect to return the 1st program statement.
     *  So on and so forth.
     */

    // program_store.program_counter = -1;      // testing. 

    int i = 0;  // program counter tracker
    int target = program_store.program_counter + 1;     // I expect to return the target-th program statement, where the first program statement is considered the 0th.

    STMT_NODE *node_p = program_store.head->next;       // A node pointer

    while (node_p != program_store.head)
    {
        if (i == target)
        {
            return node_p->statement;
        }
        i++;
        node_p = node_p->next;
    }
    
    debug("There is no program statement after the current program counter position. Returning NULL.\n");
    return NULL;
    // abort();
}

/**
 * @brief  Advance the program counter to the next existing statement.
 * @details  This function advances the program counter by one statement
 * from its original position and returns the statement just after the
 * new position.  The returned pointer should not be used after any
 * subsequent call to prog_delete that deletes the statement from the
 * program store.
 *
 * @return The first program statement after the new program counter
 * position, if any, otherwise NULL.
 */
STMT *prog_next() {
    // TO BE IMPLEMENTED
    // program_store.program_counter = 2;       // testing.
    program_store.program_counter += 1;
    STMT *next = prog_fetch();
    return next;
    // abort();
}

/**
 * @brief  Perform a "go to" operation on the program store.
 * @details  This function performs a "go to" operation on the program
 * store, by resetting the program counter to point to the position just
 * before the statement with the specified line number.
 * The statement pointed at by the new program counter is returned.
 * If there is no statement with the specified line number, then no
 * change is made to the program counter and NULL is returned.
 * Any returned statement should only be regarded as valid as long
 * as no calls to prog_delete are made that delete that statement from
 * the program store.
 *
 * @return  The statement having the specified line number, if such a
 * statement exists, otherwise NULL.
 */
STMT *prog_goto(int lineno) {
    // TO BE IMPLEMENTED
    // program_store.program_counter = lineno;
    debug("Program counter: %d\n", program_store.program_counter);
    // int old_pc = program_store.program_counter; // Save the old program counter in case no statement with 'lineno' exists.

    int new_pc = -1;    // The new program counter initialized as -1. Gets incremented after looking at each statement that doesn't have 'lineno'.

    STMT_NODE *curr_stmt = program_store.head->next;    // A STMT_NODE pointer that points to the first STMT_NODE in the doubly linked list.
    STMT *ret;      // The STMT_NODE to be returned after finding the statement with 'lineno'.

    while (curr_stmt != program_store.head)     // Iterate through each STMT_NODE in the doubly linked list.
    {
        if (curr_stmt->statement->lineno == lineno)
        {
            debug("Statement with lineno %d found.\n", lineno);
            debug("Setting program store's program counter to %d\n", new_pc);
            program_store.program_counter = new_pc;
            ret = prog_fetch();
            return ret;
        }
        curr_stmt = curr_stmt->next;
        new_pc++;
    }

    debug("Statement with lineno %d was not found. Returning NULL.\n", lineno);
    return NULL;
    // abort();
}
