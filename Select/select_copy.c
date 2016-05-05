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
    ASSERT_p (argc == 3,
              "the program needs two arguments");

    char* end_ptr = NULL;
	int nForks = strtol (argv[2], &end_ptr, 10);
    ASSERT_p (*end_ptr || errno == ERANGE || nForks > 0,
              "bad number of children");

//=============== CREATING_MSG_QUEUE ========================================
    ASSERT_p (creat ("msg_base", 0644) != -1,
              "creat (msg_base) failed");

    int msg_key = ftok ("msg_base", 1);
    ASSERT_p (msg_key != -1,
              "ftok failed");

	int msg_id = msgget (msg_key, IPC_CREAT | 0600);
	ASSERT_p (msg_id != -1,
              "msgget failed\n");

    struct Msg msg;
    msg.type = 0;
    void* msg_ptr = (void*) &msg;

//=============== CREATING_PIPES ============================================
    int i = 0;
    int double_nForks = nForks * 2;
    int *pipefds = (int*) calloc (4 * nForks - 1, sizeof (int));
    ASSERT_p (pipefds,
              "calloc (for pipefds) failed");
    for (i = 0; i < double_nForks; i++) ASSERT_p (pipe (&(pipefds[2 * i])) == 0,
                                                  "pipe failed");

//=============== FORK ======================================================
    int input_d = open (argv[1], O_RDONLY);
    ASSERT_p (input_d != -1,
              "open (input) failed");
    int cpid = fork();
    ASSERT_p (cpid != 1,
                  "fork failed");
    if (!cpid) {
        child_close_extr_fds (pipefds, 0, nForks);
        child (input_d, pipefds[1]);

        msg.type = 1;
        msgsnd (msg_id, msg_ptr, MSG_SIZE, IPC_NOWAIT);
        close (input_d);
        close (pipefds[1]);
        return 0;
    }

    for (i = 1; i < nForks; i++) {
        cpid = fork();
        ASSERT_p (cpid != 1,
                  "fork failed");
        if (!cpid) {
            child_close_extr_fds (pipefds, i, nForks);
            child (pipefds[4 * i - 2], pipefds[4 * i + 1]);

            msg.type = i + 1;
            msgsnd (msg_id, msg_ptr, MSG_SIZE, IPC_NOWAIT);
            close (pipefds[4 * i - 2]);
            close (pipefds[4 * i + 1]);
            return 0;
        }
    }

//=============== PARENT ====================================================
    ferry* ferries = calloc (nForks, sizeof (ferry));
    ASSERT_p (ferries,
              "calloc (for ferries) failed");

    prepare_pipes (ferries, pipefds, nForks);
    prepare_bufs  (ferries, nForks);

    parent_close_extr_fds (pipefds, nForks);
    parent (ferries, nForks, msg_id);

//=============== FINISHING =================================================
    for (i = 0; i < nForks; i++) {
        free (ferries[i].buf);
        ferries[i].bufsz = POISON_INT;
    }

    msgctl(msg_id, IPC_RMID, NULL);
    free (ferries);
    free (pipefds);
    return 0;
}

//=============== PARENT ====================================================
int prepare_pipes (ferry *ferries, int *pipefds, int nForks)
{
    int i = 0;
    int double_nForks = 2 * nForks;
    for (i = 0; i < double_nForks; i++) {
        if (i % 2 == 0) ferries[i / 2].rfd       = pipefds[2 * i];
        else {
            ferries[(i - 1) / 2].wfd = pipefds[2 * i + 1];
            fcntl (pipefds[2 * i + 1], F_SETFL, O_NONBLOCK);
        }
    }

    close (pipefds [4 * nForks - 1]);
    close (pipefds [4 * nForks - 2]);
    ferries[nForks - 1].wfd = STDOUT_FILENO;

    return 0;
}

int prepare_bufs (ferry *ferries, int nForks)
{
    int i = 0;
    for (i = 0; i < nForks; i++) {
        ferries[i].bufsz = P_BUF_SIZE * (i + 1);
        ferries[i].buf = calloc (ferries[i].bufsz, 1);
        ASSERT_p (ferries[i].buf,
                  "calloc (for ferries[i].buf) failed");
        ferries[i].used_space = 0;
        ferries[i].can_read = 1;
    }

    return 0;
}

int parent_close_extr_fds (int *pipefds, int nForks)
{
    int i = 0, double_nForks = 2 * nForks;
    for (i = 0; i < double_nForks - 1; i++) {
        if (i % 2 == 0) close (pipefds[2 * i + 1]);
        else            close (pipefds[2 * i]);
    }

    return 0;
}

int parent (ferry* ferries, int nForks, int msg_id)
{
    fd_set rfds_obj, wfds_obj;
    fd_set *rfds = &rfds_obj, *wfds = &wfds_obj;
    int fd_max = -1, nFds = 0;

    while (1) {
        parent_prepare_fd_sets (ferries, rfds, wfds, &fd_max, nForks);
        if (fd_max == -1) break;

        nFds = select (fd_max + 1, rfds, wfds, NULL, NULL);
        parent_rdwr (ferries, rfds, wfds, nFds, nForks, msg_id);
    }

    return 0;
}

int parent_prepare_fd_sets (ferry *ferries, fd_set *rfds, fd_set *wfds, int *fd_max, int nForks)
{
    FD_ZERO (rfds);
    FD_ZERO (wfds);
    *fd_max = -1;

    int i = 0;
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
    struct Msg msg;
    msg.type = 0;
    void* msg_ptr = (void*) &msg;

    int nChecked_fds = 0;
    int i = 0;
    while (nChecked_fds < nFds) {
        if (FD_ISSET (ferries[i].rfd, rfds)) {
            if (!ferry_read  (&ferries[i])) {
                ASSERT_p (msgrcv (msg_id, msg_ptr, MSG_SIZE, i + 1, IPC_NOWAIT) != -1,
                          "one of children terminated, but the job was not finished");
                close (ferries[i].rfd);
            }
            nChecked_fds++;
        }
        if (FD_ISSET (ferries[i].wfd, wfds)) {
            ferry_write (&ferries[i]);
            if (ferries[i].can_write == DONE
                && ferries[i].wfd != STDOUT_FILENO) close (ferries[i].wfd);
            nChecked_fds++;
        }
        i++;
    }

    return 0;
}

//=============== FERRY =====================================================
int ferry_read  (ferry *ferry_ptr)
{
    int   rfd        = ferry_ptr -> rfd;
    void* buf        = ferry_ptr -> buf;
    int   used_space = ferry_ptr -> used_space;
    int   bufsz      = ferry_ptr -> bufsz;

    int res = read (rfd, &buf[used_space], bufsz - used_space);
    ferry_ptr -> used_space += res;

    if (res + used_space == bufsz) ferry_ptr -> can_read = NO;
    else if (!res) ferry_ptr -> can_read = DONE;
    ferry_ptr -> can_write = YES;
    return res;
}

int ferry_write  (ferry *ferry_ptr)
{
    int   wfd        = ferry_ptr -> wfd;
    void* buf        = ferry_ptr -> buf;
    int   used_place = ferry_ptr -> used_space;
    //int   bufsz      = ferry_ptr -> bufsz;

    int res = 0;
    int nBytes = used_place;
    while (1) {
        res = write (wfd, buf, nBytes);
        if (res == -1) nBytes /= 2;
        else break;
    }

    if (res != used_place) {
        int i = 0;
        for (i = res; i < used_place; i++) ((char*)buf)[i - res] = ((char*)buf)[i];
        ferry_ptr -> can_read = YES;
    }
    else if (ferry_ptr -> can_read == DONE) ferry_ptr -> can_write = DONE;
         else {
             ferry_ptr -> can_write = NO;
             ferry_ptr -> can_read  = YES;
         }

    ferry_ptr -> used_space -= res;
    return res;
}

//=============== CHILD =====================================================
int child_close_extr_fds (int* pipefds, int nChild, int nForks)
{
    int i = 0;
    int limit_1 = nChild * 4 - 2;
    int limit_2 = nChild * 4 + 1;
    int limit_3 = nForks * 4 + 1;

    if (nChild != 0) {
        for (i = 0; i < limit_1; i++) close (pipefds[i]);
        close (pipefds[limit_1 + 1]);
    }
    close (pipefds[limit_1 + 2]);

    if (nChild != nForks -1) for (i = limit_2 + 1; i < limit_3; i++) close (pipefds[i]);
    return 0;
}

int child (int fd_prev, int fd_next)
{
    void* buf = calloc (C_BUF_SIZE, 1);
    ASSERT_c (buf,
              "calloc (for buf) failed");
    int nBytes = -1;
    while (nBytes) {
        nBytes = read (fd_prev, buf, C_BUF_SIZE);
        write (fd_next, buf, nBytes);
    }

    free (buf);
    return 0;
}
