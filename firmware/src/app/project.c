/* SPDX-License-Identifier: GPL-3.0-only */
/* OMNI project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h. */
project_t proj;

/* the factory chord buttons, left to right, in C: the primary chords on the first group of three,
 * their relative minors on the next two, then the dominants that lead into them */
static const uint8_t PAD_ROOT[NPADS] = {5, 0, 7, 2, 9, 4, 7, 4, 2, 10, 9};
static const uint8_t PAD_TYPE[NPADS] = {CH_MAJ, CH_MAJ, CH_MAJ, CH_MIN, CH_MIN, CH_MIN, CH_7, CH_7, CH_7, CH_MAJ,
                                        CH_7};

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = om_param_info(i)->def;
    for (i = 0; i < NPADS; i++) {
        proj.pad_root[i] = PAD_ROOT[i];
        proj.pad_type[i] = PAD_TYPE[i];
    }
    proj.leds = 1;
    proj.theme = (uint8_t)THEME_DEFAULT;
    proj.large = 0;
}

/* A saved project of format 1 or 2: the same fields, with sixteen parameters (it is from before the
 * envelopes' four times) */
typedef struct {
    uint32_t magic, format;
    int16_t par[16];
    uint8_t pad_root[NPADS], pad_type[NPADS];
    uint8_t hold, sync, leds, theme, large;
    uint8_t rsv[11];
} project_v2_t;

static int project_valid(void)
{
    int i;
    if (proj.magic != PROJ_MAGIC || proj.format != PROJ_FORMAT)
        return 0;
    for (i = 0; i < P_NPARAMS; i++) {
        const om_param_t *p = om_param_info(i);
        if (proj.par[i] < p->lo || proj.par[i] > p->hi)
            return 0;
    }
    for (i = 0; i < NPADS; i++)
        if (proj.pad_root[i] > 11u || proj.pad_type[i] >= CH_NTYPES)
            return 0;
    if (proj.theme >= NTHEMES) {                 /* a project from another firmware, or a palette gone */
        proj.theme = (uint8_t)THEME_DEFAULT;
    }
    proj.large = (uint8_t)(proj.large != 0);
    return 1;
}

int project_load(void)
{
    static union {                               /* static: the stack on the FM-1 is small */
        project_t now;
        project_v2_t old;
    } up;
    int n, i;
    memset(&up, 0, sizeof up);
    n = plat_store_load(OBJ_PROJ, &up, sizeof up);
    if (n >= 8 && up.now.magic == PROJ_MAGIC) {
        if (up.now.format == PROJ_FORMAT) {
            proj = up.now;
            if (project_valid())
                return 0;
        } else if (up.now.format == 2u || up.now.format == 1u) {   /* carried forward */
            memset(&proj, 0, sizeof proj);
            proj.magic = PROJ_MAGIC;
            proj.format = PROJ_FORMAT;
            memcpy(proj.par, up.old.par, sizeof up.old.par);
            for (i = 16; i < P_NPARAMS; i++)     /* the envelopes: as a fresh FM-1 has them */
                proj.par[i] = om_param_info(i)->def;
            if (up.now.format == 1u)             /* 0.1: that slot was Lo-fi (0 / 1) */
                proj.par[P_CREV] = om_param_info(P_CREV)->def;
            memcpy(proj.pad_root, up.old.pad_root, NPADS);
            memcpy(proj.pad_type, up.old.pad_type, NPADS);
            proj.hold = up.old.hold;
            proj.sync = up.old.sync;
            proj.leds = up.old.leds;
            proj.theme = up.old.theme;
            proj.large = up.old.large;
            if (project_valid())
                return 0;
        }
    }
    project_defaults();
    return -1;
}

int project_save(void) { return plat_store_save(OBJ_PROJ, &proj, sizeof proj); }

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        om_set(i, proj.par[i]);
}
