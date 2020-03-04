#define DEBUG2 1
#define INIT_VAL -1

typedef struct mbox_proc mbox_proc;
typedef struct mail_slot *slot_ptr;
typedef struct mailbox mail_box;
typedef struct mbox_proc *mbox_proc_ptr;
typedef struct mail_slot new_slot;
typedef struct mailbox *m_ptr; //pointer to a mailbox

struct mbox_proc {
  short            pid;
  int              mbox_id; //to keep track of which mailbox caused the process  to block.
  int              status;
  mbox_proc_ptr    next_mbox_ptr; //used for BlockedList, I think.
};

struct mailbox {
   int              mbox_id;
   mbox_proc_ptr    next_mbox;
   slot_ptr         m_slots;      //pointer to list of mailbox slots
   int              num_slots;   //number of slots mailbox holds
   int              slot_size;  //size of each mailbox slot
   int              status;     //to tell if the mailbox has been/is being released.
   /* other items as needed... */
};

struct mail_slot {
   int       status;
   int       mbox_id;
   slot_ptr  next_slot;
   char      message[MAX_MESSAGE];
   int       m_size; //size of the message copied
   /* other items as needed... */
};

enum {
  UNINIT,
  INIT,
  RELEASED
}MBOX_RELEASE_STATUS;

enum {
  CLOSED,
  OPEN
}MBOX_STATUS;

enum {
  FULL,   //slot is part of a mailbox and contains a message
  OPEN_SLOT,   //slot is part of a mailbox and does not contain a message
  EMPTY   //slot is not part of a mailbox
}SLOT_STATUS;

enum {
  BLOCKED,
  READY,
  RUNNING
}PROC_STATUS;

struct psr_bits {
    unsigned int cur_mode:1;
    unsigned int cur_int_enable:1;
    unsigned int prev_mode:1;
    unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};
