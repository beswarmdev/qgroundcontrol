# Rebuild do QGroundControl no Windows e migração para outro computador — Design

Data: 2026-11-09 · Branch: `worktree-win-rebuild` · Base: `master` @ `6eb58f084`

## Intenção

O repositório compilava corretamente no Windows (build Release concluído em 2026-11-09 11:51 em
`build\Windows`). O objetivo é provar que esse estado é reproduzível do zero, num workspace isolado,
executar o aplicativo e deixar documentado como levar tanto o código quanto o app pronto para outro
computador.

## Objetivos

- Recompilar o QGroundControl do zero (diretório de build novo) numa worktree isolada, preset `Windows`
  (Release, Ninja, MSVC 2022, Qt 6.12.0).
- Executar o `QGroundControl.exe` resultante e verificar que ele sobe e permanece estável.
- Gerar um pacote portátil (pasta + ZIP) via `cmake --install` e verificar que ele roda fora da árvore de build.
- Escrever um guia em pt-BR: pré-requisitos, build, execução, migração do código e do app, solução de problemas.

## Não-objetivos

- Corrigir no upstream a causa raiz das flags vazias do MSVC (apenas investigar e reportar; a proteção
  opcional em `cmake/Toolchain.cmake` fica como follow-up).
- Alterar o `build\Windows` ou a alteração local do checkout principal (fica como recomendação ao usuário).
- Build Debug, testes unitários, instalador NSIS (decisões do usuário: "Só Release" e "ZIP portátil").
- Qualquer mudança em código de produto (inclusive o `/EHsc` local, que se mostrou desnecessário).

## Anti-objetivos

- Reaproveitar o `build\Windows` antigo (não seria um rebuild total).
- Alterar o checkout principal `C:\gits\qgroundcontrol` ou sua branch `master`.
- Instalar software novo na máquina sem pedido explícito.

## Restrições (verificadas nesta máquina)

- Visual Studio 2022 Build Tools 17.14, MSVC 14.44.35207; `cl` só existe após `Enter-VsDevShell`/`vcvars64`.
- Qt 6.12.0 `msvc2022_64` em `C:\Qt\6.12.0\msvc2022_64`, passado via `QT_ROOT_DIR` (o preset usa
  `$penv{QT_ROOT_DIR}/lib/cmake/Qt6/qt.toolchain.cmake`).
- CMake 4.4.4 e Ninja via winget (CI fixa CMake 4.4.2 e Ninja 1.13.2; mínimo 3.28).
- O primeiro `cmake --preset Windows` de um diretório de build precisa rodar dentro do ambiente do MSVC.
  Um configure feito fora dele grava `CMAKE_<LANG>_FLAGS*` vazias no cache e o CMake nunca mais as
  reinicializa (ver "Achados").
- O configure exige um venv Python ≥ 3.10 em `tools\.venv` (gitignored). O `python3` do PATH é o stub da
  Microsoft Store (3.9.13) e o `install_python.py` exige `uv`, que não está no PATH.
- O cache de dependências CPM (`.cache\CPM`, ~4,1 GB, inclui o SDK do GStreamer 1.28.8) fica no checkout
  principal; o caminho não pode conter espaços.

## Decisões do usuário

- Migração: levar **ambos** — o código/ambiente de desenvolvimento e o app pronto.
- Pacote: **ZIP portátil** (NSIS apenas documentado como opção).
- Escopo de build: **só Release**.

## Decisões do builder (minhas)

- Worktree nativa do Claude Code em `.claude\worktrees\win-rebuild` (caminho máximo estimado ~224
  caracteres; `LongPathsEnabled=1` e `core.longpaths=true`).
- O rebuild usa o `master` sem nenhuma alteração de código: a alteração local `/EHsc` do checkout
  principal mostrou-se desnecessária (ver "Achados") e não entra na branch.
- `CPM_SOURCE_CACHE` aponta para o cache do checkout principal: o rebuild compila tudo do zero, mas não
  baixa de novo 4,1 GB de fontes/SDKs.
- `QGC_PYTHON_ENV` aponta para o venv já funcional do checkout principal, mecanismo suportado por
  `cmake/modules/PythonVenv.cmake` (evita instalar `uv`).
- Ambiente MSVC carregado no PowerShell com `Enter-VsDevShell` (equivalente ao `vcvars64.bat`).
- O ZIP portátil sai de `cmake --install --component Runtime`, o mesmo conjunto que o instalador NSIS empacota
  (sem cabeçalhos, bibliotecas estáticas nem as DLLs de depuração do runtime do MSVC).
- Documentos ficam em `docs/superpowers/` (spec, plano e o guia em `guides/`), fora da documentação
  oficial `docs/en`.

## Achados

- **Cache envenenado no `build\Windows` original.** O primeiro configure rodou fora do ambiente do MSVC
  (`CMakeConfigureLog.yaml`: `Compiler: CMAKE_C_COMPILER-NOTFOUND`) e deixou vazias no cache
  `CMAKE_CXX_FLAGS`, `CMAKE_CXX_FLAGS_RELEASE` e as flags de linker. Consequências: faltou `/EHsc`
  (C4530 virou erro com `-WX`, daí a alteração local) e o "Release" foi compilado **sem `/O2` e sem
  `/DNDEBUG`**. Um configure limpo dentro do dev shell obtém `/DWIN32 /D_WINDOWS /EHsc` e
  `/O2 /Ob2 /DNDEBUG`. O CI não é afetado (configura sempre um diretório novo depois do setup do MSVC).
- **Correção:** nenhuma mudança de código; reconfigurar com `cmake --preset Windows --fresh` (ou apagar
  `build\Windows`) a partir do dev shell.
- **Venv Python ausente em checkouts novos:** `tools\.venv` é gitignored; o bootstrap automático escolhe o
  stub `WindowsApps\python3.exe` (3.9.13) e falha. Na worktree, `QGC_PYTHON_ENV` resolve; num PC novo é
  preciso Python ≥ 3.10 + `uv` e `python tools\setup\install_python.py scripts`.

## Critérios de sucesso (verificáveis)

1. `cmake --preset Windows --fresh` e `cmake --build --preset Windows` terminam com código 0.
2. `build\Windows\Release\QGroundControl.exe` existe com timestamp do novo build.
3. O processo inicia, abre a janela principal e continua respondendo após ~30 s, sem crash.
4. `cmake --install` gera uma pasta autocontida; o ZIP extraído em outro diretório também abre o app.
5. O guia em pt-BR cobre pré-requisitos, build, migração do código, migração do app e problemas comuns,
   com os resultados medidos (tempo de build, tamanho do ZIP).
