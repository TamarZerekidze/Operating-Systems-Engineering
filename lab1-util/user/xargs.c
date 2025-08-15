#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char const *argv[])
{
	if (argc <= 1)
	{
		fprintf(2, "error\n");
		exit(1);
	}

    char *args[MAXARG];
    char c;
    char buf[512];

	char *command = malloc(strlen(argv[1]) + 1);
	strcpy(command, argv[1]);

	for (int i = 1; i < argc; ++i)
	{
		args[i - 1] = malloc(strlen(argv[i]) + 1);
		strcpy(args[i - 1], argv[i]);
	}

    for (int i = argc - 1; i < MAXARG ; i++)
        args[i] = 0;
    
    memset(buf, 0, sizeof(buf));
    int j = 0;
    while (read(0, &c, 1)) {
        if (c == '\n') {
            if (j != 0) {
                buf[j] = '\0';
                args[argc - 1] = buf;
                j = 0;
                if(fork() == 0){
			        exec(command, args);
			        fprintf(2, "error\n");
			        exit(1);
		        } else {
                    wait((int*)0);
                    }
            }
        } else buf[j++] = c;
    }
    if (j != 0) {
        buf[j] = '\0';
        args[argc - 1] = buf;
        if(fork() == 0){
			exec(command, args);
			fprintf(2, "error\n");
			exit(1);
		}else {
            wait((int*)0);
            }
    }
    exit(0);
}