# OEM storage diagnostics and QA

The initial audience is Windows software teams testing virtual storage adapters, raw block workloads and failure handling. The current preview attaches only to the owned Virtual Storage Fault Lab synthetic disk; physical-disk deployment is not a delivered feature. A fixed, bounded completion record can be embedded into a QA harness without recording customer file contents.

## Delivery model

- Public application, CLI, two-file SDK and example source help engineers reproduce the integration before purchasing services.
- A **paid, scoped integration pilot** starts with one reproducible disposable-disk workload and written acceptance tests.
- A qualified production package can carry separate **OEM redistribution/support terms**, a named Windows matrix, update policy and support scope. Annual maintenance covers agreed Windows compatibility work and regression tests.
- Additional attachment scope, always-on capture, system-disk deployment, per-process attribution and remote fleet collection are separate engineering work; they are not included in this preview.

Contact **titancsa on Discord** with the application, target Windows versions, expected device count and a sample test. There is no automatic checkout or enforced kernel entitlement in the preview. Commercial records in the portfolio administration do not activate this driver. Revenue and demand are hypotheses to validate through pilots, not promises.

## Existing alternatives and positioning

Windows already has documented [Disk I/O ETW events](https://learn.microsoft.com/en-us/windows/win32/etw/diskio) and [Windows Performance Recorder/Analyzer](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/). A new driver is not necessary for every disk-performance investigation. This project's narrow differentiation is a bounded native SDK and a chosen dispatch/completion boundary, rather than replacing WPR/WPA.

OSR's historical [IRPTracker](https://www.osronline.com/article.cfm%5Earticle%3D199.htm) demonstrates an established interest in understanding IRP paths and completions; its vendor page explicitly limits the old utility to Windows 7 and earlier. That is not evidence of current sales, and this project does not use its unsupported hooks or PatchGuard workarounds.

[HHD Device Monitoring Studio](https://hhdsoftware.com/device-monitoring-studio) sells native device-monitoring products for development/debugging and integration workflows. It is an adjacent commercial precedent, not proof of demand for this exact storage component. The first product proposition is **time saved integrating and qualifying a reproducible collector**, not access to an otherwise impossible capability.

## Technical references

- [Storage filter completion routines](https://learn.microsoft.com/en-us/windows-hardware/drivers/storage/storage-filter-driver-s-iocompletion-routines)
- [Primitive driver packages](https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/creating-a-primitive-driver)
- [KMDF control device lifetime rules](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/using-control-device-objects)

Reviewed 2026-10-25. These primary references establish technical behavior and existing product categories; they do not establish search volume or guaranteed commercial demand.
