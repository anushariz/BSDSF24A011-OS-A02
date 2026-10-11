# REPORT.md

## Feature 2: Long Listing Format (v1.1.0)

### Q1. What is the crucial difference between stat() and lstat()? When is lstat() more appropriate in ls?

Both system calls fill a `struct stat` with a file's metadata (type, permissions, owner, size, timestamps). The difference is how they treat symbolic links. If the path is a symbolic link, `stat()` follows the link and returns information about the file it points to. `lstat()` does not follow it and returns information about the link itself.

In `ls`, `lstat()` is the right choice. With `ls -l`, a symbolic link must be shown as a link: the file type is `l`, the size is the length of the link's path, and the name is followed by `-> target`. If `stat()` were used, the link would look like the file it points to, and a broken link would make `stat()` fail. `lstat()` is also needed later for colouring symbolic links and for recursive listing, so that `ls -R` does not follow a link to a directory and loop forever.

### Q2. How can bitwise operators and macros extract file type and permissions from st_mode?

The `st_mode` field is one integer in which some bits store the file type and other bits store the permissions. To read a part of it, I use the bitwise AND operator (`&`) with a mask. The result is non-zero only if that bit is set.

For the file type, `st_mode & S_IFMT` keeps only the type bits. I compare that result with constants such as `S_IFDIR` (directory), `S_IFREG` (regular file) or `S_IFLNK` (symbolic link). The macros `S_ISDIR(m)`, `S_ISREG(m)` and `S_ISLNK(m)` do exactly this comparison.

For permissions, I test one bit at a time. For example, `st_mode & S_IRUSR` is non-zero if the owner has read permission. In the same way, `S_IWUSR` and `S_IXUSR` check owner write and execute, `S_IRGRP`, `S_IWGRP` and `S_IXGRP` check the group, and `S_IROTH`, `S_IWOTH` and `S_IXOTH` check others. In my code I check each of the nine bits and print `r`, `w`, `x` or `-` to build a string like `rwxr-xr-x`. The special bits `S_ISUID`, `S_ISGID` and `S_ISVTX` (setuid, setgid and sticky) are tested the same way and change the execute character to `s`, `S`, `t` or `T`.
