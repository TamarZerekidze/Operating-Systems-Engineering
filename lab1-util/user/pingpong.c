#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(){
    char t = 't';
    int p1[2], p2[2];
    pipe(p1);
    pipe(p2);
    int pid = fork();
    if (pid == 0){
        close(p1[1]);
        close(p2[0]);
        char buf;
        int n = read(p1[0], &buf, 1);
        if(n != 1){
            fprintf(2, "couldn't read/n");
            close(p1[0]);
            close(p2[1]);
            exit(1);
        }
        fprintf(1, "%d: received ping\n", getpid());
        close(p1[0]);
        n = write(p2[1], &buf, 1);
        if(n != 1){
            fprintf(2, "couldn't write/n");
            close(p2[1]);
            exit(1);
        }
        close(p2[1]);
        exit(0);
    } else {
        close(p1[0]);
        close(p2[1]);
        int n = write(p1[1], &t, 1);
        if(n != 1){
            fprintf(2, "couldn't write/n");
            close(p1[1]);
            close(p2[0]);
            exit(1);
        }
        close(p1[1]);
        char buf;
        n = read(p2[0], &buf, 1);
        if(n != 1){
            fprintf(2, "couldn't read/n");
            close(p2[0]);
            exit(1);
        }
        fprintf(1, "%d: received pong\n", getpid());
        close(p2[0]);
        exit(0);
    }
}