# 0029: CI runs on a tag or by hand, not on every push

**Status:** accepted. Changes when CI runs in [0025](0025-ci-and-releases.md).

## Context

[0025](0025-ci-and-releases.md) runs CI on every push and pull request. A publication pushes `main`
and often a tag right after it. The two runs build the same tree twice, on three platforms, and
take about 7 minutes each. The owner cancelled such runs by hand.

## Decision

- CI runs when a tag `v*` is pushed, and when it is started by hand (`workflow_dispatch`, from the
  Actions page or `gh workflow run CI --ref main`).
- A push to a branch and a pull request run nothing.
- What a run does is unchanged: the three platforms, the suite, the packages, and the release for a
  tag.

## Consequences

- A change that fails on another platform is found when the owner starts a run, or at the tag. It
  is no longer found at each push.
- A tag that fails CI has no release. It is fixed and the tag is moved, or a new tag is made.
