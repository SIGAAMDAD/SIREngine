# Third-party dependencies

EASTL, EABase and EAAssert are pinned external repositories. They may be enabled explicitly with `SIR_WITH_EASTL=ON` for modules requiring them; the initial native launcher does not depend on these libraries. Audio providers are resolved only when explicitly configured.
