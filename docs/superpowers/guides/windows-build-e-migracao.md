# QGroundControl no Windows: compilar, executar e migrar

Guia prático para compilar o QGroundControl (QGC) 5.2.0-dev no Windows 11 x64, executar o resultado e levar o código ou o aplicativo pronto para outro computador.

Os comandos e as versões abaixo são os que produziram um build funcional nesta máquina (Intel i7-10700, 16 threads, 48 GB de RAM).
Trechos marcados com _(procedimento documentado pelo repositório, não executado nesta máquina)_ seguem a documentação e os scripts do próprio repositório, mas não foram testados aqui.

## 1. Resumo rápido

- **Código:** QGC upstream (<https://github.com/mavlink/qgroundcontrol>) no commit `6eb58f084` do `master`. O branch local `worktree-win-rebuild` só acrescenta este guia; não há nenhum patch de código para levar junto.
- **Ferramentas:** Visual Studio 2022 Build Tools (MSVC 14.44), Qt 6.12.0 kit `msvc2022_64`, CMake 4.4.4, Ninja 1.13.2, Git e Python 3.10+ com uv (para o venv `tools\.venv`).
- **Regra de ouro:** entre no shell de desenvolvedor do Visual Studio **antes** do primeiro `cmake --preset Windows`. Um configure feito fora dele grava flags vazias no cache (erro C4530 e Release sem otimização).
- **Venv de ferramentas:** o configure precisa de `tools\.venv`, que não vem no clone. Em um segundo checkout ou worktree desta máquina, aponte `QGC_PYTHON_ENV` para o venv que já existe.
- **Comandos:** bloco logo abaixo (PowerShell, na raiz do repositório).
- **Resultado:** `build\Windows\Release\QGroundControl.exe`. O build **não** copia as DLLs do Qt e do GStreamer para essa pasta: para rodar dali, ponha o `bin` do Qt e o do SDK do GStreamer no `PATH` (seção 3.5); para uma pasta autossuficiente, use `cmake --install` (seção 5). O build do zero levou 15,5 minutos nesta máquina (166 s de configure).
- **Dependências:** baixadas pelo CPM durante o configure para `.cache\CPM` (cerca de 4,1 GB, incluindo o SDK do GStreamer 1.28.8 para MSVC).
- **Outro computador:** leve o código via Git e recompile (seção 4), nunca a pasta `build\`; para só usar o app, gere um ZIP portátil (seção 5).

```powershell
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -DevCmdArguments '-arch=x64 -host_arch=x64' -SkipAutomaticLocation
Set-Location C:\gits\qgroundcontrol            # raiz do clone
$env:QT_ROOT_DIR = 'C:\Qt\6.12.0\msvc2022_64'
# tools\.venv precisa existir (seção 2.6); num segundo checkout desta máquina, use:
# $env:QGC_PYTHON_ENV = 'C:\gits\qgroundcontrol\tools\.venv'
cmake --preset Windows --fresh                 # --fresh descarta um cache antigo ou envenenado (seção 3.1)
cmake --build --preset Windows
$gst = (Select-String -Path .\build\Windows\CMakeCache.txt -Pattern '^QGC_GST_RESOLVED_ROOT:INTERNAL=(.*)$').Matches[0].Groups[1].Value -replace '/', '\'
if (-not $gst) { throw 'QGC_GST_RESOLVED_ROOT não encontrado em build\Windows\CMakeCache.txt' }
$env:Path = "$env:QT_ROOT_DIR\bin;$gst\bin;$env:Path"
.\build\Windows\Release\QGroundControl.exe
```

## 2. Pré-requisitos

### 2.1 Versões

| Componente | Nesta máquina | Observação |
| --- | --- | --- |
| Sistema operacional | Windows 11 x64 | Windows 10 ou 11 x64 |
| Compilador | Visual Studio 2022 Build Tools 17.14, MSVC 14.44.35207 | workload "C++ build tools" (`Microsoft.VisualStudio.Workload.VCTools`); o Visual Studio 2022 Community com "Desktop development with C++" também serve |
| Qt | 6.12.0, kit `msvc2022_64` em `C:\Qt\6.12.0\msvc2022_64` | única versão suportada; módulos em `qt.modules` de `.github/build-config.json` |
| CMake | 4.4.4 (winget) | mínimo 3.28; o CI fixa 4.4.2 |
| Ninja | 1.13.2 (winget) | o CI fixa 1.13.2 |
| Git | Git for Windows | com `core.longpaths=true` |
| Python | 3.9 (`C:\Program Files\Python39`), 3.12 (python.org) e o venv `tools\.venv` com 3.10 | o configure precisa de Python 3.10+ no venv `tools\.venv` |
| uv | não está no PATH | necessário para criar `tools\.venv` numa máquina nova |
| NSIS | não instalado | opcional, só para gerar o instalador `.exe` |
| just, prek, ccache | não instalados | opcionais; nenhum comando deste guia depende deles |

### 2.2 Ferramentas de linha de comando (winget)

```powershell
winget install --id Git.Git -e
winget install --id Kitware.CMake -e
winget install --id Ninja-build.Ninja -e
winget install --id Python.Python.3.12 -e
winget install --id astral-sh.uv -e
```

Feche e reabra o terminal depois da instalação para o `PATH` ser atualizado.
Confira com `git --version`, `cmake --version` (3.28 ou mais novo), `ninja --version`, `py -3.12 --version` e `uv --version`.

### 2.3 Visual Studio 2022 Build Tools

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Alternativa com o script do repositório, em um PowerShell como Administrador, na raiz do clone _(procedimento documentado pelo repositório, não executado nesta máquina)_:

```powershell
py -3.12 tools\setup\install_dependencies --platform windows --msvc --skip-gstreamer
```

O `--skip-gstreamer` evita instalar o GStreamer no sistema; o configure então baixa o próprio SDK (cerca de 540 MB)
para o cache do CPM. Sem a opção, o script instala o SDK em `C:\gstreamer\1.0\msvc_x86_64`, e o configure
(`cmake/GStreamer/platform/Windows.cmake`) usa um SDK instalado quando ele está completo e é 1.28.8 ou mais novo,
encontrado por `GSTREAMER_1_0_ROOT_MSVC_X86_64`/`GSTREAMER_1_0_ROOT_X86_64` ou nesse local padrão; isso evita o download.

Para descobrir onde o Visual Studio foi instalado (o caminho vai na variável `$vs` da seção 3):

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
```

Nesta máquina o resultado é `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`.

### 2.4 Qt 6.12.0 (kit msvc2022_64)

Use **somente** o Qt 6.12.0; o repositório não suporta outras versões.

Opção 1, Qt Online Installer (<https://www.qt.io/download-qt-installer-oss>, pede login com conta Qt):

1. Em _Installation Folder_, escolha _Custom Installation_ e a pasta `C:\Qt`.
2. Em _Select Components_, abra _Qt 6.12.0_ e marque _MSVC 2022 64-bit_ (se a versão não aparecer, marque _Archive_ e clique em _Filter_).
3. Marque todas as _Additional Libraries_ e desmarque o Qt Design Studio.

Opção 2, script do repositório, que instala a versão e os módulos exatos do CI _(procedimento documentado pelo repositório, não executado nesta máquina)_. Ele precisa do `cmake` no `PATH` para extrair os pacotes:

```powershell
py -3.12 tools\setup\install_qt.py install --from-config --host windows --arch win64_msvc2022_64 --outdir C:\Qt
```

O resultado esperado é `C:\Qt\6.12.0\msvc2022_64`.
Os módulos exigidos (`qt.modules` em `.github/build-config.json`) são: `qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors qtscxml qtwebsockets qthttpserver`.

### 2.5 Caminhos longos

```powershell
# PowerShell como Administrador
New-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1 -PropertyType DWord -Force

# PowerShell normal (antes de clonar)
git config --global core.longpaths true
```

Mantenha o repositório num caminho curto e sem espaços, por exemplo `C:\gits\qgroundcontrol`.

### 2.6 Venv de ferramentas Python (`tools\.venv`)

O configure (`cmake/modules/PythonVenv.cmake`) precisa do venv `<repo>\tools\.venv` com as dependências dos geradores de código.
Esse venv está no `.gitignore`, então **não** vem com clone, worktree, bundle ou fork.

Quando o venv não existe, o CMake tenta criá-lo sozinho:

- procura `python3` em **todo** o `PATH` antes de procurar `python`, e exige que ele seja 3.10 ou mais novo;
- executa `tools\setup\install_python.py scripts`, que exige o `uv` no `PATH`.

Nesta máquina o primeiro `python3` do `PATH` é o alias da Microsoft Store (`%LOCALAPPDATA%\Microsoft\WindowsApps\python3.exe`, Python 3.9), então um configure num checkout sem venv falha com `bootstrap interpreter ... is older than Python 3.10 (required by tools/pyproject.toml)`.
O Python 3.12 do python.org só instala `python.exe`, por isso não é escolhido.
O venv que funciona aqui é o do checkout principal, `C:\gits\qgroundcontrol\tools\.venv` (Python 3.10.22, criado com uv 0.13.7).
Em outro checkout ou worktree desta máquina, reaproveite-o com a variável `QGC_PYTHON_ENV`, que o `PythonVenv.cmake` respeita:

```powershell
$env:QGC_PYTHON_ENV = 'C:\gits\qgroundcontrol\tools\.venv'
```

Numa máquina nova, crie o venv **antes** do primeiro configure _(procedimento documentado pelo repositório, não executado nesta máquina)_:

```powershell
# terminal aberto depois de instalar uv e Python 3.12 (seção 2.2), na raiz do clone
py -3.12 tools\setup\install_python.py scripts
Test-Path .\tools\.venv\Scripts\python.exe
```

Com o venv em `tools\.venv`, o CMake o usa diretamente e não procura `python3`.
Use o Python 3.12 (`py -3.12`) para rodar o script: ele lê `tools/pyproject.toml` com o `tomllib` (Python 3.11+) e, no 3.10, precisaria do pacote `tomli`.
Se preferir que o próprio CMake crie o venv, três condições precisam valer ao mesmo tempo: os aliases `python.exe` e
`python3.exe` da Store desligados (Configurações > Aplicativos > Configurações avançadas de aplicativos > Aliases de
execução de aplicativo), o primeiro `python3`/`python` do `PATH` ser 3.10 ou mais novo e o `uv` instalado.
Nesta máquina isso não basta: sem os aliases, o primeiro `python` do `PATH` seria `C:\Program Files\Python39\python.exe`
(3.9). Por isso o caminho recomendado é rodar o `install_python.py` com `py -3.12`, como acima.

## 3. Compilar e executar neste computador

### 3.1 Por que entrar no shell do Visual Studio primeiro

O `cl.exe` só entra no `PATH` dentro do shell de desenvolvedor do Visual Studio (`Enter-VsDevShell` ou o _x64 Native Tools Command Prompt for VS 2022_).
Se o primeiro `cmake --preset Windows` de um diretório de build rodar fora desse shell, o configure falha sem compilador e grava no `CMakeCache.txt` as variáveis `CMAKE_CXX_FLAGS`, `CMAKE_CXX_FLAGS_RELEASE` e as flags do linker **vazias**.
O CMake nunca reinicializa essas variáveis depois, mesmo que os configures seguintes rodem no shell certo.
O resultado é falta de `/EHsc` (aviso C4530 promovido a erro pelo `/WX`) e um "Release" compilado sem `/O2` e sem `/DNDEBUG`, ou seja, sem otimização e com asserts ligados.
Foi isso que quebrou o primeiro build nesta máquina.
Não corrija com patch nas flags: refaça o cache de dentro do shell do VS (`cmake --preset Windows --fresh`, seção 3.4).

### 3.2 Build no PowerShell

```powershell
# 1. Shell de desenvolvedor do VS (x64)
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -DevCmdArguments '-arch=x64 -host_arch=x64' -SkipAutomaticLocation

# 2. Variáveis desta sessão (as três linhas marcadas valem só NESTA máquina, para o worktree;
#    numa máquina nova use o bloco da seção 4.7)
Set-Location 'C:\gits\qgroundcontrol\.claude\worktrees\win-rebuild'   # só nesta máquina
$env:QT_ROOT_DIR = 'C:\Qt\6.12.0\msvc2022_64'
$env:CPM_SOURCE_CACHE = 'C:\gits\qgroundcontrol\.cache\CPM'   # só nesta máquina: reaproveita o cache do checkout principal
$env:QGC_PYTHON_ENV = 'C:\gits\qgroundcontrol\tools\.venv'    # só nesta máquina: worktree sem tools\.venv próprio

# 3. Conferir as ferramentas
Get-Command cl.exe, ninja.exe | Select-Object -ExpandProperty Source
Get-Command cmake.exe -All | Select-Object -ExpandProperty Source
cmake --version

# 4. Configurar e compilar
cmake --preset Windows
cmake --build --preset Windows

# 5. Executar: seção 3.5 (o exe precisa das DLLs do Qt e do GStreamer no PATH)
```

- O preset `Windows` (`cmake/presets/Windows.json`) gera um build Release com Ninja em `build\Windows` e usa o toolchain `$env:QT_ROOT_DIR\lib\cmake\Qt6\qt.toolchain.cmake`.
- As variáveis `$env:` valem só para essa janela do PowerShell; repita o bloco em cada terminal novo.
- O `CPM_SOURCE_CACHE` só é lido no primeiro configure de um diretório de build; depois o valor fica no `CMakeCache.txt`.
- No checkout principal (`C:\gits\qgroundcontrol`) não é preciso `QGC_PYTHON_ENV`, porque o `tools\.venv` já está lá.
- Ao entrar no shell pode aparecer a mensagem `'vswhere.exe' não é reconhecido como um comando interno...`; nesta máquina ela não atrapalhou, e o `cl.exe` foi encontrado em seguida.

O shell do VS também coloca no `PATH` o CMake e o Ninja que vêm com o Visual Studio (aqui, CMake 3.31.6 e Ninja 1.12.1), em `...\Common7\IDE\CommonExtensions\Microsoft\CMake\`.
Nesta máquina o CMake 4.4.4 do winget continuou em primeiro lugar, mas confira com `cmake --version`.
Use sempre o mesmo CMake para um mesmo diretório de build (aqui, o 4.4.4 do winget; o CI usa o 4.4.2) e nunca configure o mesmo diretório com duas versões.
Se o CMake do Visual Studio aparecer primeiro, coloque o do winget na frente:

```powershell
$cmakeWinget = Get-Command cmake.exe -All | Where-Object Source -notlike '*Microsoft Visual Studio*' | Select-Object -First 1
$env:Path = (Split-Path $cmakeWinget.Source) + ';' + $env:Path
cmake --version
```

Depois do primeiro configure, confirme que o cache tem as flags certas:

```powershell
Select-String -Path .\build\Windows\CMakeCache.txt -Pattern '^CMAKE_CXX_FLAGS(_RELEASE)?:'
```

As duas linhas encontradas devem terminar assim:

```text
CMAKE_CXX_FLAGS:STRING=/DWIN32 /D_WINDOWS /EHsc
CMAKE_CXX_FLAGS_RELEASE:STRING=/O2 /Ob2 /DNDEBUG
```

Se estiverem vazias, veja "Erro C4530 ou Release sem otimização" na seção 6.

### 3.3 Alternativa: x64 Native Tools Command Prompt for VS 2022

Abra _x64 Native Tools Command Prompt for VS 2022_ pelo menu Iniciar (pasta Visual Studio 2022) e rode (os caminhos de
`cd`, `CPM_SOURCE_CACHE` e `QGC_PYTHON_ENV` são os do worktree desta máquina; numa máquina nova, ajuste-os como na
seção 4.7):

```bat
cd /d C:\gits\qgroundcontrol\.claude\worktrees\win-rebuild
set QT_ROOT_DIR=C:\Qt\6.12.0\msvc2022_64
set CPM_SOURCE_CACHE=C:\gits\qgroundcontrol\.cache\CPM
set QGC_PYTHON_ENV=C:\gits\qgroundcontrol\tools\.venv
cmake --version
cmake --preset Windows
cmake --build --preset Windows
```

Para executar, veja a seção 3.5.

### 3.4 Recompilar

- Incremental, depois de editar o código: `cmake --build --preset Windows`.
- Refazer só o cache (por exemplo, depois de um configure feito fora do shell do VS): `cmake --preset Windows --fresh`, sempre de dentro do shell do VS.
- Build completo do zero, apagando o diretório de build:

  ```powershell
  Remove-Item -Recurse -Force .\build\Windows
  cmake --preset Windows
  cmake --build --preset Windows
  ```

- Para limitar o paralelismo (pouca RAM): `cmake --build --preset Windows --parallel 8`.

Os caches em `.cache\` (CPM e moccache) ficam fora de `build\` e são preservados, então um build do zero não baixa as dependências de novo.

### 3.5 Executar

O build **não** copia o runtime para `build\Windows\Release`: a pasta tem só `QGroundControl.exe`, o `.pdb`, o
`qt.conf` e `lib\`. O exe importa diretamente `Qt6*.dll` (em `$env:QT_ROOT_DIR\bin`) e as DLLs do GStreamer (no
`bin` do SDK que o configure baixou, registrado como `QGC_GST_RESOLVED_ROOT` no `CMakeCache.txt`). Executado sem
elas, ele termina na hora, sem janela, com o código `-1073741515` (`0xC0000135`, DLL não encontrada).

Para rodar da árvore de build, no mesmo terminal da seção 3.2:

```powershell
$gst = (Select-String -Path .\build\Windows\CMakeCache.txt -Pattern '^QGC_GST_RESOLVED_ROOT:INTERNAL=(.*)$').Matches[0].Groups[1].Value -replace '/', '\'
if (-not $gst) { throw 'QGC_GST_RESOLVED_ROOT não encontrado em build\Windows\CMakeCache.txt' }
$env:Path = "$env:QT_ROOT_DIR\bin;$gst\bin;$env:Path"
.\build\Windows\Release\QGroundControl.exe
```

Use `-Path` e `-Pattern` com nome: na forma posicional o `Select-String` trata o caminho do arquivo como expressão
regular e falha (`\C` inválido), deixando `$gst` vazio.
Nesta máquina, assim, a janela "QGroundControl Daily" abriu em 4 s e continuou respondendo após 30 s.
Para uma pasta que roda sem nada no `PATH`, use o `cmake --install` da seção 5.1.
O layout do `build\Windows\Release` antigo do checkout principal (`platforms\`, `qml\`, `opengl32sw.dll`…) veio de um
`windeployqt` rodado à mão 2 minutos depois do build; ele copia o Qt, mas não as DLLs do GStreamer, que o exe também
importa.

Opções úteis de linha de comando:

- `--swrast`: renderização por software, para driver de vídeo problemático ou máquina virtual.
- `--clear-settings`: apaga as configurações salvas.
- `--simple-boot-test`: inicializa os subsistemas (incluindo o autoteste do GStreamer) e sai; código de saída 0 indica sucesso. É o mesmo teste que o CI roda nos pacotes.
- `--allow-multiple`: ignora a trava de instância única. Sem ela, se o QGC já estiver aberto, a segunda instância
  (inclusive o `--simple-boot-test`) mostra o aviso "already running" e fica esperando um clique.

No mesmo terminal em que o `PATH` foi ajustado acima:

```powershell
$p = Start-Process .\build\Windows\Release\QGroundControl.exe -ArgumentList '--simple-boot-test','--allow-multiple' -Wait -PassThru
$p.ExitCode
```

### 3.6 Checkout principal: alteração `/EHsc` e build antigo

No checkout principal `C:\gits\qgroundcontrol` ficaram dois restos do problema da seção 3.1:

- a alteração não commitada que acrescenta `/EHsc` em `cmake/CompilerWarnings.cmake`, desnecessária num cache
  correto (ela só duplica o `/EHsc` que já vem em `CMAKE_CXX_FLAGS`);
- o `build\Windows` com o cache envenenado, cujo `QGroundControl.exe` é um "Release" sem otimização.

Para deixar o checkout principal igual ao que foi verificado no worktree, de dentro do shell do VS:

```powershell
Set-Location C:\gits\qgroundcontrol
git diff -- cmake/CompilerWarnings.cmake   # confira: deve ser só a linha /EHsc
git restore cmake/CompilerWarnings.cmake
cmake --preset Windows --fresh
cmake --build --preset Windows
```

## 4. Levar o CÓDIGO para outro computador

### 4.1 O que levar e o que não levar

- **Levar:** o repositório Git (código, histórico e tags). O branch `worktree-win-rebuild` não altera nenhum código em relação ao `6eb58f084` do upstream; ele só acrescenta este guia. Não existe patch local que precise ir junto.
- **Opcional:** a pasta `.cache\CPM` (cerca de 4,1 GB), para não baixar as dependências de novo ou para trabalhar offline (seção 4.6).
- **Nunca levar nem reaproveitar `build\`:** o `CMakeCache.txt` e os arquivos do Ninja guardam caminhos absolutos desta máquina. Sempre configure de novo na máquina nova.
- **Não levar `tools\.venv`:** venvs Python não são relocáveis; crie um novo (seção 2.6).
- **Tags:** a versão do app vem de `git describe --tags`. Sem as tags o build funciona, mas a versão cai para 0.0.0 e o alvo `qgc-package` (instalador NSIS) se recusa a gerar o pacote. Clones do GitHub trazem as tags; no bundle, inclua `--tags`.

### 4.2 Caminho mais simples: clonar o upstream

Como não há alteração local de código, a máquina nova pode clonar direto do upstream e usar o mesmo commit verificado aqui:

```powershell
git -c core.longpaths=true clone https://github.com/mavlink/qgroundcontrol.git C:\gits\qgroundcontrol
Set-Location C:\gits\qgroundcontrol
git switch -c win-build 6eb58f084
```

As opções A, B e C abaixo servem para levar o branch local (este guia e qualquer alteração futura).

### 4.3 Opção A: fork no GitHub

Crie o fork em <https://github.com/mavlink/qgroundcontrol> (botão _Fork_) e, nesta máquina:

```powershell
Set-Location 'C:\gits\qgroundcontrol\.claude\worktrees\win-rebuild'
git remote add fork https://github.com/SEU_USUARIO/qgroundcontrol.git
git push -u fork worktree-win-rebuild
```

Na máquina nova:

```powershell
git -c core.longpaths=true clone -b worktree-win-rebuild https://github.com/SEU_USUARIO/qgroundcontrol.git C:\gits\qgroundcontrol
Set-Location C:\gits\qgroundcontrol
git remote add upstream https://github.com/mavlink/qgroundcontrol.git
git fetch upstream --tags
```

### 4.4 Opção B: offline com git bundle

Nesta máquina, gere um arquivo único com os dois branches e as tags:

```powershell
Set-Location 'C:\gits\qgroundcontrol\.claude\worktrees\win-rebuild'
git bundle create "$env:USERPROFILE\qgc-win.bundle" master worktree-win-rebuild --tags
git bundle verify "$env:USERPROFILE\qgc-win.bundle"
```

O bundle leva o histórico completo: o pack deste repositório tem cerca de 560 MB (`git count-objects -vH`) e 124 tags.
Copie `qgc-win.bundle` para a máquina nova (pendrive, rede) e clone a partir dele (exemplo com o arquivo em `D:\`):

```powershell
git -c core.longpaths=true clone -b worktree-win-rebuild D:\qgc-win.bundle C:\gits\qgroundcontrol
Set-Location C:\gits\qgroundcontrol
git remote set-url origin https://github.com/mavlink/qgroundcontrol.git
```

A última linha é opcional: ela troca o `origin` (que aponta para o arquivo) pelo upstream, para buscar atualizações quando houver internet.

### 4.5 Opção C: copiar a pasta com robocopy

Copie o **checkout principal** `C:\gits\qgroundcontrol`, não a pasta do worktree.
No worktree, `.git` é apenas um arquivo que aponta para `C:\gits\qgroundcontrol\.git\worktrees\win-rebuild`; sozinho ele não é um repositório.
O `.git` do checkout principal contém todos os branches, inclusive `worktree-win-rebuild`.

Nesta máquina (exemplo com um disco externo em `E:\`):

```powershell
robocopy C:\gits\qgroundcontrol E:\qgroundcontrol /E /R:1 /W:1 /XD C:\gits\qgroundcontrol\build C:\gits\qgroundcontrol\.cache C:\gits\qgroundcontrol\tools\.venv C:\gits\qgroundcontrol\.claude\worktrees C:\gits\qgroundcontrol\.git\worktrees
```

Na máquina nova:

```powershell
robocopy E:\qgroundcontrol C:\gits\qgroundcontrol /E /R:1 /W:1
Set-Location C:\gits\qgroundcontrol
git status
git switch worktree-win-rebuild
```

- O `robocopy` retorna códigos de saída de 0 a 7 quando dá certo (1 significa "arquivos copiados").
- O `/XD ...\.git\worktrees` deixa de fora o registro do worktree. Ele está travado (arquivo `locked`) enquanto a
  sessão que o criou existir; copiado, o `git worktree prune` não o removeria e o `git switch` falharia com
  "already used by worktree". Os branches ficam em `.git\refs` e `.git\packed-refs` e são copiados normalmente.
- Se o `git status` mostrar a alteração em `cmake/CompilerWarnings.cmake` (o `/EHsc` da seção 3.6), descarte-a
  com `git restore cmake/CompilerWarnings.cmake` antes do `git switch`.

### 4.6 Cache de dependências (opcional)

Copiar o cache do CPM evita baixar cerca de 4,1 GB e permite configurar sem internet.
O SDK do GStreamer no cache (`gstreamer-win-x86_64-1.28.8\install`) usa caminhos relativos no pkg-config (`prefix=${pcfiledir}/../..`), então funciona em outro lugar.

```powershell
# nesta máquina
robocopy C:\gits\qgroundcontrol\.cache\CPM E:\qgc-cpm-cache /E /R:1 /W:1

# na máquina nova: um caminho SEM espaços
robocopy E:\qgc-cpm-cache C:\qgc-cpm-cache /E /R:1 /W:1
$env:CPM_SOURCE_CACHE = 'C:\qgc-cpm-cache'
```

Defina `CPM_SOURCE_CACHE` antes do **primeiro** configure.
Outra opção é copiar a pasta para `<repo>\.cache\CPM`, que é o local padrão, e não definir a variável.

### 4.7 Checklist da máquina nova

_(Procedimento montado a partir dos passos verificados nesta máquina; a sequência completa não foi executada num
computador novo.)_

1. Windows 10 ou 11 x64, com acesso de administrador.
2. Habilite caminhos longos (`LongPathsEnabled=1`, como Administrador) e `git config --global core.longpaths true`
   assim que o Git estiver instalado, **antes** de clonar (seção 2.5).
3. Instale as ferramentas que não dependem do clone (seções 2.2 e 2.3): Git, CMake, Ninja, Python 3.12, uv e o
   Visual Studio 2022 Build Tools com o workload VCTools. Abra um terminal novo.
4. Obtenha o código (seção 4.2 ou opções A, B ou C) e confira que o histórico contém o commit verificado:
   `git merge-base --is-ancestor 6eb58f084 HEAD; $LASTEXITCODE` deve imprimir `0`.
5. Instale o Qt 6.12.0 `msvc2022_64` (seção 2.4): pelo Qt Online Installer ou, agora que o clone existe, com o
   `install_qt.py`.
6. Crie o venv: `py -3.12 tools\setup\install_python.py scripts` na raiz do clone (seção 2.6).
7. Opcional: copie o cache do CPM (seção 4.6).
8. Num PowerShell novo, rode o bloco abaixo. Ele não usa nenhum caminho específico desta máquina; ajuste só
   `Set-Location` se o clone estiver em outro lugar.

   ```powershell
   $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
   Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
   Enter-VsDevShell -VsInstallPath $vs -DevCmdArguments '-arch=x64 -host_arch=x64' -SkipAutomaticLocation
   Set-Location C:\gits\qgroundcontrol
   $env:QT_ROOT_DIR = 'C:\Qt\6.12.0\msvc2022_64'
   # $env:CPM_SOURCE_CACHE = 'C:\qgc-cpm-cache'   # só se copiou o cache no passo 7
   cmake --version
   cmake --preset Windows
   cmake --build --preset Windows
   Select-String -Path .\build\Windows\CMakeCache.txt -Pattern '^CMAKE_CXX_FLAGS(_RELEASE)?:'
   ```

9. Confira que as flags terminam em `/EHsc` e `/O2 /Ob2 /DNDEBUG`, execute o app e rode o `--simple-boot-test`
   como na seção 3.5.

## 5. Levar o APP pronto (pacote ZIP portátil)

Use este caminho quando o outro computador só precisa **usar** o QGC, sem compilar.

### 5.1 Gerar o ZIP

No mesmo terminal da seção 3 (shell do VS, `QT_ROOT_DIR` definido), na raiz do repositório e depois de um build Release:

```powershell
$staging = 'C:\gits\qgc-dist\QGroundControl'
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }   # evita restos de uma geração anterior
cmake --install build\Windows --config Release --component Runtime --prefix $staging
Compress-Archive -Path $staging -DestinationPath 'C:\gits\qgc-dist\QGroundControl-5.2.0-dev-win64-portable.zip' -Force
```

- `--component Runtime` instala só o que o app usa, o mesmo conjunto que o instalador NSIS empacota
  (`cmake/install/CPack/CreateCPackCommon.cmake`). Sem ele, o install também copia `include\`, as bibliotecas
  estáticas e as DLLs de depuração do runtime do MSVC (`*140d.dll`, que a Microsoft não permite redistribuir), vindas
  das regras de install das dependências (por exemplo, a do GeographicLib).
- Sem `--prefix`, o `cmake --install` usa `build\Windows\staging`.

No Windows, o install (`cmake/install/Install.cmake`) monta uma pasta autossuficiente:

- `bin\QGroundControl.exe`;
- o runtime do Qt (DLLs, plugins e módulos QML), via o script de deploy do Qt (`windeployqt`);
- o runtime do GStreamer: só os plugins da allowlist (em `lib\gstreamer-1.0\`, com os módulos GIO em `lib\gio\`), os auxiliares (como o `gst-plugin-scanner`, em `libexec\`) e as DLLs do SDK que eles e o QGC carregam, além das licenças em `share\licenses\gstreamer\`;
- as DLLs do runtime do MSVC (`vcruntime140*.dll`, `msvcp140*.dll` e afins) em `bin\`, via `include(InstallRequiredSystemLibraries)`.

O CI confere essa pasta com `tools/ci/packaging/pe_dependency_audit.py`: toda DLL importada precisa estar no pacote ou ser DLL de sistema do Windows, e o runtime do MSVC **não** conta como DLL de sistema.

### 5.2 O que o computador de destino precisa

- Windows 10 ou 11 x64.
- Driver de vídeo com OpenGL/Direct3D funcionando; se não houver, use `--swrast`.
- Não precisa de Qt, Visual Studio, Python nem GStreamer instalados.
- O Visual C++ Redistributable 2015-2022 x64 normalmente **não** é necessário, porque o runtime do MSVC vai dentro de `bin\` e o UCRT faz parte do Windows 10/11. Se mesmo assim aparecer erro de DLL ausente (por exemplo `VCRUNTIME140_1.dll` ou `MSVCP140.dll`), instale-o:

  ```powershell
  winget install --id Microsoft.VCRedist.2015+.x64 -e
  ```

### 5.3 Executar a partir da pasta extraída

```powershell
Unblock-File .\QGroundControl-5.2.0-dev-win64-portable.zip   # só se o ZIP veio da internet
Expand-Archive -Path .\QGroundControl-5.2.0-dev-win64-portable.zip -DestinationPath C:\Apps
& 'C:\Apps\QGroundControl\bin\QGroundControl.exe'
```

- Mantenha a estrutura da pasta: não mova o `QGroundControl.exe` para fora de `bin\`; crie um atalho se quiser.
- Para conferir o pacote no destino, rode o autoteste e veja o código de saída (0 = sucesso):

  ```powershell
  $p = Start-Process 'C:\Apps\QGroundControl\bin\QGroundControl.exe' -ArgumentList '--simple-boot-test','--allow-multiple' -Wait -PassThru
  $p.ExitCode
  ```

### 5.4 Onde ficam as configurações

As configurações não vão no ZIP; ficam no perfil do usuário, e o app começa "limpo" no computador de destino.

- Configurações (INI): `%APPDATA%\QGroundControl\QGroundControl Daily.ini`. Builds sem `QGC_STABLE_BUILD` (como este) usam o nome "QGroundControl Daily", separado do `QGroundControl.ini` de uma versão estável instalada.
- Cache de parâmetros: `%LOCALAPPDATA%\QGroundControl\QGroundControl Daily\cache\ParamCache`
  (`QStandardPaths::CacheLocation`, em `src/FactSystem/ParameterManager.cc`).
- Dados (missões, logs, vídeos): pasta Documentos do usuário, em `QGroundControl Daily` (caminho configurável no app).

Para levar as configurações junto, copie a pasta `%APPDATA%\QGroundControl` (e, se quiser, a pasta de dados em Documentos) para o mesmo lugar no destino.

### 5.5 Instalador NSIS (opcional)

_(Procedimento documentado pelo repositório, não executado nesta máquina.)_
O alvo `qgc-package` gera o instalador NSIS, que é o instalador de produção do Windows.
Ele exige o NSIS instalado e as tags do Git (versão diferente de 0.0.0, seção 4.1).

```powershell
winget install --id NSIS.NSIS -e
# ou, como Administrador, na raiz do repositório:
# py -3.12 tools\setup\install_dependencies --platform windows --nsis --skip-gstreamer

cmake --build build\Windows --target qgc-package
```

O instalador sai em `build\Windows\QGroundControl-installer-AMD64.exe`.
Ele instala em `Program Files\QGroundControl` e cria os atalhos "QGroundControl" e "QGroundControl (GPU Safe Mode)", este com `-swrast`.

### 5.6 Resultado verificado nesta máquina (2026-11-09)

| Item | Resultado |
| --- | --- |
| Código | `master` @ `6eb58f084`, sem nenhuma alteração |
| Configure + build do zero (`build\Windows` novo) | código 0; 3202 passos; 166 s de configure, 934 s no total |
| Flags no cache | `/DWIN32 /D_WINDOWS /EHsc` e `/O2 /Ob2 /DNDEBUG` |
| `--simple-boot-test` na árvore de build (Qt e GStreamer no `PATH`) | código 0 em 7,5 s |
| Interface gráfica na árvore de build | janela aberta em 4 s, respondendo após 30 s, fechada com código 0 |
| `cmake --install --component Runtime` | código 0 em 7 s; pasta de 298 MB, 130 arquivos em `bin\`, sem `*140d.dll` nem `include\` |
| ZIP (`Compress-Archive`) | `QGroundControl-5.2.0-dev-win64-portable.zip`, 142,1 MB, 16 s |
| ZIP extraído em outra pasta, `PATH` só com `C:\Windows\...` | `--simple-boot-test` com código 0 em 8 s |

Para comparação, o `cmake --install` sem `--component` gerou 415 MB (138 arquivos em `bin\`, incluindo as DLLs de
depuração) e um ZIP de 169,7 MB, que também passou no `--simple-boot-test`.
Não apague `lib\` do pacote: `lib\gstreamer-1.0\` e `lib\gio\` são necessários para o vídeo e para o autoteste.

## 6. Solução de problemas

### Compilador (`cl.exe`) ou Ninja não encontrados

- **Sintoma:** `No CMAKE_CXX_COMPILER could be found`, ou o CMake não acha um programa de build para o gerador "Ninja".
- **Causa:** terminal fora do shell de desenvolvedor do VS, ou Ninja fora do `PATH`.
- **Solução:** entre no shell (seção 3.2) ou use o _x64 Native Tools Command Prompt_ e confira com `Get-Command cl.exe, ninja.exe`. Se faltar o Ninja, instale-o com winget e abra um terminal novo. Como esse configure falho envenena o cache (próximo item), refaça-o com `cmake --preset Windows --fresh` ou apague `build\Windows`.

### Erro C4530 ou Release sem otimização (cache envenenado)

- **Sintoma:** `warning C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc` virando erro (`/WX`), ou um Release que compila mas fica lento e com asserts ligados.
- **Causa:** o primeiro configure de `build\Windows` rodou fora do shell do VS; o cache ficou com `CMAKE_CXX_FLAGS`, `CMAKE_CXX_FLAGS_RELEASE` e as flags do linker vazias, e o CMake nunca as reinicializa (seção 3.1).
- **Solução:** não altere flags nem faça patch no CMake. De dentro do shell do VS, rode `cmake --preset Windows --fresh` (ou apague `build\Windows`), compile de novo e confira com `Select-String -Path .\build\Windows\CMakeCache.txt -Pattern '^CMAKE_CXX_FLAGS_RELEASE:'`, que deve mostrar `/O2 /Ob2 /DNDEBUG`.

### O exe não abre (nada acontece) ou termina com `-1073741515`

- **Sintoma:** ao rodar `build\Windows\Release\QGroundControl.exe`, nenhuma janela aparece; o código de saída é
  `-1073741515` (`0xC0000135`, DLL não encontrada).
- **Causa:** o build não copia as DLLs do Qt nem as do GStreamer para `build\Windows\Release`.
- **Solução:** coloque `$env:QT_ROOT_DIR\bin` e o `bin` do SDK do GStreamer no `PATH` (seção 3.5) ou gere a pasta
  autossuficiente com `cmake --install` (seção 5.1).

### `QT_ROOT_DIR` não definido (toolchain não encontrado)

- **Sintoma:** o configure diz que não encontra o arquivo de toolchain `/lib/cmake/Qt6/qt.toolchain.cmake` (caminho sem a pasta do Qt na frente).
- **Causa:** o preset monta o toolchain a partir de `QT_ROOT_DIR`, que não foi definido nesta sessão.
- **Solução:** `$env:QT_ROOT_DIR = 'C:\Qt\6.12.0\msvc2022_64'`, confira com `Test-Path "$env:QT_ROOT_DIR\lib\cmake\Qt6\qt.toolchain.cmake"` e rode `cmake --preset Windows --fresh`.

### Python: "older than Python 3.10" ou "uv not found on PATH"

- **Sintoma:** `QGC: bootstrap interpreter ... is older than Python 3.10 (required by tools/pyproject.toml)`, ou `uv not found on PATH`, ou `QGC: failed to create .venv`.
- **Causa:** não há `tools\.venv` e o CMake tentou criá-lo com o primeiro `python3` do `PATH` (aqui, o alias da Store, Python 3.9), ou o `uv` não está instalado (seção 2.6).
- **Solução:** nesta máquina, `$env:QGC_PYTHON_ENV = 'C:\gits\qgroundcontrol\tools\.venv'`. Numa máquina nova, instale uv e Python 3.12 e rode `py -3.12 tools\setup\install_python.py scripts` na raiz do clone. Depois apague `build\Windows` (o cache guarda o interpretador escolhido) e configure de novo.

### Caminho do cache do CPM com espaços

- **Sintoma:** `GStreamer: CPM_SOURCE_CACHE '...', where the GStreamer SDK is downloaded, contains a space...`.
- **Causa:** o pkg-config não aceita espaços no caminho do SDK. No Windows o CMake tenta usar o nome curto 8.3 da pasta e falha se o volume não tiver nomes curtos. Com o padrão (`<repo>\.cache\CPM`), um repositório num caminho com espaços causa o mesmo problema.
- **Solução:** use um cache sem espaços, por exemplo `$env:CPM_SOURCE_CACHE = 'C:\qgc-cpm-cache'`, e apague `build\Windows` (o valor fica gravado no cache) ou passe `-DCPM_SOURCE_CACHE=C:/qgc-cpm-cache` no configure. Melhor ainda: mantenha o repositório em um caminho sem espaços.

### Caminhos longos

- **Sintoma:** `Filename too long` no clone, ou falhas ao extrair dependências ou compilar arquivos em pastas profundas.
- **Solução:** `LongPathsEnabled=1` no registro e `git config --global core.longpaths true` (seção 2.5); mantenha o repositório perto da raiz do disco. Nesta máquina o worktree em `C:\gits\qgroundcontrol\.claude\worktrees\win-rebuild` compila com essas duas configurações.

### Falha ou timeout no download do SDK do GStreamer

- **Sintoma:** o configure para no download do SDK do GStreamer (cerca de 540 MB) por timeout, rede ou proxy.
- **Solução:** escolha uma das saídas abaixo.
  - Aumente o limite de tempo (padrão 1200 s): `cmake --preset Windows -DQGC_GST_DOWNLOAD_TIMEOUT=3600`.
  - Copie a pasta `.cache\CPM\gstreamer-win-x86_64-1.28.8` de uma máquina que já baixou (seção 4.6).
  - Instale o SDK à parte (como Administrador, `py -3.12 tools\setup\install_dependencies --platform windows`, que instala em `C:\gstreamer\1.0\msvc_x86_64`); o configure o usa sozinho quando ele está completo e é 1.28.8 ou mais novo (seção 2.3), ou aponte para ele com `-DGStreamer_ROOT_DIR=C:/gstreamer/1.0/msvc_x86_64` _(procedimento documentado pelo repositório, não executado nesta máquina)_.
  - Compile sem GStreamer: `-DQGC_ENABLE_GST_VIDEOSTREAMING=OFF`. O vídeo de rede passa a usar o player do Qt Multimedia, que só grava enquanto o vídeo está na tela.

### Antivírus deixando o build lento

- **Sintoma:** build muito mais lento que os cerca de 15 minutos medidos nesta máquina, ou erros esporádicos de arquivo em uso durante a compilação ou o link.
- **Causa:** o antivírus examina cada arquivo gerado.
- **Solução:** se a política da sua máquina permitir, adicione exclusões do Microsoft Defender só para pastas confiáveis (repositório e cache), como Administrador:

  ```powershell
  Add-MpPreference -ExclusionPath 'C:\gits\qgroundcontrol'
  Add-MpPreference -ExclusionPath 'C:\qgc-cpm-cache'
  ```

### O app abre, mas não mostra vídeo

- **Causas comuns:** o `QGroundControl.exe` foi movido para fora da pasta com as DLLs e os plugins do GStreamer; o build foi feito com `QGC_ENABLE_GST_VIDEOSTREAMING=OFF`; um plugin ou decodificador de hardware falhou.
- **Diagnóstico:** rode o `--simple-boot-test` (seção 3.5). Ele verifica os plugins do GStreamer empacotados, o `gst-plugin-scanner`, a decodificação H.264 e a gravação; um código de saída diferente de 0 indica problema no runtime.
- **Log do GStreamer em arquivo:**

  ```powershell
  $env:GST_DEBUG = '3'
  $env:GST_DEBUG_FILE = "$env:TEMP\qgc-gst.log"
  .\build\Windows\Release\QGroundControl.exe
  ```

- **Decodificador:** nas configurações de vídeo do app, em "Force video decoder priority", escolha "Force software decoder" para descartar problema de decodificação por hardware; teste também `--swrast`.
- **Pacote ZIP:** gere-o de novo com `cmake --install` (seção 5.1) em vez de copiar arquivos à mão de `build\Windows\Release`.

## 7. Referências

- [Guia de desenvolvimento: Getting Started](../../en/qgc-dev-guide/getting_started/index.md) (instalação do Qt, dependências, Visual Studio, instaladores)
- [AGENTS.md](../../../AGENTS.md) e [CODING_STYLE.md](../../../CODING_STYLE.md)
- [tools/README.md](../../../tools/README.md) (scripts de setup, `install_dependencies`, receitas `just`)
- [cmake/README.md](../../../cmake/README.md) (módulos e presets do CMake)
- [cmake/presets/Windows.json](../../../cmake/presets/Windows.json) (presets `Windows` e `Windows-debug`)
- [.github/build-config.json](../../../.github/build-config.json) (versões de Qt, CMake, Ninja, GStreamer e módulos do Qt)
- [cmake/modules/PythonVenv.cmake](../../../cmake/modules/PythonVenv.cmake) (criação do `tools\.venv` e `QGC_PYTHON_ENV`)
- [tools/setup/install_python.py](../../../tools/setup/install_python.py), [tools/setup/install_qt.py](../../../tools/setup/install_qt.py) e [tools/setup/install_dependencies/\_windows.py](../../../tools/setup/install_dependencies/_windows.py)
- [cmake/install/Install.cmake](../../../cmake/install/Install.cmake) e [cmake/install/CPack/CreateCPackNSIS.cmake](../../../cmake/install/CPack/CreateCPackNSIS.cmake) (install, ZIP e instalador NSIS)
- [cmake/CustomOptions.cmake](../../../cmake/CustomOptions.cmake) (`QGC_GST_DOWNLOAD_TIMEOUT`, `QGC_ENABLE_GST_VIDEOSTREAMING`, `GStreamer_ROOT_DIR`)
- [src/VideoManager/GStreamer/README.md](../../../src/VideoManager/GStreamer/README.md) (SDK do GStreamer, cache, autoteste e logs)
