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
    int input_d = open ("input", O_RDONLY);
    void *buf = calloc (P_BUF_SIZE, 1);
    int i = 0;

    ferry fer = {input_d, STDOUT_FILENO, buf, P_BUF_SIZE, 0, YES, NO};
    ferry_dump (&fer);

    parent (&fer, 1);

    close (input_d);
    free (buf);
    return 0;
}

//=============== PARENT ====================================================
int parent (ferry* ferries, int nForks)
{
    fd_set rfds_obj, wfds_obj;
    fd_set *rfds = &rfds_obj, *wfds = &wfds_obj;
    int fd_max = -1, nFds = 0;

    while (1) {
        parent_prepare_fd_sets (ferries, rfds, wfds, &fd_max, nForks);
        if (fd_max == -1) break;

        nFds = select (fd_max + 1, rfds, wfds, NULL, NULL);
        parent_rdwr (ferries, rfds, wfds, nFds, nForks);
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

int parent_rdwr (ferry *ferries, fd_set *rfds, fd_set *wfds, int nFds, int nForks)
{
printf ("****** PARENT_RDWR ******\n");
ferry_dump (ferries);

    int nChecked_fds = 0;
    int i = 0;
printf ("nFds = %d\n", nFds);
    while (nChecked_fds < nFds) {
        if (FD_ISSET (ferries[i].rfd, rfds)) {
printf ("TRYING TO READ\n");
printf ("I read %d bytes\n", ferry_read  (&ferries[i]));
            if (ferries[i].can_read == DONE) close (ferries[i].rfd);
            nChecked_fds++;
        }
        if (FD_ISSET (ferries[i].wfd, wfds)) {
printf ("TRYING TO WRITE\n");
printf ("I wrote %d bytes\n", ferry_write (&ferries[i]));
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

int ferry_dump (ferry *fer)
{
    printf ("\n========== DUMP OF FERRY [%p] ==========\n",
            fer);
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












/*  void *buf = calloc (C_BUF_SIZE*1000, 1);
    int fd[2];

    pipe (fd);
    if (!fork()) {
        close (fd[0]);
        fd_set wfds;
        FD_ZERO (&wfds);
        FD_SET  (fd[1], &wfds);
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        if (select (fd[1] + 1, NULL, &wfds, NULL, &tv) > 0) {
            printf ("(child) I can write to pipe!\n");
            strcpy (buf, "Hello, World!");
            printf ("%d\n", write (fd[1], buf, 100000));
        }
        else {
            printf ("(child) I can't write to pipe!\n");
        };

        return 0;
    }

    close (fd[1]);
    //write (fd[1], buf, strlen ("Hello, World!"));
    free (buf);
    sleep(3);
    close (fd[0]);
    printf ("Parent finished\n");
    return 0;*/
