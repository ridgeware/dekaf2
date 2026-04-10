# dekaf2 Include Restructuring Plan

## Goal
Move from a flat `dekaf2/*.h` layout to the hierarchical structure defined in
`~/src/dekaf2-new-struct.txt`, while maintaining full backward compatibility for
existing consumers (`#include <dekaf2/kstring.h>` must keep working).

## Current State
- ~190 public headers in the root directory, installed flat into
  `${prefix}/include/dekaf2/dekaf2/*.h`
- `bits/` subdirectory for internal headers (~38 files)
- `compat/`, `from/`, `libs/` subdirectories (stay unchanged)
- Single monolithic `CMakeLists.txt` (~2400 lines) with `PUBLIC_HEADERS`,
  `BITS_HEADERS`, `SOURCES` lists and all install rules
- `dekaf2all.h` is not up to date — refresh as part of Phase 0

## Key Design Decisions

### Stub Headers for Backward Compatibility
- Stub headers go into `compat/flat/` in the source tree (NOT in the root)
- Each stub is a one-liner: `#include <dekaf2/core/strings/kstring.h>`
- At install time, stubs get installed into the flat location
  `${prefix}/include/dekaf2/dekaf2/` — so old `#include <dekaf2/kstring.h>`
  keeps working
- Installation of flat stubs is controlled by CMake option
  `DEKAF2_INSTALL_FLAT_HEADERS` (default ON) — can be switched OFF once all
  consumers have migrated
- In the source tree, the root stays clean — developers navigate the new
  hierarchy

### Source Directory Layout
- All new hierarchical files live under `source/` in the source tree
  (e.g. `source/core/strings/kstring.h`)
- This keeps the root directory clean — no new subdirectories mixed with
  `benchmarks/`, `templates/`, `samples/`, etc.
- Include path is `#include <dekaf2/core/strings/kstring.h>` (no `source/`)

### Build-Phase Include Resolution (Symlinks)
- Existing: `${BUILD_DIR}/generated/dekaf2 → ${SOURCE_DIR}` — resolves
  flat includes like `<dekaf2/kstring.h>`
- New during migration: `${BUILD_DIR}/structured/dekaf2 → ${SOURCE_DIR}/source/`
  — resolves hierarchical includes like `<dekaf2/core/strings/kstring.h>`
- Both directories are in `BUILD_INTERFACE` include paths
- After migration is complete: change `generated/dekaf2 → ${SOURCE_DIR}/source/`
  and remove the extra `structured/` symlink
- Unit tests and smoke tests (built before install) use the same include paths

### bits/ Files: Per-Category Migration
- `bits/` files that belong to a specific category move into that category's
  `bits/` subdirectory (e.g. `bits/klogwriter.h` → `core/logging/bits/klogwriter.h`)
- Generic `bits/` files that serve multiple categories stay in `bits/`

### CMake Structure
- Keep the single `CMakeLists.txt` during migration for better overview
- Split into per-category CMakeLists.txt files as a later follow-up

### Source Files (.cpp)
- Move alongside their headers into the new subdirectories
- CMake `SOURCES` list updates to use the new paths
- No stub .cpp files needed (consumers don't include them)

## Execution Plan

### Phase 0: Preparation
1. **Refresh `dekaf2all.h`** — Regenerate from the current `PUBLIC_HEADERS`
   list so it includes all ~190 headers (currently missing ~20+)
2. **Snapshot the current install manifest** — Record what's currently installed
   under `${prefix}/include/dekaf2/dekaf2/` so we can verify nothing is lost
3. **Add a CI/build smoke test** — Ensure the existing flat includes compile
   before and after changes (use `dekaf2all.h` as the canary)
4. **Create `compat/flat/` directory**
5. **Add CMake option** `DEKAF2_INSTALL_FLAT_HEADERS` (default ON) to
   `CMakeLists.txt`, gated around the flat stub install rules
6. **Create the old-to-new path mapping file** (`restructure-map.txt`) —
   one line per header: `kstring.h → core/strings/kstring.h`
   This mapping drives stub generation and the migration script

### Phase 1: Create Directory Structure (no file moves yet)
7. Create all subdirectories from `dekaf2-new-struct.txt`:
   `core/strings/`, `core/types/`, `core/errors/`, etc.
8. No CMake changes yet — this is a no-op for the build

### Phase 2: Move Files — One Category at a Time
Start with a leaf category that has few internal dependencies (e.g.
`core/types/` or `time/clock/`). For each category:

9.  **Move headers + sources** into `source/` subdirectory
    (e.g. `ktime.h` → `source/time/clock/ktime.h`, `ktime.cpp` → `source/time/clock/ktime.cpp`)
10. **Update internal #includes** in the moved files to use new absolute paths
    (`#include <dekaf2/time/clock/ktime.h>`)
11. **Create stub header** in `compat/flat/`:
    ```cpp
    // compat/flat/ktime.h — backward compatibility, include new location
    #pragma once
    #include <dekaf2/time/clock/ktime.h>
    ```
12. **Move category-specific bits/** files if applicable
    (e.g. `bits/klogwriter.h` → `source/core/logging/bits/klogwriter.h`)
13. **Update CMakeLists.txt**:
    - Update path in `SOURCES` and `PUBLIC_HEADERS` lists
    - Add new install rule for the subdirectory
    - Install `compat/flat/` stubs into the flat location (gated by
      `DEKAF2_INSTALL_FLAT_HEADERS`)
14. **Build + run unit tests + run smoke tests** after each category

### Suggested Move Order (low-dependency → high-dependency)
1. `core/types/` — kdefinitions, ktemplate, kvariant, kbit, kbitfields, kctype, kutf
2. `core/errors/` — kexception, kerror, kcrashexit, ksourcelocation
3. `core/strings/` — kstring, kstringview, kstringutils, kcaseless, ksplit, kjoin, kwords, kregex, kmpsearch
4. `core/format/` — kformat, kformtable, kbar
5. `core/logging/` — klog, klogrotate + bits/klogwriter, bits/klogserializer
6. `containers/*`
7. `io/*`
8. `system/*`
9. `crypto/*`
10. `data/*`
11. `time/*`
12. `threading/*`
13. `net/*`
14. `http/*`
15. `rest/*`
16. `web/*`
17. `util/*`

### Phase 3: Update Internal Cross-References
15. After all files are moved, do a sweep for any remaining old-path `#include`
    statements inside dekaf2 itself
16. Update `dekaf2all.h` to use new paths
17. Update `dekaf2.h` if it includes any moved headers
18. Update Doxygen `doxyfile.in` INPUT paths

### Phase 4: Install Rules
19. **New-structure headers**: Install from `source/` into
    `${prefix}/include/dekaf2/dekaf2/<category>/<subcategory>/`
20. **Flat stubs** (gated by `DEKAF2_INSTALL_FLAT_HEADERS`):
    Install `compat/flat/*.h` into `${prefix}/include/dekaf2/dekaf2/`
    (the old flat location) — this is the backward compatibility layer
21. **bits/**: Category-specific bits/ install into their category path;
    remaining generic bits/ stay in `${prefix}/include/dekaf2/dekaf2/bits/`
22. **Cleanup rules**: Remove old real headers from flat install location
    (they are replaced by stubs when `DEKAF2_INSTALL_FLAT_HEADERS=ON`,
    or simply absent when `OFF`)

### Phase 5: Consumer Validation
23. Build dekaf2 itself with new structure (including unit tests + smoke tests)
24. Build xapis against the new install — it should work unchanged via stubs
25. Build xliff, ddtd, mdev — all should work via stubs

### Phase 6: Consumer Migration Script
26. Create a **Python script** (`scripts/migrate-includes.py`) that rewrites
    `#include <dekaf2/kfoo.h>` → `#include <dekaf2/category/sub/kfoo.h>`
    in client project source files:
    - Reads the mapping from `restructure-map.txt`
    - Takes one or more directories/files as arguments
    - Dry-run mode (`--dry-run`) to preview changes
    - Reports all modified files
    - Handles both `#include <dekaf2/...>` and `#include "dekaf2/..."`
    - Idempotent — already-migrated includes are left untouched
27. Run the migration script on xapis, xliff, ddtd, mdev to validate
28. Optionally: set `DEKAF2_INSTALL_FLAT_HEADERS=OFF` once all consumers
    are migrated

## Resolved Decisions
- **Stub include style**: Absolute (`#include <dekaf2/time/clock/ktime.h>`).
  Works both in build tree (via `BUILD_INTERFACE`) and after install
  (via `INSTALL_INTERFACE`).
- **bits/ migration**: Per-category. Category-specific bits files move with
  their category. Generic bits files stay in `bits/`.
- **CMakeLists.txt split**: Later, after migration is complete.
- **Flat header install**: Controlled by `DEKAF2_INSTALL_FLAT_HEADERS` option
  (default ON). Switch OFF after all consumers are migrated.

## Risk Mitigation
- **One category at a time** — never move everything at once
- **Build + test after each category** — catch breakage immediately
- **Git commits per category** — easy to revert if needed
- **Stubs generated, not hand-written** — driven by `restructure-map.txt`
  to avoid typos
- **Migration script has dry-run mode** — preview before committing changes
  to consumer projects
