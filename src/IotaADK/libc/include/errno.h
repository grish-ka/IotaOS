/*
* IotaOS - errno.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _ERRNO_H
#define _ERRNO_H

/* The global error number variable */
extern int errno;

/* Standard POSIX error codes (values match Linux) */
#define EPERM          1  /* Operation not permitted */
#define ENOENT         2  /* No such file or directory */
#define ESRCH          3  /* No such process */
#define EINTR          4  /* Interrupted system call */
#define EIO            5  /* Input/output error */
#define ENXIO          6  /* No such device or address */
#define E2BIG          7  /* Argument list too long */
#define ENOEXEC        8  /* Exec format error */
#define EBADF          9  /* Bad file descriptor */
#define ECHILD        10  /* No child processes */
#define EAGAIN        11  /* Resource temporarily unavailable */
#define ENOMEM        12  /* Cannot allocate memory */
#define EACCES        13  /* Permission denied */
#define EFAULT        14  /* Bad address */
#define ENOTBLK       15  /* Block device required */
#define EBUSY         16  /* Device or resource busy */
#define EEXIST        17  /* File exists */
#define EXDEV         18  /* Invalid cross-device link */
#define ENODEV        19  /* No such device */
#define ENOTDIR       20  /* Not a directory */
#define EISDIR        21  /* Is a directory */
#define EINVAL        22  /* Invalid argument */
#define ENFILE        23  /* Too many open files in system */
#define EMFILE        24  /* Too many open files */
#define ENOTTY        25  /* Inappropriate ioctl for device */
#define ETXTBSY       26  /* Text file busy */
#define EFBIG         27  /* File too large */
#define ENOSPC        28  /* No space left on device */
#define ESPIPE        29  /* Illegal seek */
#define EROFS         30  /* Read-only file system */
#define EMLINK        31  /* Too many links */
#define EPIPE         32  /* Broken pipe */
#define EDOM          33  /* Numerical argument out of domain */
#define ERANGE        34  /* Result too large */
#define EDEADLK       35  /* Resource deadlock avoided */
#define ENAMETOOLONG  36  /* File name too long */
#define ENOLCK        37  /* No locks available */
#define ENOSYS        38  /* Function not implemented */
#define ENOTEMPTY     39  /* Directory not empty */
#define ELOOP         40  /* Too many levels of symbolic links */
#define EWOULDBLOCK  EAGAIN
#define ENOMSG        42  /* No message of desired type */
#define EOVERFLOW     75  /* Value too large for defined data type */
#define EILSEQ        84  /* Invalid or incomplete multibyte or wide character */
#define ENOTSOCK      88  /* Socket operation on non-socket */
#define EOPNOTSUPP    95  /* Operation not supported */
#define ENOTSUP       EOPNOTSUPP
#define EAFNOSUPPORT  97  /* Address family not supported by protocol */
#define EADDRINUSE    98  /* Address already in use */
#define EADDRNOTAVAIL 99  /* Cannot assign requested address */
#define ENETDOWN     100  /* Network is down */
#define ENETUNREACH  101  /* Network is unreachable */
#define ECONNABORTED 103  /* Software caused connection abort */
#define ECONNRESET   104  /* Connection reset by peer */
#define ENOBUFS      105  /* No buffer space available */
#define EISCONN      106  /* Transport endpoint is already connected */
#define ENOTCONN     107  /* Transport endpoint is not connected */
#define ECONNREFUSED 111  /* Connection refused */
#define EHOSTUNREACH 113  /* No route to host */
#define EALREADY     114  /* Operation already in progress */
#define EINPROGRESS  115  /* Operation now in progress */
#define EPROTONOSUPPORT 93 /* Protocol not supported */
#define ESOCKTNOSUPPORT 94 /* Socket type not supported */
#define EPROTOTYPE    91  /* Protocol wrong type for socket */
#define ENOPROTOOPT   92  /* Protocol not available */
#define EMSGSIZE      90  /* Message too long */
#define EDESTADDRREQ  89  /* Destination address required */
#define ETIMEDOUT    110  /* Connection timed out */
#define EFTYPE       EINVAL  /* BSD: inappropriate file type or format */

/* glibc extensions */
extern char *program_invocation_name;
extern char *program_invocation_short_name;

#endif