/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * Tue, 30 Jun 1998 20:23:11 +0200
 * Reported by John v/d Kamp <blade_@dds.nl>:
 * I have this song from Purple Motion called wcharts.s3m, the global
 * volume was set to 0, creating a devide by 0 error in xmp. There should
 * be an extra test if it's 0 or not.
 *
 * Claudio's fix: global volume ignored
 */

/*
 * Sat, 29 Aug 1998 18:50:43 -0500 (CDT)
 * Reported by Joel Jordan <scriber@usa.net>:
 * S3M files support tempos outside the ranges defined by xmp (that is,
 * the MOD/XM tempo ranges).  S3M's can have tempos from 0 to 255 and speeds
 * from 0 to 255 as well, since these are handled with separate effects
 * unlike the MOD format.  This becomes an issue in such songs as Skaven's
 * "Catch that Goblin", which uses speeds above 0x1f.
 *
 * Claudio's fix: FX_S3M_SPEED added. S3M supports speeds from 0 to 255 and
 * tempos from 32 to 255 (S3M speed == xmp tempo, S3M tempo == xmp BPM).
 */

/* Wed, 21 Oct 1998 15:03:44 -0500  Geoff Reedy <vader21@imsa.edu>
 * It appears that xmp has issues loading/playing a specific instrument
 * used in LUCCA.S3M.
 * (Fixed by Hipolito in xmp-2.0.0dev34)
 */

/*
 * From http://code.pui.ch/2007/02/18/turn-demoscene-modules-into-mp3s/
 * The only flaw I noticed [in xmp] is a problem with portamento in Purple
 * Motion's second reality soundtrack (1:06-1:17)
 *
 * Claudio's note: that's a dissonant beating between channels 6 and 7
 * starting at pos12, caused by pitchbending effect F25.
 */

/*
 * From: Ralf Hoffmann <ralf@boomerangsworld.de>
 * Date: Wed, 26 Sep 2007 17:12:41 +0200
 * ftp://ftp.scenesp.org/pub/compilations/modplanet/normal/bonuscd/artists/
 * Iq/return%20of%20litmus.s3m doesn't start playing, just uses 100% cpu,
 * the number of patterns is unusually high
 *
 * Claudio's fix: this module seems to be a bad conversion, bad rip or
 * simply corrupted since it has many instances of 0x87 instead of 0x00
 * in the module and instrument headers. I'm adding a simple workaround
 * to be able to load/play the module as is, see the fix87() macro below.
 */

#include "loader.h"
#include "s3m.h"
#include "period.h"
#include "synth.h"

#define MAGIC_SCRM	MAGIC4('S','C','R','M')
#define MAGIC_SCRI	MAGIC4('S','C','R','I')
#define MAGIC_SCRS	MAGIC4('S','C','R','S')

static int s3m_test (FILE *, char *, const int);
static int s3m_load (struct module_data *, FILE *, const int);

const struct format_loader s3m_loader = {
    "Scream Tracker 3 (S3M)",
    s3m_test,
    s3m_load
};

static int s3m_test(FILE *f, char *t, const int start)
{
    fseek(f, start + 44, SEEK_SET);
    if (read32b(f) != MAGIC_SCRM)
	return -1;

    fseek(f, start + 0, SEEK_SET);
    read_title(f, t, 28);

    return 0;
}



#define NONE		0xff
#define FX_S3M_EXTENDED	0xfe

#define fix87(x) do { \
	int i; for (i = 0; i < sizeof(x); i++) { \
		if (*((uint8 *)&x + i) == 0x87) *((uint8 *)&x + i) = 0; } \
	} while (0)

/* Effect conversion table */
static const uint8 fx[] =
{
	NONE,
	FX_S3M_SPEED,		/* Axx  Set speed to xx (the default is 06) */
	FX_JUMP,		/* Bxx  Jump to order xx (hexadecimal) */
	FX_BREAK,		/* Cxx  Break pattern to row xx (decimal) */
	FX_VOLSLIDE,		/* Dxy  Volume slide down by y/up by x */
	FX_PORTA_DN,		/* Exx  Slide down by xx */
	FX_PORTA_UP,		/* Fxx  Slide up by xx */
	FX_TONEPORTA,		/* Gxx  Tone portamento with speed xx */
	FX_VIBRATO,		/* Hxy  Vibrato with speed x and depth y */
	FX_TREMOR,		/* Ixy  Tremor with ontime x and offtime y */
	FX_ARPEGGIO,		/* Jxy  Arpeggio with halfnote additions */
	FX_VIBRA_VSLIDE,	/* Kxy  Dual command: H00 and Dxy */
	FX_TONE_VSLIDE,		/* Lxy  Dual command: G00 and Dxy */
	NONE,
	NONE,
	FX_OFFSET,		/* Oxy  Set sample offset */
	NONE,
	FX_MULTI_RETRIG,	/* Qxy  Retrig (+volumeslide) note */
	FX_TREMOLO,		/* Rxy  Tremolo with speed x and depth y */
	FX_S3M_EXTENDED,	/* Sxx  (misc effects) */
	FX_S3M_BPM,		/* Txx  Tempo = xx (hex) */
	FX_FINE4_VIBRA,		/* Uxx  Fine vibrato */
	FX_GLOBALVOL,		/* Vxx  Set global volume */
	NONE,
	NONE,
	NONE,
	NONE
};


/* Effect translation */
static void xlat_fx(int c, struct xmp_event *e, uint8 *arpeggio_val)
{
    uint8 h = MSN (e->fxp), l = LSN (e->fxp);

    switch (e->fxt = fx[e->fxt]) {
    case FX_ARPEGGIO:			/* Arpeggio */
	if (e->fxp)
	    arpeggio_val[c] = e->fxp;
	else
	    e->fxp = arpeggio_val[c];
	break;
    case FX_S3M_EXTENDED:		/* Extended effects */
	e->fxt = FX_EXTENDED;
	switch (h) {
	case 0x1:			/* Glissando */
	    e->fxp = LSN (e->fxp) | (EX_GLISS << 4);
	    break;
	case 0x2:			/* Finetune */
	    e->fxp = LSN (e->fxp) | (EX_FINETUNE << 4);
	    break;
	case 0x3:			/* Vibrato wave */
	    e->fxp = LSN (e->fxp) | (EX_VIBRATO_WF << 4);
	    break;
	case 0x4:			/* Tremolo wave */
	    e->fxp = LSN (e->fxp) | (EX_TREMOLO_WF << 4);
	    break;
	case 0x5:
	case 0x6:
	case 0x7:
	case 0x9:
	case 0xa:			/* Ignore */
	    e->fxt = e->fxp = 0;
	    break;
	case 0x8:			/* Set pan */
	    e->fxt = FX_MASTER_PAN;
	    e->fxp = l << 4;
	    break;
	case 0xb:			/* Pattern loop */
	    e->fxp = LSN (e->fxp) | (EX_PATTERN_LOOP << 4);
	    break;
	case 0xc:
	    if (!l)
		e->fxt = e->fxp = 0;
	}
	break;
    case NONE:				/* No effect */
	e->fxt = e->fxp = 0;
	break;
    }
}


static int s3m_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int c, r, i;
    struct s3m_adlib_header sah;
    struct xmp_event *event = 0, dummy;
    struct s3m_file_header sfh;
    struct s3m_instrument_header sih;
    int pat_len;
    uint8 n, b, x8;
    char tracker_name[40];
    int quirk87 = 0;
    uint16 *pp_ins;		/* Parapointers to instruments */
    uint16 *pp_pat;		/* Parapointers to patterns */
    uint8 arpeggio_val[32];

    LOAD_INIT();

    fread(&sfh.name, 28, 1, f);		/* Song name */
    read8(f);				/* 0x1a */
    sfh.type = read8(f);		/* File type */
    read16l(f);				/* Reserved */
    sfh.ordnum = read16l(f);		/* Number of orders (must be even) */
    sfh.insnum = read16l(f);		/* Number of instruments */
    sfh.patnum = read16l(f);		/* Number of patterns */
    sfh.flags = read16l(f);		/* Flags */
    sfh.version = read16l(f);		/* Tracker ID and version */
    sfh.ffi = read16l(f);		/* File format information */
    sfh.magic = read32b(f);		/* 'SCRM' */
    sfh.gv = read8(f);			/* Global volume */
    sfh.is = read8(f);			/* Initial speed */
    sfh.it = read8(f);			/* Initial tempo */
    sfh.mv = read8(f);			/* Master volume */
    sfh.uc = read8(f);			/* Ultra click removal */
    sfh.dp = read8(f);			/* Default pan positions if 0xfc */
    read32l(f);				/* Reserved */
    read32l(f);				/* Reserved */
    sfh.special = read16l(f);		/* Ptr to special custom data */
    fread(sfh.chset, 32, 1, f);		/* Channel settings */

#if 0
    if (sfh.magic != MAGIC_SCRM)
	return -1;
#endif

    /* S3M anomaly in return_of_litmus.s3m */
    if (sfh.version == 0x1301 && sfh.name[27] == 0x87)
	quirk87 = 1;

    if (quirk87) {
	fix87(sfh.name);
	fix87(sfh.patnum);
	fix87(sfh.flags);
    }

    copy_adjust(mod->name, sfh.name, 28);

    /* Load and convert header */
    mod->len = sfh.ordnum;
    mod->ins = sfh.insnum;
    mod->smp = mod->ins;
    mod->pat = sfh.patnum;
    pp_ins = calloc (2, mod->ins);
    pp_pat = calloc (2, mod->pat);
    if (sfh.flags & S3M_AMIGA_RANGE)
	m->quirk |= QUIRK_MODRNG;
    if (sfh.flags & S3M_ST300_VOLS)
	m->quirk |= QUIRK_VSALL;
    /* m->volbase = 4096 / sfh.gv; */
    mod->spd = sfh.is;
    mod->bpm = sfh.it;

    for (i = 0; i < 32; i++) {
	if (sfh.chset[i] == S3M_CH_OFF)
	    continue;

	mod->chn = i + 1;

	if (sfh.mv & 0x80) {	/* stereo */
		int x = sfh.chset[i] & S3M_CH_PAN;
		mod->xxc[i].pan = (x & 0x0f) < 8 ? 0x00 : 0xff;
	} else {
		mod->xxc[i].pan = 0x80;
	}
    }
    mod->trk = mod->pat * mod->chn;

    fread(mod->xxo, 1, mod->len, f);

    for (i = 0; i < mod->ins; i++)
	pp_ins[i] = read16l(f);
 
    for (i = 0; i < mod->pat; i++)
	pp_pat[i] = read16l(f);

    /* Default pan positions */

    for (i = 0, sfh.dp -= 0xfc; !sfh.dp /* && n */ && (i < 32); i++) {
	uint8 x = read8(f);
	if (x & S3M_PAN_SET)
	    mod->xxc[i].pan = (x << 4) & 0xff;
	else
	    mod->xxc[i].pan = sfh.mv % 0x80 ? 0x30 + 0xa0 * (i & 1) : 0x80;
    }

    m->c4rate = C4_NTSC_RATE;

    if (sfh.version == 0x1300)
	m->quirk |= QUIRK_VSALL;

    switch (sfh.version >> 12) {
    case 1:
	snprintf(tracker_name, 40, "Scream Tracker %d.%02x",
		(sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
	m->quirk |= QUIRK_ST3GVOL;
	break;
    case 2:
	snprintf(tracker_name, 40, "Imago Orpheus %d.%02x",
		(sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
	break;
    case 3:
	if (sfh.version == 0x3216) {
		strcpy(tracker_name, "Impulse Tracker 2.14v3");
	} else if (sfh.version == 0x3217) {
		strcpy(tracker_name, "Impulse Tracker 2.14v5");
	} else {
		snprintf(tracker_name, 40, "Impulse Tracker %d.%02x",
			(sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
	}
	break;
    case 4:
	snprintf(tracker_name, 40, "Schism Tracker %d.%02x",
		(sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
	break;
    case 5:
	snprintf(tracker_name, 40, "OpenMPT %d.%02x",
		(sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
	break;
    default:
	snprintf(tracker_name, 40, "unknown (%04x)", sfh.version);
    }

    snprintf(mod->type, XMP_NAME_SIZE, "%s S3M", tracker_name);

    MODULE_INFO();

    PATTERN_INIT();

    /* Read patterns */

    D_(D_INFO "Stored patterns: %d", mod->pat);

    memset (arpeggio_val, 0, 32);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	if (!pp_pat[i])
	    continue;

	fseek(f, start + pp_pat[i] * 16, SEEK_SET);
	r = 0;
	pat_len = read16l(f) - 2;

	/* Used to be (--pat_len >= 0). Replaced by Rudolf Cejka
	 * <cejkar@dcse.fee.vutbr.cz>, fixes hunt.s3m
	 * ftp://us.aminet.net/pub/aminet/mods/8voic/s3m_hunt.lha
	 */
	while (r < mod->xxp[i]->rows) {
	    b = read8(f);

	    if (b == S3M_EOR) {
		r++;
		continue;
	    }

	    c = b & S3M_CH_MASK;
	    event = c >= mod->chn ? &dummy : &EVENT (i, c, r);

	    if (b & S3M_NI_FOLLOW) {
		switch(n = read8(f)) {
		case 255:
		    n = 0;
		    break;	/* Empty note */
		case 254:
		    n = XMP_KEY_OFF;
		    break;	/* Key off */
		default:
		    n = 13 + 12 * MSN (n) + LSN (n);
		}
		event->note = n;
		event->ins = read8(f);
		pat_len -= 2;
	    }

	    if (b & S3M_VOL_FOLLOWS) {
		event->vol = read8(f) + 1;
		pat_len--;
	    }

	    if (b & S3M_FX_FOLLOWS) {
		event->fxt = read8(f);
		event->fxp = read8(f);
		xlat_fx(c, event, arpeggio_val);
		pat_len -= 2;
	    }
	}
    }

    D_(D_INFO "Stereo enabled: %s", sfh.mv & 0x80 ? "yes" : "no");
    D_(D_INFO "Pan settings: %s", sfh.dp ? "no" : "yes");

    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    D_(D_INFO "Instruments: %d", mod->ins);

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	fseek(f, start + pp_ins[i] * 16, SEEK_SET);
	x8 = read8(f);
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	if (x8 >= 2) {
	    /* OPL2 FM instrument */

	    fread(&sah.dosname, 12, 1, f);	/* DOS file name */
	    fread(&sah.rsvd1, 3, 1, f);		/* 0x00 0x00 0x00 */
	    fread(&sah.reg, 12, 1, f);		/* Adlib registers */
	    sah.vol = read8(f);
	    sah.dsk = read8(f);
	    read16l(f);
	    sah.c2spd = read16l(f);		/* C 4 speed */
	    read16l(f);
	    fread(&sah.rsvd4, 12, 1, f);	/* Reserved */
	    fread(&sah.name, 28, 1, f);		/* Instrument name */
	    sah.magic = read32b(f);		/* 'SCRI' */

	    if (sah.magic != MAGIC_SCRI) {
		D_(D_CRIT "error: FM instrument magic");
		return -2;
	    }
	    sah.magic = 0;

	    copy_adjust(mod->xxi[i].name, sah.name, 28);

	    mod->xxi[i].nsm = 1;
	    mod->xxi[i].sub[0].vol = sah.vol;
	    c2spd_to_note(sah.c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
	    mod->xxi[i].sub[0].xpo += 12;
	    load_sample(f, SAMPLE_FLAG_ADLIB, &mod->xxs[i], (char *)&sah.reg);
	    D_(D_INFO "[%2X] %-28.28s", i, mod->xxi[i].name);

	    continue;
	}

	fread(&sih.dosname, 13, 1, f);		/* DOS file name */
	sih.memseg = read16l(f);		/* Pointer to sample data */
	sih.length = read32l(f);		/* Length */
	sih.loopbeg = read32l(f);		/* Loop begin */
	sih.loopend = read32l(f);		/* Loop end */
	sih.vol = read8(f);			/* Volume */
	sih.rsvd1 = read8(f);			/* Reserved */
	sih.pack = read8(f);			/* Packing type (not used) */
	sih.flags = read8(f);		/* Loop/stereo/16bit samples flags */
	sih.c2spd = read16l(f);			/* C 4 speed */
	sih.rsvd2 = read16l(f);			/* Reserved */
	fread(&sih.rsvd3, 4, 1, f);		/* Reserved */
	sih.int_gp = read16l(f);		/* Internal - GUS pointer */
	sih.int_512 = read16l(f);		/* Internal - SB pointer */
	sih.int_last = read32l(f);		/* Internal - SB index */
	fread(&sih.name, 28, 1, f);		/* Instrument name */
	sih.magic = read32b(f);			/* 'SCRS' */

	if (x8 == 1 && sih.magic != MAGIC_SCRS) {
	    D_(D_CRIT "error: instrument magic");
	    return -2;
	}

	if (quirk87) {
	    fix87(sih.length);
	    fix87(sih.loopbeg);
	    fix87(sih.loopend);
	    fix87(sih.flags);
	}

	mod->xxs[i].len = sih.length;
	mod->xxi[i].nsm = sih.length > 0 ? 1 : 0;
	mod->xxs[i].lps = sih.loopbeg;
	mod->xxs[i].lpe = sih.loopend;

	mod->xxs[i].flg = sih.flags & 1 ? XMP_SAMPLE_LOOP : 0;

	if (sih.flags & 4) {
	    mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
	    mod->xxs[i].len >>= 1;
	    mod->xxs[i].lps >>= 1;
	    mod->xxs[i].lpe >>= 1;
	}

	mod->xxi[i].sub[0].vol = sih.vol;
	sih.magic = 0;

	copy_adjust(mod->xxi[i].name, sih.name, 28);

	D_(D_INFO "[%2X] %-28.28s %04x%c%04x %04x %c V%02x %5d",
			i, mod->xxi[i].name, mod->xxs[i].len,
			mod->xxs[i].flg & XMP_SAMPLE_16BIT ?'+' : ' ',
			mod->xxs[i].lps, mod->xxs[i].lpe,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ?  'L' : ' ',
			mod->xxi[i].sub[0].vol, sih.c2spd);

	c2spd_to_note(sih.c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);

	fseek(f, start + 16L * sih.memseg, SEEK_SET);
	load_sample(f, (sfh.ffi - 1) * SAMPLE_FLAG_UNS, &mod->xxs[i], NULL);
    }

    free(pp_pat);
    free(pp_ins);

    m->synth = &synth_adlib;
    m->quirk |= QUIRKS_ST3;
    m->read_event_type = READ_EVENT_ST3;

    return 0;
}

