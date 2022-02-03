#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sndkit/graforge/graforge.h>
#include <sndkit/core.h>
#include <sndkit/lil/lil.h>
#include <sndkit/nodes/sklil.h>
#include "uxn.h"

typedef struct uxnseq uxnseq;

struct seqstate {
    int val;
    int dur;
    int cnt;
    unsigned int ptr;
    int tik;
};

struct uxnseq_node {
    uxnseq *us;
    gf_cable *clk;
    gf_cable *out;
    struct seqstate ss;
};

struct uxnseq {
    Uxn u;
    struct seqstate *ss;

};

static void nil_deo(Device *d, Uint8 port)
{
	if(port == 0x1) d->vector = peek16(d->dat, 0x0);
}

static Uint8 nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void
console_deo(Device *d, Uint8 port)
{
	if(port == 0x1)
		d->vector = peek16(d->dat, 0x0);
	if(port > 0x7)
		write(port - 0x7, (char *)&d->dat[port], 1);
}

static void uxn_deo(Device *d, Uint8 port)
{
    uxnseq *us;
    struct seqstate *ss;

    us = (uxnseq *)d->u;
    ss = us->ss;

    if (port == 0) {
        ss->val = d->dat[port];
    } else if (port == 1) {
        ss->dur = d->dat[port];
    }
}

int uxnseq_load(Uxn *u, const char *rom)
{
	FILE *f;
	int r;

    char sym[3];

	if(!(f = fopen(rom, "rb"))) return 1;

    sym[0] = sym[1] = sym[2] = 0;

    fread(sym, 1, 3, f);

    if (sym[0] == 'S' && sym[1] == 'Y' && sym[2] == 'M') {
        unsigned char b[2];
        unsigned short sz;
        b[0] = b[1] = 0;
        fread(b, 1, 2, f);
        sz = b[0] | (b[1] << 8);
        fseek(f, sz, SEEK_CUR);

    } else fseek(f, 0L, SEEK_SET);

	r = fread(u->ram.dat + PAGE_PROGRAM,
              1, sizeof(u->ram.dat) - PAGE_PROGRAM, f);
	fclose(f);
	if(r < 1) return 1;
	return 0;
}

int uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	return 0;
}

static void compute(gf_node *node)
{
    int n;
    int blksize;
    struct uxnseq_node *usn;
    struct seqstate *ss;
    Uxn *u;

    blksize = gf_node_blksize(node);
    usn = gf_node_get_data(node);

    ss = &usn->ss;
    u = &usn->us->u;

    u->ram.ptr = ss->ptr;

    usn->us->ss = ss;
    ss->tik = -1;
    for (n = 0; n < blksize; n++) {
        SKFLT in;

        in = gf_cable_get(usn->clk, n);

        if (in != 0) {
            if (ss->cnt == 0) {
                ss->tik = n;
                /* printf("%x: tick %d\n", ss, ss->dur); */
                /* printf("stack pos: %d\n", u->wst.ptr); */
                uxn_eval(u, u->ram.ptr);
                ss->cnt = ss->dur;
            }
            ss->cnt--;
        }

        gf_cable_set(usn->out, n, ss->val);

    }

    ss->ptr = u->ram.ptr;
    usn->us->ss = NULL;
}

static void destroy(gf_node *node)
{
    gf_patch *patch;
    int rc;
    void *ud;
    rc = gf_node_get_patch(node, &patch);
    if (rc != GF_OK) return;
    gf_node_cables_free(node);
    ud = gf_node_get_data(node);
    gf_memory_free(patch, &ud);
}

int sk_node_uxnseq(sk_core *core, unsigned int ptr)
{
    int rc;
    void *ud;
    gf_patch *patch;
    uxnseq *us;
    struct uxnseq_node *usn;
    gf_node *node;
    sk_param clk;
    struct seqstate *ss;

    rc = sk_param_get(core, &clk);
    SK_ERROR_CHECK(rc);

    rc = sk_core_generic_pop(core, &ud);
    SK_ERROR_CHECK(rc);
    us = ud;

    patch = sk_core_patch(core);
    rc = gf_memory_alloc(patch, sizeof(struct uxnseq_node), &ud);

    if (rc) return 1;

    usn = ud;

    ss = &usn->ss;
    usn->us = us;

    ss->val = 0;
    ss->dur = 1;
    ss->cnt = 0;
    ss->ptr = ptr;
    ss->tik = -1;

    rc = gf_patch_new_node(patch, &node);
    SK_GF_ERROR_CHECK(rc);

    rc = gf_node_cables_alloc(node, 2);
    SK_GF_ERROR_CHECK(rc);
    gf_node_set_block(node, 1);

    gf_node_get_cable(node, 0, &usn->clk);
    gf_node_get_cable(node, 1, &usn->out);

    gf_node_set_data(node, usn);
    gf_node_set_compute(node, compute);
    gf_node_set_destroy(node, destroy);

    sk_param_set(core, node, &clk, 0);
    sk_param_out(core, node, 1);


    /* set to be the current state uxnseq */
    /* needed for uxnseqlast */
    us->ss = ss;
    return 0;
}

static lil_value_t l_uxnseqnode(lil_t lil,
                                size_t argc,
                                lil_value_t *argv)
{
    sk_core *core;
    int rc;
    unsigned int ptr;

    core = lil_get_data(lil);

    SKLIL_ARITY_CHECK(lil, "uxnseq", argc, 2);

    ptr = lil_to_integer(argv[1]);

    rc = sklil_param(core, argv[2]);
    SKLIL_PARAM_CHECK(lil, rc, "uxnseq");

    rc = sk_node_uxnseq(core, ptr);
    SKLIL_ERROR_CHECK(lil, rc, "uxnseq didn't work out.");
    return NULL;
}

static void deluxn(void *ptr)
{
    free(ptr);
}

static lil_value_t l_uxnseqnew(lil_t lil,
                               size_t argc,
                               lil_value_t *argv)
{
    sk_core *core;
    int rc;
    const char *key;
    uxnseq *us;
    Uxn *u;
    int i;

    core = lil_get_data(lil);

    SKLIL_ARITY_CHECK(lil, "uxnseqnew", argc, 1);

    key = lil_to_string(argv[0]);

    us = malloc(sizeof(uxnseq));
    u = &us->u;

    uxn_boot(u);

    for (i = 0x0; i <= 0xf; i++) {
        uxn_port(u, i, nil_dei, nil_deo);
    }

	uxn_port(u, 0x2, nil_dei, uxn_deo);
	uxn_port(u, 0x1, nil_dei, console_deo);

    rc = sk_core_append(core, key, strlen(key), us, deluxn);

    SKLIL_ERROR_CHECK(lil, rc, "uxnseq didn't work out.");
    return NULL;
}

static lil_value_t l_uxnseqload(lil_t lil,
                                size_t argc,
                                lil_value_t *argv)
{
    int rc;
    const char *rom;
    void *ud;
    uxnseq *us;
    sk_core *core;

    SKLIL_ARITY_CHECK(lil, "uxnseqload", argc, 2);

    core = lil_get_data(lil);
    rc = sk_core_generic_pop(core, &ud);
    SKLIL_ERROR_CHECK(lil, rc, "could not pop uxnseq");
    us = ud;

    rom = lil_to_string(argv[1]);

    rc = uxnseq_load(&us->u, rom);

    SKLIL_ERROR_CHECK(lil, rc, "issue loading ROM");

    /* uxn_eval(&us->u, PAGE_PROGRAM); */
    return NULL;
}

static lil_value_t l_uxnsym(lil_t lil,
                            size_t argc,
                            lil_value_t *argv)
{
    const char *rom;
    const char *sym;
    char buf[64];
    FILE *fp;
    unsigned int addr;
    unsigned short sz;
    unsigned char symlen;

    addr = 0;

    SKLIL_ARITY_CHECK(lil, "uxnsym", argc, 2);

    rom = lil_to_string(argv[0]);
    sym = lil_to_string(argv[1]);
    symlen = strlen(sym);

    fp = fopen(rom, "r");

    if (fp == NULL) {
        lil_set_error(lil, "could not open ROM file");
        return NULL;
    }

    memset(buf, 0, 64);

    fread(buf, 1, 3, fp);

    if (buf[0] != 'S' || buf[1] != 'Y' || buf[2] != 'M') {
        lil_set_error(lil, "ROM file does not have a symbol table.");
        return NULL;
    }

    sz = 0;
    fread(buf, 1, 2, fp);

    sz = buf[0] + (buf[1] << 8);

    while (sz) {
        unsigned char len;
        fread(&len, 1, 1, fp);

        if (len == symlen) {
            int i;
            int match;
            fread(buf, 1, len, fp);
            match = 1;
            for (i = 0; i < len; i++) {
                if (buf[i] != sym[i]) {
                    match = 0;
                    break;
                }
            }

            if (match) {
                fread(buf, 1, 2, fp);
                addr = buf[0] + (buf[1] << 8);
                break;
            } else {
                fseek(fp, 2, SEEK_CUR);
            }
        } else {
            fseek(fp, len + 2, SEEK_CUR);
        }

        sz -= (len + 2 + 1);
    }

    fclose(fp);

    if (addr == 0) {
        lil_set_error(lil, "Could not find uxn symbol");
        return NULL;
    }

    return lil_alloc_integer(addr);
}

static void tkcompute(gf_node *node)
{
    int blksize;
    int n;
    gf_cable *out;
    struct seqstate *ss;

    blksize = gf_node_blksize(node);
    gf_node_get_cable(node, 0, &out);
    ss = gf_node_get_data(node);

    for (n = 0; n < blksize; n++) {
        SKFLT o;

        o = 0;
        if (n == ss->tik) o = 1;
        gf_cable_set(out, n, o);
    }
}

int sk_node_uxnseqtk(sk_core *core, struct seqstate *ss)
{
    int rc;
    gf_patch *patch;
    gf_node *node;

    patch = sk_core_patch(core);

    rc = gf_patch_new_node(patch, &node);
    SK_GF_ERROR_CHECK(rc);

    rc = gf_node_cables_alloc(node, 1);
    SK_GF_ERROR_CHECK(rc);
    gf_node_set_block(node, 0);

    gf_node_set_data(node, ss);
    gf_node_set_compute(node, tkcompute);

    sk_param_out(core, node, 0);

    return 0;
}

static lil_value_t l_uxnseqtk(lil_t lil,
                              size_t argc,
                              lil_value_t *argv)
{
    sk_core *core;
    int rc;
    struct seqstate *ss;
    void *ud;

    SKLIL_ARITY_CHECK(lil, "uxnseqtk", argc, 1);
    core = lil_get_data(lil);
    rc = sk_core_generic_pop(core, &ud);
    SKLIL_ERROR_CHECK(lil, rc, "could not get seqstate.");
    ss = ud;

    rc = sk_node_uxnseqtk(core, ss);
    SKLIL_ERROR_CHECK(lil, rc, "uxnseqtk didn't work out.");
    return NULL;
}

static lil_value_t l_uxnseqlast(lil_t lil,
                                size_t argc,
                                lil_value_t *argv)
{
    sk_core *core;
    int rc;
    uxnseq *us;
    void *ud;

    SKLIL_ARITY_CHECK(lil, "uxnseqlast", argc, 1);

    core = lil_get_data(lil);
    rc = sk_core_generic_pop(core, &ud);
    SKLIL_ERROR_CHECK(lil, rc, "could not pop uxnseq");
    us = ud;

    rc = sk_core_generic_push(core, us->ss);

    SKLIL_ERROR_CHECK(lil, rc, "could not push uxnseq last");
    return NULL;
}

static lil_value_t l_uxnseqeval(lil_t lil,
                                size_t argc,
                                lil_value_t *argv)
{
    sk_core *core;
    int rc;
    uxnseq *us;
    void *ud;
    unsigned short ptr;

    SKLIL_ARITY_CHECK(lil, "uxnseqeval", argc, 2);

    core = lil_get_data(lil);
    rc = sk_core_generic_pop(core, &ud);
    SKLIL_ERROR_CHECK(lil, rc, "could not pop uxnseq");
    us = ud;

    ptr = lil_to_integer(argv[1]);
    uxn_eval(&us->u, ptr);


    return NULL;
}

static void load(lil_t lil)
{
    sklil_loader_withextra(lil);
    lil_register(lil, "uxnseqnew", l_uxnseqnew);
    lil_register(lil, "uxnseqnode", l_uxnseqnode);
    lil_register(lil, "uxnseqload", l_uxnseqload);
    lil_register(lil, "uxnsym", l_uxnsym);
    lil_register(lil, "uxnseqtk", l_uxnseqtk);
    lil_register(lil, "uxnseqlast", l_uxnseqlast);
    lil_register(lil, "uxnseqeval", l_uxnseqeval);
}

static void clean(lil_t lil)
{
    sklil_clean(lil);
}

int main(int argc, char *argv[])
{
    return lil_main(argc, argv, load, clean);
}
