# CRAN pre-submission review

Reviewed 2026-11-07 on macOS arm64 with R 4.6.0.

## DESCRIPTION and dependencies

DESCRIPTION passes strict metadata validation. Rcpp is required by the R
wrappers; this is a compiled sparse-volume provider, not a header-only package.
The upstream version and packaging revision remain 13.1.0-2.

## Native-code corrections

- oneTBB messages now use the bounded, thread-safe diagnostic queue retrieved
  through `vdb_messages()`. The queue lives in the private oneTBB runtime, so
  runtime and provider diagnostics use the same storage. Workers never call R.
- oneTBB and OpenVDB invariant failures throw native exceptions and invalidate
  subsequent provider operations. The C API catches exceptions and returns an
  error status; the R wrapper raises an R error after native unwinding. Native
  failures are never converted into successful returns.
- Blosc's Windows synchronization initialization cleans up partial resources and
  returns an error, which the caller checks before starting workers. The old
  printf/exit helper is removed. AVX2-unavailable shuffle entry points use the
  existing generic implementation instead of aborting.
- Provider operations use a scoped two-thread limit by default. An explicit
  positive `OPENVDBR_NUM_THREADS` value overrides that limit.
- Remaining Intel warning-suppression pragmas in optional logging code are
  removed. Packing, intrinsic and ABI pragmas are retained.

On macOS, the rebuilt provider and private oneTBB libraries have no direct imports
of abort/exit, printf/fprintf/fputs/puts, or C++ cout/cerr. C++ runtime symbols such
as `__cxa_guard_abort` cancel static initialization; they are not process aborts.
Rprintf/REprintf are R console interfaces and snprintf/vsnprintf format buffers.

OpenVDB's UUID entropy remains for file identification, not statistical sampling;
it does not use or change R's RNG. Uninstantiated debug-print utilities and the
unbuilt TBB allocator still contain upstream console code. They are not exposed
through the installed C interface or linked into the package libraries.

Native invariant failures are programming errors, not a general recovery
contract for memory corruption or exceptions inside third-party noexcept code.
Restart R after a reported invariant failure. Ordinary input/I/O errors remain
recoverable without invalidating the provider.

## Validation

Final installation followed by `R CMD check --as-cran --no-manual
--install=check:<installation-log> --library=<test-library>`: 0 errors,
0 warnings, 1 NOTE (new submission). All 110 package test expectations pass,
including runtime settings and a subprocess that buffers TBB_VERSION output.

Standalone native tests verify concurrent queue overflow, propagation of a
oneTBB task assertion, rejection of subsequent operations, and byte-exact
round-trips through the generic AVX2 fallback. R's pragma checker finds no
issues. Windows-specific synchronization changes are not locally executed.

Detailed logs and native test executables are in
`/var/folders/19/j71hqsgx1jjg5kb0nxf58b200000gn/T/native-cran-fixes-hji90jbw`.
The development test sources are retained in `dev/`.

References: [Writing R Extensions](https://cran.r-project.org/doc/manuals/r-release/R-exts.html#Writing-portable-packages)
and [CRAN Repository Policy](https://cran.r-project.org/web/packages/policies.html).

## Windows build follow-up

The supplied win-builder log stops in oneTBB because Rtools headers omit
`LOAD_LIBRARY_SAFE_CURRENT_DIRS`. A guarded definition supplies Microsoft's
documented value, preserving the existing search policy. Blosc Windows visibility
and unused-code diagnostics are corrected; the monitor membership atomic now lives in the base list node, avoiding a
downcast for stores without suppressing GCC warnings.

The updated macOS installation and check pass with 0 errors, 0 warnings, and
one new-submission NOTE; all 110 R expectations pass. Native monitor tests
exercise 10,000 notification cycles under UBSan, including sentinel and selective
wakeup cases. Windows results require a new win-builder run. Logs and archive:
`/Users/tyler/Desktop/R/win-builder-submissions/2026-11-07-openvdbr-windows-fix`.

## Windows linker follow-up

The next Windows run completes compilation but reports duplicate pthread symbols
from Blosc's Windows shim and Rtools' winpthreads. The shim's seven externally
linked functions now use `blosc_internal_` names; a symbol/coexistence link test
passes. Windows stack queries use the documented GetCurrentThreadStackLimits
API, and resumable tasks use oneTBB's thread backend. These Windows execution
paths still require win-builder validation. The README's link to the excluded
`dev/consumer` directory is replaced by a repository-only code reference.

The updated local check again has 0 errors, 0 warnings and one new-submission
NOTE; all 110 R expectations and the native tests pass. Archive and logs:
`/Users/tyler/Desktop/R/win-builder-submissions/2026-11-07-openvdbr-pthread-fix`.

## Windows installation and subprocess follow-up

The supplied Windows run now installs and loads successfully. The test subprocess
returned status 5 before producing output. Its launcher used `system2(env=...)`,
whose Windows support is limited to programs accepting assignments on their
command lines. The test now temporarily sets and restores R_TESTS in the parent,
uses an explicit Rscript.exe path on Windows, and executes a temporary script
instead of passing multiline code through command-line quoting. Local tests also
verify restoration of a preexisting R_TESTS value. Windows confirmation is pending.

The abort/exit/_exit imports are present in the actual win-builder DLLs. There
are 23 direct call/jump sites in each DLL. The unstripped private TBB library
identifies their callers as GCC unwinding, C++ termination, emulated TLS, MinGW
pseudo-relocation/CRT, and winpthreads routines (including rwl_unref). All 23
provider sites match those runtime instruction sequences after relocation
normalization, with forward-block matching for tiny cold abort stubs. No direct
package-specific termination caller was found. This is a static provenance audit,
not a claim that runtime failure paths can never execute. Replacing these runtime
handlers with R errors would not preserve native unwinding and thread semantics.

Binary inspected: https://win-builder.r-project.org/0W18eDaj3qKM/openvdbr_13.1.0-2.zip
Detailed evidence is saved with the subprocess-fix submission artifacts.


## Standard native-library installation

The check at https://win-builder.r-project.org/Yqx68zX9h3uD/ reported missing
object information because src/install.libs.R bypassed R's normal installation.
That installer and its subsequent manual symbol-table workaround are removed.

OpenVDB and Blosc remain static archives linked into the R package library.
oneTBB remains a private shared runtime, built directly into src with R's shared
library extension. R installs both libraries and its own object-symbol metadata.
The backend is a prerequisite of the wrapper object, ensuring its archives exist
before R collects symbols, including under parallel make. No package code reads,
writes, or filters symbols.rds. Compiler runtime handlers are unchanged.

R's automatic tables cover the wrapper and the two linked static archives;
they do not include the separately linked oneTBB runtime's objects. The separate
native-source and DLL provenance audits remain relevant for that runtime.
Windows validation of this installation change is pending.

References:
- https://github.com/wch/r-source/blob/trunk/src/library/tools/R/install.R
- https://github.com/wch/r-source/blob/trunk/src/library/tools/R/sotools.R

Local validation (R 4.6.0, macOS arm64): fresh build and full package check
passed. After correcting Blosc's unnecessary SIMD probe, incremental standard
installation and R CMD check using that installation log also returned Status:
OK. The final install log has no compiler or CMake warnings. Examples, package
tests, the native diagnostics fixture, and the C/C++ consumer (8,000 concurrent
samples plus R handle checks) passed. R automatically recorded r_api.o and the
two static archives; both shared libraries load from the installed directory.

## 13.1.0-3: use RcppParallel's oneTBB

The previous private-runtime check at https://win-builder.r-project.org/zp5LWPDffom8/
passed installation, compiled-code inspection, examples, and tests. Its only NOTE
was New submission; its installation log contained no compiler warnings.

This revision replaces that private runtime with RcppParallel (>= 6.2.1), which
provides a shared oneTBB library on Windows as well as Unix. The old vendored TBB
source, patches, and copied notices are removed. OpenVDB and Blosc remain static
archives, and the diagnostic queue now lives in the provider. vdb_messages()
collects OpenVDB/Blosc messages; it no longer promises to intercept TBB messages.
The build uses RcppParallel's public compiler/linker flags and namespace loading.
R performs standard native-library installation and symbol collection. This
supersedes the private-runtime build and installation descriptions above.

Validation of the RcppParallel build: R 4.6.0 on macOS arm64, full R CMD check
--no-manual: Status OK (0 errors, 0 warnings, 0 NOTEs); installation has no
compiler or CMake warnings. A fresh R process confirms the loaded TBB comes from
RcppParallel 6.2.1. The installed package contains only openvdbr.so and R's own
object metadata. The native diagnostic fixture passes with shared TBB, and the
C/C++ consumer passes 8,000 concurrent samples and R-handle checks. A clean
source archive contains no TBB source, copied TBB notices, or custom installer.
Windows confirmation of version 13.1.0-3 remains pending.

## 13.1.0-4: system Blosc with bundled fallback

Detection follows CRAN blosc's pkg-config approach, including --static on macOS,
then checks paired header/library installations at standard prefixes or explicit
BLOSC_INCLUDE_DIR/BLOSC_LIB_DIR locations. A C++ compile-and-link probe requires
Blosc 1.x >= 1.17.0 (OpenVDB 13.1's minimum), blosc_compress_ctx,
blosc_decompress_ctx, and blosc_cbuffer_validate. Incompatible or unavailable
candidates fall back to bundled 1.21.6. No downloads or installer changes are
involved. OPENVDBR_USE_SYSTEM_BLOSC=false forces the bundled build.

The selected include paths are propagated to OpenVDB via a CMake target. R's
final link uses the validated library paths and dependencies, with shell quoting
for spaces. No bundled Blosc header or archive is used in the system build.

Validation: full R CMD check --no-manual on macOS arm64 passed for both system
and bundled backends, each with 0 errors, 0 warnings, 0 NOTEs and no compiler or
CMake warnings. The system build used a custom path containing spaces. Separate
configuration tests passed for pkg-config discovery, valid custom paths, old
headers, Blosc2 headers, missing APIs, an unusable library, and switching back
to a valid installation in the same build directory. Compressed VDB files
written by each backend were read by the other with all 729 voxel values exact.

References:
- https://github.com/cran/blosc/blob/master/configure.ac
- https://github.com/cran/blosc/blob/master/src/Makevars.win
- https://github.com/AcademySoftwareFoundation/openvdb/blob/v13.1.0/cmake/config/OpenVDBVersions.cmake

Windows validation of version 13.1.0-4 is pending.
