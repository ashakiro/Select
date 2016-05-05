#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include "task_5.h"

int main (int argc, char** argv)
{
//=============== CHECKING_MAIN_ARGS ========================================
    ASSERT_p (argc == 3, "the program needs two arguments");

    char* end_ptr = NULL;
	int nForks = strtol (argv[2], &end_ptr, 10);
    ASSERT_p (*end_ptr || errno == ERANGE || nForks > 0, "bad number of children");

//=============== CREATING_MSG_QUEUE ========================================
    ASSERT_p (creat ("msg_base", 0644) != -1, "creat (msg_base) failed");

    int msg_key = ftok ("msg_base", 1);
    ASSERT_p (msg_key != -1, "ftok failed");

	int msg_id = msgget (msg_key, IPC_CREAT | 0600);
	ASSERT_p (msg_id != -1, "msgget failed\n");

    struct Msg msg;
    msg.type = 0;
    void* msg_ptr = (void*) &msg;

//=============== CREATING_PIPES ============================================
    int i = 0;
    int double_nForks = nForks * 2;
    int *pipefds = (int*) calloc (4 * nForks - 1, sizeof (int));
    ASSERT_p (pipefds, "calloc (for pipefds) failed");
    for (i = 0; i < double_nForks; i++) ASSERT_p (pipe (&(pipefds[2 * i])) == 0, "pipe failed");

//=============== FORK ======================================================
    int input_d = open (argv[1], O_RDONLY);
    ASSERT_p (input_d != -1, "open (input) failed");

    int cpid = fork();
    ASSERT_p (cpid != 1, "fork failed");
    if (!cpid) {
        child_close_extr_fds (pipefds, 0, nForks);
        child (input_d, pipefds[1]);

        msg.type = 1;
        ASSERT_p (msgsnd (msg_id, msg_ptr, MSG_SIZE, IPC_NOWAIT) != -1, "msgsnd failed");
        ASSERT_p (close (input_d)    != -1, "close failed");
        ASSERT_p (close (pipefds[1]) != -1, "close failed");
        return 0;
    }

    for (i = 1; i < nForks; i++) {
        cpid = fork();
        ASSERT_p (cpid != 1, "fork failed");
        if (!cpid) {
            child_close_extr_fds (pipefds, i, nForks);
            child (pipefds[4 * i - 2], pipefds[4 * i + 1]);

            msg.type = i + 1;
            ASSERT_p (msgsnd (msg_id, msg_ptr, MSG_SIZE, IPC_NOWAIT) != -1, "msgsnd failed");
            ASSERT_p (close (pipefds[4 * i - 2]) != -1, "close failed");
            ASSERT_p (close (pipefds[4 * i + 1]) != -1, "close failed");
            return 0;
        }
    }

//=============== PARENT ====================================================
    ferry* ferries = calloc (nForks, sizeof (ferry));
    ASSERT_p (ferries, "calloc failed");

    prepare_pipes (ferries, pipefds, nForks);
    prepare_bufs  (ferries, nForks);

    parent_close_extr_fds (pipefds, nForks);
    parent (ferries, nForks, msg_id);

//=============== FINISHING =================================================
    for (i = 0; i < nForks; i++) {
        free (ferries[i].buf);
        ferries[i].bufsz = POISON_INT;
    }

    ASSERT_p (msgctl(msg_id, IPC_RMID, NULL) != -1, "msgctl failed");
    free (ferries);
    free (pipefds);
    return 0;
}

//=============== PARENT ====================================================
int prepare_pipes (ferry *ferries, int *pipefds, int nForks)
{
    ASSERT_p (ferries,      "bad ferries");
    int i = 0;
    for (i = 0; i < nForks; i++) ASSERT_p (ferry_ok (&(ferries[i])), "bad ferry found in array");
    ASSERT_p (pipefds,      "bad pipefds");
    ASSERT_p (nForks > 0,   "bad nForks");

    int double_nForks = 2 * nForks;
    for (i = 0; i < double_nForks; i++) {
        if (i % 2 == 0) ferries[i / 2].rfd = pipefds[2 * i];
        else {
            ferries[(i - 1) / 2].wfd = pipefds[2 * i + 1];
            ASSERT_p (fcntl (pipefds[2 * i + 1], F_SETFL, O_NONBLOCK) != -1, "fcntl failed");
        }
    }

    ASSERT_p (close (pipefds [4 * nForks - 1]) != -1, "close failed");
    ASSERT_p (close (pipefds [4 * nForks - 2]) != -1, "close failed");
    ferries[nForks - 1].wfd = STDOUT_FILENO;

    return 0;
}

int prepare_bufs (ferry *ferries, int nForks)
{
    ASSERT_p (ferries,      "bad ferries");
    int i = 0;
    for (i = 0; i < nForks; i++) ASSERT_p (ferry_ok (&(ferries[i])), "bad ferry found in array");
    ASSERT_p (nForks > 0,   "bad nForks");

    for (i = 0; i < nForks; i++) {
        ferries[i].bufsz = P_BUF_SIZE * (i + 1);
        ferries[i].buf = calloc (ferries[i].bufsz, 1);
        ASSERT_p (ferries[i].buf,
                  "calloc failed");
        ferries[i].used_space = 0;
        ferries[i].can_read = 1;
    }

    return 0;
}

int parent_close_extr_fds (int *pipefds, int nForks)
{
    ASSERT_p (pipefds,      "bad pipefds");
    ASSERT_p (nForks > 0,   "bad nForks");

    int i = 0, double_nForks = 2 * nForks;
    for (i = 0; i < double_nForks - 1; i++) {
        if (i % 2 == 0) close (pipefds[2 * i + 1]);
        else            close (pipefds[2 * i]);
    }

    return 0;
}

int parent (ferry* ferries, int nForks, int msg_id)
{
    ASSERT_p (ferries,      "bad ferries");
    int i = 0;
    for (i = 0; i < nForks; i++) ASSERT_p (ferry_ok (&(ferries[i])), "bad ferry found in array");
    ASSERT_p (nForks > 0,   "bad nForks");
    ASSERT_p (msg_id >= 0,  "bad msg_id");

    fd_set rfds_obj, wfds_obj;
    fd_set *rfds = &rfds_obj, *wfds = &wfds_obj;
    int fd_max = -1, nFds = 0;

    while (1) {
        parent_prepare_fd_sets (ferries, rfds, wfds, &fd_max, nForks);
        if (fd_max == -1) break;

        nFds = select (fd_max + 1, rfds, wfds, NULL, NULL);
        ASSERT_p (nFds != -1, "select failed");
        parent_rdwr (ferries, rfds, wfds, nFds, nForks, msg_id);
    }

    return 0;
}

int parent_prepare_fd_sets (ferry *ferries, fd_set *rfds, fd_set *wfds, int *fd_max, int nForks)
{
    ASSERT_p (ferries,      "bad ferries");
    int i = 0;
    for (i = 0; i < nForks; i++) ASSERT_p (ferry_ok (&(ferries[i])), "bad ferry found in array");
    ASSERT_p (rfds,         "bad rfds");
    ASSERT_p (wfds,         "bad wfds");
    ASSERT_p (fd_max,       "bad fd_max");
    ASSERT_p (nForks > 0,   "bad nForks");

    FD_ZERO (rfds);
    FD_ZERO (wfds);
    *fd_max = -1;

    for (i = 0; i < nForks; i++) {
        if (ferries[i].can_read == YES) {
            FD_SET (ferries[i].rfd, rfds);
            if (ferries[i].rfd > *fd_max) *fd_max = ferries[i].rfd;
        }
        if (ferries[i].can_write == YES) {
            FD_SET (ferries[i].wfd, wfds);
            if (ferries[i].wfd > *fd_max) *fd_max = ferries[i].wfd;
        }
    }

    return 0;
}

int parent_rdwr (ferry *ferries, fd_set *rfds, fd_set *wfds, int nFds, int nForks, int msg_id)
{
    ASSERT_p (ferries,      "bad ferries");
    int i = 0;
    for (i = 0; i < nForks; i++) ASSERT_p (ferry_ok (&(ferries[i])), "bad ferry found in array");
    ASSERT_p (rfds,         "bad rfds");
    ASSERT_p (wfds,         "bad wfds");
    ASSERT_p (nFds >= 0 && nFds <= 2 * nForks,
                            "bad nFds");
    ASSERT_p (nForks > 0,   "bad nForks");
    ASSERT_p (msg_id >= 0,  "bad msg_id");


    struct Msg msg;
    msg.type = 0;
    void* msg_ptr = (void*) &msg;

    int nChecked_fds = 0;
    i = 0;
    while (nChecked_fds < nFds) {
        if (FD_ISSET (ferries[i].rfd, rfds)) {
            if (!ferry_read  (&ferries[i])) {
                ASSERT_p (msgrcv (msg_id, msg_ptr, MSG_SIZE, i + 1, IPC_NOWAIT) != -1,
                          "one of children terminated, but the job was not finished");
                ASSERT_p (close (ferries[i].rfd) != -1, "close failed");
            }
            nChecked_fds++;
        }
        if (FD_ISSET (ferries[i].wfd, wfds)) {
            ferry_write (&ferries[i]);
            if (ferries[i].can_write == DONE
                && ferries[i].wfd != STDOUT_FILENO) ASSERT_p (close (ferries[i].wfd) != -1,
                                                              "close failed");
            nChecked_fds++;
        }
        i++;
    }

    return 0;
}

//=============== FERRY =====================================================
int ferry_ok (ferry *fer)
{
    if (!fer)           return ERR_FERRY_NULL_PTR;
    if (fer -> rfd < 0) return ERR_FERRY_BAD_RFD;
    if (fer -> wfd < 0) return ERR_FERRY_BAD_WFD;
    if (!(fer -> buf))  return ERR_FERRY_BAD_BUF;

    int bufsz       = fer -> bufsz;
    int used_space  = fer -> used_space;
    int can_read    = fer -> can_read;
    int can_write   = fer -> can_write;

    if (bufsz <= 0)     return ERR_FERRY_BAD_BUFSZ;
    if (used_space < 0 || used_space > bufsz)
                        return ERR_FERRY_BAD_USED_SPACE;
    if (can_read != YES && can_read != NO && can_read != DONE)
                        return ERR_FERRY_BAD_CAN_READ;
    if (can_write != YES && can_write != NO && can_write != DONE)
                        return ERR_FERRY_BAD_CAN_WRITE;
    if (can_read != DONE && can_write == DONE)
                        return ERR_FERRY_CAN_READ_CANT_WRITE;

    return HAPPY;
}

int ferry_dump (ferry *fer)
{
    printf ("\n========== DUMP OF FERRY [%p] ==========\n",
            fer);
    if (ferry_ok (fer) == HAPPY) printf ("Object is ok");
    else                         printf ("object is NOT ok");
    printf ("rfd\t   = %d\nwfd\t   = %d\n\n",
            fer -> rfd, fer -> wfd);
    printf ("buf\t   = [%p]\nbufsz\t   = %d\nused_space = %d\n",
            fer -> buf, fer -> bufsz, fer -> used_space);
    write  (STDOUT_FILENO, fer -> buf, fer -> used_space);
    printf ("\n");
    printf ("can_read   = %d\ncan_write  = %d\n",
            fer -> can_read, fer -> can_write);
    printf ("====================================================\n\n");

    return 0;
}

int ferry_read  (ferry *fer)
{
    ASSERT_p (ferry_ok (fer) == HAPPY, "bad ferry");

    int   rfd        = fer -> rfd;
    void* buf        = fer -> buf;
    int   used_space = fer -> used_space;
    int   bufsz      = fer -> bufsz;

    int res = read (rfd, &buf[used_space], bufsz - used_space);
    ASSERT_p (res != -1, "read failed");

    fer -> used_space += res;
    if (res + used_space == bufsz) fer -> can_read = NO;
    else if (!res) fer -> can_read = DONE;
    fer -> can_write = YES;

    return res;
}

int ferry_write  (ferry *fer)
{
    ASSERT_p (ferry_ok (fer) == HAPPY, "bad ferry");

    int   wfd        = fer -> wfd;
    void* buf        = fer -> buf;
    int   used_place = fer -> used_space;

    int res = 0;
    int nBytes = used_place;
    while (1) {
        res = write (wfd, buf, nBytes);
        if (res == -1) {
            nBytes /= 2;
            ASSERT_p (nBytes > 0, "nBytes became not positive");
        }
        else break;
    }

    if (res != used_place) {
        int i = 0;
        for (i = res; i < used_place; i++) ((char*)buf)[i - res] = ((char*)buf)[i];
        fer -> can_read = YES;
    }
    else if (fer -> can_read == DONE) fer -> can_write = DONE;
         else {
             fer -> can_write = NO;
             fer -> can_read  = YES;
         }

    fer -> used_space -= res;
    return res;
}

//=============== CHILD =====================================================
int child_close_extr_fds (int* pipefds, int nChild, int nForks)
{
    ASSERT_c (pipefds,      "bad pipefds");
    ASSERT_c (nChild >= 0,  "bad nChild" );
    ASSERT_c (nForks > 0,   "bad nForks" );

    int i = 0;
    int limit_1 = nChild * 4 - 2;
    int limit_2 = nChild * 4 + 1;
    int limit_3 = nForks * 4 + 1;

    if (nChild != 0) {
        for (i = 0; i < limit_1; i++) ASSERT_c (close (pipefds[i]) != -1, "close failed");
        ASSERT_c (close (pipefds[limit_1 + 1]) != -1, "close failed");
    }
    ASSERT_c (close (pipefds[limit_1 + 2]) != -1, "close failed");

    if (nChild != nForks -1) for (i = limit_2 + 1; i < limit_3; i++) close (pipefds[i]);
    return 0;
}

int child (int fd_prev, int fd_next)
{
    ASSERT_c (fd_prev >= 0, "bad fd_prev");
    ASSERT_c (fd_next >= 0, "bad fd_next");

    void* buf = calloc (C_BUF_SIZE, 1);
    ASSERT_c (buf, "calloc failed");

    int nBytes = -1;
    while (nBytes) {
        nBytes = read (fd_prev, buf, C_BUF_SIZE);
        ASSERT_c (nBytes != -1, "read failed");
        ASSERT_c (write (fd_next, buf, nBytes) != -1, "write failed");
    }

    free (buf);
    return 0;
}
