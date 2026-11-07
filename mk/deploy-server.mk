SERVER_CONFIG ?=
SCENE_WORKERS ?=
SERVER_LOG_TAIL ?= 200
SERVER_PRODUCTION_ENV ?= deploy/server/environments/production.env

LOCAL_SERVER_DIR := build/server/local
LOCAL_SERVER_ENV := $(LOCAL_SERVER_DIR)/compose.env
LOCAL_SERVER_WORKERS := $(LOCAL_SERVER_DIR)/workers.compose.json
PRODUCTION_SERVER_DIR := build/server/production
PRODUCTION_SERVER_ENV := $(PRODUCTION_SERVER_DIR)/compose.env
PRODUCTION_SERVER_WORKERS := $(PRODUCTION_SERVER_DIR)/workers.compose.json
LOCAL_COMPOSE = BUILDX_NO_DEFAULT_ATTESTATIONS=1 docker compose \
	--env-file "$(LOCAL_SERVER_ENV)" \
	-f deploy/server/compose.yaml -f "$(LOCAL_SERVER_WORKERS)" \
	-f deploy/server/monitor.compose.yaml
PRODUCTION_COMPOSE = BUILDX_NO_DEFAULT_ATTESTATIONS=1 docker compose \
	--env-file "$(PRODUCTION_SERVER_ENV)" \
	-f deploy/server/compose.yaml -f "$(PRODUCTION_SERVER_WORKERS)" \
	-f deploy/server/monitor.compose.yaml

.PHONY: server-config server-ensure-config server-image server-up server-down \
        website-up website-down website-prod-up website-prod-down \
        server-web-up server-reports-up server-standings server-avatars server-logs server-status server-reset \
        server-prod-config server-prod-ensure-config server-prod-image \
        server-prod-up server-prod-web-up server-prod-reports-up server-prod-standings server-prod-avatars server-prod-down server-prod-logs server-prod-status

server-config:
	python3 deploy/server/configure.py --environment local \
		$(if $(strip $(SERVER_CONFIG)),--config "$(SERVER_CONFIG)") \
		$(if $(strip $(SCENE_WORKERS)),--scene-workers "$(SCENE_WORKERS)")

server-ensure-config:
	@if [ ! -f "$(LOCAL_SERVER_ENV)" ] || [ ! -f "$(LOCAL_SERVER_WORKERS)" ]; then \
		python3 deploy/server/configure.py --environment local; \
	fi

server-image: server-config
	@config=$$(awk -F= '$$1 == "SERVER_BUILD_CONFIG" { print $$2 }' "$(LOCAL_SERVER_ENV)"); \
	image=$$(awk -F= '$$1 == "SERVER_IMAGE" { print $$2 }' "$(LOCAL_SERVER_ENV)"); \
	target=$$(awk -F= '$$1 == "SERVER_IMAGE_TARGET" { print $$2 }' "$(LOCAL_SERVER_ENV)"); \
	bash deploy/server/image/build.sh "$$config" "$$image" "$$target" "$(LINUX_IMAGE)"

server-up: server-image
	$(LOCAL_COMPOSE) up --build --detach --remove-orphans --wait

server-web-up: website-up

website-up: server-config
	@config=$$(awk -F= '$$1 == "SERVER_BUILD_CONFIG" { print $$2 }' "$(LOCAL_SERVER_ENV)"); \
	HAZERON_LINUX_IMAGE="$(LINUX_IMAGE)" HAZERON_BUILD_CONFIG="$$config" \
		bash docker/linux/build.sh web-tools && \
	bash deploy/server/web/stage.sh "$$config"
	$(LOCAL_COMPOSE) up --build --detach --wait --no-deps --force-recreate web empire-standings avatar-reports

website-down: server-ensure-config
	$(LOCAL_COMPOSE) stop web empire-standings avatar-reports
	$(LOCAL_COMPOSE) rm --force web empire-standings avatar-reports

server-reports-up: server-config
	$(LOCAL_COMPOSE) up --detach --wait --no-deps --force-recreate empire-standings avatar-reports

server-standings: server-ensure-config
	$(LOCAL_COMPOSE) run --rm --no-deps empire-standings --once

server-avatars: server-ensure-config
	$(LOCAL_COMPOSE) run --rm --no-deps avatar-reports --once

server-down: server-ensure-config
	$(LOCAL_COMPOSE) --profile website down --remove-orphans

server-logs: server-ensure-config
	$(LOCAL_COMPOSE) logs --follow --tail="$(SERVER_LOG_TAIL)" $(SERVICE)

server-status: server-ensure-config
	$(LOCAL_COMPOSE) ps

server-reset: server-image
	@if [ "$(CONFIRM)" != "yes" ]; then \
		printf 'Delete the local universe database and web reports, then restart? [y/N] '; \
		read answer; \
		case "$$answer" in y|Y|yes|YES) ;; *) echo 'Cancelled.'; exit 1;; esac; \
	fi
	$(LOCAL_COMPOSE) --profile website down --volumes --remove-orphans
	$(LOCAL_COMPOSE) up --build --detach --remove-orphans --wait

server-prod-config:
	python3 deploy/server/configure.py --environment production \
		--env-file "$(SERVER_PRODUCTION_ENV)" \
		$(if $(strip $(SERVER_CONFIG)),--config "$(SERVER_CONFIG)") \
		$(if $(strip $(SCENE_WORKERS)),--scene-workers "$(SCENE_WORKERS)")

server-prod-ensure-config:
	@if [ ! -f "$(PRODUCTION_SERVER_ENV)" ] || [ ! -f "$(PRODUCTION_SERVER_WORKERS)" ]; then \
		python3 deploy/server/configure.py --environment production \
			--env-file "$(SERVER_PRODUCTION_ENV)"; \
	fi

server-prod-image: server-prod-config
	@config=$$(awk -F= '$$1 == "SERVER_BUILD_CONFIG" { print $$2 }' "$(PRODUCTION_SERVER_ENV)"); \
	image=$$(awk -F= '$$1 == "SERVER_IMAGE" { print $$2 }' "$(PRODUCTION_SERVER_ENV)"); \
	target=$$(awk -F= '$$1 == "SERVER_IMAGE_TARGET" { print $$2 }' "$(PRODUCTION_SERVER_ENV)"); \
	bash deploy/server/image/build.sh "$$config" "$$image" "$$target" "$(LINUX_IMAGE)"

server-prod-up: server-prod-image
	$(PRODUCTION_COMPOSE) up --build --detach --remove-orphans --wait

server-prod-web-up: website-prod-up

website-prod-up: server-prod-config
	@config=$$(awk -F= '$$1 == "SERVER_BUILD_CONFIG" { print $$2 }' "$(PRODUCTION_SERVER_ENV)"); \
	HAZERON_LINUX_IMAGE="$(LINUX_IMAGE)" HAZERON_BUILD_CONFIG="$$config" \
		bash docker/linux/build.sh web-tools && \
	bash deploy/server/web/stage.sh "$$config"
	$(PRODUCTION_COMPOSE) up --build --detach --wait --no-deps --force-recreate web empire-standings avatar-reports

website-prod-down: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) stop web empire-standings avatar-reports
	$(PRODUCTION_COMPOSE) rm --force web empire-standings avatar-reports

server-prod-reports-up: server-prod-config
	$(PRODUCTION_COMPOSE) up --detach --wait --no-deps --force-recreate empire-standings avatar-reports

server-prod-standings: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) run --rm --no-deps empire-standings --once

server-prod-avatars: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) run --rm --no-deps avatar-reports --once

server-prod-down: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) --profile website down --remove-orphans

server-prod-logs: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) logs --follow --tail="$(SERVER_LOG_TAIL)" $(SERVICE)

server-prod-status: server-prod-ensure-config
	$(PRODUCTION_COMPOSE) ps
