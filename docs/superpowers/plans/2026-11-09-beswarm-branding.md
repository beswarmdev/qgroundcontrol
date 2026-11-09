# BeSwarm Vehicle GS — Plano de rebranding da GUI

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tudo que o usuário vê como "QGroundControl"/"QGC" passa a mostrar "BeSwarm Vehicle GS", e todos os logos/ícones da
interface passam a ser derivados de `resources/icons/BESWARM.png`, sem mudar a identidade interna do app.

**Architecture:** Um nome de exibição separado (`QGC_APP_DISPLAY_NAME`) é aplicado via `QGuiApplication::setApplicationDisplayName`;
`QGC_APP_NAME` continua "QGroundControl" para que o executável, as configurações salvas e as pastas de dados não mudem. O QML
já lê o nome por `QGroundControl.appName`, que passa a retornar o nome de exibição. Os logos são trocados **no lugar** (mesmos
nomes de arquivo): cada SVG passa a embutir um PNG gerado a partir do BESWARM.png, então nenhuma referência em QML/CMake precisa mudar.

**Tech Stack:** Qt 6.12.0 (msvc2022_64), CMake + Ninja (preset `Windows`, pasta `build/Windows`), QML, Python 3 + Pillow (só para gerar os ícones).

**Spec:** o pedido do usuário nesta sessão: (1) trocar o nome exibido por "BeSwarm Vehicle GS"; (2) trocar ícones e logos por
`resources/icons/BESWARM.png`; (3) mexer somente na GUI; (4) gerar build para teste manual; (5) não executar automaticamente.

## Global Constraints

- Nome exibido exato: `BeSwarm Vehicle GS` (com esta grafia e estas maiúsculas).
- Logo de origem: `resources/icons/BESWARM.png` (732×650, RGBA, fundo branco **opaco**, quadrado arredondado `#1D1D1D` com pontos brancos).
- **Não mudar** (identidade interna, não é GUI): `QGC_APP_NAME`, `QGC_ORG_NAME`, `QGC_ORG_DOMAIN`, `QGC_PACKAGE_NAME`, o nome do
  executável (`QGroundControl.exe`), o namespace de configurações (registro/INI), a pasta de dados `Documentos\QGroundControl`,
  URIs de módulo QML (`import QGroundControl…`), nomes de classes/arquivos, categorias de log, comentários e cabeçalhos de licença,
  strings de protocolo/formato de arquivo (`"QGC WPL"`, `groundStation` do JSON, nome do documento KML, User-Agent, SDP,
  metadados de elementos GStreamer), arquivos em `docs/`.
- Não mexer na alteração local já existente em `cmake/CompilerWarnings.cmake` (`/EHsc`): ela não entra em nenhum commit deste plano.
- Estilo do repositório (CODING_STYLE.md): 4 espaços, LF, máx. 120 colunas; strings visíveis em QML sempre com `qsTr()`.
- Toda string QML que hoje diz "QGroundControl"/"QGC" passa a usar `qsTr("…%1…").arg(QGroundControl.appName)` (fonte única do nome).
  Em JSON (sem placeholders) usar o literal `BeSwarm Vehicle GS`.
- Commits só com autorização explícita do usuário; mensagens em Conventional Commits.

## Review Focus

1. **Ícone tingido (`QGCLogoWhite.svg`)**: renderizado pelo `ColoredSvgImageProvider` com `QSvgRenderer` em modo
   `Tiny12FeaturesOnly`. Esperado: silhueta branca/tingida no botão "Settings" do menu do logo e no ícone da página "General".
   Se aparecer vazio, aplicar o fallback da Task 4, passo 6.
2. **Interface em outro idioma**: as strings alteradas perdem a tradução existente e aparecem em inglês (nunca vazias e nunca com
   "QGroundControl"). Só a Task 6 (opcional) recupera o português.
3. **Configurações preservadas**: como `applicationName` não muda, links de comunicação e preferências existentes continuam
   lá depois da primeira execução do novo build.
4. **Cache de ícones do Windows**: o Explorer pode continuar mostrando o ícone antigo do `.exe`; barra de título e barra de
   tarefas precisam mostrar o novo. Não é bug do build.
5. **Marcador da estação no mapa**: com posição da GCS e sem heading aparece o logo BeSwarm (1,75× altura da fonte); com
   heading aparece a seta recolorida (Task 4, passo 4).

---

## Mapa de arquivos

| Área | Arquivos | Responsabilidade |
| --- | --- | --- |
| Nome de exibição | `cmake/CustomOptions.cmake`, `src/qgc_version.h.in`, `src/QGCApplication.cc`, `src/QmlControls/QGroundControlQmlGlobal.cc`, `cmake/platform/Windows.cmake`, `src/Utilities/Platform/Platform.cc` | Definir e aplicar "BeSwarm Vehicle GS" |
| Mensagens C++ | 13 arquivos listados na Task 2 | Trocar o nome nas mensagens mostradas em diálogos |
| Textos QML/JSON | 6 QML + 3 JSON listados na Task 3 | Trocar "QGroundControl"/"QGC" visível |
| Logos/ícones | `resources/QGCLogoFull.svg`, `resources/QGCLogoWhite.svg`, `resources/QGCLogoBlack.svg`, `resources/QGCLogoArrow.svg`, `resources/icons/qgroundcontrol.ico`, `resources/icons/qgroundcontrol.png`, `deploy/windows/WindowsQGC.ico`, `resources/icons/BESWARM.png` (novo, rastreado) | Conteúdo trocado, nomes mantidos |

Onde cada logo aparece hoje (nenhuma destas referências muda):

- `QGCLogoFull.svg`: botão do logo na toolbar (Fly e Plan: `src/Toolbar/FlyViewToolBar.qml:81`, `src/Toolbar/PlanViewToolBar.qml:41`,
  `src/MainWindow/MainWindow.qml:470`), renderizado por `VectorImage` (só aceita SVG; testado: renderiza `<image>` embutido);
  marcador da GCS no mapa (`src/FlightMap/FlightMap.qml:251`, `src/GeoMap/GeoMap.qml:588`).
- `QGCLogoWhite.svg` (tingido, usa só o canal alfa): botão "Settings" (`src/Toolbar/SelectViewDropdown.qml:83`), ícone da
  página "General" (`src/AppSettings/pages/SettingsPages.json:8`), ícone do título "Application Settings" (`src/MainWindow/MainWindow.qml:176`).
- `QGCLogoArrow.svg`: marcador da GCS com heading e `src/AutoPilotPlugins/APM/APMFollowComponent.qml:357`.
- `deploy/windows/WindowsQGC.ico`: ícone do `.exe`, da barra de título e da barra de tarefas no Windows (`QT_TARGET_RC_ICONS`).
- `resources/icons/qgroundcontrol.ico`: ícone de janela no Linux e instaladores CPack; `resources/icons/qgroundcontrol.png`: CPack.
- `resources/SplashScreen.png`: não é referenciado por nenhum QML (só alias no CMake) — **fica como está**.

---

### Task 0: Preparação

- [ ] **Step 1: Criar branch** — `git switch -c feat/beswarm-branding` a partir de `master`.
- [ ] **Step 2: Conferir estado** — `git status --short` deve mostrar só ` M cmake/CompilerWarnings.cmake` e `?? resources/icons/BESWARM.png`.
  Nos commits seguintes usar sempre `git add <arquivos da task>` (nunca `git add -A`), para não levar o `/EHsc`.

### Task 1: Nome de exibição "BeSwarm Vehicle GS"

**Files:**
- Modify: `cmake/CustomOptions.cmake:20-23` (logo após `QGC_APP_NAME`)
- Modify: `src/qgc_version.h.in`
- Modify: `src/QGCApplication.cc:99` (logo após `setApplicationName`)
- Modify: `src/QmlControls/QGroundControlQmlGlobal.cc:325-328`
- Modify: `cmake/platform/Windows.cmake:31,34`
- Modify: `src/Utilities/Platform/Platform.cc:439,450`

**Interfaces:**
- Produces: macro C++ `QGC_APP_DISPLAY_NAME` (via `qgc_version.h`); `QGuiApplication::applicationDisplayName()` == `"BeSwarm Vehicle GS"`;
  propriedade QML `QGroundControl.appName` == `"BeSwarm Vehicle GS"`. Tasks 2 e 3 dependem disso.

- [ ] **Step 1: Variável CMake** — em `cmake/CustomOptions.cmake`, depois do bloco `QGC_APP_NAME`:
  `set(QGC_APP_DISPLAY_NAME "BeSwarm Vehicle GS" CACHE STRING "User-visible application name (window title and UI text)")`.
- [ ] **Step 2: Header gerado** — em `src/qgc_version.h.in`, abaixo de `QGC_APP_NAME`: `#define QGC_APP_DISPLAY_NAME "@QGC_APP_DISPLAY_NAME@"`.
- [ ] **Step 3: Aplicar no app** — em `QGCApplication::QGCApplication`, logo após `setApplicationName(applicationName);`:
  `setApplicationDisplayName(QStringLiteral(QGC_APP_DISPLAY_NAME));`.
  Efeito: a janela principal (sem `title` no QML) passa a ter título "BeSwarm Vehicle GS" (`QPlatformWindow::formatWindowTitle`).
- [ ] **Step 4: Expor ao QML** — `QGroundControlQmlGlobal::appName()` passa a retornar `QGuiApplication::applicationDisplayName()`
  (adicionar `#include <QtGui/QGuiApplication>` se ainda não estiver incluído). Isso atualiza sozinho:
  `SelectViewDropdown.qml:135` ("%1 Version"), `FirmwareUpgrade.qml:32`, `VehicleConfigView.qml:299`, `ESCCalibrationDialog.qml:23,29`.
- [ ] **Step 5: Metadados do .exe (Gerenciador de Tarefas / Propriedades)** — em `cmake/platform/Windows.cmake`,
  `QT_TARGET_DESCRIPTION` e `QT_TARGET_PRODUCT_NAME` passam de `"${CMAKE_PROJECT_NAME}"` para `"${QGC_APP_DISPLAY_NAME}"`.
  `QT_TARGET_COMPANY_NAME` continua `QGC_ORG_NAME`.
- [ ] **Step 6: Diálogos de inicialização** — em `Platform.cc`, `showRootError` e `showMultipleInstanceError` passam a usar
  `QLatin1String(QGC_APP_DISPLAY_NAME)` no `.arg(...)`. **Não** alterar `runguardString` (linha 476): ele precisa continuar em
  `QGC_APP_NAME` para que a trava de instância única siga funcionando.
- [ ] **Step 7: Build incremental** — rodar o comando da Task 7, Step 1. Esperado: sucesso, e
  `grep QGC_APP_DISPLAY_NAME build/Windows/qgc_version.h` mostra `#define QGC_APP_DISPLAY_NAME "BeSwarm Vehicle GS"`.
  Observação: por ser `CACHE`, se no futuro o valor padrão mudar, é preciso `-DQGC_APP_DISPLAY_NAME=…` ou apagar a entrada do cache.
- [ ] **Step 8: Commit (com autorização)** — `feat(branding): show BeSwarm Vehicle GS as the application display name`.

### Task 2: Mensagens geradas em C++ (somente texto exibido)

> Esta é a única task que toca vários `.cc`, e só troca o nome que aparece em diálogos de mensagem. Se você quiser zero
> mudanças em C++ além da Task 1, pule-a: essas mensagens continuam dizendo "QGroundControl Daily".

**Files (trocar `applicationName()` por `applicationDisplayName()` — mesmo objeto, só o nome exibido muda):**
- `src/Comms/LinkInterface.cc:215`, `src/FactSystem/ParameterManager.cc:1456,1512,1552`,
  `src/MissionManager/CorridorScanComplexItem.cc:113`, `src/MissionManager/LandingComplexItem.cc:768`,
  `src/MissionManager/MissionController.cc:813`, `src/MissionManager/StructureScanComplexItem.cc:270`,
  `src/MissionManager/SurveyComplexItem.cc:257,312`, `src/QGCApplication.cc:356,432`,
  `src/Vehicle/MultiVehicleManager.cc:116`, `src/Utilities/QGCVersionCheck.cc:120`.
- Regra: `qgcApp()->applicationName()` → `qgcApp()->applicationDisplayName()`; `QCoreApplication::applicationName()` →
  `QGuiApplication::applicationDisplayName()` (+ `#include <QtGui/QGuiApplication>` se faltar); em `QGCApplication.cc`, `applicationName()` → `applicationDisplayName()`.
- **Não mudar** (caminhos/protocolo): `src/Settings/AppSettings.cc`, `src/Android/AndroidInterface.cc`,
  `src/QmlControls/QGCFileDialogController.cc:134`, `src/MissionManager/KMLPlanDomDocument.cc:12`,
  `src/Utilities/Network/Client/QGCNetworkClient.cc:38`, `src/VideoManager/Receiver/VideoReceiver.cc:21`.

**Literais com o nome (texto novo exato):**
- `src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc:743` →
  `tr("%1 supports PX4 Pro firmware Version %2.%3.%4 and above. You are using a version prior to that which will lead to unpredictable results. Please upgrade your firmware.")`
  com `.arg(QGuiApplication::applicationDisplayName()).arg(supportedMajorVersion).arg(supportedMinorVersion).arg(supportedPatchVersion)`.
- `src/GPS/Receiver/GPSReceiverMessages.cc:88` → `"the receiver's flash. To let %3 save the %2 receiver's settings, "` e
  `.arg(detail, gpsReceiverName(family), QGuiApplication::applicationDisplayName())`; comentário do tradutor (linha 85) ganha
  `; %3 is the application name`.
- `src/QmlControls/ParameterEditorController.cc:719` →
  `tr("No valid parameters found in file. Check that the file is in %1 or Mission Planner format.").arg(QGuiApplication::applicationDisplayName())`.

- [ ] **Step 1: Aplicar as trocas acima.**
- [ ] **Step 2: Conferir que nada visível ficou para trás** — `git grep -nP "(tr|translate)\([^;]*\"[^\"]*\b(QGroundControl|QGC)\b" -- 'src/*.cc' 'src/*.h'`
  deve retornar vazio.
- [ ] **Step 3: Build incremental** (Task 7, Step 1). Esperado: sucesso.
- [ ] **Step 4: Commit (com autorização)** — `feat(branding): use the display name in user-facing C++ messages`.

### Task 3: Textos QML e JSON visíveis

**Files / texto novo (todas as QML já importam `QGroundControl`):**
- `src/AppSettings/HelpSettings.qml`
  - `:27` → `qsTr("%1 User Guide").arg(QGroundControl.appName)`; `:48` → `qsTr("%1 Discord Channel").arg(QGroundControl.appName)`.
  - `:30,:37,:44,:51` — o texto visível do link (hoje é a própria URL com "qgroundcontrol") vira `qsTr("Open link")`;
    o `href` **não muda**: `"<a href=\"https://docs.qgroundcontrol.com\">" + qsTr("Open link") + "</a>"` (idem nos outros três).
- `src/AnalyzeView/VideoRecordings/VideoRecordingsPage.qml:17` →
  `qsTr("Play the videos %1 recorded, with the telemetry recorded alongside them.").arg(QGroundControl.appName)` (o comentário da linha 12 fica).
- `src/AutoPilotPlugins/Common/ESP8266Component.qml`: `:244` → `qsTr("%1 UDP Port")`, `:280` → `qsTr("Bridge/%1 Link")`,
  `:284` → `qsTr("%1/Bridge Link")`, `:437` → `…computer WiFi settings and %1 link settings to match…` — todos com `.arg(QGroundControl.appName)`.
- `src/FlyView/PreFlightSoundCheck.qml:8,9` → `"QGC audio output…"` vira `"%1 audio output…"` + `.arg(QGroundControl.appName)`.
- `src/Vehicle/VehicleSetup/JoystickComponentButtons.qml:247` → `qsTr("%1 functions do not support shift actions").arg(QGroundControl.appName)`.
- `src/GPS/Qml/GPSReceiverSettings.qml:95,96,144,305` → cada "QGroundControl" vira `%1` + `.arg(QGroundControl.appName)`
  (na 305 o `.arg` vai no `qsTr(...)`, antes da concatenação com `root._quectelOnly`).
- JSON (literal, sem placeholder):
  - `src/Settings/Mavlink.SettingsGroup.json:70` → `…inform vehicles that BeSwarm Vehicle GS is connected.`
  - `src/Settings/RTK.SettingsGroup.json:25,35,74` → "QGroundControl" → "BeSwarm Vehicle GS".
  - `src/Settings/Video.SettingsGroup.json:112` → `…when the total size of video recorded by BeSwarm Vehicle GS exceeds…`;
    `:219` → `BeSwarm Vehicle GS normally has the video sink ask…`.
- Fica de fora: `src/AppSettings/QmlTest.qml` (página de teste de paleta, só para desenvolvedores).

- [ ] **Step 1: Aplicar as trocas acima.**
- [ ] **Step 2: Varredura** — com `LC_ALL=C.UTF-8`:
  `git grep -nP "qsTr\([^)]*\b(QGroundControl|QGC)\b" -- 'src/*.qml'` deve listar só `QmlTest.qml`, e
  `git grep -nP "\"(shortDesc|longDesc)\"\s*:\s*\"[^\"]*(QGroundControl|\bQGC\b)" -- 'src/Settings/*.json'` deve retornar vazio.
- [ ] **Step 3: Build incremental** (Task 7, Step 1). Esperado: sucesso, sem novos avisos do `qmlcachegen`/`qmllint` nesses arquivos.
- [ ] **Step 4: Commit (com autorização)** — `feat(branding): show BeSwarm Vehicle GS in QML and settings text`.

### Task 4: Logos e ícones a partir do BESWARM.png

**Files:**
- Add (rastrear): `resources/icons/BESWARM.png` (fonte; não entra no `.qrc`)
- Replace content: `resources/QGCLogoFull.svg`, `resources/QGCLogoWhite.svg`, `resources/QGCLogoBlack.svg`,
  `resources/icons/qgroundcontrol.ico`, `resources/icons/qgroundcontrol.png`, `deploy/windows/WindowsQGC.ico`
- Modify: `resources/QGCLogoArrow.svg` (só cores)
- Gerador (fora do repo): `make_beswarm_assets.py` do Apêndice A, salvo em uma pasta temporária (ex.: o scratchpad da sessão).

**Interfaces:**
- Produces: mesmos caminhos e nomes de antes → nenhuma mudança em QML, JSON ou CMake.

- [ ] **Step 1: Gerar os arquivos** — `python <tmp>/make_beswarm_assets.py C:/gits/qgroundcontrol <tmp>/out`.
  Saída esperada: os 6 arquivos acima em `<tmp>/out/...` + `preview_full.png` e `preview_white.png`.
  O script: remove o fundo branco externo por flood-fill a partir das bordas (os pontos brancos internos ficam opacos), recorta,
  centraliza num quadrado transparente; `QGCLogoFull.svg` embute o PNG colorido 256×256 (`viewBox 0 0 72 72`);
  `QGCLogoWhite/Black.svg` embutem uma **máscara** (quadrado opaco, pontos vazados) porque esses ícones são tingidos pelo alfa
  (`viewBox 0 0 215 215`, igual ao original); `.ico` com 16, 20, 24, 32, 40, 48, 64, 128 e 256 px; `.png` 128×128.
- [ ] **Step 2: Conferir as prévias** — abrir `preview_full.png` (logo escuro com pontos brancos sobre fundo verde, sem borda
  branca) e `preview_white.png` (silhueta branca com os pontos vazados mostrando o verde). Já validado uma vez durante o planejamento.
- [ ] **Step 3: Copiar para o repo** — os 6 arquivos de `<tmp>/out` para os mesmos caminhos relativos em `C:/gits/qgroundcontrol`.
- [ ] **Step 4: Recolorir a seta** — em `resources/QGCLogoArrow.svg`, trocar todas as ocorrências de `#4b2c6d` por `#1d1d1d` e
  de `#4e005b` por `#3a3a3a` (os estilos inline sobrescrevem as classes `.st*`; branco `#fff`/`#f9f9f9` fica).
- [ ] **Step 5: Validar o SVG no pipeline do VectorImage** —
  `C:/Qt/6.12.0/msvc2022_64/bin/svgtoqml.exe resources/QGCLogoFull.svg <tmp>/full.qml` e confirmar que `<tmp>/full.qml` contém `Image {`.
- [ ] **Step 6: Fallback (só se o Review Focus 1 falhar no teste manual)** — salvar a máscara branca como
  `resources/BeSwarmLogoWhite.png`, adicioná-la a `QGC_APP_RESOURCES` em `CMakeLists.txt:338-354` e apontar as três referências
  de `QGCLogoWhite` (`SettingsPages.json:8`, `MainWindow.qml:176`, `SelectViewDropdown.qml:83`) para `/res/BeSwarmLogoWhite.png`
  (o provider aceita raster).
- [ ] **Step 7: Build incremental** (Task 7, Step 1). Esperado: sucesso; `rcc` reempacota os recursos alterados.
- [ ] **Step 8: Commit (com autorização)** — `feat(branding): replace QGC logos and icons with the BeSwarm logo`.

### Task 5 (opcional, não necessária para testar no Windows): ícones de outras plataformas e instalador

Fazer só se for distribuir para outras plataformas. Gerar a partir do mesmo `isolate_logo()` do Apêndice A:
`deploy/linux/QGroundControl_256.png` (256×256), `deploy/linux/QGroundControl.svg` (cópia de `QGCLogoFull.svg`),
`deploy/macos/qgroundcontrol.icns`, `android/res/mipmap-*/ic_launcher.png`, `android/res/drawable-*/icon.png`,
`android/res/drawable/ic_launcher_foreground.xml` (vetor; precisaria redesenho), `deploy/ios/Images.xcassets/AppIcon.appiconset/*`,
`resources/icons/android_*.png` e `deploy/windows/installheader.bmp` (150×57, cabeçalho do instalador NSIS).
Nomes de instalador/atalhos do Menu Iniciar (vêm de `CMAKE_PROJECT_NAME`) ficam fora, pois mudá-los muda a identidade do pacote.

### Task 6 (opcional): manter a tradução em português

Só vale se você usa a interface em português. Em `translations/qgc_source_pt_PT.ts`, para cada `<source>` alterado nas Tasks 2 e 3,
atualizar o `<source>` para o novo texto em inglês e, no `<translation>`, trocar "QGroundControl"/"QGC" pelo mesmo placeholder `%N`
do `<source>`; fazer o mesmo em `translations/qgc_json_pt_PT.ts` para as strings JSON (com o literal "BeSwarm Vehicle GS").
Os demais idiomas mostram essas poucas strings em inglês. Build + commit `feat(branding): update Portuguese strings for the new name`.

### Task 7: Build para teste manual

- [ ] **Step 1: Build** (PowerShell, reaproveita a configuração existente em `build/Windows`; o CMake reconfigura sozinho):

  ```powershell
  cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake --build C:\gits\qgroundcontrol\build\Windows'
  ```

  Esperado: termina com o link de `QGroundControl.exe` e código de saída 0. Rodar em background (pode passar de 2 min).
- [ ] **Step 2: Conferir artefatos** — `build/Windows/qgc_version.h` contém `QGC_APP_DISPLAY_NAME "BeSwarm Vehicle GS"`;
  `build/Windows/QGroundControl_resource.rc` contém `BeSwarm Vehicle GS`; `build/Windows/Release/QGroundControl.exe` com data atual.
- [ ] **Step 3: Lint** — `just`, `uv` e `prek` não estão no PATH desta máquina, então `just lint` não roda aqui. Se instalar
  (ver `tools/README.md`), rodar `just lint`; caso contrário o CI cobre. Testes unitários estão desligados neste preset
  (`QGC_BUILD_TESTING=OFF`), e nenhuma task altera lógica testável.
- [ ] **Step 4: Entregar para o usuário testar** — executável: `C:\gits\qgroundcontrol\build\Windows\Release\QGroundControl.exe`
  (o nome do arquivo não muda, por decisão). Checklist manual:
  1. Barra de título: "BeSwarm Vehicle GS". Ícone da janela e da barra de tarefas: logo BeSwarm.
  2. Toolbar (Fly e Plan): logo BeSwarm no canto superior esquerdo, nítido.
  3. Clicar no logo: botão "Settings" com a silhueta BeSwarm; rodapé "BeSwarm Vehicle GS Version".
  4. Application Settings: ícone da página "General"; página Help com "BeSwarm Vehicle GS User Guide" e links "Open link".
  5. Vehicle Configuration → Firmware: "BeSwarm Vehicle GS can upgrade the firmware…".
  6. Abrir uma segunda instância: "A second instance of BeSwarm Vehicle GS is already running…".
  7. Gerenciador de Tarefas: processo aparece como "BeSwarm Vehicle GS".
  8. Configurações e links salvos antes continuam lá.
  9. Mapa com posição da GCS: logo BeSwarm (sem heading) ou seta escura (com heading).

---

## Decisões a confirmar antes de executar

1. **Página Help** (padrão do plano: rótulos com o nome novo, texto dos links vira "Open link", URLs continuam do QGroundControl).
   Alternativa: você fornece URLs do BeSwarm e elas substituem os `href`.
2. **Abreviação "QGC"** em rótulos curtos (ESP8266, PreFlight, Joystick) — padrão: nome completo "BeSwarm Vehicle GS".
   Alternativa: um nome curto (ex.: "BeSwarm GS") só nesses rótulos.
3. **Task 2 (mensagens C++)** — incluída por padrão; pule se quiser restringir o C++ à Task 1.
4. **Tasks 5 e 6** — opcionais; não incluídas por padrão.

## Apêndice A — `make_beswarm_assets.py` (validado durante o planejamento)

```python
"""Derive BeSwarm GUI assets from resources/icons/BESWARM.png.

Usage: python make_beswarm_assets.py <repo_root> <out_root>
Writes files under <out_root> using the same relative paths as the repo.
"""

import base64
import io
import sys
from collections import deque
from pathlib import Path

from PIL import Image

DARK = (29, 29, 29)  # logo square colour (#1D1D1D)
BG_THRESHOLD = 40  # luminance above this, connected to the border, is background


def luminance(rgb):
    r, g, b = rgb[:3]
    return (299 * r + 587 * g + 114 * b) // 1000


def darkness_alpha(lum):
    return max(0, min(255, (255 - lum) * 255 // (255 - DARK[0])))


def isolate_logo(src):
    """Opaque white background -> transparent; keeps the white dots inside the square."""
    im = src.convert("RGBA")
    w, h = im.size
    px = im.load()
    seen = bytearray(w * h)
    queue = deque((x, y) for x in range(w) for y in (0, h - 1))
    queue.extend((x, y) for x in (0, w - 1) for y in range(h))
    while queue:
        x, y = queue.popleft()
        if not (0 <= x < w and 0 <= y < h) or seen[y * w + x]:
            continue
        seen[y * w + x] = 1
        lum = luminance(px[x, y])
        if lum <= BG_THRESHOLD:
            continue
        px[x, y] = (*DARK, darkness_alpha(lum))
        queue.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    im = im.crop(im.getbbox())
    side = max(im.size)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(im, ((side - im.width) // 2, (side - im.height) // 2))
    return square


def mask(logo, rgb):
    """Single-colour silhouette: square opaque, dots and background transparent (for tinting)."""
    out = Image.new("RGBA", logo.size)
    src, dst = logo.load(), out.load()
    for y in range(logo.height):
        for x in range(logo.width):
            r, g, b, a = src[x, y]
            dst[x, y] = (*rgb, a * darkness_alpha(luminance((r, g, b))) // 255)
    return out


def png_bytes(im, size):
    buf = io.BytesIO()
    im.resize((size, size), Image.LANCZOS).save(buf, "PNG", optimize=True)
    return buf.getvalue()


def svg_wrapper(png, view):
    data = base64.b64encode(png).decode()
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
        f'width="{view}" height="{view}" viewBox="0 0 {view} {view}">\n'
        f'    <image x="0" y="0" width="{view}" height="{view}" xlink:href="data:image/png;base64,{data}"/>\n'
        "</svg>\n"
    )


def main(repo, out):
    logo = isolate_logo(Image.open(repo / "resources/icons/BESWARM.png"))
    white, black = mask(logo, (255, 255, 255)), mask(logo, (0, 0, 0))
    files = {
        "resources/QGCLogoFull.svg": svg_wrapper(png_bytes(logo, 256), 72),
        "resources/QGCLogoWhite.svg": svg_wrapper(png_bytes(white, 256), 215),
        "resources/QGCLogoBlack.svg": svg_wrapper(png_bytes(black, 256), 215),
    }
    for rel, text in files.items():
        (out / rel).parent.mkdir(parents=True, exist_ok=True)
        (out / rel).write_bytes(text.encode("utf-8"))
    ico_sizes = [(s, s) for s in (16, 20, 24, 32, 40, 48, 64, 128, 256)]
    for rel in ("resources/icons/qgroundcontrol.ico", "deploy/windows/WindowsQGC.ico"):
        (out / rel).parent.mkdir(parents=True, exist_ok=True)
        logo.resize((256, 256), Image.LANCZOS).save(out / rel, sizes=ico_sizes)
    (out / "resources/icons/qgroundcontrol.png").write_bytes(png_bytes(logo, 128))
    # Previews only (not copied into the repo)
    for name, im in (("preview_full.png", logo), ("preview_white.png", white)):
        bg = Image.new("RGBA", im.size, (90, 140, 90, 255))
        bg.alpha_composite(im)
        bg.resize((200, 200), Image.LANCZOS).save(out / name)


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
```
