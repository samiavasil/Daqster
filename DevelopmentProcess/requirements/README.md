# Requirements — ONE-WAY EXPORT

> **This directory is a read-only export.**
>
> The **source of truth** for all requirements is the upstream mono-repo the
> export is generated from.
>
> Do not edit files here directly. Changes made here will be overwritten by the
> next export.

## Structure

```
<upstream>/DevelopmentProcess/requirements/
├── README.md                    ← this file (export)
├── RDD-PROCESS.md               ← RDD process definition
├── RDD-STATUS.md                ← current phase state (export)
├── traceability-matrix.md       ← REQ ⟷ Commit ⟷ Test matrix (export)
├── active/
│   ├── app/                     REQ-SW-APP-*
│   ├── framework/               REQ-SW-FW-*
│   ├── plugins/                 REQ-SW-PL-*
│   ├── REQ-PLG-*                plugin requirements
│   └── REQ-SEC-*                security requirements
└── archive/
    ├── app/
    ├── build/
    ├── framework/
    └── plugins/
```

## Verification

Run from the export's upstream repo root:
```bash
./scripts/validate-traceability.sh
```

This script validates commit hashes against **both** repos (public + private)
using repo-qualified refs.
