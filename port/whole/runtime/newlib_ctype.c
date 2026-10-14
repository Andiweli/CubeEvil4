/* The newlib character class table that include/newlib_local.h reads directly
 * (isupper(c) = (_ctype_ + 1)[c] & _U); used by the t_movie debug tool. Flags
 * as in newlib: _U 01, _L 02, _N 04, _S 010, _P 020, _C 040, _X 0100, _B 0200.
 * ASCII only, like newlib's "C" locale. */
#define U 01
#define L 02
#define N 04
#define S 010
#define P 020
#define C 040
#define X 0100
#define B 0200

/* The game's units see this as _ctype_ (re4_whole_prelude.h renames it:
 * Android's C library exports a pointer named _ctype_). */
const char re4_newlib_ctype_[1 + 256] = {
    0,
    C, C, C, C, C, C, C, C, C, C | S, C | S, C | S, C | S, C | S, C, C,
    C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
    S | B, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
    N, N, N, N, N, N, N, N, N, N, P, P, P, P, P, P,
    P, U | X, U | X, U | X, U | X, U | X, U | X, U, U, U, U, U, U, U, U, U,
    U, U, U, U, U, U, U, U, U, U, U, P, P, P, P, P,
    P, L | X, L | X, L | X, L | X, L | X, L | X, L, L, L, L, L, L, L, L, L,
    L, L, L, L, L, L, L, L, L, L, L, P, P, P, P, C,
};
