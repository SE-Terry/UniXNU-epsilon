# package/ (kits/epsilon in the assembled tree)

Empty scaffold. Board services (CPU/timer provider, display adapter, input, storage
bindings) are added here as separate tasks. Shared SM8150 operations stay in the
pinned `UniXNU-sm8150-common` feed (`kits/Sm8150Platform`), not here.

`firmware/README.md` holds the placeholder for the next task's first acceptance test.
