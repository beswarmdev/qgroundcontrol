# BeSwarm Vehicle GS: distribuição para Windows e Linux

Scripts que levam o código de `C:\gits\qgroundcontrol` até os artefatos em `C:\gits\qgc-dist`. Tudo é disparado do
PowerShell do Windows; o WSL só entra onde o Linux é indispensável.

| Plataforma | Script | Artefato em `C:\gits\qgc-dist` |
| --- | --- | --- |
| Windows | `deploy\beswarm\dist-windows.ps1` | `BeSwarmVehicleGS\`, `BeSwarmVehicleGS-win64-portable.zip` e `.sha256` |
| Linux | `deploy\beswarm\dist-linux.ps1` | `BeSwarmVehicleGS-linux-x86_64.AppImage` e `.sha256` |

O `.sha256` tem o formato `<hash minúsculo><dois espaços><nome do arquivo>` e termina com LF, compatível com
`sha256sum -c`. Nome interno do aplicativo (`QGroundControl`) e pastas de configuração não mudam; só o nome exibido.

## Windows

Pré-requisitos: os do guia `docs/superpowers/guides/windows-build-e-migracao.md` (Visual Studio Build Tools, Qt
`msvc2022_64`, GStreamer, CMake e Ninja).

```powershell
powershell -ExecutionPolicy Bypass -File deploy\beswarm\dist-windows.ps1
```

| Parâmetro | Padrão |
| --- | --- |
| `-RepoDir` | raiz do repositório que contém o script |
| `-BuildDir` | `<RepoDir>\build\Windows` |
| `-DistDir` | `C:\gits\qgc-dist` |
| `-Name` | `BeSwarmVehicleGS` |
| `-QtRoot` | `C:\Qt\6.12.0\msvc2022_64` |
| `-SkipBuild` | desligado (compila antes de empacotar) |

## Linux

O AppImage é compilado na distro WSL `Ubuntu-26.04` (glibc 2.43, GCC 15.2, CMake 4.2, GStreamer 1.28 do apt). Dentro do
WSL ficam só pacotes, Qt `gcc_64`, compilação, AppImage e o teste de boot dela. Edição, git, lint, cópia para o
`qgc-dist` e hashes ficam no Windows.

`dist-linux.ps1` resolve o commit com o git do Windows e chama `build-appimage.sh` no WSL. O `.sh` mantém um clone
Linux em `~/src/qgroundcontrol`, cujo `origin` é o repositório do Windows, compila com o preset `Linux`, gera o
AppImage com `cmake --install` (o `CreateAppImage.cmake` baixa linuxdeploy e appimagetool) e roda
`--simple-boot-test` no binário e no AppImage. Nunca edite o clone Linux nem compile dentro de `/mnt/c`: toda
mudança nasce no repositório do Windows.

### Preparação única do WSL

Cada passo é disparado do PowerShell do Windows. Tarefas de sistema rodam como root (`-u root`), porque o `sudo` do
usuário pede senha.

1. Conferir a distro (esperado: `Ubuntu 26.04.1 LTS`, glibc 2.43, GCC 15.2):

   ```powershell
   wsl.exe -d Ubuntu-26.04 -- bash -lc 'lsb_release -ds; ldd --version | head -1; gcc --version | head -1'
   ```

2. Pacotes do sistema. O instalador termina com `Could not install build tools: uv not found on PATH` e código 1:
   é esperado, porque o uv só entra no passo 4 e CMake e Ninja já vêm do apt. O apt escolhe `libfuse2t64` no lugar
   de `libfuse2` e o instalador descarta `libgstreamer-plugins-good1.0-dev`, ausente no 26.04; ambos são normais.

   ```powershell
   $cmd = 'cd /mnt/c/gits/qgroundcontrol && python3 tools/setup/install_dependencies --platform debian'
   wsl.exe -d Ubuntu-26.04 -u root -- bash -lc $cmd
   wsl.exe -d Ubuntu-26.04 -u root -- apt-get install -y curl
   ```

3. Clone Linux (a versão do app vem de `git describe --tags`, então ele precisa dos tags):

   ```powershell
   $cmd = 'git config --global --add safe.directory /mnt/c/gits/qgroundcontrol && mkdir -p ~/src && ' +
       'git clone /mnt/c/gits/qgroundcontrol ~/src/qgroundcontrol'
   wsl.exe -d Ubuntu-26.04 -- bash -lc $cmd
   ```

4. uv. O `~/.profile` padrão do Ubuntu já põe `~/.local/bin` no `PATH` quando a pasta existe; confira com
   `uv --version` num shell de login novo:

   ```powershell
   wsl.exe -d Ubuntu-26.04 -- bash -lc 'cd ~/src/qgroundcontrol && python3 tools/setup/install_uv.py'
   wsl.exe -d Ubuntu-26.04 -- bash -lc 'uv --version'
   ```

5. Venv das ferramentas (o `tools/.python-version` fixa o Python 3.10, que o uv baixa sozinho):

   ```powershell
   wsl.exe -d Ubuntu-26.04 -- bash -lc 'cd ~/src/qgroundcontrol && python3 tools/setup/install_python.py scripts'
   ```

6. Qt 6.12.0 `gcc_64` (demora; esperado `~/Qt/6.12.0/gcc_64/lib/cmake/Qt6/qt.toolchain.cmake`):

   ```powershell
   $cmd = 'cd ~/src/qgroundcontrol && python3 tools/setup/install_qt.py install --from-config ' +
       '--host linux --arch linux_gcc_64 --outdir ~/Qt'
   wsl.exe -d Ubuntu-26.04 -- bash -lc $cmd
   ```

### Gerar o AppImage

```powershell
powershell -ExecutionPolicy Bypass -File deploy\beswarm\dist-linux.ps1
```

O primeiro build leva cerca de 40 minutos; os seguintes são incrementais.

| Parâmetro | Padrão |
| --- | --- |
| `-Ref` | `HEAD` do checkout ou worktree que contém o script (aceita qualquer branch, tag ou SHA do repositório) |
| `-DistDir` | `C:\gits\qgc-dist` |
| `-Name` | `BeSwarmVehicleGS` |
| `-Jobs` | número de processadores lógicos menos 2 |
| `-Distro` | `Ubuntu-26.04` |
| `-SkipBuild` | desligado (reaproveita a árvore de build do WSL e só refaz o AppImage) |

O script termina com código diferente de 0 em qualquer falha e imprime o commit, o caminho, o tamanho e o SHA-256 do
artefato.

## Rodar o AppImage

- Exige glibc 2.43 ou mais nova (Ubuntu 26.04 ou posterior). Ubuntu 24.04 e Debian 13 não o executam.
- Antes de executar: `chmod +x BeSwarmVehicleGS-linux-x86_64.AppImage`.
- Sem FUSE no sistema: `./BeSwarmVehicleGS-linux-x86_64.AppImage --appimage-extract-and-run`.
- Teste rápido no próprio Windows, via WSLg:

  ```powershell
  wsl.exe -d Ubuntu-26.04 -- /mnt/c/gits/qgc-dist/BeSwarmVehicleGS-linux-x86_64.AppImage --appimage-extract-and-run
  ```

## Licenças

O AppImage leva o `gst-libav` do Ubuntu, que puxa x264 e x265 sob GPL (mesmo aviso do CI do QGroundControl upstream).
