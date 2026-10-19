//
// IotaOS - i_iotasound.c
// DG_sound_module on top of the kernel's OSS-style /dev/dsp (SB16).
//
// Vanilla DMODMA semantics, simplified: DMX sfx lumps (0x03 0x00 header,
// 16-bit rate, 32-bit length, payload at +24 with 16+16 bytes trimmed)
// are cached once per sfxinfo and mixed 8-bit unsigned mono at 11025 Hz.
// I_UpdateSound runs once per game tic (35 Hz), so each call emits a
// 315-byte slice; blocking writes to the 32 KiB kernel ring provide the
// same pacing DMODMA got from its DMA buffer.
//
// Music (DG_music_module) stays silent: OPL/DMXWAV playback is a separate
// milestone. Sound effects only.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "doomtype.h"
#include "i_sound.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"

#define OUT_RATE   11025
#define SLICE      (OUT_RATE / 35)   /* one tic: 315 samples, 8-bit mono */
#define MAX_CHAN   8

typedef struct {
    const unsigned char *data;  /* payload inside the PU_STATIC lump */
    unsigned int len;           /* payload length, samples            */
    unsigned int step;          /* 16.16 sample step for OUT_RATE     */
} iota_sfx_t;

typedef struct {
    const iota_sfx_t *sfx;
    unsigned int pos;           /* 16.16 position in sfx->data  */
    int vol;                    /* 0..127                       */
    int active;
} iota_chan_t;

static snddevice_t sound_devices[] = { SNDDEVICE_SB };
static snddevice_t music_devices[] = { SNDDEVICE_SB };

static int dsp_fd = -1;
static iota_chan_t chans[MAX_CHAN];
static int use_prefix;

/* Bound to config; the module ignores resampling (mixes in place). */
int use_libsamplerate = 0;
float libsamplerate_scale = 0.66525f;

static boolean IotaSfx_Init(boolean use_sfx_prefix)
{
    int fmt, rate, ch;

    use_prefix = use_sfx_prefix;
    memset(chans, 0, sizeof(chans));

    dsp_fd = open("/dev/dsp", O_WRONLY);
    if (dsp_fd < 0)
        return false;

    fmt = AFMT_U8;
    rate = OUT_RATE;
    ch = 1;
    if (ioctl(dsp_fd, SNDCTL_DSP_SETFMT, &fmt) < 0 || fmt != AFMT_U8
     || ioctl(dsp_fd, SNDCTL_DSP_SPEED, &rate) < 0 || rate != OUT_RATE
     || ioctl(dsp_fd, SNDCTL_DSP_CHANNELS, &ch) < 0 || ch != 1)
    {
        close(dsp_fd);
        dsp_fd = -1;
        return false;
    }

    /* Start playback on the first tic rather than idling the ring. */
    return true;
}

static void IotaSfx_Shutdown(void)
{
    if (dsp_fd >= 0)
    {
        ioctl(dsp_fd, SNDCTL_DSP_SYNC, 0);
        close(dsp_fd);
        dsp_fd = -1;
    }
}

static int IotaSfx_GetSfxLumpNum(sfxinfo_t *sfxinfo)
{
    char lumpname[12];
    int n = 0;

    if (use_prefix)
    {
        lumpname[n++] = 'D';
        lumpname[n++] = 'S';
    }
    while (n < 10 && sfxinfo->name[n - (use_prefix ? 2 : 0)])
    {
        lumpname[n] = sfxinfo->name[n - (use_prefix ? 2 : 0)];
        n++;
    }
    lumpname[n] = '\0';

    return W_CheckNumForName(lumpname);
}

// Load and parse the DMX lump once; result lives in sfxinfo->driver_data.

static const iota_sfx_t *CacheSfx(sfxinfo_t *sfxinfo)
{
    const unsigned char *data;
    unsigned int lumplen, rate, len;
    iota_sfx_t *s;

    if (sfxinfo->driver_data != NULL)
        return (const iota_sfx_t *)sfxinfo->driver_data;

    if (sfxinfo->lumpnum < 0)
        return NULL;

    data = W_CacheLumpNum(sfxinfo->lumpnum, PU_STATIC);
    lumplen = W_LumpLength(sfxinfo->lumpnum);

    /* DMX header: 03 00 | rate(16 LE) | length(32 LE) | 16 pad | PCM */
    if (lumplen < 8 + 32 + 1 || data[0] != 0x03 || data[1] != 0x00)
        return NULL;

    rate = data[2] | (data[3] << 8);
    len  = (unsigned int)data[4] | ((unsigned int)data[5] << 8)
         | ((unsigned int)data[6] << 16) | ((unsigned int)data[7] << 24);

    if (rate < 1000 || rate > 44100)
        return NULL;
    if (len > lumplen - 8)
        len = lumplen - 8;
    if (len <= 32)
        return NULL;

    /* DMX trims 16 bytes off each end of the payload (i_sdlsound parity). */
    data += 24;
    len  -= 32;

    s = malloc(sizeof(*s));
    if (s == NULL)
        return NULL;
    s->data = data;
    s->len  = len;
    s->step = (unsigned int)(((unsigned long long)rate << 16) / OUT_RATE);
    if (s->step == 0)
        s->step = 1;

    sfxinfo->driver_data = s;
    return s;
}

static int IotaSfx_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    const iota_sfx_t *s;

    (void)sep;      /* mono output */

    if (dsp_fd < 0 || channel < 0 || channel >= MAX_CHAN)
        return -1;

    s = CacheSfx(sfxinfo);
    if (s == NULL)
        return -1;

    chans[channel].sfx    = s;
    chans[channel].pos    = 0;
    chans[channel].vol    = vol;
    chans[channel].active = 1;
    return channel;
}

static void IotaSfx_StopSound(int channel)
{
    if (channel >= 0 && channel < MAX_CHAN)
        chans[channel].active = 0;
}

static boolean IotaSfx_SoundIsPlaying(int channel)
{
    if (channel < 0 || channel >= MAX_CHAN)
        return false;
    return chans[channel].active ? true : false;
}

static void IotaSfx_UpdateSoundParams(int channel, int vol, int sep)
{
    (void)sep;
    if (channel >= 0 && channel < MAX_CHAN)
        chans[channel].vol = vol;
}

static short mixacc[SLICE];

static void IotaSfx_Update(void)
{
    unsigned char slice[SLICE];
    int i, c;

    if (dsp_fd < 0)
        return;

    for (i = 0; i < SLICE; i++)
        mixacc[i] = 0;

    for (c = 0; c < MAX_CHAN; c++)
    {
        iota_chan_t *ch = &chans[c];
        if (!ch->active)
            continue;

        for (i = 0; i < SLICE; i++)
        {
            unsigned int idx = ch->pos >> 16;
            if (idx >= ch->sfx->len)
            {
                ch->active = 0;
                break;
            }
            /* u8 sample around the 128 silence point, scaled by volume */
            mixacc[i] += ((int)ch->sfx->data[idx] - 128) * ch->vol / 127;
            ch->pos += ch->sfx->step;
        }
    }

    for (i = 0; i < SLICE; i++)
    {
        int v = 128 + mixacc[i];
        if (v > 255) v = 255;
        if (v < 0)   v = 0;
        slice[i] = (unsigned char)v;
    }

    /* One blocking write per tic: the kernel ring (≈3 s deep at 11 kHz)
       absorbs hitches; a full ring paces the game, like DMODMA. */
    if (write(dsp_fd, slice, sizeof(slice)) < 0)
    {
        close(dsp_fd);
        dsp_fd = -1;
    }
}

sound_module_t DG_sound_module =
{
    sound_devices,
    sizeof(sound_devices) / sizeof(sound_devices[0]),
    IotaSfx_Init,
    IotaSfx_Shutdown,
    IotaSfx_GetSfxLumpNum,
    IotaSfx_Update,
    IotaSfx_UpdateSoundParams,
    IotaSfx_StartSound,
    IotaSfx_StopSound,
    IotaSfx_SoundIsPlaying,
    NULL,   /* no bulk precaching: lumps cache on first use */
};

/* ------------------------------------------------------------------ */
/* DMX MUS -> OPL: the classic FM music, synthesized by the SB16 chip */
/* itself through the kernel's /dev/opl register pipe.                */
/* ------------------------------------------------------------------ */

#define MUS_TICKS_PER_SEC 140   /* DMX stream resolution */
#define DOOM_TICS_PER_SEC 35

static const unsigned char opl_slot[9] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };

/* Default MUS channel velocities, from the engine's own table. */
static const int mus_ch_vol[16] =
{
    127, 127, 127, 127, 127, 127, 127, 127,
    104, 160, 154, 173, 10, 10, 10, 10
};

typedef struct
{
    int busy;
    int ch, note;
    int hi;                     /* block/fnum high byte, for key-off */
    unsigned int stamp;
} opl_voice_t;

static struct
{
    const unsigned char *data;  /* MUS lump, owned by the wad cache */
    const unsigned char *score, *pos, *end;
    int playing, looping;
    unsigned int delay;         /* ticks left before the next event */
    unsigned int stamp;
    float acc;                  /* tic -> 140 Hz tick carry */
} mus;

static struct
{
    const unsigned char *instr; /* into the MUS instrument table */
    int vol, bend;
} mus_ch[16];

static opl_voice_t ovoice[9];
static int opl_fd = -1;
static int mus_volume = 127;
static unsigned char regbuf[128];
static int regn;

static void opl_flush(void)
{
    if (opl_fd >= 0 && regn > 0)
    {
        write(opl_fd, regbuf, regn * 2);
        regn = 0;
    }
}

static void oplw(int reg, int val)
{
    if (opl_fd < 0)
        return;
    if (regn == 64)
        opl_flush();
    regbuf[2 * regn] = (unsigned char)reg;
    regbuf[2 * regn + 1] = (unsigned char)(val & 0xFF);
    regn++;
}

static void opl_all_off(void)
{
    int i;
    for (i = 0; i < 9; i++)
        oplw(0xB0 + opl_slot[i], 0x00);
    memset(ovoice, 0, sizeof(ovoice));
}

/* Program one OPL voice from a MUS instrument + frequency. */
static void opl_note_on(int ch, int note)
{
    const unsigned char *in = mus_ch[ch].instr;
    int v, i, base, op1, op2, tl, block, fnum, vol, bender;
    double freq, fn;

    if (in == NULL)
        in = mus.data + 32;                     /* instrument 0 */

    if (in[2] & 0x80)                           /* fixed-pitch instrument */
        freq = (double)(in[1] | ((in[2] & 0x7F) << 8));
    else
    {
        bender = in[9] > 0 ? in[9] : 2;         /* bend range, semitones */
        freq = 440.0 * pow(2.0,
            ((double)(note - 69)
             + ((double)(mus_ch[ch].bend - 64) / 64.0) * (double)bender) / 12.0);
    }

    fn = freq * 1073741824.0 / 49716.0;         /* f-num at block 0 */
    block = 0;
    while (fn >= 1024.0 && block < 7) { fn /= 2.0; block++; }
    fnum = (int)(fn + 0.5);
    if (fnum > 1023) { fnum = 1023; }

    /* voice allocation: a free slot, else steal the oldest */
    v = -1;
    for (i = 0; i < 9; i++)
        if (!ovoice[i].busy) { v = i; break; }
    if (v < 0)
    {
        v = 0;
        for (i = 1; i < 9; i++)
            if (ovoice[i].stamp < ovoice[v].stamp) v = i;
        oplw(0xB0 + opl_slot[v], 0x00);
    }
    ovoice[v].busy = 1;
    ovoice[v].ch = ch;
    ovoice[v].note = note;
    ovoice[v].stamp = ++mus.stamp;

    base = opl_slot[v];
    op1 = base; op2 = base + 3;
    vol = mus_ch[ch].vol;
    if (vol > 127) vol = 127;
    tl = ((127 - vol) * 3) / 127;               /* 2-bit coarse level */
    tl += (127 - mus_volume) / 43;
    if (tl > 3) tl = 3;

    /* DMX two-op voice: instrument drives the modulator, carrier is
       hardcoded 01/23/33, exactly like the vanilla DMXOPL dump. */
    oplw(0x20 + op1, in[3]);
    oplw(0x40 + op1, (in[4] & 0x3F) | (tl << 6));
    oplw(0x60 + op1, in[5]);
    oplw(0x80 + op1, in[6]);
    oplw(0xE0 + op1, in[7]);
    oplw(0x20 + op2, 0x01);
    oplw(0x40 + op2, 0x23);
    oplw(0x60 + op2, 0x33);
    oplw(0x80 + op2, in[6]);
    oplw(0xE0 + op2, in[8]);
    oplw(0xC0 + base, in[0] & 7);               /* waveform (OPL3) */
    oplw(0xA0 + base, fnum & 0xFF);
    ovoice[v].hi = (block << 2) | ((fnum >> 8) & 7);
    oplw(0xB0 + base, 0x20 | ovoice[v].hi);
}

static void opl_note_off(int ch, int note)
{
    int i;
    for (i = 0; i < 9; i++)
    {
        if (ovoice[i].busy && ovoice[i].ch == ch && ovoice[i].note == note)
        {
            ovoice[i].busy = 0;
            oplw(0xB0 + opl_slot[i], ovoice[i].hi);
        }
    }
}

/* Pump the MUS stream by up to one tic worth of DMX ticks. */
static void mus_pump(void)
{
    int ticks;

    mus.acc += (float)MUS_TICKS_PER_SEC / DOOM_TICS_PER_SEC;
    ticks = (int)mus.acc;
    mus.acc -= (float)ticks;

    while (ticks-- > 0 && mus.playing)
    {
        if (mus.delay > 0) { mus.delay--; continue; }

        for (;;)                                       /* simultaneous block */
        {
            unsigned char d, key, ctrl, val, w;
            int chn, ev;

            if (mus.pos >= mus.end) { mus.playing = 0; break; }
            d = *mus.pos++;
            chn = d & 0x0F;
            ev = d & 0x70;

            switch (ev)
            {
            case 0x00:                                  /* key off */
                if (mus.pos >= mus.end) { mus.playing = 0; break; }
                key = *mus.pos++;
                opl_note_off(chn, key & 0x7F);
                break;
            case 0x10:                                  /* key on */
                if (mus.pos >= mus.end) { mus.playing = 0; break; }
                key = *mus.pos++;
                if (key & 0x80)
                {
                    if (mus.pos >= mus.end) { mus.playing = 0; break; }
                    mus_ch[chn].vol = *mus.pos++ & 0x7F;
                }
                opl_note_on(chn, key & 0x7F);
                break;
            case 0x20:                                  /* pitch bend */
                if (mus.pos >= mus.end) { mus.playing = 0; break; }
                mus_ch[chn].bend = *mus.pos++ & 0x7F;
                break;
            case 0x30: case 0x50:                       /* system/data byte */
                if (mus.pos >= mus.end) { mus.playing = 0; break; }
                mus.pos++;
                break;
            case 0x40:                                  /* controller */
                if (mus.pos + 1 >= mus.end) { mus.playing = 0; break; }
                ctrl = *mus.pos++;
                val = *mus.pos++;
                if (ctrl == 0)
                    mus_ch[chn].instr = mus.data + 32 + 14 * (val % 175);
                break;
            case 0x60:                                  /* score end */
                if (mus.looping)
                {
                    opl_all_off();
                    mus.pos = mus.score;
                    mus.delay = 0;
                }
                else
                    mus.playing = 0;
                break;
            default:
                mus.playing = 0;
                break;
            }

            if (!mus.playing)
                break;
            if (!(d & 0x80))
                break;                                  /* more events now */

            mus.delay = 0;                              /* wait code */
            do
            {
                if (mus.pos >= mus.end) { mus.playing = 0; break; }
                w = *mus.pos++;
                mus.delay = mus.delay * 128 + (w & 0x7F);
            } while (w & 0x80);
            break;
        }
    }

    if (!mus.playing)
        opl_all_off();
    opl_flush();
}

static boolean IotaMusic_Init(void)
{
    opl_fd = open("/dev/opl", O_WRONLY);
    if (opl_fd < 0)
        return false;
    oplw(0x04, 0x60);      /* 4 Hz amplitude and vibrato depth */
    oplw(0x01, 0x20);      /* waveform control enable */
    opl_flush();
    return true;
}

static void IotaMusic_Shutdown(void)
{
    if (opl_fd >= 0)
    {
        opl_all_off();
        opl_flush();
        close(opl_fd);
        opl_fd = -1;
    }
}

static void IotaMusic_SetVolume(int volume)
{
    mus_volume = volume;  /* 0..127 */
}

static void IotaMusic_Pause(void)
{
    mus.playing = 0;
    opl_all_off();
    opl_flush();
}

static void IotaMusic_Resume(void)
{
    if (mus.data != NULL)
        mus.playing = 1;
}

static void *IotaMusic_Register(void *data, int len)
{
    const unsigned char *d = (const unsigned char *)data;
    int score;
    int i;

    if (len < 64 || d == NULL)
        return NULL;
    if (!(d[0] == 'M' && d[1] == 'U' && d[2] == 'S'
       && (d[3] == 0x1A || d[3] == 0x00)))
        return NULL;

    memset(&mus, 0, sizeof(mus));
    score = d[6] | (d[7] << 8);
    if (score >= len)
        score = 40;

    mus.data = d;
    mus.score = d + score;
    mus.pos = mus.score;
    mus.end = d + len;

    for (i = 0; i < 16; i++)
    {
        mus_ch[i].instr = NULL;
        mus_ch[i].vol = mus_ch_vol[i] > 127 ? 127 : mus_ch_vol[i];
        mus_ch[i].bend = 64;
    }
    return &mus;
}

static void IotaMusic_UnRegister(void *handle)
{
    (void)handle;
    mus.data = NULL;
    mus.playing = 0;
}

static void IotaMusic_Play(void *handle, boolean looping)
{
    (void)handle;
    if (mus.data == NULL)
        return;
    opl_all_off();
    mus.pos = mus.score;
    mus.delay = 0;
    mus.acc = 0.0f;
    mus.looping = looping;
    mus.playing = 1;
    opl_flush();
}

static void IotaMusic_Stop(void)
{
    mus.playing = 0;
    opl_all_off();
    opl_flush();
}

static boolean IotaMusic_IsPlaying(void)
{
    return mus.playing ? true : false;
}

static void IotaMusic_Poll(void)
{
    if (mus.playing)
        mus_pump();
}

music_module_t DG_music_module =
{
    music_devices,
    sizeof(music_devices) / sizeof(music_devices[0]),
    IotaMusic_Init,
    IotaMusic_Shutdown,
    IotaMusic_SetVolume,
    IotaMusic_Pause,
    IotaMusic_Resume,
    IotaMusic_Register,
    IotaMusic_UnRegister,
    IotaMusic_Play,
    IotaMusic_Stop,
    IotaMusic_IsPlaying,
    IotaMusic_Poll,
};
