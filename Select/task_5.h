//=============== MODS ======================================================
#define DEBUG_MOD
#ifdef DEBUG_MOD
#define DEBUG
#else
#define DEBUG if(0)
#endif

//=============== DEFINES ===================================================
#define ASSERT_c( cond, message )                           \
 if (!(cond)) {                                             \
    printf ("[%d] (child) ERROR: ", getpid());              \
    fflush (stdout);                                        \
    printf ("%s, line %d: ", __ASSERT_FUNCTION, __LINE__);  \
    fflush (stdout);                                        \
    printf (message);                                       \
    fflush (stdout);                                        \
    putchar ('\n');                                         \
    abort();                                                \
 }

#define ASSERT_p( cond, message )                           \
 if (!(cond)) {                                             \
    printf ("(parent) ERROR: ");                            \
    fflush (stdout);                                        \
    printf ("%s, line %d: ", __ASSERT_FUNCTION, __LINE__);  \
    fflush (stdout);                                        \
    printf (message);                                       \
    fflush (stdout);                                        \
    putchar ('\n');                                         \
    abort();                                                \
 }

//=============== CONSTS ====================================================
enum SOME_CONSTS
{
    P_BUF_SIZE  =  1111,
    C_BUF_SIZE  =  1024,
    POISON_INT  = -228,
    NO          =  0,
    YES         =  1,
    DONE        =  2,
    MSG_SIZE    =  0,
};

enum ERRORS
{
    HAPPY = 777,

    ERR_FERRY_NULL_PTR = -100,
    ERR_FERRY_BAD_RFD,
    ERR_FERRY_BAD_WFD,
    ERR_FERRY_BAD_BUF,
    ERR_FERRY_BAD_BUFSZ,
    ERR_FERRY_BAD_USED_SPACE,
    ERR_FERRY_BAD_CAN_READ,
    ERR_FERRY_BAD_CAN_WRITE,
    ERR_FERRY_CAN_READ_CANT_WRITE,
};

//=============== STRUCTURES ================================================
typedef struct ferry {
    int rfd;
    int wfd;
    void* buf;
    int bufsz;
    int used_space;
    int can_read;
    int can_write;
} ferry;

struct Msg
{
    long type;
};

//=============== PROTOTYPES ================================================
/*int prepare_pipes (ferry *ferries, int *pipefds, int *rfds_arr, int *wfds_arr, int nForks);*/
int prepare_pipes (ferry *ferries, int *pipefds, int nForks);
int prepare_bufs (ferry *ferries, int nForks);

int parent_close_extr_fds (int *pipefds, int nForks);
int parent (ferry* ferries, int nForks, int msg_id);
int parent_prepare_fd_sets (ferry *ferries, fd_set *rfds, fd_set *wfds, int *fd_max, int nForks);
int parent_rdwr (ferry *ferries, fd_set *rfds, fd_set *wfds, int nFds, int nForks, int msg_id);

//int parent (ferry* ferries, int nForks);
//int parent_rdwr (ferry *ferries, fd_set *rfds, fd_set *wfds, int nFds, int nForks);

int ferry_ok    (ferry *fer);
int ferry_dump  (ferry *fer);
int ferry_read  (ferry *fer);
int ferry_write (ferry *fer);

int child_close_extr_fds (int* pipefds, int nChild, int nForks);
int child (int fd_1, int fd_2);
