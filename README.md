# Sabak Engine
3D game engine in plain JavaScript + WebGL2 (no build step). By SABAK STUDIOS.

- Renderer: PBR-lite, shadows, skinning, instancing, sky, fog, ACES
- glTF/GLB loading with animations, ECS scene graph, physics, input (touch joystick), audio
- **Simulator** (`src/driver.js`, `tools/sim.py`): `window.sabak` lets a script press keys, step frames deterministically, read state and take screenshots
- **iOS**: `.github/workflows/ios.yml` builds an unsigned IPA (Capacitor) -> Releases > latest

```
python3 tools/sim.py demo/index.html '[["pause"],["press","KeyW"],["step",120],["shot"],["state"]]' out
```
Commands: pause, resume, step(n,dt), press(...keys), release, releaseAll, tap, stick(x,y), look(dx,dy), nodes, find(name), teleport(name,x,y,z), camera(pos,target), state, shot.

## C++ core (`core/`)
Native engine core (C++17, OpenGL ES 3). Headless simulator `sabaksim` reads commands from stdin:
`size W H | press W/A/S/D | release K | releaseall | stick X Y | look DYAW DPITCH | step N [dt] | shot FILE.png | state`
Build: `cmake -S core -B core/build -G Ninja && cmake --build core/build`
