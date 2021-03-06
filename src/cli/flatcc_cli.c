#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "flatcc/flatcc.h"
#include "config.h"

#define VERSION FLATCC_VERSION_TEXT
#define TITLE FLATCC_TITLE_TEXT

void usage(FILE *fp)
{
    fprintf(fp, "%s\n", TITLE);
    fprintf(fp, "version: %s\n", VERSION);
    fprintf(fp, "usage: flatcc [options] file [...]\n");
    fprintf(fp, "options:\n"
            "  -c                         Generate common include header(s)\n"
            "  -w                         Generate builders (writable buffers)\n"
            "  -r                         Recursively generate included schema files\n"
            "  -a                         Generate all (like -cwvr)\n"
            "  -I<inpath>                 Search path for include files (multiple allowed)\n"
            "  -o<outpath>                Write files to given output directory (must exist)\n"
            "  --stdout                   Concatenate all output to stdout\n"
            "  --prefix=<prefix>          Add prefix to all generated names (no _ added)\n"
            "  --common-prefix=<prefix>   Replace 'flatbuffers' prefix in common files\n"
#if FLATCC_REFLECTION
            "  --schema                   Generate binary schema (.bfbs)\n"
            "  --schema-namespace=yes     Generate namespace prefix in binary schema\n"
            "  --schema-length=no         Add length prefix to binary schema\n"
#endif
            "  --verifier                 Generate verifier for schema\n"
            "  --json-parser              Generate json parser for schema\n"
            "  --json-printer             Generate json printer for schema\n"
            "  --json                     Generate both json parser and printer for schema\n"
            "  --version                  Show version\n"
            "  -h | --help                Help message\n"
    );
}

void help(FILE *fp)
{
    usage(fp);
    fprintf(fp,
        "\n"
        "This is a flatbuffer compatible compiler implemented in C generating C\n"
        "source. It is largely compatible with the flatc compiler provided by\n"
        "Google Fun Propulsion Lab but does not support JSON objects or binary schema.\n"
        "\n"
        "By example 'flatcc monster.fbs' generates a 'monster.h' file which\n"
        "provides functions to read a flatbuffer. A common include header is also\n"
        "required. The common file is generated with the -c option. The reader has\n"
        "no external dependencies.\n"
        "\n"
        "The -w option enables code generation to write buffers: `flatbuffers\n"
        "-w monster.fbs` will generate `monster.h` and `monster_builder.h`, and\n"
        "also a builder specific common file with the -cw option. The builder\n"
        "must link with the extern `flatbuilder` library.\n"
        "\n"
        "-v generates a verifier file per schema. It depends on the runtime library\n"
        "but not on other generated files, except other included verifiers.\n"
        "\n"
        "All C output can be concatenated to a single file using --stdout. This is\n"
        "the exact same content as the generated files ordered by dependencies.\n"
        "\n"
#if FLATCC_REFLECTION
        "--schema will generate a binary .bfbs file for each top-level schema file.\n"
        "Can be used with --stdout if no C output is specified. When used with multiple\n"
        "files --schema-length=yes is recommend.\n"
        "\n"
        "--schema-namespace controls if typenames in schema are prefixed a namespace.\n"
        "\n"
        "--schema-length adds a length prefix of type uoffset_t to binary schema so\n"
        "they can be concatenated - the aligned buffer starts after the prefix.\n"
        "\n"
#else
        "Flatbuffers binary schema support (--schema) has been disabled."
        "\n"
#endif
        "--json-parser generates a file that implements a fast typed json parser for\n"
        "the schema. It depends on some flatcc headers and the runtime library but\n"
        "not on other generated files except other parsers from included schema.\n"
        "\n"
        "--json-printer generates a file that implements json printers for the schema\n"
        "and has dependencies similar to --json-parser.\n"
        "\n"
        "--json is generates both printer and parser.\n"
        "\n"
        "The generated source can redefine offset sizes by including a modified\n"
        "`flattypes.h` file. The flatbuilder library must then be compiled with the\n"
        "same `flattypes.h` file. In this case --prefix and --common-prefix options\n"
        "may be helpful to avoid conflict with standard offset sizes.\n"
        "\n"
        "The output size may seem bulky, but most content is rarely used inline\n"
        "functions and macros. The compiled binary need not be large.\n"
        "\n"
        "The generated source assumes C11 functionality for alignment, compile\n"
        "time assertions and inline functions but an optional set of portability\n"
        "headers can be included to work with most any compiler. The portability\n"
        "layer is not throughly tested so a platform specific test is required\n"
        "before production use. Upstream patches are welcome.\n");
}

enum { noarg, suffixarg, nextarg };

int parse_bool_arg(const char *a)
{
    if (strcmp(a, "0") == 0 || strcmp(a, "no") == 0) {
        return 0;
    }
    if (strcmp(a, "1") == 0 || strcmp(a, "yes") == 0) {
        return 1;
    }
    fprintf(stderr, "invalid boolean argument: '%s', must be '0', '1', 'yes' or 'no'\n", a);
    return -1;
}

int match_long_arg(const char *option, const char *s, size_t n)
{
    return strncmp(option, s, n) == 0 && strlen(option) == n;
}

int set_opt(flatcc_options_t *opts, const char *s, const char *a)
{
    int ret = noarg;
    size_t n = strlen(s);
    const char *v = strchr(s, '=');
    if (v) {
        a = v + 1;
        n = v - s;
    }
    if (*s == 'h' || 0 == strcmp("-help", s)) {
        /* stdout so less and more works. */
        help(stdout);
        exit(0);
    }
    if (0 == strcmp("-version", s)) {
        fprintf(stderr, "%s\n", TITLE);
        fprintf(stderr, "version: %s\n", VERSION);
        exit(0);
    }
    if (0 == strcmp("-stdout", s)) {
        opts->gen_stdout = 1;
        return noarg;
    }
#if FLATCC_REFLECTION
    if (0 == strcmp("-schema", s)) {
        opts->bgen_bfbs = 1;
        return noarg;
    }
#endif
    if (0 == strcmp("-json-parser", s)) {
        opts->cgen_json_parser = 1;
        return noarg;
    }
    if (0 == strcmp("-json-printer", s)) {
        opts->cgen_json_printer = 1;
        return noarg;
    }
    if (0 == strcmp("-json", s)) {
        opts->cgen_json_parser = 1;
        opts->cgen_json_printer = 1;
        return noarg;
    }
#if FLATCC_REFLECTION
    if (match_long_arg("-schema-namespace", s, n)) {
        if (!a) {
            fprintf(stderr, "--schema-namespace option needs an argument\n");
            exit(-1);
        }
        if(0 > (opts->bgen_qualify_names = parse_bool_arg(a))) {
            exit(-1);
        }
        return v ? noarg : nextarg;
    }
    if (match_long_arg("-schema-length", s, n)) {
        if (!a) {
            fprintf(stderr, "--schema-length option needs an argument\n");
            exit(-1);
        }
        if(0 > (opts->bgen_length_prefix = parse_bool_arg(a))) {
            exit(-1);
        }
        return v ? noarg : nextarg;
    }
#endif
    if (match_long_arg("-common-prefix", s, n)) {
        if (!a) {
            fprintf(stderr, "--common-prefix option needs an argument\n");
            exit(-1);
        }
        opts->nsc = a;
        return v ? noarg : nextarg;
    }
    if (match_long_arg("-prefix", s, n)) {
        if (!a) {
            fprintf(stderr, "-n option needs an argument\n");
            exit(-1);
        }
        opts->ns = a;
        return v ? noarg : nextarg;
    }
    switch (*s) {
    case '-':
        fprintf(stderr, "invalid option: -%s\n", s);
        exit(-1);
    case 'I':
        if (s[1]) {
            ret = suffixarg;
            a = s + 1;
        } else if (!a) {
            fprintf(stderr, "-I option needs an argument\n");
            exit(-1);
        } else {
            ret = nextarg;
        }
        opts->inpaths[opts->inpath_count++] = a;
        return ret;
    case 'o':
        if (opts->outpath) {
            fprintf(stderr, "-o option can only be specified once\n");
            exit(-1);
        }
        if (s[1]) {
            ret = suffixarg;
            a = s + 1;
        } else if (!a) {
            fprintf(stderr, "-o option needs an argument\n");
            exit(-1);
        } else {
            ret = nextarg;
        }
        opts->outpath = a;
        return ret;
    case 'w':
        opts->cgen_builder = 1;
        return noarg;
    case 'v':
        opts->cgen_verifier = 1;
        return noarg;
    case 'c':
        opts->cgen_common_reader = 1;
        return noarg;
    case 'r':
        opts->cgen_recursive = 1;
        return noarg;
    case 'a':
        opts->cgen_builder = 1;
        opts->cgen_verifier = 1;
        opts->cgen_common_reader = 1;
        opts->cgen_recursive = 1;
        return noarg;
    default:
        fprintf(stderr, "invalid option: -%c\n", *s);
        exit(-1);
    }
    return noarg;
}

int get_opt(flatcc_options_t *opts, const char *s, const char *a)
{
    if (s[1] == '-') {
        return nextarg == set_opt(opts, s + 1, a);
    }
    ++s;
    if (*s == 0) {
        fprintf(stderr, "- is not a valid option\n");
        exit(-1);
    }
    while (*s) {
        switch (set_opt(opts, s, a)) {
        case noarg:
            ++s;
            continue;
        case suffixarg:
            return 0;
        case nextarg:
            return 1;
        }
    }
    return noarg;
}

int main(int argc, const char *argv[])
{
    flatcc_options_t opts;
    flatcc_context_t ctx;
    int i, ret, status, cgen;
    const char *s, *a;

    ctx = 0;
    status = 0;
    if (argc < 2) {
        usage(stderr);
        exit(-1);
    }
    flatcc_init_options(&opts);
    opts.inpaths = malloc(argc * sizeof(char *));

    for (i = 1; i < argc && argv[i][0] == '-'; ++i) {
        s = argv[i];
        a = i + 1 < argc ? argv[i + 1] : 0;
        i += get_opt(&opts, s, a);
    }
    if (opts.gen_stdout && opts.outpath) {
        fprintf(stderr, "--stdout is not compatible with -o option\n");
        status = -1;
        goto done;
    }
    opts.cgen_common_builder = opts.cgen_builder && opts.cgen_common_reader;
    if (i == argc) {
        /* No input files, so only generate header. */
        if (!opts.cgen_common_reader || opts.bgen_bfbs) {
            fprintf(stderr, "filename missing\n");
            status = -1;
            goto done;
        }
        ctx = flatcc_create_context(&opts, 0, 0, 0);
        ret = flatcc_generate_files(ctx);
        flatcc_destroy_context(ctx);
        status = ret ? -1 : 0;
        goto done;
    }
    cgen = opts.cgen_reader || opts.cgen_builder || opts.cgen_verifier
        || opts.cgen_common_reader || opts.cgen_common_builder
        || opts.cgen_json_parser || opts.cgen_json_printer;
    if (!opts.bgen_bfbs && (!cgen || opts.cgen_builder)) {
        /* Assume default if no other output specified. */
        opts.cgen_reader = 1;
    }
    if (opts.bgen_bfbs && cgen) {
        if (opts.gen_stdout) {
            fprintf(stderr, "--stdout cannot be used with mixed text and binary output");
            status = -1;
            goto done;
        }
    }
    for (; i < argc; ++i) {
        ctx = flatcc_create_context(&opts, argv[i], 0, 0);
        ret = flatcc_parse_file(ctx, argv[i]);
        status |= ret;
        if (!ret) {
            status |= flatcc_generate_files(ctx);
        }
        flatcc_destroy_context(ctx);
        ctx = 0;
        /* Only generate common files once. */
        opts.cgen_common_reader = 0;
        opts.cgen_common_builder = 0;
    }
done:
    if (status) {
        fprintf(stderr, "output failed\n");
    }
    free(opts.inpaths);
    return status ? -1 : 0;
}
