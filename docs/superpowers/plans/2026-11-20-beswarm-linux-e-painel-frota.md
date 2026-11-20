# BeSwarm Vehicle GS — Binário Linux e Painel de Frota (RTL/LAND para todos)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Gerar o BeSwarm Vehicle GS também para Linux (AppImage x86_64), no mesmo fluxo código → `C:\gits\qgc-dist`, e
adicionar no Fly View, à direita do mapa, um painel recolhível com a lista de veículos e o estado do link e dois botões:
amarelo (RTL para todos) e vermelho (LAND para todos), que reenviam o comando um número configurável de vezes.

**Architecture:**

- **Windows primeiro:** tudo roda no Windows, menos o que exige Linux. O WSL2 `Ubuntu-26.04` só instala pacotes, guarda o
  Qt `gcc_64`, compila, empacota e faz o boot test da AppImage. Edição, git, lint (inclusive shellcheck), testes, cópia
  para `C:\gits\qgc-dist` e hashes ficam no Windows, e os dois scripts de distribuição são chamados do PowerShell.
- **Linux:** `deploy/beswarm/dist-linux.ps1` (Windows) resolve o commit, chama `deploy/beswarm/build-appimage.sh`
  dentro do WSL e grava o `.sha256` no Windows. O `.sh` mantém um clone Linux cujo `origin` é o repositório Windows,
  compila com o preset `Linux` e gera a AppImage com `cmake --install` (linuxdeploy + appimagetool, baixados pelo próprio
  `cmake/install/CreateAppImage.cmake`). O Windows ganha o equivalente `deploy/beswarm/dist-windows.ps1`, que formaliza o
  que hoje é feito à mão (guia `windows-build-e-migracao.md`, seção 5.1).
- **Painel de frota:** controlador C++ novo, `FleetCommandController` (`QML_ELEMENT`, instanciado pelo painel), que para
  cada veículo armado pede a troca para o modo de voo de RTL/Land **do firmware** (`Vehicle::rtlFlightMode()` /
  `landFlightMode()` + `Vehicle::setFlightMode()`), confirma pelo modo reportado no heartbeat e reenvia até N tentativas
  (Fact `flyViewSettings.fleetCommandAttempts`). Todos os veículos são tratados em paralelo e sem bloquear a UI (não usa
  `FirmwarePlugin::_setFlightModeAndValidate`, que trava a UI com `msleep` e tem 3 tentativas fixas). A UI é
  `FlyViewFleetPanel.qml`, ancorado à direita, logo acima da barra de telemetria/instrumentos. `RTL ALL` envia na hora,
  sem confirmação; `LAND ALL` passa pelo deslizador de confirmação do `GuidedActionsController`, como as ações
  multi-veículo existentes (Arm/Pause).
- **Paralelismo:** Onda 0 (contrato, curta e sequencial) → Onda 1 (4 agentes em paralelo, arquivos disjuntos) →
  Onda 2 (integração, testes e distribuição). O agente Linux (A) não depende do contrato e começa já na Onda 0.

**Tech Stack:** Qt 6.12.0 (`msvc2022_64` no Windows, `gcc_64` no Linux), CMake ≥ 3.28 + Ninja, C++20, QML, QtTest +
MockLink, WSL2 `Ubuntu-26.04` (glibc 2.43, GCC 15.2, CMake 4.2 e GStreamer 1.28 do apt), PowerShell 5.1, Bash,
uv 0.13.7 + prek (lint no Windows).

**Spec:** pedido do usuário nesta sessão (2026-11-20):
(0) manter o modelo atual — código em `C:\gits\qgroundcontrol`, distribuição em `C:\gits\qgc-dist`;
(1) criar binário para Linux além do Windows;
(2) sobre o mapa, no lado direito, seguindo o padrão do que já existe lá, um espaço com
(2.1) lista dos veículos e status do link,
(2.2) botão amarelo que envia RTL para todos os veículos e botão vermelho que envia LAND para todos,
(2.3) número de vezes que se tenta enviar os comandos configurável,
(2.4) o espaço pode ser recolhido e expandido;
(3) plano detalhado para ser executado por outro modelo, com o máximo de atividades em 4 agentes paralelos;
(4) na revisão do plano: Linux na distro WSL `Ubuntu-26.04`, o máximo possível no Windows, `RTL ALL` sem confirmação e
`LAND ALL` com confirmação.
Contexto: `docs/superpowers/plans/2026-11-09-beswarm-branding.md` (branding, já executado) e o guia do build Windows
`docs/superpowers/guides/windows-build-e-migracao.md` (branch `worktree-win-rebuild`, commit `db1bed872`).

## Global Constraints

- Branch base: `feat/beswarm-branding`, cujo HEAD é o commit que adiciona este plano (pai `d134eab1d`). Branch de
  integração: `feat/beswarm-fleet-panel`.
- **Sem** `git push`, PR ou merge em `master` sem autorização explícita do usuário. Commits locais nos branches dos
  agentes são permitidos (a integração depende deles), sempre com `git add <arquivos da task>` (nunca `-A`/`.`).
- Commits em Conventional Commits, terminando com a linha de coautoria do modelo executor.
- Identidade interna inalterada (decisão do branding): executável `QGroundControl` / `QGroundControl.exe`, `QGC_APP_NAME`,
  pastas de configuração. Nome exibido: `BeSwarm Vehicle GS`.
- Artefatos de distribuição (nomes exatos):
  - Windows: pasta `C:\gits\qgc-dist\BeSwarmVehicleGS\`, `C:\gits\qgc-dist\BeSwarmVehicleGS-win64-portable.zip` e
    `C:\gits\qgc-dist\BeSwarmVehicleGS-win64-portable.zip.sha256`.
  - Linux: `C:\gits\qgc-dist\BeSwarmVehicleGS-linux-x86_64.AppImage` e `…AppImage.sha256`.
  - Formato do `.sha256`: `<hash minúsculo><dois espaços><nome do arquivo>` + LF (compatível com `sha256sum -c`).
- Decisões do usuário na revisão (2026-11-20), não reabrir: Linux na distro WSL `Ubuntu-26.04`; o máximo possível no
  Windows; `RTL ALL` envia sem confirmação; `LAND ALL` exige o deslizador de confirmação.
- **Windows primeiro:** o WSL só faz o que exige Linux (pacotes apt, Qt `gcc_64`, compilação, AppImage e o boot test
  dela). Todo o resto roda no Windows: edição, git, lint (shellcheck incluído, via `shellcheck-py` do prek), testes,
  cópia para o `qgc-dist` e hashes (`Get-FileHash`). Nenhum lint nem `ctest` roda no WSL, salvo o fallback de lint da
  Task 2, Step 5.
- Linux compila **só** na distro WSL `Ubuntu-26.04` (padrão do WSL nesta máquina; `Ubuntu 26.04.1 LTS`, glibc 2.43,
  GCC 15.2, CMake 4.2.3 e GStreamer 1.28.2 do apt). Consequência aceita pelo usuário: a AppImage só roda em distros com
  glibc ≥ 2.43 (Ubuntu 26.04 ou mais novo); Ubuntu 24.04 e Debian 13 não a executam. Não trocar de distro sem o usuário.
- O CI upstream compila com GCC 13; erro de compilação que só aparece no GCC 15 é corrigido na fonte, no repositório
  Windows, em commit próprio `fix(build): …` feito pelo coordenador (o arquivo está fora da tabela de posse). O agente A
  para, anota o erro exato e avisa o coordenador.
- Clone Linux em `~/src/qgroundcontrol` (no WSL), `origin` = `/mnt/c/gits/qgroundcontrol`, gerenciado só pelo
  `build-appimage.sh`. Nunca compilar dentro de `/mnt/c` (I/O lento) e nunca editar o clone Linux à mão: toda mudança
  nasce no repositório Windows.
- Tarefas de sistema no WSL rodam como root com `wsl.exe -d Ubuntu-26.04 -u root -- …` (o `sudo` do usuário pede senha).
- Estilo (`CODING_STYLE.md`): 4 espaços, LF, ≤ 120 colunas; C++ com membros `_privados`, chaves sempre, sem `Q_ASSERT`,
  logging categorizado `"Vehicle.FleetCommandController"`; QML sem tamanhos/cores fixos (`ScreenTools`, `QGCPalette`),
  `qsTr()` em todo texto visível, `Connections` com sintaxe de função; testes sem `QTest::qWait(<n>)` nem
  `QTest::ignoreMessage` (usar `QTRY_*_WITH_TIMEOUT`, `QSignalSpy::wait`, `expectLogMessage`/`ignoreLogMessage`).
- Firmware Plugin: nada de `if (px4)`. O modo alvo vem sempre de `Vehicle::rtlFlightMode()`/`landFlightMode()`.
- Unity build: nomes locais de arquivo (namespace anônimo/`static`) não podem repetir nomes de outros `.cc`.
- O preset `Windows` (Release) tem `QGC_BUILD_TESTING=OFF`. Testes rodam no preset `Windows-debug`.
- No Windows desta máquina, `just` não existe e o `uv` 0.13.7 está em `%APPDATA%\Python\Python312\Scripts`, fora do
  `PATH`. O `bash` do `PATH` é o lançador do WSL (`C:\Windows\system32\bash.exe`), e os hooks `check-no-qassert`,
  `check-no-qtest-ignore-message` etc. chamam `bash -c`. Por isso todo lint no Windows usa o bloco **LINT-ENV**.
- Cada chamada de shell do executor é um processo novo: repita o bloco **PS-ENV** em toda chamada que compila.
  Comandos que podem passar de 2 minutos (configure, build, install, ctest completo) rodam em background.

## Decisões a confirmar antes de executar

Os padrões abaixo já estão incorporados nas tasks. Se o usuário escolher a alternativa, ajustar a task indicada.

1. **Veículos desarmados** — padrão: não recebem o comando e aparecem como "ignorado (desarmado)". Alternativa: enviar
   para todos os conectados (Task B1: remover a regra `Skipped` e o teste `_testDisarmedVehicleSkipped`).
2. **Tentativas** — padrão: 3 (faixa 1–20), intervalo fixo de 2 s entre tentativas (`kAttemptTimeoutMs`). Alternativa:
   tornar o intervalo configurável também (novo Fact na Task 1).
3. **RTL no ArduPilot** — padrão: RTL comum (não Smart RTL).
4. **Posição do painel** — padrão: canto inferior direito, acima da barra de telemetria/instrumentos; o painel
   multi-veículo existente (topo direito) encolhe para dar espaço.
5. **Tradução pt_PT** das novas strings — padrão: não incluída (o painel multi-veículo vizinho também está sem tradução
   no `qgc_source_pt_PT.ts`); as strings aparecem em inglês.

## Review Focus

1. **Painel multi-veículo + painel de frota juntos** (≥ 2 veículos; `enableMultiVehiclePanel` vem ligado por padrão),
   janela 1280×720: nenhum cobre a barra de instrumentos nem o outro; o multi-veículo encolhe e rola.
   → D2 `_testFleetPanelLayoutWithMultiVehiclePanel`.
2. **Link caindo no meio**: veículo sem link recebe exatamente N envios e termina "failed after N attempts"; se o link
   volta antes de esgotar, termina "confirmed". → B1 `_testRetriesThenFails`, `_testSucceedsWhenLinkReturns`.
3. **LAND apertado enquanto o RTL ainda tenta**: LAND substitui o RTL para todos, sem nenhum reenvio tardio de RTL.
   → B1 `_testLandOverridesRtl`.
4. **Veículo desconecta durante as tentativas**: sai do status, sem crash, e o comando termina para os demais.
   → B1 `_testVehicleRemovedWhileRetrying`.
5. **Piloto quer retomar o controle durante as tentativas**: "Stop retries" interrompe os reenvios na hora (senão o GCS
   brigaria com o piloto por até N × 2 s). → B1 `_testCancelStopsResending`.

Ruído conhecido e aceito: no ArduPilot a troca de modo usa `MAV_CMD_DO_SET_MODE` com `showError`, então cada tentativa
sem resposta mostra a mensagem padrão do QGC de comando sem resposta (no PX4 é a mensagem `SET_MODE`, sem ACK e sem aviso).

---

## Organização: ondas, agentes e posse de arquivos

```text
Onda 0 (coordenador, ~30 min): Task 0 + Task 1 ─┬─► Onda 1: B1 ∥ C1→C2 ∥ D1→D2 ─┐
                                                 │                                ├─► Onda 2: Task 2 → Task 3 (Windows ∥ Linux) → Task 4
Agente A começa junto com a Onda 0: A1 → A2 ─────┴────────────────────────────────┘
```

| Agente | Onde trabalha | Branch | Build próprio | Tasks |
| --- | --- | --- | --- | --- |
| Coordenador | `C:\gits\qgroundcontrol` | `feat/beswarm-fleet-panel` | `build\Windows` (Release, incremental) | 0, 1, 2, 3, 4 |
| A — Linux | worktree `C:\gits\qgroundcontrol\.claude\worktrees\linux-dist` (edição, git, lint) + WSL `Ubuntu-26.04` (só build) | `feat/linux-dist` | WSL `~/src/qgroundcontrol/build/Linux` | A1, A2 |
| B — Núcleo C++ | worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-core` | `feat/fleet-core` | `build\Windows-debug` (novo, com testes) | B1 |
| C — QML | `C:\gits\qgroundcontrol` (checkout principal, o coordenador fica ocioso na Onda 1) | `feat/beswarm-fleet-panel` | `build\Windows` (Release, incremental) | C1, C2 |
| D — Extras | worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-extras` | `feat/fleet-extras` | nenhum na Onda 1 | D1, D2 |

Posse de arquivos (ninguém edita arquivo de outro agente; por isso a integração não tem conflitos):

| Dono | Arquivos |
| --- | --- |
| Onda 0 | `src/Vehicle/FleetCommandController.h` (novo, API pública congelada), `src/Vehicle/FleetCommandController.cc` (novo, stub), `src/Vehicle/CMakeLists.txt`, `src/Settings/FlyView.SettingsGroup.json`, `src/Settings/FlyViewSettings.h`, `src/Settings/FlyViewSettings.cc` |
| A | `deploy/beswarm/dist-linux.ps1` (novo), `deploy/beswarm/build-appimage.sh` (novo), `deploy/beswarm/README.md` (novo) |
| B | `src/Vehicle/FleetCommandController.cc`, seção `private:` de `src/Vehicle/FleetCommandController.h`, `test/Vehicle/FleetCommandControllerTest.h` e `.cc` (novos), `test/Vehicle/CMakeLists.txt` |
| C | `src/FlyView/FlyViewFleetPanel.qml` (novo), `src/FlyView/CMakeLists.txt`, `src/FlyView/FlyViewWidgetLayer.qml`, `src/FlyView/FlyViewOccluders.qml`, `src/FlyView/GuidedActionsController.qml`, `src/AppSettings/pages/FlyView.SettingsUI.json` |
| D | `deploy/beswarm/dist-windows.ps1` (novo), `test/QmlUITests/FlyViewFleetPanelUITest.h` e `.cc` (novos), `test/QmlUITests/CMakeLists.txt` |

Carga de CPU (16 threads, 48 GB): A usa `-Jobs 6` (build no WSL), B usa `--parallel 8`, C compila incremental. Tempo estimado:
Onda 0 ≈ 30 min; Onda 1 ≈ 1 h 30 (dominada pelo primeiro build Linux e pelo primeiro build `Windows-debug`);
Onda 2 ≈ 1 h.

### Bloco PS-ENV (Windows, toda chamada que compila)

```powershell
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -DevCmdArguments '-arch=x64 -host_arch=x64' -SkipAutomaticLocation | Out-Null
$env:QT_ROOT_DIR      = 'C:\Qt\6.12.0\msvc2022_64'
$env:CPM_SOURCE_CACHE = 'C:\gits\qgroundcontrol\.cache\CPM'   # cache compartilhado pelos worktrees
$env:QGC_PYTHON_ENV   = 'C:\gits\qgroundcontrol\tools\.venv'  # worktrees não têm tools\.venv próprio
```

Para **executar** binários/testes de um diretório de build `$b` (o build não copia as DLLs do Qt e do GStreamer),
acrescentar no mesmo processo:

```powershell
$gst = (Select-String -Path "$b\CMakeCache.txt" -Pattern '^QGC_GST_RESOLVED_ROOT:INTERNAL=(.*)$').Matches[0].Groups[1].Value -replace '/', '\'
$env:Path = "$env:QT_ROOT_DIR\bin;$gst\bin;$env:Path"
```

### Bloco LINT-ENV (Windows, toda chamada de lint)

```powershell
$env:Path = "C:\Program Files\Git\bin;$env:APPDATA\Python\Python312\Scripts;$env:Path"  # bash = Git Bash; uv 0.13.7
$env:UV_PROJECT_ENVIRONMENT = 'C:\gits\qgroundcontrol\tools\.venv'  # worktrees reaproveitam o venv do checkout principal
$env:QT_ROOT_DIR = 'C:\Qt\6.12.0\msvc2022_64'                       # o hook qmllint procura o qmllint do Qt
```

Conferir uma vez por processo: `(Get-Command bash).Source` = `C:\Program Files\Git\bin\bash.exe` e `uv --version` ≥
0.12.8. Comando de lint, na raiz do checkout/worktree:
`uv run --frozen --project tools --group dev prek run --files <arquivos>`. O primeiro uso sincroniza o grupo `dev` no
venv e baixa os ambientes dos hooks (`shellcheck-py` etc.); é normal demorar alguns minutos.

---

## Contrato (criado na Onda 0, usado por todos)

### C++ — `src/Vehicle/FleetCommandController.h` (API pública congelada)

```cpp
#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

Q_DECLARE_LOGGING_CATEGORY(FleetCommandControllerLog)

/// Sends RTL or Land to every connected vehicle and retries until each one reports the target flight mode.
class FleetCommandController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool        busy          READ busy          NOTIFY busyChanged)
    Q_PROPERTY(Command     activeCommand READ activeCommand NOTIFY activeCommandChanged)
    Q_PROPERTY(QVariantMap vehicleStatus READ vehicleStatus NOTIFY vehicleStatusChanged)

public:
    explicit FleetCommandController(QObject* parent = nullptr);
    ~FleetCommandController() override;

    enum class Command { None, RTL, Land };
    Q_ENUM(Command)

    enum class VehicleState { Pending, Succeeded, Failed, Unsupported, Skipped, Cancelled };
    Q_ENUM(VehicleState)

    Q_INVOKABLE void rtlAll();
    Q_INVOKABLE void landAll();
    Q_INVOKABLE void cancel();

    bool busy() const { return _busy; }
    Command activeCommand() const { return _activeCommand; }
    QVariantMap vehicleStatus() const { return _vehicleStatus; }

    static constexpr int kAttemptTimeoutMs = 2000;
    static constexpr int kMinAttempts = 1;
    static constexpr int kMaxAttempts = 20;

signals:
    void busyChanged();
    void activeCommandChanged();
    void vehicleStatusChanged();
    void finished(FleetCommandController::Command command);

private:
    bool _busy = false;
    Command _activeCommand = Command::None;
    QVariantMap _vehicleStatus;
};
```

`vehicleStatus`: chave `QString::number(vehicle->id())` → `QVariantMap` com
`{"state": int(VehicleState), "attempt": int, "maxAttempts": int}`. Contém só os veículos do comando em curso/último.

**Regras de comportamento** (B implementa, C exibe, D testa a UI):

1. `rtlAll()`/`landAll()` iniciam um comando. Se outro estiver em curso, ele é encerrado **sem** `finished` (é
   substituído). `activeCommand` passa a ser o novo comando e só volta a `None` antes do primeiro uso.
2. `maxAttempts = qBound(kMinAttempts, flyViewSettings()->fleetCommandAttempts()->rawValue().toInt(), kMaxAttempts)`,
   lido uma vez por comando. Alterar o Fact no meio não afeta o comando em curso.
3. Fotografia de `MultiVehicleManager::instance()->vehicles()` no início; veículos que chegarem depois são ignorados.
4. Classificação por veículo, nesta ordem: modo alvo vazio → `Unsupported`; `!vehicle->armed()` → `Skipped`;
   `vehicle->flightMode() == alvo` → `Succeeded`; senão `Pending` e a tentativa 1 sai imediatamente. Nos três primeiros
   casos `attempt == 0`.
5. Tentativa: `attempt++`, `vehicle->setFlightMode(alvo)` e janela de `kAttemptTimeoutMs`.
6. `Vehicle::flightModeChanged(mode)` com `mode == alvo` em veículo `Pending` → `Succeeded` na hora.
7. Fim da janela: se já está no alvo → `Succeeded`; se `attempt < maxAttempts` → nova tentativa; senão → `Failed`
   (`qCWarning` com id do veículo, comando e tentativas).
8. `MultiVehicleManager::vehicleRemoved(v)` → a entrada de `v` sai do mapa (sem crash; guardar `QPointer<Vehicle>`).
9. `busy` = existe entrada `Pending`. Quando o comando termina sozinho (último `Pending` resolvido) emite
   `finished(activeCommand)` uma única vez. Comando sem nenhum `Pending` emite `finished` já dentro de `rtlAll()/landAll()`
   e `busy` nunca fica `true`.
10. `cancel()`: para timers e conexões; entradas `Pending` → `Cancelled`; `busy = false`; **não** emite `finished`.
11. Toda mudança de estado/tentativa reconstrói `_vehicleStatus` e emite `vehicleStatusChanged()`.
12. `qCDebug` a cada tentativa (id, comando, `attempt/maxAttempts`).

### Fact — `flyViewSettings.fleetCommandAttempts`

Objeto JSON (inserir em `src/Settings/FlyView.SettingsGroup.json` logo depois do objeto `allowForceArm`, que é o último):

```json
{
    "name": "fleetCommandAttempts",
    "shortDesc": "Number of times the Fleet panel sends RTL or Land to each vehicle before reporting it as failed.",
    "type": "uint32",
    "min": 1,
    "max": 20,
    "default": 3,
    "label": "Fleet RTL/Land attempts",
    "keywords": "fleet,swarm,rtl,land,retry,attempts"
}
```

C++: `DEFINE_SETTINGFACT(fleetCommandAttempts)` em `FlyViewSettings.h` e
`DECLARE_SETTINGSFACT(FlyViewSettings, fleetCommandAttempts)` em `FlyViewSettings.cc`, ambos depois de `allowForceArm`.

### QML — nomes, propriedades e textos exatos

`FlyViewFleetPanel.qml` (módulo `QGroundControl.FlyView`):

- Propriedades públicas: `property real maximumHeight`, `readonly property bool expanded`,
  `readonly property alias controller: fleetController` (o `FleetCommandController` filho, `id: fleetController`).
- Persistência do recolhido/expandido: `QGroundControl.loadBoolGlobalSetting("FlyViewFleetPanelExpanded", true)` na
  criação e `QGroundControl.saveBoolGlobalSetting("FlyViewFleetPanelExpanded", <valor>)` a cada troca (mesmo padrão do
  `PipView.qml`).
- `objectName`s: `flyViewFleetPanel` (raiz), `fleetPanelHeader` (linha clicável do título), `fleetPanelToggle` (ícone),
  `fleetVehicleList` (lista), e por veículo `fleetVehicleRow_<id>`, `fleetLinkDot_<id>`, `fleetLinkText_<id>`,
  `fleetCommandStatus_<id>`; botões `fleetRtlAllButton`, `fleetLandAllButton`, `fleetStopButton`; resumo `fleetSummary`.
- No `FlyViewWidgetLayer.qml`, a instância do painel multi-veículo existente ganha `objectName: "flyViewTopRightPanel"`.

Textos (todos com `qsTr`; `%1` = nome do comando: `qsTr("RTL")` ou `qsTr("Land")`):

| Onde | Texto |
| --- | --- |
| Título | `"Fleet (%1)"` (%1 = nº de veículos) e, se algum sem link, `"%1 lost"` ao lado, em `qgcPal.colorRed` |
| Botões | `"RTL ALL"`, `"LAND ALL"`, `"Stop retries"` |
| Link | `"%1 · Loss %2%"` (nome do link primário, `mavlinkLossPercent` sem decimais); sem link: `"%1 · Comm Lost"`; sem nome de link: `"No link"` |
| Status `Pending` | `"%1: attempt %2 of %3"` |
| Status `Succeeded` | `"%1: confirmed"` |
| Status `Failed` | `"%1: failed after %2 attempts"` |
| Status `Unsupported` | `"%1: not supported"` |
| Status `Skipped` | `"%1: skipped (disarmed)"` |
| Status `Cancelled` | `"%1: stopped"` |
| Resumo | `"%1: %2 of %3 confirmed, %4 failed"` (confirmados, total de entradas, `Failed`) |

Botões (decisão do usuário):

- `fleetRtlAllButton` chama `fleetController.rtlAll()` **direto**, sem deslizador e sem passar pelo
  `GuidedActionsController`.
- `fleetLandAllButton` abre o deslizador de confirmação. Só ao confirmar o `GuidedActionsController` chama
  `landAll()`.

`GuidedActionsController.qml` (só a ação de Land):

| Nome | Valor |
| --- | --- |
| `actionFleetLand` | `31` |
| `fleetLandTitle` | `qsTr("Fleet Land")` |
| `fleetLandMessage` | `qsTr("Land ALL connected vehicles at their current position.")` |

O `actionData` da ação é o `FleetCommandController` do painel. O `executeAction` chama `actionData.landAll()`, com
checagem de nulo.

### Scripts de distribuição

```text
deploy/beswarm/dist-windows.ps1 [-RepoDir <dir>] [-BuildDir <dir>] [-DistDir <dir>] [-Name <nome>] [-QtRoot <dir>] [-SkipBuild]
  padrões: RepoDir = raiz do repo do script; BuildDir = <RepoDir>\build\Windows; DistDir = C:\gits\qgc-dist;
           Name = BeSwarmVehicleGS; QtRoot = C:\Qt\6.12.0\msvc2022_64

deploy/beswarm/dist-linux.ps1 [-Ref <git-ref>] [-DistDir <dir>] [-Name <nome>] [-Jobs <n>] [-Distro <nome>] [-SkipBuild]
  padrões: Ref = HEAD do checkout/worktree onde está o script; DistDir = C:\gits\qgc-dist; Name = BeSwarmVehicleGS;
           Jobs = [Environment]::ProcessorCount - 2; Distro = Ubuntu-26.04
  artefato: <DistDir>\<Name>-linux-x86_64.AppImage (+ .sha256 gravado pelo PowerShell)

deploy/beswarm/build-appimage.sh --origin <dir> --sha <commit> --out <arquivo> [--src <dir>] [--jobs <n>] [--skip-build]
  roda dentro do WSL e só é chamado pelo dist-linux.ps1 (caminhos já no formato /mnt/c/...);
  padrões: --src ~/src/qgroundcontrol; --jobs $(nproc) - 2
```

Os dois `.ps1` terminam com código ≠ 0 em qualquer falha e imprimem caminho, tamanho e SHA-256 do artefato. O `.sh`
termina com código ≠ 0 em qualquer falha.

Nos `.ps1`, todo comando nativo (git, cmake, wsl) passa por uma função `Invoke-Native([scriptblock]$Command)`, que:
- usa `$ErrorActionPreference = 'Continue'` só dentro dela;
- executa `& $Command`;
- dá `throw` se `$LASTEXITCODE` ≠ 0.

Motivo: no PowerShell 5.1 com `Stop`, o progresso que git/cmake escrevem no stderr vira erro fatal quando a saída está
redirecionada (background, agentes).

---

## Onda 0 — coordenador

### Task 0: Branch e worktrees

- [ ] **Step 1: Conferir estado** — em `C:\gits\qgroundcontrol`: `git status --short` vazio e
  `git rev-parse --abbrev-ref HEAD` = `feat/beswarm-branding`.
- [ ] **Step 2: Worktree do agente A (libera A imediatamente)** —
  `git worktree add .claude/worktrees/linux-dist -b feat/linux-dist feat/beswarm-branding`. Disparar o agente A (A1).
- [ ] **Step 3: Branch de integração** — `git switch -c feat/beswarm-fleet-panel`.
- [ ] **Step 4 (depois da Task 1):** worktrees de B e D a partir do commit do contrato:
  `git worktree add .claude/worktrees/fleet-core -b feat/fleet-core feat/beswarm-fleet-panel` e
  `git worktree add .claude/worktrees/fleet-extras -b feat/fleet-extras feat/beswarm-fleet-panel`.
  Disparar B, C e D (C fica no checkout principal).

### Task 1: Contrato — stub do controlador e Fact de tentativas

**Files:**

- Create: `src/Vehicle/FleetCommandController.h` (exatamente o bloco do Contrato)
- Create: `src/Vehicle/FleetCommandController.cc` (stub)
- Modify: `src/Vehicle/CMakeLists.txt` (lista `target_sources`, ordem alfabética sem distinção de caixa: antes de `FTPController.cc`)
- Modify: `src/Settings/FlyView.SettingsGroup.json`, `src/Settings/FlyViewSettings.h`, `src/Settings/FlyViewSettings.cc`

**Interfaces:**

- Produces: tipo QML `FleetCommandController` (disponível com `import QGroundControl`, como `MavlinkActionManager`),
  Fact `QGroundControl.settingsManager.flyViewSettings.fleetCommandAttempts`, C++
  `SettingsManager::instance()->flyViewSettings()->fleetCommandAttempts()`.

- [ ] **Step 1: Stub** — `.cc` com `QGC_LOGGING_CATEGORY(FleetCommandControllerLog, "Vehicle.FleetCommandController")`,
  construtor (`: QObject(parent)`), destrutor `= default` no `.cc`, e `rtlAll()`, `landAll()`, `cancel()` só com
  `qCDebug(FleetCommandControllerLog) << "not implemented";`. Includes: `FleetCommandController.h`, `QGCLoggingCategory.h`.
- [ ] **Step 2: Fact** — aplicar o JSON e as duas macros do Contrato.
- [ ] **Step 3: Build** — PS-ENV + `cmake --build C:\gits\qgroundcontrol\build\Windows` (background). Esperado: código 0.
  Conferir o registro QML: `Select-String -Path C:\gits\qgroundcontrol\build\Windows\*qmltyperegistrations*.cpp -Pattern FleetCommandController`
  encontra ao menos uma linha (se o arquivo estiver em subpasta, usar `Get-ChildItem -Recurse -Filter *qmltyperegistrations*.cpp`).
- [ ] **Step 4: Commit** —
  `git add src/Vehicle/FleetCommandController.h src/Vehicle/FleetCommandController.cc src/Vehicle/CMakeLists.txt src/Settings/FlyView.SettingsGroup.json src/Settings/FlyViewSettings.h src/Settings/FlyViewSettings.cc`;
  mensagem `feat(fleet): add fleet command controller interface and attempts setting`.
- [ ] **Step 5:** executar Task 0, Step 4.

---

## Onda 1 — quatro agentes em paralelo

### Task A1 (agente A): o mínimo de Linux no WSL `Ubuntu-26.04`

Sem commit. Só o que precisa existir dentro do Linux para compilar: pacotes, clone, uv/venv e Qt `gcc_64`. Todos os
comandos são disparados do Windows: os de usuário com `wsl.exe -d Ubuntu-26.04 -- bash -lc '<cmd>'`, os de root com
`wsl.exe -d Ubuntu-26.04 -u root -- bash -lc '<cmd>'`. No Git Bash do executor, prefixar com `MSYS_NO_PATHCONV=1`
para os caminhos `/mnt/...` não serem convertidos. Comando com aspas duplas internas vai num script no scratchpad,
executado com `wsl.exe -d Ubuntu-26.04 -- bash /mnt/c/.../script.sh`, porque o PowerShell 5.1 estraga essas aspas.

- [ ] **Step 1: Conferir a distro** — `lsb_release -ds; ldd --version | head -1; gcc --version | head -1` →
  `Ubuntu 26.04.1 LTS`, glibc `2.43`, GCC `15.2.0`.
- [ ] **Step 2: Dependências de sistema (root)** —
  `cd /mnt/c/gits/qgroundcontrol && python3 tools/setup/install_dependencies --platform debian && apt-get install -y curl`.
  Esperado: código 0. Diferenças do 26.04 já conferidas, ambas normais:
  - O apt escolhe `libfuse2t64` no lugar de `libfuse2` ("Note, selecting…").
  - O instalador descarta `libgstreamer-plugins-good1.0-dev`, que é opcional e não existe no 26.04.

  Depois, como usuário:
  - `cmake --version` → 4.2.x (≥ 3.28).
  - `ninja --version` → 1.13.x.
  - `pkg-config --modversion gstreamer-1.0` → 1.28.x.
  - `dpkg -s libfuse2t64 | grep -c 'Status: install ok installed'` → 1.

  `shellcheck` não é instalado aqui: ele roda no Windows, pelo prek.
- [ ] **Step 3: Clone Linux (usuário)** —
  `git config --global --add safe.directory /mnt/c/gits/qgroundcontrol && mkdir -p ~/src && git clone /mnt/c/gits/qgroundcontrol ~/src/qgroundcontrol`.
  Esperado: `git -C ~/src/qgroundcontrol tag | wc -l` > 0 (a versão do app vem de `git describe --tags`).
- [ ] **Step 4: uv (usuário)** — `cd ~/src/qgroundcontrol && python3 tools/setup/install_uv.py`; garantir
  `export PATH="$HOME/.local/bin:$PATH"` no `~/.profile` (acrescentar só se faltar). Esperado: `uv --version` funciona
  num `bash -lc` novo.
- [ ] **Step 5: venv de ferramentas (usuário)** — `cd ~/src/qgroundcontrol && python3 tools/setup/install_python.py scripts`.
  Esperado: `~/src/qgroundcontrol/tools/.venv/bin/python` existe. O Python do sistema é o 3.14, mas o
  `tools/.python-version` fixa o 3.10, que o uv baixa sozinho.
- [ ] **Step 6: Qt 6.12.0 gcc_64 (usuário, background)** —
  `cd ~/src/qgroundcontrol && python3 tools/setup/install_qt.py install --from-config --host linux --arch linux_gcc_64 --outdir ~/Qt`.
  Esperado: `~/Qt/6.12.0/gcc_64/lib/cmake/Qt6/qt.toolchain.cmake` existe.

### Task A2 (agente A): `dist-linux.ps1` + `build-appimage.sh`, README e primeira AppImage

**Files:**

- Create: `deploy/beswarm/dist-linux.ps1` (Windows; ponto de entrada)
- Create: `deploy/beswarm/build-appimage.sh` (WSL; shebang `#!/usr/bin/env bash` e bit executável no git)
- Create: `deploy/beswarm/README.md`

**Interfaces:**

- Consumes: ambiente da Task A1; interface do `dist-windows.ps1` (seção Contrato) para documentar no README.
- Produces: `dist-linux.ps1` e `build-appimage.sh` com as interfaces do Contrato; `dist-linux.ps1` é usado na Task 3.

- [ ] **Step 1: `dist-linux.ps1`** — no worktree `linux-dist`. `$ErrorActionPreference = 'Stop'`; parâmetros do
  Contrato; comandos nativos via `Invoke-Native` (seção Contrato). Sequência exata:
  1. `$sha = git -C $PSScriptRoot rev-parse --verify "$Ref^{commit}"` (git do Windows; `HEAD` é o do worktree do script).
  2. `$repoDir`: pai de `git -C $PSScriptRoot rev-parse --path-format=absolute --git-common-dir`. Isso dá o
     repositório principal (`C:/gits/qgroundcontrol`) mesmo rodando de um worktree, e lá estão os commits de todos os
     branches.
  3. Função `ConvertTo-WslPath([string]$Path)`: caminho absoluto do Windows → `/mnt/<letra minúscula>/<resto com />`.
     Rejeitar (com `throw`) caminho que não comece com letra de unidade.
  4. Criar `$DistDir` se faltar. Definir `$out = Join-Path $DistDir "$Name-linux-x86_64.AppImage"` e apagar `$out` e
     `"$out.sha256"`, se existirem.
  5. Chamar o WSL com um argumento por item, sem `bash -lc` (evita o problema de aspas do PowerShell 5.1):
     `& wsl.exe -d $Distro -- bash -l <wsl de "$PSScriptRoot\build-appimage.sh"> --origin <wsl de $repoDir> --sha $sha --jobs $Jobs --out <wsl de $out>`
     (+ `--skip-build` se pedido). O `-l` carrega o `~/.profile` (PATH do uv). Se `$LASTEXITCODE` ≠ 0, `throw`.
  6. `Test-Path $out`, senão `throw`. Gravar o `.sha256` igual ao do Windows:
     `[IO.File]::WriteAllText("$out.sha256", "$hash  $Name-linux-x86_64.AppImage`n")`, com `$hash` minúsculo de
     `Get-FileHash -Algorithm SHA256`.
  7. Resumo: SHA do commit, caminho, tamanho em MB, hash.
- [ ] **Step 2: `build-appimage.sh`** — `set -euo pipefail`; parse dos argumentos do Contrato (`--origin`, `--sha` e
  `--out` obrigatórios). Sequência exata:
  1. `safe.directory`: `git config --global --get-all safe.directory | grep -qxF "$ORIGIN"`; se não achar,
     `git config --global --add safe.directory "$ORIGIN"`.
  2. Clone: se `$SRC/.git` não existe, `git clone "$ORIGIN" "$SRC"`. Depois
     `git -C "$SRC" fetch --force --tags origin '+refs/heads/*:refs/remotes/origin/*'`. Conferir com
     `git -C "$SRC" cat-file -e "$SHA^{commit}"`; se falhar, abortar com "commit $SHA não está em nenhum branch do
     repositório Windows". Por fim, `git -C "$SRC" checkout --force --detach "$SHA"`.
  3. Ambiente: `export QT_ROOT_DIR="$HOME/Qt/6.12.0/gcc_64" CPM_SOURCE_CACHE="$HOME/.cache/qgc-cpm" APPIMAGE_EXTRACT_AND_RUN=1`
     e `PATH="$HOME/.local/bin:$PATH"`.
     - Se `$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake` não existir, abortar dizendo para rodar a Task A1 / o README.
     - `APPIMAGE_EXTRACT_AND_RUN=1` porque o WSL nem sempre monta FUSE. O `CreateAppImage.cmake` já remove essa
       variável onde precisa.
  4. Sem `--skip-build`: se `$SRC/build/Linux/CMakeCache.txt` não existe, `cmake --preset Linux` (em `$SRC`); depois
     `cmake --build --preset Linux --parallel "$JOBS"`.
  5. Boot test do binário: `QT_QPA_PLATFORM=offscreen "$SRC/build/Linux/Release/QGroundControl" --simple-boot-test --allow-multiple`
     (confirmar o caminho do binário no primeiro build; o CI usa `<build>/Release/QGroundControl`).
  6. `rm -f "$SRC/build/Linux/QGroundControl-x86_64.AppImage"` e `cmake --install "$SRC/build/Linux" --config Release`.
     Esperado: `$SRC/build/Linux/QGroundControl-x86_64.AppImage`.
  7. Smoke da AppImage, ainda no ext4: `QT_QPA_PLATFORM=offscreen <AppImage> --simple-boot-test --allow-multiple` → 0.
  8. `cp` da AppImage para `$OUT`; imprimir `$SHA` e `$OUT`. O hash fica com o PowerShell.
- [ ] **Step 3: README** — `deploy/beswarm/README.md` (pt-BR, máx. 120 colunas), com estas seções:
  - **Windows:** pré-requisitos = guia `windows-build-e-migracao.md`; comando
    `powershell -ExecutionPolicy Bypass -File deploy\beswarm\dist-windows.ps1`; parâmetros.
  - **Linux:** preparação única do WSL `Ubuntu-26.04` (os comandos da Task A1, todos disparados do Windows); comando
    `powershell -ExecutionPolicy Bypass -File deploy\beswarm\dist-linux.ps1`; parâmetros.
  - **Rodar a AppImage:**
    - Exige glibc ≥ 2.43 (Ubuntu 26.04 ou mais novo).
    - Antes, `chmod +x`.
    - Sem FUSE: `--appimage-extract-and-run`.
    - Teste rápido no próprio Windows via WSLg:
      `wsl.exe -d Ubuntu-26.04 -- /mnt/c/gits/qgc-dist/BeSwarmVehicleGS-linux-x86_64.AppImage --appimage-extract-and-run`.
  - **Licenças:** a AppImage leva o `gst-libav` do Ubuntu, que puxa x264/x265 GPL (mesmo aviso do CI upstream).
- [ ] **Step 4: Checagem estática (Windows)** — no worktree:
  1. `git add deploy/beswarm/dist-linux.ps1 deploy/beswarm/build-appimage.sh deploy/beswarm/README.md` e
     `git update-index --chmod=+x deploy/beswarm/build-appimage.sh`. O hook `check-shebang-scripts-are-executable`
     lê o modo do índice, por isso o `add` vem antes do lint.
  2. `& 'C:\Program Files\Git\bin\bash.exe' -n deploy/beswarm/build-appimage.sh` → código 0.
  3. `$e = $null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path deploy\beswarm\dist-linux.ps1).Path, [ref]$null, [ref]$e); $e.Count`
     → `0`.
  4. LINT-ENV + `uv run --frozen --project tools --group dev prek run --files deploy/beswarm/dist-linux.ps1 deploy/beswarm/build-appimage.sh deploy/beswarm/README.md`
     → todos os hooks `Passed`/`Skipped` (o `shellcheck` precisa aparecer como `Passed`).
- [ ] **Step 5: Primeira AppImage (do branding atual; background, ~40 min no primeiro build)** —
  `powershell -ExecutionPolicy Bypass -File C:\gits\qgroundcontrol\.claude\worktrees\linux-dist\deploy\beswarm\dist-linux.ps1 -Ref feat/beswarm-branding -Jobs 6`.
  Esperado:
  - Código 0.
  - `C:\gits\qgc-dist\BeSwarmVehicleGS-linux-x86_64.AppImage` (~150–250 MB) e o `.sha256`.
  - `(Get-FileHash C:\gits\qgc-dist\BeSwarmVehicleGS-linux-x86_64.AppImage).Hash.ToLower()` igual ao 1º campo do
    `.sha256`.

  Isto já entrega o item (1) do pedido com a versão atual. Se o build falhar por erro exclusivo do GCC 15, seguir a regra
  das Global Constraints (parar e avisar o coordenador).
- [ ] **Step 6: Commit** — conferir `git ls-files -s deploy/beswarm/build-appimage.sh` (modo `100755`); mensagem
  `build(linux): add BeSwarm AppImage distribution scripts`.

### Task B1 (agente B): lógica do `FleetCommandController` e testes

**Files:**

- Modify: `src/Vehicle/FleetCommandController.cc`; seção `private:` de `src/Vehicle/FleetCommandController.h`
  (a parte pública é o Contrato e **não muda**)
- Create: `test/Vehicle/FleetCommandControllerTest.h`, `test/Vehicle/FleetCommandControllerTest.cc`
- Modify: `test/Vehicle/CMakeLists.txt` (`target_sources(QGCUnitTests …)` +
  `add_qgc_test(FleetCommandControllerTest LABELS Integration Vehicle TIMEOUT 240)`)

**Interfaces:**

- Consumes: `MultiVehicleManager::instance()` (`vehicles()`, sinal `vehicleRemoved(Vehicle*)`),
  `Vehicle::{id, armed, flightMode, setFlightMode, rtlFlightMode, landFlightMode}`, sinal
  `Vehicle::flightModeChanged(const QString&)`, `SettingsManager::instance()->flyViewSettings()->fleetCommandAttempts()`.
- Produces: comportamento das Regras 1–12 do Contrato.

Base de teste: `UnitTest` com MockLinks gerenciados pelo próprio teste (o `VehicleTest` só conecta um veículo).
Membros/helpers da classe de teste (sem namespace anônimo, por causa do unity build):

- `init()`: `UnitTest::init()`; `MultiVehicleManager::instance()->init()`; salvar o valor do Fact de tentativas.
- `Vehicle* _connectVehicle(MockLink* link, bool arm)`: recebe um link já iniciado (`MockLink::startPX4MockLink()`,
  `startAPMArduCopterMockLink()` ou `startAPMArduSubMockLink()`; para os ArduPilot, `QSKIP` antes se
  `!apmFirmwareSupported()`), guarda o `QPointer<MockLink>` em `_links`, espera o veículo daquele link aparecer em
  `vehicles()` (`vehicleAdded`) e `isInitialConnectComplete()` (`TestTimeout::longMs()`), e se `arm` chama
  `vehicle->setArmed(true, false)` e espera `armed()`.
- `cleanup()`: `disconnect()` em cada link, `QTRY_COMPARE_WITH_TIMEOUT(MultiVehicleManager::instance()->vehicles()->count(), 0, TestTimeout::longMs())`,
  restaurar o Fact, `UnitTest::cleanup()`.
- `int _state(const FleetCommandController&, const Vehicle*)` e `int _attempt(...)` lendo `vehicleStatus()`.

Mensagens esperadas com link mudo (ArduPilot "did not respond", perda de comunicação): suprimir com
`ignoreLogMessage(<categoria>, <nível>, <regex>)` usando a categoria e o texto que aparecerem no log da primeira
execução — nunca `QTest::ignoreMessage`.

- [ ] **Step 1: Escrever os testes** (todos falham contra o stub):

  ```cpp
  // _data: rows "PX4 RTL", "PX4 Land", "ArduCopter RTL", "ArduCopter Land" (MAV_AUTOPILOT autopilot, bool land)
  void _testCommandAllSucceeds();
  //   2 veículos armados; land ? landAll() : rtlAll()
  //   QTRY_COMPARE_WITH_TIMEOUT(_state(c, v), int(VehicleState::Succeeded), TestTimeout::longMs()) para os 2
  //   QCOMPARE(v->flightMode(), land ? v->landFlightMode() : v->rtlFlightMode())
  //   QCOMPARE(c.vehicleStatus().size(), 2); QVERIFY(!c.busy())
  //   QCOMPARE(finishedSpy.count(), 1); QCOMPARE(finishedSpy[0][0].value<Command>(), land ? Command::Land : Command::RTL)

  void _testDisarmedVehicleSkipped();
  //   PX4 v1 armado, v2 desarmado; rtlAll()
  //   v2: state Skipped, attempt 0, flightMode inalterado; v1: Succeeded

  void _testUnsupportedVehicle();
  //   MockLink::startAPMArduSubMockLink(), armado (ArduSub não tem modo RTL); rtlAll()
  //   imediatamente: state Unsupported, attempt 0, !busy(), finishedSpy.count() == 1,
  //   mockLink->receivedMavCommandCount(MAV_CMD_DO_SET_MODE) == 0

  void _testRetriesThenFails();
  //   Fact = 2; ArduCopter armado; clearReceivedMavCommandCounts(); setCommLost(true); landAll()
  //   QTRY state Failed (timeout 3 * kAttemptTimeoutMs + TestTimeout::shortMs())
  //   attempt == 2, maxAttempts == 2, receivedMavCommandCount(MAV_CMD_DO_SET_MODE) == 2, !busy(), finished 1x

  void _testSucceedsWhenLinkReturns();
  //   Fact = 5; PX4 armado; setCommLost(true); rtlAll(); QTRY attempt >= 2; setCommLost(false)
  //   QTRY state Succeeded; attempt <= 5; flightMode == rtlFlightMode

  void _testLandOverridesRtl();
  //   Fact = 5; ArduCopter armado; setCommLost(true); rtlAll(); QTRY attempt == 1; landAll()
  //   QCOMPARE(activeCommand, Land); setCommLost(false); QTRY state Succeeded e flightMode == landFlightMode
  //   QSignalSpy no flightModeChanged; QVERIFY_NO_SIGNAL_WAIT(spy, 2 * kAttemptTimeoutMs)  // nenhum RTL tardio
  //   finishedSpy: exatamente 1 emissão, com Land

  void _testVehicleRemovedWhileRetrying();
  //   2 PX4 armados; link de v2 com setCommLost(true); rtlAll(); QTRY attempt(v2) >= 1
  //   _links[1]->disconnect(); QTRY vehicles()->count() == 1; QTRY vehicleStatus() sem a chave de v2
  //   v1 Succeeded; QTRY !busy(); finished 1x

  void _testCancelStopsResending();
  //   Fact = 5; ArduCopter armado; setCommLost(true); landAll(); QTRY attempt == 1
  //   clearReceivedMavCommandCounts(); cancel()
  //   state Cancelled; !busy(); finishedSpy.count() == 0
  //   statusSpy criado DEPOIS do cancel() (o próprio cancel emite vehicleStatusChanged)
  //   QVERIFY_NO_SIGNAL_WAIT(statusSpy, 2 * kAttemptTimeoutMs); receivedMavCommandCount(MAV_CMD_DO_SET_MODE) == 0
  ```

- [ ] **Step 2: Build Debug com testes (background, ~25 min no primeiro)** — PS-ENV +
  `Set-Location C:\gits\qgroundcontrol\.claude\worktrees\fleet-core; cmake --preset Windows-debug; cmake --build --preset Windows-debug --parallel 8`.
  Esperado: código 0. Confirmar o executável em `build\Windows-debug\Debug\QGroundControl.exe` (ou onde o build o puser).
- [ ] **Step 3: Rodar e ver falhar** — PS-ENV + bloco de execução com `$b = '…\fleet-core\build\Windows-debug'` +
  `ctest --test-dir $b -R FleetCommandControllerTest --output-on-failure`. Esperado: FAIL (stub não envia nada).
- [ ] **Step 4: Implementar as Regras 1–12** em `FleetCommandController.cc`. Estrutura livre na seção privada; sugestão:
  `struct VehicleEntry { QPointer<Vehicle> vehicle; QString targetMode; VehicleState state; int attempt; QTimer* timer; QMetaObject::Connection modeConnection; };`
  em `QHash<quint32, VehicleEntry>`, um `QTimer` single-shot por veículo (filho do controlador), conexão ao
  `vehicleRemoved` feita no construtor, e funções privadas `_start(Command)`, `_sendAttempt(quint32 id)`,
  `_attemptTimedOut(quint32 id)`, `_resolve(quint32 id, VehicleState)`, `_publish()`, `_updateBusy()`, `_clear()`.
  Destrutor: parar timers e desconectar (sem callbacks após destruição).
- [ ] **Step 5: Rodar e ver passar** — mesmo comando do Step 3. Esperado: todos PASS. Regressão:
  `ctest --test-dir $b -R "MultiVehicleManagerTest|FirmwarePluginTest" --output-on-failure` → PASS.
- [ ] **Step 6: Commit** —
  `git add src/Vehicle/FleetCommandController.h src/Vehicle/FleetCommandController.cc test/Vehicle/FleetCommandControllerTest.h test/Vehicle/FleetCommandControllerTest.cc test/Vehicle/CMakeLists.txt`;
  mensagem `feat(fleet): send RTL or Land to every vehicle with configurable retries`.

### Task C1 (agente C): painel QML e integração no Fly View

**Files:**

- Create: `src/FlyView/FlyViewFleetPanel.qml`
- Modify: `src/FlyView/CMakeLists.txt` (`QML_FILES`: `FlyViewFleetPanel.qml` logo após `FlyViewCustomLayer.qml`)
- Modify: `src/FlyView/FlyViewWidgetLayer.qml`, `src/FlyView/FlyViewOccluders.qml`

**Interfaces:**

- Consumes: tipo `FleetCommandController` (Contrato; na Onda 1 é o stub, então os botões não fazem nada ainda),
  `QGroundControl.multiVehicleManager.vehicles`, `vehicle.vehicleLinkManager.{communicationLost, primaryLinkName}`,
  `vehicle.mavlinkLossPercent`, `vehicle.flightMode`, `vehicle.armed`.
- Produces: `objectName`s, propriedades e textos da seção "QML" do Contrato.

- [ ] **Step 1: `FlyViewFleetPanel.qml`** — ordem de declaração do `CODING_STYLE.md`. Estrutura:
  - Raiz `Item` (`objectName: "flyViewFleetPanel"`), `width: ScreenTools.defaultFontPixelWidth * 36`,
    `height: Math.min(maximumHeight, column.implicitHeight + _margin * 2)` com
    `readonly property real _margin: ScreenTools.defaultFontPixelWidth / 2` (margem do `ColumnLayout` em todos os lados),
    `visible: !QGroundControl.videoManager.fullScreen && QGroundControl.multiVehicleManager.vehicles.count > 0`.
  - Fundo igual ao `TelemetryValuesBar.qml`: `Rectangle { anchors.fill: parent; color: qgcPal.window; radius: ScreenTools.defaultFontPixelWidth / 2; opacity: 0.75 }`
    + `DeadMouseArea` para cliques não irem ao mapa (como `FlyViewTopRightPanel.qml`).
  - `ColumnLayout`:
    1. Cabeçalho (`fleetPanelHeader`, clicável inteiro via `QGCMouseArea` → `_setExpanded(!expanded)`): título
       `"Fleet (%1)"`, contador `"%1 lost"` quando houver veículo com `communicationLost`, e `QGCColoredImage`
       (`fleetPanelToggle`) com `/res/chevron-double-down.svg` quando expandido e `/res/chevron-double-up.svg` quando
       recolhido, cor `qgcPal.textPrimary`, lado `ScreenTools.defaultFontPixelHeight`.
    2. Só quando `expanded`: `QGCListView` (`fleetVehicleList`, `clip: true`, altura
       `Math.min(contentHeight, ScreenTools.defaultFontPixelHeight * 12)`), modelo `vehicles`. Delegate
       (`fleetVehicleRow_<id>`, fundo `qgcPal.windowCard`, raio pequeno): círculo de link (`fleetLinkDot_<id>`,
       `qgcPal.colorGreen` com link, `qgcPal.colorRed` sem), `qsTr("Vehicle %1").arg(id)`, modo de voo e
       `qsTr("Armed")`/`qsTr("Disarmed")`, texto do link (`fleetLinkText_<id>`), status do comando
       (`fleetCommandStatus_<id>`, visível só se houver entrada em `controller.vehicleStatus`; cores: `Pending`
       `qgcPal.warningText`, `Succeeded` `qgcPal.successText`, `Failed` `qgcPal.errorText`, demais
       `qgcPal.textSecondary`). Clique na linha: `QGroundControl.multiVehicleManager.activeVehicle = vehicle`.
    3. Só quando `expanded`: `RowLayout` com `QGCButton` `fleetRtlAllButton` (`"RTL ALL"`,
       `backgroundColor: qgcPal.colorYellow`, `textColor: qgcPal.mapWidgetBorderDark`) e `fleetLandAllButton`
       (`"LAND ALL"`, `backgroundColor: qgcPal.colorRed`, `textColor: qgcPal.mapWidgetBorderLight`), ambos
       `Layout.fillWidth: true`. Cores de sinalização são fixas de propósito (amarelo/vermelho legíveis em qualquer tema).
       O `onClicked` entra na Task C2.
    4. `fleetStopButton` (`"Stop retries"`), visível só com `controller.busy`, `onClicked: controller.cancel()`.
    5. `fleetSummary`, visível quando `controller.activeCommand !== FleetCommandController.Command.None`, com o
       texto de resumo do Contrato.
  - `FleetCommandController { id: fleetController }`.
  - Funções privadas: `_commandName()`, `_statusText(entry)`, `_statusColor(entry)`, `_setExpanded(value)`;
    comparar estados com `FleetCommandController.VehicleState.<Valor>`. Se o `qmlcachegen` não resolver o enum
    qualificado, usar a forma curta `FleetCommandController.<Valor>` e registrar isso no commit.
  - `Component.onCompleted`: carregar `FlyViewFleetPanelExpanded`.
- [ ] **Step 2: Encaixe no `FlyViewWidgetLayer.qml`** — logo após `FlyViewBottomRightRowLayout { … }`:
  instância `FlyViewFleetPanel { id: fleetPanel; anchors.right: parent.right; anchors.bottom: bottomRightRowLayout.top; anchors.bottomMargin: _margins * 2; maximumHeight: parent.height * 0.45 }`.
  O `maximumHeight` **não** pode depender de `y` nem do painel do topo (evita laço de bindings).
  - `topRightPanel`: `objectName: "flyViewTopRightPanel"` e
    `maximumHeight: parent.height - (bottomRightRowLayout.height + (fleetPanel.visible ? fleetPanel.height + _margins * 2 : 0) + _margins * 4)`.
  - Occluder: `fleetPanel: fleetPanel.visible ? Qt.rect(fleetPanel.x, fleetPanel.y, fleetPanel.width, fleetPanel.height) : Qt.rect(_root.width, _root.height, 0, 0)`.
  - Não usar `_layoutSpacing` (é `bool` no arquivo atual); usar `_margins`.
- [ ] **Step 3: `FlyViewOccluders.qml`** — `property rect fleetPanel` e `"fleetPanel"` em `types`, logo após `"bottomRight"`.
- [ ] **Step 4: Build** — PS-ENV + `cmake --build C:\gits\qgroundcontrol\build\Windows` (background). Esperado: código 0
  e nenhum aviso do `qmlcachegen`/`qmllint` citando `FlyViewFleetPanel.qml`, `FlyViewWidgetLayer.qml` ou
  `FlyViewOccluders.qml` (filtrar o log com `Select-String`).
- [ ] **Step 5: Commit** —
  `git add src/FlyView/FlyViewFleetPanel.qml src/FlyView/CMakeLists.txt src/FlyView/FlyViewWidgetLayer.qml src/FlyView/FlyViewOccluders.qml`;
  mensagem `feat(flyview): add collapsible fleet panel with vehicle link status`.

### Task C2 (agente C): ação dos botões (RTL direto, LAND confirmado) e página de configurações

**Files:**

- Modify: `src/FlyView/GuidedActionsController.qml`, `src/FlyView/FlyViewFleetPanel.qml` (só os `onClicked`),
  `src/AppSettings/pages/FlyView.SettingsUI.json`

**Interfaces:**

- Consumes: `confirmAction(actionCode, actionData, mapIndicator)` / `executeAction(actionCode, actionData, …)` existentes;
  `globals.guidedControllerFlyView`.
- Produces: `actionFleetLand` e textos da tabela do Contrato.

- [ ] **Step 1: `GuidedActionsController.qml`** (só Land; RTL não passa por aqui):
  - `fleetLandTitle` e `fleetLandMessage` junto dos outros `readonly property string`.
  - `actionFleetLand: 31` logo após `actionChangeLoiterRadius: 30`.
  - Em `confirmAction`, um caso que só define `confirmDialog.title`/`message` (sem `hideTrigger`).
  - Em `executeAction`, `case actionFleetLand:` → `if (actionData) { actionData.landAll() }`.
- [ ] **Step 2: Página de configurações** — em `FlyView.SettingsUI.json`, novo grupo logo após "Guided Commands":
  `{"heading": "Fleet Panel", "keywords": ["fleet", "swarm", "rtl", "land", "retry", "attempts"], "controls": [{"setting": "flyViewSettings.fleetCommandAttempts"}]}`
  (formatado como o resto do arquivo, 4 espaços).
- [ ] **Step 3: Botões** — no painel, `property var _guidedController: globals.guidedControllerFlyView`;
  `fleetRtlAllButton.onClicked: fleetController.rtlAll()` (direto, sem confirmação, decisão do usuário) e
  `fleetLandAllButton.onClicked: _guidedController.confirmAction(_guidedController.actionFleetLand, fleetController)`.
- [ ] **Step 4: Build** — como na C1, Step 4.
- [ ] **Step 5: Commit** —
  `git add src/FlyView/GuidedActionsController.qml src/FlyView/FlyViewFleetPanel.qml src/AppSettings/pages/FlyView.SettingsUI.json`;
  mensagem `feat(flyview): send fleet RTL at once, confirm fleet Land and expose the attempts setting`.

### Task D1 (agente D): `dist-windows.ps1`

**Files:**

- Create: `deploy/beswarm/dist-windows.ps1`

**Interfaces:**

- Produces: script com a interface do Contrato; usado na Task 3 e documentado por A no README.

- [ ] **Step 1: Escrever o script** — `$ErrorActionPreference = 'Stop'`; parâmetros do Contrato; comandos nativos via
  `Invoke-Native` (seção Contrato); sequência exata:
  1. Ambiente do PS-ENV (VS dev shell; `QT_ROOT_DIR = $QtRoot`; `CPM_SOURCE_CACHE = <RepoDir>\.cache\CPM` e
     `QGC_PYTHON_ENV = C:\gits\qgroundcontrol\tools\.venv` só se ainda não definidos e se `<RepoDir>\tools\.venv` não existir).
  2. Sem `-SkipBuild`: se `<BuildDir>\CMakeCache.txt` não existe, `cmake --preset Windows` em `$RepoDir`; depois
     `cmake --build $BuildDir`. Checar `$LASTEXITCODE` após cada comando nativo.
  3. `$staging = Join-Path $DistDir $Name`; apagar se existir;
     `cmake --install $BuildDir --config Release --component Runtime --prefix $staging`.
  4. Boot test sem Qt/GStreamer no `PATH`:
     `Start-Process "$staging\bin\QGroundControl.exe" -ArgumentList '--simple-boot-test','--allow-multiple' -Wait -PassThru`
     → `ExitCode` 0, senão `throw`.
  5. `$zip = Join-Path $DistDir "$Name-win64-portable.zip"`; `Compress-Archive -Path $staging -DestinationPath $zip -Force`.
  6. `.sha256` com `[IO.File]::WriteAllText("$zip.sha256", "$hash  $Name-win64-portable.zip`n")` (hash de
     `Get-FileHash -Algorithm SHA256`, em minúsculas; `WriteAllText` grava UTF-8 sem BOM).
  7. Resumo: caminho, tamanho em MB, hash.
- [ ] **Step 2: Validar sem tocar no build do agente C** — usar o build Release já pronto do worktree `win-rebuild`:
  `powershell -ExecutionPolicy Bypass -File C:\gits\qgroundcontrol\.claude\worktrees\fleet-extras\deploy\beswarm\dist-windows.ps1 -RepoDir C:\gits\qgroundcontrol\.claude\worktrees\win-rebuild -SkipBuild -DistDir <scratchpad>\dist-test -Name DistTest`
  (background). Esperado: código 0; `DistTest\bin\QGroundControl.exe`, `DistTest-win64-portable.zip` (~140 MB) e
  `.sha256` cujo hash confere com `Get-FileHash`. Apagar `<scratchpad>\dist-test` depois.
- [ ] **Step 3: Checagem estática (Windows)** — `git add deploy/beswarm/dist-windows.ps1`; parse do PowerShell como na
  Task A2, Step 4.3 (`$e.Count` → `0`); LINT-ENV + `uv run --frozen --project tools --group dev prek run --files deploy/beswarm/dist-windows.ps1`
  → `Passed`/`Skipped`.
- [ ] **Step 4: Commit** — mensagem `build(windows): add BeSwarm portable ZIP distribution script`.

### Task D2 (agente D): teste de UI do painel

**Files:**

- Create: `test/QmlUITests/FlyViewFleetPanelUITest.h`, `test/QmlUITests/FlyViewFleetPanelUITest.cc`
- Modify: `test/QmlUITests/CMakeLists.txt` (lista de fontes, após `FlyViewOccluderUITest.*`, e
  `add_qgc_test(FlyViewFleetPanelUITest LABELS Integration NoSanitizer TIMEOUT 180)` após a linha do `FlyViewROIAPMUITest`)

**Interfaces:**

- Consumes: `objectName`s e textos do Contrato; `QmlUITestBase` (`runWithMockLink`, `findVisibleItem`, `findItem`,
  `clickButton`). A confirmação segue o padrão de `FlyViewGuidedUnitsUITest.cc` (ler antes de escrever):
  `findVisibleItem(_rootItem, "guidedActionConfirmButton", 3000)` e depois
  `QMetaObject::invokeMethod(confirmButton, "activated")`.
- Produces: teste executado na Task 2.

Na Onda 1 este teste **só é escrito** (D não tem build); compila e roda na Task 2, Step 4, com o D retomado para corrigir.

- [ ] **Step 1: Escrever os testes** (`UT_REGISTER_TEST(FlyViewFleetPanelUITest, TestLabel::Integration)`):

  ```cpp
  void _testPanelListsVehiclesAndCollapses();
  //   apagar a chave "FlyViewFleetPanelExpanded" do grupo do QGroundControlQmlGlobal (QSettings) antes do startUI
  //   runWithMockLink(PX4): findVisibleItem("flyViewFleetPanel", 10000) e "fleetVehicleRow_<id>" não nulos
  //   texto de "fleetLinkText_<id>" não contém "Comm Lost"; mockLink->setCommLost(true)
  //   QTRY_VERIFY_WITH_TIMEOUT(texto contém "Comm Lost", TestTimeout::longMs())
  //   clickButton("fleetPanelToggle") → QTRY !findItem("fleetVehicleList")->isVisible()
  //   QGroundControlQmlGlobal::loadBoolGlobalSetting("FlyViewFleetPanelExpanded", true) == false
  //   clickButton("fleetPanelToggle") → lista visível de novo (scope guard restaura a chave)

  void _testRtlAllSendsWithoutConfirmation();
  //   runWithMockLink(PX4): vehicle->setArmed(true, false); QTRY armed
  //   clickButton("fleetRtlAllButton")  — nenhum clique no deslizador
  //   QTRY vehicle->flightMode() == vehicle->rtlFlightMode()
  //   QTRY texto de "fleetCommandStatus_<id>" == "RTL: confirmed"
  //   confirm = findItem(_rootItem, "guidedActionConfirmButton"); QVERIFY(!confirm || !confirm->isVisible())

  void _testLandAllRequiresConfirmation();
  //   runWithMockLink(PX4): vehicle->setArmed(true, false); QTRY armed
  //   QSignalSpy modeSpy(vehicle, &Vehicle::flightModeChanged); clickButton("fleetLandAllButton")
  //   confirm = findVisibleItem(_rootItem, "guidedActionConfirmButton", 3000); QVERIFY(confirm)
  //   QCOMPARE(modeSpy.count(), 0); QVERIFY(vehicle->flightMode() != vehicle->landFlightMode())
  //   status "fleetCommandStatus_<id>" ausente ou invisível (nada foi enviado antes de confirmar)
  //   QMetaObject::invokeMethod(confirm, "activated")
  //   QTRY vehicle->flightMode() == vehicle->landFlightMode(); QTRY status == "Land: confirmed"

  void _testFleetPanelLayoutWithMultiVehiclePanel();
  //   enableMultiVehiclePanel = true (guard restaura); janela 1280x720
  //   runWithMockLink(PX4) + segundo MockLink::startPX4MockLink() no corpo (desconectar no fim)
  //   itens: "flyViewTopRightPanel", "flyViewFleetPanel", "flyViewBottomRightRowLayout" (retângulos em coordenadas de cena)
  //   topRight.bottom <= fleet.top; fleet.bottom <= bottomRow.top; os três dentro da janela
  ```

- [ ] **Step 2: Commit** —
  `git add test/QmlUITests/FlyViewFleetPanelUITest.h test/QmlUITests/FlyViewFleetPanelUITest.cc test/QmlUITests/CMakeLists.txt`;
  mensagem `test(flyview): cover the fleet panel list, collapse, direct RTL and confirmed Land`.

---

## Onda 2 — integração, testes e distribuição

### Task 2: Integração (coordenador; retoma D no Step 4)

- [ ] **Step 1: Esperar** os commits de A2, B1, C1, C2, D1 e D2.
- [ ] **Step 2: Histórico linear** — no checkout principal (`feat/beswarm-fleet-panel`, que já tem Onda 0 + C), nesta
  ordem (D2 fica por último para poder ser emendado no Step 4):
  `git cherry-pick feat/beswarm-branding..feat/linux-dist`,
  `git cherry-pick $(git merge-base feat/beswarm-fleet-panel feat/fleet-core)..feat/fleet-core`,
  `git cherry-pick $(git merge-base feat/beswarm-fleet-panel feat/fleet-extras)..feat/fleet-extras`.
  (O `merge-base` é o commit da Task 1.) Esperado: sem conflitos (arquivos disjuntos). Conflito = violação da tabela de
  posse; resolver e anotar no relatório.
- [ ] **Step 3: Build Debug integrado** — reaproveitar o build do B (rápido, incremental):
  `git -C C:\gits\qgroundcontrol\.claude\worktrees\fleet-core checkout --detach feat/beswarm-fleet-panel`, depois PS-ENV +
  `cmake --build C:\gits\qgroundcontrol\.claude\worktrees\fleet-core\build\Windows-debug --parallel 12` (background).
- [ ] **Step 4: Testes direcionados** — PS-ENV + bloco de execução (`$b` = build do fleet-core) +
  `ctest --test-dir $b -R "FleetCommandControllerTest|FlyViewFleetPanelUITest|FlyViewOccluderUITest|FlyViewGuidedUnitsUITest|MultiVehicleManagerTest" --output-on-failure`.
  Esperado: todos PASS.
  - Falha no `FlyViewFleetPanelUITest`: retomar o agente D (SendMessage). D corrige **no worktree `fleet-core`** (que está
    destacado no topo da integração, cujo último commit é o D2), recompila, roda de novo e faz
    `git commit --amend --no-edit` ali; o coordenador traz o resultado com `git reset --keep <sha emendado>` no checkout
    principal (mesmo pai, só troca o último commit).
  - Se `_testFleetPanelLayoutWithMultiVehiclePanel` falhar por sobreposição real, a correção é no layout (C1): baixar o
    `maximumHeight` do painel de frota para `parent.height * 0.35`. Não afrouxar o teste.
  - Falha em teste de B ou C: correção em commit próprio (`fix(fleet): …`/`fix(flyview): …`) explicando a causa.
- [ ] **Step 5: Lint no Windows** — no checkout principal, LINT-ENV +
  `$files = git diff --name-only feat/beswarm-branding...HEAD; uv run --frozen --project tools --group dev prek run --files @files`.
  Esperado: todos os hooks `Passed`/`Skipped`. Correções viram um único commit `style: fix lint findings in fleet panel files`
  (ou entram no commit dono, se ainda não houver commits depois dele).
  - Fallback, só para hook que falha por **ambiente** e não por achado (ferramenta não encontrada, erro do próprio
    hook no Windows): rodar só esse hook no clone do WSL e registrar no relatório qual foi e por quê. O comando vai num
    script no scratchpad, executado com `wsl.exe -d Ubuntu-26.04 -- bash -l <script>`:
    `cd ~/src/qgroundcontrol && git fetch --force origin '+refs/heads/*:refs/remotes/origin/*' && git checkout --force --detach origin/feat/beswarm-fleet-panel && uv run --frozen --project tools --group dev prek run <hook-id> --files <arquivos>`.
  - Achado real (código fora do estilo) nunca vai para o fallback: corrige-se no Windows.
- [ ] **Step 6: Suíte completa (background)** —
  `ctest --test-dir $b -L "Unit|Integration" -LE "Flaky|Network" --output-on-failure -j 4`. Falha fora das áreas tocadas:
  rodar o mesmo teste no build do `feat/beswarm-branding` (ou registrar como pré-existente com a saída); falha nas
  áreas tocadas: corrigir antes de seguir.
- [ ] **Step 7: Conferência visual com MockLink** — rodar `$b\Debug\QGroundControl.exe --allow-multiple` (com o bloco de
  execução), Application Settings → Mock Link → criar 3 veículos PX4; verificar os itens 1–6 do checklist da Task 4.

### Task 3: Distribuição final (coordenador; Windows ∥ Linux, ambos disparados do PowerShell)

- [ ] **Step 1 (Windows, background):**
  `powershell -ExecutionPolicy Bypass -File C:\gits\qgroundcontrol\deploy\beswarm\dist-windows.ps1`.
  Esperado: código 0; `C:\gits\qgc-dist\BeSwarmVehicleGS\` substituída, ZIP e `.sha256` novos.
- [ ] **Step 2 (Linux, background, em paralelo ao Step 1):**
  `powershell -ExecutionPolicy Bypass -File C:\gits\qgroundcontrol\deploy\beswarm\dist-linux.ps1 -Ref feat/beswarm-fleet-panel -Jobs 6`.
  Esperado: código 0; AppImage e `.sha256` novos.
- [ ] **Step 3: Conferir (Windows)** — para o ZIP e para a AppImage, `(Get-FileHash <arquivo>).Hash.ToLower()` igual ao
  1º campo do `.sha256`; `Get-ChildItem C:\gits\qgc-dist` mostra os 4 artefatos BeSwarm com data de hoje.

### Task 4: Entrega para revisão do usuário (sem push)

- [ ] **Step 1:** `git log --oneline feat/beswarm-branding..feat/beswarm-fleet-panel` (esperado: 7 commits, + o de lint
  se houver) e `git status --short` limpo.
- [ ] **Step 2:** relatar ao usuário artefatos, hashes, testes executados (e o que não foi executado) e este checklist
  manual. Itens 1–7 no Windows. Item 8 com a AppImage, numa máquina/VM Linux com glibc ≥ 2.43 ou no próprio Windows
  via WSLg, no `Ubuntu-26.04`.
  1. Com 1 veículo: painel "Fleet (1)" no canto inferior direito, acima da telemetria; linha com ponto verde, nome do link
     e perda em %.
  2. Desligar o link do veículo: ponto vermelho, "Comm Lost" e "1 lost" no título.
  3. Recolher pelo título/chevron: só o cabeçalho fica; fechar e reabrir o app: continua recolhido.
  4. "RTL ALL" (amarelo) envia na hora, **sem** deslizador. Cada veículo armado mostra "RTL: attempt 1 of 3" e depois
     "RTL: confirmed"; os desarmados mostram "RTL: skipped (disarmed)".
  5. "LAND ALL" (vermelho) abre o deslizador "Fleet Land" e nada muda antes de confirmar. Ao confirmar durante um RTL
     em curso, todos passam a Land. "Stop retries" aparece só enquanto há tentativas.
  6. Settings → Fly View → "Fleet Panel" → "Fleet RTL/Land attempts" = 5: o próximo comando mostra "of 5".
  7. Com ≥ 2 veículos o painel multi-veículo do topo continua visível e não sobrepõe o painel de frota.
  8. Linux, em uma de duas formas:
     - Numa máquina Linux: `chmod +x BeSwarmVehicleGS-linux-x86_64.AppImage && ./BeSwarmVehicleGS-linux-x86_64.AppImage`.
     - No Windows: `wsl.exe -d Ubuntu-26.04 -- /mnt/c/gits/qgc-dist/BeSwarmVehicleGS-linux-x86_64.AppImage --appimage-extract-and-run`.

     Esperado: janela "BeSwarm Vehicle GS" com o logo.
- [ ] **Step 3:** perguntar se pode fazer push do `feat/beswarm-fleet-panel` (e abrir PR, se quiser). Os worktrees
  `fleet-core`, `fleet-extras` e `linux-dist` podem ser removidos depois (`git worktree remove …`) só com o OK do usuário.

---

## Briefs para disparar os agentes (copiar no prompt de cada um)

Cada agente recebe: este arquivo, `AGENTS.md`, `CODING_STYLE.md`, e a instrução de ler o Contrato, as Global
Constraints e **só as suas tasks**; não editar arquivo fora da sua linha na tabela de posse; commitar no próprio branch;
não fazer push; ao terminar, responder com SHA(s) dos commits, comandos de verificação executados e resultado.
Disparar B, C e D no mesmo turno, em background (A já foi disparado na Task 0, Step 2). Não usar a opção
`isolation: "worktree"` do disparo de agentes: os worktrees já existem (Task 0).

- **A — Linux:** "Execute as Tasks A1 e A2 do plano `docs/superpowers/plans/2026-11-20-beswarm-linux-e-painel-frota.md`
  no worktree `C:\gits\qgroundcontrol\.claude\worktrees\linux-dist` (branch `feat/linux-dist`). Use o WSL
  `Ubuntu-26.04` só para pacotes, Qt e build; edição, git e lint ficam no Windows. Se o build falhar por erro exclusivo
  do GCC 15, pare e reporte o erro exato."
- **B — Núcleo C++:** "Execute a Task B1 no worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-core`
  (branch `feat/fleet-core`), em TDD: testes primeiro, ver falhar, implementar, ver passar."
- **C — QML:** "Execute as Tasks C1 e C2 no checkout `C:\gits\qgroundcontrol` (branch `feat/beswarm-fleet-panel`).
  O controlador ainda é um stub: valide só build e ausência de avisos de QML."
- **D — Extras:** "Execute as Tasks D1 e D2 no worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-extras`
  (branch `feat/fleet-extras`). D2 é só escrita nesta onda; você será retomado na Task 2, Step 4, para compilar e corrigir."
