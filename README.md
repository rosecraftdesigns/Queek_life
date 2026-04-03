# Queek Life — ALifeSim v0.4

A C++17 artificial life simulation with a biochemistry engine, neural brain, genome-driven evolution, and SDL2 rendering. Inspired by *Creatures / Norns*, rebuilt with systemic clarity.

## Building (Windows / MSYS2 UCRT64)

```powershell
# Install dependencies once
pacman -S mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-glm mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja

# Configure + build
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
cmake -B build_win -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build_win
```

## Running

```powershell
./build_win/alife.exe
```

| Key | Action |
|-----|--------|
| Arrow keys / drag | Pan camera |
| `+` / `-` | Zoom |
| `Space` | Pause / unpause |
| `F1` | Toggle ATP / chemical heatmap |
| `F2` | Toggle fear / brain state overlay |
| `F3` | Toggle food / genome diversity |
| `F4` | Toggle perception radius / velocity vectors |
| `Esc` | Quit |

## Tests

```powershell
ninja -C build_win test_biochem test_brain test_world test_genome test_sim
cd build_win && ctest --output-on-failure
```

## Architecture

```
include/biochem/   — 256-slot ChemicalPool, Michaelis-Menten Reactions, Emitters, Receptors
include/brain/     — 1000-neuron Brain, SVRule opcodes, WTA decision lobe, reinforcement learning
include/world/     — 100×100 WorldGrid, logistic food growth, toxin decay, zone types
include/sim/       — Genome (32 genes), GenomePhenotype, Creature, SimSystem, RewardSystem, Species
include/render/    — Renderer, Camera, TileRenderer, DebugOverlay (bitmap font)
src/main.cpp       — SDL2 game loop, creature + signal rendering, HUD
```

## Roadmap

- [x] Phase 1 — SDL2 window, camera, debug overlay
- [x] Phase 2 — WorldGrid, food growth, tile renderer
- [x] Phase 3 — Genome, phenotype, species tracker
- [x] Phase 4 — SimSystem, reward learning, signals, visual traits
- [ ] Phase 5 — Creature interactions (collision, predator/prey)
- [ ] Phase 6 — Evolution loop (reproduction rules, population cap)
- [ ] Phase 7 — Player agency (world editor, Creature Lab, scenario modes)
- [ ] Phase 8 — UI / observability (inspector panel, event log)
- [ ] Phase 9 — Save / replay / deterministic seeds
- [ ] Phase 10 — Full integration test suite
