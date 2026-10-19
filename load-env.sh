#!/bin/sh
#
# IotaOS - load-env.sh
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#
# Sets up cross-compiler toolchain and environment variables for IotaOS.
# Usage: source ./load-env.sh
#

if [ -d "/home/grish/opt/cross/bin" ]; then
    case ":$PATH:" in
        *":/home/grish/opt/cross/bin:"*) ;;
        *) export PATH="/home/grish/opt/cross/bin:$PATH" ;;
    esac
fi

export CC=i686-elf-gcc
export AR=i686-elf-ar
export RANLIB=i686-elf-ranlib
export CC_FOR_BUILD=gcc

# Configure caches for GNU ports cross-compilation
export bash_cv_printf_a_format=yes
export bash_cv_job_control_missing=missing
export ac_cv_func_mmap_fixed_mapped=no
export ac_cv_func_strcoll_works=yes
export ac_cv_func_working_mktime=no
export bash_cv_sys_named_pipes=missing
export bash_cv_getcwd_malloc=yes
export bash_cv_func_sigsetjmp=present
export bash_cv_unusable_rtsigs=yes

echo "IotaOS environment loaded (CC=$CC, PATH includes /home/grish/opt/cross/bin)"
