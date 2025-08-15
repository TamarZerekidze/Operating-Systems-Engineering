#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int parent[2]) {
    close(parent[1]);
    int pr;
    int n = read(parent[0],&pr,4);
    if(n == 0) {
        close(parent[0]);
        exit(0);
    } else if (n != 4) {
        fprintf(2, " couldn't read\n");
        close(parent[0]);
        wait((int *) 0);
        exit(1);
    }
    fprintf(1, "prime %d\n", pr);
    int child[2];
    pipe(child);
    int pid = fork();
    if (pid != 0) {
        close(child[0]);
        while(1){
            int k;
            n = read(parent[0],&k,4);
            if(n == 0) {
                close(parent[0]);
                break;
            } else if (n != 4) {
                fprintf(2, " couldn't read\n");
                close(parent[0]);
                close(child[1]);
                wait((int *) 0);
                exit(1);
            }
            if (k % pr) {
                n = write(child[1], &k, 4); 
                if (n != 4){
                    fprintf(2, " couldn't write\n");
                    close(parent[0]);
                    close(child[1]);
                    wait((int *) 0);
                    exit(1);
                }
            }
        }
        close(child[1]);
        wait((int *) 0);
    } else {
        close(parent[0]);
        primes(child);
    }
    exit(0);
}

int main(){
    int p[2];
    pipe(p);
    int pid = fork();
    if (pid != 0) {
        close(p[0]);
        for(int i = 2; i <= 35; i++) {
            int n = write(p[1], &i, 4);
            if (n != 4){
                fprintf(2, " couldn't write\n");
                close(p[1]);
                wait((int *) 0);
                exit(1);
            }
        }
        close(p[1]);
        wait((int*) 0);
        exit(0);
    } else {
        close(p[1]);
        primes(p);
    }
    exit(0);
}