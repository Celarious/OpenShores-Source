BEGIN;

CREATE TABLE IF NOT EXISTS a_LegacyCity
(
  name text NOT NULL DEFAULT '',
  controllingEmpire bigint NOT NULL DEFAULT 0,
  allegianceEmpire bigint NOT NULL DEFAULT 0,
  population integer NOT NULL DEFAULT 0 CHECK (population >= 0),
  satisfaction smallint NOT NULL DEFAULT 1 CHECK (satisfaction BETWEEN -20 AND 20),
  stateVersion smallint NOT NULL DEFAULT 4 CHECK (stateVersion BETWEEN 1 AND 4),
  stateData bytea NOT NULL CHECK (octet_length(stateData) BETWEEN 1 AND 8388608),
  reportHistory bytea NOT NULL DEFAULT ''::bytea
) INHERITS (a_Atom);
-- Keep version-1/2/3 envelopes readable until their next ordinary city save.
ALTER TABLE a_LegacyCity DROP CONSTRAINT IF EXISTS a_legacycity_stateversion_check;
ALTER TABLE a_LegacyCity ALTER COLUMN stateVersion SET DEFAULT 4;
ALTER TABLE a_LegacyCity ADD CONSTRAINT a_legacycity_stateversion_check CHECK (stateVersion BETWEEN 1 AND 4);
CREATE UNIQUE INDEX IF NOT EXISTS legacy_city_id ON a_LegacyCity(id);
CREATE INDEX IF NOT EXISTS legacy_city_parent ON a_LegacyCity(idp);
CREATE INDEX IF NOT EXISTS legacy_city_empire ON a_LegacyCity(controllingEmpire);
GRANT ALL ON a_LegacyCity TO audog;

-- Original source IDs and hashes remain separate from modern n_BdDesign IDs.
CREATE TABLE IF NOT EXISTS n_LegacyBdDesign
(
  id bigint PRIMARY KEY CHECK (id BETWEEN 1 AND 4294967295),
  contentHash text NOT NULL UNIQUE CHECK (contentHash ~ '^[0-9a-f]{64}$'),
  sourceData bytea NOT NULL CHECK (octet_length(sourceData) BETWEEN 1 AND 8388608),
  sourceId bigint NOT NULL CHECK (sourceId BETWEEN 1 AND 4294967295),
  name text NOT NULL,
  industry smallint NOT NULL CHECK (industry BETWEEN 1 AND 129),
  publicDefault boolean NOT NULL DEFAULT false
);
GRANT SELECT, INSERT ON n_LegacyBdDesign TO audog;

COMMIT;
