/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* Loader for Slamtilt modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Get the Slamtilt demo
 * from game/demo in Aminet.
 */

/* Tested with the Slamtilt modules sent by Sipos Attila */

#include "loader.h"

#define MAGIC_STIM	MAGIC4('S','T','I','M')

static int stim_test(FILE *, char *, const int);
static int stim_load(struct module_data *, FILE *, const int);

const struct format_loader stim_loader = {
	"Slamtilt",
	stim_test,
	stim_load
};

static int stim_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != MAGIC_STIM)
		return -1;

	read_title(f, t, 0);

	return 0;
}

struct stim_instrument {
	uint16 size;		/* Lenght of the sample (/2) */
	uint8 finetune;		/* Finetune (as ptk) */
	uint8 volume;		/* Volume (as ptk) */
	uint16 loop_start;	/* Loop start (/2) */
	uint16 loop_size;	/* Loop lenght (/2) */
};

struct stim_header {
	uint32 id;		/* "STIM" ID string */
	uint32 smpaddr;		/* Address of the sample descriptions */
	uint32 unknown[2];
	uint16 nos;		/* Number of samples (?) */
	uint16 len;		/* Size of pattern list */
	uint16 pat;		/* Number of patterns saved */
	uint8 order[128];	/* Pattern list */
	uint32 pataddr[64];	/* Pattern addresses (add 0xc) */
};

static int stim_load(struct module_data *m, FILE * f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	struct xmp_event *event;
	struct stim_header sh;
	struct stim_instrument si;
	uint8 b1, b2, b3;

	LOAD_INIT();

	sh.id = read32b(f);
	sh.smpaddr = read32b(f);
	read32b(f);
	read32b(f);
	sh.nos = read16b(f);
	sh.len = read16b(f);
	sh.pat = read16b(f);
	fread(&sh.order, 128, 1, f);
	for (i = 0; i < 64; i++)
		sh.pataddr[i] = read32b(f) + 0x0c;

	mod->len = sh.len;
	mod->pat = sh.pat;
	mod->ins = sh.nos;
	mod->smp = mod->ins;
	mod->trk = mod->pat * mod->chn;

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = sh.order[i];

	set_type(m, "Slamtilt");

	MODULE_INFO();

	PATTERN_INIT();

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		fseek(f, start + sh.pataddr[i] + 8, SEEK_SET);

		for (j = 0; j < 4; j++) {
			for (k = 0; k < 64; k++) {
				event = &EVENT(i, j, k);
				b1 = read8(f);

				if (b1 & 0x80) {
					k += b1 & 0x7f;
					continue;
				}

				/* STIM event format:
				 *
				 *     __ Fx __
				 *    /        \
				 *   ||        ||
				 *  0000 0000  0000 0000  0000 0000
				 *  |  |    |    |     |  |       |
				 *  |   \  /      \   /    \     /
				 *  |    smp      note      Fx Val
				 *  |
				 *  Description bit set to 0.
				 */

				b2 = read8(f);
				b3 = read8(f);

				if ((event->note = b2 & 0x3f) != 0)
					event->note += 47;
				event->ins = b1 & 0x1f;
				event->fxt = ((b2 >> 4) & 0x0c) | (b1 >> 5);
				event->fxp = b3;

				disable_continue_fx(event);
			}
		}
	}

	INSTRUMENT_INIT();

	D_(D_INFO "Stored samples: %d", mod->smp);

	fseek(f, start + sh.smpaddr + mod->smp * 4, SEEK_SET);

	for (i = 0; i < mod->smp; i++) {
		si.size = read16b(f);
		si.finetune = read8(f);
		si.volume = read8(f);
		si.loop_start = read16b(f);
		si.loop_size = read16b(f);

		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		mod->xxs[i].len = 2 * si.size;
		mod->xxs[i].lps = 2 * si.loop_start;
		mod->xxs[i].lpe = mod->xxs[i].lps + 2 * si.loop_size;
		mod->xxs[i].flg = si.loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = (int8) (si.finetune << 4);
		mod->xxi[i].sub[0].vol = si.volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].nsm = !!(mod->xxs[i].len);
		mod->xxi[i].rls = 0xfff;

		D_(D_INFO "[%2X] %04x %04x %04x %c V%02x %+d",
			       i, mod->xxs[i].len, mod->xxs[i].lps,
			       mod->xxs[i].lpe, si.loop_size > 1 ? 'L' : ' ',
			       mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin >> 4);

		if (!mod->xxs[i].len)
			continue;
		load_sample(f, 0, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	m->quirk |= QUIRK_MODRNG;

	return 0;
}