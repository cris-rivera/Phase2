#define DEBUG2 1
#define INIT_VAL -1

typedef struct MboxProc mbox_proc;
typedef struct mail_slot *slot_ptr;
typedef struct mailbox mail_box;
typedef struct mbox_proc *mbox_proc_ptr;
typedef struct mail_slot new_slot;
typedef struct mailbox *m_ptr; //pointer to a mailbox

struct MboxProc {
  short            pid;
  mbox_proc_ptr    next_mbox_ptr; //used for BlockedList, I think.
};

struct mailbox {
   int              mbox_id;
   mbox_proc_ptr    next_mbox;
   slot_ptr         m_slots;      //pointer to list of mailbox slots
   int              num_slots;   //number of slots mailbox holds
   int              slot_size;  //size of each mailbox slot
   /* other items as needed... */
};

struct mail_slot {
   int       status;
   slot_ptr  next_slot;
   char      *message;
   /* other items as needed... */
};

enum {
  FULL,
  EMPTY
}SLOT_STATUS;

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
