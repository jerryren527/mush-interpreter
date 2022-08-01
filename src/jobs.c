#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include "mush.h"
#include "debug.h"
#include "csapp.h"

typedef enum job_status {
    NEW,        // 0
    RUNNING,    // 1
    COMPLETED,  // 2
    ABORTED,    // 3
    CANCELED    // 4
} JOB_STATUS;

typedef struct job {
    PIPELINE *pipeline;
    int jobid;      // Unique job id.
    pid_t pid;      // PID of the pipeline leader process
    JOB_STATUS status;     // "new", "running", "completed", "aborted", or "canceled"
    int exit_status;    // exit status of the job, set when job has terminated
} JOB;

int num_jobs = 0;                 // Number of jobs currently in the jobs_table
int global_jobid = 0;             // global counter for unique job IDs;
JOB *jobs_table[MAX_JOBS];         // jobs_table is a statically sized array of JOB object pointers.

volatile sig_atomic_t pid;          // reads and writes to 'pid' are atomic
// volatile sig_atomic_t child_pid;    // reads and writes to 'pid' are atomic
sigset_t mask_all, mask_one, prev_one;  // Bit masks for Blocked bit vector.
volatile int children_reaped;



/* Helper Functions */
JOB *find_job(pid_t pid);
JOB *find_job_by_jobid(int jobid);
int how_many_commands(PIPELINE *pline);
int how_many_args(COMMAND *cmd);
void execute_pipe(char ****cmds_array, char *input_file, char *output_file, int num_cmds, PIPELINE *pline);
void create_cmds_array(PIPELINE *pline, char ****cmd);
void free_cmds_array(PIPELINE *pline, char ****cmd);

void sigchld_handler(int s)
{
    debug("Inside sigchld_handler\n");
    int olderrno = errno;
    int status;
    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)   // Reap any zombie children
    {
        debug("Process %d has been reaped.\n", pid);
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);   // Block all signals before accessing global data structure.
        
        if (WIFEXITED(status))
        {
            int exit_status = WEXITSTATUS(status);
            (void) exit_status;
            debug("The child process termianted normally with exit or _exit with status %d.\n", exit_status);
        }
        debug("Status: %d\n", status);
        JOB *completed_job = find_job(pid);
        if (completed_job)
        {
            debug("Pipeline leader process (pid %d) has been reaped.\n", pid);
            completed_job->status = COMPLETED;
            if (status == 139)
                completed_job->exit_status = 0;
            else if (status == 134)
            {
                completed_job->exit_status = 6;
            }
            else
                completed_job->exit_status = status; // Set the exit_status of the reaped 
        }
        else
        {
            debug("Child process (pid %d) has been reaped.\n", pid);    // Never gets displayed
        }
        sigprocmask(SIG_SETMASK, &prev_all, NULL);      // Reset the signals.
    }
    errno = olderrno;
}

/*
 * This is the "jobs" module for Mush.
 * It maintains a table of jobs in various stages of execution, and it
 * provides functions for manipulating jobs.
 * Each job contains a pipeline, which is used to initialize the processes,
 * pipelines, and redirections that make up the job.
 * Each job has a job ID, which is an integer value that is used to identify
 * that job when calling the various job manipulation functions.
 *
 * At any given time, a job will have one of the following status values:
 * "new", "running", "completed", "aborted", "canceled".
 * A newly created job starts out in with status "new".
 * It changes to status "running" when the processes that make up the pipeline
 * for that job have been created.
 * A running job becomes "completed" at such time as all the processes in its
 * pipeline have terminated successfully.
 * A running job becomes "aborted" if the last process in its pipeline terminates
 * with a signal that is not the result of the pipeline having been canceled.
 * A running job becomes "canceled" if the jobs_cancel() function was called
 * to cancel it and in addition the last process in the pipeline subsequently
 * terminated with signal SIGKILL.
 *
 * In general, there will be other state information stored for each job,
 * as required by the implementation of the various functions in this module.
 */

/**
 * @brief  Initialize the jobs module.
 * @details  This function is used to initialize the jobs module.
 * It must be called exactly once, before any other functions of this
 * module are called.
 *
 * @return 0 if initialization is successful, otherwise -1.
 */
int jobs_init(void) {
    // TO BE IMPLEMENTED
    // abort();
    debug("jobs_init installs sigchld_handler and initializes bit masks.\n");
    Signal(SIGCHLD, sigchld_handler);   // Install sigchld_handler.
    // The main mush process will block SIGCHLD signals so that it does not "lose" the receipt of any SIGCHLD signal.
    sigfillset(&mask_all);          // Create a mask with every bit set.
    sigemptyset(&mask_one); 
    sigaddset(&mask_one, SIGCHLD);  // Create SIGCHLD mask
    return 0;
}

/**
 * @brief  Finalize the jobs module.
 * @details  This function is used to finalize the jobs module.
 * It must be called exactly once when job processing is to be terminated,
 * before the program exits.  It should cancel all jobs that have not
 * yet terminated, wait for jobs that have been cancelled to terminate,
 * and then expunge all jobs before returning.
 *
 * @return 0 if finalization is completely successful, otherwise -1.
 */
int jobs_fini(void) {
    // TO BE IMPLEMENTED
    int i;
    for (i = 0; i < num_jobs; i++)
    {
        if (jobs_table[i]->status == COMPLETED)    // If job completed,
        {
            jobs_expunge(jobs_table[i]->jobid);         // then expunge it from the jobs table.
        }
        else if (jobs_table[i]->status == ABORTED)    // If job aborted,
        {
            jobs_expunge(jobs_table[i]->jobid);         // then expunge it from the jobs table.
        }
        else if (jobs_table[i]->status == CANCELED)    // If job canceled,
        {
            jobs_expunge(jobs_table[i]->jobid);         // then expunge it from the jobs table.
        }
        else
        {
            debug("Waiting for job (jobid %d) to terminate.\n", jobs_table[i]->jobid);
            int ret;
            if ((ret = jobs_wait(jobs_table[i]->jobid)) == -1)
            {
                debug("Error calling jobs_wait().\n");
                return -1;
            } 
            debug("Expunging job (jobid %d) to terminate.\n", jobs_table[i]->jobid);
            jobs_expunge(jobs_table[i]->jobid);
        }
    }
    return 0;
    // abort();
}

/**
 * @brief  Print the current jobs table.
 * @details  This function is used to print the current contents of the jobs
 * table to a specified output stream.  The output should consist of one line
 * per existing job.  Each line should have the following format:
 *
 *    <jobid>\t<pgid>\t<status>\t<pipeline>
 *
 * where <jobid> is the numeric job ID of the job, <status> is one of the
 * following strings: "new", "running", "completed", "aborted", or "canceled",
 * and <pipeline> is the job's pipeline, as printed by function show_pipeline()
 * in the syntax module.  The \t stand for TAB characters.
 *
 * @param file  The output stream to which the job table is to be printed.
 * @return 0  If the jobs table was successfully printed, -1 otherwise.
 */
int jobs_show(FILE *file) {
    // TO BE IMPLEMENTED
    debug("Entered jobs_show().\n");
    int i;
    // fprintf(file, "jobid\tpgid\tstatus\tpipeline\n");
    for (i = 0; i < num_jobs; i++)
    {
        fprintf(file, "%d\t%d\t", jobs_table[i]->jobid, jobs_table[i]->pid);
        switch(jobs_table[i]->status)
        {
            case 0:
                fprintf(file, "new\t");
                break;
            case 1:
                fprintf(file, "running\t");
                break;
            case 2:
                fprintf(file, "completed\t");
                break;
            case 3:
                fprintf(file, "aborted\t");
                break;
            case 4:
                fprintf(file, "canceled\t");
                break;
            default:
                ;
        }
        show_pipeline(file, jobs_table[i]->pipeline);
        fprintf(file, "\n");
    }
    return 0;
    // abort();
}

/**
 * @brief  Create a new job to run a pipeline.
 * @details  This function creates a new job and starts it running a specified
 * pipeline.  The pipeline will consist of a "leader" process, which is the direct
 * child of the process that calls this function, plus one child of the leader
 * process to run each command in the pipeline.  All processes in the pipeline
 * should have a process group ID that is equal to the process ID of the leader.
 * The leader process should wait for all of its children to terminate before
 * terminating itself.  The leader should return the exit status of the process
 * running the last command in the pipeline as its own exit status, if that
 * process terminated normally.  If the last process terminated with a signal,
 * then the leader should terminate via SIGABRT.
 *
 * If the "capture_output" flag is set for the pipeline, then the standard output
 * of the last process in the pipeline should be redirected to be the same as
 * the standard output of the pipeline leader, and this output should go via a
 * pipe to the main Mush process, where it should be read and saved in the data
 * store as the value of a variable, as described in the assignment handout.
 * If "capture_output" is not set for the pipeline, but "output_file" is non-NULL,
 * then the standard output of the last process in the pipeline should be redirected
 * to the specified output file.   If "input_file" is set for the pipeline, then
 * the standard input of the process running the first command in the pipeline should
 * be redirected from the specified input file.
 *
 * @param pline  The pipeline to be run.  The jobs module expects this object
 * to be valid for as long as it requires, and it expects to be able to free this
 * object when it is finished with it.  This means that the caller should not pass
 * a pipeline object that is shared with any other data structure, but rather should
 * make a copy to be passed to this function.
 * 
 * @return  -1 if the pipeline could not be initialized properly, otherwise the
 * value returned is the job ID assigned to the pipeline.
 */
int jobs_run(PIPELINE *pline) {
    // TO BE IMPLEMENTED
    JOB *job = malloc(sizeof(*job));    // allocate space for job node b/c I want it to persist.
    job->pipeline = malloc(sizeof(*pline));  // Allocate space for the pipeline in the job node.
    PIPELINE *pline_cpy = copy_pipeline(pline); // I want a copy of the pipeline to work with.
    job->pipeline = pline_cpy;          // Assign the pipeline copy to the allocated space.
    job->jobid = global_jobid++;    // Job id is set to the global_jobid counter. I incremented it so the next call to jobs_run() has a unique job id.
    job->status = NEW;  // Set job status to "new".
    
    char *input_file = NULL;    // Input file.
    char *output_file = NULL;   // Output file.
    if (pline->input_file)  // If pipeline's input_file is defined,
    {
        debug("There is an input file.\n");
        // input_file = malloc(strlen(pline->input_file) + 1); // Allocate memeory for it
        // strcpy(input_file, pline->input_file);  // And copy it to 'input_file'
        input_file = strdup(pline->input_file);
    }
    if (pline->output_file) // If pipeline's output_file is defined,
    {
        debug("There is an output file.\n");
        // output_file = malloc(strlen(pline->output_file) + 1);   // Allocate memeory for it
        // strcpy(output_file, pline->output_file);    // And copy it to 'output_file'
        output_file = strdup(pline->output_file);

    }
    
    debug("Main process (pid: %d. pgid: %d) is BLOCKING SIGCHLD.\n", getpid(), getpgid(getpid()));
    sigprocmask(SIG_BLOCK, &mask_one, &prev_one);  // In the main mush process, block SIGCHLD signal.
    if ((pid = fork()) == 0)    // Inside the pipeline leader process
    {
        debug("Entered Pipeline leader process (pid: %d. pgid: %d).\n", getpid(), getpgid(getpid()));
        int num_cmds = how_many_commands(pline_cpy);    // Find the number of commands in pipeline.
        debug("num_cmds: %d\n", num_cmds);
        
        char ***cmds_array;
        create_cmds_array(pline, &cmds_array);
        
        int ret;
        if ((ret = setpgid(getpid(), getpid())) == -1)     // Set the pipeline leaders's pgid to the pipeline leader's pid.
        {
            debug("error with setpid().\n");
            debug("%s\n", strerror(errno));
        }
        debug("Pipeline leader process has changed its pgid (pid: %d. pgid: %d).\n", getpid(), getpgid(getpid()));
        children_reaped = num_cmds;     // 'children_reaped' is a counter used by the pipeline leader process to keep track of how many of its children were reaped.
        debug("children_reaped initial value: %d\n", children_reaped);

        execute_pipe(&cmds_array, input_file, output_file, num_cmds, pline_cpy);   // Execute the pipeline. The pipeline leader process will fork a child for each command in the pipeline. Each child will execute a command.
        free_cmds_array(pline_cpy, &cmds_array);
        debug("The pipeline leader (pid: %d. pgid: %d) will exit 0.\n", getpid(), getpgid(getpid()));
        _exit(0);   // Pipeline leader process terminates normally.
    }

    // Parent process.
    debug("Entered Main mush process (pid: %d. pgid: %d).\n", getpid(), getpgid(getpid()));
    job->pid = pid;     // Set job->pid to the pid returned from fork(). It is the pipeline leader's PID.
    jobs_table[num_jobs++] = job; // Add job node to jobs table
    job->status = RUNNING;  // Set status to running

    sigprocmask(SIG_SETMASK, &prev_one, NULL); // The main mush program unmask SIGCHLD signals
    return job->jobid;      // Return the jobid assigned of the job executing the pipeline.
}

/**
 * @brief  Wait for a job to terminate.
 * @details  This function is used to wait for the job with a specified job ID
 * to terminate.  A job has terminated when it has entered the COMPLETED, ABORTED,
 * or CANCELED state.
 *
 * @param  jobid  The job ID of the job to wait for.
 * @return  the exit status of the job leader, as returned by waitpid(),
 * or -1 if any error occurs that makes it impossible to wait for the specified job.
 */
int jobs_wait(int jobid) {
    // TO BE IMPLEMENTED
    // sigset_t mask_one, empty;
    // Signal(SIGCHLD, sigchld_handler); // Install a SIGCHLD handler. 
    // sigemptyset(&empty);
    // sigemptyset(&mask_one);
    // sigaddset(&mask_one, SIGCHLD);  // Create SIGCHLD mask
    sigprocmask(SIG_BLOCK, &mask_one, NULL);  // Block SIGCHLD signal.

    debug("Inside jobs_wait(). Is jobid %d completed?\n", jobid);
    JOB *job = find_job_by_jobid(jobid);

    if (job->status == COMPLETED)
    {
        debug("Job (jobid: %d) has already terminated. Status: COMPLETED.\n", jobid);
        return job->exit_status;
    }
    else if (job->status == ABORTED)
    {
        debug("Job (jobid: %d) has already terminated. Status: ABORTED.\n", jobid);
        return job->exit_status;
    }
    else if (job->status == CANCELED)
    {
        debug("Job (jobid: %d) has already terminated. Status: CANCELED.\n", jobid);
        return job->exit_status;
    }

    debug("Waiting for job (jobid: %d) to terminate...\n", jobid);
    
    // pid = 0;    
    // while (!pid)    // wait for SIGCHLD to be received
    // {
    debug("Main mush process is suspended until it receives a SIGCHLD signal.\n");
    sigsuspend(&prev_one);  // Temporarily unblock SIGCHLD signal, pause until received SIGCHLD signal, then resets the Block vector bit (which blocks SIGCHLD).
    // }
    sigprocmask(SIG_UNBLOCK, &mask_one, NULL);      // Unblock SIGCHLD signal after sigsuspend returns.

    debug("job->status after sigsuspend: %d\n", job->status);
    if (job->status == COMPLETED)
    {
        debug("Job (jobid: %d) has now terminated. Status: COMPLETED.\n", jobid);
        return job->exit_status;
    }
    else if (job->status == ABORTED)
    {
        debug("Job (jobid: %d) has now terminated. Status: ABORTED.\n", jobid);
        return job->exit_status;
    }
    else if (job->status == CANCELED)
    {
        debug("Job (jobid: %d) has now terminated. Status: CANCELED.\n", jobid);
        return job->exit_status;
    }
    
    debug("Returning -1\n");
    return -1;

    // abort();
}

/**
 * @brief  Poll to find out if a job has terminated.
 * @details  This function is used to poll whether the job with the specified ID
 * has terminated.  This is similar to jobs_wait(), except that this function returns
 * immediately without waiting if the job has not yet terminated.
 *
 * @param  jobid  The job ID of the job to wait for.
 * @return  the exit status of the job leader, as returned by waitpid(), if the job
 * has terminated, or -1 if the job has not yet terminated or if any other error occurs.
 */
int jobs_poll(int jobid) {
    // TO BE IMPLEMENTED
    debug("Inside jobs_poll(). Polling for job with jobid %d.\n", jobid);
    JOB *job = find_job_by_jobid(jobid);    // Find the job by it jobid.

    // If the job's status is COMPLETED, ABORTED, or CANCELED, then the job has terminated. Return its exit status.
    if (job->status == COMPLETED)
    {
        debug("Job (jobid: %d) has already terminated. Status: COMPLETED.\n", jobid);
        return job->exit_status;
    }
    else if (job->status == ABORTED)
    {
        debug("Job (jobid: %d) has already terminated. Status: ABORTED.\n", jobid);
        return job->exit_status;
    }
    else if (job->status == CANCELED)
    {
        debug("Job (jobid: %d) has already terminated. Status: CANCELED.\n", jobid);
        return job->exit_status;
    }
    else
    {
        debug("Job (jobid: %d) has NOT terminated. Returning -1.\n", jobid);
        return -1;
    }

    // abort();
}

/**
 * @brief  Expunge a terminated job from the jobs table.
 * @details  This function is used to expunge (remove) a job that has terminated from
 * the jobs table, so that space in the table can be used to start some new job.
 * In order to be expunged, a job must have terminated; if an attempt is made to expunge
 * a job that has not yet terminated, it is an error.  Any resources (exit status,
 * open pipes, captured output, etc.) that were being used by the job are finalized
 * and/or freed and will no longer be available.
 *
 * @param  jobid  The job ID of the job to expunge.
 * @return  0 if the job was successfully expunged, -1 if the job could not be expunged.
 */
int jobs_expunge(int jobid) {
    // TO BE IMPLEMENTED
    JOB *job = find_job_by_jobid(jobid);    // Find job by its jobid.
    debug("job->status: %d\n", job->status);
    // if (job->status == COMPLETED)
    // {
    //     debug("Your job has termianted.\n");
    // }
    if ((job->status != COMPLETED) && (job->status != ABORTED) && (job->status != CANCELED))    // If job has not terminated, return -1.
    {
        debug("The job you are trying to expunge has not terminated. Returning -1.\n");
        return -1; 
    }
    else
    {   
        debug("Your job has termianted.\n");
        // free_pipeline(job->pipeline);   // Free the pipeline.
        int i;  // jobs_table counter
        for (i = 0; jobs_table[i]->jobid != jobid; i++)
            ;
        debug("job's index in jobs_table: %d\n", i);
        free_pipeline(jobs_table[i]->pipeline);     // Free the job node's pipeline.
        free(jobs_table[i]);    // Free the JOB node at the i-th index of jobs_table.
        jobs_table[i] = NULL;   // No dangling pointer.
        num_jobs--;             // Decrement number of jobs in jobs_rable.
        global_jobid--;         // Decrement global_jobid so it can be reused for another job.
    }


    // abort();
    return 0;

}

/**
 * @brief  Attempt to cancel a job.
 * @details  This function is used to attempt to cancel a running job.
 * In order to be canceled, the job must not yet have terminated and there
 * must not have been any previous attempt to cancel the job.
 * Cancellation is attempted by sending SIGKILL to the process group associated
 * with the job.  Cancellation becomes successful, and the job actually enters the canceled
 * state, at such subsequent time as the job leader terminates as a result of SIGKILL.
 * If after attempting cancellation, the job leader terminates other than as a result
 * of SIGKILL, then cancellation is not successful and the state of the job is either
 * COMPLETED or ABORTED, depending on how the job leader terminated.
 *
 * @param  jobid  The job ID of the job to cancel.
 * @return  0 if cancellation was successfully initiated, -1 if the job was already
 * terminated, a previous attempt had been made to cancel the job, or any other
 * error occurred.
 */
int jobs_cancel(int jobid) {
    // TO BE IMPLEMENTED
    JOB *job = find_job_by_jobid(jobid);        // Find job by its jobid.
    debug("job->status: %d\n", job->status);    

    if ((job->status == COMPLETED) || (job->status == ABORTED) || (job->status == CANCELED))    // If job terminated, return -1.
    {
        debug("The job you are trying to cancel has already terminated. Returning -1.\n");
        return -1; 
    }
    // abort();
    debug("The job you are trying to cancel has not terminated yet. Sending SIGKILL to job->pid now.\n");
    kill(job->pid, SIGKILL);
    debug("SIGKILL has been sent to %d.\n", job->pid);

    job->status = CANCELED;
    job->exit_status = 9;

    return 0;

}

/**
 * @brief  Get the captured output of a job.
 * @details  This function is used to retrieve output that was captured from a job
 * that has terminated, but that has not yet been expunged.  Output is captured for a job
 * when the "capture_output" flag is set for its pipeline.
 *
 * @param  jobid  The job ID of the job for which captured output is to be retrieved.
 * @return  The captured output, if the job has terminated and there is captured
 * output available, otherwise NULL.
 */
char *jobs_get_output(int jobid) {
    // TO BE IMPLEMENTED
    debug("Inside jobs_get_output()\n");
    return NULL;
    // abort();
}

/**
 * @brief  Pause waiting for a signal indicating a potential job status change.
 * @details  When this function is called it blocks until some signal has been
 * received, at which point the function returns.  It is used to wait for a
 * potential job status change without consuming excessive amounts of CPU time.
 *
 * @return -1 if any error occurred, 0 otherwise.
 */
int jobs_pause(void) {
    // TO BE IMPLEMENTED
    sigprocmask(SIG_BLOCK, &mask_one, NULL);  // Block SIGCHLD signal.
    debug("Inside jobs_pause().\n");

    debug("Main mush process is suspended until it receives a SIGCHLD signal.\n");
    sigsuspend(&prev_one);  // Temporarily unblock SIGCHLD signal, pause until received SIGCHLD signal, then resets the Block vector bit (which blocks SIGCHLD).
    sigprocmask(SIG_UNBLOCK, &mask_one, NULL);      // Unblock SIGCHLD signal after sigsuspend returns.
    debug("jobs_pause() is returning 0.\n");
    return 0;
    // abort();
}

/**
 * @brief Finds the job in the jobs table with the specified pid.
 * @return JOB pointer associated with 'pid'. If job with 'pid' could not be found,
 * NULL is returned.
 */
JOB *find_job(pid_t pid)
{
    int i;
    for (i = 0; i < MAX_JOBS; i++)
    {
        if (jobs_table[i]->pid == pid)
        {
            debug("Job with pid %d found.\n", pid);
            return jobs_table[i];
        }
    }
    debug("Job with pid %d could not be found.\n", pid);
    return NULL;
}

/**
 * @brief Finds the job in the jobs table with the specified jobid.
 * @return JOB pointer associated with 'jobid'. If job with 'jobid' could not be found,
 * NULL is returned.
 */
JOB *find_job_by_jobid(int jobid)
{
    int i;
    for (i = 0; i < MAX_JOBS; i++)
    {
        if (jobs_table[i]->jobid == jobid)
        {
            debug("Job with jobid %d found.\n", jobid);
            return jobs_table[i];
        }
    }
    debug("Job with jobid %d could not be found.\n", jobid);
    return NULL;
}


/**
 * @brief Calculates the number of COMMANDs in a pipeline.
 * @return The number of commands in a pipeline. -1 if pipeline is NULL
 */
int how_many_commands(PIPELINE *pline)
{
    if (pline == NULL)
    {
        debug("pline is NULL, returning -1.\n");
        return -1;
    }
    COMMAND *curr_command = pline->commands;
    int num_commands = 0;
    while (curr_command)
    {
        num_commands++;
        curr_command = curr_command->next;
    }
    return num_commands;
}

/**
 * @brief Calculates the number of arguments in a command.
 * @return The number of args in a command. -1 if command is NULL
 */
int how_many_args(COMMAND *cmd)
{
    if (cmd == NULL)
    {
        debug("cmd is NULL, returning -1.\n");
        return -1;
    }
    ARG *curr_arg = cmd->args;
    int num_args = 0;
    while (curr_arg)
    {
        num_args++;
        curr_arg = curr_arg->next;
    }
    return num_args;
}

void execute_pipe(char ****cmds_array, char *input_file, char *output_file, int num_cmds, PIPELINE *pline)
{
    int p[2];   // Array of two ints to hold the file descriptors, where p[0] is for the read side of the pipe. And p[1] is for the write side.
    int fd_in = 0;  // fd_in will be used to save the fd of the read side of the current command. Will be redirected as standard input for the next command.
    pid_t child_pid;

    char ***cmd = *cmds_array;
    char ***first_cmd = *cmds_array;    // A pointer to the first string array in cmds_array. Used in a conditional expression to see if the current command is the first command in the pipeline.

    children_reaped = num_cmds;
    while (*cmd != NULL)    // Iterate through each command in the array.
    {
        // printf("Before pipe     p[0]: %d p[1]: %d\n", p[0], p[1]);
        pipe(p);    // Create a pipe. This will populate the array of file descriptors.
        // printf("After pipe      p[0]: %d p[1]: %d\n", p[0], p[1]);
        if ((child_pid = fork()) == -1)   // If fork() error, return failure.
        {
            _exit(EXIT_FAILURE);
        }
        else if (child_pid == 0)  // Inside the child process,
        {
            debug("Inside child process (pid: %d. pgid: %d).\n", getpid(), getpgid(getpid()));
            debug("cmd: %p\n", cmd);
            debug("first_cmd: %p\n", first_cmd);
            dup2(fd_in, STDIN_FILENO);                  // Redirect the stdin for this child process from the read side of the pipe of the last command.
            if ((cmd == first_cmd) && input_file)       // If this child process is executing the first command in the pipeline and input_file is defined,
            {
                debug("input_file '%s' is not null.\n", input_file);
                int in = open(input_file, O_RDONLY);            // Open 'input_file',
                free(input_file);                               // Free the memory allocated in 'input_file'
                dup2(in, STDIN_FILENO);                         // Redirect the stdin for this child process from the read side of the pipe of the last command.
                close(in);                                      // Close the file descriptor.
            }
            else
                dup2(fd_in, STDIN_FILENO);  // Redirect the stdin for this child process from the read side of the pipe of the last command.
            
            if (*(cmd+1) != NULL)   // If the command is not the last command in the pipeline,
            {
                dup2(p[1], STDOUT_FILENO);  // Redirect the stdout of this child process to the write end of pipe.
            }
            if (*(cmd+1) == NULL)   // If the command is the last command in the pipeline
            {
                if (output_file)
                {
                    debug("output_file '%s' is not null.\n", output_file);
                    int out = open(output_file, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                    free(output_file);          // Free the memory allocated in 'output_file'
                    dup2(out, STDOUT_FILENO);   // Redirect the stdout of the last child process to the output file.
                    close(out);                 // Close the file descriptor.
                }
            }
            close(p[0]);                        // Close the read side of the pipe.
            debug("executing %s\n", (*cmd)[0]);
            int ret;                                    // Return value.
            if ((ret = execvp((*cmd)[0], (*cmd))) == -1) // Execute the command.
            {
                fprintf(stderr, "execvp failed to run %s.\n", (*cmd)[0]);
                kill(getppid(), SIGABRT);       // Child sends SIGABRT to its pipeline leader process.
                abort();                        // Child aborts itself.
                _exit(ret);
            }            
            _exit(EXIT_FAILURE);                // If execvp fails somehow, return this child process will return EXIT_FAILURE.
        }
        else 
        {
            close(p[1]);    // Close the write end of pipe.
            fd_in = p[0];   // Save the fd that points to the read side of the pipe for the next command.
            cmd++;  // Increment cmd to move to point to the next command in the array of commands.
        }   
    }
    while (children_reaped-- > 0)    // While there are children to be reaped, pipeline leader will suspend for SIGCHLD to be received.
    {
        debug("Children_reaped value: %d\n", children_reaped);
        debug("Pipeline leader process is suspended until it receives a SIGCHLD signal.\n");
        sigsuspend(&prev_one);  // Temporarily unblock SIGCHLD signal, pause until received SIGCHLD signal, then resets the Block vector bit (which blocks SIGCHLD).
    }
    sigprocmask(SIG_UNBLOCK, &mask_one, NULL);  // When sigsuspend returns, it resets the main process's signal mask to the original value (which was blocking SIGCHLD). So we want to explicitly unblock SIGCHLD signals.


    debug("The pipeline leader process (pid: %d. pgid: %d) is freeing dynamically allocated memory...\n", getpid(), getpgid(getpid()));

    free_cmds_array(pline, cmds_array);

    debug("The pipeline leader (pid: %d. pgid: %d) will exit 0.\n", getpid(), getpgid(getpid()));
    return;
}

void create_cmds_array(PIPELINE *pline, char ****cmd)
{
    debug("create_cmds_array() called.\n");
    int num_cmds = how_many_commands(pline);    // Find the number of commands in pipeline.
    *cmd = malloc((num_cmds+1) * sizeof(char **));  // Allocate memory for num_cmds+1 string arrays.

    int i, j;                                   // Counters for loops.
    int num_args = 0;                           // Number of arguments in a command.
    long num_expr_val = 0;
    (void) num_expr_val;
    COMMAND *curr_cmd = pline->commands;    // COMMAND pointer we will use to traverse through all COMMAND objects in the pipeline.

    for (i = 0; i < num_cmds; i++)
    {
        num_args = how_many_args(curr_cmd);     // Find the number of args in the command.
        (*cmd)[i] = malloc((num_args+1) * sizeof(char *));  // Allocate memory for (num_args+1) strings.
        
        ARG *curr_arg = curr_cmd->args;         // ARG pointer we will use it to traverse through all ARG objects in current command.
        for (j = 0; j < num_args; j++)          // For each argument in the command...
        {
            debug("value: %s\n",curr_arg->expr->members.value);
            if (curr_arg->expr->class == NUM_EXPR_CLASS)
            {
                store_get_int(curr_arg->expr->members.value, &num_expr_val);
                int length = snprintf(NULL, 0, "%ld", num_expr_val);    // NULL and 0 as the first two args will cause snprintf to return the size needed to store 'num_expr_val'.
                debug("length: %d\n", length);
                (*cmd)[i][j] = malloc(sizeof(char) * (length+1));       // Allocate 'length + 1' bytes, the last byte is '\0'.
                snprintf((*cmd)[i][j], length+1 , "%ld", num_expr_val);
            }
            else
            {
                (*cmd)[i][j] = strdup(curr_arg->expr->members.value);
            }
            curr_arg = curr_arg->next;          // Iterate to the next argument in the command.
        }
        (*cmd)[i][j] = NULL;                    // Assign NULL as the last element of the string array.
        curr_cmd = curr_cmd->next;              // Iterate to the next command in the pipeline.
    }
    (*cmd)[i] = NULL;                           // The last element of 'cmds_array' is set to NULL, as required by execvp.


    // for (i = 0; i < num_cmds; i++)
    // {
    //     for (j = 0; j < num_args+1; j++)
    //     {
    //         printf("%s, ", (*cmd)[i][j]);
    //     }
    //     printf("\n");
    // }
    return;
}

void free_cmds_array(PIPELINE *pline, char ****cmd)
{
    debug("free_cmds_array() called.\n");
    int num_cmds = how_many_commands(pline);    // Find the number of commands in pipeline.
    int num_args;

    COMMAND *curr_cmd = pline->commands;    // COMMAND pointer we will use to traverse through all COMMAND objects in the pipeline.
    int i,j;
    for (i=0; i < num_cmds; i++)                // For each command...
    {
        num_args = how_many_args(curr_cmd);     // Find the number of strings in the string array.
        for (j = 0; j < num_args; j++)          // For each string in the string array...
        {
            free((*cmd)[i][j]);             // Free its dynamically allocated memory.
            (*cmd)[i][j] = NULL;            // No dangling pointer.
        }
        free((*cmd)[i]);                    // Free the dynamically allocated memory for the string array itself.
        curr_cmd = curr_cmd->next;              // Iterate to the next command in the pipeline.
    }
    return;
}

/**
 * @brief Extracts the executable filename and its arguments from a pipeline.
 * @param 'command' is the list of commands.
 * 'num_args' is the number of args in 'command'.
 * 'args_arr' is a pointer to a strings array (a triple pointer) so it can be modified
 * via pass by reference.
 * 'executable_filename' is a pointer to a string (a double pointer) so it can be modified
 * via pass by reference.
 * 
 * @return 0 if successful extraction. -1 if error.
 */
// int *extract_args(COMMAND *command, int num_args, char **cmd_args, char *executable_filename)
// {
//     debug("extract_args() called\n");
//     // ARG *curr_arg = command->args;     // ARG pointer to traverse through the list of commands.

//     // Populate a static array of strings representing all the args.
//     char *args[num_args];  // Array of size n.
//     strcpy(executable_filename, args[0]); // Assign args[0] to the value of executable_filename
    
//     int i;
//     for (i = 1; i < num_args; i++)
//     {
//         strcpy(cmd_args[i], command->args[i].expr->members.value); // Copy the values from the command object into each array element.
//     }

//     // // Print the args.
//     // for (i = 0; i < num_args; i++)
//     // {
//     //     debug("args[i]: %s\n", args[i]);
//     // }

//     // // Create a new array without args[0], which is the executable filename.
//     // char *my_cmd_args[num_args-1];
//     // char *my_executable_name = args[0];    // Save the executable filename.

//     // for (i = 1; i < num_args; i++)
//     // {
//     //     strcpy(cmd_args[i-1], args[i]); // Copy everything but args[0]. This new array will be passed to execvp().
//     // }

//     // strcpy(*executable_name, my_executable_name); // Set the executable filename
//     // // *cmd_args = my_cmd_args;   // And the array of command args.

//     return 0;
// }