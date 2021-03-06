/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "period.h"

/* Nir Oren's Liquid Tracker old "NO" format. I have only one NO module,
 * Moti Radomski's "Time after time" from ftp.modland.com.
 */


static int no_test (FILE *, char *, const int);
static int no_load (struct module_data *, FILE *, const int);

const struct format_loader no_loader = {
	"Liquid Tracker NO (LIQ)",
	no_test,
	no_load
};

static int no_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != 0x4e4f0000)		/* NO 0x00 0x00 */
		return -1;

	read_title(f, t, read8(f));

	return 0;
}


static const uint8 fx[] = {
	FX_ARPEGGIO,
	0,
	FX_BREAK,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};


static int no_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j, k;
	int nsize;

	LOAD_INIT();

	read32b(f);			/* NO 0x00 0x00 */

	set_type(m, "Liquid Tracker");

	nsize = read8(f);
	for (i = 0; i < nsize; i++) {
		uint8 x = read8(f);
		if (i < XMP_NAME_SIZE)
			mod->name[i] = x;
	}

	read16l(f);
	read16l(f);
	read16l(f);
	read16l(f);
	read8(f);
	mod->pat = read8(f);
	read8(f);
	mod->chn = read8(f);
	mod->trk = mod->pat * mod->chn;
	read8(f);
	read16l(f);
	read16l(f);
	read8(f);
	mod->ins = mod->smp = 63;

	for (i = 0; i < 256; i++) {
		uint8 x = read8(f);
		if (x == 0xff)
			break;
		mod->xxo[i] = x;
	}
	fseek(f, 255 - i, SEEK_CUR);
	mod->len = i;

	MODULE_INFO();

	INSTRUMENT_INIT();

	/* Read instrument names */
	for (i = 0; i < mod->ins; i++) {
		int hasname, c2spd;

		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		nsize = read8(f);
		hasname = 0;
		for (j = 0; j < nsize; j++) {
			uint8 x = read8(f);
			if (x != 0x20)
				hasname = 1;
			if (j < 32)
				mod->xxi[i].name[j] = x;
		}
		if (!hasname)
			mod->xxi[i].name[0] = 0;

		read32l(f);
		read32l(f);
		mod->xxi[i].sub[0].vol = read8(f);
		c2spd = read16l(f);
		mod->xxs[i].len = read16l(f);
		mod->xxs[i].lps = read16l(f);
		mod->xxs[i].lpe = read16l(f);
		read32l(f);
		read16l(f);

		mod->xxi[i].nsm = !!(mod->xxs[i].len);
		mod->xxs[i].lps = 0;
		mod->xxs[i].lpe = 0;
		mod->xxs[i].flg = mod->xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = 0;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-22.22s  %04x %04x %04x %c V%02x %5d",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, c2spd);

		c2spd = 8363 * c2spd / 8448;
		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
	}

	PATTERN_INIT();

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d ", mod->pat);

	for (i = 0; i < mod->pat; i++) {
//printf("%d  %x\n", i, ftell(f));
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				uint32 x, note, ins, vol, fxt, fxp;

				event = &EVENT (i, k, j);

				x = read32l(f);
				note = x & 0x0000003f;
				ins = (x & 0x00001fc0) >> 6;
				vol = (x & 0x000fe000) >> 13;
				fxt = (x & 0x00f00000) >> 20;
				fxp = (x & 0xff000000) >> 24;

				if (note != 0x3f)
					event->note = 36 + note;
				if (ins != 0x7f)
					event->ins = 1 + ins;
				if (vol != 0x7f)
					event->vol = vol;
				if (fxt != 0x0f) {
					event->fxt = fx[fxt];
					event->fxp = fxp;
				}
			}
		}
	}

	/* Read samples */
	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxs[i].len == 0)
			continue;
		load_sample(f, SAMPLE_FLAG_UNS, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	m->quirk |= QUIRKS_ST3;
	m->read_event_type = READ_EVENT_ST3;

	return 0;
}
