/*
* IotaOS - linux/version.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*
* For programs ported from Linux that check the kernel version at build time.
* IotaOS is not Linux; this reports a recent kernel so modern code paths are used.
*/

#ifndef _LINUX_VERSION_H
#define _LINUX_VERSION_H

#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + ((c) > 255 ? 255 : (c)))
#define LINUX_VERSION_CODE KERNEL_VERSION(6, 1, 0)

#endif
