# AI Recovery Core — arrives in Phase 4

This directory will hold a **host-side** Python tool (it deliberately runs
*outside* the OS) that:

1. Reads the structured JSON log stream the kernel emits over QEMU's serial
   port (Phase 2 feature).
2. Groups log lines into incidents (e.g. a panic plus its preceding context).
3. Sends only structured logs — never code, never secrets — to the Claude API
   and asks for: a human-language explanation, fixes chosen **from a fixed
   catalog of repair actions**, and a risk class per fix.
4. Shows the proposal to a human. In Phase 4, **every** action requires manual
   approval; "applying" a fix means emitting a recovery flag/command for the
   next boot, never patching code.

Hard limits (see [docs/00-architecture.md](../docs/00-architecture.md)):
no automatic kernel modification, no unsigned code, no fetching executables
from the internet, no policy bypass — these are enforced by the policy
engine's design, not by asking the AI nicely.
