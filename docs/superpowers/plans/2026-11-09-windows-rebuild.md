# Rebuild do QGroundControl no Windows — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Recompilar do zero o QGroundControl (Release) numa worktree isolada, executá-lo, empacotá-lo como ZIP
portátil e documentar a migração do código e do app para outro computador.

**Architecture:** Worktree nativa em `.claude\worktrees\win-rebuild` sobre o `master` sem alterações; build pelo preset
`Windows` (Ninja + MSVC 2022 + Qt 6.12.0) reaproveitando apenas caches de download (CPM) e o venv Python do
checkout principal. Três frentes paralelas: build (caminho crítico), investigação das flags MSVC (subagente,
só leitura) e redação do guia (subagente). Execução, empacotamento e revisão dependem do build.

**Tech Stack:** CMake 4.4.4 presets, Ninja, MSVC 14.44, Qt 6.12.0 msvc2022_64, GStreamer 1.28.8 (SDK via CPM),
PowerShell 5.1.

**Spec:** `docs/superpowers/specs/2026-11-09-windows-rebuild-design.md`

## Global Constraints

- Preset `Windows` (Release, `binaryDir` `build/Windows`); nunca reutilizar `C:\gits\qgroundcontrol\build`.
- Ambiente: `Enter-VsDevShell` (BuildTools 2022, `-arch=x64 -host_arch=x64`),
  `QT_ROOT_DIR=C:\Qt\6.12.0\msvc2022_64`, `CPM_SOURCE_CACHE=C:\gits\qgroundcontrol\.cache\CPM`,
  `QGC_PYTHON_ENV=C:\gits\qgroundcontrol\tools\.venv`.
- Nada é escrito no checkout principal `C:\gits\qgroundcontrol` (só leitura dos caches/venv).
- Nenhum software novo é instalado (sem NSIS, sem uv).
- Documentos em pt-BR, Markdown com LF/UTF-8, em `docs/superpowers/`.
- Commits em Conventional Commits com a linha `Co-Authored-By` do agente.

## Review Focus

1. Computador sem `tools\.venv` ou com o stub da Store (`python3` 3.9) no PATH: o configure falha; o guia
   precisa dizer como criar o venv (Python ≥ 3.10 + `uv` + `install_python.py scripts`).
2. Computador de destino sem o Visual C++ Redistributable: o app do ZIP pode não abrir; o guia precisa dizer
   se o runtime vai no pacote ou precisa ser instalado.
3. Máquina sem GPU/OpenGL adequado (VM, RDP): o app abre em branco ou fecha; o guia cita `--swrast`.
4. Copiar `build\` para outro PC: caminhos absolutos no cache quebram o build; o guia proíbe e manda reconfigurar.
5. `CPM_SOURCE_CACHE` com espaços no caminho: o pkg-config do GStreamer falha; o guia indica um caminho sem espaços.

---

### Task 1: Worktree isolada — CONCLUÍDA

- [x] **Step 1:** `EnterWorktree` com nome `win-rebuild` (base `6eb58f084`, igual a `origin/master`).
- [x] **Step 2:** O `/EHsc` local foi aplicado e commitado (`b07215938`) e depois **descartado** quando a Task 3
  provou que ele só mascarava um cache envenenado; a branch voltou a `6eb58f084` sem mudanças de código.

### Task 2: Rebuild total Release (caminho crítico)

**Files:** nenhum arquivo versionado; saída em `build\Windows` (gitignored); log em `<scratchpad>\build.log`.

**Interfaces:**

- Produces: `build\Windows\Release\QGroundControl.exe` (sem o runtime do Qt/GStreamer ao lado; ver Task 4);
  `build\Windows` configurado para `cmake --install` (Task 5).

- [x] **Step 1:** Primeira tentativa: o configure falhou em `cmake/modules/PythonVenv.cmake:93` (bootstrap com
  `WindowsApps\python3.exe` 3.9.13). Causa raiz: `tools\.venv` é gitignored e não existe na worktree;
  `install_python.py` exige `uv`, ausente do PATH.
- [x] **Step 2:** Segunda tentativa (com `QGC_PYTHON_ENV`) configurou corretamente e foi interrompida em 1291/3202
  para remover o `/EHsc` redundante (Task 3).
- [x] **Step 3:** Apagar `build\Windows` e rodar com o ambiente das Global Constraints:
  `cmake --preset Windows` e depois `cmake --build --preset Windows`.
  Esperado: log com `QGC: using Python venv interpreter C:/gits/qgroundcontrol/tools/.venv/Scripts/python.exe`,
  `-- Generating done`, `BUILD END exit=0` e, no cache, `CMAKE_CXX_FLAGS_RELEASE:STRING=/O2 /Ob2 /DNDEBUG`.
- [x] **Step 4:** Verificar: `Test-Path build\Windows\Release\QGroundControl.exe` → `True`, com `LastWriteTime`
  posterior ao início do build; registrar a duração total do log. Resultado: exit 0, 3202 passos, 934 s
  (166 s de configure), exe de 49,8 MB às 12:31:12.

### Task 3: Investigar as flags vazias do MSVC (subagente, somente leitura, paralela à Task 2)

**Interfaces:**

- Produces: relatório com causa raiz (arquivo:linha), se o Release sai sem `/O2`/`/DNDEBUG`, se o CI é afetado
  e a correção mínima recomendada. Nenhuma alteração no repositório.

- [x] **Step 1:** Reproduzir com um projeto CMake mínimo no scratchpad (sem toolchain, com o toolchain do Qt,
  com as policies do QGC) e comparar `CMAKE_CXX_FLAGS`/`CMAKE_CXX_FLAGS_RELEASE`. Resultado: só o caso
  "primeiro configure fora do dev shell" deixa as flags vazias.
- [x] **Step 2:** Confirmado: `build.ninja` antigo sem nenhum `/O*`; configure novo com `/O2 /Ob2 /DNDEBUG`.
- [x] **Step 3:** O coordenador registra o resultado no guia (Task 6) e propõe a proteção em
  `cmake/Toolchain.cmake` como follow-up upstream.

### Task 4: Executar e verificar o QGroundControl

**Interfaces:**

- Consumes: `build\Windows\Release\QGroundControl.exe` (Task 2).

- [x] **Step 1:** Smoke test automatizado: `QGroundControl.exe --simple-boot-test --allow-multiple --log-output`
  com stdout/stderr redirecionados. Esperado: o processo termina sozinho com código 0 em até 120 s.
  Resultado: sem nada no `PATH`, `0xC0000135` (o build não copia as DLLs do Qt/GStreamer para `Release`);
  com `$QT_ROOT_DIR\bin` e o `bin` de `QGC_GST_RESOLVED_ROOT` no `PATH`, código 0 em 7,5 s.
- [x] **Step 2:** Execução real com interface (resultado: janela em 4,1 s, respondendo após 30 s, saída 0): `Start-Process` em `QGroundControl.exe --allow-multiple`, aguardar
  até o `MainWindowTitle` ficar não vazio (no máximo 60 s), confirmar `Responding=True` e processo vivo após mais
  30 s, capturar screenshot da janela e encerrar o processo iniciado.

### Task 5: Pacote portátil ZIP

**Interfaces:**

- Consumes: `build\Windows` configurado (Task 2).
- Produces: `C:\gits\qgc-dist\QGroundControl\` e `C:\gits\qgc-dist\QGroundControl-5.2.0-dev-win64-portable.zip`
  (fora da worktree, para sobreviver à remoção dela).

- [x] **Step 1:** `cmake --install build\Windows --config Release --prefix C:\gits\qgc-dist\QGroundControl`.
  Resultado: código 0 em 14 s; `bin\QGroundControl.exe` existe; 415 MB.
- [x] **Step 2:** Runtime MSVC (`vcruntime140*.dll`, `msvcp140*.dll`) incluído em `bin\` (também as versões `*d.dll`
  de depuração, vindas da regra de install do GeographicLib).
- [x] **Step 3:** ZIP de 169,7 MB; extraído numa pasta nova e com `PATH` só de `C:\Windows`, `--simple-boot-test`
  saiu com código 0 em 9,6 s.
- [x] **Step 4 (após a revisão):** refazer com `--component Runtime`, o mesmo conjunto do instalador NSIS.
  Resultado: 298 MB, 130 arquivos em `bin\`, sem `*140d.dll` nem `include\`; ZIP de 142,1 MB; extraído e com
  `PATH` mínimo, `--simple-boot-test` com código 0 em 8 s. Este é o ZIP final.

### Task 6: Guia de build e migração (subagente, paralela à Task 2)

**Files:**

- Create: `docs/superpowers/guides/windows-build-e-migracao.md`

- [x] **Step 1:** O subagente escreve o guia (seções: resumo, pré-requisitos, build, migração do código,
  migração do app, problemas, referências) com o marcador `<!-- RESULTADO-VERIFICADO -->` na seção do app.
- [x] **Step 2:** O coordenador revisa os fatos contra o repo, substitui o marcador pelos números medidos
  (tempo de build, tamanho do ZIP, resultado do teste do ZIP) e inclui o resultado da Task 3.
- [x] **Step 3:** Commit `docs: add Windows build and migration guide` com spec, plano e guia.

### Task 7: Revisão, verificação e fechamento

- [x] **Step 1:** superpowers:requesting-code-review sobre a branch inteira (só documentos; nenhum código).
  Veredito "With fixes": comando `Select-String` posicional quebrado, boot test sem `--allow-multiple`, worktree
  travado na cópia por robocopy, checklist com caminhos desta máquina, conteúdo do ZIP; todos corrigidos.
- [ ] **Step 2:** superpowers:verification-before-completion: reexecutar as verificações das Tasks 2, 4 e 5 e
  só então declarar sucesso.
- [ ] **Step 3:** superpowers:finishing-a-development-branch: apresentar as opções de integração ao usuário.

## Paralelismo

```text
T1 ──► T2 (build, ~15 min) ──► T4 (executar) ──► T5 (ZIP) ──┐
   ├─► T3 (flags, subagente) ──────────────────────────────────┼─► T6 Step 2-3 ──► T7
   └─► T6 Step 1 (guia, subagente) ────────────────────────────┘
```
