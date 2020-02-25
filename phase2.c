/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona South
   Computer Science 452

   ------------------------------------------------------------------------ */
#include<stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
void check_kernel_mode(char *str);
void enableInterrupts();
void disableInterrupts();
int check_io();


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

unsigned int next_mbox_id = 0;

/* the mail boxes */
mail_box MailBoxTable[MAXMBOX];

/* lists */
mbox_proc_ptr BlockedList;


/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
   if (DEBUG2 && debugflag2)
      console("start1(): at beginning\n");

   check_kernel_mode("start1\n");

   /* Disable interrupts */
   disableInterrupts();

   /* Initialize the mail box table, slots, & other data structures.
    * Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */
   int kid_pid = 0;
   int status = 0;
   int i;
   BlockedList = NULL;

   for(i = 0; i < MAXPROC; i++)
   {
     MailBoxTable[i].mbox_id = -1;
     MailBoxTable[i].next_mbox = NULL;
   }

   enableInterrupts();

   /* Create a process for start2, then block on a join until start2 quits */
   if (DEBUG2 && debugflag2)
      console("start1(): fork'ing start2 process\n");
   kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
   if ( join(&status) != kid_pid ) {
      console("start2(): join returned something other than start2's pid\n");
   }

   return 0;
} /* start1 */

/* ------------------------------------------------------------------------
    Name - check_io
    Purpose - dummy function which represents checking IOs.
    Parameters - none
    Returns - one, returns 0
    Side Effects - none
    ----------------------------------------------------------------------- */
int check_io()
{
  return 0;
}/* check_io */

/* ------------------------------------------------------------------------
     Name - enableInterrupts
     Purpose - enables interrupts so interrupts.
     Parameters - none
     Returns - none
     Side Effects - PSR value is altered
     ----------------------------------------------------------------------- */
void enableInterrupts()
{
  check_kernel_mode("enableInterrupts");
  psr_set( psr_get() | PSR_CURRENT_INT );
}/* enableInterrupts */

/* ------------------------------------------------------------------------
     Name - disableInterrupts
     Purpose - disables interrupts so interrupts do not stop flow of execution.
     Parameters - none
     Returns - none
     Side Effects - PSR value is altered.
     ----------------------------------------------------------------------- */
void disableInterrupts()
{
  check_kernel_mode("enableInterrupts");
  psr_set( psr_get() & ~PSR_CURRENT_INT );
}/* disableInterrupts */

/* ------------------------------------------------------------------------
     Name - check_kernel_mode
     Purpose - Checks if calling process is in kernel mode. Halts simulation 
               if calling process is not in kernel mode.
     Parameters - one, char *str which is a pointer to a string holding which
                  function called check_kernel_mode so it may be displayed in
                  the error message, if necessary.
     Returns - none
     Side Effects - halts USLOSS if calling process is not in kernel mode.
     ----------------------------------------------------------------------- */
void check_kernel_mode(char *str)
{
  if((PSR_CURRENT_MODE & psr_get()) == 0)
  {
    console("%s: not in kernel mode\n", str);
    halt(1);
  }
}/*check_kernel_mode */

/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
  check_kernel_mode("MboxCreate");

  int table_pos = next_mbox_id % MAXPROC;
  int mbox_id_count = 0;

  if(slot_size < 0 || slot_size > MAX_MESSAGE)
    return -1;

  while(mbox_id_count < MAXPROC && MailBoxTable[table_pos].mbox_id != -1)
  {
    next_mbox_id++;
    table_pos = next_mbox_id % MAXPROC;
    mbox_id_count++;
  }

  if(mbox_id_count >= MAXPROC)
    return -1;

  MailBoxTable[table_pos].mbox_id = next_mbox_id++;
  MailBoxTable[table_pos].num_slots = slots;
  MailBoxTable[table_pos].slot_size = slot_size;

  return MailBoxTable[table_pos].mbox_id;

} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
  check_kernel_mode("MboxSend");
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  check_kernel_mode("MboxReceive");
} /* MboxReceive */
