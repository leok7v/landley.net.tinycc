#include "libtinycc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

// Data type for dynamic resizeable arrays
struct dynarray {
    char **data;
    int len;
};

// All these tccg_ things can be grouped into a structure, but not until after
// they're broken out of TCCState and moved over.

// Warning switches

int tccg_warn_unsupported;
int tccg_warn_write_strings;
int tccg_warn_error;
int tccg_warn_implicit_function_declaration;
int tccg_warn_none;

// C language options

int tccg_char_is_unsigned;
int tccg_leading_underscore;

// Don't merge identical symbols in .bss segment, error instead.
int tccg_nocommon;

// if true, describe each room as you enter it, unless it contains a grue
int tccg_verbose;

// Include file handling
struct dynarray tccg_include_paths;
struct dynarray tccg_library_paths;

int tccg_output_type;
int tccg_output_format;// TCC_OUTPUT_FORMAT_xxx
int tccg_static_link;  // Perform static linking?
int tccg_nostdinc;     // If true, no standard headers are added.
int tccg_nostdlib;     // If true, no standard libraries are added.
int tccg_rdynamic;     // Export all symbols.

unsigned long tccg_text_addr;  // Address of text section.
int tccg_has_text_addr;

FILE *tccg_outfile;    // Output file for preprocessing.

// Functions from elsewhere.

void error(char *fmt, ...);
void *xmalloc(unsigned long size);
void dynarray_add(void ***ptab, int *nb_ptr, void *data);
void add_dynarray_path(TCCState *s, char *pathname, struct dynarray *dd);
int strstart(char *str, char *val, char **ptr);
void warning(char *fmt, ...);
int init_output_type(TCCState *s);
char *pstrcpy(char *buf, int buf_size, char *s);
int tcc_add_file_internal(TCCState *s, char *filename, int flags);

extern char *tinycc_path;

#ifndef offsetof
#define offsetof(type, field) ((size_t) &((type *)0)->field)
#endif

#ifndef countof
#define countof(tab) (sizeof(tab) / sizeof((tab)[0]))
#endif

// This token begins a word/line/file
#define TOK_FLAG_BOW   0x0001
#define TOK_FLAG_BOL   0x0002
#define TOK_FLAG_BOF   0x0004

// Add file flags (passed to tcc_add_file_internal())
#define AFF_PRINT_ERROR     0x0001 // print error if file not found
#define AFF_PREPROCESS      0x0004 // preprocess file

// First identifier token
#define TOK_IDENT      256  // First identifier/string token.

// This should come from dlfcn.h but doesn't unless you claim to be written
// by the fsf, which we aren't.
#define RTLD_DEFAULT   0

int is_space(int ch);
