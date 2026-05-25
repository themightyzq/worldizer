## What

Briefly describe the change.

## Why

What problem this solves or what slice/issue it advances.

## Testing Performed

How you verified the change (build, manual test in a host, pluginval, etc.). Include relevant output.

## Related Issues

Closes #

## Checklist

- [ ] Builds clean (`cmake -B build && cmake --build build`)
- [ ] Passes `pluginval --strictness-level 10` (if it touches the plugin)
- [ ] Follows the project code style (`JUCE_VST3_BEST_PRACTICES.md` / `JUCE_VST3_UI_UX_BEST_PRACTICES.md`)
- [ ] Updates docs where relevant (`Docs/architecture.md`, `TODO.md`, `CLAUDE.md`)
- [ ] No allocations / locks / file I/O added to the audio thread
