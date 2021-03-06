#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "zhban.h"

int usage() {
    fprintf(stderr, "Usage: zhbantest path-to-ttf-file path-to-text-file\n\n");
    fprintf(stderr, "  e.g. zhbantest /usr/share/fonts/truetype/droid/DroidSans.ttf /etc/lsb-release\n\n");
    return 1;
}

void *fufread(const char *fname, uint32_t *fsize) {
    FILE *tfp = fopen(fname, "r");
    if (!tfp) {
        fprintf(stderr, " fopen(%s): %s\n", fname, strerror(errno));
        return NULL;
    }

    fseek(tfp, 0, SEEK_END);
    *fsize = ftell(tfp);
    fseek(tfp, 0, SEEK_SET);

    void *buf = malloc(*fsize);
    if (!buf) {
        fprintf(stderr, " malloc(%u): %s\n", *fsize, strerror(errno));
        fclose(tfp);
        return NULL;
    }

    if (1 != fread(buf, *fsize, 1, tfp)) {
        fprintf(stderr, " fread(%s, %u): %s\n", fname, *fsize, strerror(errno));
        fclose(tfp);
        free(buf);
        return NULL;
    }
    fclose(tfp);
    return buf;
}

int main(int argc, char *argv[]) {
    uint32_t fsize;

    if (argc < 3)
        return usage();

    void *fbuf = fufread(argv[1], &fsize);
    if (!fbuf)
        return 1;

    zhban_t *zhban = zhban_open(fbuf, fsize, 18, 1, 1<<20, 1<<16, 1<<24, 5, NULL);
    if (!zhban)
        return 1;

    void *tbuf = fufread(argv[2], &fsize);
    if (!tbuf)
        return 1;

    /* testing strategy: ? */
    uint16_t *p, *ep, *et = (uint16_t*)tbuf + fsize/2 - 1;
    *et = 10;

    zhban_shape_t **zsa;
    zhban_bitmap_t *zb;
    int sindex;
    uint32_t zsa_sz = sizeof(zhban_shape_t *) * fsize/2;
    zsa = malloc(zsa_sz);
    if (!zsa) {
        fprintf(stderr, " malloc(%u): %s\n", zsa_sz, strerror(errno));
        goto zsa_fail;
    }
    for (int i=0; i< 1<<12 ; i++) {
        memset(zsa, 0, sizeof(zhban_shape_t *) * fsize/2);
        ep = p = (uint16_t *)tbuf;
        sindex = 0;
        if (*p == 0xFEFF)
            p++;
        do {
            while ((*ep != 10) && (et-ep > 0))
                ep++;
            //printf("str %p->%p: #%s#\n\n", p, ep, utf16to8(p, (ep-p)));
            zsa[sindex] = zhban_shape(zhban, p, 2*( ep - p));
            sindex ++;
            ep = p = ep + 1;
        } while(et - ep > 0);

        for (int j=0; j < sindex ; j++) {
            zb = zhban_render(zhban, zsa[j]);
            zhban_release_shape(zhban, zsa[j]);
            if (!zb)
                return 1;
        }
        break;
    }

    printf("glyph misses %d/%d; size %d;\n%d spans %d glyphs; %.2f spans/glyph\nshaper misses %d/%d size %d\nbitmap misses %d/%d size %d\n",
        zhban->glyph_gets - zhban->glyph_hits, zhban->glyph_gets, zhban->glyph_size,
        zhban->glyph_spans_seen, zhban->glyph_rendered, ((float)zhban->glyph_spans_seen)/zhban->glyph_rendered,
        zhban->shaper_gets - zhban->shaper_hits, zhban->shaper_gets, zhban->shaper_size,
        zhban->bitmap_gets - zhban->bitmap_hits, zhban->bitmap_gets, zhban->bitmap_size);

    zhban_drop(zhban);
    free(fbuf);
    free(tbuf);
  zsa_fail:
    free(zsa);
    return 1;
}
    