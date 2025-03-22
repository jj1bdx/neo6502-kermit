# E-Kermit -- Kermit for Embedding

See <https://kermitproject.org/ek.html> for the details.

## Tags

* Currently adding all stuff on `main`
* `v1.8`: original file tag

## Testing status

* Build tested on macOS 15.3.1 Apple Silicon
* Build tested on Ubuntu 24.04.1 amd64

## On compilation warnings

(Originally written in AAREADME.TXT by Frank da Cruz)

### E-KERMIT COMPILATION WARNINGS

EK 1.7 compiled without warnings on a great many platforms in 2011.
Now in 2021 it gets all kinds of political-correctness warnings,
different ones on different platforms.  This is because every release
of every variety of Unix tends to change or move header files and/or
the function definitions within them out from under the applications
that depend on them, and also because the C compilers become
increasingly picky.  Earlier programming environments were not like
this; for example, if you wrote a program in PL/I or SNOBOL, it
*stayed* written.

I'm not going to worry about this; it's a rabbit hole and no amount
of fixing will prevent it happening again in another year or two.
E-Kermit 1.8, released today, corrects a bug when using the Unix demo
version and the -B or -T option is included on the command line.  It
gets the same warnings as 1.7

The warnings are harmless; the program works on at least on Red Hat
Enterprise Linux 2.6, on NetBSD 9.0, and Ubuntu 20.04.1.  If you have any
ideas how the improve the E-Kermit code to cope with this situation without
burying it in mountains of #ifdefs, please let me know.

Frank da Cruz<br>
The Kermit Project<br>
`fdc@columbia.edu`<br>
26 May 2021

## License

Revised 3-Clause BSD License. See also [Revised 3-Clause BSD License for Columbia University Kermit Software](https://kermitproject.org/cu-bsd-license.html), which is consistent with the COPYING file of this repository.

