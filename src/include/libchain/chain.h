#ifndef CHAIN_H
#define CHAIN_H

#include <stddef.h>
#include <stdint.h>

#include <libmsp/mem.h>

#include "repeat.h"

#define TASK_NAME_SIZE 32
#define CHAN_NAME_SIZE 32

#define MAX_DIRTY_SELF_FIELDS 4

typedef void (task_func_t)(void);
typedef unsigned chain_time_t;
typedef uint32_t task_mask_t;
typedef uint16_t field_mask_t;
typedef unsigned task_idx_t;

typedef enum {
    CHAN_TYPE_T2T,
    CHAN_TYPE_SELF,
    CHAN_TYPE_MULTICAST,
    CHAN_TYPE_CALL,
    CHAN_TYPE_RETURN,
} chan_type_t;

// TODO: include diag fields only when diagnostics are enabled
typedef struct _chan_diag_t {
    char source_name[CHAN_NAME_SIZE];
    char dest_name[CHAN_NAME_SIZE];
} chan_diag_t;

typedef struct _chan_meta_t {
    chan_type_t type;
    chan_diag_t diag;
} chan_meta_t;

typedef struct _var_meta_t {
    chain_time_t timestamp;
} var_meta_t;

typedef struct _self_field_meta_t {
    // Single word (two bytes) value that contains
    // * bit 0: dirty bit (i.e. swap needed)
    // * bit 1: index of the current var buffer from the double buffer pair
    // * bit 5: index of the next var buffer from the double buffer pair
    // This layout is so that we can swap the bytes to flip between buffers and
    // at the same time (atomically) clear the dirty bit.  The dirty bit must
    // be reset in bit 4 before the next swap.
    unsigned idx_pair;
} self_field_meta_t;

typedef struct {
    task_func_t *func;
    task_mask_t mask;
    task_idx_t idx;

    // Dirty self channel fields are ones to which there had been a
    // chan_out. The out value is "staged" in the alternate buffer of
    // the self-channel double-buffer pair for each field. On transition,
    // the buffer index is flipped for dirty fields.
    self_field_meta_t *dirty_self_fields[MAX_DIRTY_SELF_FIELDS];
    volatile unsigned num_dirty_self_fields;

    volatile chain_time_t last_execute_time; // to execute prologue only once

    char name[TASK_NAME_SIZE];
} task_t;

#define SELF_CHAN_IDX_BIT_DIRTY_CURRENT  0x0001U
#define SELF_CHAN_IDX_BIT_DIRTY_NEXT     0x0100U
#define SELF_CHAN_IDX_BIT_CURRENT        0x0002U
#define SELF_CHAN_IDX_BIT_NEXT           0x0200U

#define VAR_TYPE(type) \
    struct { \
        var_meta_t meta; \
        type value; \
    } \

#define FIELD_TYPE(type) \
    struct { \
        VAR_TYPE(type) var; \
    }

#define SELF_FIELD_TYPE(type) \
    struct { \
        self_field_meta_t meta; \
        VAR_TYPE(type) var[2]; \
    }

#define CH_TYPE(src, dest, type) \
    struct _ch_type_ ## src ## _ ## dest ## _ ## type { \
        chan_meta_t meta; \
        struct type data; \
    }

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
 *
 *  TODO: could CHAN_FIELD be a special case of CHAN_FIELD_ARRARY with size = 1?
 */
#define CHAN_FIELD(type, name)                  FIELD_TYPE(type) name
#define CHAN_FIELD_ARRAY(type, name, size)      FIELD_TYPE(type) name[size]
#define SELF_CHAN_FIELD(type, name)             SELF_FIELD_TYPE(type) name
#define SELF_CHAN_FIELD_ARRAY(type, name, size) SELF_FIELD_TYPE(type) name[size]

/** @brief Execution context */
typedef struct _context_t {
    /** @brief Pointer to the most recently started but not finished task */
    task_t *task;

    /** @brief Logical time, ticks at task boundaries */
    chain_time_t time;

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
    __nv task_t TASK_SYM_NAME(func) = { func, (1UL << idx), idx, {0}, 0, 0, #func }; \

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
extern task_t TASK_SYM_NAME(_entry_task);

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

void task_prologue();
void transition_to(task_t *task);
void *chan_in(const char *field_name, size_t var_size, int count, ...);
void chan_out(const char *field_name, const void *value,
              size_t var_size, int count, ...);

#define FIELD_COUNT_INNER(type) NUM_FIELDS_ ## type
#define FIELD_COUNT(type) FIELD_COUNT_INNER(type)

/** @brief Initializers for the fields in a channel
 *  @details The user defines a FILED_INIT_<chan_msg_type> macro that,
 *           in braces contains comma separated list of initializers
 *           one for each field, in order of the declaration of the fields.
 *           Each initializer is either
 *             * SELF_FIELD_INITIALIZER, or
 *             * SELF_FIELD_ARRAY_INITIALIZER(count) [only count=2^n supported]
 */

#define SELF_FIELD_META_INITIALIZER { (SELF_CHAN_IDX_BIT_NEXT) }
#define SELF_FIELD_INITIALIZER { SELF_FIELD_META_INITIALIZER }

#define SELF_FIELD_ARRAY_INITIALIZER(count) { REPEAT(count, SELF_FIELD_INITIALIZER) }

#define SELF_FIELDS_INITIALIZER_INNER(type) FIELD_INIT_ ## type
#define SELF_FIELDS_INITIALIZER(type) SELF_FIELDS_INITIALIZER_INNER(type)

#define CHANNEL(src, dest, type) \
    __nv CH_TYPE(src, dest, type) _ch_ ## src ## _ ## dest = \
        { { CHAN_TYPE_T2T, { #src, #dest } } }

#define SELF_CHANNEL(task, type) \
    __nv CH_TYPE(task, task, type) _ch_ ## task ## _ ## task = \
        { { CHAN_TYPE_SELF, { #task, #task } }, SELF_FIELDS_INITIALIZER(type) }

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
        { { CHAN_TYPE_CALL, { #callee, "call:"#callee } } }
#define RET_CHANNEL(callee, type) \
    __nv CH_TYPE(caller, callee, type) _ch_ret_ ## callee = \
        { { CHAN_TYPE_RETURN, { #callee, "ret:"#callee } } }

/** @brief Delcare a channel for receiving results from a callable task
 *  @details Callable tasks output values into this channel, and a
 *           result-processing task would collect the result. The
 *           result-processing task does not need to be dedicated
 *           to this purpose, but the results need to be collected
 *           before the next call to the same task is made.
 */
#define RETURN_CHANNEL(callee, type) \
    __nv CH_TYPE(caller, callee, type) _ch_ret_ ## callee = \
        { { CHAN_TYPE_RETURN, { #callee, "ret:"#callee } } }

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
        { { CHAN_TYPE_MULTICAST, { #src, "mc:" #name } } }

#define CH(src, dest) (&_ch_ ## src ## _ ## dest)
#define SELF_CH(tsk)  CH(tsk, tsk)

/* For compatibility */
#define SELF_IN_CH(tsk)  CH(tsk, tsk)
#define SELF_OUT_CH(tsk) CH(tsk, tsk)

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
#define CHAN_IN1(type, field, chan0) \
    ((type*)((unsigned char *)chan_in(#field, sizeof(VAR_TYPE(type)), 1, \
          chan0, offsetof(__typeof__(chan0->data), field))))
#define CHAN_IN2(type, field, chan0, chan1) \
    ((type*)((unsigned char *)chan_in(#field, sizeof(VAR_TYPE(type)), 2, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field))))
#define CHAN_IN3(type, field, chan0, chan1, chan2) \
    ((type*)((unsigned char *)chan_in(#field, sizeof(VAR_TYPE(type)), 3, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field), \
          chan2, offsetof(__typeof__(chan2->data), field))))
#define CHAN_IN4(type, field, chan0, chan1, chan2, chan3) \
    ((type*)((unsigned char *)chan_in(#field, sizeof(VAR_TYPE(type)), 4, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field), \
          chan2, offsetof(__typeof__(chan2->data), field), \
          chan3, offsetof(__typeof__(chan3->data), field))))
#define CHAN_IN5(type, field, chan0, chan1, chan2, chan3, chan4) \
    ((type*)((unsigned char *)chan_in(#field, sizeof(VAR_TYPE(type)), 5, \
          chan0, offsetof(__typeof__(chan0->data), field), \
          chan1, offsetof(__typeof__(chan1->data), field), \
          chan2, offsetof(__typeof__(chan2->data), field), \
          chan3, offsetof(__typeof__(chan3->data), field), \
          chan4, offsetof(__typeof__(chan4->data), field))))

/** @brief Write a value into a channel
 *  @details Note: the list of arguments here is a list of
 *  channels, not of multicast destinations (tasks). A
 *  multicast channel would show up as one argument here.
 */
#define CHAN_OUT1(type, field, val, chan0) \
    chan_out(#field, &val, sizeof(VAR_TYPE(type)), 1, \
             chan0, offsetof(__typeof__(chan0->data), field))
#define CHAN_OUT2(type, field, val, chan0, chan1) \
    chan_out(#field, &val, sizeof(VAR_TYPE(type)), 2, \
             chan0, offsetof(__typeof__(chan0->data), field), \
             chan1, offsetof(__typeof__(chan1->data), field))
#define CHAN_OUT3(type, field, val, chan0, chan1, chan2) \
    chan_out(#field, &val, sizeof(VAR_TYPE(type)), 3, \
             chan0, offsetof(__typeof__(chan0->data), field), \
             chan1, offsetof(__typeof__(chan1->data), field), \
             chan2, offsetof(__typeof__(chan2->data), field))
#define CHAN_OUT4(type, field, val, chan0, chan1, chan2, chan3) \
    chan_out(#field, &val, sizeof(VAR_TYPE(type)), 4, \
             chan0, offsetof(__typeof__(chan0->data), field), \
             chan1, offsetof(__typeof__(chan1->data), field), \
             chan2, offsetof(__typeof__(chan2->data), field), \
             chan3, offsetof(__typeof__(chan3->data), field))
#define CHAN_OUT5(type, field, val, chan0, chan1, chan2, chan3, chan4) \
    chan_out(#field, &val, sizeof(VAR_TYPE(type)), 5, \
             chan0, offsetof(__typeof__(chan0->data), field), \
             chan1, offsetof(__typeof__(chan1->data), field), \
             chan2, offsetof(__typeof__(chan2->data), field), \
             chan3, offsetof(__typeof__(chan3->data), field), \
             chan4, offsetof(__typeof__(chan4->data), field))

/** @brief Transfer control to the given task
 *  @param task     Name of the task function
 *  */
#define TRANSITION_TO(task) transition_to(TASK_REF(task))

#endif // CHAIN_H
