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
int MboxRelease(int mbox_id);
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size);
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size);
int ProcTable_Insert(int pid);
void Slot_Remove(m_ptr mailbox);
void BlkList_Remove();
void BlkList_Insert(int pid);
void BlkList_Delete(int pid);
void block_proc(int mbox_id);

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
     MboxProcTable[i].status = INIT_VAL;
   }

   /* Initializes the MailBox Array */
   for(i = 0; i < MAXMBOX; i++)
   {
     MailBoxTable[i].mbox_id = INIT_VAL;
     MailBoxTable[i].next_mbox = NULL;
     MailBoxTable[i].m_slots = NULL;
     MailBoxTable[i].num_slots = 0;
     MailBoxTable[i].slot_size = 0;
     MailBoxTable[i].status = INIT_VAL;
   }

   /* Initializes the MailSlot Array. */
   for(i = 0; i < MAXSLOTS; i++)
   {
     MSlot_Table[i].status = EMPTY;
     MSlot_Table[i].next_slot = NULL;
     memset(MSlot_Table[i].message, 0, sizeof(MSlot_Table[i].message));
     MSlot_Table[i].m_size = 0;
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
  /*
   * Verifies calling process is in Kernel Mode.
   */
  check_kernel_mode("MboxCreate");

  /*
   * initializes local variables.
   */
  int table_pos = next_mbox_id % MAXMBOX/*MAXPROC*/;
  int mbox_id_count = 0;

  /*
   * Verifies that parameter values are not invalid.
   */
  if(slot_size < 0 || slot_size > MAX_MESSAGE || slots < 0)
    return -1;

  /*
   * generates a new mbox_id which is not being used by another mailbox in the
   * mailbox table. Neither the ID nor the mail box is being utilized elswehre
   * in the mailbox table.
   */
  while(mbox_id_count < MAXMBOX && MailBoxTable[table_pos].mbox_id != INIT_VAL)
  {
    next_mbox_id++;
    table_pos = next_mbox_id % MAXMBOX;
    mbox_id_count++;
  }

  /*
   * Verifies a unique mailbox entry was found in the mailbox table.
   */
  if(mbox_id_count >= MAXMBOX/*MAXPROC*/)
    return -1;

  /*
   * Adds values to selected mailbox entry.
   */
  MailBoxTable[table_pos].mbox_id = next_mbox_id++;
  MailBoxTable[table_pos].num_slots = slots;
  MailBoxTable[table_pos].slot_size = slot_size;

  return MailBoxTable[table_pos].mbox_id;

} /* MboxCreate */

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - Frees a mailbox and its mail slots based on the provided mail 
             box ID.
   Parameters - mbox_id which is the unique mail box ID that is to be freed.
   Returns - -1 to indicate there is no mail box corresponding with the 
             mbox_id, -3 if the calling process was zapped while this function
             went through execution, and 0 if function executed successfully. 
   Side Effects - A Mailbox is freed and its messages destroyed. 
   ----------------------------------------------------------------------- */
int MboxRelease(int mbox_id)
{
  /*
   * Verifies calling process is in Kernel Mode.
   */
  check_kernel_mode("MboxRelease");

  /*
   * Initalize local variables.
   */
  int i;
  m_ptr current = NULL;
  slot_ptr walker = NULL;
  mbox_proc_ptr temp = NULL;

  /*
   * find the specific mailbox to be released based on the mbox_id parameter.
   */
  for(i = 0; i < MAXMBOX; i++)
  {
    if(MailBoxTable[i].mbox_id == mbox_id)
      current = &MailBoxTable[i];
  }

  /*
   * If current is still NULL, that means the mbox_id was not found.
   */
  if(current == NULL)
    return -1;

  walker = current->m_slots;

  /*
   * If there are active mail slots in the mail box to be released, this clears
   * the mail slots. 
   */
  if(walker != NULL)
  {
    for(i = 0; i < MAXSLOTS; i++)
    {
      if(MSlot_Table[i].mbox_id == mbox_id)
      {
        walker->m_size = 0;
        memset(walker->message, 0, sizeof(walker->message));
        walker->next_slot = NULL;
        walker->status = EMPTY;
        walker = current->m_slots;
      }
    }
  }

  /*
   * clears out the values held by the mailbox in the mailbox table.
   */
  current->mbox_id = INIT_VAL;
  current->next_mbox = NULL;
  current->m_slots = NULL;
  current->num_slots = 0;
  current->slot_size = 0;
  current->status = RELEASED;
  
  /*
   * Unblocks all blocked processes that are blocked on a send or receive for
   * the released mailbox.
   */
  while(BlockedList != NULL)
  {
    temp = BlockedList;

    while(temp != NULL)
    {
      if(temp->mbox_id == mbox_id)
        BlkList_Delete(temp->pid);
      temp = temp->next_mbox_ptr;
    }
  }

  /*
   * Checks if processes was zapped during execution. If it was, it returns -3.
   * If it was not, 0 is returned.
   */
  if(is_zapped())
    return -3;
  else
    return 0;
}/* MboxRelease */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args, and -3 if the mailbox
             was released while the process was blocked.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
  /*
   * Checks if calling process is in kernel mode.
   */
  check_kernel_mode("MboxSend");

  /*
   * Initializes local variables and pointers.
   */
  int i;
  int table_pos = INIT_VAL;
  int counter = 0;
  m_ptr current = NULL;
  slot_ptr walker = NULL;
  slot_ptr new_slot = NULL;

  /* 
   * Checks if mbox_id parameter is an invalid mbox_id value. 
   */
  if(mbox_id == -1)
    return -1;

  /*
   * Finds mailbox in the mailbox table that corresponds with the mbox_id
   * provided as a parameter.
   */
  for(i = 0; i < MAXMBOX; i++)
  {
    if(mbox_id == MailBoxTable[i].mbox_id)
      table_pos = i;
  }

  /*
   * Checks if any of the parameters are invalid.
   */
  if(table_pos == INIT_VAL || msg_size > MailBoxTable[table_pos].slot_size || msg_size < 0)
    return -1;

  current = &MailBoxTable[table_pos];

  /*
   * finds an empty entry in the mail slot table and sets it as the new slot
   * for the current mailbox.
   */
  for(i = 0; i < MAXSLOTS; i++)
  {
    if(MSlot_Table[i].status == EMPTY)
    {
      new_slot = &MSlot_Table[i];
      i = MAXSLOTS;
    }
  }
  
  /*
   * Copies message from parameter msg_ptr to the newly created mail slot.
   * Checks first if the mail box is empty, or if the newly created mail slot
   * should be appendended to already existent slot list.
   */
  if(current->m_slots == NULL)
  {
    current->m_slots = new_slot;
    current->m_slots->mbox_id = mbox_id;
    memcpy(current->m_slots->message,msg_ptr, msg_size);
    current->m_slots->m_size = msg_size;
    current->m_slots->status = FULL;
    current->m_slots->next_slot = NULL;
  }
  else
  {
    walker = MailBoxTable[table_pos].m_slots;

    while(walker->next_slot != NULL)
      walker = walker->next_slot;

    walker->next_slot = new_slot;
    walker = walker->next_slot;
    walker->mbox_id = mbox_id;
    memcpy(walker->message, msg_ptr, msg_size);
    walker->m_size = msg_size;
    walker->status = FULL;
    walker->next_slot = NULL;
  }

  walker = current->m_slots;

  /*
   * iterates walker through entire slot list of the current mailbox. Checks
   * how many slots are present in the mailbox.
   */
  while(walker != NULL)
  {
    walker = walker->next_slot;
    counter++;
  }

  /*
   * Checks if mailbox is full. If mailbox slots are full, blocks calling 
   * process until a message slot has been freed from the mail box. If 
   * mailbox is released while the calling process was blocked, -3 is 
   * returned.
   */
  if(counter > current->num_slots)
  {
    block_proc(mbox_id);

    if(current->status == RELEASED)
      return -3;
  }

  /*
   * Checks if any processes are blocked on a receive. If they are, it unblocks
   * them.
   */
  if(BlockedList != NULL)
    BlkList_Remove();

  return 0;
} /* MboxSend */

/* ------------------------------------------------------------------------
    Name - MboxCondSend
    Purpose - Put a message into a slot for the indicated mailbox.
              Return if no slot available.
    Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
    Returns - zero if successful, -1 if invalid args, -2 if the mailbox 
                  is full and -3 if the mailbox was released while the 
                  process was blocked.
    Side Effects - none.
    ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
  /*
   *Checks if calling process is in Kernel Mode.
   */
  check_kernel_mode("MboxSend");

  /*
   * Initializes local variables and pointers.
   */
  int i;
  int table_pos = INIT_VAL;
  int counter = 0;
  m_ptr current = NULL;
  slot_ptr walker = NULL;
  slot_ptr new_slot = NULL;

  /* invalid mbox_id value */
  if(mbox_id == -1)
    return -1;

  for(i = 0; i < MAXMBOX; i++)
  {
    if(mbox_id == MailBoxTable[i].mbox_id)
      current = &MailBoxTable[i];
  }

  if(current == NULL || msg_size > MailBoxTable[table_pos].slot_size || msg_size < 0)
    return -1;

  /*
   * Checks if mailbox is full. If mailbox slots are not full, selects next
   * empty mail slot to place message passed by msg_ptr. If mailbox slots are
   * full, returns -2;
   */
  walker = current->m_slots;
  counter = 0;

  while(walker != NULL)
  {
    if(walker->status == OPEN)
    {
      new_slot = walker;
      walker = NULL;
    }
    else
    {
      walker = walker->next_slot;
      counter++;
    }
  }

  if(counter == current->num_slots)
    return -2;

  /*
   * Finds an empty entry in the mail slot table and sets it as the new slot of
   * the current mailbox
   */
  if(new_slot == NULL)
  {
    for(i = 0; i < MAXSLOTS; i++)
    {
      if(MSlot_Table[i].status == EMPTY)
      {
        new_slot = &MSlot_Table[i];
        i = MAXSLOTS;
      }
    }
  }

  if(new_slot == NULL)
    return -2;
  
  /*
   * Places message passed by msg_ptr into the next available empty mailslot.
   */
  if(current->m_slots == NULL)
  {
    current->m_slots = new_slot;
    current->m_slots->mbox_id = mbox_id;
    memcpy(current->m_slots->message, msg_ptr, msg_size);
    current->m_slots->m_size = msg_size;
    current->m_slots->status = FULL;
    current->m_slots->next_slot = NULL;

    if(BlockedList != NULL)
      BlkList_Remove();

  }
  else
  {
    walker = MailBoxTable[table_pos].m_slots;

    for(i = 0; i < current->num_slots; i++)
    {
      if(walker->next_slot == NULL)
      {
        walker->next_slot = new_slot;
        walker = walker->next_slot;
        walker->mbox_id = mbox_id;
        memcpy(walker->message, msg_ptr, msg_size);
        walker->m_size = msg_size;
        walker->status = FULL;
        walker->next_slot = NULL;
        i = current->num_slots;
      }
      else
        walker = walker->next_slot;
    }
  }

  /*
   * if calling process was zapped, return -3.
   * if not return 0;
   */
  if(is_zapped())
    return -3;
  else
    return 0;
}/* MboxCondSend */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args, and -3 if
             calling process was zapped while it was blocked.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  /*
   * Checks if calling process is in Kernel Mode.
   */
  check_kernel_mode("MboxReceive");

  /*
   * initializes local variables.
   */
  int i;
  int message_size = 0;
  m_ptr mail_box = NULL;
  slot_ptr current = NULL;

  /*
   * Attempts to find mail box that corresponds with the parameter mbox_id.
   */
  for(i = 0; i < MAXMBOX; i++)
  {
     if(mbox_id == MailBoxTable[i].mbox_id)
       mail_box = &MailBoxTable[i];
  }

  /*
   * Checks if correct mailbox exists on the mailbox table.
   */
  if(mail_box == NULL)
    return -1;

  /*
   * If mailbox slots are empty, it blocks the calling process.
   * if the mailbox was released while the calling process was
   * blocked, then -3 is returned.
   */
  while(mail_box->m_slots == NULL)
  {
    block_proc(mbox_id);

    if(mail_box->status == RELEASED)
      return -3;
  }

  /*
   * Checks to see if the size of the parameter buffer
   * is too small to receive the message held in the
   * mail slot.
   */
  if(mail_box->m_slots->m_size > msg_size)
    return -1;

  /*
   * copies the message from the mail slot to the 
   * receiving buffer. Empties the mail slot, then
   * removes it from the mail slot list.
   */
  current = mail_box->m_slots;
  memcpy(msg_ptr, current->message, msg_size);
  message_size = current->m_size;
  current->status = EMPTY;
  Slot_Remove(mail_box);

  /*
   * unblocks any blocked processes that 
   * were waiting for a free slot on the 
   * mailbox.
   */
  if(BlockedList != NULL)
    BlkList_Remove();

  return message_size;
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxCondReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             return if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args, -2 if the
             mailbox is empty, and -3 if calling process was zapped while it 
             was blocked.
   Side Effects - None
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  /*
   * Checks if calling process is in Kernel Mode.
   */
  check_kernel_mode("MboxCondReceive");

  /*
   * Initializes local variables.
   */
  int i;
  int message_size = 0;
  m_ptr mail_box = NULL;
  slot_ptr current = NULL;

  /*
   * Finds mailbox in mailbox table that corresponds with
   * mail box ID parameter.
   */
  for(i = 0; i < MAXMBOX; i++)
  {
    if(mbox_id == MailBoxTable[i].mbox_id)
      mail_box = &MailBoxTable[i];
  }

  /*
   * Verified mbox_id was valid.
   */
  if(mail_box == NULL)
    return -1;

  /*
   * if mailbox is empty, return -2.
   */
  if(mail_box->m_slots == NULL)
    return -2;

  /*
   * if receiving message buffer is too small to hold
   * the message held in the mail slot, return -1;
   */
  if(mail_box->m_slots->m_size > msg_size)
    return -1;

  /*
   * copies the message held in the mail slot into 
   * the recieving buffer passed as a parameter.
   * Afterwards, the mail slot is emptied and 
   * removed from the mail slot list.
   */
  current = mail_box->m_slots;
  memcpy(msg_ptr, current->message, msg_size);
  message_size = current->m_size;
  current->status = EMPTY;
  Slot_Remove(mail_box);

  /*
   * If any processes are blocked due to the
   * mailbox being full, unblock them.
   */
  if(BlockedList != NULL)
    BlkList_Remove();

  /*
   * checks if the calling process was zapped during
   * execution. If it was, return -3, otherwise return
   * the size of the message copied. 
   */
  if(is_zapped())
    return -3;
  else
    return message_size;
}/* MboxCondReceive */

/* ------------------------------------------------------------------------
   Name - ProcTable_Insert
   Purpose - Helper function which inserts process with pid into the phase 2
             process table, then returns the index of that process.
   Parameters - Integar pid which represents the pid of the process which is
                to be inserted into the phase 2 process table.
   Returns - table_pos which represents the index at which the newly inserted
             process can be found in the process table.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int ProcTable_Insert(int pid)
{
  /*
   * Verifies calling process is in Kernel Mode.
   */
  check_kernel_mode("ProcTable_Insert");

  /*
   * Initializes local variables.
   */
  int i;
  int table_pos = INIT_VAL;

  /*
   * Cycles through phase 2 process table and finds if
   * process with pid is either already on the process
   * or finds an empty entry in the table and fills it.
   * Afterwards, table_pos is filled with the index 
   * position on the process.
   */
  for(i = 0; i < MAXPROC; i++)
  {
    if(MboxProcTable[i].pid == pid)
    {
      table_pos = i;
      i = MAXPROC;
    }
    else if(MboxProcTable[i].pid == INIT_VAL)
    {
      MboxProcTable[i].pid = pid;
      table_pos = i;
      i = MAXPROC;
    }
  }

  return table_pos;
}/* ProcTable_Insert */

/* ------------------------------------------------------------------------
   Name - Slot_Remove
   Purpose - Removes an empty slot from the mail slot list maintained on a 
             mailbox.
   Parameters - m_ptr being a mailbox pointer to the mailbox which will holds the
                slot list.
   Returns - none.
   Side Effects - none.
   ----------------------------------------------------------------------- */
void Slot_Remove(m_ptr mailbox)
{
  /*
   * Verifies calling process is in Kernel Mode.
   */
  check_kernel_mode("Slot_Remove");

  /*
   * Initializes local pointers.
   */
  slot_ptr previous, walker;
  previous = NULL;
  walker = mailbox->m_slots;

  /*
   * Iterates through slot list to find the empty slot. Once found, the slot
   * is removed from the list.
   */
  while(walker != NULL)
  {
    if(walker->status == EMPTY && previous == NULL)
    {
      previous = walker;
      walker = walker->next_slot;
      mailbox->m_slots = walker;
      previous->next_slot = NULL;
    }
    else if(walker->status == EMPTY)
    {
      previous = walker->next_slot;
      walker->next_slot = NULL;
    }
    else
    {
      previous = walker;
      walker = walker->next_slot;
    }
  }
}/* Slot_Remove */

/* ------------------------------------------------------------------------
   Name - BlkList_Remove
   Purpose - Removes item from the top of the blocked list.
   Parameters - none.
   Returns - none.
   Side Effects - none.
   ----------------------------------------------------------------------- */
void BlkList_Remove()
{
  /*
   * Verifies calling process is in Kernel Mode.
   */
  check_kernel_mode("BlkList_Remove");

  /*
   * Initializes local pointer proc_ptr.
   */
  mbox_proc_ptr proc_ptr = NULL;

  /*
   * Sets proc_ptr to the top of the block list, then moves blocked list
   * pointer over to the next item. proc_ptr then clears its next pointer
   * removing it fully from the blocked list. The process is then unblocked.
   */
  proc_ptr = BlockedList;
  BlockedList = BlockedList->next_mbox_ptr;
  proc_ptr->next_mbox_ptr = NULL;
  proc_ptr->status = READY;
  unblock_proc(proc_ptr->pid);
}/* BlkList_Remove */

/* ------------------------------------------------------------------------
   Name - BlkList_Insert
   Purpose - Inserts process with pid into the blocked list.
   Parameters - pid representing the pid of the process to be inserted 
                into the blocked list.
   Returns - none.
   Side Effects - none.
   ----------------------------------------------------------------------- */
void BlkList_Insert(int pid)
{
  /*
   * Verfies calling process is in Kernel Mode.
   */
  check_kernel_mode("BlkList_Insert");

  /*
   * initializes local variable and pointers.
   */
  int i;
  mbox_proc_ptr proc_ptr = NULL;
  mbox_proc_ptr walker = NULL;

  /*
   * Finds the process with process ID of pid in the process table.
   */
  for(i = 0; i < MAXPROC; i++)
  {
    if(MboxProcTable[i].pid == pid)
    {
      proc_ptr = &MboxProcTable[i];
      i = MAXPROC;
    }
  }

  /*
   * If there are no items in the blocked list, simply point the blocked list
   * pointer to the process. Otherwise insert found process into the end of the
   * blocked list.
   */
  if(BlockedList == NULL)
    BlockedList = proc_ptr;
  else
  {
    walker = BlockedList;
    while(walker->next_mbox_ptr != NULL)
      walker = walker->next_mbox_ptr;
    walker->next_mbox_ptr = proc_ptr;
  }
}/* BlkList_Insert */

/* ------------------------------------------------------------------------
   Name - BlkList_Delete
   Purpose - Removes item from anywhere on the blocked list.
   Parameters - pid representing the pid of the process to be removed from
                the blocked list.
   Returns - none.
   Side Effects - none.
   ----------------------------------------------------------------------- */
void BlkList_Delete(int pid)
{
  /*
   * Verifies the calling process is in Kernel Mode.
   */
   check_kernel_mode("BlkList_Delete()");

   /*
    * Initializes the local pointers.
    */
   mbox_proc_ptr walker, previous;
   previous = NULL;
   walker = BlockedList;

   /*
    * finds the pid in the blocked list, and sets previous to the process
    * that comes before it in the blocked list.
    */
   while(walker->pid != pid)
   {
     previous = walker;
     walker = walker->next_mbox_ptr;
   }

   /*
    * Removes the process depending on its position.
    */
   if(previous == NULL)
     BlkList_Remove();
   else
   {
     if(walker->next_mbox_ptr == NULL)
     {
       previous->next_mbox_ptr = NULL;
       walker->status = READY;
       unblock_proc(pid);
     }
     else
     {
       previous->next_mbox_ptr = walker->next_mbox_ptr;
       walker->next_mbox_ptr = NULL;
       walker->status = READY;
       unblock_proc(pid);
     }
   }
}/* BlkList_Delete */

/* ------------------------------------------------------------------------
   Name - block_pro
   Purpose - Goes through required steps to block a process in phase 2. 
             Finds the process' pid, places it on the blocked list, and
             then blocks the process.
   Parameters - mbox_id used so the process can tell which mailbox caused it
                to block.
   Returns - none.
   Side Effects - none.
   ----------------------------------------------------------------------- */
void block_proc(int mbox_id)
{
  /*
   * Verifies the calling process is in Kernel Mode.
   */
  check_kernel_mode("block_proc");

  /*
   * Initializes local variables.
   */
  int table_pos = INIT_VAL;
  int pid = INIT_VAL;

  /*
   * gets the calling process' pid, places it on the blocked list
   * them blocks the process.
   */
  pid = getpid();
  table_pos = ProcTable_Insert(pid);
  MboxProcTable[table_pos].status = BLOCKED;
  MboxProcTable[table_pos].mbox_id = mbox_id;
  BlkList_Insert(MboxProcTable[table_pos].pid);
  block_me(11);
}/* block_proc */
