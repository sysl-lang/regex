/* The C that a POSIX regex binding cannot be written without.
 *
 * What is left here is about *layouts*, which is the one thing that genuinely
 * cannot cross:
 *
 *   - `sizeof(regex_t)`. The caller allocates it, and it is 32 bytes on Darwin
 *     and 64 under glibc. A sysl program cannot spell storage whose size it has
 *     no way to learn -- so this file allocates it and hands back a pointer, and
 *     the sysl side never learns the size at all.
 *   - `regmatch_t`. Another layout only the header knows. The bounds are copied
 *     out into two plain integers here, so it never has to cross.
 *
 * **`REG_EXTENDED` used to be on that list and no longer is.** It is a `#define`
 * with no symbol, so there was nothing for a linker to resolve -- but a macro's
 * *number* is something `c const` asks the C compiler for, and `c.sysl` does. So
 * the flags are no longer this file's decision: `sysl_regex_compile` takes
 * whatever the caller or'd together, and the sysl side is what names
 * `REG_EXTENDED` as the default.
 *
 * Nothing in this file is sysl-specific. It is the ordinary shim any language
 * writes when it binds a C library, and the build compiles it because it is
 * sitting in the library's tree -- nothing lists it and nothing declares it.
 */
#include <regex.h>
#include <stdlib.h>

void *sysl_regex_new(void) { return calloc(1, sizeof(regex_t)); }

void sysl_regex_free(void *re) {
    if (re) {
        regfree((regex_t *)re);
        free(re);
    }
}

/* Zero on success, and otherwise a code `sysl_regex_error` turns into a message.
 *
 * `cflags` arrives already or'd together from the constants `c.sysl` asks the
 * header for, so this function makes no choice about syntax at all.
 */
int sysl_regex_compile(void *re, const char *pattern, int cflags) {
    return regcomp((regex_t *)re, pattern, cflags);
}

/* Whether the pattern matches, with the bounds of the whole match written back
 * through `so` and `eo` when it does. Splitting the answer from the bounds is
 * what keeps `regmatch_t` on this side of the boundary.
 */
int sysl_regex_exec(void *re, const char *s, long *so, long *eo) {
    regmatch_t m;

    if (regexec((regex_t *)re, s, 1, &m, 0) != 0) return 0;

    *so = (long)m.rm_so;
    *eo = (long)m.rm_eo;

    return 1;
}

/* The message the C library itself supplies for a compile that failed, written
 * into storage the caller owns -- the same shape as `strerror_r`. The sysl side
 * turns it into a `string` with `from_cstring`.
 */
void sysl_regex_error(void *re, int code, char *buf, unsigned long n) {
    regerror(code, (regex_t *)re, buf, (size_t)n);
}
