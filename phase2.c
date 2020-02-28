/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona South
   Computer Science 452

   Author: Cristian Rivera
   Group: Cristian Rivera

   ------------------------------------------------------------------------ */
#include<stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
void check_kernel_mode(char *str);
void enableInterrupts();
void disableInterrupts();
int check_io();
void BlkList_Insert(int pid);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
unsigned int next_mbox_id = 0;

/* Process table for Phase 2 */
mbox_proc MboxProcTable[MAXPROC];

/* the mail boxes */
mail_box MailBoxTable[MAXMBOX];

/* the mail slots */
new_slot MSlot_Table[MAXSLOTS];

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

   /* Initializes the Phase 2 Process Table */
   for(i = 0; i < MAXPROC; i++)
   {
     MboxProcTable[i].pid = INIT_VAL;
     MboxProcTable[i].next_mbox_ptr = NULL;
   }

   /* Initializes the MailBox Array */
   for(i = 0; i < MAXMBOX; i++)
   {
     MailBoxTable[i].mbox_id = INIT_VAL;
     MailBoxTable[i].next_mbox = NULL;
     MailBoxTable[i].m_slots = NULL;
     MailBoxTable[i].num_slots = 0;
     MailBoxTable[i].slot_size = 0;
   }

   /* Initializes the MailSlot Array. */
   for(i = 0; i < MAXSLOTS; i++)
   {
     MSlot_Table[i].status = EMPTY;
     MSlot_Table[i].next_slot = NULL;
     memset(MSlot_Table[i].message, 0, sizeof(MSlot_Table[i].message));
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

  while(mbox_id_count < MAXMBOX && MailBoxTable[table_pos].mbox_id != INIT_VAL)
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
  int i;
  int table_pos = INIT_VAL;
  m_ptr current = NULL;
  slot_ptr walker = NULL;
  slot_ptr new_slot = NULL;
  mbox_proc_ptr proc_ptr = NULL;

  /* invalid mbox_id value */
  if(mbox_id == -1)
    return -1;

  for(i = 0; i < MAXMBOX; i++)
  {
    if(mbox_id == MailBoxTable[i].mbox_id)
      table_pos = i;
  }

  if(table_pos == INIT_VAL || msg_size > MailBoxTable[table_pos].slot_size || msg_size <= 0)
    return -1;

  current = &MailBoxTable[table_pos];

  for(i = 0; i < MAXSLOTS; i++)
  {
    if(MSlot_Table[i].status == EMPTY)
      new_slot = &MSlot_Table[i];
  }
      

  if(current->m_slots == NULL)
  {
    current->m_slots = new_slot;
    memcpy(current->m_slots->message, msg_ptr, msg_size);
    current->m_slots->m_size = msg_size;
    current->m_slots->status = FULL;
    current->m_slots->next_slot = NULL;

    if(BlockedList != NULL)
    {
      proc_ptr = BlockedList;
      BlockedList = BlockedList->next_mbox_ptr;
      proc_ptr->status = READY;
      unblock_proc(proc_ptr->pid);
    }
  }
  else
  {
    walker = MailBoxTable[table_pos].m_slots;

    for(i = 0; i < current->num_slots - 1; i++)
    {
      if(walker->next_slot == NULL)
      {
        walker->next_slot = new_slot;
        walker = walker->next_slot;
        memcpy(walker->message, msg_ptr, msg_size);
        walker->m_size = msg_size;
        walker->status = FULL;
        walker->next_slot = NULL;
        i = current->num_slots;
      }
      
      walker = walker->next_slot;
    }

    if(i == current->num_slots - 1)//may cause a bug since loop will not start at 1.
      block_me(11);

  }

  return 0;
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
  int i;
  int table_pos = INIT_VAL;
  int message_size = 0;
  m_ptr mail_box = NULL;



  for(i = 0; i < MAXMBOX; i++)
  {
     if(mbox_id == MailBoxTable[i].mbox_id)
       table_pos = i;
  }

  if(table_pos == INIT_VAL)
    return -1;
  
  mail_box = &MailBoxTable[table_pos];

  while(mail_box->m_slots == NULL)
  {
    for(i = 0; i < MAXPROC; i++)
    {
      if(MboxProcTable[i].pid == INIT_VAL)
      {
        table_pos = i;
        i = MAXPROC;
      }
    }

    MboxProcTable[table_pos].pid = getpid();
    MboxProcTable[table_pos].status = BLOCKED;
    BlkList_Insert(MboxProcTable[table_pos].pid);
    block_me(11);
  }


  memcpy(msg_ptr, mail_box->m_slots->message, msg_size);
  message_size = mail_box->m_slots->m_size;

  return message_size;
} /* MboxReceive */

void BlkList_Insert(int pid)
{
  int i;
  mbox_proc_ptr proc_ptr = NULL;
  mbox_proc_ptr walker = NULL;

  for(i = 0; i < MAXPROC; i++)
  {
    if(MboxProcTable[i].pid == pid)
    {
      proc_ptr = &MboxProcTable[i];
      i = MAXPROC;
    }
  }

  if(BlockedList == NULL)
    BlockedList = proc_ptr;
  else
  {
    walker = BlockedList;
    while(walker->next_mbox_ptr != NULL)
      walker = walker->next_mbox_ptr;
    walker->next_mbox_ptr = proc_ptr;
  }
}
