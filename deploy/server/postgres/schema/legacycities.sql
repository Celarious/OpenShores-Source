BEGIN;

CREATE TABLE IF NOT EXISTS a_LegacyCity
(
  name text NOT NULL DEFAULT '',
  controllingEmpire bigint NOT NULL DEFAULT 0,
  allegianceEmpire bigint NOT NULL DEFAULT 0,
  population integer NOT NULL DEFAULT 0 CHECK (population >= 0),
  satisfaction smallint NOT NULL DEFAULT 1 CHECK (satisfaction BETWEEN -20 AND 20),
  stateVersion smallint NOT NULL DEFAULT 7 CHECK (stateVersion BETWEEN 1 AND 7),
  stateData bytea NOT NULL CHECK (octet_length(stateData) BETWEEN 1 AND 8388608),
  reportHistory bytea NOT NULL DEFAULT ''::bytea
) INHERITS (a_Atom);
-- Keep version-1 through version-6 envelopes readable until their next ordinary city save.
ALTER TABLE a_LegacyCity DROP CONSTRAINT IF EXISTS a_legacycity_stateversion_check;
ALTER TABLE a_LegacyCity ALTER COLUMN stateVersion SET DEFAULT 7;
ALTER TABLE a_LegacyCity ADD CONSTRAINT a_legacycity_stateversion_check CHECK (stateVersion BETWEEN 1 AND 7);
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
-- IDs 1 through 74 belong to the original public catalog. Each studio save
-- receives an immutable ID so replacing a name preserves its older designs.
CREATE SEQUENCE IF NOT EXISTS n_legacybddesign_studio_id_seq
  AS bigint MINVALUE 500 MAXVALUE 4294967295 START WITH 500 NO CYCLE;
-- ALTER takes the sequence lock before inspecting the old allocation position.
ALTER SEQUENCE n_legacybddesign_studio_id_seq START WITH 500;
DO $$
DECLARE next_id bigint;
BEGIN
  SELECT greatest(500, last_value + is_called::integer,
          (SELECT coalesce(max(id), 0) + 1 FROM n_LegacyBdDesign))
    INTO next_id FROM n_legacybddesign_studio_id_seq;
  IF next_id <= 4294967295 THEN
    EXECUTE format('ALTER SEQUENCE n_legacybddesign_studio_id_seq MINVALUE 500 RESTART WITH %s', next_id);
  END IF;
END $$;
ALTER TABLE n_LegacyBdDesign ADD COLUMN IF NOT EXISTS architectId bigint
  CHECK (architectId BETWEEN 1 AND 4294967295);
CREATE INDEX IF NOT EXISTS legacy_bd_library_architect_name
  ON n_LegacyBdDesign (architectId, name, id DESC) WHERE architectId IS NOT NULL;
GRANT USAGE, SELECT ON SEQUENCE n_legacybddesign_studio_id_seq TO audog;

CREATE TABLE IF NOT EXISTS n_LegacyBdFinal
(
  designId bigint PRIMARY KEY REFERENCES n_LegacyBdDesign(id)
);
CREATE TABLE IF NOT EXISTS n_LegacyBdPublication
(
  empireId bigint NOT NULL CHECK (empireId BETWEEN 1 AND 4294967295),
  designId bigint NOT NULL REFERENCES n_LegacyBdFinal(designId),
  PRIMARY KEY (empireId, designId)
);
GRANT SELECT, INSERT ON n_LegacyBdFinal, n_LegacyBdPublication TO audog;

COMMIT;
