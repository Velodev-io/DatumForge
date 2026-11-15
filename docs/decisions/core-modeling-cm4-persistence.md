# CM4.c — durable supports and explicit reference repair

1 November 2026. Authorized by “go ahead with CM4.c”. Work on
`review/cm4-c-20261101`; commit/push after scoped verification, no merge.

Store the versioned base support model in the existing Part feature collection,
alongside its evaluated sketches and independently revalidated reference-evolution
record. Add a sticky reader manifest; older readers must refuse this semantic
extension even after undo. SQLite schema/ownership and existing budgets stay fixed.
One atomic semantic delta restores both collections, including pre-support state.
Reopen rebuilds from original definitions, never the stored evaluated frame.

`SetModelSupports` replaces the complete bounded same-Part model. Existing loose
sketch/feature edit commands must refuse supported Parts rather than erase their
associations. Repair may explicitly replace typed references in this command.
`SupportReferenceCandidates` inspects a selected datum/conversion/attachment
operand in its exact history position. `ResolveSupportReference` checks the model
fingerprint and a current typed candidate before committing the edited model.
Missing source objects/generated members can be explicitly reselected in the
complete model command; no guessed replacement or frozen-frame fallback.

Automatic topology evolution accepts direct unique signatures or a unique
one-to-one Modified kernel-history lineage in the producing stage. Split, merged,
deleted and unavailable results fail. No nearest shape or display ordinal is used.
The original reference remains its semantic identity; independently recomputed
evolution evidence records the current token without renaming converted members.

Validate save/recovery/undo/redo, analytic downstream geometry, failure/cancel
atomicity, manifest tampering and explicit repair against marked disposable roots.
CM4.d native controls and CM4.e complete course/native acceptance remain open;
preserve deferred 6.1b/6.4c/7.4c, physical 8 GB and other-platform gates.
