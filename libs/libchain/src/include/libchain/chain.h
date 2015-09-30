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

/** @brief Execution context */
typedef struct _context_t {
    /** @brief Pointer to the most recently started but not finished task */
    task_func_t *task;
    /** @brief Logical time, ticks at task boundaries */
    chain_time_t time;
    /** @brief Index that selects the current buffer for the self-channel */
    unsigned self_chan_idx;

    // TODO: move this to top, just feels cleaner
    struct _context_t *next_ctx;
} context_t;

extern context_t * volatile curctx;

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


void transition_to(task_func_t *next_task);
void *chan_in(int count, ...);

#define CHANNEL(src, dest, type) __fram type _ch_ ## src ## _ ## dest
#define SELF_CHANNEL(task, type) __fram type _ch_ ## task ## [2]

#define CH(src, dest) (&_ch_ ## src ## _ ## dest)
#define SELF_CH(task) (&_ch_ ## task ## [curctx->self_chan_idx])

/** @brief Internal macro for counting channel arguments to a variadic macro */
#define NUM_CHANS(...) (sizeof((void *[]){__VA_ARGS__})/sizeof(void *))

/** @brief Read the named field from the given channels
 *  @details This macro retuns a pointer to value of the requested field.
 */
#define CHAN_IN1(field, chan0) (&(chan0->field.value))

/** @brief Read the most recently modified value from one of the given channels
 *  @details This macro retuns a pointer to the most recently modified value
 *           of the requested field.
 */
#define CHAN_IN(field, chan0, ...) \
    ((__typeof__(chan0->field.value)*) \
      ((unsigned char *)chan_in(1 + NUM_CHANS(__VA_ARGS__), chan0, __VA_ARGS__) + \
      offsetof(__typeof__(chan0->field), value)))

/** @brief Write a value into a channel
 *  @details NOTE: must take value by value (not by ref, which would be
 *           consistent with CHAN_IN), because must support constants.
 *
 *  TODO: eliminate dest field through compiler support
 *  TODO: multicast: the chan will become a list: var arg
 *  TODO: does the compiler optimize what is effectively
 *        (&chan_buf)->value into chan_buf.value, for non-self channels?
 */
#define CHAN_OUT(field, val, chan) \
    do { \
        chan->field.value = val; \
        chan->field.meta.timestamp = curctx->time; \
    } while(0)

#endif // CHAIN_H
