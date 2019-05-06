- All the configuration is done by the `configure` script.  This script has not
  been generated by `autoconf` tools but yields similar kind of installation.
  Building can be done in another directory than the top level directory of the
  source distribution.  Meta-information is saved in `pkg-config` configuration
  files.  By using `make DESTDIR=root_directory install`, installation can be
  relative to a specific root directory (useful for constructing distributed
  packages).
- Build shared and static libraries.
- To make Play and Gist libraries compatible with other software and avoid name
  conflicts, exported symbols (functions and variables) and macros have been
  renamed to use more specific prefixes (`pl_` and `PL_` for the public symbols
  of the Play library, `_pl_` for the private symbols of this library).