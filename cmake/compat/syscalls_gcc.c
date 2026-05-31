#if defined(__GNUC__)

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

extern int fputc(int ch, FILE *f);

int _write(int file, char *ptr, int len)
{
    int i;
    FILE *stream;

    if ((file != 1) && (file != 2))
    {
        errno = EBADF;
        return -1;
    }

    stream = (file == 2) ? stderr : stdout;
    for (i = 0; i < len; ++i)
    {
        (void)fputc((unsigned char)ptr[i], stream);
    }
    return len;
}

int _read(int file, char *ptr, int len)
{
    (void)ptr;
    (void)len;

    if (file == 0)
    {
        return 0;
    }

    errno = EBADF;
    return -1;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    if ((file >= 0) && (file <= 2))
    {
        st->st_mode = S_IFCHR;
        return 0;
    }

    errno = EBADF;
    return -1;
}

int _isatty(int file)
{
    if ((file >= 0) && (file <= 2))
    {
        return 1;
    }

    errno = EBADF;
    return 0;
}

int _lseek(int file, int ptr, int dir)
{
    (void)ptr;
    (void)dir;

    if ((file >= 0) && (file <= 2))
    {
        return 0;
    }

    errno = EBADF;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

#endif
