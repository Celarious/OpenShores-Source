# OpenShores

Pick any star in the night sky and fly to it. Build cities, design your own starships deck by deck, and walk aboard.

OpenShores is a community volunteer project continuing development of **Shores of Hazeron**, with the goal of keeping the game alive and free to play.

The original game was created by **David Hoeft (“Haxus”)**. David’s passion for the project over more than two decades shaped a remarkably ambitious game, bringing together interstellar exploration, city building, and starships you can design and walk aboard. We’re grateful for the work he put into it and for sharing the source code so its development can continue.

Support David by purchasing [Hazeron Starship on Steam](https://store.steampowered.com/app/2239520/Hazeron_Starship/), the official single-player version of the game.

Shores of Hazeron is copyright © 2026 **Hazeron Freehold LLC**. See [LICENSE](LICENSE) for usage and distribution terms. You can find the original source repository provided by David [here](https://github.com/Hazeron-Freehold-LLC/Shores-of-Hazeron).

Original project notes: [README-legacy.md](README-legacy.md).

## Requirements

- Docker with Linux container support
- GNU Make
- Bash

Docker Desktop can be used on Windows.

## Installing WSL and Docker on Windows

Open PowerShell as Administrator and install WSL:

```powershell
wsl --install -d Ubuntu
```

Restart Windows, open Ubuntu, and finish its user setup. Then install Docker Desktop, enable its WSL 2 engine, and enable integration for Ubuntu under **Settings > Resources > WSL Integration**.

Alternatively, skip Docker Desktop and install Docker Engine directly inside Ubuntu using Docker's [Ubuntu installation guide](https://docs.docker.com/engine/install/ubuntu/).

Inside Ubuntu, install the remaining tools and verify Docker:

```bash
sudo apt update
sudo apt install git make
docker run --rm hello-world
```

For Linux development, keep the repository under `~/src/OpenShores` in WSL for better filesystem performance. For native Windows F5 debugging, open a Windows checkout such as `C:\src\OpenShores` in Windows VS Code.

## Build

```bash
make                 # Show common commands
make all             # All Linux programs and Windows clients
make linux           # All Linux programs
make linux-client    # Linux scene client
make linux-clients   # All Linux clients
make linux-servers   # Linux multiplayer servers
make win             # All Windows clients
make win-client      # Windows x64 scene client
```

Use `make help FULL=1` to include advanced commands and options.

Set `CONFIG` to `release` (default), `debug`, or `relwithdebinfo`:

```bash
make win-client CONFIG=debug
```

Output goes to `build/<platform>-<config>/`. Run clients from `bin/` and keep the runtime files beside them.

## Release packages

Native macOS builds use `make mac-client` on a Mac with Qt 5.15, SDL2,
CMake, Ninja, Python 3 and Apple's command-line developer tools installed.
See [the Mac build guide](tools/macos/README.md) for packaging, output paths
and connection testing. Linux and Windows build commands are unchanged.

```bash
make linux-package   # Linux game and launcher archives
make win-package     # Windows game and launcher archives
make release-package # Every release archive
```

Archives and checksums go to `build/packages/`.

## F5 Debugging in VS Code

Install the recommended C/C++ extension, select a Linux or Windows launch configuration, and press `F5`. Docker builds the client; GDB runs it locally.

Use Debug for easier debugging or RelWithDebInfo for better performance. Disable CMake Tools for this workspace if installed.

### Windows

Set up WSL and Docker as described above. Make sure Docker works without `sudo` in Ubuntu. Set Ubuntu as the default in PowerShell:

```powershell
wsl --set-default Ubuntu
```

Install [MSYS2](https://www.msys2.org/) at `C:\msys64`. Open its UCRT64 terminal and run:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gdb
```

Open the repository in Windows VS Code or Remote WSL. Select a Windows launch option and press `F5`. From WSL, VS Code starts Windows GDB through WSL interop. The game runs natively on Windows.

If you installed MSYS2 elsewhere, change `miDebuggerPath` in `.vscode/launch.json`.

### Linux

Install [Docker Engine](https://docs.docker.com/engine/install/ubuntu/) and follow the [post-install steps](https://docs.docker.com/engine/install/linux-postinstall/) to run it without `sudo`.

On Ubuntu 22.04, install GDB and the runtime libraries:

```bash
sudo apt update
sudo apt install make git gdb libqt5opengl5 libqt5network5 \
    libqt5sql5-psql libopenal1 libsdl2-2.0-0 libglu1-mesa
docker run --rm hello-world
```

Open the repository in VS Code, select a Linux launch option, and press `F5`. Other distributions need libraries compatible with the Ubuntu 22.04 build.


## Local and production servers

**WSL 2:** If Docker Engine runs inside WSL in NAT mode and you’re connecting from Windows, follow the [WSL networking setup](deploy/server/README.md#windows-clients-with-docker-engine-inside-wsl-2) before starting the server. `localhost` forwarding does not carry the game’s UDP controls.

```bash
make server-up       # Start the local Debug stack
make server-logs
make server-down
```

See [the server guide](deploy/server/README.md) for configuration and deployment details.

