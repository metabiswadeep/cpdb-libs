# CHANGES - Common Print Dialog Backends - Libraries - v2.0b6 - 2024-06-18

## CHANGES IN V2.0b6 (18th June 2024)

 - Stream print data through a Unix domain socket
   To ease making a Snap from the CPDB backend for CUPS (and other
   CPDB backends in the future) we now transfer the print job data
   from the dialog to the backend via a Unix domain socket and not by
   dropping the data into a file (PR #30).

 - Add newly appearing backends while the dialog is open
   CPDB Backends can get installed or removed at any time, also while
   a print dialog is open. Now a background thread is added to observe
   the come and go of backends and to update the printer list
   appropriately. New API functions are
   cpdbStartBackendListRefreshing() and cpdbStopBackendListRefreshing,
   to start and stop this thread (PR #32).

 - Added support for CPDB backends running permanently
   Do not only find backends as registered D-Bus services (*.service) files
   but also permanently running backends which are not necessarily registered
   D-Bus services

 - Let the frontend not be a D-Bus service, only the backends
   To control hiding temporary or remote printers in the backend's
   printer list we have added methods to the D-Bus service provided by
   the backends now. Before, the frontends were also D-Bus services
   just to send signals to the backends for controling the filtering
   (PR #32).

 - Convenience API functions to start/stop listing printers
   Added convenience API functions cpdbStartListingPrinters() and
   cpdbStopListingPrinters() to be called when opening and closing the
   print dialog, resp. (PR #32).

 - Removed API functions cpdbGetAllJobs(), cpdbGetActiveJobsCount(),
   cpdbCancelJob(), and cpdbPrintFilePath() and the corresponding
   D-Bus methods (PR #30).

 - Removed support for the "FILE" CPDB backend (PR #30).

 - cpdbActivateBackends(): Fixed crash caused by wrong unreferencing

 - cpdbConnectToDBus(): Use g_main_context_get_thread_default() for wait loop
   g_main_context_get_thread_default() always returns a valid context and
   never NULL. This way we avoid crashes.

 - Fixed crash when Handling saved settings
   Let cpdbReadSettingsFromDisk() return an empty data structure
   instead of NULL when there are no saved settings. Also let
   cpdbIgnoreLastSavedSettings() always output and empty data
   structure.

 - Removed the commands, "get-all-jobs", "get-active-jobs-count", and
   "cancel-job" from the "cpdb-text-frontend" utility (PR #30).

 - cpdb-text-frontend: Removed unnecessary g_main_loop
   The g_main_loop is not actually needed, the main thread can just wait
   for the background thread using g_thread_join().

 - cpdb-text-frontend: Shut down cleanly with "stop" command
   Instead of committing suicide with "exit(0);" we actually quit the
   main loop now.

 - cpdb-text-frontend: Spawn command interpreter not before the start
   of the main loop, to assure that "stop" command quits main loop
   and we do not fall into an infinite loop.

 - cpdb-text-frontend: acquire_translations_callback(): Only issue the
   success message and list the translation if the loading of the
   translations actually succeeded.


## CHANGES IN V2.0b5 (2nd August 2023)

 - Removed browsing for backends via file system
   The frontend should only shout into the D-Bus to find out which
   backends are available and to communicate with them. Depending on
   the way (for example sandboxed packaging, like Snap) how the
   frontend and backand are installed the frontend cannot access the
   host's or the backend's file systems (PR #27).

 - Limit scanned string length in `scanf()`/`fscanf()` functions
   cpdb-libs uses the `fscanf()` and `scanf()` functions to parse
   command lines and configuration files, dropping the read string
   components into fixed-length buffers, but does not limit the length
   of the strings to be read by `fscanf()` and `scanf()` causing
   buffer overflows when a string is longer than 1023 characters
   (CVE-2023-34095).

 - Fixed memory bugs leading to leaks and crashes (PR #26)

 - Build system: Removed unnecessary lines in `tools/Makefile.am`
   Removed the `TESTdir` and `TEST_SCRIPTS` entries in
   `tools/Makefile.am`.  They are not needed and let `make install`
   try to install `run-tests.sh` in the source directory, where it
   already is, causing an error.


## CHANGES IN V2.0b4 (20th March 2023)

 - Added test script for `make test`/`make check`
   The script tools/run-tests.sh runs the `cpdb-text-frontend` text mode
   example frontend and stops it by supplying "stop" to its standard
   input.

 - Allow changing the backend info directory via env variable
   To make it possible to test backends which are not installed into
   the system, one can now set the environment variable
   CPDB_BACKEND_INFO_DIR to the directory where the backend info file
   for the backend is, for example in its source tree.

 - Install sample frontend with `make install`
   We use the sample frontend `cpdb-text-frontend` for several tests now,
   especially "make check" and also the autopkgtests in the
   Debian/Ubuntu packages. They are also useful for backend developers
   for manual testing.

 - Renamed develping/debug tools
   As we install the development and debugging tools now, they should
   be more easily identifiable as part of CPDB. Therefore they get
   `cpdb-`-prefixed names.

 - `cpdb-text-frontend`: Use larger and more easily adjustable string
   buffers

 - Fixed segfault in the frontend library
   `cpdbResurrectPrinterFromFile()`, when called with an invalid file
   name, caused a crash.


## CHANGES IN V2.0b3 (20th February 2023)

- Added functions to fetch all printer strings translations (PR #23)
  * Added `cpdbGetAllTranslations()` to synchronously fetch all
    printer string translations
  * Added `cpdbAcquireTranslations()` to asychronously fetch them.
  * Removed `get-human-readable-option`/`choice-name` methods
  * Removed `cpdb_async_obj_t` from `cpdb-frontend.h` as that is meant
    for internal use.


## CHANGES IN V2.0b2 (13th February 2023)

- Options groups: To allow better structuring of options in print
  dialogs, common options are categorized in groups, like media, print
  quality, color, finishing, ... This can be primarily done by the
  backends but the frontend library can do fallback/default
  assignments for options not assigned by the backend.

- Many print dialogs have a "Color" option group (usually shown on one
  tab), so also have one in cpdb-libs to match with the dialogs and
  more easily map the options into the dialogs.

- Add macros for new options and choices, also add "Color" group

- Synchronous printer fetching upon backend activation (PR #21) Made
  `cpdbConnectToDbus()` wait until all backends activate

- Backends will automatically signal any printer updates instead of
  the frontend having to manually ask them (PR #21)

- Add `printer-state-changed` signal (PR #21)
  * Changed function callback type definition for printer updates
  * Added callback to frontends for changes in printer state

- Translation support: Translations of option, choice, and group names
  are now supported, not only English human-readable strings. And
  Translations can be provided by the backends (also polling them from
  the print service) and by the frontend library.

- Use autopoint instead of gettextize

- Enable reconnecting to dbus (PR #14)

- Debug logging: Now backends forward their log messages to the
  frontend library for easier debugging.

- Use <glib/gi18n.h> instead of redefining macros (Issue #20)

- Add functions to free objects (PR #15)

- Remove hardcoded paths and follow XDG base dir specs (PR #14)

- Added javadoc comments to function declarations (PR #21)

- Build system: Let "make dist" also create .tar.bz2 and .tar.xz

- Add the dependency on libdbus to README.md
  libdbus-1-dev is needed to configure pkg-config variables for
  backends

- COPYING: Added Priydarshi Singh


## CHANGES IN V2.0b1 (11th December 2022)

- Added interfaces to get human readable option and settings names
    
  Print attributes/options and their choices are usually defined in a
  machine-readable form which is more made for easy typing in a
  command line, not too long, no special characters, always in English
  and human-readable form for GUI (print dialogs), more verbose for
  easier understanding, with spaces and other special characters,
  translated, ...

  Older backends without human-readable strings can still be used. In
  such a case it is recommended that the dialog does either its own
  conversion or simply shows the machine-readable string as a last
  mean.

- Added get_media_size() function to retrieve media dimensions for a
  given "media" option value

- Support for media sizes to have multiple margin variants (like
  standard and borderless)

- Support for configurable user and system-wide default printers

- Acquire printer details asynchronously (non blocking)

- Made cpdb-libs completely CUPS-neutral

  Removed CUPS-specific functions from the frontend library functions
  and the dependency on libcups, renamed CUPS-base function and signal
  names

- Use "const" qualifiers on suitable function parameters

- DBG_LOG now includes error-message

- Renamed all API functions, data types and constants
    
  To make sure that the resources of libcpdb and libcpdb-frontend do
  not conflict with the ones of any other library used by a frontend
  or backend created with CPDB, all functions, data types, and
  constants of CPDB got renamed to be unique to CPDB.
    
  Here we follow the rules of CUPS and cups-filters (to get unique
  rules for all libraries by OpenPrinting): API functions are in
  camelCase and with "cpdb" prefix, data types all-lowercase, with '_'
  as word separator, and constants are all-uppercase, also with '_' as
  word separator, and with "CPDB_" prefix.

- Renamed and re-organized source files to make all more
  standards-conforming and naming more consistent.
    
- All headers go to /usr/include/cpdb now: Base API headers cpdb.h and
  cpdb-frontend.h, interface headers (and also part of the API)
  backend-interface.h and frontend-interface.h, and the convenience
  header files backend.h and frontend.h (include exactly the headers
  needed).
    
- Bumped soname of the libraries to 2.
    
- Check settings pointer for NULL before freeing it

- NULL check on input parameters for many functions

- Fixed incompatibility with newer versions of glib()

  glib.h cannot get included inside 'extern "C" { ... }'

- Corrected AC_INIT() in configure.ac: Bug report destination,
  directory for "make dist".

- README.md: Fixed typos and updated usage instructions

- Updated .gitignore

