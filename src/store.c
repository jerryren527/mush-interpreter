#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"

void store_show(FILE *f); 

typedef struct data_entry {
    char *var;
    char *val;
} DATA_ENTRY;

DATA_ENTRY *data_store;    // data_store is a dynamically allocated array. It will grow dynamically via calls to realloc(). 

int num_data_entries = 0;           // current number of data entries in data_store.
int max_num_data_entries = 10;  // initial number of data entries in data store array is 10.
/*
 * This is the "data store" module for Mush.
 * It maintains a mapping from variable names to values.
 * The values of variables are stored as strings.
 * However, the module provides functions for setting and retrieving
 * the value of a variable as an integer.  Setting a variable to
 * an integer value causes the value of the variable to be set to
 * a string representation of that integer.  Retrieving the value of
 * a variable as an integer is possible if the current value of the
 * variable is the string representation of an integer.
 */

/**
 * @brief  Get the current value of a variable as a string.
 * @details  This function retrieves the current value of a variable
 * as a string.  If the variable has no value, then NULL is returned.
 * Any string returned remains "owned" by the data store module;
 * the caller should not attempt to free the string or to use it
 * after any subsequent call that would modify the value of the variable
 * whose value was retrieved.  If the caller needs to use the string for
 * an indefinite period, a copy should be made immediately.
 *
 * @param  var  The variable whose value is to be retrieved.
 * @return  A string that is the current value of the variable, if any,
 * otherwise NULL.
 */
char *store_get_string(char *var) {
    // TO BE IMPLEMENTED
    // Look if 'var' in data_store
    int i;
    for(i = 0; i < num_data_entries; i++)
    {
        if (strcmp(data_store[i].var, var) == 0)
        {
            debug("var (%s) exists in data store.\n", data_store[i].var);
            return data_store[i].val;
        }
    }
    debug("var (%s) does not exist in data store. Returning -1.\n", data_store[i].var);
    return NULL;
    // abort();
}

/**
 * @brief  Get the current value of a variable as an integer.
 * @details  This retrieves the current value of a variable and
 * attempts to interpret it as an integer.  If this is possible,
 * then the integer value is stored at the pointer provided by
 * the caller.
 *
 * @param  var  The variable whose value is to be retrieved.
 * @param  valp  Pointer at which the returned value is to be stored.
 * @return  If the specified variable has no value or the value
 * cannot be interpreted as an integer, then -1 is returned,
 * otherwise 0 is returned.
 */
int store_get_int(char *var, long *valp) {
    // TO BE IMPLEMENTED
    // Look if 'var' in data_store
    int i;
    for(i = 0; i < num_data_entries; i++)
    {
        if (strcmp(data_store[i].var, var) == 0)
        {
            debug("var (%s) exists in data store.\n", data_store[i].var);
            int j;
            for (j = 0; j < strlen(data_store[i].val); j++)
            {
                if(!isdigit(data_store[i].val[j]))
                {
                    debug("The specified variable has no value or the value cannot be interpreted as an integer\n");
                    return -1;
                }
            }
            debug("The specified variable has a value that can be interpreted an in integer\n");
            *valp = atoi(data_store[i].val);
            return 0;
        }
    }
    debug("var (%s) does not exist in data store. Returning -1.\n", data_store[i].var);
    return -1;

    // abort();
}

/**
 * @brief  Set the value of a variable as a string.
 * @details  This function sets the current value of a specified
 * variable to be a specified string.  If the variable already
 * has a value, then that value is replaced.  If the specified
 * value is NULL, then any existing value of the variable is removed
 * and the variable becomes un-set.  Ownership of the variable and
 * the value strings is not transferred to the data store module as
 * a result of this call; the data store module makes such copies of
 * these strings as it may require.
 *
 * @param  var  The variable whose value is to be set.
 * @param  val  The value to set, or NULL if the variable is to become
 * un-set.
 */
int store_set_string(char *var, char *val) {
    // TO BE IMPLEMENTED
     // Look if 'var' is already in data_store
    int i;
    for(i = 0; i < num_data_entries; i++)
    {
        if (strcmp(data_store[i].var, var) == 0)
        {
            debug("var (%s) already exists in data store. Overwriting its value.\n", data_store[i].var);

            // free the old val.
            free(data_store[i].val);
            
            if (!val)   // If val is NULL (i.e. an unset statement),
            {
                data_store[i].val = NULL;   // Set value to NULL.
            }
            else
            {
                // allocate and assign new val
                data_store[i].val = malloc(strlen(val) * sizeof(char));
                strcpy(data_store[i].val, val);
                // store_show(stdout); // for debugging;
            }
            return 0;
        }
    }

    // 'var' is new and will be added to data_store
    if (num_data_entries == 0)
    {
        debug("data_store is empty. Allocating space for 10 data_entry objects.\n");
        data_store = calloc(max_num_data_entries, sizeof(*data_store));
        if (data_store == NULL)
        {
            debug("Error calling calloc(). Returning -1\n");
            return -1;
        }
        // set string here. We are allocating space for them because we want the strings to persist after leaving this function
        data_store[num_data_entries].var = malloc(strlen(var) * sizeof(char));
        strcpy(data_store[num_data_entries].var, var);

        data_store[num_data_entries].val = malloc(strlen(val) * sizeof(char));
        strcpy(data_store[num_data_entries].val, val);
        num_data_entries++;
    }
    else if (num_data_entries >= max_num_data_entries)
    {
        debug("data_store currently holds %d data_entry objects. It holds a max of %d objects. Resizing data_store..\n", num_data_entries, max_num_data_entries);
        max_num_data_entries *= 2;  // Growing twice as large
        data_store = realloc(data_store, max_num_data_entries * sizeof(*data_store));
        if (data_store == NULL)
        {
            debug("Error calling realloc(). Returning -1\n");
            return -1;
        }

        // set string here
        data_store[num_data_entries].var = malloc(strlen(var) * sizeof(char));
        strcpy(data_store[num_data_entries].var, var);

        data_store[num_data_entries].val = malloc(strlen(val) * sizeof(char));
        strcpy(data_store[num_data_entries].val, val);
        num_data_entries++;
    }
    else
    {
        // set string here
        data_store[num_data_entries].var = malloc(strlen(var) * sizeof(char));
        strcpy(data_store[num_data_entries].var, var);

        data_store[num_data_entries].val = malloc(strlen(val) * sizeof(char));
        strcpy(data_store[num_data_entries].val, val);
        num_data_entries++;
    }
    
    // store_show(stdout); // for debugging;
    return 0;
    // abort();
}

/**
 * @brief  Set the value of a variable as an integer.
 * @details  This function sets the current value of a specified
 * variable to be a specified integer.  If the variable already
 * has a value, then that value is replaced.  Ownership of the variable
 * string is not transferred to the data store module as a result of
 * this call; the data store module makes such copies of this string
 * as it may require.
 *
 * @param  var  The variable whose value is to be set.
 * @param  val  The value to set.
 */
int store_set_int(char *var, long val) {
    // TO BE IMPLEMENTED

     // Look if 'var' is already in data_store
    int i;
    for(i = 0; i < num_data_entries; i++)
    {
        if (strcmp(data_store[i].var, var) == 0)
        {
            debug("var (%s) already exists in data store. Overwriting its value.\n", data_store[i].var);

            // free the old val.
            free(data_store[i].val);
            // allocate and assign new val
            int length = snprintf(NULL, 0, "%ld", val); // snprintf() function shall return the number of bytes that would be written to s had n been sufficiently large excluding the terminating null byte.
            data_store[i].val = malloc(length + 1);
            snprintf(data_store[i].val, length+1, "%ld", val);
            // TODO; free the buffer after calling snprintf()?
            // store_show(stdout); // for debugging;
            return 0;
        }
    }

    // 'var' is new and will be added to data_store
    if (num_data_entries == 0)
    {
        debug("data_store is empty. Allocating space for 10 data_entry objects.\n");
        data_store = calloc(max_num_data_entries, sizeof(*data_store));
        if (data_store == NULL)
        {
            debug("Error calling calloc(). Returning -1\n");
            return -1;
        }
        // set int here. 
        data_store[num_data_entries].var = malloc(strlen(var) * sizeof(char));
        strcpy(data_store[num_data_entries].var, var);
        int length = snprintf(NULL, 0, "%ld", val); // snprintf() function shall return the number of bytes that would be written to s had n been sufficiently large excluding the terminating null byte.
        data_store[num_data_entries].val = malloc(length + 1);
        snprintf(data_store[num_data_entries].val, length+1, "%ld", val);
        // debug("str_val: %s\n", str_val);
        // debug("sizeof(buf): %lu\n", sizeof(buf));
        // data_store[num_data_entries].val = malloc(strlen(buf) * sizeof(char));
        num_data_entries++;
    }
    else if (num_data_entries >= max_num_data_entries)
    {
        debug("data_store currently holds %d data_entry objects. It holds a max of %d objects. Resizing data_store..\n", num_data_entries, max_num_data_entries);
        max_num_data_entries *= 2;  // Growing twice as large
        data_store = realloc(data_store, max_num_data_entries * sizeof(*data_store));
        if (data_store == NULL)
        {
            debug("Error calling realloc(). Returning -1\n");
            return -1;
        }

        // set int here
        data_store[num_data_entries].var = malloc(strlen(var) * sizeof(char));
        strcpy(data_store[num_data_entries].var, var);
        int length = snprintf(NULL, 0, "%ld", val); // snprintf() function shall return the number of bytes that would be written to s had n been sufficiently large excluding the terminating null byte.
        data_store[num_data_entries].val = malloc(length + 1);
        snprintf(data_store[num_data_entries].val, length+1, "%ld", val);
        num_data_entries++;
    }
    else
    {
        // set int here
        data_store[num_data_entries].var = malloc(strlen(var) * sizeof(char));
        strcpy(data_store[num_data_entries].var, var);
        int length = snprintf(NULL, 0, "%ld", val); // snprintf() function shall return the number of bytes that would be written to s had n been sufficiently large excluding the terminating null byte.
        data_store[num_data_entries].val = malloc(length + 1);
        snprintf(data_store[num_data_entries].val, length+1, "%ld", val);
        num_data_entries++;
    }
    
    // store_show(stdout); // for debugging;
    return 0;

    // abort();
}

/**
 * @brief  Print the current contents of the data store.
 * @details  This function prints the current contents of the data store
 * to the specified output stream.  The format is not specified; this
 * function is intended to be used for debugging purposes.
 *
 * @param f  The stream to which the store contents are to be printed.
 */
void store_show(FILE *f) {
    // TO BE IMPLEMENTED
    int i;
    if (data_store)
    {
        fprintf(f, "{");
        for (i = 0; i < max_num_data_entries; i++)
        {
            if ((data_store[i].var == NULL) && (data_store[i].val == NULL))
            {
                break;
            }
            fprintf(f, "%s=%s", data_store[i].var, data_store[i].val);
            if ((data_store[i+1].var) && (data_store[i+1].val))
            {
                fprintf(f, ",");
            }
        }
        fprintf(f, "}");
    }
    // else
        // fprintf(f, "data_store[%d]: (%s, %s)\n", i, data_store[i].var, data_store[i].val);
    return;
    // abort();
}
