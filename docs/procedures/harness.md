# Harness del sistema multiagente — Multiband Saturator

Procedimiento operativo que el `orchestrator` debe respetar. Todo lo que hay aquí está medido sobre
esta máquina (Windows + MSYS2); no se aceptan comandos «probables».

- Repo: `C:\Coal_Dynamics\Multiband_Saturator` (rama de integración: `main`)
- SDK: `C:/Rack-SDK2.6.6` (`Makefile`: `RACK_DIR ?= /c/Rack-SDK2.6.6`)
- Config de agentes: `C:\Users\manub\.config\opencode\agent\`

## 1. Roster y modelos

| Rol | Agente | Modelo | Permisos |
|---|---|---|---|
| Plan / reparto | `orchestrator` (primary) | `opencode/deepseek-v4.1-flash-free` | `edit: deny`, `task: allow` |
| Specs / plan de ficheros | `architect` | `opencode/flint-40b-free` (`variant: xhigh`) | escribe solo en `specs/` |
| Mapeo y verificación rápida | `auditor` | `opencode/nemotron-3.5-lightning-free` | solo lectura |
| Implementación | `local-coder` | `ollama/qwen3:8b` (`num_ctx 16384`) | `edit: allow`, `bash: ask` |
| Desborde (2 escritores más) | `general` | built-in | `edit: allow` |
| Juez J1 (correctitud) | `judge-correctness` | `opencode/nemotron-3-ultra-free` | solo lectura, `webfetch: deny` |
| Juez J2 (simplicidad) | `judge-simplicity` | `opencode/glm-5.3-flash-free` | solo lectura, `webfetch: deny` |

Los jueces tienen `hidden: true`: no aparecen en la lista de subagentes del orquestador, pero **sí son
invocables por id**. Los tres modelos de juicio/arquitectura son distintos entre sí a propósito
(independencia de los veredictos).

## 2. Gate (criterio único de «verde»)

No hay framework de tests unitarios en este repo. El gate es **build limpio + criterios de aceptación
de la spec**. Se ejecuta con el shell de MSYS2 (MINGW64), nunca con `g++` a mano (falta
`-D_USE_MATH_DEFINES`, ver `specs/multiband_saturator_spec.md` B3).

```bash
export PATH=/mingw64/bin:$PATH          # jq; sin esto make falla con "SLUG could not be found"
cd /c/Coal_Dynamics/Multiband_Saturator

make clean && make 2>&1 | tee /tmp/build.log
grep -ci warning /tmp/build.log         # debe dar 0
grep -ci error   /tmp/build.log         # debe dar 0
test -f plugin.dll                       # `make` enlaza ./plugin.dll (raiz); `Saturator/` es solo
                                         # una copia local de instalacion, no la genera `make`

make dist                               # -> dist/Coal_Dynamics-<version>-win-x64.vcvplugin
```

Verificado sobre un `git clone` limpio del repo: `make` → 0 warnings / 0 errores y `plugin.dll`
(2.1 MB), `make dist` → `.vcvplugin` de 134 KiB. Si un clone no compila, el problema es de ficheros
fuera de git, no del entorno.

Referencia de tiempos (medidos): `make` limpio ~7 s, incremental ~1-3 s, no-op ~0.9 s,
`make clean && make` ~4 s, `make dist` ~1.6 s. Si un build tarda un orden de magnitud más, algo se ha
roto en el entorno (jq, `RACK_DIR`, reparto de capas a CPU en Ollama).

**El gate no es «compila»**: la spec activa define los criterios verificables (p. ej.
`specs/oversampling_spec.md` §7). El orquestador los copia literalmente en la directiva del escritor y
en la del juez. Sin criterios verificables no se integra nada.

### 2.1 Medición DSP (cuando se toca el audio)

Arnés **offline fuera del repo** (`%TEMP%`), sin Rack ni `make`, según `specs/oversampling_spec.md` §8:

```bash
g++ -std=c++11 -D_USE_MATH_DEFINES \
  -I C:/Rack-SDK2.6.6/include -I C:/Rack-SDK2.6.6/dep/include \
  -I C:/Coal_Dynamics/Multiband_Saturator/src \
  os_harness.cpp -o os_harness.exe
```

Los números de referencia viven en `specs/multiband_saturator_spec.md` §C-7. Si el cambio toca DSP y no
se re-mide, el veredicto es RECHAZADO por falta de evidencia.

## 3. Reparto de escritores

1. **Un escritor por worktree.** Nunca dos escritores en el mismo árbol ni dos worktrees sobre el mismo
   fichero.
2. **Máximo 3 escritores** en vuelo: `local-coder` (prioridad) + hasta dos `general` de desborde.
3. **Ficheros disjuntos** por unidad de trabajo. Si dos unidades comparten fichero, se serializan.
4. `local-coder` recibe **una sola operación de fichero por directiva** (crear *o* editar *o* leer) y
   se continúa con la misma sesión. Mezclar escritura y edición en el mismo turno falla por vista
   obsoleta del sistema de ficheros.

Crear y retirar worktrees (PowerShell o MSYS2, desde el repo principal):

```bash
git worktree add ../MS_w1 -b feat/w1        # una por unidad de trabajo
git worktree list
git worktree remove ../MS_w1                # al terminar; --force solo con confirmación
```

Reglas de propiedad: `.gitignore`, `Makefile`, `plugin.json` y los ficheros de `res/` son **zona
caliente** (los tocan muchos cambios): un solo dueño por ronda. `specs/` es del `architect`.

## 4. Flujo

1. **`architect`** → `specs/<tema>.md`: objetivo, alcance, contratos, plan de ficheros (con la lista
   explícita por unidad y su disjunción), criterios de aceptación verificables y riesgos.
2. **`auditor`** → tabla `fichero → propósito → punto de cambio` con `file:line`. Verifica que las
   rutas y firmas citadas por el arquitecto existen.
3. **Reparto** → worktrees + directiva por escritor (fichero, línea, contrato, gate parcial).
4. **Implementación** → `local-coder` (y `general` de desborde), diff mínimo, sin tocar ficheros fuera
   de la lista.
5. **Gate** → §2 sobre el artefacto integrado en la rama de integración.
6. **Juicio** → `judge-correctness` y `judge-simplicity` en **sesiones separadas**, sin ver los
   informes de los demás ni el uno del otro. Cada uno recibe solo: spec + criterios + artefacto.
7. **Integración** → en `main`, tras gate verde y ambos veredictos. Nunca `git push` desde un worktree.

Bucle: si un juez RECHAZA, el orquestador convierte cada defecto priorizado en una directiva nueva
para `local-coder` y vuelve al paso 5. Los jueces se vuelven a invocar con contexto limpio.

## 5. Invariantes de compatibilidad

- No cambiar los IDs de parámetros, entradas ni salidas sin spec explícita: rompe patches guardados.
- Cambios de estado deben tener valor por defecto y caída segura ante valor inválido (ver
  `specs/oversampling_spec.md` §7.7).
- El dry path de MIX y la latencia declarada deben seguir cuadrando si se toca el oversampling.

## 6. Prohibiciones

- No compilar fuera de `make` (ver B3 de la spec).
- No commitear artefactos: `build/`, `dist/`, `Saturator/`, `*.dll`, `*.vcvplugin`,
  `opencode-backup-*` están en `.gitignore`.
- No `git push` desde un worktree; no reescribir `main` (sin `reset --hard`, sin `push --force`).
- Un agente no edita la config de otro agente ni `docs/procedures/` durante una tarea de código.
