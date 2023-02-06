#include <ginger.h>

void do_list(DIR *dir){
    struct dirent *file = readdir(dir);
    while(file){
        printf("%s\n", file->d_name);
        free(file);
        file = readdir(dir);
    }
}

int main(int argc, const char *argv[])
{
    DIR *dir = NULL;
    int i = 1;
    char cwd[1024] = {0};

    if (argc <= 1){
        getcwd(cwd, sizeof cwd);
        dir = opendir(cwd);
        do_list(dir);
        exit(0);
    }

    while(--argc){
        dir =  opendir(argv[i++]);
        do_list(dir);
    }
    return 0;
}