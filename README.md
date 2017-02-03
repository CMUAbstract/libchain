This document is a companion guide to the artifacts for OOSPLA 2016 paper #28:
"Chain: Tasks and Channels for Reliable Intermittent Programs."

Table of Contents
=================

   * [Synopsis](#synopsis)
      * [Virtual Machine Image](#virtual-machine-image)
   * [Dependencies](#dependencies)
      * [MSP430 Toolchain](#msp430-toolchain)
      * [Maker](#maker)
      * ['''OPTIONAL''': LLVM/Clang](#optional-llvmclang)
      * [Environment](#environment)
      * [Libraries](#libraries)
        * [WISP base firmware library](#wisp-base-firmware-library)
        * [Energy-Interference-free Debugger (EDB) and libedb](#energy-interference-free-debugger-edb-and-libedb)
        * [Auxiliary libraries](#auxiliary-libraries)
           * [libmsp](#libmsp)
           * [libmspprintf](#libmspprintf)
           * [libmspconsole](#libmspconsole)
           * [libio](#libio)
           * [libmspmath](#libmspmath)
           * [libmspbuiltins](#libmspbuiltins)
   * [Chain Runtime](#chain-runtime)
   * [Prior work (OPTIONAL)](#prior-work-optional)
      * [Mementos](#mementos)
         * [Runtime libs](#runtime-libs)
         * [LLVM passes](#llvm-passes)
      * [DINO](#dino)
   * [Applications](#applications)
      * [LED Blinker / Template for Custom Applications](#led-blinker--template-for-custom-applications)
      * [Cuckoo Filter](#cuckoo-filter)
      * [Cold-Chain Equipment Monitoring](#cold-chain-equipment-monitoring)
      * [AR](#ar)
      * [RSA](#rsa)

Synopsis
========

This document is a companion guide to the artifacts for OOSPLA 2016 paper #28:
"Chain: Tasks and Channels for Reliable Intermittent Programs."

The artifact is a complete, deployment-ready development environment for our
language Chain, which targets intermittently-powered energy-harvesting embedded
systems.  This guide precisely documents how to set up the
development environment from scratch and how to build the applications
evaluated in the paper. 

The main Chain language implementation is a self-contained library called
`libchain`.  This guide describes how to build `libchain` and then build all
applications to refer to `libchain` headers and link to the built `libchain`
library.  This guide also describes how to build build-system components and
supporting libraries that are required by applications and by `libchain`.
This guide also includes a "Hello, World!" (LED blinking) application that
makes a handy starting point for quickly creating custom Chain applications to
run on real hardware.

In the interest of completeness of the evaluation of our artifact, we also
include a complete, deployment-ready development environment for DINO and
Mementos, the systems  that were the basis of comparison for Chain in the
paper.  That environment also includes DINO- and Mementos-ready versions of the
applications, too, allowing an artifact reviewer to build Chain, DINO, and
Mementos versions side-by-side.  Our custom build system manages the complexity of
targeting these multiple build variants. 

Chain targets real, energy-harvesting hardware, namely the Wireless
Identification and Sensing Platform (WISP) v5, which is based on the TI
MSP430FR5969 microcontroller.  Our custom build system manages the complexity
of cross-compiling for this hardware.

The workflow in this guide consists of (1) installing third-party dependencies,
(2) building auxiliary platform-support libraries, (3) building the Chain
runtime, and (4) building the applications.  This guide describes exactly what
it takes to set up a Chain development environment on any reasonable Linux
machine (tested on Arch Linux and users reported success on Ubuntu).

To save the reviewer time, we already followed all the steps in this guide on a
virtual machine (VM) and included the VM image with the artifact submission.
If you are using the VM, the Chain development environment is already built,
set up, and ready to roll: you can skip directly to the section on building and
re-building the applications, or perhaps, create your own, new Chain
application based on the LED blinking application.  

The URLs of the source code repositories have been hidden in this guide, to
keep the review blind. The URLs are still present in the VM image, because it
is impractical to purge that information, as VCS is part of our workflow to
keep the code modular. To keep the review blind, we trust the reviewer to not
inspect the Git history.

To demonstrate the results illustrated in the paper directly would put an
unreasonable burden on an artifact evaluator, requiring them to obtain and
configure an RF energy-harvesting power source, a WISP, and a hardware WISP
programmer.  Instead, (as per discussion with the AEC chairs) we included a
demonstration video for evaluation, alongside our artifact VM.   The video
demonstrates the application binaries running on the target hardware,
illustrating the expected execution behavior under intermittent power,
validating the results from the paper. The videos are located in `experiments/`
directory:

 * `blinker-experiments.ogv` : a demonstration how Chain maintains
application correctness across power failures, using a basic
"Hello, World!" application
 * Screencast demonstrations of each implementation variant of an application
   using Chain and using state-of-the-art (Mem-NV, Mem-V, and DINO), running on
the real WISP device, first powered by a wireless intermittent energy source,
and then -- for validation -- powered by a wired continuously available energy
source:
    * `cuckoo-filter/` : Cuckoo Filtering application
    * `cem/` : Cold-Chain Equipment Monitor application
    * `rsa/` : RSA encryption application
    * *Experiments with Activity Recognition app could not be repeated because
the experiment requires wiring setup for training the model that could
not be re-constructed in the alloted time. The code for this application can be
inspected and built for all systems.

*NOTE*: The WISP device used for the original experiments is no longer
available in it's past configuration, since it had to be modified (e.g. the
capacitor had to be decreased from 47uF to 10uF). The RF environment also
necesserily changed. As a result, the magnitudes of the quantative performance
results in the video will differ from the original experiments.  Naturally, all
correctness claims about consistency of program state in non-volatile memory
hold in the old and the new experiments alike.

Virtual Machine Image
---------------------

We include a virtual machine appliance image in OVA format. It was created with
VirtualBox v5, but it might also be possible to use the appliance in VMware.

Prior to importing the virtual appliance into VirtualBox, create a host-only
network interface to create a network connection to the VM from the host. The
virtual appliance has two network interfaces, one NAT, once host-only. The
point of creating a host-only network connection is to allow you to SSH to the
VM from your host machine; the terminal in the VM is ugly and using your native
terminal via SSH will be much nicer.  The VM automatically starts an SSH
daemon, allowing you to SSH from your host to the VM.  If you are unsure of the
IP address of the VM, you can check by logging in directly through the VirtualBox GUI
and executing `ip addr`.  The IP address will probably be something like
`192.168.56.101` and its subnet should correspond to the subnet of the
host-only network adaptor on your host machine. 

The virtual machine should have two disks attached to the SATA controller.
Both disks mount automatically on boot.

The operating system is a minimal installation of Arch Linux.

*Credentials* are: user `reviewer` password `review`.

If you are using the VM, the build environment is already installed, and all
source code is already fetched and built -- *it is not necessary to build
everything again unless you want to*.  You can follow the next steps to set up
the development environment again from scratch, either as an exercise, or
to ensure that our tutorial is correct.  Alternatively, you can skip to the
section called `Applications` below for instructions on how to build, re-build,
or extend our applications.

Dependencies
============

## MSP430 Toolchain

TI MSP430 GCC Toolchain is the cross-compiler used to build executables for the
MSP430 platform.

Version: Only v3.05 is supported; changes in v4.00 appear to not be backward compatible.

Upstream: http://www.ti.com/tool/msp430-gcc-opensource

Arch Linux package (from AUR): `mspgcc-ti`, installed to `/opt/ti/mspgcc`


## Maker

The Maker tool is a custom package manager and builder for C code written in
GNU Make. Maker makefiles are high level instructions that can specify
dependent libraries.

This artifact uses an older version of Maker, in which libraries need to be
built manually.

    git clone -b oopsla16-artifact https://github.com/CMUAbstract/maker

Edit path to TI MSP430 GCC Toolchain in `maker/Makefile.env`:

    TOOLCHAIN_ROOT ?= $(TI_ROOT)/mspgcc


## '''OPTIONAL''': LLVM/Clang

LLVM/Clang is a compiler with a MSP430 assembly backend, that relies on MSP430
GCC Toolchain (see above) to compile the assembly into native object code.

The LLVM/Clang toolchain is NOT needed to build Chain applications. It is only
needed to build applications with competing systems: Mementos and DINO.

Version: v3.8 with a patch that fixes a compatibility issue in MSP430 backend.

Upstream: http://llvm.org/

Build instructions: http://llvm.org/docs/CMake.html

Brief synopsis:

    mkdir llvm-install && mkdir llvm-build && cd llvm-build
    cmake -G "Unix Makefiles" -DLLVM_OPTIMIZED_TABLEGEN=1 -DLLVM_TARGETS_TO_BUILD="MSP430;X86" \
	-DCMAKE_LINKER=/usr/bin/ld.gold -DCMAKE_INSTALL_PREFIX=../llvm-install ../llvm-src

In the virtual machine, LLVM was built from source at `/opt/llvm/llvm-src` and
installed to `/opt/llvm/llvm-install`. The build directory (`/opt/llvm/llvm-build`) was
removed to save space.

The patch for MSP430 backend is commit 81386d4b4fdd80f038fd4ebddc59613770ea236c.

Environment
-----------

Create a workspace directory: `mkdir ~/src && cd ~/src`.

The build framework relies on the following environment variables to be set:

    export DEV_ROOT=$HOME/src
    export MAKER_ROOT=$HOME/src/maker
    export TOOLCHAIN_ROOT=/opt/ti/mspgcc
    export LLVM_ROOT=/opt/llvm/llvm-install

Libraries
---------

## WISP base firmware library

The `wisp-base` library provides low-level functionality for setting up the
microcontroller and using the peripherals on the WISP platform.

We use a forked version with bug fixes and other build-related patches.

    git clone -b oopsla16-artifact https://github.com/CMUAbstract/wisp5.git
    cd wisp5/CCS/wisp-base/gcc
    make

The above command will create `libwisp-base.a` in the above directory.

## Energy-Interference-free Debugger (EDB) and libedb

EDB is a hardware/software debugger for energy-harvesting devices previously
developed and [published in prior
work](http://dl.acm.org/citation.cfm?id=2872409).  EDB is a hardware printed
circuit board, a library that links into the target application (`libedb`), and
host-side software.  Our target hardware (i.e., WISP) has no simple way to
produce human-readable output.  We use EDB's intermittence-safe `PRINTF` to
forward output to the host workstation from the target, while it runs on
intermittent (RF) energy.
 
    cd ~/src
    git clone -b oopsla16-artifact git@github.com:CMUAbstract/libedb.git
    cd libedb/gcc
    make

The above command will create `libedb.a` in the above folder.

## Auxiliary libraries

In order to run on the target hardware platform, the applications rely on
several re-usable platform support libraries:

 * libmsp: MCU clock setup, peripherals (complementary to `libwisp-base`)
 * libmspprintf: a lean `printf` implementation
 * libmspconsole: an output "backend" for `libio` using HW UART
 * libio: wrapper around different output "backends" (e.g. HW UART, SW UART, EDB PRINTF)
 * libmspmath: division, multiplications, sqrt
 * libmspbuiltins: builtin functions (e.g. `__delay_cycles`) not provided by LLVM/Clang's runtime library

Each of these libraries must be built either with the same compiler as the
application or unconditionally with GCC (even if linked into an LLVM-built
application). The following steps obtain the source and produce all the
necessary builds of the libraries.

### libmsp

    cd ~/src
    git clone -b oopsla16-artifact https://github.com/CMUAbstract/libmsp.git

    cd bld/gcc && make

    export LLVM_ROOT=/opt/llvm/llvm-install
    cd bld/clang && make

### libmspprintf

    cd ~/src
    git clone -b oopsla16-artifact https://github.com/CMUAbstract/libmspprintf.git
    cd bld/gcc && make

### libmspconsole

    cd ~/src
    git clone -b oopsla16-artifact https://github.com/CMUAbstract/libmspconsole.git
    cd bld/gcc && make 

### libio

The `libio` library only contains headers, so there is nothing to build.

    cd ~/src
    git clone -b oopsla16-artifact https://github.com/CMUAbstract/libio.git

### libmspmath

    cd ~/src
    git clone -b oopsla16-artifact https://github.com/CMUAbstract/libmspmath.git
    cd bld/gcc && make

### libmspbuiltins

Note that `libmspbuiltins` is always built with GCC, but included in LLVM/Clang
builds of the application. (In GCC application builds, this library may be still
included for uniformity, but in that case it is effectively empty.)

    cd ~/src
    git clone -b oopsla16-artifact https://github.com/CMUAbstract/libmspbuiltins.git
    cd bld/gcc && make

Chain Runtime
=============

The Chain runtime consists of one library: `libchain`. In this section we build
the library. Any application that wishes to use the Chain abstractions needs to
include the headers from this library and link against the library binary.

   git clone -b oopsla16-artifact https://github.com/CMUAbstract/libchain.git
   cd bld/gcc
   make

The above command will create `libchain.a` in the above directory.

To show communication activity over channels using printf, define the following
flag when compiling libchain *and* the application:

    make LIBCHAIN_ENABLE_DIAGNOSTICS=1


Prior work (OPTIONAL)
=====================

Mementos
--------

Mementos is one of the previously proposed systems for intermittent computing
that we evaluated. It consists of a runtime library and LLVM passes that
must be run on the application binaries.  Once both parts of Mementos have been
built, building applications with Mementos is handled by the Maker tool.
The application developer needs to create only a high-level Maker makefile in a
dedicated build directory (`bld/mementos`). The applications in this artifact
all contain this makefile.

We use a fork of Mementos that differs from the original only in packaging
details to make it easier to re-use across applications. Part of this
re-packaging are patches for compatibility with a newer version of GCC and
LLVM/Clang.

The following steps in this section fetch and build Mementos. This needs to be
done only once -- multiple applications use the artifacts built here.

    cd ~/src
	git clone -b oopsla16-artifact https://github.com/CMUAbstract/mementos.git

### Runtime libs

	cd mementos/autoreconf && ./AutoRegen.sh && cd ..
	./configure --with-gcc=/opt/ti/mspgcc --with-llvm=/opt/llvm/llvm-install
	make FRAM=1

The above command builds several variants of the Mementos runtime library
(`mementos+*.bc`) with LLVM/Clang. This library is linked into the application
when the application is built with Mementos.

We use the best performing variant, `timer+latch`, which inserts checkpoints at
loop backedges and throttles checkpointing frequency with a timer.

### LLVM passes

    cd llvm && mkdir build && cd build
    cmake -DCMAKE_MODULE_PATH=/opt/llvm/llvm-install/share/llvm/cmake \
        -DCMAKE_PREFIX_PATH=/opt/llvm/llvm-install \
        -DCMAKE_INSTALL_PREFIX=/opt/llvm/llvm-install \
        -DCMAKE_CXX_FLAGS="-std=c++11" ..
    cd ..
    make && make install

The above command installs `Mementos.so` shared object to the LLVM installation path.
This shared object will be loaded by the LLVM `opt` tool as part of each application
Mementos build.

DINO
----

DINO is one of the previously proposed systems for intermittent computing that
we evaluated. Like Mementos, it consists of a runtime library and LLVM passes.
We avoid building the LLVM passes by instrumenting our applications manually.
Maker handles the complexity of building an application with DINO.

Similarly to our re-packaging of Mementos, we fork and re-package DINO,
patching it as necessary for compatibility with a newer version of GCC and
LLVM/Clang, and ease of re-use by applications.

The following steps fetch and build DINO. This needs to be done only once --
multiple applications use the artifacts built here.

    cd ~/src
	git clone -b oopsla16-artifact https://github.com/CMUAbstract/dino.git

    cd ~/dino/DinoRuntime
	export LLVM_ROOT=/opt/llvm/llvm-install
	export GCC_ROOT=/opt/ti/mspgcc
	export MEMENTOS_ROOT=~/src/mementos
	make -f Makefile.clang

The above command will create `dino.a.bc` in the above directory. Note 

Applications
============

In this section we build five applications with Chain as well as with Mementos
and DINO. The first application is a "Hello, World!" (LED blinking) template
skeleton that can be copied and used to quickly create a custom application,
build and run on the target hardware. The remaining four applications are those
evaluated in the paper:

 * CF: Approximate set membership using a cuckoo filtering
 * CEM: Cold-Chain Equipment Monitoring with LZW compression 
 * RSA: Data Encryption using RSA
 * AR: Activity Recognition using an accelerometer

[Several additional application
examples](https://github.com/amjadmajid/chain_apps) in Chain have been
generously contributed by [Amjad Majid](http://www.st.ewi.tudelft.nl/~amjad/)
at Embedded Software group at [TU Delft](http://www.es.ewi.tudelft.nl).

Each application in the above list has a dedicated repository for its
implementation in Chain and a separate repository with its implementation in C.
The latter is optionally compilable with checkpointing-based systems, Mementos
or DINO. The names of the pairs of repositories for each application follow the
patterns, `app-*-chain` and `app-*-chkpt`, respectively.

In the virtual machine image applications have been cloned and built
in `~/src/apps/*` directories.

The source tree layout for all application repositories follows the following
structure:

     README
     + src
     + bld

Application source code is in `src`. Subdirectories of `bld` contain Maker
makefiles and built application binaries. Each of these build directories
contains a high-level Maker makefile specific to that build type.
Builds can co-exist without interference.  To build a particular build type run
`make` in the corresponding directory.

For *Chain implementations*, the relevant build sub-directory is always
`bld/gcc`. For example, to build CF application implemented in Chain:

    cd ~/src/apps/app-cuckoo-chain
    cd bld/gcc
    make

For *checkpointing-based implementations* the build sub-directories are:

     + bld
       + gcc
       + clang
       + mementos
       + dino

For exampe, to build a particular build type run `make` in the corresponding directory.
For example, to build the CF application with DINO:

    cd ~/src/apps/app-cuckoo-chkpt
    cd bld/dino
    make

The Mem-V and Mem-NV variants share the same build directory. The Mem-V variant
is the default. To build the Mem-NV variant run `make MEMENTOS_VARIANT=NV`.

**NOTE**: Some applications may have a further hierarchy split at the top-level into
`bin` and `lib`. In this case `lib` is an auxiliary library that contains
source files that must be excluded from the checkpointing instrumentation (e.g.
printf-related code). The application in `bin` has the same structure as
described in the diagram above, and simply links in the auxiliary library from
`lib`.  The library and the binary must be built separately in order. The
necessary commands are given below.

**NOTE**: Before building any applications, make sure that the environment
variables listed in the "Environment"  section of this document are set.

## LED Blinker / Template for Custom Applications

The LED Blinker is a simple application that demonstrates basic use of tasks
and channels. The source tree of this application code can be copied and
modified to quickly create a custom application using Chain.

    git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-blinker-chain.git
    cd bld/gcc && make

## Cuckoo Filter

The Cuckoo Filter application populates a cuckoo filter data structure with
pseudo-random keys and quieries it to determine appoximate set membership.

   cd ~/src/apps
   git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-cuckoo-chain.git
   cd app-cuckoo-chain && cd bld/gcc && make

   cd ~/src/apps
   git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-cuckoo-chkpt.git
   cd app-cuckoo-chkpt/lib/bld
   cd gcc      && make && cd ..
   cd clang    && make && cd ..
   cd ../..
    
   cd bin/bld
   cd gcc      && make && cd ..
   cd clang    && make && cd ..
   cd mementos && make && cd ..
   cd dino     && make && cd ..

The above commands will create binaries `cuckoo.out` in each build subdirectory,
that are ready to be flashed onto the target device.


## Cold-Chain Equipment Monitoring 

The CEM application reads temperature samples from a sensor, compresses the
data stream with LZW and outputs the compressed stream to EDB console.

   cd ~/src/apps
   git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-temp-log-chain.git
   cd app-temp-log-chain && cd bld/gcc && make

   cd ~/src/apps
   git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-temp-log-chkpt.git
   cd app-temp-log-chkpt/lib/bld
   cd gcc      && make && cd ..
   cd clang    && make && cd ..
   cd ../..
    
   cd bin/bld
   cd gcc      && make && cd ..
   cd clang    && make && cd ..
   cd mementos && make && cd ..
   cd dino     && make && cd ..

The above commands will create binaries `temp-log.out` in each build subdirectory,
that are ready to be flashed onto the target device.

## AR

Activity Recognition application reads sensor data from an accelerometer and
classifies into two classes -- "stationary" and "moving" -- based on a
pre-trained model. At the end of set number of samples, aggregate statistics
about the classes are reported via the EDB console.

   cd ~/src/apps
   git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-activity-chain.git
   cd app-activity-chain && cd bld/gcc && make

   cd ~/src/apps
   git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-activity-chkpt.git
   cd app-activity-chkpt/bld
   cd gcc      && make && cd ..
   cd clang    && make && cd ..
   cd mementos && make && cd ..
   cd dino     && make && cd ..

The above commands will create binaries `ar.out` in each build subdirectory,
that are ready to be flashed onto the target device.


## RSA

The RSA application encrypts plain text stored in non-volatile memory and
stores the cyphertext also in non-volatile memory. After encryption is
completed, the cyphertex is output in hex via EDB console. Sample plaintext
inputs are available in `data/` subdirectory.

**IMPORTANT NOTE**: RSA was the first application implemented prior to
(syntactic) improvements to `libchain` API, and therefore is compatible only
with an earlier version of `libchain` (v0.1). To fetch and build this version
of `libchain` into `~/src/libchain-v0.1` (already fetched and built in the VM
image):

     cd ~/src
     git clone -b oopsla16-artifact-v0.1 https://github.com/CMUAbstract/libchain.git libchain-v0.1
     cd libchain-v0.1 bld/gcc
     make

To build the applicaion against the earlier `libchain` version, set 

     cd ~/src/apps
     git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-rsa-chain.git
     cd app-rsa-chain/bld/gcc
     make LIBCHAIN_ROOT=$DEV_ROOT/libchain-v0.1
  
     cd ~/src/apps
     git clone -b oopsla16-artifact https://github.com/CMUAbstract/app-rsa-chkpt.git
     cd app-rsa-chkpt/bld
     cd gcc      && make && cd ..
     cd clang    && make && cd ..
     cd mementos && make && cd ..
     cd dino     && make && cd ..
  
The above commands will create binaries `rsa.out` in each build subdirectory,
that are ready to be flashed onto the target device.
