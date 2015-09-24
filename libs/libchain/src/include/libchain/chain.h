#ifndef CHAIN_H
#define CHAIN_H

/* Variable placement in nonvolatile memory; linker puts this in right place */
#define __fram __attribute__((section(".fram_vars")))

typedef void (task_func_t)(void);
typedef unsigned chain_time_t;

typedef struct {
    unsigned value;
    chain_time_t timestamp;
} chan_field_unsigned_t;

extern task_func_t * volatile curtask;
extern chain_time_t volatile curtime;

/** @brief Function called on every reboot
 *  @details This function usually initializes hardware, such as GPIO
 *           direction. The application must define this function.
 */
void init();

/** @brief First task to run when the application starts
 *  @details The application must define this function.
 *
 *  TODO: An alternative would be to have a macro that defines
 *        the curtask symbol and initializes it to the entry task. The
 *        application would be required to have a definition using that macro.
 *        An advantage is that the names of the tasks in the application are
 *        not constrained, and the whole thing is less magical when reading app
 *        code, but slightly more verbose.
 */
void entry_task();

/** @brief Declare the first task of the application
 *  @details This macro defines a function with a special name that is
 *           used to initialize the current task pointer.
 *
 *           This does incur the penalty of an extra task transition, but it
 *           happens only once in application lifetime.
 *
 *           The alternatives are to force the user to define functions
 *           with a special name or to define a task pointer symbol outside
 *           of the library.
 */
#define ENTRY_TASK(task) void _entry_task() { transition_to(task); }

/** @brief Declare the function to be called on each boot
 *  @details The same notes apply as for entry task.
 */
#define INIT_FUNC(func) void _init() { func(); }

/**
 * @brief Transfer control to the given task
 * @details Finalize the current task and jump to the given task.
 *          This function does not return.
 */
static inline void transition_to(task_func_t *next_task)
{
    // reset stack pointer
    // update current task pointer
    // tick logical time
    // jump to next task

    // NOTE: the order of these does not seem to matter, a reboot
    // at any point in this sequence seems to be harmless.
    //
    // NOTE: It might be a bit cleaner to reset the stack and
    // set the current task pointer in the function *prologue* --
    // would need compiler support for this.
    //
    // NOTE: It is harmless to increment the time even if we fail before
    // transitioning to the next task. The reverse, i.e. failure to increment
    // time while having transitioned to the task, would break the
    // semantics of CHAN_IN (aka. sync), which should get the most recently
    // updated value.

    // TODO: handle overflow of timestamp. Some very raw ideas:
    //          * limit the age of values
    //          * a maintainance task that fixes up stored timestamps
    //          * extra bit to mark timestamps as pre/post overflow

    // TODO: re-use the top-of-stack address used in entry point, instead
    //       of hardcoding the address.
    //
    //       Probably need to write a custom entry point in asm, and
    //       use it instead of the C runtime one.
    __asm__ volatile ( // volatile because output operands unused by C
        "mov #0x2400, r1\n"
        "inc %[ctime]\n"
        "mov %[ntask], %[ctask]\n"
        "br %[ntask]\n"
        : [ctask] "=m" (curtask),
          [ctime] "=m" (curtime)
        : [ntask] "r" (next_task)
    );

    // Alternative:
    // task-function prologue:
    //     mov pc, curtask 
    //     mov #0x2400, sp
    //
    // transition_to(next_task):
    //     br next_task
}

#define FIELD_NAME(src, dest, field) _ch_ ## src ## _ ## dest.field

#define CHANNEL(src, dest, type) __fram type _ch_ ## src ## _ ## dest

/** @brief Read a value from one of the given channels into a variable
 *  @details This family of macros assigns the most recently-modified value
 *           of the requested field to the variable.
 */
#define CHAN_IN1(var, field, dest, src0) \
    var = FIELD_NAME(src0, dest, field).value

#define CHAN_IN2(var, field, dest, src0, src1) \
    var = (FIELD_NAME(src0, dest, field).timestamp > FIELD_NAME(src1, dest, field).timestamp) ? \
            FIELD_NAME(src0, dest, field).value : FIELD_NAME(src1, dest, field).value

#define CHAN_IN3(var, field, dest, src0, src1, src2) \
    do { \
        if (FIELD_NAME(src0, dest, field).timestamp >= FIELD_NAME(src1, dest, field).timestamp && \
                FIELD_NAME(src0, dest, field).timestamp >= FIELD_NAME(src2, dest, field).timestamp) \
            var = FIELD_NAME(src0, dest, field).value; \
        else if (FIELD_NAME(src1, dest, field).timestamp >= FIELD_NAME(src0, dest, field).timestamp && \
                FIELD_NAME(src1, dest, field).timestamp >= FIELD_NAME(src2, dest, field).timestamp) \
            var = FIELD_NAME(src1, dest, field).value; \
        else \
            var = FIELD_NAME(src2, dest, field).value; \
    } while(0)

/* A sketch for an alternative approach: requires
 *    (1) an indirection table
 *    (2) referring to tasks by indices
 **/
#if 0
#define CHAN_IN(var, field, dest, src) \
    { \
        task_func_t *srcs = { __VA_ARGS__ }; \
        _chain_time_t t = 0; \
        unsigned latest_i = 0, i; \
        for (i = 0; i < sizeof(srcs) / sizeof(srcs[0]); ++i) \
            if (chan_table(srcs[0], dest).field.timestamp > t) { \
                t = FIELD_NAME(src, dest, field).timestamp; \
                latest_i = i;  \
            } \
        var = srs[latest[i]]
    }
#endif

#define CHAN_OUT(src, dest, field, val) \
    do { \
        FIELD_NAME(src, dest, field).value = val; \
        FIELD_NAME(src, dest, field).timestamp = curtime; \
    } while(0)

#endif // CHAIN_H
