# regex

POSIX regular expressions for [sysl](https://github.com/sysl-lang/sysl), bound to the C library the
machine already has.

**Nothing is vendored and nothing has to be installed.** `regcomp`, `regexec`, `regerror` and
`regfree` have been in POSIX since 1003.2 and are in every hosted C library, so this package carries
no upstream source and its header carries no `@link` — the driver already passes what holds them.

**It is the organisation's worked example of binding libc.** The other bindings —
[linenoise](https://github.com/sysl-lang/linenoise),
[monocypher](https://github.com/sysl-lang/monocypher),
[qcbor](https://github.com/sysl-lang/qcbor) — carry the library's own C, which is the other half of
the story. This one shows what a binding still owes when the implementation is already there.

```
sh/sysl/regex/
    regex.sysl          the binding
    tests.sysl          10 tests, each written so only real matching passes it
    c/
        c.sysl          the shim as C declares it, and the compile flags <regex.h> defines
        shim.c          the C that only C can reach
package.hocon           who this package is, and what it needs of the machine
```

**Everything that is C lives in `c/`**, module `sh.sysl.regex.c`. That is the two-layer shape every
binding in this organisation uses: the `c` layer has to be *faithful*, because a signature that
disagrees with the C links perfectly and corrupts the call at run time, and the layer above it has to
be *pleasant*, which is a different question that would otherwise be answered in the same breath.

The module is **`sh.sysl.regex`**, and the directories are that name: a dotted module name
mirrors its path from the library root. The prefix is the reverse-DNS of `sysl.sh`, so that a package
claims a name nobody else will mint rather than the top-level word `regex`.

## Using it

Name it in your project's `package.hocon` and `sysl build` fetches it:

```hocon
dependencies {
  regex { git = "github.com/sysl-lang/regex", version = "0.2.0" }
}
```

The coordinate is an identity rather than a URL, so it carries no `https://`, and `version` is the
tag `v0.2.0` here.

Or point at it directly, which needs no fetching and is what this repository's own tests do. Either a
built artifact or the source tree works, and they are the same road:

```
sysl build-lib . -o /tmp/regex.syslib
sysl run yourprogram.sysl --lib /tmp/regex.syslib

sysl run yourprogram.sysl --lib /path/to/this/repo
```

## The surface

```sysl
compile(pattern: string) -> Result[&Regex, string]

struct Match
    start: usize        // byte offsets into the subject
    end: usize

struct Regex
    matches(&self, s: string) -> bool
    find(&self, s: string) -> Option[Match]

struct Options
    icase: bool         // match without regard to case
    newline: bool       // `^` and `$` mean the ends of a line, not of the subject

plain() -> Options
compile(pattern) -> Result[&Regex, string]
compile_with(pattern, Options) -> Result[&Regex, string]
```

Patterns are **extended** regular expressions always — `+`, `?`, `|`, `{m,n}` and `()` are operators
rather than the characters they are written as. Basic syntax is not offered: it is a 1970s
compatibility mode in which `a+b` matches three characters, and a binding does its reader no favour by
making that reachable.

`compile` answers `Err` with the C library's own sentence, which is BSD's or glibc's depending on the
machine and is not the same text on both.

**There is nothing to free.** `Regex` is a `&Regex` with an `impl Drop`, so `regfree` and the `free` of
the storage it sat in happen when the last reference goes. A `free` method left this API when the
package took the two-layer shape — the handle is still storage the shim allocated, and a destructor is
how a binding says who releases it without asking anybody to remember.

## An example

A package has no way to carry a program of its own — everything under the package root is compiled
*into* the library — so the demo lives here (`design/packages.md § Open h`):

```sysl
import sh.sysl.regex.*

show(re: &Regex, s: string) =
    re.find(s) match
        Some(m) -> print(f"${s}%-8s ${m.start}..${m.end}")
        None    -> print(f"${s}%-8s no match")

compile("^a+b+$") match
    Ok(re) ->
        show(re, "aab")
        show(re, "abbbb")
        show(re, "ba")

    Err(why) -> print(why)

// A pattern that is not one, which is the path a binding has to have an answer for. The message
// comes from the C library itself.
compile("a[") match
    Ok(_) ->
        print("a[ compiled, and should not have")

    Err(why) -> print(f"a[ is not a pattern: ${why}")
```

```
aab      0..3
abbbb    0..5
ba       no match
a[ is not a pattern: brackets ([ ]) not balanced
```

## How to bind a libc interface

This is the part the package exists to demonstrate. "The machine already has it" answers where the
implementation comes from, and answers nothing about whether sysl can reach it.

**1. Write a shim for what only C can see.** Two things in `<regex.h>` are reachable from C and from
nothing else, and `c/shim.c` exists for exactly those:

| what | why it cannot cross |
|---|---|
| `sizeof(regex_t)` | The caller allocates it, and it is 32 bytes on Darwin and 64 under glibc. A program can only allocate storage whose size it knows — so the shim allocates it and hands back a pointer, and the sysl side never learns the size. |
| `regmatch_t` | A layout only the header knows. The shim copies the two offsets out into plain integers, so the struct never crosses. |

The alternative is transcribing those numbers into sysl, which compiles everywhere and is correct on
whichever machine the author happened to use. Nothing checks it — sysl's own `sizeof` reports what
sysl laid out, not what C did — and getting it wrong writes past the end of the caller's storage.

**`REG_EXTENDED` was the third row of that table, and it is worth saying what moved it.** A `#define`
has no symbol, so there is nothing for a linker to resolve and nothing for `extern` to name — but a
macro's *number* is something the C compiler can be asked for, which is what `c const` does:

```sysl
@include("regex.h")

c const
    EXTENDED: int = "REG_EXTENDED"
    ICASE:    int = "REG_ICASE"
    NEWLINE:  int = "REG_NEWLINE"
```

**And that is not a tidy-up — it is why `Options` exists.** While the flags could only be reached from
C, `shim.c` had to pick one value for everybody, and the whole `cflags` argument of `regcomp` was
unreachable: case-insensitive matching, which is one bit, could not be asked for. Naming the constants
in sysl put the choice back where it belongs. **A binding that hard-codes something because the
language could not reach it is worth re-reading whenever the language moves.**

**2. Drop the `.c` in the module's directory and stop.** Nothing declares it and nothing lists it:
the build walks the tree, finds `shim.c` beside `regex.sysl`, and compiles it for the same target
(`15 §7`). In an artifact it becomes a member named after its path — `sh.sysl.regex.shim.o` — which
is what keeps it distinct from another package's file of the same name in the same link.

**3. Do not name a library you do not need.** There is no `@link` here. The implementation is inside
the C library, which the driver passes on every hosted target unasked, so naming it would be naming
what is already on the line (`15 §8`). Compare qcbor, which names `m` because its numeric coercions
reach `pow`.

**4. Say what you need of the machine.** `package.hocon` carries `requires { heap = true,
posix = true }` — `posix` because `<regex.h>` is POSIX and not ISO C, `heap` because the shim calls
`calloc`. That turns "this cannot work on a freestanding target" into one refusal before anything is
compiled, rather than a link error naming `regcomp`.

**5. Test it so that only the real library passes.** A shim that dropped an argument, or that returned
the wrong half of a pair, still compiles, still links, and still answers something. `tests.sysl` picks
offsets that are mid-string, repeats that stop short of the available input, and patterns whose meaning
changes under basic syntax — that last is what says `REG_EXTENDED` arrived, since a macro is invisible
to the linker whichever side names it.

**A flag is checked the same way, by matching rather than by reading.** `Options.code` or-ing the wrong
constant compiles and answers a pattern that is merely case-sensitive, so the icase case matches a
subject differing from the pattern only in case, and the newline case matches a line in the middle of a
subject the anchored pattern refuses.

```
sysl test .
```

## Licence

ISC. See [LICENSE](LICENSE).
