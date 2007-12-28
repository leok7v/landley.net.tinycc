/*
 *  options.c - Option parsing logic for tinycc
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *  Copyright (c) 2006-2007 Rob Landley
 *
 *  Licensed under GPLv2, see file LICENSE in this tarball
 */

#include "tcc.h"


void *xmalloc(unsigned long size);
void dynarray_add(void ***ptab, int *nb_ptr, void *data);
void add_dynarray_path(TCCState *s, char *pathname, struct dynarray *dd);
int strstart(char *str, char *val, char **ptr);
void warning(char *fmt, ...);
int init_output_type(TCCState *s);

extern char *tinycc_path;
int do_bounds_check = 0;
int do_debug = 0;
int next_tok_flags;

// Benchmark info
int do_bench = 0;
int total_lines;
int total_bytes;
int tok_ident;

// inlines
int is_space(int ch);


#define WD_ALL    0x0001 /* warning is activated when using -Wall */
#define FD_INVERT 0x0002 /* invert value before storing */

typedef struct FlagDef {
    uint16_t offset;
    uint16_t flags;
    char *name;
} FlagDef;

static FlagDef warning_defs[] = {
    { offsetof(TCCState, warn_unsupported), 0, "unsupported" },
    { offsetof(TCCState, warn_write_strings), 0, "write-strings" },
    { offsetof(TCCState, warn_error), 0, "error" },
    { offsetof(TCCState, warn_implicit_function_declaration), WD_ALL,
      "implicit-function-declaration" },
};

static int set_flag(TCCState *s, FlagDef *flags, int nb_flags,
                    char *name, int value)
{
    int i;
    FlagDef *p;
    char *r;

    r = name;
    if (r[0] == 'n' && r[1] == 'o' && r[2] == '-') {
        r += 3;
        value = !value;
    }
    for(i = 0, p = flags; i < nb_flags; i++, p++) {
        if (!strcmp(r, p->name))
            goto found;
    }
    return -1;
 found:
    if (p->flags & FD_INVERT)
        value = !value;
    *(int *)((uint8_t *)s + p->offset) = value;
    return 0;
}


/* set/reset a warning */
int tcc_set_warning(TCCState *s, char *warning_name, int value)
{
    int i;
    FlagDef *p;

    if (!strcmp(warning_name, "all")) {
        for(i = 0, p = warning_defs; i < countof(warning_defs); i++, p++) {
            if (p->flags & WD_ALL)
                *(int *)((uint8_t *)s + p->offset) = 1;
        }
        return 0;
    } else {
        return set_flag(s, warning_defs, countof(warning_defs),
                        warning_name, value);
    }
}

static FlagDef flag_defs[] = {
    { offsetof(TCCState, char_is_unsigned), 0, "unsigned-char" },
    { offsetof(TCCState, char_is_unsigned), FD_INVERT, "signed-char" },
    { offsetof(TCCState, nocommon), FD_INVERT, "common" },
    { offsetof(TCCState, leading_underscore), 0, "leading-underscore" },
};

/* set/reset a flag */
int tcc_set_flag(TCCState *s, char *flag_name, int value)
{
    return set_flag(s, flag_defs, countof(flag_defs),
                    flag_name, value);
}

/* extract the basename of a file */
static char *tcc_basename(char *name)
{
    char *p;
    p = strrchr(name, '/');
#ifdef WIN32
    if (!p)
        p = strrchr(name, '\\');
#endif    
    if (!p)
        p = name;
    else 
        p++;
    return p;
}

#if !defined(LIBTCC)

static int64_t getclock_us(void)
{
#ifdef WIN32
    struct _timeb tb;
    _ftime(&tb);
    return (tb.time * 1000LL + tb.millitm) * 1000LL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
#endif
}

void show_version(void)
{
    printf("tinycc version " TINYCC_VERSION "\n");
}

void help(TCCState *s)
{
    show_version();
    printf("Tiny C Compiler - Copyright (C) 2001-2006 Fabrice Bellard, 2007 Rob Landley\n"
           "usage: tcc [-v] [-c] [-o outfile] [-Bdir] [-bench] [-Idir] [-Dsym[=val]] [-Usym]\n"
           "           [-Wwarn] [-g] [-b] [-bt N] [-Ldir] [-llib] [-shared] [-static]\n"
           "           [infile1 infile2...] [-run infile args...]\n"
           "\n"
           "General options:\n"
           "  -v          Verbose compile, repeat for more verbosity\n"
           "  -c          compile only - generate an object file\n"
           "  -o outfile  set output filename\n"
           "  -Bdir       set tcc internal library path\n"
           "  -bench      output compilation statistics\n"
           "  -run        run compiled source\n"
           "  -fflag      set or reset (with 'no-' prefix) 'flag' (see man page)\n"
           "  -Wwarning   set or reset (with 'no-' prefix) 'warning' (see man page)\n"
           "  -w          disable all warnings\n"
           "Preprocessor options:\n"
           "  -Idir       add include path 'dir'\n"
           "  -Dsym[=val] define 'sym' with value 'val'\n"
           "  -Usym       undefine 'sym'\n"
           "  -E          preprocess only\n"
           "Linker options:\n"
           "  -Ldir       add library path 'dir'\n"
           "  -llib       link with dynamic or static library 'lib'\n"
           "  -shared     generate a shared library\n"
           "  -static     static linking\n"
           "  -rdynamic   export all global symbols to dynamic linker\n"
           "  -r          output relocatable .o file\n"
           "Debugger options:\n"
           "  -g          generate runtime debug info\n"
#ifdef CONFIG_TCC_BCHECK
           "  -b          compile with built-in memory and bounds checker (implies -g)\n"
#endif
           );
}

#define TCC_OPTION_HAS_ARG 0x0001
#define TCC_OPTION_NOSEP   0x0002 /* cannot have space before option and arg */

typedef struct TCCOption {
    char *name;
    uint16_t index;
    uint16_t flags;
} TCCOption;

enum {
    TCC_OPTION_HELP,
    TCC_OPTION_I,
    TCC_OPTION_D,
    TCC_OPTION_E,
    TCC_OPTION_U,
    TCC_OPTION_L,
    TCC_OPTION_B,
    TCC_OPTION_l,
    TCC_OPTION_bench,
    TCC_OPTION_b,
    TCC_OPTION_g,
    TCC_OPTION_c,
    TCC_OPTION_static,
    TCC_OPTION_shared,
    TCC_OPTION_o,
    TCC_OPTION_r,
    TCC_OPTION_Wl,
    TCC_OPTION_W,
    TCC_OPTION_O,
    TCC_OPTION_m,
    TCC_OPTION_f,
    TCC_OPTION_nostdinc,
    TCC_OPTION_nostdlib,
    TCC_OPTION_print_search_dirs,
    TCC_OPTION_rdynamic,
    TCC_OPTION_run,
    TCC_OPTION_v,
    TCC_OPTION_w,
    TCC_OPTION_pipe,
};

static TCCOption tcc_options[] = {
    { "h", TCC_OPTION_HELP, 0 },
    { "?", TCC_OPTION_HELP, 0 },
    { "I", TCC_OPTION_I, TCC_OPTION_HAS_ARG },
    { "D", TCC_OPTION_D, TCC_OPTION_HAS_ARG },
    { "E", TCC_OPTION_E, 0 },
    { "U", TCC_OPTION_U, TCC_OPTION_HAS_ARG },
    { "L", TCC_OPTION_L, TCC_OPTION_HAS_ARG },
    { "B", TCC_OPTION_B, TCC_OPTION_HAS_ARG },
    { "l", TCC_OPTION_l, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "bench", TCC_OPTION_bench, 0 },
#ifdef CONFIG_TCC_BCHECK
    { "b", TCC_OPTION_b, 0 },
#endif
    { "g", TCC_OPTION_g, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "c", TCC_OPTION_c, 0 },
    { "static", TCC_OPTION_static, 0 },
    { "shared", TCC_OPTION_shared, 0 },
    { "o", TCC_OPTION_o, TCC_OPTION_HAS_ARG },
    { "run", TCC_OPTION_run, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "rdynamic", TCC_OPTION_rdynamic, 0 },
    { "r", TCC_OPTION_r, 0 },
    { "Wl,", TCC_OPTION_Wl, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "W", TCC_OPTION_W, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "O", TCC_OPTION_O, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "m", TCC_OPTION_m, TCC_OPTION_HAS_ARG },
    { "f", TCC_OPTION_f, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "nostdinc", TCC_OPTION_nostdinc, 0 },
    { "nostdlib", TCC_OPTION_nostdlib, 0 },
    { "print-search-dirs", TCC_OPTION_print_search_dirs, 0 }, 
    { "v", TCC_OPTION_v, 0 },
    { "w", TCC_OPTION_w, 0 },
    { "pipe", TCC_OPTION_pipe, 0},
    { NULL },
};

/* convert 'str' into an array of space separated strings */
static int expand_args(char ***pargv, char *str)
{
    char *s1;
    char **argv, *arg;
    int argc, len;

    argc = 0;
    argv = NULL;
    for(;;) {
        while (is_space(*str))
            str++;
        if (*str == '\0')
            break;
        s1 = str;
        while (*str != '\0' && !is_space(*str))
            str++;
        len = str - s1;
        arg = xmalloc(len + 1);
        memcpy(arg, s1, len);
        arg[len] = '\0';
        dynarray_add((void ***)&argv, &argc, arg);
    }
    *pargv = argv;
    return argc;
}

static char **files;
static int nb_files, nb_libraries;
static int multiple_files;
static int print_search_dirs;
static int reloc_output;
static char *outfile;

int parse_args(TCCState *s, int argc, char **argv)
{
    int optind;
    TCCOption *popt;
    char *optarg, *p1, *r1;
    char *r;

    optind = 0;
    while (1) {
        if (optind >= argc) {
            if (nb_files == 0 && !print_search_dirs) {
                if (!s->verbose) help(s);
                exit(1);
            } else break;
        }
        r = argv[optind++];
        if (r[0] != '-') {
            /* add a new file */
            dynarray_add((void ***)&files, &nb_files, r);
            if (!multiple_files) {
                optind--;
                /* argv[0] will be this file */
                break;
            }
        } else {
            /* find option in table (match only the first chars */
            popt = tcc_options;
            for(;;) {
                p1 = popt->name;
                if (p1 == NULL)
                    error("invalid option -- '%s'", r);
                r1 = r + 1;
                for(;;) {
                    if (*p1 == '\0')
                        goto option_found;
                    if (*r1 != *p1)
                        break;
                    p1++;
                    r1++;
                }
                popt++;
            }
        option_found:
            if (popt->flags & TCC_OPTION_HAS_ARG) {
                if (*r1 != '\0' || (popt->flags & TCC_OPTION_NOSEP)) {
                    optarg = r1;
                } else {
                    if (optind >= argc)
                        error("argument to '%s' is missing", r);
                    optarg = argv[optind++];
                }
            } else {
                if (*r1 != '\0') {
                    help(s);
                    exit(1);
                }
                optarg = NULL;
            }
                
            switch(popt->index) {
            case TCC_OPTION_HELP:
                help(s);
                exit(1);
            case TCC_OPTION_I:
                add_dynarray_path(s, optarg, &(s->include_paths));
                break;
            case TCC_OPTION_D:
                {
                    char *sym, *value;
                    sym = (char *)optarg;
                    value = strchr(sym, '=');
                    if (value) {
                        *value = '\0';
                        value++;
                    }
                    tcc_define_symbol(s, sym, value);
                }
                break;
            case TCC_OPTION_E:
                s->output_type = TCC_OUTPUT_PREPROCESS;
                break;
            case TCC_OPTION_U:
                tcc_undefine_symbol(s, optarg);
                break;
            case TCC_OPTION_L:
                add_dynarray_path(s, optarg, &(s->library_paths));
                break;
            case TCC_OPTION_B:
                /* set tcc utilities path (mainly for tcc development) */
                tinycc_path = optarg;
                break;
            case TCC_OPTION_l:
                dynarray_add((void ***)&files, &nb_files, r);
                nb_libraries++;
                break;
            case TCC_OPTION_bench:
                do_bench = 1;
                break;
#ifdef CONFIG_TCC_BCHECK
            case TCC_OPTION_b:
                do_bounds_check = 1;
                do_debug = 1;
                break;
#endif
            case TCC_OPTION_g:
                do_debug = 1;
                break;
            case TCC_OPTION_c:
                multiple_files = 1;
                s->output_type = TCC_OUTPUT_OBJ;
                break;
            case TCC_OPTION_static:
                s->static_link = 1;
                break;
            case TCC_OPTION_shared:
                s->output_type = TCC_OUTPUT_DLL;
                break;
            case TCC_OPTION_o:
                multiple_files = 1;
                outfile = optarg;
                break;
            case TCC_OPTION_r:
                /* generate a .o merging several output files */
                reloc_output = 1;
                s->output_type = TCC_OUTPUT_OBJ;
                break;
            case TCC_OPTION_nostdinc:
                s->nostdinc = 1;
                break;
            case TCC_OPTION_nostdlib:
                s->nostdlib = 1;
                break;
            case TCC_OPTION_print_search_dirs:
                print_search_dirs = 1;
                break;
            case TCC_OPTION_run:
                {
                    int argc1;
                    char **argv1;
                    argc1 = expand_args(&argv1, optarg);
                    if (argc1 > 0) {
                        parse_args(s, argc1, argv1);
                    }
                    multiple_files = 0;
                    s->output_type = TCC_OUTPUT_MEMORY;
                }
                break;
            case TCC_OPTION_v:
                if (!s->verbose++) show_version();
                break;
            case TCC_OPTION_f:
                if (tcc_set_flag(s, optarg, 1) < 0 && s->warn_unsupported)
                    goto unsupported_option;
                break;
            case TCC_OPTION_W:
                if (tcc_set_warning(s, optarg, 1) < 0 && 
                    s->warn_unsupported)
                    goto unsupported_option;
                break;
            case TCC_OPTION_w:
                s->warn_none = 1;
                break;
            case TCC_OPTION_rdynamic:
                s->rdynamic = 1;
                break;
            case TCC_OPTION_Wl:
                {
                    char *p;
                    if (strstart(optarg, "-Ttext,", &p)) {
                        s->text_addr = strtoul(p, NULL, 16);
                        s->has_text_addr = 1;
                    } else if (strstart(optarg, "--oformat,", &p)) {
                        if (strstart(p, "elf32-", NULL)) {
                            s->output_format = TCC_OUTPUT_FORMAT_ELF;
                        } else if (!strcmp(p, "binary")) {
                            s->output_format = TCC_OUTPUT_FORMAT_BINARY;
                        } else
#ifdef TCC_TARGET_COFF
                        if (!strcmp(p, "coff")) {
                            s->output_format = TCC_OUTPUT_FORMAT_COFF;
                        } else
#endif
                        {
                            error("target %s not found", p);
                        }
                    } else {
                        error("unsupported linker option '%s'", optarg);
                    }
                }
                break;
            default:
                if (s->warn_unsupported) {
                unsupported_option:
                    warning("unsupported option '%s'", r);
                }
                break;
            }
        }
    }
    return optind;
}

int main(int argc, char **argv)
{
    int i;
    TCCState *s;
    int nb_objfiles, ret, optind;
    char objfilename[1024];
    int64_t start_time = 0;

    s = tcc_new();
    s->output_type = TCC_OUTPUT_EXE;
    outfile = NULL;
    multiple_files = 1;
    files = NULL;
    nb_files = 0;
    nb_libraries = 0;
    reloc_output = 0;
    print_search_dirs = 0;

#ifdef WIN32
    /* on win32, we suppose the lib and includes are at the location
       of 'tcc.exe' */
    {
        static char path[1024];
        char *p, *d;
        
        GetModuleFileNameA(NULL, path, sizeof path);
        p = d = strlwr(path);
        while (*d)
        {
            if (*d == '\\') *d = '/', p = d;
            ++d;
        }
        *p = '\0';
        tinycc_path = path;
    }
#else
    tinycc_path = TINYCC_INSTALLDIR;
#endif

    optind = parse_args(s, argc - 1, argv + 1) + 1;

    /* Just enough for the Linux kernel, which is hardwired to use a directory
       named "include" beneath this output value for the compiler headers.*/
    if (print_search_dirs) {
        printf("install: %s/\n", tinycc_path);
        return 0;
    }

    nb_objfiles = nb_files - nb_libraries;

    // if outfile provided without other options, we output an executable
    if (outfile && s->output_type == TCC_OUTPUT_MEMORY)
        s->output_type = TCC_OUTPUT_EXE;

    // check -c consistency : only single file handled. XXX: checks file type
    if (s->output_type == TCC_OUTPUT_OBJ && !reloc_output) {
        /* accepts only a single input file */
        if (nb_objfiles != 1)
            error("cannot specify multiple files with -c");
        if (nb_libraries != 0)
            error("cannot specify libraries with -c");
    }

    if (s->output_type == TCC_OUTPUT_PREPROCESS) {
        if (!outfile) s->outfile = stdout;
        else {
            s->outfile = fopen(outfile, "wb");
            if (!s->outfile) error("could not open '%s'", outfile);
        }
    } else if (s->output_type != TCC_OUTPUT_MEMORY) {
        if (!outfile) {
    /* compute default outfile name */
            pstrcpy(objfilename, sizeof(objfilename) - 1, 
                    /* strip path */
                    tcc_basename(files[0]));
#ifdef TCC_TARGET_PE
            pe_guess_outfile(objfilename, s->output_type);
#else
            if (s->output_type == TCC_OUTPUT_OBJ && !reloc_output) {
                char *ext = strrchr(objfilename, '.');
            if (!ext)
                goto default_outfile;
                /* add .o extension */
            strcpy(ext + 1, "o");
        } else {
        default_outfile:
            pstrcpy(objfilename, sizeof(objfilename), "a.out");
        }
#endif
        outfile = objfilename;
        }
    }

    if (do_bench) {
        start_time = getclock_us();
    }

    init_output_type(s);

    /* compile or add each files or library */
    for(i = 0;i < nb_files; i++) {
        char *filename;

        next_tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF | TOK_FLAG_BOW;

        filename = files[i];
        if (s->output_type == TCC_OUTPUT_PREPROCESS) {
            tcc_add_file_internal(s, filename,
                                  AFF_PRINT_ERROR | AFF_PREPROCESS);
        } else if (filename[0] == '-') {
            if (tcc_add_library(s, filename + 2) < 0)
                error("cannot find %s", filename);
        } else if (tcc_add_file(s, filename) < 0) {
            ret = 1;
            goto the_end;
        }
    }

    /* free all files */
    free(files);

    if (do_bench) {
        double total_time;
        total_time = (double)(getclock_us() - start_time) / 1000000.0;
        if (total_time < 0.001)
            total_time = 0.001;
        if (total_bytes < 1)
            total_bytes = 1;
        printf("%d idents, %d lines, %d bytes, %0.3f s, %d lines/s, %0.1f MB/s\n", 
               tok_ident - TOK_IDENT, total_lines, total_bytes,
               total_time, (int)(total_lines / total_time), 
               total_bytes / total_time / 1000000.0); 
    }

    if (s->output_type == TCC_OUTPUT_PREPROCESS) {
        if (outfile) fclose(s->outfile);
        ret = 0;
    } else if (s->output_type == TCC_OUTPUT_MEMORY) {
        ret = tcc_run(s, argc - optind, argv + optind);
    } else
#ifdef TCC_TARGET_PE
    if (s->output_type != TCC_OUTPUT_OBJ) {
        ret = tcc_output_pe(s, outfile);
    } else
#endif
    {
        ret = tcc_output_file(s, outfile) ? 1 : 0;
    }
the_end:
    /* XXX: cannot do it with bound checking because of the malloc hooks */
    if (!do_bounds_check)
        tcc_delete(s);

    return ret;
}

#endif
