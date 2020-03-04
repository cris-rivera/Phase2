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
int send_count = 0;
int recv_count = 0;

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

int MboxRelease(int mbox_id)
{
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

  /*
   * clears out all the mail slots
   */
  walker = current->m_slots;

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

  /*
   * clears out the values held by the mailbox in the mailbox table.
   */
  current->mbox_id = INIT_VAL;
  current->next_mbox = NULL;
  current->m_slots = NULL;
  current->num_slots = 0;
  current->slot_size = 0;
  current->status = RELEASED;
  
  if(BlockedList != NULL)
  {
    temp = BlockedList;

    while(temp != NULL)
    {
      if(temp->mbox_id == mbox_id)
        BlkList_Delete(temp->pid);
      temp = temp->next_mbox_ptr;
    }
  }

  if(is_zapped())
    return -3;
  else
    return 0;
}

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
  //Checks if calling process is in kernel mode then creates and initializes
  //temporary variables/pointers
  check_kernel_mode("MboxSend");
  //disableInterrupts();

  int i;
  int table_pos = INIT_VAL;
  int proc_table_pos = INIT_VAL;
  int mbox_status = CLOSED;
  int pid = INIT_VAL;
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
      table_pos = i;
  }

  if(table_pos == INIT_VAL || msg_size > MailBoxTable[table_pos].slot_size || msg_size < 0)
    return -1;

  current = &MailBoxTable[table_pos];

  /*
   * finds an empty entry in the mail slot table and sets it as the new slot
   * for the current mailbox. Also assigns this slot the counter value that
   * will correspond with the current process.
   */
  //console("send_count: %d\n", send_count);
  for(i = 0; i < MAXSLOTS; i++)
  {
    if(MSlot_Table[i].status == EMPTY)
    {
      new_slot = &MSlot_Table[i];
      new_slot->m_count = send_count++;
      i = MAXSLOTS;
    }
  }
  
  /*
   * Checks if mailbox is full. If mailbox slots are not full, selects next
   * empty mail slot to place message passed by msg_ptr. If mailbox slots are
   * full, blocks calling process until a free slot appears in the mailbox.
   */
  while(mbox_status == CLOSED)
  {
    walker = current->m_slots;
    counter = 0;

    while(walker != NULL)
    {
      //if(walker->status == OPEN)
      //{
        //new_slot = walker;
        //walker = NULL;
     // }
      //else
      //{
        walker = walker->next_slot;
        counter++;
      //}
    }

    if(counter == current->num_slots)
    {
      pid = getpid();
      proc_table_pos = ProcTable_Insert(pid);
      MboxProcTable[proc_table_pos].status = BLOCKED;
      MboxProcTable[proc_table_pos].mbox_id = mbox_id;
      BlkList_Insert(MboxProcTable[proc_table_pos].pid);
      block_me(11);

      if(current->status == RELEASED)
        return -3;
    }
    /*
    else if(recv_count != new_slot->m_count)
    {
      pid = getpid();
      proc_table_pos = ProcTable_Insert(pid);
      MboxProcTable[proc_table_pos].status = BLOCKED;
      MboxProcTable[proc_table_pos].mbox_id = mbox_id;
      BlkList_Insert(MboxProcTable[proc_table_pos].pid);
      block_me(11);
    }*/
    else
      mbox_status = OPEN;
  }

  /*
   * Finds an empty entry in the mail slot table and sets it as the new slot of
   * the current mailbox
   *
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
  }*/

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

    /*
    if(BlockedList != NULL)
    {
      //console("unblock\n");
      BlkList_Remove();
    }*/

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
        //console("\nmessage sent->%s\n",walker->message);
        walker->m_size = msg_size;
        walker->status = FULL;
        walker->next_slot = NULL;
        i = current->num_slots;

        /*
        if(BlockedList != NULL)
        {
          //console("unblock\n");
          BlkList_Remove();
        }*/
      }
      else
        walker = walker->next_slot;
    }
  }

  if(BlockedList != NULL)
    BlkList_Remove();
  //enableInterrupts();
  return 0;
} /* MboxSend */

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
  //Checks if calling process is in kernel mode then creates and initializes
  //temporary variables/pointers
  check_kernel_mode("MboxSend");
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
      table_pos = i;
  }

  if(table_pos == INIT_VAL || msg_size > MailBoxTable[table_pos].slot_size || msg_size < 0)
    return -1;

  current = &MailBoxTable[table_pos];

  /*
   * Checks if mailbox is full. If mailbox slots are not full, selects next
   * empty mail slot to place message passed by msg_ptr. If mailbox slots are
   * full, blocks calling process until a free slot appears in the mailbox.
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
  if(is_zapped())
    return -3;
  else
    return 0;
}

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
  int pid = -1;
  int count_found = FALSE;
  m_ptr mail_box = NULL;
  slot_ptr current = NULL;
  slot_ptr walker = NULL;



  for(i = 0; i < MAXMBOX; i++)
  {
     if(mbox_id == MailBoxTable[i].mbox_id)
       table_pos = i;
  }

  if(table_pos == INIT_VAL)
    return -1;
  
  mail_box = &MailBoxTable[table_pos];

  /*
  if(mail_box->m_slots != NULL)
    console("not null\n");*/

  while(mail_box->m_slots == NULL)
  {
    pid = getpid();
    table_pos = ProcTable_Insert(pid);
    MboxProcTable[table_pos].status = BLOCKED;
    MboxProcTable[table_pos].mbox_id = mbox_id;
    BlkList_Insert(MboxProcTable[table_pos].pid);
    //console("first block me\n");
    block_me(11);

    if(mail_box->status == RELEASED)
      return -3;
  }

  current = mail_box->m_slots;
  memcpy(msg_ptr, current->message, msg_size);
  message_size = current->m_size;
  current->status = EMPTY;
  Slot_Remove(mail_box);
  recv_count++;

  /*
   * copies message from mail slot to msg_ptr
   */
  /*
  current = mail_box->m_slots;
  walker = current;

  while(count_found == FALSE)
  {
    walker = mail_box->m_slots;
    for(i = 0; i < mail_box->num_slots; i++)
    {
      if(walker != NULL)
      {
        //console("made it into if\n");
        memcpy(msg_ptr, walker->message, msg_size);
        message_size = walker->m_size;
        walker->status = EMPTY;
        Slot_Remove(mail_box);
        ///walker = NULL;
        i = mail_box->num_slots;
        count_found = TRUE;
        recv_count++;
      }
      else if(walker != NULL)
        walker = walker->next_slot;
      else
        i = mail_box->num_slots;
    }

    if(count_found == FALSE)
    {
      pid = getpid();
      table_pos = ProcTable_Insert(pid);
      MboxProcTable[table_pos].status = BLOCKED;
      MboxProcTable[table_pos].mbox_id = mbox_id;
      BlkList_Insert(MboxProcTable[table_pos].pid);
      console("block me in recv reached\n");
      console("count: %d\n", recv_count);
      block_me(11);
    }
  }*/

  /*
   * empties mail slot and removes it from the mail box.
   *
  current->status = EMPTY;
  walker = current;
  current = current->next_slot;
  walker->next_slot = NULL;
  mail_box->m_slots = current;*/

  /* DELETE COMMENTED CODE 
   * have walker point to the last slot in the mailbox list.
   *
  walker = current;
  while(walker->next_slot != NULL)
    walker = walker->next_slot;

  *
   * move mail slot that was just emptied to the end of the mailbox slot list.
   *
  walker->next_slot = current;
  walker = current;
  current = current->next_slot;
  walker->next_slot = NULL;
  mail_box->m_slots = current;*/

  if(BlockedList != NULL && mail_box->m_slots == NULL)
  {
    //console("in remove...\n");
    BlkList_Remove();
    //block_proc(mbox_id);

  }
  if(BlockedList != NULL)
  {
    BlkList_Remove();
    //block_proc(mbox_id);
  }

  return message_size;
} /* MboxReceive */

int ProcTable_Insert(int pid)
{
  int i;
  int table_pos = -1;

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
}

void Slot_Remove(m_ptr mailbox)
{
  slot_ptr previous, walker;
  previous = NULL;
  walker = mailbox->m_slots;

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
}

void BlkList_Remove()
{
  mbox_proc_ptr proc_ptr = NULL;
  proc_ptr = BlockedList;
  BlockedList = BlockedList->next_mbox_ptr;
  proc_ptr->status = READY;
  unblock_proc(proc_ptr->pid);
}

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

void BlkList_Delete(int pid)
{
   check_kernel_mode("BlkList_Delete()");

   mbox_proc_ptr walker, previous;
   previous = NULL;
   walker = BlockedList;

   while(walker->pid != pid)
   {
     previous = walker;
     walker = walker->next_mbox_ptr;
   }

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

void block_proc(int mbox_id)
{
  int table_pos = INIT_VAL;
  int pid = INIT_VAL;
  pid = getpid();
  table_pos = ProcTable_Insert(pid);
  MboxProcTable[table_pos].status = BLOCKED;
  MboxProcTable[table_pos].mbox_id = mbox_id;
  BlkList_Insert(MboxProcTable[table_pos].pid);
  block_me(11);
}
