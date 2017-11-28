Chain: Task-based Programming Model for Intermittent Software
=============================================================

Chain ([OOPSLA\'16](http://dl.acm.org/citation.cfm?id=2983995)) is a
programming model for the intermittent execution model, that arises on
intermittently-powered energy-harvesting devices. The `libchain` library
implements this programming model with a set of extensions to the C language
and a runtime. To write a program using Chain, the application source must
include the header and link against the library.

To get started quickly writing, building and deploying applications with Chain
on real energy-harvesting devices, see the guide in the [release
documentation](https://github.com/CMUAbstract/releases/Chain.md).

Table of Contents
-----------------

* [Overview](#overview)
* [Programming Interface](#chain-programming-interface)
* [Diagnostics](#diagnostics)
* [Dependencies](#dependencies)

Overview
--------

In the Chain programming model, applications are expressed as a graph of
statically-defined tasks with explicit control flow statements that transfer
control from one task to the next. Tasks store *local* data in variables
allocated on the (volatile) stack and *shared* data in persisted channels
statically allocated in non-volatile memory. The programming interface
provided by `libchain` includes macros for declaring tasks and channels,
and for accessing channels from task code.

Programming Interface
---------------------

The programmer\'s interface provided by `libchain`, and defined in
`libchain/src/include/libchain/libchain.h`, consists of a set of macros for
declaring tasks and channels, passing data through channels, and transferring
control flow between tasks.

A guiding principle followed in this implementation is to keep as much work as
possible static, avoiding introducing any kind of runtime state that is
fundamentally statically-determined. This approach often sacrifices conciseness
in return for performance and extensibility for eventually creating a compiler
that would compile a nicer syntax into the above primitives.

Declare and define a task:

    TASK(index, task_body_func)

    void task_body_func() {
        ...
    }

The *index* parameter must be a distinct number for each task. Throughout
the API, tasks are identified by the name of their body function.

Declare a channel, which declares the typed fields that hold the data exchanged
between tasks:

    struct msg_type {
        CHAN_FIELD(type, name);
        ...
    };

    CHANNEL(task_name_from, task_name_to, msg_type);

Self-channels require a dedicated macro for its fields and an initializer:

    struct msg_self_type {
        SELF_CHAN_FIELD(type, name);
        ...
    };
    #define FIELD_INIT_msg_self_type \
        SELF_FIELD_INITIALIZER \
        ...
    }

    SELF_CHANNEL(task_name, msg_self_type);

Multicast channels are declared using a dedicated macro that accepts a
unique name for the channel and a list of destination tasks:

    MULTICAST_CHANNEL(msg_type, ch_name, task_name_from, task_name_to_1, task_name_to_2, ...);

In the task code, channels are identified by their endpoints. Channel identifiers
for different channel types are constructed using the following macros.

Regular channels are identified using the `CH()` macro, which takes the source
task and the destination task:

    CH(task_name_from, task_name_to)

Multicast channels are identified by the `MC_OUT_CH()`/`MC_IN_CH()` macros
(when writing to and reading from the channel, respectively), which take the
unique name of the multicast channel, as well as the source, and all
destination tasks:

    MC_OUT_CH(ch_name, task_name_from, task_name_to_1, task_name_to_2, ...)
    MC_IN_CH(ch_name, task_name_from, task_name_to)

Self-channels are identified by the `SELF_IN_CH()/SELF_OUT_CH()`, macro which
takes the channel name:

    SELF_CH(task_name)

To write data from local variable `var` of type `type` to the field `field` in
one or more (*n*) channels, each identified by one of the above
channel-identifier macros:

    CHAN_OUTn(type, var, field, CH(...), CH(...), MC_OUT_CH(...), SELF_CH(...), ...)

To read data into local variable `var` of type `type` from the field `field` from
the channel where the field was most-recently updated out of a set of *n*
potential channels all of which contain this field:

    type var = *CHAN_INn(type, field, CH(...), SELF_IN_CH(...), CH(...), ...)

To transition control between tasks, task code may invoke the transition statement
at any point:

    TRANSITION_TO(task_destination)

Diagnostics
-----------

To have `libchain` produce verbose console output on each operation, e.g.
channel access, set the [Maker](https://github.com/CMUAbstract/maker) build
configuration variable `LIBCHAIN_ENABLE_DIAGNOSTICS`, by adding the following
statement to the `bld/Makefile` of the application:

    export LIBCHAIN_ENABLE_DIAGNOSTICS = 1

Dependencies
------------

Note that `libchain` is packaged for [Maker](https://github.com/CMUAbstract/maker),
which means that the application's dependencies (including all transitive dependencies)
are declared in `bld/Makefile`, included into the application as submodules in
`ext/`, and are automatically built along with the application.

Chain depends on the following libraries:

* [libmsp](https://github.com/CMUAbstract/libmsp) : basic lowest-level
common "drivers" for MSP430 microcontrollers

`libmsp` is required for macros for declaring persistent variables.

Optional dependencies for [diagnostic builds](#diagnostics) for a console:

* [libio](https://github.com/CMUAbstract/libio) and its transient dependencies
