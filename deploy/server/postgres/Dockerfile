FROM postgres:14-bookworm

COPY deploy/server/postgres/init.sh /docker-entrypoint-initdb.d/10-openshores.sh
COPY deploy/server/postgres/schema/initlogin.sql /opt/openshores/sql/initlogin.sql
COPY deploy/server/postgres/schema/initmail.sql /opt/openshores/sql/initmail.sql
COPY deploy/server/postgres/schema/inituniverse.sql /opt/openshores/sql/inituniverse.sql
COPY deploy/server/postgres/schema/initstatic.sql /opt/openshores/sql/initstatic.sql
COPY deploy/server/postgres/schema/legacyspacecraft.sql /opt/openshores/sql/legacyspacecraft.sql
COPY deploy/server/postgres/schema/gamegeneration.sql /opt/openshores/sql/gamegeneration.sql
COPY deploy/server/postgres/schema/legacycities.sql /opt/openshores/sql/legacycities.sql
COPY deploy/server/postgres/schema/initlegacystatic.sql /opt/openshores/sql/initlegacystatic.sql

RUN chmod 0755 /docker-entrypoint-initdb.d/10-openshores.sh
