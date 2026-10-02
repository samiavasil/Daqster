# Requirements — ONE-WAY EXPORT

> **This directory is a read-only export.**
>
> The **source of truth** for all requirements (both public `REQ-SW-*` and private
> `REQ-AI-*` / `REQ-PLG-*` / `REQ-SEC-*`) is the **private mono-repo**:
> `../DaqsterAiStudio/DevelopmentProcess/requirements/`
>
> Do not edit files here directly. Changes made here will be overwritten by the
> next export from the private repo.

## Structure (mirrored from private repo)

```
../DaqsterAiStudio/DevelopmentProcess/requirements/
├── README.md                    ← this file (export)
├── RDD-PROCESS.md               ← RDD process definition
├── RDD-STATUS.md                ← current phase state (export)
├── traceability-matrix.md       ← REQ ⟷ Commit ⟷ Test matrix (export)
├── active/
│   ├── app/                     REQ-SW-APP-*
│   ├── framework/               REQ-SW-FW-*
│   └── plugins/                 REQ-SW-PL-*
│   ├── REQ-AI-*                 private AI Studio requirements
│   ├── REQ-PLG-*                private plugin requirements
│   └── REQ-SEC-*                private security requirements
└── archive/
    ├── app/
    ├── build/
    ├── framework/
    └── plugins/
```

## Verification

Run from the private repo root:
```bash
./scripts/validate-traceability.sh
```

This script validates commit hashes against **both** repos (public + private)
using repo-qualified refs.
