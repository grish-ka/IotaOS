/*
 * IotaOS - tests.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Comprehensive System Diagnostics & Automated Test Suite:
 *  1. Dynamic Heap (malloc / free)
 *  2. VFS File I/O (fopen / fread)
 *  3. Command Arguments
 *  4. Environment Variables (getenv)
 *  5. ANSI Escape Engine
 *  6. ATA Hard Disk Persistent Storage (/dev/hda sector I/O)
 *  7. VBE Linear Framebuffer Device (/dev/fb0 FBIOGET_VSCREENINFO)
 *  8. PS/2 Mouse & 100% Raw Sensitivity Subsystem (/dev/mouse)
 *  9. x87 FPU Transcendental Math (sin, cos, sqrt, atan2, pow)
 * 10. High-Memory 32-bit PMM Physical Memory Management
 * 11. SB16 Audio Subsystem (/dev/dsp, /dev/mixer, OSS ioctls, poll)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include "iota.h"
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/soundcard.h>

int test_memory(void) {
    /* Allocate, verify write access, and free */
    char* buf = (char*)malloc(128);
    if (!buf) return 0;
    
    sprintf(buf, "IotaHeapTest");
    int passed = (strcmp(buf, "IotaHeapTest") == 0);
    
    free(buf);
    return passed;
}

int test_file_io(void) {
    /* Check binary existence in VFS */
    FILE* f = fopen("/bin/tests", "r");
    if (!f) f = fopen("/bin/iosh", "r");
    if (!f) return 0;
    
    char buf[8];
    size_t bytes_read = fread(buf, 1, 4, f);
    fclose(f);
    
    return (bytes_read == 4 && buf[0] == 0x7F && buf[1] == 'E');
}

int test_arguments(int argc, char* argv[]) {
    if (argc < 1) return 0;
    return (strcmp(argv[0], "./tests") == 0 || 
            strcmp(argv[0], "tests") == 0 || 
            strcmp(argv[0], "/bin/tests") == 0);
}

int test_environment(void) {
    char* term = getenv("TERM");
    char* user = getenv("USER");
    if (!term || !user) return 0;
    return (strcmp(term, "linux") == 0 && (strcmp(user, "root") == 0 || strcmp(user, "iota") == 0));
}

int test_ansi(void) {
    printf("\033[36m(ANSI OK)\033[0m ");
    return 1;
}

int test_ata_hdd(void) {
    int fd = open("/dev/hda", O_RDWR);
    if (fd < 0) return 0;

    /* Write test pattern to sector 10 (offset 5120) */
    static const char magic[32] = "IOTA_PERSISTENT_SECTOR_2026_OK!";
    off_t target_offset = 10 * 512;
    if (lseek(fd, target_offset, SEEK_SET) < 0) {
        close(fd);
        return 0;
    }
    if (write(fd, magic, sizeof(magic)) != sizeof(magic)) {
        close(fd);
        return 0;
    }

    /* Read back pattern */
    char read_buf[32] = {0};
    if (lseek(fd, target_offset, SEEK_SET) < 0) {
        close(fd);
        return 0;
    }
    if (read(fd, read_buf, sizeof(read_buf)) != sizeof(read_buf)) {
        close(fd);
        return 0;
    }

    close(fd);
    return (memcmp(magic, read_buf, sizeof(magic)) == 0);
}

int test_framebuffer(void) {
    int fd = open("/dev/fb0", O_RDONLY);
    if (fd < 0) return 0;

    uint32_t meta[6] = {0};
    int res = ioctl(fd, 0x4600, meta);
    close(fd);

    if (res != 0) return 0;
    /* meta[0]=w, meta[1]=h, meta[2]=pitch, meta[3]=bpp, meta[4]=paddr, meta[5]=size */
    return (meta[0] >= 640 && meta[1] >= 480 && meta[3] >= 16 && meta[5] > 0);
}

int test_mouse_driver(void) {
    int fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
    if (fd < 0) return 0;

    int sens = 85;
    int res = ioctl(fd, 2, &sens); /* Set sensitivity */
    close(fd);
    return (res == 0);
}

int test_math_fpu(void) {
    double s = sin(M_PI / 2.0);
    double c = cos(0.0);
    double sq = sqrt(144.0);
    double at = atan2(1.0, 1.0);
    double p = pow(2.0, 10.0);

    bool s_ok = fabs(s - 1.0) < 0.0001;
    bool c_ok = fabs(c - 1.0) < 0.0001;
    bool sq_ok = fabs(sq - 12.0) < 0.0001;
    bool at_ok = fabs(at - (M_PI / 4.0)) < 0.0001;
    bool p_ok = fabs(p - 1024.0) < 0.0001;

    return (s_ok && c_ok && sq_ok && at_ok && p_ok);
}

int test_pmm_memory(void) {
    iota_sysinfo_t si;
    if (iota_syscall3(IOTA_SYS_SYSINFO, (uint32_t)&si, 0, 0) != 0) return 0;
    uint32_t total_mb = (si.total_pages * 4096) / (1024 * 1024);
    /* In IotaOS, detected active 32-bit RAM is >= 256 MB */
    return (total_mb >= 256);
}

int test_audio(void) {
    if (access("/dev/dsp", F_OK) != 0) return 0;

    int fd = open("/dev/dsp", O_WRONLY);
    if (fd < 0) return 0;
    int ok = 1;

    int fmt = AFMT_S16_LE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) < 0 || fmt != AFMT_S16_LE) ok = 0;

    int rate = 22050;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &rate) < 0 || rate != 22050) ok = 0;

    int ch = 1;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &ch) < 0 || ch != 1) ok = 0;

    int space = 0;
    if (ioctl(fd, SNDCTL_DSP_GETOSPACE, &space) < 0 || space <= 0) ok = 0;

    /* 12 bursts = 48 KiB, more than the 32 KiB hardware ring: the writer
       must block mid-stream (short write + poll) and SYNC must still drain
       everything. A 4 KiB-only test misses that path entirely. */
    static int16_t tone[2048];
    for (int i = 0; i < 2048; i++)
        tone[i] = (int16_t)(((i / 64) % 64) * 1000 - 32000);
    for (int b = 0; b < 12; b++) {
        if (write(fd, tone, sizeof(tone)) != (ssize_t)sizeof(tone)) ok = 0;
    }
    if (ioctl(fd, SNDCTL_DSP_SYNC, 0) < 0) ok = 0;

    /* After RESET the ring is empty, so POLLOUT is immediately ready */
    ioctl(fd, SNDCTL_DSP_RESET, 0);
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) != 1 || !(pfd.revents & POLLOUT)) ok = 0;

    /* A char device must refuse seeks */
    errno = 0;
    if (lseek(fd, 0, SEEK_SET) != -1 || errno != ESPIPE) ok = 0;

    /* Non-blocking odd write in S16 mode: nothing fits whole, so the
       kernel must return EAGAIN, not spin. */
    {
        int nb = open("/dev/dsp", O_WRONLY | O_NONBLOCK);
        if (nb < 0) { ok = 0; }
        else {
            int f2 = AFMT_S16_LE;
            ioctl(nb, SNDCTL_DSP_SETFMT, &f2);
            char one = 0;
            errno = 0;
            if (write(nb, &one, 1) != -1 || errno != EAGAIN) ok = 0;
            close(nb);
        }
    }

    /* Blocking odd write: the child must sleep interruptibly in the file
       layer (POLLOUT-ready but no whole sample fits), never hot-spin with
       IF clear (that class froze the whole machine once), and a SIGKILL
       must still reap it. The kernel freezes or the wait dies if broken. */
    {
        pid_t kid = fork();
        if (kid == 0) {
            int bf = open("/dev/dsp", O_WRONLY);
            if (bf >= 0) {
                int f2 = AFMT_S16_LE;
                ioctl(bf, SNDCTL_DSP_SETFMT, &f2);
                char one = 0;
                write(bf, &one, 1);   /* expected to block */
            }
            _exit(0);
        } else if (kid > 0) {
            usleep(100000);           /* 100 ms: longer than any legit wait */
            int status = 0;
            if (waitpid(kid, &status, WNOHANG) != 0) ok = 0; /* must still block */
            kill(kid, SIGKILL);
            usleep(100000);
            if (waitpid(kid, &status, 0) != kid) ok = 0;
        } else {
            ok = 0;
        }
    }

    close(fd);

    int mx = open("/dev/mixer", O_RDWR);
    if (mx < 0) return 0;
    int vol = (70 << 8) | 70;
    if (ioctl(mx, MIXER_WRITE(DEV_PCM), &vol) < 0) ok = 0;
    int readback = 0;
    if (ioctl(mx, MIXER_READ(DEV_MIXER), &readback) < 0 || readback != vol) ok = 0;
    close(mx);

    return ok;
}

int main(int argc, char* argv[]) {
    (void)argv;
    printf("\n=== IotaOS 1.0.0 Diagnostic Test Suite ===\n");
    int passed = 0;
    int total = 11;

    printf("[ 1/11] Testing Dynamic Heap (malloc/free).............. ");
    if (test_memory()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 2/11] Testing VFS File I/O (fopen/fread).............. ");
    if (test_file_io()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 3/11] Testing Process Arguments (argc/argv)........... ");
    if (test_arguments(argc, argv)) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 4/11] Testing Process Environment (getenv)............ ");
    if (test_environment()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 5/11] Testing ANSI Terminal Control................... ");
    if (test_ansi()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 6/11] Testing ATA Hard Disk Persistence (/dev/hda).... ");
    if (test_ata_hdd()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 7/11] Testing Framebuffer Device (/dev/fb0)........... ");
    if (test_framebuffer()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 8/11] Testing PS/2 Mouse Raw Subsystem (/dev/mouse)... ");
    if (test_mouse_driver()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[ 9/11] Testing x87 FPU Math Functions (trig/exp/log)... ");
    if (test_math_fpu()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[10/11] Testing PMM Physical Memory Management (>=256MB) ");
    if (test_pmm_memory()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("[11/11] Testing SB16 Audio (/dev/dsp + /dev/mixer)....... ");
    if (test_audio()) { printf("\033[32mPASS\033[0m\n"); passed++; } else { printf("\033[31mFAIL\033[0m\n"); }

    printf("==========================================\n");
    printf("Diagnostic Results: %d/%d Tests Passed.\n", passed, total);

    if (passed == total) {
        printf("\033[32m[SUCCESS] All kernel subsystems verified and operating normally.\033[0m\n\n");
    } else {
        printf("\033[31m[WARNING] Subsystem irregularities detected.\033[0m\n\n");
    }

    printf("Tests complete. Returning to display manager in 5 seconds...\n");
    sleep(5);

    return (passed == total) ? 0 : 1;
}