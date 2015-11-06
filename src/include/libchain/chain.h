#ifndef CHAIN_H
#define CHAIN_H

#include <stddef.h>
#include <stdint.h>

#ifndef LIBCHAIN_ENABLE_DIAGNOSTICS
#define LIBCHAIN_PRINTF(...)
#else
#include <stdio.h>
#define LIBCHAIN_PRINTF printf
#endif

#include <libmsp/mem.h>

#define CHAN_NAME_SIZE 32

typedef void (task_func_t)(void);
typedef unsigned chain_time_t;
typedef uint32_t task_mask_t;
typedef unsigned task_idx_t;

typedef struct {
    task_func_t *func;
    task_mask_t mask;
    task_idx_t idx;
} task_t;

typedef struct _chan_diag_t {
    char source_name[CHAN_NAME_SIZE];
    char dest_name[CHAN_NAME_SIZE];
} chan_diag_t;

typedef struct _chan_meta_t {
    chan_diag_t diag;
} chan_meta_t;

typedef struct _chan_field_meta_t {
    chain_time_t timestamp;
} chan_field_meta_t;

/** @brief Declare a value transmittable over a channel
 *  @param  type    Type of the field value
 *  @param  name    Name of the field, include [] suffix to declare an array
 *  @details Metadata field must be the first, so that these
 *           fields can be upcast to a generic type.
 *  NOTE: To make compiler error messages more useful, instead of making
 *        the struct anonymous, we can name it with '_chan_field_ ## name'.
 *        But, this imposes the restriction that field names should be unique
 *        across unrelated channels. Adding the channel name to each CHAN_FIELD
 *        macro is too verbose, so compromising on error msgs.
 */
#define CHAN_FIELD(type, name) \
    struct { \
        chan_field_meta_t meta; \
        type value; \
    } name

#define CHAN_FIELD_ARRAY(type, name, size) \
    struct { \
        chan_field_meta_t meta; \
        type value; \
    } name[size]

/** @brief Execution context */
typedef struct _context_t {
    /** @brief Pointer to the most recently started but not finished task */
    const task_t *task;

    /** @brief Logical time, ticks at task boundaries */
    chain_time_t time;

    /** @brief Bitmask that maps task index to its self-channel buffer index */
    task_mask_t self_chan_idx;

    // TODO: move this to top, just feels cleaner
    struct _context_t *next_ctx;
} context_t;

extern context_t * volatile curctx;

/** @brief Internal macro for constructing name of task symbol */
#define TASK_SYM_NAME(func) _task_ ## func

/** @brief Declare a task
 *
 *  @param idx      Global task index, zero-based
 *  @param func     Pointer to task function
 *
 *   TODO: These do not need to be stored in memory, could be resolved
 *         into literal values and encoded directly in the code instructions.
 *         But, it's not obvious how to implement that with macros (would
 *         need "define inside a define"), so for now create symbols.
 *         The compiler should actually optimize these away.
 *
 *   TODO: Consider creating a table in form of struct and store
 *         for each task: mask, index, name. That way we can
 *         have access to task name for diagnostic output.
 */
#define TASK(idx, func) \
    void func(); \
    const task_t TASK_SYM_NAME(func) = { func, (1UL << idx), idx }; \

#define TASK_REF(func) &TASK_SYM_NAME(func)

/** @brief Function called on every reboot
 *  @details This function usually initializes hardware, such as GPIO
 *           direction. The application must define this function.
 */
extern void init();

/** @brief First task to run when the application starts
 *  @details Symbol is defined by the ENTRY_TASK macro.
 *           This is not wrapped into a delaration macro, because applications
 *           are not meant to declare tasks -- internal only.
 *
 *  TODO: An alternative would be to have a macro that defines
 *        the curtask symbol and initializes it to the entry task. The
 *        application would be required to have a definition using that macro.
 *        An advantage is that the names of the tasks in the application are
 *        not constrained, and the whole thing is less magical when reading app
 *        code, but slightly more verbose.
 */
extern const task_t TASK_SYM_NAME(_entry_task);

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
#define ENTRY_TASK(task) \
    TASK(0, _entry_task) \
    void _entry_task() { TRANSITION_TO(task); }

/** @brief Init function prototype
 *  @details We rely on the special name of this symbol to initialize the
 *           current task pointer. The entry function is defined in the user
 *           application through a macro provided by our header.
 */
void _init();

/** @brief Declare the function to be called on each boot
 *  @details The same notes apply as for entry task.
 */
#define INIT_FUNC(func) void _init() { func(); }

void transition_to(const task_t *task);
void *chan_in(const char *field_name, int count, ...);

// TODO: make a meta field and put diag inside it
// TODO: include diag field only when diagnostics are enabled
#define CH_TYPE(src, dest, type) \
    struct _ch_type_ ## src ## _ ## dest ## _ ## type { \
        chan_meta_t meta; \
        struct type data; \
    }

#define CHANNEL(src, dest, type) \
    __nv CH_TYPE(src, dest, type) _ch_ ## src ## _ ## dest = \
        { { { #src, #dest } } }

#define SELF_CHANNEL(task, type) \
    __nv CH_TYPE(task, task, type) _ch_ ## task[2] = \
        { { { { #task, #task } } }, { { { #task, #task } } } }

/** @brief Declare a channel for passing arguments to a callable task
 *  @details Callers would output values into this channels before
 *           transitioning to the callable task.
 *
 *  TODO: should this be associated with the callee task? i.e. the
 *        'callee' argument would be a task name? The concern is
 *        that a callable task might need to be a special type
 *        of a task composed of multiple other tasks (a 'hyper-task').
 * */
#define CALL_CHANNEL(callee, type) \
    __nv CH_TYPE(caller, callee, type) _ch_call_ ## callee = \
        { { { #callee, "call:"#callee } } }
#define RET_CHANNEL(callee, type) \
    __nv CH_TYPE(caller, callee, type) _ch_ret_ ## callee = \
        { { { #callee, "ret:"#callee } } }

/** @brief Delcare a channel for receiving results from a callable task
 *  @details Callable tasks output values into this channel, and a
 *           result-processing task would collect the result. The
 *           result-processing task does not need to be dedicated
 *           to this purpose, but the results need to be collected
 *           before the next call to the same task is made.
 */
#define RETURN_CHANNEL(callee, type) \
    __nv CH_TYPE(caller, callee, type) _ch_ret_ ## callee = \
        { { { #callee, "ret:"#callee } } }

/** @brief Declare a multicast channel: one source many destinations
 *  @params name    short name used to refer to the channels from source and destinations
 *  @details Conceptually, the channel is between the specified source and
 *           destinations only. The arbitrary name exists only to simplify referring
 *           to the channel: to avoid having to list all sources and destinations
 *           every time. However, access control is not currently enforced.
 *           Declarations of sources and destinations is necessary for perform
 *           compile-time checks planned for the future.
 */
#define MULTICAST_CHANNEL(type, name, src, dest, ...) \
    __nv CH_TYPE(src, name, type) _ch_mc_ ## src ## _ ## name = \
        { { { #src, "mc:" #name } } }

#define CH(src, dest) (&_ch_ ## src ## _ ## dest)
// TODO: compare right-shift vs. branch implementation for this:
#define SELF_IN_CH(tsk)  (&_ch_ ## tsk[(curctx->self_chan_idx & curctx->task->mask) ? 0 : 1])
#define SELF_OUT_CH(tsk) (&_ch_ ## tsk[(curctx->self_chan_idx & curctx->task->mask) ? 1 : 0])

/** @brief Reference to a channel used to pass "arguments" to a callable task
 *  @details Each callable task that takes arguments would have one of these.
 * */
#define CALL_CH(callee)  (&_ch_call_ ## callee)

/** @brief Reference to a channel used to receive results from a callable task */
#define RET_CH(callee)  (&_ch_ret_ ## callee)

/** @brief Multicast channel reference
 *  @details Require the source for consistency with other channel types.
 *           In IN, require the one destination that's doing the read for consistency.
 *           In OUT, require the list of destinations for consistency and for
 *           code legibility.
 *           Require name only because of implementation constraints.
 *
 *           NOTE: The name is not pure syntactic sugar, because currently
 *           refering to the multicast channel from the destination by source
 *           alone is not sufficient: there may be more than one channels that
 *           have overlapping destination sets. TODO: disallow this overlap?
 *           The curent implementation resolves this ambiguity using the name.
 *
 *           A separate and more immediate reason for the name is purely
 *           implementation: if if we disallow the overlap ambiguity, this
 *           macro needs to resolve among all the channels from the source
 *           (incl. non-overlapping) -- either we use a name, or we force the
 *           each destination to specify the complete destination list. The
 *           latter is not good since eventually we want the type of the
 *           channel (task-to-task/self/multicast) be transparent to the
 *           application. type nature be transparent , or we use a name.
 */
#define MC_IN_CH(name, src, dest)         (&_ch_mc_ ## src ## _ ## name)
#define MC_OUT_CH(name, src, dest, ...)   (&_ch_mc_ ## src ## _ ## name)

/** @brief Internal macro for counting channel arguments to a variadic macro */
#define NUM_CHANS(...) (sizeof((void *[]){__VA_ARGS__})/sizeof(void *))

/** @brief Read the most recently modified value from one of the given channels
 *  @details This macro retuns a pointer to the most recently modified value
 *           of the requested field.
 *
 *  NOTE: We pass the channel pointer instead of the field pointer
 *        to have access to diagnostic info. The logic in chain_in
 *        only strictly needs the fields, not the channels.
 */
// #define CHAN_IN1(field, chan0) (&(chan0->data.field.value))
#define CHAN_IN1(field, chan0) \
    ((__typeof__(chan0->data.field.value)*) \
      ((unsigned char *)chan_in(#field, 1, \
          chan0, offsetof(__typeof__(chan0->data), field)) + \
      offsetof(__typeof__(chan0->data.field), value)))
#define CHAN_IN2(field, chan0, chan1) \
    ((__typeof__(chan0->data.field.value)*) \
      ((unsigned char *)chan_in(#field, 2, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field)) + \
      offsetof(__typeof__(chan0->data.field), value)))
#define CHAN_IN3(field, chan0, chan1, chan2) \
    ((__typeof__(chan0->data.field.value)*) \
      ((unsigned char *)chan_in(#field, 3, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field), \
          chan2, offsetof(__typeof__(chan2->data), field)) + \
      offsetof(__typeof__(chan0->data.field), value)))
#define CHAN_IN4(field, chan0, chan1, chan2, chan3) \
    ((__typeof__(chan0->data.field.value)*) \
      ((unsigned char *)chan_in(#field, 4, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field), \
          chan2, offsetof(__typeof__(chan2->data), field), \
          chan3, offsetof(__typeof__(chan3->data), field)) + \
      offsetof(__typeof__(chan0->data.field), value)))
#define CHAN_IN5(field, chan0, chan1, chan2, chan3, chan4) \
    ((__typeof__(chan0->data.field.value)*) \
      ((unsigned char *)chan_in(#field, 5, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field), \
          chan2, offsetof(__typeof__(chan2->data), field), \
          chan3, offsetof(__typeof__(chan3->data), field), \
          chan4, offsetof(__typeof__(chan4->data), field)) + \
      offsetof(__typeof__(chan0->data.field), value)))

/** @brief Write a value into a channel
 *  @details NOTE: must take value by value (not by ref, which would be
 *           consistent with CHAN_IN), because must support constants.
 *
 *  NOTE: It is possible to write this as a function to be consistent
 *        with chan_in, but it is more complicated because the type of
 *        value is not known, so we would have to pass a field offset
 *        and size and do a memcpy.
 *
 *  TODO: eliminate dest field through compiler support
 *  TODO: multicast: the chan will become a list: var arg
 *  TODO: does the compiler optimize what is effectively
 *        (&chan_buf)->value into chan_buf.value, for non-self channels?
 *  TODO: printing the value without knowing the type is tricky,
 *        perhaps, do got the memcpy route described above and
 *        print in hex? Or, provide a format string in channel
 *        field definition, or even a whole render function.
 */
#define CHAN_OUT(field, val, chan) \
    do { \
        chan->data.field.value = val; \
        chan->data.field.meta.timestamp = curctx->time; \
        LIBCHAIN_PRINTF("[%u][0x%x & 0x%x] out: '%s': %s -> %s\r\n", \
               curctx->time, \
               (uint16_t)curctx->self_chan_idx, \
               (uint16_t)curctx->task->mask, \
               #field, chan->meta.diag.source_name, chan->meta.diag.dest_name); \
    } while(0)

/** @brief Transfer control to the given task
 *  @param task     Name of the task function
 *  */
#define TRANSITION_TO(task) transition_to(TASK_REF(task))

#endif // CHAIN_H
