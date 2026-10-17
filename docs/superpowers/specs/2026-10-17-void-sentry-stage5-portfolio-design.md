# Void Sentry Stage 5: Portfolio Polish

Date: 2026-10-17. Status: written by the assistant; the user asked to continue the roadmap without waiting for input.

## Problem

The roadmap's Stage 5 is "README with a demo GIF, architecture diagram, CI badges, write-up - something worth linking on a CV". The current README predates the rebuild and is wrong in several places:

- It describes an MJPEG stream, PWM pan/tilt servos and an overlay "injected into the MJPEG stream". In fact the stream is a push WebSocket of JPEG frames, yaw is a 28BYJ-48 stepper, and the HUD draws the overlay client-side.
- Its badges point at the old repository name (`Smart_Camera_ESP32`), and the main CI workflow (`ci.yml`) has no badge.
- It has no measured numbers, no wiring table, no build/test instructions that match the repo, and an unfinished setup section.
- The Doxygen project name is still "Smart Camera ESP32".

## Goal

A reader (recruiter or engineer) understands in two minutes what the system does, how it is built, and why the design looks the way it does. Every claim is backed by the code, the specs, or a measurement recorded in a PR.

## Decisions

| Decision | Choice | Why |
|---|---|---|
| Source of truth | Facts come only from `CLAUDE.md`, `docs/superpowers/specs/*`, the code, and the measured numbers listed in the plan | A portfolio README that overclaims is worse than none |
| Diagrams | Mermaid in Markdown (GitHub renders it): data flow from camera to HUD and turret; task-to-core layout | Text diagrams stay in sync in diffs; no binary images to maintain |
| README scope | Overview, highlights, architecture, measured performance, hardware and wiring, build/flash/test, HUD controls, project layout, roadmap status, license | What a reader needs; details live in the write-up and Doxygen |
| Write-up | New `docs/engineering-notes.md`: the design decisions and debugging stories, each as problem -> alternatives -> what won and why -> evidence | Interview material; keeps the README short |
| Demo GIF | A clearly marked slot in the README (HTML comment), recorded with the user later | Needs the hardware and a person moving in front of it |
| Badges | `ci.yml`, `static_analysis.yml`, Doxygen workflow, license, C++17, platform; all with the current repo path `KoganTheDev/void-sentry-esp32` | Shows CI health at a glance |
| Doxygen | `PROJECT_NAME = "Void Sentry"` | Matches the project |

## Success criteria

| Criterion | Target |
|---|---|
| Accuracy | No claim contradicts `CLAUDE.md` or the code; every number in the README appears in the plan's facts list |
| Rendering | Mermaid blocks parse (checked with `@mermaid-js/mermaid-cli` if available, else by careful syntax review); links resolve to files that exist |
| Style | No em dashes; plain, specific language; no marketing filler |
| CI | All checks pass |

## Out of scope

Recording the GIF (later, with the user), a project website, changes to firmware behaviour.
