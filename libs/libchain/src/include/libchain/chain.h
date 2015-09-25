#ifndef CHAIN_H
#define CHAIN_H

#include <stddef.h>

/* Variable placement in nonvolatile memory; linker puts this in right place */
#define __fram __attribute__((section(".fram_vars")))

typedef void (task_func_t)(void);
typedef unsigned chain_time_t;

typedef struct {
    chain_time_t timestamp;
} chan_field_meta_t;

/** @brief Declare a value transmittable over a channel
 *  @details Metadata field must be the first, so that these
 *           fields can be upcast to a generic type.
 */
#define CHAN_FIELD(type, name) \
    struct { \
        chan_field_meta_t meta; \
        type value; \
    } name

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

void *chan_in(int count, ...);

#define FIELD_NAME(src, dest, field) _ch_ ## src ## _ ## dest.field

#define CHANNEL(src, dest, type) __fram type _ch_ ## src ## _ ## dest

/** @brief Read a value from one of the given channels into a variable
 *  @details This family of macros assigns the most recently-modified value
 *           of the requested field to the variable.
 *
 *  TODO: eliminate dest field through compiler support
 */
#define CHAN_IN1(field, dest, src0) \
    (&FIELD_NAME(src0, dest, field).value)

#define CHAN_IN2(field, dest, src0, src1) \
    CHAN_IN(field, src0, dest, 2, \
            FIELD_NAME(src0, dest, field), \
            FIELD_NAME(src1, dest, field))

#define CHAN_IN3(field, dest, src0, src1, src2) \
    CHAN_IN(field, src0, dest, 3, \
            FIELD_NAME(src0, dest, field), \
            FIELD_NAME(src1, dest, field), \
            FIELD_NAME(src2, dest, field))

#define CHAN_IN4(field, dest, src0, src1, src2, src3) \
    CHAN_IN(field, src0, dest, 4, \
            FIELD_NAME(src0, dest, field), \
            FIELD_NAME(src1, dest, field), \
            FIELD_NAME(src2, dest, field), \
            FIELD_NAME(src3, dest, field))

/** @brief Internal macro that wraps a call to internal chan_in function
 *  @param src      any of the sources, used only for type info
 */
#define CHAN_IN(field, src, dest, count, ...) \
    ((__typeof__(FIELD_NAME(src, dest, field).value)*) \
     ((unsigned char *)chan_in(count, __VA_ARGS__) + \
      offsetof(__typeof__(FIELD_NAME(src, dest, field)), value)))

/** @brief Write a value into a channel
 *  @details NOTE: must take value by value (not by ref, which would be
 *           consistent with CHAN_IN), because must support constants.
 *  TODO: eliminate dest field through compiler support
 */
#define CHAN_OUT(src, dest, field, val) \
    do { \
        FIELD_NAME(src, dest, field).value = val; \
        FIELD_NAME(src, dest, field).meta.timestamp = curtime; \
    } while(0)

#endif // CHAIN_H