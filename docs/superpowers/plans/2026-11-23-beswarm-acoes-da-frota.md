# BeSwarm Vehicle GS — Ações da frota: confirmação central, reenvio X/Y, TAKEOFF ALL e REBOOT ALL

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** No painel de frota do Fly View, trocar a confirmação discreta do LAND ALL por um aviso grande no centro da
tela; fazer RTL ALL e LAND ALL enviarem o comando exatamente X vezes, uma a cada Y segundos, com X e Y editáveis ao
lado dos botões; adicionar TAKEOFF ALL (todos os veículos decolam a 5 m) e REBOOT ALL (reinício da controladora de
todos, com duas confirmações no centro da tela); e gerar de novo os pacotes Windows e Linux em `C:\gits\qgc-dist`.

**Architecture:**

- **Um único dono dos comandos da frota.** O `FleetCommandController` (C++, já existe, `src/Vehicle/`) ganha os
  comandos `Takeoff` e `Reboot`. Continua valendo a regra "um comando novo substitui o anterior": LAND ALL apertado
  durante a decolagem encerra a sequência de decolagem antes de qualquer arm ou `MAV_CMD_NAV_TAKEOFF` tardio.
- **RTL/Land com X envios a cada Y s.** Deixa de ser "reenvia até confirmar": cada veículo armado recebe exatamente X
  envios, um a cada Y s, mesmo depois de já estar no modo alvo (se alguém tirar o veículo do modo, o próximo envio o
  devolve). X = Fact `flyViewSettings.fleetCommandAttempts` (já existe), Y = Fact novo
  `flyViewSettings.fleetCommandIntervalSec`; os dois são lidos uma vez por comando. "Stop" interrompe.
- **Decolagem sem travar a UI e sem `if (px4)`.** Por veículo: troca para o modo em que o firmware arma para decolar
  (acessor novo `VehicleReadiness::takeoffArmFlightMode()`, que só expõe o que cada plugin já declara em
  `_actionFlightModes(GuidedAction::Takeoff)`), arma uma única vez e então chama `Vehicle::guidedModeTakeoff(5.0)`.
  Com modo e arm já corretos, os helpers bloqueantes do plugin ArduPilot (`_setFlightModeAndValidate`,
  `_armVehicleAndValidate`) retornam na primeira linha, então nada bloqueia. Sucesso = `Vehicle::airborne()`.
- **Reboot.** `Vehicle::rebootVehicle()` para todos os conectados, armados inclusive (decisão do usuário). Quando o
  firmware aceita, o próprio `Vehicle` se fecha (`_rebootCommandResultHandler` → `closeVehicle()`), então "veículo
  removido durante o reboot" = aceito. O `rebootVehicle()` usa handler próprio, por isso o sinal
  `Vehicle::mavCommandResult` não é emitido para ele e não serve para confirmar.
- **UI.** Componente novo `FleetConfirmOverlay.qml`: fundo escurecido sobre todo o Fly View, cartão no centro com borda
  grossa piscando na cor do comando, título grande e botões grandes. LAND ALL usa 1 etapa; REBOOT ALL usa 2 etapas, e o
  botão da 2ª só habilita 1,5 s depois da 1ª (um toque duplo não passa pelas duas). O LAND ALL deixa de usar o
  `GuidedActionsController` (a ação `actionFleetLand` é removida). RTL ALL e TAKEOFF ALL enviam na hora, sem
  confirmação. Campos X e Y (`FactTextField`) numa linha logo acima de RTL ALL/LAND ALL; TAKEOFF ALL e REBOOT ALL numa
  linha acima dos campos.
- **Paralelismo:** Onda 0 (contrato, coordenador) → Onda 1 (B: C++ e testes ∥ C: QML ∥ D: testes de UI, arquivos
  disjuntos) → Onda 2 (integração, testes, lint, distribuição Windows ∥ Linux).

**Tech Stack:** Qt 6.12.0 (`msvc2022_64` no Windows, `gcc_64` no Linux), CMake ≥ 3.28 + Ninja, C++20, QML, QtTest +
MockLink, PowerShell 5.1, WSL2 `Ubuntu-26.04` (só para o build Linux), uv 0.13.7 + prek (lint no Windows).

**Spec:** pedido do usuário nesta sessão (2026-11-23):
(1) o botão de confirmação do LAND da tela principal está muito tímido; tem que haver algo no meio da tela que chame a
atenção;
(2) RTL e LAND da tela principal têm que ser reenviados X vezes com intervalo de Y segundos, com X e Y editáveis perto
do botão;
(3) botão de takeoff que faça todos os veículos conectados decolarem a 5 metros;
(4) botão de boot (reinício) do ArduPilot de todos os veículos, confirmado duas vezes e chamando a atenção;
(5) build para Linux e Windows como nas pastas anteriores (`C:\gits\qgc-dist`);
(6) plano detalhado para ser executado por outro modelo.
Decisões do usuário nesta sessão, antes do plano (não reabrir): reenvio = **sempre X envios**, mesmo depois de
confirmado; TAKEOFF ALL **sem confirmação**; REBOOT ALL **envia para todos**, inclusive armados.
Contexto: `docs/superpowers/plans/2026-11-20-beswarm-linux-e-painel-frota.md` (painel de frota e scripts de
distribuição, já executado e no `master`).

## Global Constraints

- Branch base: `master`, cujo HEAD é o commit que adiciona este plano (pai `297d8aeef`). Branch de integração:
  `feat/fleet-actions`.
- **Sem** `git push`, PR ou merge em `master` sem autorização explícita do usuário. Commits locais nos branches dos
  agentes são permitidos (a integração depende deles), sempre com `git add <arquivos da task>` (nunca `-A`/`.`).
- Commits em Conventional Commits, terminando com a linha `Co-Authored-By:` do modelo executor.
- Identidade interna inalterada: executável `QGroundControl` / `QGroundControl.exe`, `QGC_APP_NAME`, pastas de
  configuração. Nome exibido: `BeSwarm Vehicle GS`.
- Artefatos (nomes exatos, substituem os atuais):
  - Windows: pasta `C:\gits\qgc-dist\BeSwarmVehicleGS\`, `C:\gits\qgc-dist\BeSwarmVehicleGS-win64-portable.zip` e
    `….zip.sha256`.
  - Linux: `C:\gits\qgc-dist\BeSwarmVehicleGS-linux-x86_64.AppImage` e `….AppImage.sha256` (build na distro WSL
    `Ubuntu-26.04`; a AppImage exige glibc ≥ 2.43, consequência já aceita pelo usuário).
- **Windows primeiro:** edição, git, lint, testes, cópia e hashes no Windows. O WSL só compila e empacota o Linux, e
  quem o chama é o `deploy\beswarm\dist-linux.ps1`.
- Estilo (`CODING_STYLE.md`): 4 espaços, LF, ≤ 120 colunas; C++ com membros `_privados`, chaves sempre, sem
  `Q_ASSERT`, logging na categoria existente `FleetCommandControllerLog` (`"Vehicle.FleetCommandController"`); QML sem
  tamanhos/cores fixos (`ScreenTools`, `QGCPalette`), `qsTr()` em todo texto visível, `Connections` com sintaxe de
  função; testes sem `QTest::qWait(<n>)` nem `QTest::ignoreMessage` (usar `QTRY_*_WITH_TIMEOUT`, `QTest::qWaitFor`,
  `QSignalSpy::wait`, `expectLogMessage`/`ignoreLogMessage`/`expectAppMessage`).
- Firmware Plugin: nada de `if (px4)`/`if (apm)`. Modos vêm de `Vehicle::rtlFlightMode()`/`landFlightMode()` e de
  `VehicleReadiness::takeoffArmFlightMode()`; a capacidade de decolar vem de
  `vehicle->supports()->guidedTakeoffWithAltitude()`.
- Segurança, regra do próprio QGC (`FirmwarePlugin::_armVehicleAndValidate`): **arm nunca é reenviado**. Na
  decolagem, troca de modo e arm saem uma vez cada.
- Unity build: nomes locais de arquivo (namespace anônimo/`static`) não podem repetir nomes de outros `.cc`.
- Traduções `.ts` não são tocadas (mesma decisão do plano anterior; as strings novas aparecem em inglês).
- O preset `Windows` (Release) tem `QGC_BUILD_TESTING=OFF`. Testes rodam no build Debug já existente
  `C:\gits\qgroundcontrol\.claude\worktrees\fleet-core\build\Windows-debug` (incremental, `QGC_BUILD_TESTING=ON`).
- No Windows, os testes de UI (`test/QmlUITests`) **não** rodam pelo `ctest` (crash 0xc0000409 com backend Quick
  offscreen). Rodam na tela, com o executável Debug: bloco **RUN-UITEST**.
- No Windows desta máquina não há `just`; o `uv` 0.13.7 está em `%APPDATA%\Python\Python312\Scripts`, fora do `PATH`, e
  o `bash` do `PATH` é o lançador do WSL. Todo lint usa o bloco **LINT-ENV**.
- Cada chamada de shell do executor é um processo novo: repita o bloco **PS-ENV** em toda chamada que compila ou roda
  teste. Comandos que podem passar de 2 minutos (build, ctest completo, distribuição) rodam em background.

## Decisões já tomadas e padrões adotados

Decididas pelo usuário (não reabrir): sempre X envios; TAKEOFF ALL sem confirmação; REBOOT ALL para todos, inclusive
armados (o aviso central mostra quantos estão armados).

Padrões deste plano. Se o usuário pedir outra coisa na revisão, ajustar a task indicada:

1. **Faixas de X e Y** — X continua 1–20 (padrão 3); Y novo, 1–60 s (padrão 2 s), inteiro. X e Y valem para RTL e
   LAND (um único par). Task 1 e Task C2.
2. **Altitude da decolagem** — fixa em 5 m (`kTakeoffAltitudeMeters`), não editável. No ArduPilot o plugin já usa
   `max(PILOT_TKO_ALT_M ou PILOT_TKOFF_ALT, 5 m)`: se o parâmetro do veículo for maior que 5 m, vale o dele. Task B2.
3. **Tempos da decolagem** — troca de modo: 5 s; arm: 5 s; subida até `airborne`: 15 s. Task B2.
4. **Confirmação do LAND** — um toque no botão grande do aviso central (sem "segurar para confirmar"). Task C1.
5. **Anti toque duplo no REBOOT** — o botão da 2ª etapa só habilita 1,5 s depois da 1ª. Task C1.
6. **Veículos já armados no chão** entram no TAKEOFF ALL (só trocam de modo, se preciso, e decolam). Task B2.
7. **Reboot aceito = veículo desconectou** em até 6 s. Um veículo que caia por outro motivo nessa janela também
   aparece como "accepted". Task B3.

## Review Focus

1. **LAND ALL ou RTL ALL apertado enquanto o TAKEOFF ALL ainda troca de modo/arma** → a sequência de decolagem para na
   hora; nenhum arm nem `MAV_CMD_NAV_TAKEOFF` sai depois. → B2 `_testLandCancelsTakeoff`.
2. **Piloto (ou outro GCS) tira o veículo do RTL durante os X envios** → o próximo envio o devolve ao RTL; ao fim, o
   veículo está no RTL e o estado é "confirmed"; "Stop" interrompe os envios. → B1 `_testReassertsTargetMode` e
   `_testCancelStopsResending`.
3. **X ou Y alterados enquanto um comando ainda envia** → o comando em curso mantém os valores do início; o próximo usa
   os novos. → B1 `_testSettingsSnapshotPerCommand` e `_testIntervalFromSettings`.
4. **Valor inválido nos campos X/Y (0, 21, 61, vazio)** → recusado com o aviso de validação do `FactTextField`; o
   setting não muda. → D `_testResendFieldsEditSettings`.
5. **Toque duplo rápido na confirmação do REBOOT** (ou toque no botão da 2ª etapa ainda desabilitado) → nada é enviado
   antes da 2ª confirmação habilitada. → D `_testRebootNeedsTwoConfirmations`. Relacionado: aviso aberto quando o
   último veículo desconecta → o aviso fecha sem executar. → D `_testConfirmOverlayClosesWhenFleetEmpty`.

Ruído conhecido e aceito: no ArduPilot a troca de modo usa `MAV_CMD_DO_SET_MODE` pela fila de comandos, então com link
mudo cada envio pode gerar "Vehicle did not respond to command: Set flight mode" ou "Unable to send command: Waiting on
previous response". Com X envios fixos, isso agora acontece também com o link bom se Y for menor que o tempo de ACK
(1,2 s); por isso Y mínimo = 1 s e padrão = 2 s.

---

## Organização: ondas, agentes e posse de arquivos

```text
Onda 0 (coordenador, ~20 min): Task 0 → Task 1 ─► Onda 1: B1→B2→B3 ∥ C1→C2 ∥ D1 ─► Onda 2: Task 2 → Task 3 (Win ∥ Linux) → Task 4
```

| Agente | Onde trabalha | Branch | Build | Tasks |
| --- | --- | --- | --- | --- |
| Coordenador | `C:\gits\qgroundcontrol` | `feat/fleet-actions` | `build\Windows` (Release, incremental) | 0, 1, 2, 3, 4 |
| B — Núcleo C++ | worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-core` | `feat/fleet-actions-core` | `build\Windows-debug` do próprio worktree (já existe, com testes) | B1, B2, B3 |
| C — QML | `C:\gits\qgroundcontrol` (o coordenador fica ocioso na Onda 1) | `feat/fleet-actions` | `build\Windows` (Release, incremental) | C1, C2 |
| D — Testes de UI | worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-extras` | `feat/fleet-actions-uitests` | nenhum na Onda 1 (executa na Onda 2) | D1 |

Posse de arquivos (ninguém edita arquivo de outro dono; a integração não pode ter conflito):

| Dono | Arquivos |
| --- | --- |
| Onda 0 | parte pública de `src/Vehicle/FleetCommandController.h`, stubs novos em `src/Vehicle/FleetCommandController.cc`, `src/Settings/FlyView.SettingsGroup.json`, `src/Settings/FlyViewSettings.h`, `src/Settings/FlyViewSettings.cc` |
| B | `src/Vehicle/FleetCommandController.h` e `.cc` (depois da Onda 0), `src/Vehicle/VehicleReadiness.h`, `test/Vehicle/FleetCommandControllerTest.h` e `.cc`, `test/Vehicle/CMakeLists.txt` (só o `TIMEOUT`, se precisar) |
| C | `src/FlyView/FleetConfirmOverlay.qml` (novo), `src/FlyView/CMakeLists.txt`, `src/FlyView/FlyViewFleetPanel.qml`, `src/FlyView/FlyViewWidgetLayer.qml`, `src/FlyView/GuidedActionsController.qml`, `src/AppSettings/pages/FlyView.SettingsUI.json` |
| D | `test/QmlUITests/FlyViewFleetPanelUITest.h` e `.cc`, `test/QmlUITests/CMakeLists.txt` (só o `TIMEOUT`, se precisar) |

Tempo estimado: Onda 0 ≈ 20 min; Onda 1 ≈ 1 h 30 (B domina); Onda 2 ≈ 1 h 30 (build Linux ≈ 40 min).

### Bloco PS-ENV (Windows, toda chamada que compila ou roda teste)

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

### Bloco RUN-UITEST (Windows, teste de UI na tela)

PS-ENV + bloco de execução acima com `$b = 'C:\gits\qgroundcontrol\.claude\worktrees\fleet-core\build\Windows-debug'`,
e então:

```powershell
& "$b\Debug\QGroundControl.exe" --unittest:FlyViewFleetPanelUITest --onscreen --allow-multiple 2>&1 | Out-String
"exit=$LASTEXITCODE"
```

Esperado: `Totals: <n> passed, 0 failed` e `exit=0`. O `2>&1` é obrigatório (executável de subsistema GUI). Não mexer
no mouse/teclado durante a execução.

### Bloco LINT-ENV (Windows, toda chamada de lint)

```powershell
$env:Path = "C:\Program Files\Git\bin;$env:APPDATA\Python\Python312\Scripts;C:\Qt\6.12.0\msvc2022_64\bin;$env:Path"  # bash = Git Bash; uv 0.13.7; qmllint
$env:UV_PROJECT_ENVIRONMENT = 'C:\gits\qgroundcontrol\tools\.venv'  # worktrees reaproveitam o venv do checkout principal
$env:QT_ROOT_DIR = 'C:\Qt\6.12.0\msvc2022_64'
$env:PRE_COMMIT_FROM_REF = 'master'  # sem isto o clang-format checa os arquivos inteiros
```

Conferir uma vez por processo: `(Get-Command bash).Source` = `C:\Program Files\Git\bin\bash.exe`. Comando, na raiz do
checkout/worktree: `uv run --frozen --project tools --group dev prek run --files <arquivos>`.

---

## Contrato (criado na Onda 0, usado por todos)

### C++ — parte pública de `src/Vehicle/FleetCommandController.h` (congelada)

Formatação como a do arquivo atual (o clang-format põe um enumerador por linha). Seção `private:` fica com B.

```cpp
class QTimer;
class Vehicle;

Q_DECLARE_LOGGING_CATEGORY(FleetCommandControllerLog)

/// Sends RTL, Land, Takeoff or Reboot to every connected vehicle and tracks the result per vehicle.
class FleetCommandController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(Command activeCommand READ activeCommand NOTIFY activeCommandChanged)
    Q_PROPERTY(QVariantMap vehicleStatus READ vehicleStatus NOTIFY vehicleStatusChanged)
    Q_PROPERTY(double takeoffAltitude READ takeoffAltitude CONSTANT)

public:
    explicit FleetCommandController(QObject* parent = nullptr);
    ~FleetCommandController() override;

    enum class Command { None, RTL, Land, Takeoff, Reboot };
    Q_ENUM(Command)

    enum class VehicleState { Pending, Succeeded, Failed, Unsupported, Skipped, Cancelled };
    Q_ENUM(VehicleState)

    /// Where a takeoff is, or where it stopped when it failed
    enum class TakeoffStep { None, Mode, Arm, Climb };
    Q_ENUM(TakeoffStep)

    Q_INVOKABLE void rtlAll();
    Q_INVOKABLE void landAll();
    Q_INVOKABLE void takeoffAll();
    Q_INVOKABLE void rebootAll();
    Q_INVOKABLE void cancel();

    bool busy() const { return _busy; }
    Command activeCommand() const { return _activeCommand; }
    QVariantMap vehicleStatus() const { return _vehicleStatus; }
    double takeoffAltitude() const { return kTakeoffAltitudeMeters; }

    static constexpr int kMinAttempts = 1;
    static constexpr int kMaxAttempts = 20;
    static constexpr int kMinIntervalSec = 1;
    static constexpr int kMaxIntervalSec = 60;
    static constexpr double kTakeoffAltitudeMeters = 5.0;
    static constexpr int kTakeoffStepTimeoutMs = 5000;
    static constexpr int kTakeoffClimbTimeoutMs = 15000;
    static constexpr int kRebootTimeoutMs = 6000;

signals:
    void busyChanged();
    void activeCommandChanged();
    void vehicleStatusChanged();
    void finished(FleetCommandController::Command command);
```

Transição: a Onda 0 **mantém** `static constexpr int kAttemptTimeoutMs = 2000;` (o `.cc` e os testes atuais usam). B o
remove na Task B1, junto com os usos.

`vehicleStatus`: chave `QString::number(vehicle->id())` → `QVariantMap`
`{"state": int(VehicleState), "attempt": int, "maxAttempts": int, "confirmed": bool, "step": int(TakeoffStep)}`.
`confirmed` só é significativo em RTL/Land (`false` nos outros); `step` só em Takeoff (`None` nos outros).

### Regras de comportamento (B implementa, C exibe, D testa a UI)

Comuns (valem para os 4 comandos):

- **G1** `rtlAll()`/`landAll()`/`takeoffAll()`/`rebootAll()` iniciam um comando. Se outro estiver em curso, ele é
  encerrado **sem** `finished`: todos os timers dele param e todas as conexões dele são desfeitas antes de o novo
  começar. `activeCommand` passa a ser o novo comando.
- **G2** Fotografia de `MultiVehicleManager::instance()->vehicles()` no início; veículos que chegarem depois ficam fora.
- **G3** `busy` = existe entrada `Pending`. Quando o último `Pending` se resolve, emite `finished(activeCommand)` uma vez.
  Comando sem nenhum `Pending` emite `finished` já dentro da chamada, e `busy` nunca fica `true`.
- **G4** `cancel()`: para timers e conexões; `Pending` → `Cancelled`; `busy = false`; **não** emite `finished`.
- **G5** Toda mudança reconstrói `_vehicleStatus` e emite `vehicleStatusChanged()`.
- **G6** `MultiVehicleManager::vehicleRemoved(v)`: em Reboot ver R2; nos outros comandos a entrada de `v` sai do mapa.
  Guardar `QPointer<Vehicle>`; nada de crash com veículo destruído.

RTL e Land:

- **L1** No início: `X = qBound(kMinAttempts, fleetCommandAttempts, kMaxAttempts)` e
  `Y = qBound(kMinIntervalSec, fleetCommandIntervalSec, kMaxIntervalSec)` segundos. Mudar os Facts depois não afeta o
  comando em curso.
- **L2** Classificação, nesta ordem: modo alvo vazio → `Unsupported`; `!vehicle->armed()` → `Skipped`; senão `Pending`,
  com `confirmed = (vehicle->flightMode() == alvo)`. Veículo que **já** está no modo alvo também fica `Pending` e recebe
  os X envios. `Unsupported`/`Skipped` têm `attempt == 0`.
- **L3** Envio 1 sai imediatamente para todos os `Pending`. A cada Y s (um timer só para o comando), para cada
  `Pending`: se `attempt < X` → novo envio (`attempt++`, `vehicle->setFlightMode(alvo)`, `qCDebug` com id, comando e
  `attempt/X`); senão → resolve: `confirmed` → `Succeeded`; não confirmado → `Failed`, com
  `qCWarning(FleetCommandControllerLog) << "vehicle:" << id << "command:" << _activeCommand << "failed after" << X << "attempts"`
  (formato atual; os testes casam `vehicle: <id> command: .*Land failed after 2 attempts`).
- **L4** `Vehicle::flightModeChanged(mode)` de veículo `Pending`: `confirmed = (mode == alvo)`; se `confirmed` e
  `attempt >= X` → `Succeeded` na hora.

Takeoff:

- **T1** Classificação, nesta ordem: `!vehicle->supports()->guidedTakeoffWithAltitude()` → `Unsupported`;
  `vehicle->airborne()` → `Skipped`; senão `Pending` com `attempt = 1`, `maxAttempts = 1`. Todos em paralelo, cada um
  com o próprio timer.
- **T2** Passo `Mode`: `armMode = vehicle->readiness() ? vehicle->readiness()->takeoffArmFlightMode() : QString()`. Se
  `armMode` não é vazio e difere de `vehicle->flightMode()`: `vehicle->setFlightMode(armMode)` **uma vez** e espera
  `flightModeChanged(armMode)` por até `kTakeoffStepTimeoutMs`. Senão pula para `Arm`.
- **T3** Passo `Arm`: se `!vehicle->armed()`: `vehicle->setArmed(true, false)` **uma vez** e espera
  `armedChanged(true)` por até `kTakeoffStepTimeoutMs`. Senão pula para `Climb`.
- **T4** Passo `Climb`: `vehicle->guidedModeTakeoff(kTakeoffAltitudeMeters)` e espera `airborneChanged(true)` (ou
  `airborne()` já `true`) por até `kTakeoffClimbTimeoutMs` → `Succeeded`.
- **T5** Tempo esgotado em qualquer passo → `Failed`, `step` fica no passo onde parou, e
  `qCWarning(FleetCommandControllerLog) << "vehicle:" << id << "command:" << _activeCommand << "failed at step" << step`.

Reboot:

- **R1** Todo veículo da fotografia → `Pending` (`attempt = 1`, `maxAttempts = 1`) e `vehicle->rebootVehicle()` uma
  vez. Sem filtro de armado (decisão do usuário).
- **R2** `vehicleRemoved(v)` com a entrada de `v` `Pending` → `Succeeded`, e a entrada **fica** no mapa (o veículo volta
  com o mesmo id depois do reinício e a linha dele mostra "Reboot: accepted").
- **R3** `kRebootTimeoutMs` sem remoção → `Failed`, com
  `qCWarning(FleetCommandControllerLog) << "vehicle:" << id << "command:" << _activeCommand << "failed: still connected after" << kRebootTimeoutMs << "ms"`.
  (Se o firmware recusar ou não responder, o próprio `Vehicle` já mostra "Vehicle reboot failed.")

### Facts — `flyViewSettings`

Em `src/Settings/FlyView.SettingsGroup.json`, o objeto `fleetCommandAttempts` (já existe) troca só `shortDesc` e
`label`, e o objeto novo entra logo depois dele (é o último do arquivo):

```json
{
    "name": "fleetCommandAttempts",
    "shortDesc": "Number of times the Fleet panel sends RTL or Land to each armed vehicle.",
    "type": "uint32",
    "min": 1,
    "max": 20,
    "default": 3,
    "label": "Fleet RTL/Land sends",
    "keywords": "fleet,swarm,rtl,land,retry,attempts"
},
{
    "name": "fleetCommandIntervalSec",
    "shortDesc": "Seconds between two sends of RTL or Land to the same vehicle from the Fleet panel.",
    "type": "uint32",
    "units": "s",
    "min": 1,
    "max": 60,
    "default": 2,
    "label": "Fleet RTL/Land interval",
    "keywords": "fleet,swarm,rtl,land,retry,interval"
}
```

C++: `DEFINE_SETTINGFACT(fleetCommandIntervalSec)` em `FlyViewSettings.h` e
`DECLARE_SETTINGSFACT(FlyViewSettings, fleetCommandIntervalSec)` em `FlyViewSettings.cc`, ambos logo depois de
`fleetCommandAttempts`.

### QML — `FleetConfirmOverlay.qml` (módulo `QGroundControl.FlyView`)

API (C1 implementa, C2 usa, D testa):

- `function show(steps, onAccepted)`: `steps` é um array de 1 ou 2 objetos
  `{ title: string, message: string, confirmText: string, accentColor: color }`; `onAccepted` é uma função JS chamada
  **uma vez**, depois da confirmação da última etapa. Chamar `show()` com o aviso aberto substitui o anterior sem chamar
  o `onAccepted` antigo.
- `function cancel()`: fecha sem chamar `onAccepted`.
- `readonly property int currentStep` (1-based; 0 = fechado), `readonly property int stepCount`.
- `readonly property int secondStepDelayMs: 1500`: o botão de aceitar da etapa 2 fica `enabled: false` por esse tempo
  depois de a etapa 1 ser aceita, com o texto `qsTr("Wait")`; depois mostra o `confirmText` da etapa 2.
- `visible: currentStep > 0`. Enquanto visível, absorve todo clique/toque fora do cartão (nada passa para o mapa nem
  para o painel); clicar no fundo **não** fecha.
- Visual: fundo `qgcPal.window` com `opacity` 0.7 cobrindo todo o pai; cartão `anchors.centerIn: parent`, largura
  `Math.min(parent.width * 0.8, ScreenTools.defaultFontPixelWidth * 60)`, fundo `qgcPal.window`, borda de largura
  `ScreenTools.defaultFontPixelWidth * 0.6` na `accentColor` da etapa, piscando (animação infinita de `opacity` da
  borda entre 1 e 0.35, 600 ms cada sentido); título em negrito, `font.pointSize: ScreenTools.largeFontPointSize * 1.5`,
  cor `accentColor`; mensagem com `wrapMode: Text.WordWrap`; linha "Step %1 of %2" só quando `stepCount === 2`; botões
  `Cancel` e aceitar lado a lado, cada um com `Layout.fillWidth: true`, `pointSize: ScreenTools.largeFontPointSize`,
  aceitar com `backgroundColor: accentColor`.
- `objectName`s: `fleetConfirmOverlay` (raiz), `fleetConfirmCard`, `fleetConfirmTitle`, `fleetConfirmMessage`,
  `fleetConfirmStep`, `fleetConfirmAcceptButton`, `fleetConfirmCancelButton`.

Instância: em `FlyViewWidgetLayer.qml`, irmão do painel de frota:
`FleetConfirmOverlay { id: fleetConfirmOverlay; anchors.fill: parent; z: QGroundControl.zOrderTopMost + 2 }`, e o
painel recebe `confirmOverlay: fleetConfirmOverlay`.

### QML — `FlyViewFleetPanel.qml`

- Propriedades públicas: as atuais (`maximumHeight`, `expanded`, `controller`) + `property var confirmOverlay`. A
  propriedade privada `_guidedController` sai.
- Ordem dos elementos, de cima para baixo: cabeçalho; lista de veículos; `fleetSummary`; `fleetStopButton`; linha
  TAKEOFF ALL | REBOOT ALL; linha dos campos X/Y (`fleetResendRow`); linha RTL ALL | LAND ALL. As três linhas de baixo
  ficam visíveis sempre que o painel está expandido, então resumo e "Stop" aparecem **acima** delas e nenhum botão muda
  de lugar quando um comando começa.
- `objectName`s novos: `fleetTakeoffAllButton`, `fleetRebootAllButton`, `fleetResendRow`, `fleetAttemptsField`,
  `fleetIntervalField`. Os atuais continuam.
- Fechar o aviso: `Connections { target: root._vehicles; function onCountChanged() { … } }` chama
  `confirmOverlay.cancel()` quando `count === 0`.

Botões e ações:

| Botão | Texto | Cor (`backgroundColor` / `textColor`) | Ação |
| --- | --- | --- | --- |
| `fleetTakeoffAllButton` | `qsTr("TAKEOFF ALL %1 m").arg(fleetController.takeoffAltitude)` | `qgcPal.colorGreen` / `qgcPal.mapWidgetBorderDark` | `fleetController.takeoffAll()` direto |
| `fleetRebootAllButton` | `qsTr("REBOOT ALL")` | `qgcPal.colorOrange` / `qgcPal.mapWidgetBorderDark` | aviso de 2 etapas → `rebootAll()` |
| `fleetRtlAllButton` | `qsTr("RTL ALL")` (atual) | atual | `fleetController.rtlAll()` direto (atual) |
| `fleetLandAllButton` | `qsTr("LAND ALL")` (atual) | atual | aviso de 1 etapa → `landAll()` |
| `fleetStopButton` | `qsTr("Stop")` | atual | `fleetController.cancel()` (atual) |

Linha `fleetResendRow`: `QGCLabel` `qsTr("Send")`, `FactTextField` `fleetAttemptsField`
(`fact: QGroundControl.settingsManager.flyViewSettings.fleetCommandAttempts`), `QGCLabel` `qsTr("times, every")`,
`FactTextField` `fleetIntervalField` (`fact: …flyViewSettings.fleetCommandIntervalSec`, mostra a unidade "s"). Largura
de cada campo: `ScreenTools.defaultFontPixelWidth * 6`.

Textos do aviso (`%1`/`%2` calculados na hora do clique; `armed` = veículos com `armed`, `count` = `_vehicles.count`):

| Etapa | `title` | `message` | `confirmText` | `accentColor` |
| --- | --- | --- | --- | --- |
| Land | `qsTr("LAND ALL VEHICLES")` | `qsTr("%1 armed vehicles will land at their current position.").arg(armed)` | `qsTr("LAND ALL")` | `qgcPal.colorRed` |
| Reboot 1 | `qsTr("REBOOT ALL AUTOPILOTS")` | `qsTr("%1 vehicles will reboot their flight controller and lose their link until it restarts.").arg(count)` e, se `armed > 0`, uma 2ª linha `qsTr("WARNING: %1 of them are ARMED.").arg(armed)` | `qsTr("REBOOT ALL")` | `qgcPal.colorOrange` |
| Reboot 2 | `qsTr("CONFIRM REBOOT")` | `qsTr("Second confirmation: reboot %1 autopilots now?").arg(count)` | `qsTr("YES, REBOOT NOW")` | `qgcPal.colorRed` |

Linha de status por veículo (`fleetCommandStatus_<id>`); `%1` = nome do comando: RTL `qsTr("RTL")`, Land
`qsTr("Land")`, Takeoff `qsTr("Takeoff")`, Reboot `qsTr("Reboot")`:

| Comando | Estado | Texto |
| --- | --- | --- |
| RTL/Land | `Pending`, `confirmed` falso | `"%1: sent %2 of %3"` (`attempt`, `maxAttempts`) |
| RTL/Land | `Pending`, `confirmed` verdadeiro | `"%1: sent %2 of %3, confirmed"` |
| RTL/Land | `Succeeded` / `Failed` / `Unsupported` / `Skipped` / `Cancelled` | `"%1: confirmed"` / `"%1: failed after %2 attempts"` / `"%1: not supported"` / `"%1: skipped (disarmed)"` / `"%1: stopped"` (atuais) |
| Takeoff | `Pending` com `step` `Mode` / `Arm` / `Climb` | `"Takeoff: setting mode"` / `"Takeoff: arming"` / `"Takeoff: climbing to %1 m"` (`takeoffAltitude`) |
| Takeoff | `Succeeded` | `"Takeoff: airborne"` |
| Takeoff | `Failed` com `step` `Mode` / `Arm` / `Climb` | `"Takeoff: failed (mode)"` / `"Takeoff: failed (arming)"` / `"Takeoff: failed (no climb)"` |
| Takeoff | `Unsupported` / `Skipped` / `Cancelled` | `"Takeoff: not supported"` / `"Takeoff: skipped (airborne)"` / `"Takeoff: stopped"` |
| Reboot | `Pending` / `Succeeded` / `Failed` / `Cancelled` | `"Reboot: sent"` / `"Reboot: accepted"` / `"Reboot: failed"` / `"Reboot: stopped"` |

Cores do status: as atuais (`Pending` → `qgcPal.warningText`, `Succeeded` → `qgcPal.successText`, `Failed` →
`qgcPal.errorText`, resto → `qgcPal.textSecondary`). Resumo `fleetSummary`: template atual
`"%1: %2 of %3 confirmed, %4 failed"` com o nome do comando acima.

---

## Onda 0 — coordenador

### Task 0: Branch e worktrees

- [ ] **Step 1: Conferir estado** — `git -C C:\gits\qgroundcontrol status --short` vazio e branch `master`;
  `git -C C:\gits\qgroundcontrol\.claude\worktrees\fleet-core status --short` e
  `git -C C:\gits\qgroundcontrol\.claude\worktrees\fleet-extras status --short` vazios. Se algum worktree tiver
  alterações, parar e perguntar ao usuário (não descartar nada).
- [ ] **Step 2: Branch de integração** — `git -C C:\gits\qgroundcontrol switch -c feat/fleet-actions`.
- [ ] **Step 3 (depois da Task 1): worktrees dos agentes** —
  `git -C C:\gits\qgroundcontrol\.claude\worktrees\fleet-core switch -c feat/fleet-actions-core feat/fleet-actions` e
  `git -C C:\gits\qgroundcontrol\.claude\worktrees\fleet-extras switch -c feat/fleet-actions-uitests feat/fleet-actions`.
  Disparar B, C e D (seção Briefs) no mesmo turno, em background.

### Task 1: Contrato — API pública, stubs e Fact do intervalo

**Files:**

- Modify: `src/Vehicle/FleetCommandController.h` (parte pública = bloco do Contrato + `kAttemptTimeoutMs` mantido)
- Modify: `src/Vehicle/FleetCommandController.cc` (stubs de `takeoffAll()` e `rebootAll()`)
- Modify: `src/Settings/FlyView.SettingsGroup.json`, `src/Settings/FlyViewSettings.h`, `src/Settings/FlyViewSettings.cc`

**Interfaces:**

- Produces: valores QML `FleetCommandController.Command.Takeoff`/`.Reboot`,
  `FleetCommandController.TakeoffStep.{None,Mode,Arm,Climb}`, propriedade `takeoffAltitude`, métodos `takeoffAll()` e
  `rebootAll()`; Fact `QGroundControl.settingsManager.flyViewSettings.fleetCommandIntervalSec` e, em C++,
  `SettingsManager::instance()->flyViewSettings()->fleetCommandIntervalSec()`.

- [ ] **Step 1: Header** — aplicar o bloco do Contrato à parte pública (sem mexer na seção `private:`).
- [ ] **Step 2: Stubs** — no `.cc`, `takeoffAll()` e `rebootAll()` só com
  `qCDebug(FleetCommandControllerLog) << "not implemented";`.
- [ ] **Step 3: Fact** — aplicar o JSON e as duas macros do Contrato.
- [ ] **Step 4: Build** — PS-ENV + `cmake --build C:\gits\qgroundcontrol\build\Windows` (background). Esperado:
  código 0.
- [ ] **Step 5: Commit** —
  `git add src/Vehicle/FleetCommandController.h src/Vehicle/FleetCommandController.cc src/Settings/FlyView.SettingsGroup.json src/Settings/FlyViewSettings.h src/Settings/FlyViewSettings.cc`;
  mensagem `feat(fleet): add takeoff, reboot and resend interval to the fleet command interface`.
- [ ] **Step 6:** executar Task 0, Step 3.

---

## Onda 1 — três agentes em paralelo

### Task B1 (agente B): RTL/Land com X envios a cada Y s

**Files:**

- Modify: `src/Vehicle/FleetCommandController.h` (seção `private:`; remover `kAttemptTimeoutMs`),
  `src/Vehicle/FleetCommandController.cc`
- Modify: `test/Vehicle/FleetCommandControllerTest.h`, `test/Vehicle/FleetCommandControllerTest.cc`

**Interfaces:**

- Consumes: Contrato; `SettingsManager::instance()->flyViewSettings()->{fleetCommandAttempts, fleetCommandIntervalSec}()`.
- Produces: regras G1–G6 e L1–L4; chave `confirmed` no `vehicleStatus`.

Testes (base atual do arquivo; manter helpers e o `ignoreLogMessage` do `init()`):

- `init()` também salva o Fact de intervalo em `_savedInterval` e deixa `_setAttempts(1); _setInterval(1);` (testes
  rápidos por padrão); `cleanup()` restaura os dois. Helper novo `void _setInterval(int seconds)` e
  `static int _intervalMs()` (lê o Fact × 1000). Helper novo `static bool _confirmed(controller, vehicle)` lendo a chave
  `confirmed`.

- [ ] **Step 1: Ajustar e escrever os testes**

  ```cpp
  // Ajustes nos existentes (trocar todo kAttemptTimeoutMs por _intervalMs()):
  // _testRetriesThenFails: _setAttempts(2); timeout do QTRY Failed = 3 * _intervalMs() + TestTimeout::shortMs();
  //   resto igual (attempt 2, maxAttempts 2, DO_SET_MODE == 2, !busy, finished 1x)
  // _testSucceedsWhenLinkReturns: _setAttempts(5); depois de religar o link:
  //   QTRY state Succeeded com timeout 6 * _intervalMs() + TestTimeout::longMs(); QCOMPARE(_attempt, 5)
  // _testLandOverridesRtl / _testCancelStopsResending: QVERIFY_NO_SIGNAL_WAIT(..., 2 * _intervalMs())
  // _testLandOverridesRtl: QTRY Succeeded com timeout 6 * _intervalMs() + TestTimeout::longMs()

  void _testSendsEveryAttemptAfterConfirmation();
  //   ArduCopter armado; _setAttempts(3); link->clearReceivedMavCommandCounts(); landAll()
  //   QTRY_VERIFY(_confirmed(c, v) && _state(c, v) == int(Pending))       // confirmado e ainda enviando
  //   QTRY_COMPARE(_state, int(Succeeded)) com timeout 4 * _intervalMs() + TestTimeout::shortMs()
  //   QCOMPARE(_attempt, 3); QCOMPARE(link->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 3)

  void _testReassertsTargetMode();
  //   ArduCopter armado; _ignoreUnansweredCommandNoise(); _setAttempts(3) (Y = 1 s do init); rtlAll()
  //   QTRY flightMode == rtlFlightMode (heartbeat do MockLink é 10 Hz: bem antes do envio 2, em t = 1 s)
  //   vehicle->setFlightMode(vehicle->pauseFlightMode())              // "piloto" tira do RTL
  //   QTRY_COMPARE_WITH_TIMEOUT(flightMode, pauseFlightMode, 500); QVERIFY(!_confirmed(c, v))
  //   QTRY state Succeeded (timeout 4 * _intervalMs() + longMs); QCOMPARE(flightMode, rtlFlightMode)
  //   (com a lógica antiga o comando já tinha terminado e o veículo ficaria em Brake)

  void _testSettingsSnapshotPerCommand();
  //   PX4 armado; _setAttempts(3); _setCommLost(link, true); rtlAll(); QTRY _attempt == 1
  //   _setAttempts(7); _setInterval(5)
  //   QCOMPARE(_maxAttempts, 3); QTRY_COMPARE_WITH_TIMEOUT(_attempt, 2, 1000 + TestTimeout::shortMs())  // Y continua 1 s

  void _testIntervalFromSettings();
  //   PX4 armado; _setAttempts(2); _setInterval(2); _setCommLost(link, true); rtlAll()
  //   QCOMPARE(_attempt, 1)
  //   QVERIFY2(!QTest::qWaitFor([&] { return _attempt(c, v) >= 2; }, 1500), "second send before the interval")
  //   QTRY_COMPARE_WITH_TIMEOUT(_attempt, 2, 1000 + TestTimeout::shortMs())
  ```

  Declarar os 4 testes novos em `private slots:` do `.h`, depois de `_testCancelStopsResending`.

- [ ] **Step 2: Build e ver falhar** — PS-ENV +
  `cmake --build C:\gits\qgroundcontrol\.claude\worktrees\fleet-core\build\Windows-debug --parallel 8` (background);
  depois bloco de execução com `$b` = esse build +
  `ctest --test-dir $b -R FleetCommandControllerTest --output-on-failure`. Esperado: compila; falham ao menos
  `_testSendsEveryAttemptAfterConfirmation`, `_testReassertsTargetMode` e `_testIntervalFromSettings`.
- [ ] **Step 3: Implementar L1–L4** — trocar o intervalo fixo pelo Y do Fact (`_attemptTimer->setInterval(Y * 1000)`
  no início do comando), guardar `confirmed` na entrada, aplicar L2–L4 e publicar `confirmed`/`step` (`None`) em
  `_publish()`. Remover `kAttemptTimeoutMs` do header.
- [ ] **Step 4: Ver passar** — mesmo comando do Step 2. Esperado: todos PASS.
- [ ] **Step 5: Commit** —
  `git add src/Vehicle/FleetCommandController.h src/Vehicle/FleetCommandController.cc test/Vehicle/FleetCommandControllerTest.h test/Vehicle/FleetCommandControllerTest.cc`;
  mensagem `feat(fleet): send RTL and Land the configured number of times at the configured interval`.

### Task B2 (agente B): TAKEOFF ALL

**Files:**

- Modify: `src/Vehicle/VehicleReadiness.h`, `src/Vehicle/FleetCommandController.h` (privado),
  `src/Vehicle/FleetCommandController.cc`
- Modify: `test/Vehicle/FleetCommandControllerTest.h`, `test/Vehicle/FleetCommandControllerTest.cc`

**Interfaces:**

- Consumes: `Vehicle::{readiness(), supports(), airborne(), armed(), flightMode(), setFlightMode(), setArmed(bool, bool),
  guidedModeTakeoff(double)}`, sinais `Vehicle::{flightModeChanged, armedChanged, airborneChanged}`.
- Produces: `QString VehicleReadiness::takeoffArmFlightMode() const` (público); regras T1–T5.

- [ ] **Step 1: Escrever os testes**

  ```cpp
  // _data: rows "PX4" (MAV_AUTOPILOT_PX4) e "ArduCopter" (MAV_AUTOPILOT_ARDUPILOTMEGA; QSKIP se !apmFirmwareSupported())
  void _testTakeoffAll();
  //   2 veículos DESARMADOS no mesmo autopiloto; para cada link: clearReceivedMavCommandCounts()
  //   takeoffAll(); para os 2:
  //     QTRY state Succeeded (timeout 2 * kTakeoffStepTimeoutMs + kTakeoffClimbTimeoutMs)
  //     QVERIFY(armed()); QVERIFY(airborne()); QCOMPARE(_step, int(TakeoffStep::Climb))
  //     QCOMPARE(flightMode(), readiness()->takeoffArmFlightMode())
  //     QCOMPARE(link->receivedMavCommandCount(MAV_CMD_NAV_TAKEOFF), 1)
  //   só ArduCopter: QTRY qAbs(altitudeRelative()->rawValue().toDouble() - kTakeoffAltitudeMeters) < 0.5
  //   !busy(); finished 1x com Command::Takeoff

  void _testTakeoffSkipsAirborneAndUnsupported();
  //   ArduCopter desarmado + ArduSub desarmado (sem guided takeoff); QSKIP se !apmFirmwareSupported()
  //   takeoffAll(); QTRY Copter Succeeded; ArduSub: Unsupported, attempt 0
  //   clearReceivedMavCommandCounts() do Copter; takeoffAll() de novo:
  //   Copter: Skipped; finished emitido dentro da chamada; NAV_TAKEOFF do Copter continua 0

  void _testTakeoffStepTimeoutFails();
  //   PX4 desarmado; clearReceivedMavCommandCounts()
  //   expectLogMessage("Vehicle.FleetCommandController", QtWarningMsg, "vehicle: <id> command: .*Takeoff failed at step .*Mode")
  //   _setCommLost(link, true); takeoffAll()
  //   QTRY state Failed (timeout kTakeoffStepTimeoutMs + TestTimeout::shortMs()); verifyExpectedLogMessage()
  //   _step == int(TakeoffStep::Mode); COMPONENT_ARM_DISARM == 0 e NAV_TAKEOFF == 0 no link

  void _testLandCancelsTakeoff();   // Review Focus 1
  //   ArduCopter desarmado; _ignoreUnansweredCommandNoise(); clearReceivedMavCommandCounts()
  //   _setCommLost(link, true); takeoffAll(); QTRY _step == int(TakeoffStep::Mode)
  //   landAll(); QCOMPARE(activeCommand, Land); QCOMPARE(_state, int(Skipped))   // desarmado → Land pula
  //   _setCommLost(link, false)
  //   QVERIFY2(!QTest::qWaitFor([&] { return link->receivedMavCommandCount(MAV_CMD_COMPONENT_ARM_DISARM) > 0
  //                                       || link->receivedMavCommandCount(MAV_CMD_NAV_TAKEOFF) > 0; },
  //                             kTakeoffStepTimeoutMs + 1000), "takeoff continued after Land")
  //   QVERIFY(!vehicle->armed())
  ```

  Helper novo `static int _step(controller, vehicle)` lendo a chave `step`.

- [ ] **Step 2: Build e ver falhar** — comandos do B1, Step 2. Esperado: os 4 testes novos falham (stub).
- [ ] **Step 3: `takeoffArmFlightMode()`** — em `VehicleReadiness.h`, seção `public:` depois de `status()`:
  `/// Mode the firmware plugin arms the vehicle in for a guided takeoff, empty when it does not switch modes` e
  `QString takeoffArmFlightMode() const { return _actionFlightModes(GuidedAction::Takeoff).armFlightMode; }`.
- [ ] **Step 4: Implementar T1–T5** — sugestão: `_startTakeoff()`, `_advanceTakeoff(quint32 id)`,
  `_takeoffStepTimedOut(quint32 id)`; na entrada, `TakeoffStep step`, `QTimer* stepTimer` (filho do controlador,
  apagado em `_releaseEntries()` e ao resolver) e `QList<QMetaObject::Connection> connections` (todas desfeitas em
  `_releaseEntries()`/`_resolve()`). Chamar `guidedModeTakeoff()` só quando o modo for `armMode` e `armed()` for
  verdadeiro (senão o plugin ArduPilot bloquearia a UI).
- [ ] **Step 5: Ver passar** — Esperado: todos PASS (os do B1 inclusive).
- [ ] **Step 6: Commit** —
  `git add src/Vehicle/VehicleReadiness.h src/Vehicle/FleetCommandController.h src/Vehicle/FleetCommandController.cc test/Vehicle/FleetCommandControllerTest.h test/Vehicle/FleetCommandControllerTest.cc`;
  mensagem `feat(fleet): take off every vehicle on the ground to 5 m`.

### Task B3 (agente B): REBOOT ALL

**Files:**

- Modify: `src/Vehicle/FleetCommandController.h` (privado), `src/Vehicle/FleetCommandController.cc`
- Modify: `test/Vehicle/FleetCommandControllerTest.h`, `test/Vehicle/FleetCommandControllerTest.cc`,
  `test/Vehicle/CMakeLists.txt` (só se a soma dos testes passar de 240 s: `TIMEOUT 300`)

**Interfaces:**

- Consumes: `Vehicle::rebootVehicle()`, `MultiVehicleManager::vehicleRemoved(Vehicle*)`.
- Produces: regras R1–R3.

- [ ] **Step 1: Escrever os testes**

  ```cpp
  void _testRebootAllIncludingArmed();
  //   PX4 v1 ARMADO e PX4 v2 desarmado; guardar as chaves (ids) antes
  //   rebootAll(); QCOMPARE(activeCommand, Reboot)
  //   QTRY MultiVehicleManager::instance()->vehicles()->count() == 0 (TestTimeout::longMs())
  //   pelas chaves: state Succeeded para as duas; vehicleStatus().size() == 2; !busy(); finished 1x com Reboot

  void _testRebootFailsWithoutAck();
  //   PX4 desarmado; _setCommLost(link, true)
  //   ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg, "Giving up sending command after max retries: MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN")
  //   expectAppMessage("Vehicle reboot failed.")                 // ~3,6 s: a fila desiste (3 x 1,2 s)
  //   expectLogMessage("Vehicle.FleetCommandController", QtWarningMsg, "vehicle: <id> command: .*Reboot failed: still connected after")  // 6 s
  //   rebootAll(); QTRY state Failed (timeout kRebootTimeoutMs + TestTimeout::shortMs())
  //   verifyExpectedLogMessage() duas vezes (ordem FIFO, na ordem acima); vehicles()->count() == 1
  ```

  Se aparecer outra mensagem de app/log não prevista na primeira execução (ex.: perda de comunicação), tratar com
  `expectAppMessage`/`ignoreLogMessage` usando o texto exato do log, e anotar no relatório.

- [ ] **Step 2: Build e ver falhar.**
- [ ] **Step 3: Implementar R1–R3** — `_startReboot()`; no `_vehicleRemoved`, o ramo de Reboot marca `Succeeded`,
  zera o ponteiro e **não** apaga a entrada; timer por entrada com `kRebootTimeoutMs`.
- [ ] **Step 4: Ver passar** — Esperado: todos PASS. Regressão:
  `ctest --test-dir $b -R "MultiVehicleManagerTest|VehicleReadiness" --output-on-failure` → PASS.
- [ ] **Step 5: Commit** — `git add` dos arquivos tocados; mensagem `feat(fleet): reboot every connected autopilot`.

### Task C1 (agente C): `FleetConfirmOverlay.qml`

**Files:**

- Create: `src/FlyView/FleetConfirmOverlay.qml`
- Modify: `src/FlyView/CMakeLists.txt` (lista de QML do módulo, ordem alfabética: antes de `FlyView.qml`)

**Interfaces:**

- Produces: a API do Contrato ("QML — `FleetConfirmOverlay.qml`").

- [ ] **Step 1: Implementar** conforme o Contrato. Estrutura de arquivo do `CODING_STYLE.md` (id, públicas, privadas,
  funções, propriedades, filhos). `Timer` interno para o `secondStepDelayMs`. Sem cores/tamanhos fixos.
- [ ] **Step 2: Build** — PS-ENV + `cmake --build C:\gits\qgroundcontrol\build\Windows` (background). Esperado: código
  0, sem aviso de QML novo no log do build.
- [ ] **Step 3: Lint** — LINT-ENV + `uv run --frozen --project tools --group dev prek run --files src/FlyView/FleetConfirmOverlay.qml src/FlyView/CMakeLists.txt`.
  Esperado: `Passed`/`Skipped`.
- [ ] **Step 4: Commit** — mensagem `feat(flyview): add a centered confirmation overlay for fleet commands`.

### Task C2 (agente C): painel, camada do Fly View, limpeza do Guided e página de configurações

**Files:**

- Modify: `src/FlyView/FlyViewFleetPanel.qml`, `src/FlyView/FlyViewWidgetLayer.qml`,
  `src/FlyView/GuidedActionsController.qml`, `src/AppSettings/pages/FlyView.SettingsUI.json`

**Interfaces:**

- Consumes: Contrato (C++, Facts, overlay). O controlador ainda é stub para Takeoff/Reboot nesta onda: validar só build,
  lint e ausência de avisos de QML.

- [ ] **Step 1: Painel** — aplicar a seção "QML — `FlyViewFleetPanel.qml`": novas linhas e botões, campos X/Y,
  `confirmOverlay`, textos de status por comando e `_commandName()` com os 4 comandos. LAND ALL e REBOOT ALL chamam
  `confirmOverlay.show(...)` (com checagem de nulo); RTL ALL e TAKEOFF ALL chamam o controlador direto.
- [ ] **Step 2: Camada** — em `FlyViewWidgetLayer.qml`, instanciar o `FleetConfirmOverlay` (Contrato) e passar
  `confirmOverlay: fleetConfirmOverlay` ao `FlyViewFleetPanel`.
- [ ] **Step 3: Guided** — em `GuidedActionsController.qml`, remover `fleetLandTitle`, `fleetLandMessage`,
  `actionFleetLand` e os dois `case actionFleetLand` (em `confirmAction` e `executeAction`). Conferir com
  `git grep -n "actionFleetLand\|fleetLand" -- src` → nenhum resultado.
- [ ] **Step 4: Configurações** — no grupo `"Fleet Panel"` de `FlyView.SettingsUI.json`, acrescentar
  `{ "setting": "flyViewSettings.fleetCommandIntervalSec" }` depois do de tentativas e `"interval"` em `keywords`.
- [ ] **Step 5: Build** — como no C1. Esperado: código 0.
- [ ] **Step 6: Lint** — LINT-ENV + prek nos 4 arquivos. Esperado: `Passed`/`Skipped`.
- [ ] **Step 7: Commit** —
  mensagem `feat(flyview): add takeoff, reboot and resend fields to the fleet panel and confirm Land and Reboot in the center`.

### Task D1 (agente D): testes de UI do painel

**Files:**

- Modify: `test/QmlUITests/FlyViewFleetPanelUITest.h`, `test/QmlUITests/FlyViewFleetPanelUITest.cc`,
  `test/QmlUITests/CMakeLists.txt` (só se precisar: `TIMEOUT 300`)

**Interfaces:**

- Consumes: `objectName`s, textos e API do overlay do Contrato; Facts de X/Y; `FleetCommandController` do painel via
  `findVisibleItem(_rootItem, "flyViewFleetPanel")->property("controller").value<FleetCommandController*>()`.

Base: o arquivo atual. Sobrescrever `init()`/`cleanup()` (chamando os da base) para salvar e restaurar
`fleetCommandAttempts` e `fleetCommandIntervalSec` e deixar `1`/`1` durante cada teste (status final em ~1 s). Atualizar
o comentário `///` da classe com o que passa a ser coberto.

- [ ] **Step 1: Ajustar os existentes**

  ```cpp
  // _testRtlAllSendsWithoutConfirmation: também QVERIFY(!findVisibleItem(_rootItem, "fleetConfirmOverlay", 0))
  // _testLandAllRequiresConfirmation → renomear para _testLandAllConfirmsInCenterOverlay:
  //   clicar fleetLandAllButton; overlay = findVisibleItem("fleetConfirmOverlay", mediumMs); card = "fleetConfirmCard"
  //   centro do card (fleetPanelSceneRect) a ≤ 1 px do centro do overlay, em x e em y
  //   texto de fleetConfirmTitle == "LAND ALL VEHICLES"; fleetConfirmMessage começa com "1 armed vehicles"
  //   nada enviado em TestTimeout::shortMs() (mesma verificação de hoje); clickButton("fleetConfirmAcceptButton")
  //   overlay some; QTRY flightMode == landFlightMode; QTRY status == "Land: confirmed"
  // _testActionButtonsStayPutWhileRetrying: medir também fleetTakeoffAllButton e fleetRebootAllButton antes/depois
  ```

- [ ] **Step 2: Escrever os novos**

  ```cpp
  void _testLandOverlayCancelSendsNothing();
  //   PX4 armado; clicar LAND ALL; clickButton("fleetConfirmCancelButton"); overlay some
  //   QVERIFY2(!QTest::qWaitFor(flightMode == landFlightMode, TestTimeout::shortMs()), ...); sem status do veículo

  void _testTakeoffAllSendsWithoutConfirmation();
  //   PX4 desarmado; clickButton("fleetTakeoffAllButton"); overlay nunca visível
  //   QTRY armed() && airborne() (timeout 2 * kTakeoffStepTimeoutMs + kTakeoffClimbTimeoutMs)
  //   QTRY status == "Takeoff: airborne"

  void _testRebootNeedsTwoConfirmations();   // Review Focus 5
  //   PX4 ARMADO; clicar REBOOT ALL; título "REBOOT ALL AUTOPILOTS"; fleetConfirmStep == "Step 1 of 2";
  //   mensagem contém "1 of them are ARMED"
  //   clicar aceitar → overlay continua; título "CONFIRM REBOOT"; "Step 2 of 2"; aceitar !isEnabled()
  //   clicar aceitar desabilitado → continua na etapa 2; activeVehicle() != nullptr
  //   QTRY aceitar isEnabled() (timeout 1500 + TestTimeout::shortMs()); clicar → overlay some
  //   QTRY MultiVehicleManager::instance()->activeVehicle() == nullptr (TestTimeout::longMs())

  void _testResendFieldsEditSettings();      // Review Focus 4
  //   campo = findVisibleItem("fleetAttemptsField"); setProperty("text", "5"); invokeMethod(campo, "editingFinished")
  //   QCOMPARE(fleetCommandAttempts rawValue, 5)
  //   "0" e "21" → rawValue continua 5; mesmo roteiro no fleetIntervalField com "7" (aceito), "0" e "61" (recusados)

  void _testConfirmOverlayClosesWhenFleetEmpty();
  //   PX4 armado; clicar LAND ALL; overlay = findVisibleItem("fleetConfirmOverlay") (guardar em QPointer)
  //   mockLink->disconnect()
  //   QTRY !overlay->isVisible() (TestTimeout::longMs()); overlay->property("currentStep").toInt() == 0
  ```

- [ ] **Step 3: Commit (sem build nesta onda)** —
  `git add test/QmlUITests/FlyViewFleetPanelUITest.h test/QmlUITests/FlyViewFleetPanelUITest.cc`; mensagem
  `test(flyview): cover the fleet confirmation overlay, takeoff, reboot and resend fields`. D será retomado na Task 2,
  Step 4, para compilar, rodar e corrigir.

---

## Onda 2 — integração, testes e distribuição

### Task 2: Integração (coordenador; retoma D no Step 4)

- [ ] **Step 1: Esperar** os commits de B1, B2, B3, C1, C2 e D1.
- [ ] **Step 2: Histórico linear** — no checkout principal (`feat/fleet-actions`, que já tem Onda 0 + C):
  `git cherry-pick $(git merge-base feat/fleet-actions feat/fleet-actions-core)..feat/fleet-actions-core` e depois
  `git cherry-pick $(git merge-base feat/fleet-actions feat/fleet-actions-uitests)..feat/fleet-actions-uitests`
  (D por último, para poder ser emendado). Esperado: sem conflitos; conflito = violação da tabela de posse, resolver e
  anotar no relatório.
- [ ] **Step 3: Build Debug integrado** —
  `git -C C:\gits\qgroundcontrol\.claude\worktrees\fleet-core checkout --detach feat/fleet-actions`; PS-ENV +
  `cmake --build C:\gits\qgroundcontrol\.claude\worktrees\fleet-core\build\Windows-debug --parallel 12` (background).
- [ ] **Step 4: Testes direcionados** —
  - PS-ENV + execução + `ctest --test-dir $b -R "FleetCommandControllerTest|MultiVehicleManagerTest|VehicleReadiness" --output-on-failure` → PASS.
  - RUN-UITEST para `FlyViewFleetPanelUITest` → `0 failed`. Também `FlyViewGuidedUnitsUITest` e
    `FlyViewOccluderUITest` (trocar o nome no `--unittest:`), que passam pela camada do Fly View.
  - Falha no `FlyViewFleetPanelUITest`: retomar D (SendMessage). D corrige **no worktree `fleet-core`** (destacado no
    topo da integração, cujo último commit é o D1), recompila, roda de novo e faz `git commit --amend --no-edit` ali; o
    coordenador traz com `git reset --keep <sha emendado>` no checkout principal.
  - Teste de layout (`_testFleetPanelLayoutWithMultiVehiclePanel`, `_testFleetPanelStaysBelowCameraControls`) falhando
    por sobreposição real: a correção é no layout, num commit `fix(flyview): …` (a lista de veículos encolhe primeiro;
    se não bastar, reduzir a altura mínima da lista). Não afrouxar o teste.
  - Falha em teste de B ou C: correção em commit próprio (`fix(fleet): …`/`fix(flyview): …`) explicando a causa.
- [ ] **Step 5: Lint** — no checkout principal, LINT-ENV +
  `$files = git diff --name-only master...HEAD; uv run --frozen --project tools --group dev prek run --files @files`.
  Esperado: `Passed`/`Skipped`. Correções num único commit `style: fix lint findings in fleet action files`.
- [ ] **Step 6: Suíte completa (background)** —
  `ctest --test-dir $b -L "Unit|Integration" -LE "Flaky|Network" -E "UITest$" --output-on-failure -j 4`. Falha fora das
  áreas tocadas: rodar o mesmo teste no build do `master` para mostrar que já falhava, ou corrigir se for causada por
  esta mudança.
- [ ] **Step 7: Conferência visual com MockLink** — `$b\Debug\QGroundControl.exe --allow-multiple` (com o bloco de
  execução), Application Settings → Mock Link → 3 veículos ArduCopter; verificar os itens 1–7 do checklist da Task 4.

### Task 3: Distribuição (coordenador; Windows ∥ Linux, ambos do PowerShell)

- [ ] **Step 1 (Windows, background):**
  `powershell -ExecutionPolicy Bypass -File C:\gits\qgroundcontrol\deploy\beswarm\dist-windows.ps1`.
  Esperado: código 0; `C:\gits\qgc-dist\BeSwarmVehicleGS\` substituída; ZIP e `.sha256` novos.
- [ ] **Step 2 (Linux, background, em paralelo):**
  `powershell -ExecutionPolicy Bypass -File C:\gits\qgroundcontrol\deploy\beswarm\dist-linux.ps1 -Ref feat/fleet-actions -Jobs 6`.
  Esperado: código 0; AppImage e `.sha256` novos.
- [ ] **Step 3: Conferir** — para o ZIP e a AppImage, `(Get-FileHash <arquivo>).Hash.ToLower()` igual ao 1º campo do
  `.sha256`; `Get-ChildItem C:\gits\qgc-dist` mostra os 4 artefatos BeSwarm com a data de hoje.

### Task 4: Entrega para revisão do usuário (sem push)

- [ ] **Step 1:** `git log --oneline master..feat/fleet-actions` (esperado: Onda 0, B1, B2, B3, C1, C2, D1, e os `fix`
  /`style` se houver) e `git status --short` limpo.
- [ ] **Step 2:** relatar artefatos, hashes, testes executados (e o que não foi executado) e o checklist manual:
  1. Painel expandido: de cima para baixo, lista, linha TAKEOFF ALL 5 m | REBOOT ALL, linha "Send [3] times, every
     [2 s]", linha RTL ALL | LAND ALL.
  2. "LAND ALL": fundo escurece, cartão vermelho piscando no centro da tela com "LAND ALL VEHICLES"; Cancel não envia
     nada; "LAND ALL" no cartão envia. O mapa não responde a cliques com o aviso aberto.
  3. "RTL ALL" com X = 5 e Y = 2 (editados no painel): cada veículo armado mostra "RTL: sent 1 of 5", "…, confirmed" e,
     ~10 s depois, "RTL: confirmed". Trocar o modo de um veículo à mão no meio: ele volta ao RTL no envio seguinte.
     "Stop" interrompe.
  4. Campos X/Y recusam 0, 21 (X) e 61 (Y); o valor aceito continua após reiniciar o app (é setting) e também aparece em
     Settings → Fly View → "Fleet Panel".
  5. "TAKEOFF ALL 5 m" (sem confirmação): os veículos no chão mostram "setting mode", "arming", "climbing to 5 m" e
     "airborne"; os que já voam, "skipped (airborne)".
  6. "LAND ALL" durante a decolagem: todos passam a Land/skipped; nenhum veículo arma depois.
  7. "REBOOT ALL": cartão laranja "REBOOT ALL AUTOPILOTS" (com "WARNING: N of them are ARMED." se houver armados),
     depois cartão vermelho "CONFIRM REBOOT" com o botão em "Wait" por 1,5 s; ao confirmar, os veículos somem e
     voltam; a linha mostra "Reboot: accepted".
  8. AppImage: `wsl.exe -d Ubuntu-26.04 -- /mnt/c/gits/qgc-dist/BeSwarmVehicleGS-linux-x86_64.AppImage --appimage-extract-and-run`
     abre a janela "BeSwarm Vehicle GS" com o painel novo.
- [ ] **Step 3:** perguntar se pode fazer merge de `feat/fleet-actions` no `master` e `git push`. Os branches
  `feat/fleet-actions-core` e `feat/fleet-actions-uitests` podem ser apagados depois, só com o OK do usuário.

---

## Briefs para disparar os agentes (copiar no prompt de cada um)

Cada agente recebe: este arquivo, `AGENTS.md`, `CODING_STYLE.md` e a instrução de ler o Contrato, as Global
Constraints e **só as suas tasks**; não editar arquivo fora da sua linha na tabela de posse; commitar no próprio branch;
não fazer push; ao terminar, responder com SHA(s), comandos de verificação executados e resultado. Não usar a opção
`isolation: "worktree"` do disparo de agentes: os worktrees já existem (Task 0).

- **B — Núcleo C++:** "Execute as Tasks B1, B2 e B3 do plano `docs/superpowers/plans/2026-11-23-beswarm-acoes-da-frota.md`
  no worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-core` (branch `feat/fleet-actions-core`), em TDD: testes
  primeiro, ver falhar, implementar, ver passar, um commit por task. Use o build Debug que já existe no worktree."
- **C — QML:** "Execute as Tasks C1 e C2 no checkout `C:\gits\qgroundcontrol` (branch `feat/fleet-actions`). Takeoff e
  Reboot ainda são stubs no controlador: valide build, lint e ausência de avisos de QML."
- **D — Testes de UI:** "Execute a Task D1 no worktree `C:\gits\qgroundcontrol\.claude\worktrees\fleet-extras`
  (branch `feat/fleet-actions-uitests`). Nesta onda só escreva e commite; você será retomado na Task 2, Step 4, para
  compilar, rodar na tela e corrigir."
