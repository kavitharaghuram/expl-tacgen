#include "tac.h"
#include "tacprint.h"
#include "tacread.h"
#include "blocks.h"
#include "stats.h"
#include "ast.h"
#include "tacgen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern FILE *yyin;
extern int yyparse(void);
extern Program *parse_result;
extern Quad *optimize_block(Block *b, int *n);

static void get_base_name(const char *path, char *base) {
    const char *slash = strrchr(path, '/');
    const char *start = slash ? slash + 1 : path;
    strcpy(base, start);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.expl | input.tac> [--tac] [-o outdir]\n", argv[0]);
        return 1;
    }

    int is_tac_mode = 0;
    const char *input_path = NULL;
    const char *outdir = ".";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tac") == 0) {
            is_tac_mode = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outdir = argv[++i];
        } else if (argv[i][0] != '-') {
            input_path = argv[i];
        }
    }

    if (!input_path) {
        fprintf(stderr, "Error: No input file provided.\n");
        return 1;
    }

    char base[128];
    get_base_name(input_path, base);
    stats_reset();

    Quad *unopt_quads = NULL;
    int unopt_n = 0;

    if (is_tac_mode || strstr(input_path, ".tac")) {
        FILE *tf = fopen(input_path, "r");
        if (!tf) {
            perror("Failed to open input TAC file");
            return 1;
        }
        unopt_quads = read_tac(tf, &unopt_n);
        fclose(tf);
    } else {
        yyin = fopen(input_path, "r");
        if (!yyin) {
            perror("Failed to open input ExpL file");
            return 1;
        }
        if (yyparse() != 0 || !parse_result) {
            fprintf(stderr, "Parsing failed on source: %s\n", input_path);
            fclose(yyin);
            return 1;
        }
        fclose(yyin);

        unopt_quads = gen_tac(parse_result, &unopt_n);
    }

    stats.quads_before = unopt_n;

    /* Write unoptimized TAC */
    char unopt_filename[256];
    snprintf(unopt_filename, sizeof(unopt_filename), "%s/%s.unopt.tac", outdir, base);
    FILE *f_unopt = fopen(unopt_filename, "w");
    if (f_unopt) {
        print_quads(f_unopt, unopt_quads, unopt_n);
        fclose(f_unopt);
    }

    /* ALSU 8.4: Basic block partitioning */
    int nb = 0;
    Block *blocks = split_blocks(unopt_quads, unopt_n, &nb);

    /* Local block optimization */
    QList opt_list;
    qlist_init(&opt_list);

    for (int i = 0; i < nb; i++) {
        int opt_count = 0;
        Quad *opt_q = optimize_block(&blocks[i], &opt_count);
        for (int j = 0; j < opt_count; j++) {
            qlist_add(&opt_list, opt_q[j]);
        }
        if (opt_q) free(opt_q);
    }

    stats.quads_after = opt_list.n;

    /* Write optimized TAC */
    char opt_filename[256];
    snprintf(opt_filename, sizeof(opt_filename), "%s/%s.opt.tac", outdir, base);
    FILE *f_opt = fopen(opt_filename, "w");
    if (f_opt) {
        print_quads(f_opt, opt_list.q, opt_list.n);
        fclose(f_opt);
    }

    /* Write optimization statistics */
    char stats_filename[256];
    snprintf(stats_filename, sizeof(stats_filename), "%s/%s.stats.txt", outdir, base);
    write_stats(stats_filename, input_path);

    /* Free allocated resources */
    if (unopt_quads) free(unopt_quads);
    free_blocks(blocks, nb);
    qlist_free(&opt_list);

    return 0;
}
