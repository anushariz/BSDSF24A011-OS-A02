# REPORT.md

## Feature 2: Long Listing Format (v1.1.0)

### Q1. What is the crucial difference between stat() and lstat()? When is lstat() more appropriate in ls?

Both system calls fill a `struct stat` with a file's metadata (type, permissions, owner, size, timestamps). The difference is how they treat symbolic links. If the path is a symbolic link, `stat()` follows the link and returns information about the file it points to. `lstat()` does not follow it and returns information about the link itself.

In `ls`, `lstat()` is the right choice. With `ls -l`, a symbolic link must be shown as a link: the file type is `l`, the size is the length of the link's path, and the name is followed by `-> target`. If `stat()` were used, the link would look like the file it points to, and a broken link would make `stat()` fail. `lstat()` is also needed later for colouring symbolic links and for recursive listing, so that `ls -R` does not follow a link to a directory and loop forever.

### Q2. How can bitwise operators and macros extract file type and permissions from st_mode?

The `st_mode` field is one integer in which some bits store the file type and other bits store the permissions. To read a part of it, I use the bitwise AND operator (`&`) with a mask. The result is non-zero only if that bit is set.

For the file type, `st_mode & S_IFMT` keeps only the type bits. I compare that result with constants such as `S_IFDIR` (directory), `S_IFREG` (regular file) or `S_IFLNK` (symbolic link). The macros `S_ISDIR(m)`, `S_ISREG(m)` and `S_ISLNK(m)` do exactly this comparison.

For permissions, I test one bit at a time. For example, `st_mode & S_IRUSR` is non-zero if the owner has read permission. In the same way, `S_IWUSR` and `S_IXUSR` check owner write and execute, `S_IRGRP`, `S_IWGRP` and `S_IXGRP` check the group, and `S_IROTH`, `S_IWOTH` and `S_IXOTH` check others. In my code I check each of the nine bits and print `r`, `w`, `x` or `-` to build a string like `rwxr-xr-x`. The special bits `S_ISUID`, `S_ISGID` and `S_ISVTX` (setuid, setgid and sticky) are tested the same way and change the execute character to `s`, `S`, `t` or `T`.

## Feature 3: Column Display, Down Then Across (v1.2.0)

### Q1. Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop through the filenames insufficient?

First I read all filenames into an array and find the longest one. The column width is the longest name plus two spaces of padding. The number of columns is the terminal width divided by the column width, and the number of rows is the total number of files divided by the number of columns, rounded up.

Then I print row by row. For row `r` and column `c`, the filename to print is `names[r + c * rows]`, so each step to the right skips `rows` entries. The order in the array therefore runs down a column first, and continues at the top of the next column.

A single loop over the array prints names in the order they are stored, which would be across a row (left to right), not down a column. The printing order and the storage order are different, so I need the row and column calculation. A single pass also cannot work because the layout depends on the number of files and the longest name, which are only known after all the entries have been read.

### Q2. What is the purpose of ioctl in this context? What are the limitations of a fixed 80-column width?

`ioctl` with the `TIOCGWINSZ` request asks the terminal driver for the current window size and fills a `struct winsize`. `ws_col` gives the real width in characters, so my program can calculate how many columns fit on the actual screen.

If I only used a fixed width of 80, the output would not adapt to the terminal. On a wide terminal, much of the screen would be wasted because I would use fewer columns than could fit. On a narrow terminal (less than 80 columns), the lines would be longer than the screen, so they would wrap in the middle of the columns and the layout would be broken. Resizing the window would never change the output.
