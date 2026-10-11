/*
* Programming Assignment 02: ls-v1.5.0
* Adds colorized output based on file type (ANSI escape codes)
* Usage:
*       $ ./bin/ls          (down then across, sorted, colored)
*       $ ./bin/ls -l       (long listing, colored names)
*       $ ./bin/ls -x       (across, colored)
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

/* ANSI escape codes: ESC [ attribute m ... text ... ESC [ 0 m */
#define COLOR_RESET    "\033[0m"
#define COLOR_BLUE     "\033[0;34m"
#define COLOR_GREEN    "\033[0;32m"
#define COLOR_RED      "\033[0;31m"
#define COLOR_PINK     "\033[0;35m"
#define COLOR_REVERSE  "\033[7m"

/* which display function should be used */
typedef enum {
    MODE_DEFAULT,      /* down then across */
    MODE_LONG,         /* -l */
    MODE_HORIZONTAL    /* -x */
} display_mode_t;

void do_ls(const char *dir, display_mode_t mode);
void do_ls_long(const char *dir, char **names, int count);
char **read_names(const char *dir, int *count, size_t *maxlen);
void free_names(char **names, int count);
int  compare_names(const void *a, const void *b);
int  has_suffix(const char *s, const char *suffix);
void print_colored(const char *dir, const char *name);
void print_vertical(const char *dir, char **names, int count, size_t maxlen);
void print_horizontal(const char *dir, char **names, int count, size_t maxlen);
int  get_terminal_width(void);
void mode_to_string(mode_t mode, char *str);
void get_owner(uid_t uid, char *buf, size_t n);
void get_group(gid_t gid, char *buf, size_t n);

int main(int argc, char *argv[])
{
    int opt;
    display_mode_t mode = MODE_DEFAULT;

    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
        case 'l':
            mode = MODE_LONG;
            break;
        case 'x':
            mode = MODE_HORIZONTAL;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l | -x] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode);
            puts("");
        }
    }
    return 0;
}

/* Comparison function for qsort: a and b point to elements of the array,
   and each element is itself a char *, so we receive a char ** */
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

/* Read all names, sort them, then call the right display function */
void do_ls(const char *dir, display_mode_t mode)
{
    int count = 0;
    size_t maxlen = 0;
    char **names = read_names(dir, &count, &maxlen);
    if (names == NULL || count == 0)
    {
        free(names);
        return;
    }

    qsort(names, count, sizeof(char *), compare_names);

    if (mode == MODE_LONG)
        do_ls_long(dir, names, count);
    else if (mode == MODE_HORIZONTAL)
        print_horizontal(dir, names, count, maxlen);
    else
        print_vertical(dir, names, count, maxlen);

    free_names(names, count);
}

/* Read every (non-hidden) filename into a dynamic array and track the longest */
char **read_names(const char *dir, int *count, size_t *maxlen)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return NULL;
    }

    char **names = NULL;
    int n = 0, capacity = 0;
    size_t longest = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (n == capacity)
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

        names[n] = strdup(entry->d_name);
        if (names[n] == NULL)
        {
            perror("strdup failed");
            break;
        }

        size_t len = strlen(entry->d_name);
        if (len > longest)
            longest = len;
        n++;
    }
    if (errno != 0)
        perror("readdir failed");
    closedir(dp);

    *count = n;
    *maxlen = longest;
    return names;
}

void free_names(char **names, int count)
{
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* Ask the terminal for its width; fall back to 80 if that is not possible */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return ws.ws_col;
}

/* 1 if string s ends with suffix, 0 otherwise */
int has_suffix(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    if (lx > ls)
        return 0;
    return strcmp(s + ls - lx, suffix) == 0;
}

/* Print one filename in the colour that matches its file type */
void print_colored(const char *dir, const char *name)
{
    char path[PATH_MAX];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &st) == -1)
    {
        printf("%s", name);          /* cannot stat: print it plain */
        return;
    }

    const char *color = NULL;
    mode_t m = st.st_mode;

    if (S_ISLNK(m))
        color = COLOR_PINK;
    else if (S_ISDIR(m))
        color = COLOR_BLUE;
    else if (S_ISCHR(m) || S_ISBLK(m) || S_ISSOCK(m) || S_ISFIFO(m))
        color = COLOR_REVERSE;
    else if (has_suffix(name, ".tar") || has_suffix(name, ".gz") ||
             has_suffix(name, ".zip"))
        color = COLOR_RED;
    else if (m & (S_IXUSR | S_IXGRP | S_IXOTH))
        color = COLOR_GREEN;

    if (color != NULL)
        printf("%s%s%s", color, name, COLOR_RESET);
    else
        printf("%s", name);
}

/* Default display: columns, filled "down then across" */
void print_vertical(const char *dir, char **names, int count, size_t maxlen)
{
    int term_width = get_terminal_width();
    int col_width = (int)maxlen + 2;
    int cols = term_width / col_width;
    if (cols < 1)     cols = 1;
    if (cols > count) cols = count;
    int rows = (count + cols - 1) / cols;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int idx = r + c * rows;
            if (idx >= count)
                break;
            print_colored(dir, names[idx]);
            if (c < cols - 1 && idx + rows < count)
                printf("%*s", col_width - (int)strlen(names[idx]), "");
        }
        putchar('\n');
    }
}

/* -x display: left to right, wrap to a new line when the next name will not fit */
void print_horizontal(const char *dir, char **names, int count, size_t maxlen)
{
    int term_width = get_terminal_width();
    int col_width = (int)maxlen + 2;
    int pos = 0;                          /* current horizontal position */

    for (int i = 0; i < count; i++)
    {
        if (pos > 0 && pos + col_width > term_width)
        {
            putchar('\n');
            pos = 0;
        }
        print_colored(dir, names[i]);
        printf("%*s", col_width - (int)strlen(names[i]), "");
        pos += col_width;
    }
    putchar('\n');
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

/* Long listing: uses the already sorted array of names */
void do_ls_long(const char *dir, char **names, int count)
{
    struct stat st;
    char path[PATH_MAX];
    char perms[11], owner[64], group[64];
    long long total = 0;
    int w_link = 0, w_user = 0, w_group = 0, w_size = 0;

    /* PASS 1: find the widest value in each column and the block total */
    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
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

    /* PASS 2: print each line in sorted order */
    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &st) == -1)
            continue;

        mode_to_string(st.st_mode, perms);
        get_owner(st.st_uid, owner, sizeof(owner));
        get_group(st.st_gid, group, sizeof(group));

        printf("%s %*lu %-*s %-*s %*lld %.12s ",
               perms,
               w_link, (unsigned long)st.st_nlink,
               w_user, owner,
               w_group, group,
               w_size, (long long)st.st_size,
               ctime(&st.st_mtime) + 4);

        print_colored(dir, names[i]);

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
}
