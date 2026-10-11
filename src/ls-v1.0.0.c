/*
* Programming Assignment 02: ls-v1.2.0
* Adds column display (down then across) as the default output
* Usage:
*       $ ./bin/ls
*       $ ./bin/ls -l
*       $ ./bin/ls /home /etc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>

extern int errno;

void do_ls(const char *dir, int long_flag);
void do_ls_long(const char *dir);
void do_ls_columns(const char *dir);
int  get_terminal_width(void);
void mode_to_string(mode_t mode, char *str);
void get_owner(uid_t uid, char *buf, size_t n);
void get_group(gid_t gid, char *buf, size_t n);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", long_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], long_flag);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int long_flag)
{
    if (long_flag)
        do_ls_long(dir);
    else
        do_ls_columns(dir);
}

/* Ask the terminal for its width; fall back to 80 if that is not possible */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return ws.ws_col;
}

/* Default display: columns, filled "down then across" */
void do_ls_columns(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    char **names = NULL;
    int count = 0, capacity = 0;
    size_t maxlen = 0;

    /* Step 1: read every filename into a dynamic array, track the longest */
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity)
        {
            capacity = (capacity == 0) ? 16 : capacity * 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc failed");
                break;
            }
            names = tmp;
        }

        names[count] = strdup(entry->d_name);
        if (names[count] == NULL)
        {
            perror("strdup failed");
            break;
        }

        size_t len = strlen(entry->d_name);
        if (len > maxlen)
            maxlen = len;
        count++;
    }
    if (errno != 0)
        perror("readdir failed");
    closedir(dp);

    if (count == 0)
    {
        free(names);
        return;
    }

    /* Step 2: work out the layout */
    int term_width = get_terminal_width();
    int col_width = (int)maxlen + 2;          /* longest name + 2 spaces */
    int cols = term_width / col_width;
    if (cols < 1)     cols = 1;
    if (cols > count) cols = count;
    int rows = (count + cols - 1) / cols;     /* ceiling of count / cols */

    /* Step 3: print row by row, picking one item from each column */
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int idx = r + c * rows;
            if (idx >= count)
                break;
            if (c < cols - 1 && idx + rows < count)
                printf("%-*s", col_width, names[idx]);   /* pad to column width */
            else
                printf("%s", names[idx]);                /* last item in the row */
        }
        putchar('\n');
    }

    /* Step 4: free memory */
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* Convert st_mode into a string like "drwxr-xr-x" */
void mode_to_string(mode_t mode, char *str)
{
    /* file type */
    if (S_ISDIR(mode))       str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '-';

    /* owner, group, other permission bits */
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    /* special bits: setuid, setgid, sticky */
    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

/* uid -> user name (falls back to the number if no name exists) */
void get_owner(uid_t uid, char *buf, size_t n)
{
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        snprintf(buf, n, "%s", pw->pw_name);
    else
        snprintf(buf, n, "%u", (unsigned)uid);
}

/* gid -> group name (falls back to the number if no name exists) */
void get_group(gid_t gid, char *buf, size_t n)
{
    struct group *gr = getgrgid(gid);
    if (gr != NULL)
        snprintf(buf, n, "%s", gr->gr_name);
    else
        snprintf(buf, n, "%u", (unsigned)gid);
}

void do_ls_long(const char *dir)
{
    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];
    char perms[11], owner[64], group[64];
    long long total = 0;
    int w_link = 0, w_user = 0, w_group = 0, w_size = 0;

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* PASS 1: find the widest value in each column and the block total */
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }
        total += st.st_blocks;

        int len = snprintf(NULL, 0, "%lu", (unsigned long)st.st_nlink);
        if (len > w_link) w_link = len;

        get_owner(st.st_uid, owner, sizeof(owner));
        len = strlen(owner);
        if (len > w_user) w_user = len;

        get_group(st.st_gid, group, sizeof(group));
        len = strlen(group);
        if (len > w_group) w_group = len;

        len = snprintf(NULL, 0, "%lld", (long long)st.st_size);
        if (len > w_size) w_size = len;
    }

    printf("total %lld\n", total / 2);   /* st_blocks counts 512-byte units, ls shows 1K */

    /* PASS 2: go back to the start and print each line */
    rewinddir(dp);
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
            continue;

        mode_to_string(st.st_mode, perms);
        get_owner(st.st_uid, owner, sizeof(owner));
        get_group(st.st_gid, group, sizeof(group));

        printf("%s %*lu %-*s %-*s %*lld %.12s %s",
               perms,
               w_link, (unsigned long)st.st_nlink,
               w_user, owner,
               w_group, group,
               w_size, (long long)st.st_size,
               ctime(&st.st_mtime) + 4,
               entry->d_name);

        /* for symbolic links also show where they point */
        if (S_ISLNK(st.st_mode))
        {
            char target[PATH_MAX];
            ssize_t len = readlink(path, target, sizeof(target) - 1);
            if (len != -1)
            {
                target[len] = '\0';
                printf(" -> %s", target);
            }
        }
        putchar('\n');
    }

    closedir(dp);
}
