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
    shim.c              the C that only C can reach
    tests.sysl          7 tests, each written so only real matching passes it
package.hocon           who this package is, and what it needs of the machine
```

The module is **`sh.sysl.regex`**, and the three directories are that name: a dotted module name
mirrors its path from the library root. The prefix is the reverse-DNS of `sysl.sh`, so that a package
claims a name nobody else will mint rather than the top-level word `regex`.

## Using it

Name it in your project's `package.hocon` and `sysl build` fetches it:

```hocon
dependencies {
  regex { git = "github.com/sysl-lang/regex", version = "0.1.0" }
}
```

The coordinate is an identity rather than a URL, so it carries no `https://`, and `version` is the
tag `v0.1.0` here.

Or point at it directly, which needs no fetching and is what this repository's own tests do. Either a
built artifact or the source tree works, and they are the same road:

```
sysl build-lib . -o /tmp/regex.syslib
sysl run yourprogram.sysl --lib /tmp/regex.syslib

sysl run yourprogram.sysl --lib /path/to/this/repo
```

## The surface

```sysl
compile(pattern: string) -> Result[Regex, string]

struct Match
    start: usize        // byte offsets into the subject
    end: usize

struct Regex
    matches(self, s: string) -> bool
    find(self, s: string) -> Option[Match]
    free(self)
```

Patterns are **extended** regular expressions — `REG_EXTENDED` — so `+`, `?`, `|`, `{m,n}` and `()`
are operators rather than the characters they are written as.

`compile` answers `Err` with the C library's own sentence, which is BSD's or glibc's depending on the
machine and is not the same text on both.

**`free` is not optional.** The handle is storage the shim allocated, and a binding to a library that
allocates has to say who releases it.

## An example

A package has no way to carry a program of its own — everything under the package root is compiled
*into* the library — so the demo lives here (`design/packages.md § Open h`):

```sysl
import sh.sysl.regex.*

show(re: Regex, s: string) =
    re.find(s) match
        Some(m) -> print(f"${s}%-8s ${m.start}..${m.end}")
        None    -> print(f"${s}%-8s no match")

compile("^a+b+$") match
    Ok(re) ->
        show(re, "aab")
        show(re, "abbbb")
        show(re, "ba")
        re.free()

    Err(why) -> print(why)

// A pattern that is not one, which is the path a binding has to have an answer for. The message
// comes from the C library itself.
compile("a[") match
    Ok(re) ->
        print("a[ compiled, and should not have")
        re.free()

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

**1. Write a shim for what only C can see.** Three things in `<regex.h>` are reachable from C and
from nothing else, and `shim.c` exists for exactly those:

| what | why it cannot cross |
|---|---|
| `sizeof(regex_t)` | The caller allocates it, and it is 32 bytes on Darwin and 64 under glibc. A program can only allocate storage whose size it knows — so the shim allocates it and hands back a pointer, and the sysl side never learns the size. |
| `REG_EXTENDED` | A `#define` has no symbol. There is nothing for a linker to resolve and nothing for `extern` to name. |
| `regmatch_t` | A layout only the header knows. The shim copies the two offsets out into plain integers, so the struct never crosses. |

The alternative is transcribing those numbers into sysl, which compiles everywhere and is correct on
whichever machine the author happened to use. Nothing checks it — sysl's own `sizeof` reports what
sysl laid out, not what C did — and getting it wrong writes past the end of the caller's storage.

**2. Drop the `.c` in the module's directory and stop.** Nothing declares it and nothing lists it:
the build walks the tree, finds `shim.c` beside `regex.sysl`, and compiles it for the same target
(`15 §7`). In an artifact it becomes a member named after its path — `sh.sysl.regex.shim.o` — which
is what keeps it distinct from another package's file of the same name in the same link.

**3. Do not name a library you do not need.** There is no `@link` here. The implementation is inside
the C library, which the driver passes on every hosted target unasked, so naming it would be naming
what is already on the line (`15 §8`). Compare qcbor, which names `m` because its numeric coercions
reach `pow`.

**4. Say what you need of the machine.** `package.hocon` carries `requires { alloc = true,
posix = true }` — `posix` because `<regex.h>` is POSIX and not ISO C, `alloc` because the shim calls
`calloc`. That turns "this cannot work on a freestanding target" into one refusal before anything is
compiled, rather than a link error naming `regcomp`.

**5. Test it so that only the real library passes.** A shim that dropped an argument, transcribed a
constant, or returned the wrong half of a pair still compiles, still links, and still answers
something. `tests.sysl` picks offsets that are mid-string, repeats that stop short of the available
input, and patterns whose meaning changes under basic syntax — that last is the only check there is
that `REG_EXTENDED` arrived, since a macro is invisible to the linker.

```
sysl test .
```

## Licence

ISC. See [LICENSE](LICENSE).
