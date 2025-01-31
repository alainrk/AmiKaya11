# AMIKaya11

Project for the Operating Systems course, written, developed and tested on Intel x86 with Ubuntu Linux v10.04 operating system and umps2 v1.9.7. The implemented levels are Phase1 and Phase2.

## PHASE1

This level implements two modules, one for thread management and one for messages. Essentially phase1 provides lists and their management utilities for the higher levels.

### TCB.C
This module provides support for thread management, first creating a free list of Thread Control Blocks (fundamental atomic structure of the system) and then their allocation and destruction (return to the free TCB list).

It also implements functions for creating thread queues and lists, manageable through functions for adding/removing/verifying TCB membership to such queues.

Functions for managing TCB trees are also fundamental, useful for maintaining an executive hierarchy in the system, with parent-child-sibling relationships.

### MSG.C
Very similar to the TCB module, it largely mirrors it regarding list management. It differs in that it provides functionality to help phase2 message passing, implementing for example functions for POP and PUSH operations of messages in TCB inboxes.

In this case too we have a list of free messages in the system, from which messages are removed or returned based on what is requested by the executing kernel.

## PHASE2

### BOOT.C
This contains the kernel entry point, along with the declaration of some important global variables used throughout its lifetime.

First, the four New Areas are populated, which are those memory zones that must contain the state that will be loaded when exceptions occur.

For this purpose, the Program Counter is set to the Handler entry point and the state is set so that it has interrupts disabled, is obviously in Kernel Mode and with virtual memory off; all this to ensure maximum protection, access, and non-interruptibility.

At boot, constants are also initialized, system lists and queues are created and initialized, particularly the Ready queue, which will contain processes ready to be executed, and the Wait queue, containing those processes waiting for some event, and especially messages.

Finally, a state for the SSI and one for the thread with entry point test() are created and put in Ready Queue in this order, the Pseudo Clock is started and control is passed to the scheduler.

### SCHEDULER.C
This module mainly deals with thread management on the CPU, and therefore loading onto it.
The scheduler first identifies two situations:

#### No process is currently active
This situation occurs immediately after boot or when a thread's time slice has expired, or if a process that was currently executing has been frozen/terminated.

In this situation, it is first necessary to check if there are processes in Ready Queue that can be loaded. In the favorable case, the first one is extracted, times are updated for the Pseudo Clock and the cpu_slice field of the TCB in question is set to zero to start. Then the minimum time between the time slice and the remaining Pseudo Tick is loaded on the Interval Timer and the thread state is loaded on the CPU which then starts executing its code.

In case the Ready Queue is empty, three further cases are distinguished:
- There is a single process in the system. This process will therefore be in wait, and will presumably be the SSI, so at this point it is evident that we are in a state where shutdown is necessary. Therefore, the ROM routine HALT() is invoked.
- There is more than one process in the system, but simultaneously no one is waiting for a service or a response from I/O. In this state it means that a circular wait situation has been created between threads, and therefore a deadlock is identified. What remains to be done is to invoke the PANIC() routine.
- There is more than one process in the system, and at least one thread is waiting for the termination of a service or an I/O. What remains to be done is to wait for something to happen, so the processor state is set to receive any interrupt, and the scheduler is put on hold (through an infinite loop, or with the WAIT() routine).

#### There is currently an active process (whose TCB is pointed to by current_thread)
This situation occurs every time an event (exception/interrupt) is triggered that has not, however, had the effect of putting the current process in Wait Queue or terminating it.

In this case, the Pseudo Tick is updated, and the minimum remaining time between the expiration of the current process's time slice and the remaining Pseudo Tick is loaded on the Interval Timer.

Finally, the process is reloaded so that it resumes its execution for its remaining time slice.

In both cases, before setting the Interval Timer, it is checked that the Pseudo Tick has not exceeded its maximum time, an event that could occur when control is passed to the scheduler from the exception/interrupt handler if the Tick is about to expire; in this case the Interval Timer is set to 1 so that as soon as interrupts are re-enabled, the Pseudo Clock can be managed more precisely by triggering an exception for interrupt on line 2.

### HANDLERS

Interrupts.c and Exceptions.c are the two modules that handle "extraordinary" events that occur during processor life.

They mainly implement the four functions corresponding to the Program Counters of the four New Areas populated at boot; exceptions can be of four types: Interrupt, Program Trap, TLB, SYS/BP.

What all four handlers have in common are the following activities:
- Saving the state of the current process (if there was one active at the time of the exception) from the corresponding Old Area where it was positioned by the ROM routine to the TCB.
- Updating the temporal fields of the TCB and for slice management.
- Possible Pseudo Tick update.
- Call to the scheduler at the end of the procedure.

Specifically:

### INTERRUPTS.C
This module implements the interrupt exception handler.

AMIKaya11 has no software interrupts (lines 0 and 1), so the remaining lines from 2 to 7 are handled. It is remembered that line 2 can be thought of as active for a single "device", which unlike others does not have a Device Register, and that line 7 includes two subdevices for each device (and therefore Device Register).

Initially, the cause register saved in the Old Area is retrieved and it is determined which line triggered the interrupt. Given the possibility of multiple lines simultaneously generating an interrupt, it is checked from the line with the lowest index (high priority) to the one with the highest index (low priority), creating a sort of loop for handling all pending interrupts, given that first of all one is handled at a time, and furthermore, as soon as IEc re-enables interrupts, we re-enter the handler.

Regarding lines 3 to 6, the activities are practically identical. What is done is first to load the bitmap of the treated line and then identify within it, with the same priority principle for the lines, the device that generated the interruption; this is done through the which_device function, which is provided with a bitmap and returns the index of the highest priority device with a pending interrupt on that line.

At this point, the corresponding device register is identified, so as to write the acknowledgment command in the command field and send to the SSI a message that has as sender the Device Register being treated, and as payload the status field of the same.

The message will be recognized by the SSI through the service field of the SSI_request_msg structure which will contain the magic number INTERRUPT_MSG.

Line 2 sets in motion the heart of timing management; it can be divided into two cases:
- Interrupt for Pseudo Clock: This state is recognized in the case where the Pseudo Tick has reached or exceeded its maximum, so a message is sent to the SSI (with priority, so the message is put at the head), which will take care of waking up all threads waiting for this event, with sender BUS_INTERVALTIMER and service PSEUDOCLOCK_MSG. The Pseudo Tick is obviously reset to zero and counting is restarted.
- Interrupt for Time Slice: This state is entered in a non-exclusive way with respect to the previous one, given the possibility of both conditions occurring when near both deadlines. In this case, the current thread is moved to the Ready Queue and current_thread is set to NULL.

Line 7 also supports eight devices, which however being terminals, are in turn composed of two subdevices, one for transmission (high priority) and one for reception (low priority).
Everything said for lines 3-6 also applies to this one, except for the demultiplexing of the two subdevices, thus managing two fields for status, two fields for commands, two different messages depending on the subdevice, and acknowledgment directed to the one being treated.

In this module, the structures for SSI messages are maintained at a global level as an array (struct SSI_request_msg interrupt_msg_array[]), and are used in a circular manner with an index internal to the module, incremented each time one is used.

### EXCEPTIONS.C
In addition to the actions described above, the modules implement appropriate procedures depending on the exception that occurred.

#### SYS/BP Exception Handler
This handler is entered whenever a SYSCALL or BREAKPOINT exception is raised; AMIKaya11 is based substantially on only two system calls, specifically on the two message passing primitives (send/receive), callable in kernel mode with codes 1 and 2 respectively, so the heart of the sys/bp exception handler therefore lies in the implementation of this message passing, executed when these circumstances are met.

It is remembered that registers a0, a1, a2 are used for parameter passing, and this also applies to the Syscalls in question.

- Send: first a check is made on sender, recipient and payload of the message, wanting to distinguish the case where a Trap Manager sends a TRAPTERMINATE / TRAPCONTINUE message to a thread previously blocked waiting for a decision (see below). The target thread is therefore terminated or awakened respectively.

In the "normal" case instead, the send() function is invoked which takes as parameters a sender, a recipient and a payload (which in the case of SSIRequest will be a pointer to the request structure just created).

This function allocates a message and inserts it in the recipient's inbox in case they were in Ready Queue, the current thread, or in Wait Queue waiting for another message, so that it can be retrieved in the future.

In case instead the recipient thread is waiting for a message from this thread (or from anyone, ANYMESSAGE), it is moved to Ready Queue and is returned the payload in the location specified in the corresponding recv(), saved in the reply field of its TCB.

- Receive: for both cases treated (ANYMESSAGE and with specified Thread) a popMessage() is first performed to verify the presence of the sought message.
In the positive case, the message is freed, the payload contained in it is positioned in the specified location and the recipient is returned; it is therefore a non-blocking operation.

It becomes blocking when instead the sought message is not present in the inbox, as the receiver is removed from the current thread and moved to Wait Queue, specifying in its TCB fields (waiting_for and reply), from whom it is waiting for a message and where it wants to receive the response.

Associated with these operations, SYS/BP Exception Handler recognizes message exchange situations to and from the SSI, first providing support for the protection of its TCB by transforming the magic number MAGIC_SSI into the associated real address, and also increments/decrements the soft_block_count whenever a thread requests/obtains a service.

In all other cases, and therefore: SYS/BP not in Kernel Mode, SYSCALL different from SEND/RECV, BREAKPOINT, TLB Trap, Program Trap, the exception is handled as follows.

It is verified that the current thread has specified a Trap Manager for the treated exception, through the services made available by the SSI.
In the negative case, the thread and all its progeny (the subtree of processes) are directly terminated.
In the positive case instead, the thread is blocked and moved to Wait Queue, setting the waiting_for field of its TCB with the appropriate Trap Manager, to which will also be sent a message containing the cause register saved in the Old Area and based on this the Manager will decide what to do, sending back a TRAPTERMINATE/TRAPCONTINUE message to the blocked thread, handled as described above.

### Terminate()
This function, used abundantly in this module but also by the SSI, has the function of terminating a subtree of threads, taking as parameter the root of the same.

The function acts recursively, terminating leaf by leaf the entire tree, until arriving at the root, calling the function itself on each child, as long as there are any.

In the base case, terminate() takes care of "cleaning" everything possible of the thread's presence in the system, therefore eliminating it from any list it is in, nullifying the current_thread variable if it is the currently executing thread, returning to the free list all messages it had in inbox, removing it from the Managers if it was part of them and decrementing the soft_block_count if it was waiting for services or I/O.

### SSI.C
What provides an interface to the system is this module, through the implemented services and accessible anyway through the two SYSCALL send and recv.

What is first implemented is the SSIRequest() wrapper, a function that in a very simple way acts like this:
- Creates an SSI_request_msg structure.
- Maps the passed fields (service, payload, reply) in the three fields specially created of the request structure.
- Sends a message (with a MsgSend) to the SSI that has as recipient a fake address MAGIC_SSI and as payload, the pointer to the created structure.
- Waits for a message (with a MsgRecv) from the SSI, which will have during its execution "unpacked" the passed structure to obtain useful information for the activation of the correct procedure.

The SSI thread lives its execution in a continuous loop, at the beginning of which it waits for a message, which is then demultiplexed to the correct procedure based on the service field extracted from the SSI_request_msg structure.

Priority is given first to messages that arrive from the Interrupt Handler, and therefore for the Pseudo Clock and for handling interrupts associated with WAITFORIO.

For details on the service interface, the specifications of the same are recommended; regarding instead the implementation choices on the mechanisms proper to the design, they can be summarized as follows.

- A global devStatus_array is declared, in which, at the time of handling the message for interrupts, the status field of the Device Register from which the message arrives (sent by the Interrupt Handler) will be stored if a WAITFORIO related to this device has not yet arrived.
- There is dually a global devTcb_array, in which the TCB pointer will be stored for the thread that has just done WAITFORIO for a device from which Device Register a interrupt message has not yet arrived.
- In the same way, for WAITFORCLOCK, a clockTcb_array is created, where the TCB pointers of those threads waiting for the Pseudo Tick will be stored, which will then be awakened in order by the appropriate SSI routine.

The first two arrays are managed with an indexing policy, aided by the rest_index() function, which associates to each Device Register (address) an index that indeed represents a position in the SSI help array. Given the presence of the two subdevices for line 7, there are two indices for each device, associated with the address of their status field.

### MANAGER.C
Finally, this module is simple support for the kernel's "accounting" regarding Trap Managers specified by threads with the appropriate calls to SSI.

Manager.c in fact implements a trap_managers array, containing the TCB pointers of managers currently present in the system, and a small set of basic functions useful for managing the same.

## NOTES
- Tested and working on umps2 v. 1.9.7.
- The tar.gz already contains a ready kernel configuration (the latest).
  [ Give the command "make clean && make all" to create a new one. ]
- Present warnings of the type: "warning: implicit declaration of function 'PANIC'" on provided routines such as PANIC, HALT, STST etc.
