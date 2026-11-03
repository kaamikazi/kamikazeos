# Revision 2 Finding Resolutions And Hard Stops

This is a design-author response, not an independent acceptance review. The
previous re-review remains authoritative evidence, not a passing test result.

## F02: Exact A1 Mutation Ownership

**Status: PARTIALLY RESOLVED; BUILD-SPECIFIC CLOSURE NOT DEMONSTRATED.**

Revision 1 offered categories and a 4096-byte fatal-stack cap without a complete
store witness. Revision 2 requires an exhaustive address-domain and DWORD/lane
owner map, qualified write origin, per-PC effective-address/value/occurrence
rules, and an exact finite store-support union derived from linked F. No byte
becomes writable merely by lying in a stack/table/capture region.

The strict source interval is fixed by the unchanged case, not guessed from
payload size: `[B+1020,B+1032)`. The preserved live-stack interval is the observed
`[ESP_live,T)` plus the full bootstrap snapshot; nonoverlap is checked before
pivot. The destination fatal stack is the exact linker span, but only the
per-PC union within the 4096 cap can be writable. Source/record/CPU writes have
different origins and phase predicates. The maximum footprint is an exact set
cardinality required in B-RAM, not a statement that4096 bytes are permissible.

Remaining condition: F does not exist. Concrete arm/ack addresses, full CFG/store
union, exact cardinalities, per-word metadata bindings and complete independent
write-stream coverage are absent. **U-BUILD/U-RAM stay OPEN.** Their closure
requires the artifact rows in the build and RAM contracts, independently reviewed
before a separate execution request. Failure to derive even one reachable store
or normal-RAM owner blocks A1; no broad mask or observed-only high-water is allowed.

## F04: Operational CMOS-T1

**Status: PARTIALLY RESOLVED; SENSOR COVERAGE/CALIBRATION UNVERIFIED.**

Revision 2 names two independent observation surfaces: CPU predecode I/O attempts
at0x70/0x71, and effective RTC index/value cell updates. A calibration contract
pins two existing normal healthy-boot saves with the same known nine-byte block,
including the second save's idempotent writes. All18 data writes and18 index
selects must correlate, as must model update counts and loss/transport counters.
Positive counts cannot come from the guest's own success log or observer reads.

Coverage is enumerated per cell and port and for guest, initialization/reset,
timer, load/migration, management and alias/bank paths. A source-hash table alone
is not source-to-binary proof. Unaccounted update path, unknown trace semantics,
missing effective-index/predecode signal or silent calibration blocks equality.

Remaining condition: no exact RTC source/build attestation, audited complete
update graph or calibrated dual-surface observation is supplied. **U-CMOS stays
OPEN.** Historical Stage 2A trace reconstruction is retained within its limited
scope and cannot fill that gap. Calibration is neither implemented nor run here.

## F05: Non-Vacuous Forbidden-Writer Coverage

**Status: UNRESOLVED; COMPLETE OPERATIONAL COVERAGE NOT DEMONSTRATED.**

Revision 2 specifies an eleven-family core registry and explicit expansion rules
for every linked clone/inline/call-edge body. Actual numeric F entries, bytes,
caller/target proof, coverage mode and positive calibration are mandatory. The
core names are not eleven proved sufficient observation PCs. If complete
simultaneous coverage exceeds16 sites, the contract blocks; it cannot rotate
breakpoints or omit an inline body to fit the budget.

NC, ND and LK remain separate. NC requires predecode port-attempt evidence even
without a device callback. ND requires actual controller/ec/status00 evidence.
LK retains the pinned seed and genuine initialized ambiguity tuple; boot reads
are outside the fault window, and no writer entry or new ATA request is allowed
after fault onset. Device silence and write-return codes cannot establish nonentry.

Remaining condition: the complete numeric F registry, source-to-PC coverage,
simultaneous-capacity proof, per-site positive calibration and NC/ND topology
coverage are absent. **U-WRITER/U-DEVICE stay OPEN.** Missing coverage is UNVERIFIED;
an observed forbidden entry is immediate FAIL even with no I/O. This is not a
claim that the production unsafe branch currently calls a writer.

## Result And Change Control

Revision 2 narrows each unresolved condition to exact required files/evidence
and states how a false positive must be rejected. It does not claim to resolve
an unbuilt fixture or uncalibrated sensor. A1 remains **REVISE BEFORE
AUTHORIZATION** for A1-E. H0 remains ready for consideration of H0-I only.

F01's valid-frame/isolated-headroom predicate, F03's R1 parent/phase tuple and
F06's distinct later boundaries remain unchanged. All later experiments and
calibrations remain separately gated. No execution or Git write occurred.
