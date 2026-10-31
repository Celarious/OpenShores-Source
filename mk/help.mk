.PHONY: help

help:
	@if [ -t 1 ] && [ -z "$${NO_COLOR:-}" ]; then \
		heading='\033[1;36m'; reset='\033[0m'; \
	else \
		heading=; reset=; \
	fi; \
	printf '%b\n' \
		"$${heading}Builds$${reset}" \
		'  make                      Show this help' \
		'  make all                  Build Linux programs and Windows clients' \
		'  make linux                Build all Linux programs' \
		'  make linux-client         Build the Linux scene client' \
		'  make linux-clients        Build all Linux clients' \
		'  make linux-servers        Build all Linux servers' \
		'  make win                  Build all Windows clients' \
		'  make win-client           Build the Windows scene client' \
		'  make mac-client           Build and package the native Mac client' \
		'  make mac-package          Package an existing native Mac build' \
		'  make mac-dmg              Create a DMG from an already packaged Mac app' \
		'  make clean                Remove all build outputs and cache volumes' \
		'' \
		"$${heading}Release packages (always Docker)$${reset}" \
		'  make linux-package        Build Linux game and launcher archives' \
		'  make win-package          Build Windows game and launcher archives' \
		'  make release-package      Build both platforms’ archives' \
		'' \
		"$${heading}Local server deployment$${reset}" \
		'  make server-up            Build and start the local stack' \
		'  make server-web-up        Compile web tools and start local Apache' \
		'  make server-reports-up    Apply report interval and restart report schedulers' \
		'  make server-standings     Refresh local empire standings (no build)' \
		'  make server-avatars       Refresh local avatar pages (no build)' \
		'  make server-down          Stop it, preserving data' \
		'  make server-logs          Follow logs' \
		'  make server-status        Show container and health status' \
		'  make server-reset         Delete local data and restart' \
		'' \
		"$${heading}Production server deployment$${reset}" \
		'  make server-prod-up       Build and start production' \
		'  make server-prod-web-up   Compile web tools and start production Apache' \
		'  make server-prod-reports-up Apply production report interval and restart schedulers' \
		'  make server-prod-standings Refresh production empire standings (no build)' \
		'  make server-prod-avatars  Refresh production avatar pages (no build)' \
		'  make server-prod-down     Stop production, preserving data' \
		'  make server-prod-logs     Follow production logs' \
		'  make server-prod-status   Show container and health status' \
		''; \
	if [ "$(FULL)" = 1 ]; then \
		printf '%b\n' \
			"$${heading}Advanced builds and assets$${reset}" \
			'  make linux-cgi            Build account and status programs' \
			'  make linux-tools          Build admin and content tools' \
			'  make linux-toolchain      Build the Linux Docker toolchain image' \
			'  make win-toolchain        Build the Windows Docker toolchain image' \
			'  make assets               Rebuild all content archives' \
			'  make audio-assets         Rebuild the runtime sound archive' \
			'  make shader-assets        Rebuild the embedded shader archive' \
			'  make story-assets         Validate and rebuild story archives' \
			'' \
			"$${heading}Deployment preparation$${reset}" \
			'  make server-config        Generate local configuration' \
			'  make server-image         Configure and build the local server image' \
			'  make server-prod-config   Generate production configuration' \
			'  make server-prod-image    Configure and build the production image' \
			'' \
			"$${heading}Advanced options$${reset}"; \
		printf '  %-25s  %s\n' \
			'PARALLEL=N'                'Native build parallelism' \
			'CMAKE=path'                'CMake executable (default: cmake)' \
			'GENERATOR=name'            'Native generator (default: Ninja)' \
			'NATIVE_BUILD_DIR=path'     'Default: build/native-linux-<config>' \
			'LINUX_IMAGE=name'          'Default: hazeron-linux-qt5' \
			'WINDOWS_IMAGE=name'        'Default: hazeron-mingw-qt5' \
			'LINUX_CACHE_VOLUME=name'   'Default depends on CONFIG' \
			'WINDOWS_CACHE_VOLUME=name' 'Default depends on CONFIG' \
			'ASSETS_CACHE_VOLUME=name'  'Default: hazeron-assets-cache' \
			'SERVER_PRODUCTION_ENV=path' 'Production environment file' \
			'SERVER_LOG_TAIL=N'         'Log history lines (default: 200)' \
			'CONFIRM=yes'               'Skip the local server-reset prompt'; \
		printf '\n'; \
	fi; \
	printf '%b\n' "$${heading}Options$${reset}"; \
	printf '  %-18s  %s\n' \
		'CONFIG=type'        'release, debug, relwithdebinfo (default: release)' \
		'NATIVE=1'           'Native Linux compilation only (default: 0)' \
		'SERVER_CONFIG=type' 'debug, release, relwithdebinfo (server builds)' \
		'SCENE_WORKERS=N'    'Worker count (default: 1)' \
		'SERVICE=name'       'Show logs for one Compose service'; \
	printf '%b\n' \
		'' \
		'  With NATIVE=1, assets, packages and deployment still use Docker.' \
		'  make help FULL=1    Include advanced commands and options' \
		'' \
		"$${heading}Examples$${reset}" \
		'  make linux-client CONFIG=debug' \
		'  make linux-client NATIVE=1 CONFIG=debug' \
		'  make win-client CONFIG=relwithdebinfo' \
		'  make server-up SCENE_WORKERS=3'
