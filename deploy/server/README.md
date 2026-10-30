# Docker server deployment

The deployment runs every server process in its own container. The login, mail, and scene roles share a game-server base image; the scene master adds its INI in a small derived image. PostgreSQL and Apache each use their own image and container.

This deployment targets one Docker host. Scene workers scale across containers on that host. Multi-host scheduling is not included.

## Topology

```text
web (Apache + mod_cgid)
postgres
login-master
login-worker
mail-master
mail-worker
scene-master
scene-manager
scene-worker-1 ... scene-worker-N
```

Containers use Docker service names for private traffic. Native clients use the configured public host and published ports. PostgreSQL is never published.

Server compilation reuses the repository's existing named Docker volumes (`hazeron-linux-<config>-cache`). The build exports `build/linux-<config>/bin` and `lib`; the deployment then packages those artifacts into the runtime image. There is no second CMake cache inside the image build.

The `server-deployment` build target compiles the game servers and the five web
tools (including `AuEmpireReport`) together. Deployment containers use no bind mounts: executables, web files,
and INI configuration are copied into images. Only database data and generated
reports use named Docker volumes. The release archive includes every local input
needed to assemble those images, with paths relative to the extracted archive.

The image build uses a unique temporary context under `build/server/` and removes it when the script exits, including on failure. Set `HAZERON_KEEP_STAGING=1` to retain the context for inspection; its path is printed when the script exits.

## Local development

Start one Debug scene worker:

```bash
make server-up
```

Common commands:

```bash
make server-status
make server-logs
make server-logs SERVICE=scene-worker-1
make server-down
```

`server-down` preserves the PostgreSQL and web report volumes. Delete and recreate the local universe and web reports only when intended:

```bash
make server-reset
# Non-interactive: make server-reset CONFIRM=yes
```

Scale the scene-worker pool:

```bash
make server-up SCENE_WORKERS=3
```

Local defaults are in `environments/local.env.example`. Copy it to the ignored `environments/local.env` only when overrides are needed:

```bash
cp deploy/server/environments/local.env.example \
   deploy/server/environments/local.env
```

The local database creates this development account:

```text
account:  local
password: local
```

Change the credentials in `local.env` before the first initialization, or run `make server-reset` after changing them.

### Apache web service

The `web` service installs Apache 2 with the `event` MPM and `mod_cgid` in a
separate Ubuntu image. Its homepage loads immediately with “Loading server status...”,
then JavaScript fetches `/server-status.php`. This endpoint executes `AuServerStatus`
with `-l`, `-m`, and `-g` and returns HTML fragments for the status panel.
The scene status uses the management channel, which recognizes the database states.
The tool connects to the masters by service name on the private Docker network.
Apache runs PHP through `mod_actions` and `mod_cgid`, keeping the `event` MPM;
`AuServerStatus` remains a command-line program. `/register` serves the static
`register.html` form, which submits to `/cgi-bin/AuAcctNew`. The CGI handler
creates an account through the deployment's `login-worker` and returns its
existing success or error page. Apache passes `AU_LOGIN_SERVER`,
`AU_ACCOUNT_REGISTRATION=1`, and `AU_WEBMASTER_PASSWORD` (from
`SERVER_LOGIN_PASSWORD`) to this handler. The password must match the Webmaster
account in the login database, as initialized by the PostgreSQL setup script.
Outside Docker, the tool also accepts `AU_LOGIN_PORT` (default 16757);
registration uses `AuAcctNew.ini` unless `AU_ACCOUNT_REGISTRATION` is supplied.

`/reset` currently displays an unavailable notice. `AuAcctSendPassword` is the
forgotten-password tool; `AuAcctPassword` changes a password using the current
password. The reset tool is deliberately not exposed yet: the login server
changes the password before invoking `mail`, and email delivery is not configured
in the server image.

`/emailchange` and `/passchange` serve static forms that POST to `AuAcctEmail`
and `AuAcctPassword`, respectively. Both verify the account's current password
against the deployment's `login-worker`; they do not need Webmaster credentials
or email delivery. The handlers return their existing result pages. The legacy
change protocol does not acknowledge database updates, so verify changes by
logging in with the new password or inspecting the account record.

To compile the web tools, rebuild the Apache image and start it without
rebuilding or restarting the game servers:

```bash
make server-web-up
make server-logs SERVICE=web
```

Open `http://localhost/`. `/index.html` redirects to the PHP homepage.
The page loads status once, then refreshes it only when **Refresh status** is
clicked. It does not poll in the background. The button is disabled while a request
is pending. HTTP/network failures leave any previous results visible with an error
message and allow another attempt. Requests have a 45-second browser timeout.
Version information is fetched separately once from `/server-status.php?version=1`,
so no executable is run during the initial homepage render. With JavaScript disabled,
the page provides a link to view status directly.
Each status command has a 12-second timeout, after which the page shows an error
for that service. Command failures also go to the Apache error log.

The Apache image copies `AuServerStatus`, `AuAcctNew`, `AuAcctEmail`,
`AuAcctPassword`, and `AuEmpireReport` from the `bin/` directory inside its own
build context, along with reporting libraries from `lib/`. After
compilation, `deploy/server/web/stage.sh` assembles the web files, executables, and libraries
into `build/server/web-context-<config>/` for development. The generated Compose
settings select that context. The web-up targets compile `web-tools` and stage
the context before rebuilding the image. The container has no executable mounts.

The normal `make server-up` compiles and packages both the servers and web tools.
For production, use `make server-prod-web-up` or the full `make server-prod-up`.
The production archive includes this complete context as `deploy/server/web/`,
with the five Release executables in `deploy/server/web/bin/` and their shared
libraries in `deploy/server/web/lib/`. Its Compose
settings use `WEB_BUILD_CONTEXT=./web`, relative to `deploy/server/compose.yaml`.
The archive needs no `build/` directory, source checkout, or C++ compiler.
Running `./start.sh` builds Apache from that packaged context alongside
PostgreSQL and the scene-master configuration image.

The avatar count links to `/playerson.php`, which runs `AuServerStatus -p` to
query the scene master for a fresh JSON list. PHP renders the avatar names and
AFK markers; failures display an unavailable message rather than an empty list.
`/playerson.html` redirects to this page. No player report file or history is
written, and the homepage no longer makes the additional player-list query.

The homepage displays recent public galactic chat and system broadcasts beside
the server status, stacking the panels on narrow screens. Both panels share the
same border, padding, and refresh icon styling. The chat border remains visible
while loading, when there are no recent messages, and when a request fails.
Each panel loads once and has its own refresh button; refreshing one does not
refresh the other. Neither panel polls automatically.

`/galactic.php` serves only the chat HTML fragment. It runs
`AuServerStatus -c`, which uses the existing scene-master management status query
and outputs the chat as JSON. PHP renders escaped message text, sender names,
galaxies, and UTC timestamps in chronological order. `/galactic` and
`/galactic.html` redirect to the homepage chat panel. An empty result displays
“No recent chatter.” Failed refreshes preserve any previously loaded messages
and show an error; a failed initial request displays an unavailable message.

The scene master retains approximately five minutes of recent chat. This page
is read-only and does not archive older messages. Neither the chat query nor the
homepage writes the old `Galactic.msgs` or `galactic.html` report files. No extra
volume is required. The status output retains its legacy timezone labels;
the persistent reports volume is not yet connected to these pages.

`WEB_PORT` defaults to `80`. `WEB_BIND_ADDRESS` defaults to `127.0.0.1` locally
and `0.0.0.0` in production, independently of the game's `BIND_ADDRESS`.
For example, a host already running a reverse proxy can set:

```dotenv
WEB_BIND_ADDRESS=127.0.0.1
WEB_PORT=8080
```

Set these in the environment's `.env` file, then rerun its web-up target.
For simultaneous local and production stacks on the same host, assign distinct
web ports or bind addresses. Apache currently serves HTTP; a host reverse proxy
can provide HTTPS.

Docker checks health by requesting `/cgi-bin/health.cgi` from inside the
container and checking its response. This verifies CGI execution as well as
HTTP availability. The health endpoint accepts only container-local requests;
the homepage remains accessible through the published web port.

Apache configuration, static site files, and CGI executables live in the image.
Generated reports have their own named volume,
`${COMPOSE_PROJECT_NAME}-web-reports`, mounted at
`/var/lib/openshores/reports` and served under `/reports/`. Apache mounts it
read-only; the `empire-standings` and `avatar-reports` services mount the same
volume read-write, publishing into separate subdirectories.
New volumes are initialized with owner UID/GID `1000:1000`, matching the web
image's `openshores` user. Directory listings and script execution are disabled
for reports, and PHP/script files are denied. Standings-only output is static HTML;
the legacy combined report still needs adaptation before it can be published.

The web image includes `/opt/openshores/bin/AuEmpireReport` and the report
runners. The `empire-standings` container uses this same image with the report
scheduler as its entrypoint and user `1000:1000`; it does not start Apache. The
Apache container keeps its read-only report mount, while this container writes
to the existing named volume. Reporting executables are not packaged in the
game-server image.

The executable's `-standings`
option generates only empire standings on stdout and flag images in the current
working directory; `-report` retains the legacy combined standings, avatar pages,
and blueprint exchange output. Database connections use `AU_UNIVERSE_DB_HOST`
and `AU_LOGIN_DB_HOST` (both default to `postgres`), with user `audog` and
`AU_DB_PASSWORD` (default `Gold`), matching the game servers' deployment settings.
The legacy `-huey` and `-u0` flags override the universe database host when supplied.
The `empire-standings` service generates a report immediately after PostgreSQL
is healthy, then refreshes it periodically. Apache can start while reports are
being generated; on first deployment, report pages are available after the first
successful refresh. It publishes
`/reports/standings/index.html`, linked from the homepage as `/standings`.
Generation has a five-minute timeout and a lock to prevent overlapping refreshes;
the published page is replaced only after the generator succeeds and produces
nonempty output. The generator's existing layout is retained, but standings-only
output omits the old PHP header include and unavailable avatar links.

Both report services default to waiting **20 minutes after each run finishes**
before starting the next run. Change the shared interval in
`deploy/server/environments/local.env` or `deploy/server/environments/production.env`:

```dotenv
REPORT_INTERVAL_MINUTES=20
```

The value must be a whole number from 1 to 525600. After changing it, run
`make server-reports-up` or `make server-prod-reports-up` with the stack already
running. These targets regenerate configuration and restart only the report
schedulers; they do not build or restart the game servers or Apache. An interval
change takes effect when the report containers are recreated. Existing configs
that omit the setting default to 20 minutes.

For a packaged release, edit `REPORT_INTERVAL_MINUTES` in the extracted
`compose.env`, then apply it without rebuilding:

```sh
docker compose --env-file compose.env -f deploy/server/compose.yaml \
  -f workers.compose.json up --detach --no-deps --force-recreate empire-standings avatar-reports
```

Each service retries after its configured interval if generation fails, logging
the failure and continuing to serve the previous published report. Runs do not
overlap; the existing locks also guard against simultaneous manual refreshes.
Stopping a report container interrupts its current job or wait.

With the stack running and the updated web image installed, refresh manually
(these commands run once and do not build):

```sh
make server-standings
# Production:
make server-prod-standings
```

From an extracted release directory, the equivalent is:

```sh
docker compose --env-file compose.env -f deploy/server/compose.yaml \
  -f workers.compose.json run --rm --no-deps empire-standings --once
```

To rebuild reporting changes and publish fresh standings without restarting game
servers, use `make server-web-up` (production: `make server-prod-web-up
SERVER_CONFIG=release`). The web-up target rebuilds the shared image and recreates
Apache and both report schedulers, which begin an immediate refresh. Full-stack
`server-up` targets also build the web image and start the report services.
On a newly initialized universe, refresh again after the scene server has created
the galaxy and players have established empires.

The homepage's `/avatars` link opens a static avatar directory with views by
name and empire. Each avatar links to its profile, including its selfie when
present. `AuEmpireReport -avatars` generates these pages and their flag images
independently of standings or blueprint exchange. The current legacy profile
layout is retained; deleted avatars and pirate-empire avatars are excluded, and
the old synthetic Silverwill entry is not added.

The `avatar-reports` service uses the same web image and begins on stack
startup after PostgreSQL is healthy, then uses the same configurable interval.
It publishes under `/reports/avatars/`, with a five-minute timeout and a separate
lock. A failed generation leaves the previous published pages in place. After a
successful generation, obsolete profiles and images are removed from this
subdirectory.

To build and publish avatar changes while the stack is running:

```sh
make server-web-up
# Production:
make server-prod-web-up SERVER_CONFIG=release
```

For an additional immediate refresh, `make server-avatars` and
`make server-prod-avatars` run once without building. From an extracted release directory:

```sh
docker compose --env-file compose.env -f deploy/server/compose.yaml \
  -f workers.compose.json run --rm --no-deps avatar-reports --once
```

The report volume survives image rebuilds, container replacement, and normal
`server-down`/`server-prod-down`. `server-reset` and Compose `down --volumes`
delete it along with the database. Back up reports separately from PostgreSQL.
Apache access/error logs go to Docker's bounded logs rather than this volume.

Standings and avatar pages load `/styles.css`, shared with the homepage. Their
shared rules are scoped under `.generated-page` in `deploy/server/web/styles.css`.
Empire standings use the body class `.standings-page` and `standings-*` classes;
avatar directories and profiles use `.avatars-page` and `avatar-*` classes.
The `page-*` classes are shared helpers: for example, `.standings-page .page-title`
and `.avatars-page .page-title` let you style their titles independently. These
selectors keep generated page styling separate from the homepage and chat. Generated markup
uses classes for colours, typography, spacing and tables; flag dimensions remain
HTML attributes based on each image. The absolute stylesheet URL works with the
reports' `<base>` URLs and the `/standings` and `/avatars` aliases.

After installing this change, regenerate both reports once to pick up the class
names and stylesheet link. `make server-web-up` (or `make server-prod-web-up`)
rebuilds the shared image and restarts the schedulers, which regenerate immediately.
Later CSS-only changes need the updated web image; existing report HTML will use
the new stylesheet without requiring regeneration. Refresh the browser if it
has cached the old stylesheet.

### Connect a local client

Linux:

```bash
(cd build/linux-debug/bin && ./AuSceneClient -alocalhost -log)
```

Windows:

```powershell
Push-Location .\build\windows-debug\bin
.\AuSceneClient.exe -alocalhost -log
Pop-Location
```

`-a` points the login, mail, and scene masters at `localhost`. Their fixed ports are `16757`, `16758`, and `16759`. Redirected workers advertise `127.0.0.1` and their configured published ports.

VS Code includes local Debug client launches for Linux and Windows. Launch targets do not start or restart the stack; run `make server-up` first. The Linux-only `Linux Server: Attach scene-worker-1` launch attaches GDB to the first scene worker. The local Debug worker receives ptrace permissions; production workers do not.

### AuServerMon

In AuServerMon, choose **Connect > Local Servers** to connect to the scene master,
manager, and first worker. **Local Workers** connects just the first worker;
**Local Server...** lets you select one of the three endpoints.

| Scene service | Management endpoint |
| --- | --- |
| Master | `127.0.0.1:18062` |
| Manager | `127.0.0.1:18063` |
| Worker 1 | `127.0.0.1:18064` |

These match `monitor.compose.yaml`, included by the local and production server
Make commands. AuServerMon uses the same addresses in either environment, with
no dependency on a checkout, deployment paths, or Docker container names. It
does not connect automatically on startup. After connecting, use **Servers >
Start Monitoring** to select a polling interval. Failed connections show the
endpoint and socket error, and repeated connection requests skip endpoints that
are already connected or awaiting a connection.

Run the monitor on the Docker host, or forward these same local ports through
SSH when running it remotely. These built-in choices cover the three published
management endpoints; they do not discover additional workers. The legacy
system-reassignment address mapping still requires a separate update.

AuServerMon's account manager and universe queries connect directly to PostgreSQL.
The monitor override publishes PostgreSQL only on `127.0.0.1:5432`. Apply this
mapping to an existing local deployment from the repository root, without a build:

```bash
docker compose \
  --env-file build/server/local/compose.env \
  -f deploy/server/compose.yaml \
  -f build/server/local/workers.compose.json \
  -f deploy/server/monitor.compose.yaml \
  up -d --no-build --no-deps --pull never postgres
```

This recreates PostgreSQL and briefly interrupts database access; its named data
volume is retained. For a production deployment, replace both `build/server/local`
paths with `build/server/production`. Future server Make commands include the
mapping automatically. Port 5432 must be available on the Docker host.

The monitor reads the following environment variables from the process that
launches it, independently of any repository or deployment files:

| Variable | Default | Purpose |
| --- | --- | --- |
| `AU_LOGIN_DB_HOST` | `127.0.0.1` | Host of `auLogin` |
| `AU_UNIVERSE_DB_HOST` | `127.0.0.1` | Host of `auUniverse` |
| `AU_DB_PORT` | `5432` | PostgreSQL port for both databases |
| `AU_DB_USER` | `audog` | Existing application database role |
| `AU_DB_PASSWORD` | `Gold` | Password for that role |

Defaults match a local deployment with unchanged database credentials. For a
custom or production deployment, use the `AU_DB_PASSWORD` used when initializing
that database, not the PostgreSQL administrator or database-owner password.
For example, in Bash, enter it without displaying it or putting it in shell history:

```bash
read -r -s -p 'Application database password: ' AU_DB_PASSWORD
printf '\n'
export AU_DB_PASSWORD
./build/linux-release/bin/AuServerMon
unset AU_DB_PASSWORD
```

The monitor does not save credentials in Qt settings. Account connection errors
appear in a dialog; universe-query errors appear in the query report. Explicit
queries can be retried after a connection failure. A failed background game-data
load pauses until **Reload Empire Data** is requested or an explicit query succeeds.
When using SSH for remote monitoring, also forward local port 5432 to the server's
`127.0.0.1:5432` (or use another local port and set `AU_DB_PORT` to match). Keep the
Docker mapping bound to loopback. Linux needs the Qt PostgreSQL plugin, typically
provided by the `libqt5sql5-psql` package.

### Windows clients with Docker Engine inside WSL 2

When Docker Engine runs directly inside WSL 2 in NAT mode, Windows `localhost`
forwarding handles TCP but does not forward the game's UDP controls. Login and
scene loading can work while attacking and switching held items fail. Movement
can still appear to work because the client also processes it locally.

Check the networking mode with `wslinfo --networking-mode`. For NAT mode, run
`hostname -I` inside the WSL distribution hosting Docker and note its WSL IPv4
address (usually the first address, not a Docker bridge address). Set both values
in `deploy/server/environments/local.env` to that address, preserving any other
overrides. For example:

```dotenv
BIND_ADDRESS=172.18.91.213
PUBLIC_HOST=172.18.91.213
```

Run `make server-up` to apply the settings, then reconnect the Windows client
using that address in the connection target field or with `-a172.18.91.213`.
Use the actual WSL address in place of the example. Set the same address in the
VS Code launch arguments when debugging. WSL's address can change after a WSL
restart; update both settings and restart the stack when it does.

This publishes the ports on the WSL interface and advertises an address Windows
can reach over both TCP and UDP. Changing only the client connection target is
insufficient because the servers redirect the client to `PUBLIC_HOST`.

See Microsoft's [WSL localhost implementation notes](https://github.com/microsoft/WSL/blob/master/doc/docs/technical-documentation/localhost.md)
for the TCP forwarding behavior in NAT mode.

## Single-host production

### Host configuration

Copy the production template and set the public host and every secret:

```bash
cp deploy/server/environments/production.env.example \
   deploy/server/environments/production.env
$EDITOR deploy/server/environments/production.env
```

The renderer rejects template hostnames and unset secrets. Validate without starting services:

```bash
make server-prod-config
```

Start and operate production:

```bash
make server-prod-up
make server-prod-status
make server-prod-logs
make server-prod-down
```

Scale during deployment:

```bash
make server-prod-up SCENE_WORKERS=8
```

The public firewall must permit:

- the configured web TCP port (`80` by default) when serving HTTP directly;
- TCP `16757`, `16758`, and `16759` for the masters;
- the configured login-worker, mail-worker, and scene-manager TCP ports;
- each scene worker's TCP/UDP scene port and TCP chat port.

All public game roles currently advertise the same `PUBLIC_HOST` with unique ports. `server-prod-down` preserves the production PostgreSQL and web report volumes. There is deliberately no production reset target.

The original protocol does not provide modern transport security. Public deployment still requires host hardening, firewall policy, monitoring, backups, and an explicit assessment of the legacy authentication protocol.

### GitHub Actions image publishing

Pushes to `main` publish `ghcr.io/<username>/openshores-server` with `latest` and
commit SHA tags, plus a deployment artifact named `server-production-compose-<commit SHA>`.

Set these repository Actions secrets:

- `GHCR_USERNAME`: registry username.
- `GHCR_TOKEN`: token with permission to publish the image.
- `SERVER_PRODUCTION_ENV`: production `.env` contents; use
  [`production.env.example`](environments/production.env.example) as a template.

Download the artifact from the workflow run. On a host with Docker Engine and
Docker Compose installed, extract its tar into a dedicated directory and start:

```bash
tar -xf server-production-compose.tar
./start.sh
```

For private images, run `docker login ghcr.io` first. Run `./stop.sh` to stop the
stack while preserving the database and web reports. **The artifact contains production credentials.**

## Configuration model

Important settings:

| Setting | Purpose |
| --- | --- |
| `SERVER_BUILD_CONFIG` | `debug`, `release`, or `relwithdebinfo` |
| `SCENE_WORKERS` | Number of generated scene-worker containers |
| `ATOM_MANAGERS` | Atom-manager bundles per scene worker |
| `BIND_ADDRESS` | Host interface used for published game ports |
| `WEB_BIND_ADDRESS` | Host interface used for HTTP; independent of game binding |
| `WEB_PORT` | Published HTTP port, default `80` |
| `PUBLIC_HOST` | Host/IP placed in client redirect packets |
| `*_PORT` | Published role and worker ports |
| `POSTGRES_VOLUME` | Persistent database volume name |
| `AU_DB_PASSWORD` | Runtime database-user password |
| `SERVER_LOGIN_PASSWORD` | Password used by scene server accounts |

Generated Compose environment and worker services live under `build/server/local/` or `build/server/production/`.

Application defaults remain compatible with the original deployment. Docker supplies private login, mail, and database endpoints through environment variables; workers separately report the configured public host and port to clients.

### Scene-master INI file

The scene-master image copies an editable deployment file to
`/etc/openshores/scene-master.ini`. This file does not use a volume or a bind mount:

| Stack | Deployment file | Initial setting |
| --- | --- | --- |
| Local | `deploy/server/environments/local.ini` | `galaxyGen=1galaxy` |
| Production | `deploy/server/environments/production.ini` | `galaxyGen=all` |

For a fresh universe containing only Shores of Hazeron, set:

```ini
[Settings]
galaxyGen=1galaxy
```

Use `galaxyGen=all` to generate every galaxy. Existing galaxies are retained;
changing the setting does not reduce an existing universe to one galaxy.
Managers and workers load the galaxies from the database and do not need this INI.

Compose passes `=/etc/openshores/scene-master` to `AuSceneServer`; the application
appends `.ini`. At launch, Compose builds a small image derived from the game
server image, using `environments/scene-master.Dockerfile` and the selected INI.
The build context is relative to the deployment's Compose file, so it works
from either the repository or an extracted release archive. An INI change
rebuilds this configuration layer and recreates the scene-master container when
you rerun `make server-up`, `make server-prod-up`, or the release's `./start.sh`.
The prebuilt game server executable is reused. A plain container restart keeps
the INI that was copied into its image.

These INI files are maintained directly, not generated by `configure.py`.
Keep deployment credentials and network settings in the existing `.env` files:
INI settings override command-line arguments when both specify the same key.
The production deployment archive includes
`deploy/server/environments/scene-master.Dockerfile` and
`deploy/server/environments/production.ini`, which can be edited after extraction
and before running `./start.sh`.

### Legacy city construction

Legacy universes use a separate **F11 Construction** window and reuse the existing
construction labor controls in **F10 Labor**, adapted to legacy city sites.
F11 selects historical industries, public design variants, materials,
levels, road grading and clearing. Its world cursor places buildings with one
click and roads with two clicks. The original icon rail selects categories;
press a building icon to begin placement. Plus/minus or the mouse wheel change
building level counts. Road grading and width controls are on the Roads &
Landscape page. F10 supplies construction materials, contributes
player labor, enables citizen labor, and halts or resumes projects. Extensions
retain the existing building's design and material. A new town square requires
an empire flag and a valid city name.

Construction, economy and ship personnel services use legacy city state version 4. Fresh database
initialization creates the current legacy city tables. For an existing universe,
stop the servers and apply `postgres/schema/legacycities.sql` to `auUniverse`
using the existing deployment database administration procedure. That script
updates the version constraint and retains version-1/2/3 envelopes; ordinary city
saves write version 4, including the city's garrison troop pool. Spacecraft factory
processes also use a versioned manufacturing envelope; existing processes remain
readable. Also apply `postgres/schema/legacyspacecraft.sql` for the per-empire
Legacy blueprint publication table. Rebuild both client and servers manually
before resuming.

After a manual client/server build, verify founding with and without a flag,
material shortages and repeated labor, halt/resume, multi-level completion and
extensions, each road/landscaping type, demolition and town-square deletion
restrictions, and restart/reconnect with work partially complete. Check the
separate F10/F11 windows and a modern server as well. Stage-5 economy recovery was
accepted on 19 October 2026. Stage-6 spacecraft manufacturing and city ship
services require a new manual build and gameplay acceptance. The full political
lifecycle remains stage 7.

F10 now exposes historical manufacturing recipes, citizen employment and minimum
TL, plus bank loans, treasury transfers, reward redemption and capital
designation at the appropriate sites. The Govern Taxes page provides the wealth
levy. Citizens and government use separate city accounts. Original capital
recognition is cached until the next city cycle: government deposits made before
recognition are deducted from the player but discarded by the treasury. The bank
page displays that condition. DNA3 citizens retain the original calculations;
DNA4 citizens use their existing body and diet data within the legacy economy.

For economy acceptance, include a complete food/air/power chain, worker
shortages, housing and service morale, mixed quality/TL inputs, research and
building upgrades, recipe-change refunds, livestock and caged animals, bank
loans and both account ledgers, all tax categories and a wealth levy, connected
city trade and power, and save/restart during running production. City population
and account reports normally advance about every 13 minutes. Static catalogue
checks do not replace these manual checks.

### Legacy spacecraft storage

Legacy designs are stored in `n_LegacyScDesign` in `auUniverse`. Scene servers
load these records at startup, check their hashes, reparse the original bytes,
and recompute geometry and statistics before publishing an immutable cache.
Geometry diagnostics remain advisory at this stage. Online uploads also have
limits on file size and geometry analysis work.

Original detail IDs and designer coordinates are preserved. Separate one-based
32-bit record IDs follow the immutable file order. The server derives the hull
center for conversion to live ship coordinates. On the first spawn, the server
stores the derived runtime geometry as an ordinary `n_ScDesign` blueprint and
links it through `n_LegacyScDesign.runtimeDesignId`. Repeated spawns reuse that
blueprint. The original legacy bytes, hash, IDs, and coordinates remain intact.

New databases include the legacy design schema. For an existing local database,
apply the additive schema once before restarting the updated scene servers:

```bash
docker compose --env-file build/server/local/compose.env \
  -f deploy/server/compose.yaml exec -T --user postgres postgres \
  psql -X --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
  < deploy/server/postgres/schema/legacyspacecraft.sql
```

For production, use `build/server/production/compose.env` instead. The script is
safe to rerun and preserves existing data. Run it again after updating to the
architect spawn feature: it adds the runtime blueprint link and its update
permission. Its original bytes and hash remain immutable. From an extracted
release, use its
`compose.env` and packaged schema path.

To inspect stored identities without printing design blobs:

```bash
docker compose --env-file build/server/local/compose.env \
  -f deploy/server/compose.yaml exec -T --user postgres postgres \
  psql -X --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
  --command 'SELECT id, name, runtimeDesignId, contentHash, octet_length(sourceData) AS bytes FROM n_LegacyScDesign ORDER BY id;'
```

In the architect menu (Shift+F11), open the Design tab and use the Legacy
spacecraft controls. Enable architect powers, browse for a `.ship` file, enter a
name, and click Upload design. The name defaults to the filename without its
extension. Names must contain 1 to 128 characters. Uploads are limited to 8 MiB,
512 rooms, 1,024 stored vertices per room, the historical deck range, and a
bounded estimate of boundary-comparison work. The server checks these limits
and parses the original bytes independently of the client.

Uploading identical bytes retains the database ID and updates the chosen name.
Naming does not rewrite the `.ship` bytes or its original IDs. The active worker
adds the accepted design to its cache immediately. Refresh reads the database
catalog, including designs uploaded through other workers. Existing designs
whose stored names are empty are displayed using their source filenames.

Select a stored design and click Spawn nearby to create a stationary debug
spacecraft near your avatar. The client sends only the catalog ID; the server
checks architect powers, loads the stored bytes, derives or loads the runtime
blueprint, and chooses the placement. NPC navigation, subsystem mechanics, and
console controls are deferred. Both client and server need the updated protocol;
build them manually. No Docker `.ship` packaging or directory scans are used.

The first spawn stores a derived `n_ScDesign` blueprint and links it through
`n_LegacyScDesign.runtimeDesignId`. Subsequent ships reuse it; live instances
persist through the existing `a_Sc` table. Generated geometry subtracts the
legacy author center, while the source bytes retain their original coordinates.
Legacy detail identities remain source-index-plus-one; generated modern mesh
parts use a separate 16-bit ID space. These are geometry adapters, with open
doors and hatches and static equipment. The server includes a compact material
map with the runtime blueprint; clients recover the original UVs and draw their
bundled legacy textures at full resolution. Procedural hull, glass, and equipment
surfaces use linear filtering without mipmaps and the recovered alpha test.
Resource models retain their original texture filtering and transparency.
The original `.ship` bytes stay on the server. Existing derived blueprints gain
their material map when loaded, and clients refresh older cached copies; no
schema update or respawn is needed for this rendering change. The fragment
shaders are included in Qt resources by an ordinary client build.
Their persisted debug flag supplies air, gravity, and
lighting and disables movement, subsystem processing, and damage. It is set by
the authorized server spawn path, independently of editable blueprint metadata.

Manual checks after your build:

1. Upload a known design with a custom name; verify it appears and is selected.
2. Upload the same file with another name; confirm the ID is unchanged and the
   name updates, including after a server restart.
3. Refresh from another architect client and verify the stored name and ID.
4. Try a truncated file, an empty name, and an oversized file; confirm rejection
   leaves existing designs intact.
5. Select a stored design and spawn it; verify it appears nearby, remains
   stationary, and can be boarded. Begin with a small design, then `Station.ship`.
   Try walking through its rooms and openings, climbing stairs, and descending
   through open hatches. Architect flight may be needed to reach elevated ships.
6. Spawn the same design again; verify a second ship appears and `runtimeDesignId`
   stays the same. Restart the scene server and check both ships load again.
7. Turn architect powers off and try uploading or spawning; verify the server
   refuses both operations. Ordinary accounts cannot manage the catalog.
8. Compare hull glass, equipment, and consoles with the offline designer. Check
   glass from outside and inside, including in space, and verify the drive skins
   remain visible. Reconnect after restarting the updated server and verify
   existing ships use the original textures. Check an ordinary modern ship for
   rendering regressions as well.

Scene startup and upload logs use the prefix `Legacy spacecraft catalog:`.

## Logs and data

`make server-logs` and `make server-prod-logs` use Docker Compose logs. Each service uses bounded Docker local logs: three files of up to 20 MiB.

PostgreSQL state lives in the named volume selected by `POSTGRES_VOLUME`. Back up that volume before upgrades or production topology changes.

Generated web reports live in `${COMPOSE_PROJECT_NAME}-web-reports`. Both data
volumes are preserved by normal shutdown commands.
