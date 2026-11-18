BEGIN;

-- One immutable gameplay generation per universe. The master initializes the
-- row after validating records and catalogues; schema install is not
-- a gameplay selection and does not convert an existing universe.
CREATE TABLE IF NOT EXISTS g_GameGeneration
(
  singleton smallint PRIMARY KEY DEFAULT 1 CHECK (singleton = 1),
  generation text NOT NULL CHECK (generation IN ('modern', 'legacy')),
  catalogueVersion text NOT NULL DEFAULT '',
  manifestHash bytea NOT NULL DEFAULT ''::bytea,
  CHECK ((generation = 'modern' AND catalogueVersion = '' AND octet_length(manifestHash) = 0)
      OR (generation = 'legacy' AND catalogueVersion <> '' AND octet_length(manifestHash) = 32))
);

CREATE OR REPLACE FUNCTION forbid_game_generation_change() RETURNS trigger
LANGUAGE plpgsql AS $$
BEGIN
  -- SQL catalogue edits refresh the Legacy fingerprint after startup preflight.
  -- The universe's gameplay generation and singleton identity remain immutable.
  IF TG_OP = 'UPDATE' THEN
    IF OLD.generation = 'legacy'
       AND NEW.generation = OLD.generation
       AND NEW.singleton = OLD.singleton THEN
      RETURN NEW;
    END IF;
  END IF;
  RAISE EXCEPTION 'A universe game generation is immutable; use a separate universe database.';
END;
$$;

DROP TRIGGER IF EXISTS game_generation_immutable ON g_GameGeneration;
CREATE TRIGGER game_generation_immutable BEFORE UPDATE OR DELETE ON g_GameGeneration
FOR EACH ROW EXECUTE FUNCTION forbid_game_generation_change();

GRANT SELECT, INSERT ON g_GameGeneration TO audog;
GRANT UPDATE (catalogueVersion, manifestHash) ON g_GameGeneration TO audog;

COMMIT;
