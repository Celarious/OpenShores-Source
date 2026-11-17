-- Used for legacy commodities and recipes, separate from initstatic.sql for clarity.
-- Initial extraction source: GD2016 (2016-GD-0x3a), 212679 bytes.
-- Source SHA-256: dac1d7b576a8405d55a85f9eb2723d757d0cce0ba7dc49aeb88f7be8e34f9106
-- Field layout: src/lib/data/economy/legacy/aulegacycitycatalogue.cpp.
-- Independently checked against the AuData13.dll manual decompilation and
-- Ghidra DbManufacturingProcess::Rx (0x180116fb0) for labor/default/TL packing.
-- Extraction verification reconstructed all six exported GD sections byte-for-byte,
-- including NULL strings, both weapon modes, and all 17 armor effect entries.
-- Preserve historical IDs, names, asset paths, enum values and technology levels.
-- Runtime aliases and model overrides belong to the later game integration.
-- All 350 commodities (including ID 0), 526 manufacturing processes, and
-- 2420 manufacturing components are present. The 75 named industries plus
-- one ID-zero sentinel preserve all unique industry definitions: GD's 55
-- byte-identical empty industry slots are represented by that single sentinel.
-- Named industry IDs equal their original slot indices in the 130-slot table.
-- Also includes all 82 construction processes and 476 construction components.
-- Spacecraft-detail/room definitions are outside this six-table export.
--
-- Absorption is stored as percentages, like modern SQL, with half-percent
-- precision: GD byte / 2 = SQL percentage; divide SQL by 100 for runtime fractions.
-- Historical Energy and Kinetic armor entries are retained even though the
-- modern DbCommodity class no longer has members for them.
-- Up vectors retain GD float64 precision; insertHeight and wt retain float32.
--
-- GD does not serialize the manufacturing nat flag. Values below retain the
-- current recovery's constructor default true and approved false exceptions
-- for process IDs 12,14,23,58,92,127,170,171; these are not extracted server flags.
-- Labor/default are unpacked from GD's labor word (low 15 bits / high bit).
-- techLevel is the zero-based minimum TL, unchanged from GD.
-- sourceOrder preserves original recipe order, including default tie-breaking.
--
-- Like initstatic.sql, reapplying this file replaces its static definitions.
-- It only replaces the six *_Legacy tables. Game loading is wired separately.
BEGIN;
SET LOCAL standard_conforming_strings = on;

DROP TABLE IF EXISTS b_ConstructionComponent_Legacy;
DROP TABLE IF EXISTS b_ConstructionProcess_Legacy;
DROP TABLE IF EXISTS b_ManufacturingComponent_Legacy;
DROP TABLE IF EXISTS b_ManufacturingProcess_Legacy;
DROP TABLE IF EXISTS b_Industry_Legacy;
DROP TABLE IF EXISTS b_Commodity_Legacy;

CREATE TABLE b_Commodity_Legacy
(
  id               integer primary key unique,
  ammo1Qty         smallint DEFAULT 0,   -- Round count held for weapons that use ammo or have a charge.
  ammo1Type        smallint DEFAULT 0,   -- AMMOTYPE of ammo needed for weapon or provided by ammo.
  ammo2Qty         smallint DEFAULT 0,   -- Round count held for weapons that use ammo or have a charge.
  ammo2Type        smallint DEFAULT 0,   -- AMMOTYPE of ammo needed for weapon or provided by ammo.
                                         -- Not used for ammo because one unit reloads a weapon completely.
  armorEffectAbsorbAcid                 numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbBludgeon             numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbCold                 numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbCrush                numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbDisease              numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbElectric             numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbEnergy               numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbHeat                 numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbImplant              numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbKinetic              numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbPierce               numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbPoison               numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbRadiation            numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbSlash                numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbSonic                numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbStun                 numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectAbsorbSuffocate            numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorEffectBlockAcid                  smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockBludgeon              smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockCold                  smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockCrush                 smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockDisease               smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockElectric              smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockEnergy                smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockHeat                  smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockImplant               smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockKinetic               smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockPierce                smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockPoison                smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockRadiation             smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockSlash                 smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockSonic                 smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockStun                  smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorEffectBlockSuffocate             smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorModeAbsorbArea                   numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorModeAbsorbContact                numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorModeAbsorbProjectile             numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorModeAbsorbPsionic                numeric(4,1) DEFAULT 0,     -- armor effect as a percent in exact half-percent steps (0-127.5) of total damage absorbed
  armorModeBlockArea                    smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorModeBlockContact                 smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorModeBlockProjectile              smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorModeBlockPsionic                 smallint DEFAULT 0,     -- armor effect as a number of damage points blocked
  armorSpec                             smallint DEFAULT 0,     -- WEAPONEFFECT of specialty. Hardened vs this effect. Armor takes less (1/2) wear.
  bitsWear         bigint DEFAULT 0,              -- Where this commodity can be worn/used.
  capht            smallint DEFAULT 0,   -- >0 for Containers. Height of
                                         -- storage space, 20max
  capwid           smallint DEFAULT 0,   -- >0 for Containers. Width of
                                         -- storage space, 12max
  combits          bigint DEFAULT 0,    -- COMMODITYBITS from dbef.h. OR'd together
  description      text,                 -- Deprecated description, retained including NULL values.
  food             smallint DEFAULT 0,   -- food value if one unit is consumed
                                         -- liquor value
  health           smallint DEFAULT 0,   -- health affect if one unit is consumed, or [1..health] when damage effect is in combits
  iconPath         text,                 -- Path to small square image, 32x32 pixels, png
  modelPath        text,                 -- Path to the model used for this item
  insertHeight     float4 DEFAULT 0,     -- Height above insertion point when on the ground and up vector applied
                                         -- Used to determine the height it lays on the ground
  upVectorX        double precision DEFAULT 0,     -- X component of up vector
  upVectorY        double precision DEFAULT 0,     -- Y component of up vector
  upVectorZ        double precision DEFAULT 1,     -- Z component of up vector
  light            smallint DEFAULT 0,   -- light radius in five feet increments. i.e. value 4 = 20' light radius
  name             text,                 -- Short name suitable for lists.
  newcond          smallint default 1,   -- Condition points when new.
  sizeht           smallint default 0,   -- Height required in a container.
  sizewid          smallint default 0,   -- Width required in a container.
  stackLimit       smallint DEFAULT 1,   -- Maximum count in a stack.
  wpn1Mode         smallint DEFAULT 0,   -- WEAPONMODE enum value from dbdef.h
  wpn1Range        integer DEFAULT 0,   -- Weapon maximum range in feet.
  wpn1Effect1      smallint DEFAULT 0,   -- WEAPONEFFECT enum value. WE_None = Not a weapon for ammoType and ammoQty.
  wpn1Damage1      integer DEFAULT 0,   -- Weapon damage dice roll [1..weaponDamage]
  wpn1DamageMod1   smallint DEFAULT 0,   -- Added to weaponDamage dice roll. LIMITED TO AN UNSIGNED BYTE.
  wpn1Radius1      integer DEFAULT 0,   -- Weapon effect radius in feet.
  wpn1AP1          smallint DEFAULT 0,   -- Armor Pierce percent (0-100)
  wpn1ST1          integer DEFAULT 0,   -- Strike Through amount (0-n)
  wpn1Effect2      smallint DEFAULT 0,   -- WEAPONEFFECT enum value.
  wpn1Damage2      integer DEFAULT 0,   -- Weapon special damage [1..weaponDamage]
  wpn1DamageMod2   smallint DEFAULT 0,   -- Added to weapon 1..weaponDamage dice roll. LIMITED TO AN UNSIGNED BYTE.
  wpn1Radius2      integer DEFAULT 0,   -- Weapon effect radius in feet.
  wpn1AP2          smallint DEFAULT 0,   -- Armor Pierce percent (0-100)
  wpn1ST2          integer DEFAULT 0,   -- Strike Through amount (0-n)
  wpn2Mode         smallint DEFAULT 0,   -- WEAPONMODE enum value from dbdef.h
  wpn2Range        integer DEFAULT 0,   -- Weapon maximum range in feet.
  wpn2Effect1      smallint DEFAULT 0,   -- WEAPONEFFECT enum value. WE_None = Not a weapon for ammoType and ammoQty.
  wpn2Damage1      integer DEFAULT 0,   -- Weapon damage dice roll [1..weaponDamage]
  wpn2DamageMod1   smallint DEFAULT 0,   -- Added to weaponDamage dice roll. LIMITED TO AN UNSIGNED BYTE.
  wpn2Radius1      integer DEFAULT 0,   -- Weapon effect radius in feet.
  wpn2AP1          smallint DEFAULT 0,   -- Armor Pierce percent (0-100)
  wpn2ST1          integer DEFAULT 0,   -- Strike Through amount (0-n)
  wpn2Effect2      smallint DEFAULT 0,   -- WEAPONEFFECT enum value.
  wpn2Damage2      integer DEFAULT 0,   -- Weapon special damage [1..weaponDamage]
  wpn2DamageMod2   smallint DEFAULT 0,   -- Added to weapon 1..weaponDamage dice roll. LIMITED TO AN UNSIGNED BYTE.
  wpn2Radius2      integer DEFAULT 0,   -- Weapon effect radius in feet.
  wpn2AP2          smallint DEFAULT 0,   -- Armor Pierce percent (0-100)
  wpn2ST2          integer DEFAULT 0,   -- Strike Through amount (0-n)
  wt               float4 DEFAULT 0      -- In pounds per single unit when carried in a person's inventory. Not relevant otherwise.
                                         -- 1 kg = 2.20462 lbs
);
GRANT ALL ON b_Commodity_Legacy TO audog;
-- Fully populated commodity definitions; append new Legacy items here.
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,capwid,capht,newcond,sizewid,sizeht,stackLimit) VALUES (0,NULL,NULL,NULL,255,255,0,255,255,0);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (1,'Flag',':data/c_Flag.png',':data/mFlag.3dsX',8193,1,1,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (2,'Computer',':data/c_Computer.png',':data/mComputer.3dsN',8193,40,3,2,5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (3,'Electricity',':data/c_Electricity.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (4,'Hammer',':data/c_Hammer.png',':data/mHammer.3dsN',33562625,0.0500000007,1.0,0.0,5,1,2,2,1,8,11,4,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (5,'Air',':data/c_Air.png',':data/mAir.3dsN',8193,1.79999995,2,3,5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile) VALUES (6,'Helicopter',':data/c_Helicopter.png',':data/mHelicopter.3dsN',3.0,3.0,3.0,4,4,4);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic) VALUES (7,'Space Rocket',':data/c_SpaceRocket.png',':data/mRocketShip.3dsX',6.0,6.0,6.0,1.0,6,6,6,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic) VALUES (8,'Space Fighter F1',':data/c_SpaceFighter.png',':data/mSpacefighter.3dsN',15.0,15.0,15.0,2.0,15,15,15,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic) VALUES (9,'Space Transport',':data/c_CommercialAircraft.png',':data/mFlyingAT.3dsN',15.0,15.0,15.0,2.0,15,15,15,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (10,'Pistol Ammunition',':data/c_PistolAmmo.png',':data/mPistolAmmo.3dsX',4,8193,0.0500000007,1,1,13,31,9,0.200000003);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorY,upVectorZ,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1AP1,wt) VALUES (11,'Heavy Arms Ammunition Pi',':data/c_HeavyArmsAmmo.png',':data/mHeavyArmsAmmo.3ds',2,8193,0.0500000007,1.0,0.0,1,5,13,71,29,10,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (12,'Bomb Bl Black Cat',':data/c_Bomb.png',':data/mBomb.3dsX',8193,2,8,3,8,751,249,100,250);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (13,'Plastic Explosive',':data/c_PlasticExplosive.png',':data/mPlasticExplosive.3dsN',8193,2,2,3,8,151,49,20,2.20000005);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (14,'Animal Carcass',':data/c_Animal.png',':data/mAnimal.3dsX',4099,20,5,4,6,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (15,'Beans',':data/c_Beans.png',':data/mBeans.3dsN',20483,20,0.300000012,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (16,'Beer',':data/c_Beer.png',':data/mBeer.3dsX',20485,1,4,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (17,'Leather Body Armor',':data/c_LeatherBodyArmor.png',':data/mLeatherBodyArmor.3dsN',1.0,1.0,1,2.0,2.0,2.0,2,2,2,8,237,8193,0.100000001,10,3,4,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (18,'Bread',':data/c_Bread.png',':data/mBread.3dsX',20483,15,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (19,'Candy',':data/c_Candy.png',':data/mCandy.3dsX',20483,20,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (20,'Cheese',':data/c_Cheese.png',':data/mCheese.3dsX',20483,20,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbCold,armorModeAbsorbArea,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (21,'Fabric Clothing',':data/c_LinenClothes.png',':data/mLinenShirt.3dsN',1.0,1.0,1,1,1,14592,8193,3,2,2,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (22,'Coal',':data/c_Coal.png',':data/mCoal.3dsX',1,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (23,'Oil',':data/c_CrudeOil.png',':data/mCrudeOil.3dsN',8193,3,3,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (24,'Crystals',':data/c_Crystals.png',':data/mCrystals.3dsX',1,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (25,'Dynamite',':data/c_Dynamite.png',':data/mDynamite.3dsX',8193,2,1,4,8,151,49,20,1.64999998);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (26,'Eggs',':data/c_Eggs.png',':data/mEggs.3dsX',20483,15,2,1,1,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (27,'Electronic Part',':data/c_ElectronicPart.png',':data/mElectronicPart.3dsX',8193,2,1,0.25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile) VALUES (28,'Farm Machinery',':data/c_FarmMachinery.png',':data/mFarmEquipment.3dsN',12.0,12.0,12.0,12,12,12);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (29,'Fertilizer',':data/c_Fertilizer.png',':data/mFertilizer.3dsX',1,3,4,8);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (30,'Plant Fiber',':data/c_Fiber.png',':data/mFiber.3dsX',1,2,2,5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Qty,ammo1Type,combits,health,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wt) VALUES (31,'Pistol',':data/c_Pistol.png',':data/mPistol.3dsN',15,4,8449,6,0.100000001,1.0,0.0,80,1,1,5,150,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Qty,ammo1Type,combits,health,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wt) VALUES (32,'Rifle',':data/c_Rifle.png',':data/mRifle.3dsN',30,5,8449,10,0.0500000007,1.0,0.0,100,2,6,5,1000,4);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (33,'Fish',':data/c_Fish.png',':data/mFish.3dsN',4099,15,0.150000006,1.0,0.0,2,2,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (34,'Flour',':data/c_Flour.png',':data/mFlour.3dsX',4099,15,2,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (35,'Fruit',':data/c_Fruit.png',':data/mFruit.3dsX',20483,10,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (36,'Gasoline',':data/c_Gasoline.png',':data/mGasoline.3dsN',8193,2,2,3,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (37,'Gems',':data/c_Gems.png',':data/mGems.3dsN',1,0.400000006,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (38,'Grain',':data/c_Grain.png',':data/mGrain.3dsN',20483,15,2,2,3,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (39,'Grapes',':data/c_Grapes.png',':data/mGrapes.3dsN',20483,10,0.100000001,1.0,0.0,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (40,'Gun Powder',':data/c_GunPowder.png',':data/mGunPowder.3dsN',1,1,2,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCrush,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (41,'Woven Helmet',':data/c_WovenHelmet.png',':data/mWovenHelmet.3dsN',1.0,1.0,2.0,2.0,2.0,2,2,2,13,16,8193,8,2,2,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (42,'Herbs',':data/c_Herbs.png',':data/mHerbs.3dsN',20483,10,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (43,'Hops',':data/c_Hops.png',':data/mHops.3dsN',20483,10,1.45000005,2,2,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (44,'Hydrogen',':data/c_Hydrogen.png',':data/mHydrogen.3dsN',8193,2.5,2,3,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (45,'Ice',':data/c_Ice.png',':data/mIce.3dsN',1,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorY,upVectorZ,sizewid,sizeht,wt) VALUES (46,'Crystal Necklace',':data/c_CrystalJewelry.png',':data/mCrystalJewelry.3dsN',524288,8193,0.100000001,1.0,0.0,1,1,0.300000012);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (47,'Leather',':data/c_Leather.png',':data/mLeather.3dsN',20483,15,0.100000001,1.0,0.0,2,3,5,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,combits,health,insertHeight,upVectorY,upVectorZ,sizewid,sizeht,wt) VALUES (48,'Lumber',':data/c_Lumber.png',':data/mLumber.3dsN',10.0,10.0,10.0,10,10,10,8449,15,0.100000001,1.0,0.0,1,8,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (49,'Animal Meat',':data/c_AnimalMeat.png',':data/mAnimalMeat.3dsX',20483,20,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,sizewid,sizeht,wt) VALUES (50,'Mechanical Part',':data/c_MechanicalPart.png',':data/mMechanicalPart.3dsN',8193,0.150000006,-1.0,0.0,2,2,5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,combits,health,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (51,'Metal',':data/c_Metal.png',':data/mMetal.3dsN',25.0,25.0,25.0,25,25,25,8449,15,0.100000001,1.0,0.0,2,1,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (52,'Milk',':data/c_Milk.png',':data/mMilk.3dsN',20483,20,0.550000012,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (53,'Minerals',':data/c_Minerals.png',':data/mMinerals.3dsN',1,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (54,'Mine Machinery',':data/c_MineMachinery.png',':data/mMineMachinery.3dsX');
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (55,'Small Missile Bl Zipper',':data/c_MissileSm.png',':data/mMissileSm.3dsN',7,8193,0.100000001,1.0,0.0,1,8,2500,8,126,24,15,50);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (56,'Large Missile Bl Thumper',':data/c_MissileLg.png',':data/mMissileLg.3dsN',3,1,3.79999995,3,12,25000,8,701,199,90,300);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (57,'Natural Gas',':data/c_NaturalGas.png',':data/mNaturalGas.3dsN',8193,2,2,3,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (58,'Nuts',':data/c_Nuts.png',':data/mNuts.3dsN',20483,15,0.349999994,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (59,'Ore',':data/c_Ore.png',':data/mOre.3dsN',1,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (60,'Pastry',':data/c_Pastry.png',':data/mPastry.3dsN',20483,10,2,1,1,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (61,'Petrochemicals',':data/c_Petrochemicals.png',':data/mPetrochemicals.3dsN',1,3,3,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (62,'Pharmaceuticals',':data/c_Pharmaceuticals.png',':data/mPharmaceuticals.3dsN',12289,0.300000012,1,1,0.200000003);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (63,'Plastic',':data/c_Plastic.png',':data/mPlastic.3dsX',8193,1,6,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (64,'Large Rocket Motor',':data/c_RocketMotorLg.png',':data/mRocketMotorLg.3dsN',1,3.20000005,3,9,50);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbCrush,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockCrush,armorEffectBlockPierce,armorEffectBlockRadiation,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,armorSpec,bitsWear,combits,light,newcond,sizewid,sizeht,wt) VALUES (65,'Metal Power Armor',':data/c_PowerArmor.png',':data/mPowerArmor.3dsN',1.0,1.0,1.0,1.0,1,1,1,10.0,10.0,10.0,2.0,10,10,10,2,7,237,8193,15,80,4,10,50);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (66,'Radioactives',':data/c_Radioactives.png',':data/mRadioactives.3dsN',1,0.5,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (67,'Small Rocket Bl Partypopper',':data/c_RocketSm.png',':data/mRocketSm.3dsN',7,8193,0.200000003,1.0,0.0,1,8,2500,8,151,49,20,40);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (68,'Large Rocket Bl Thud',':data/c_RocketLg.png',':data/mRocketLg.3dsN',3,1,3.79999995,3,12,25000,8,751,249,100,280);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (69,'Small Rocket Motor',':data/c_RocketMotorSm.png',':data/mRocketMotorSm.3dsN',1,0.150000006,1.0,0.0,1,6,25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (70,'Commercial Ship',':data/c_CommercialShip.png',':data/mCommercialShip.3dsX');
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (71,'Naval Ship',':data/c_NavalShip.png',':data/mNavalShip.3dsX');
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbRadiation,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (72,'Environment Suit Helmet',':data/c_VacSuitHelmet.png',':data/mVacSuitHelmet.3dsN',1.0,1.0,1.0,1.0,1,1,1,2.0,2.0,2.0,1.0,1,1,1,16,8193,10,2,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,health,insertHeight,newcond,sizewid,sizeht,wt) VALUES (73,'Medical Kit',':data/c_MedKit.png',':data/mMedKit.3dsN',12417,20,0.899999976,2,2,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (74,'Spices',':data/c_Spices.png',':data/mSpices.3dsN',20483,10,0.400000006,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (75,'Rifle Ammunition',':data/c_RifleAmmo.png',':data/mRifleAmmo.3dsX',5,8193,1,1,13,46,14,0.800000012);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,combits,health,sizewid,sizeht,wt) VALUES (76,'Stone',':data/c_Stone.png',':data/mStone.3dsN',20.0,20.0,20.0,20,20,20,8449,15,2,1,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic) VALUES (77,'Submarine',':data/c_Submarine.png',':data/mNeptune.3dsX',10.0,10.0,10.0,1.0,10,10,10,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (78,'Sugar',':data/c_Sugar.png',':data/mSugar.3dsX',4099,20,2,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (79,'Textiles',':data/c_Textiles.png',':data/mTextiles.3dsN',8193,0.100000001,1,5,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbCold,armorEffectAbsorbHeat,armorEffectBlockCold,armorEffectBlockHeat,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,bitsWear,combits,light,newcond,sizewid,sizeht,wt) VALUES (80,'Environment Suit',':data/c_VacSuit.png',':data/mVacSuit.3dsN',2.0,2.0,2,2,3.0,3.0,3.0,1.0,3,3,3,1,237,8193,15,25,3,8,25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (81,'Vegetable',':data/c_Vegetable.png',':data/mVegetable.3dsN',20483,10,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile) VALUES (82,'SUV',':data/c_Suv.png',':data/mSUV.3dsN',7.0,7.0,7.0,8,8,8);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (83,'Water',':data/c_Water.png',':data/mWater.3dsN',20547,1.5,4,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (84,'Whiskey',':data/c_Whiskey.png',':data/mWhiskey.3dsN',16389,2,0.5,4,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (85,'Wine',':data/c_Wine.png',':data/mWine.3dsN',20485,1,0.800000012,4,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,combits,health,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (86,'Log',':data/c_Wood.png',':data/mWood.3dsN',15.0,15.0,15.0,15,15,15,8449,15,0.200000003,-1.0,0.0,2,8,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (87,'Atmosphere Density',':data/c_Atmosphere.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (88,'Water in the Environment',':data/c_Hydrographics.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (89,'Vegetation Density',':data/c_Vegetation.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCrush,armorEffectAbsorbPierce,armorEffectAbsorbSlash,armorEffectBlockCrush,armorEffectBlockPierce,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (90,'Metal Body Armor',':data/c_MetalBodyArmor.png',':data/mMetalBodyArmor.3dsN',1.0,1.0,1.0,1.0,1,1,8.0,8.0,8.0,10,10,10,15,237,8193,40,3,4,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbAcid,armorEffectAbsorbBludgeon,armorEffectAbsorbElectric,armorEffectAbsorbPierce,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCrush,armorEffectBlockElectric,armorEffectBlockPierce,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (91,'Plastic Body Armor',':data/c_PlasticBodyArmor.png',':data/mPlasticBodyArmor.3dsN',1.0,1.0,1.0,1.0,1.0,1,1,2,1,6.0,6.0,6.0,7,7,7,1,237,8193,0.300000012,1.0,0.0,30,3,4,4);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbHeat,armorEffectAbsorbPierce,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (92,'Woven Body Armor',':data/c_WovenBodyArmor.png',':data/mWovenBodyArmor.3dsN',1.0,1.0,1.0,1.0,1.0,1,1,4.0,4.0,4.0,5,5,5,13,237,8193,0.150000006,20,3,4,6);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wt) VALUES (93,'Nitrate Explosive',':data/c_NitrateExplosive.png',':data/mNitrateExplosive.3dsN',8193,2,3,3,8,151,49,20,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (94,'Sunlight',':data/c_Sunlight.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbAcid,armorEffectAbsorbCold,armorEffectAbsorbHeat,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (95,'Leather Clothing',':data/c_LeatherClothes.png',':data/mLeatherShirt.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,8,14592,8193,0.150000006,1.0,0.0,5,2,3,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbAcid,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbHeat,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (96,'Leather Helmet',':data/c_LeatherHelmet.png',':data/mLeatherHelmet.3dsN',1.0,1.0,1.0,1.0,1.0,1,1.0,1.0,1.0,1,1,1,8,16,8193,6,2,2,0.600000024);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbAcid,armorEffectAbsorbBludgeon,armorEffectAbsorbCrush,armorEffectAbsorbElectric,armorEffectAbsorbPierce,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCrush,armorEffectBlockPierce,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (97,'Plastic Helmet',':data/c_PlasticHelmet.png',':data/mPlasticHelmet.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,3.0,3.0,3.0,3,3,3,1,16,8193,10,2,2,0.699999988);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (98,'Metal Bracelet',':data/c_MetalJewelry.png',':data/mMetalJewelry.3dsN',1879048192,8193,0.0500000007,1.0,0.0,2,1,1,0.300000012);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (99,'Gem Ring',':data/c_GemJewelry.png',':data/mGemJewelry.3dsN',1089470464,8193,0.100000001,1.0,0.0,2,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (100,'Fish Meat',':data/c_FishMeat.png',':data/mFishMeat.3dsN',20483,15,0.150000006,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (101,'Vodka',':data/c_Vodka.png',':data/mVodka.3dsN',16389,2,0.75,4,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (102,'Heavy Weapon',':data/c_HeavyWeapon.png',':data/mTurretGun.3dsN',8193,1.39999998,3,20,100);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (103,'Tank',':data/c_Tank.png',':data/mTank.3dsX');
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic) VALUES (104,'APC',':data/c_APC.png',':data/mAT.3dsN',30.0,30.0,30.0,5.0,30,30,30,5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCrush,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockCrush,armorEffectBlockElectric,armorEffectBlockPierce,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (105,'Metal Helmet',':data/c_MetalHelmet.png',':data/mMetalHelmet.3dsN',1.0,1.0,1.0,1.0,1.0,1,1,1,1,4.0,4.0,4.0,5,5,5,15,16,8193,12,2,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (106,'Metal Power Armor Helmet',':data/c_PowerArmorHelmet.png',':data/mPowerArmorHelmet.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,5.0,5.0,5.0,1.0,5,5,5,1,7,16,8193,25,2,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,capwid,capht,combits,newcond,sizewid,sizeht,wt) VALUES (107,'Crate',':data/c_Crate.png',':data/mCrate.3dsN',12,20,8193,50,12,20,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,light,newcond,sizewid,sizeht,wt) VALUES (108,'Lantern',':data/c_Lantern.png',':data/mLantern.3dsN',8257,0.850000024,10,10,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,capwid,capht,combits,upVectorY,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (109,'Knapsack',':data/c_Knapsack.png',':data/mKnapsack.3dsN',4,5,8193,-1.0,0.0,5,4,5,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,capwid,capht,combits,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (110,'Backpack',':data/c_Backpack.png',':data/mBackpack.3dsN',6,8,8193,1.0,0.0,10,6,8,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (111,'Lumenite',':data/c_Lumenite.png',':data/mLumenite.3dsN',8193,0.5,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (112,'Blank Disk',':data/c_BlankDisk.png',':data/mBlankDisk.3dsN',8193,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (113,'Ship Design Disk',':data/c_DesignDisk.png',':data/mShipDesignDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (114,'Shovel',':data/c_Shovel.png',':data/mShovel.3dsN',536879105,0.100000001,1.0,0.0,5,1,8,2,3,15,11,4,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (115,'Gin',':data/c_Gin.png',':data/mGin.3dsN',16389,2,0.649999976,4,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (116,'Knife',':data/c_Knife.png',':data/mKnife.3dsN',268447745,0.0199999996,-1.0,0.0,100,1,2,2,1,13,16,4,0.800000012);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Qty,ammo1Type,combits,health,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wt) VALUES (117,'Shotgun',':data/c_Shotgun.png',':data/mShotgun.3dsN',8,6,8449,10,0.100000001,-1.0,0.0,90,2,8,5,150,3.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (118,'Shotgun Ammunition',':data/c_ShotgunAmmo.png',':data/mShotgunAmmo.3dsX',6,8193,1,1,8,21,9,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (119,'Antidote',':data/c_Antidote.png',':data/mAntidote.3dsX',12321,1,1,0.200000003);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (120,'Antitoxin',':data/c_Antitoxin.png',':data/mAntitoxin.3dsX',12305,1,1,0.200000003);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (121,'Rum',':data/c_Rum.png',':data/mRum.3dsN',16389,2,0.800000012,4,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (122,'Tequila',':data/c_Tequila.png',':data/mTequila.3dsN',16389,2,0.699999988,4,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wt) VALUES (123,'Fitting Wrench',':data/c_FittingWrench.png',':data/mFittingWrench.3dsN',1056769,0.0500000007,-1.0,0.0,5,1,1,2,1,8,6,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wt) VALUES (124,'Glue Gun',':data/c_GlueGun.png',':data/mGlueGun.3dsN',67117057,0.0500000007,-1.0,0.0,5,1,1,2,1,3,8,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wt) VALUES (125,'Screw Driver',':data/c_ScrewDriver.png',':data/mScrewDriver.3dsN',2105345,0.0500000007,1.0,0.0,5,1,1,2,1,13,8,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wt) VALUES (126,'Sewing Needle',':data/c_SewingNeedle.png',':data/mSewingNeedle.3dsN',134225921,0.100000001,4,1,1,2,1,13,4,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wt) VALUES (127,'Soldering Iron',':data/c_SolderIron.png',':data/mSolderingIron.3dsN',4202497,0.0500000007,1.0,0.0,5,1,1,2,1,12,8,0.25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (128,'Tongs',':data/c_Tongs.png',':data/mTongs.3dsN',8396801,0.100000001,-1.0,0.0,5,2,2,2,1,8,9,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (129,'Wrench',':data/c_Wrench.png',':data/mWrench.3dsN',16785409,0.0500000007,1.0,0.0,5,1,2,2,1,8,9,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (130,'Hay',':data/c_Hay.png',':data/mHay.3dsN',20483,10,2,5,6,3,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,light,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (131,'Torch',':data/c_Torch.png',':data/mTorch.3dsN',8257,0.100000001,1.0,0.0,7,5,1,3,2,2,12,7,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile) VALUES (132,'Sailboat',':data/c_SailBoat.png',':data/mWoodenBoat.3dsX',2.0,2.0,2.0,3,3,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile) VALUES (133,'Motor Bike',':data/c_MotorBike.png',':data/mMotorBike.3dsN',5.0,5.0,5.0,5,5,5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (134,'Eludium',':data/c_Eludium.png',':data/mEludium.3dsN',1,0.0500000007,1.0,0.0,1,2,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (135,'Pliers',':data/c_Pliers.png',':data/mPliers.3dsN',1073750017,0.0199999996,1.0,0.0,6,1,2,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (136,'Torque Wrench',':data/c_TorqueWrench.png',':data/mTorqueWrench.3dsN',532481,0.0399999991,1.0,0.0,3,1,3,2,1,8,9,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (137,'Vulcanite',':data/c_Vulcanite.png',':data/mVulcanite.3dsN',1,0.100000001,2,2,9);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (138,'Ioplasma',':data/c_Ioplasma.png',':data/mIoplasma.3dsN',1,2,3,8);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (139,'Magmex',':data/c_Magmex.png',':data/mMagmex.3dsN',1,2,2,8);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (140,'Phlogiston',':data/c_Phlogiston.png',':data/mPhlogiston.3dsN',1,2,3,4);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (141,'Bolite',':data/c_Bolite.png',':data/mBolite.3dsN',1,2,2,20);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (142,'Cryozine',':data/c_Cryozine.png',':data/mCryozine.3dsN',1,3,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (143,'Myrathane',':data/c_Myrathane.png',':data/mMyrathane.3dsN',1,0.899999976,2,3,9);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (144,'Polytaride',':data/c_Polytaride.png',':data/mPolytaride.3dsN',1,1.79999995,2,3,9);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (145,'Vulcium',':data/c_Vulcium.png',':data/mVulcium.3dsN',35.0,35.0,35.0,35,35,35,8193,0.100000001,1.0,0.0,2,1,8);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (146,'Magmium',':data/c_Magmium.png',':data/mMagmium.3dsN',30.0,30.0,30.0,30,30,30,8193,0.100000001,1.0,0.0,2,1,6);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (147,'Lava in the Environment',':data/c_Magma.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (148,'Supercooled Liquid in the Environment',':data/c_SupercooledLiquid.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (149,'Inferno Atmosphere Density',':data/c_InfernoAtmosphere.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath) VALUES (150,'Frigid Atmosphere Density',':data/c_FrigidAtmosphere.png',NULL);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (151,'Magmium Knife',':data/c_MagmiumKnife.png',':data/mKnifeM.3dsN',268447745,0.0199999996,-1.0,0.0,150,1,2,2,1,13,21,4,0.649999976);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (152,'Vulcium Knife',':data/c_VulciumKnife.png',':data/mKnifeV.3dsN',268447745,0.0199999996,-1.0,0.0,200,1,2,2,1,13,26,4,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorEffectBlockRadiation,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,armorSpec,bitsWear,combits,light,newcond,sizewid,sizeht,wt) VALUES (153,'Magmium Power Armor',':data/c_MagmiumPowerArmor.png',':data/mPowerArmorM.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,1,1,15.0,15.0,15.0,3.0,15,15,15,4,12,237,8193,15,90,4,10,30);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorEffectBlockRadiation,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (154,'Magmium Power Armor Helmet',':data/c_MagmiumPowerArmorHelmet.png',':data/mPowerArmorHelmetM.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,7.0,7.0,7.0,2.0,7,7,7,2,12,16,8193,27,2,2,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorEffectBlockRadiation,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,armorSpec,bitsWear,combits,light,newcond,sizewid,sizeht,wt) VALUES (155,'Vulcium Power Armor',':data/c_VulciumPowerArmor.png',':data/mPowerArmorV.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,1,1,30.0,30.0,30.0,4.0,30,30,30,5,5,237,8193,15,100,4,10,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorEffectBlockRadiation,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,armorSpec,bitsWear,combits,newcond,sizewid,sizeht,wt) VALUES (156,'Vulcium Power Armor Helmet',':data/c_VulciumPowerArmorHelmet.png',':data/mPowerArmorHelmetV.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,1,1,10.0,10.0,10.0,2.0,10,10,10,3,5,16,8193,30,2,2,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,sizewid,sizeht,wt) VALUES (157,'Cronodollars',':data/c_Money.png',':data/mMoney.3dsX',1,1,0.0625);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (158,'Aircraft Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (159,'Military Guard Tower Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (160,'Airport Repair Shop Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (161,'Airport Terminal Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (162,'Apartment Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (163,'Arena Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (164,'Armorer Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (165,'Bakery Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (166,'Bank Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (167,'Brewery Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (168,'Broker Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (169,'Capitol Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (170,'Carpenter Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (171,'Casino Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (172,'Church Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (173,'Clothing Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (174,'Coal Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (175,'Military Shield Generator Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (176,'Computer Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (177,'Design Studio Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (178,'Distillery Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (179,'Electronics Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (180,'Explosives Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (181,'Farm Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (182,'Fire Station Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (183,'Gasoline Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (184,'Grist Mill Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (185,'Grocery Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (186,'Military Weapon System Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (187,'Hospital Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (188,'Condominium Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (189,'House Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (190,'Hydroelectric Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (191,'Hydrogen Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (192,'Jeweler Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (193,'Logging Camp Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (194,'Lumber Mill Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (195,'Machine Shop Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (196,'Meat Processing Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (197,'Mine Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (198,'Munitions Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (199,'Natural Gas Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (200,'Nuclear Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (201,'Orchard Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (202,'Park Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (203,'Pharmaceutical Lab Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (204,'Plastics Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (205,'Police Station Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (206,'Refinery Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (207,'Retail Store Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (208,'Rocket Motor Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (209,'University Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (210,'Ship Yard Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (211,'Smelter Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (212,'Solar Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (213,'Spacecraft Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (214,'Sugar Mill Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (215,'Cantina Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (216,'Textile Mill Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (217,'Treatment Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (218,'Vehicle Factory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (219,'Warehouse Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (220,'Weapon Smith Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (221,'Well Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (222,'Wharf Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (223,'Wind Power Plant Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (224,'Winery Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (225,'Zoo Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,sizewid,sizeht,wt) VALUES (226,'Grav Coupling',':data/c_GravCoupling.png',':data/mGravCoupling.3dsN',8193,0.400000006,2,2,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorZ,sizewid,sizeht,wt) VALUES (227,'Cryo Heat Sink',':data/c_CryoHeatSink.png',':data/mCryoHeatSink.3dsN',8193,0.319999993,-1.0,1,1,0.25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (228,'Io Tube',':data/c_IoTube.png',':data/mIoTube.3dsN',8193,0.300000012,1.0,0.0,1,2,0.150000006);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (229,'Clamp',':data/c_Clamp.png',':data/mClamp.3dsN',139265,0.0399999991,1.0,0.0,4,1,1,2,1,10,9,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (230,'Oscilloscope',':data/c_OScope.png',':data/mOScope.3dsN',270337,0.0500000007,1.0,0.0,4,1,2,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic) VALUES (231,'Space Fighter F2',':data/c_SpaceFighter2.png',':data/mSpaceFighter2.3dsN',15.0,15.0,15.0,2.0,15,15,15,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (232,'Mission Disk',':data/c_MissionDisk.png',':data/mMissionDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,health,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (233,'Bone',':data/c_Bone.png',':data/mBone.3dsN',4355,15,9,0.300000012,1.0,0.0,1,3,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (234,'Command And Control Module',':data/c_CommandAndControlModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (235,'Gravity Drive Module',':data/c_DriveModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (236,'Rocket Drive Module',':data/c_DriveModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbCrush,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockCrush,armorEffectBlockPierce,armorEffectBlockRadiation,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,combits,newcond,sizewid,sizeht,wt) VALUES (237,'Metal Hull Module',':data/c_HullModule.png',':data/mCrate.3dsN',1.0,1.0,1.0,1.0,1,1,1,10.0,10.0,10.0,2.0,10,10,10,2,8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (238,'Life Support Module',':data/c_LifeSupportModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (239,'Power Plant Module',':data/c_PowerPlantModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbCold,armorEffectAbsorbElectric,armorEffectAbsorbEnergy,armorEffectAbsorbHeat,armorEffectAbsorbRadiation,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,combits,newcond,sizewid,sizeht,wt) VALUES (240,'Energy Shield Module',':data/c_ShieldModule.png',':data/mCrate.3dsN',1.0,1.0,1.0,1.0,1.0,5.0,5.0,5.0,1.0,5,5,5,1,8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCrush,armorEffectAbsorbKinetic,armorEffectAbsorbPierce,armorEffectAbsorbSlash,armorEffectAbsorbSonic,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,combits,newcond,sizewid,sizeht,wt) VALUES (241,'Kinetic Shield Module',':data/c_ShieldModule.png',':data/mCrate.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,5.0,5.0,5.0,1.0,5,5,5,1,8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,combits,newcond,sizewid,sizeht,wt) VALUES (242,'Cloaking Shield Module',':data/c_ShieldModule.png',':data/mCrate.3dsN',5.0,5.0,5.0,1.0,5,5,5,1,8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (243,'Surgery Medical Module',':data/c_MedicalModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (244,'Genetics Medical Module',':data/c_MedicalModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (245,'Cybernetics Medical Module',':data/c_MedicalModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (246,'Personnel Transporter Module',':data/c_TransporterModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (247,'Vehicle Transporter Module',':data/c_TransporterModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (248,'Energy Weapon Turret Module',':data/c_TurretModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (249,'Beam Laser Turret Module',':data/c_TurretModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (250,'Weapon Launcher Turret Module',':data/c_TurretModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (251,'Missile Launcher Turret Module',':data/c_TurretModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (252,'Energy Weapon Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (253,'Pulse Laser Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (254,'Weapon Launcher Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (255,'Missile Launcher Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (256,'Atmosphere Condenser Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (257,'Prospector Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (258,'Tractor Beam Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (259,'Wormhole Drive FTL Module',':data/c_FTLModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (260,'Warp Drive FTL Module',':data/c_FTLModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (261,'Ballistic Weapon Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (262,'Ballistic Weapon Turret Module',':data/c_TurretModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (263,'Sensor Module',':data/c_SensorModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorZ,sizewid,sizeht,wt) VALUES (264,'Magmium Heat Sink',':data/c_MagmiumHeatSink.png',':data/mCryoHeatSink.3dsN',8193,0.319999993,-1.0,1,1,0.25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorZ,sizewid,sizeht,wt) VALUES (265,'Vulcium Heat Sink',':data/c_VulciumHeatSink.png',':data/mCryoHeatSink.3dsN',8193,0.319999993,-1.0,1,1,0.25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (266,'Myrasplicer',':data/c_Myrasplicer.png',':data/mMyrasplicer.3dsN',8193,1,2,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (267,'Microscope',':data/c_Microscope.png',':data/mMicroscope.3dsN',8193,4,1,2,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (268,'Gigacell',':data/c_Gigacell.png',':data/mGigacell.3ds',1,8193,1,1,12,61,19,0.5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (269,'Tesloid Dissipator',':data/c_TesloidDissipator.png',':data/mTesloidDissipator.3dsN',8193,1,2,2.20000005);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (270,'Nanopath Calibrator',':data/c_NanopathCalibrator.png',':data/mNanopathCalibrator.3dsN',8193,1,2,0.219999999);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (271,'Antiflux Particles',':data/c_AntifluxParticles.png',':data/mAntifluxParticles.3dsN',1,2,2,0.219999999);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (272,'Borexino Precipitate',':data/c_BorexinoPrecipitate.png',':data/mBorexinoPrecipitate.3dsN',1,2,2,2.20000005);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (273,'Type O Preons',':data/c_TypeOPreons.png',':data/mPreon.3ds',1,2,2,1.10000002);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (274,'Type B Preons',':data/c_TypeBPreons.png',':data/mPreon.3ds',1,2,2,1.10000002);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (275,'Type A Preons',':data/c_TypeAPreons.png',':data/mPreon.3ds',1,2,2,1.10000002);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (276,'Type F Preons',':data/c_TypeFPreons.png',':data/mPreon.3ds',1,2,2,1.10000002);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (277,'Type G Preons',':data/c_TypeGPreons.png',':data/mPreon.3ds',1,2,2,1.10000002);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (278,'Type K Preons',':data/c_TypeKPreons.png',':data/mPreon.3ds',1,2,2,1.10000002);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (279,'Type M Preons',':data/c_TypeMPreons.png',':data/mPreon.3ds',1,2,2,1.10000002);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorEffectBlockRadiation,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,combits,newcond,sizewid,sizeht,wt) VALUES (280,'Magmium Hull Module',':data/c_HullModule.png',':data/mCrate.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,1,1,15.0,15.0,15.0,3.0,15,15,15,4,8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,armorEffectAbsorbBludgeon,armorEffectAbsorbCold,armorEffectAbsorbCrush,armorEffectAbsorbHeat,armorEffectAbsorbPierce,armorEffectAbsorbRadiation,armorEffectAbsorbSlash,armorEffectBlockBludgeon,armorEffectBlockCold,armorEffectBlockCrush,armorEffectBlockHeat,armorEffectBlockRadiation,armorEffectBlockSlash,armorModeAbsorbArea,armorModeAbsorbContact,armorModeAbsorbProjectile,armorModeAbsorbPsionic,armorModeBlockArea,armorModeBlockContact,armorModeBlockProjectile,armorModeBlockPsionic,combits,newcond,sizewid,sizeht,wt) VALUES (281,'Vulcium Hull Module',':data/c_HullModule.png',':data/mCrate.3dsN',1.0,1.0,1.0,1.0,1.0,1.0,1.0,1,1,1,1,1,1,30.0,30.0,30.0,4.0,30,30,30,5,8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (282,'Paper',':data/c_Paper.png',':data/mPaper.3dsN',8193,1.0,0.0,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (283,'Particle Collector Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (284,'DNA Sample Kit',':data/c_DNASampleKit.png',':data/mAntidote.3dsX',12289,1,1,0.200000003);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (285,'DNA Sample',':data/c_DNASample.png',':data/mAntidote.3dsX',1,1,1,0.200000003);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (286,'Observatory Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (287,'Building Design Disk',':data/c_BuildingDesignDisk.png',':data/mShipDesignDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (288,'Building Design Drawing',':data/c_BuildingDesignDrawing.png',':data/mPaper.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (289,'Siphon Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (290,'Officer Log',':data/c_OfficerLog.png',':data/mMissionDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (291,'Wildcatter Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Qty,ammo1Type,combits,health,insertHeight,upVectorY,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wt) VALUES (292,'Кала́шников',':data/c_AK47.png',':data/mAK47.3dsN',30,5,8449,10,0.0500000007,1.0,0.0,100,2,6,5,1000,4);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,capwid,capht,combits,upVectorY,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (293,'Utility Pouch',':data/c_UtilityPouch.png',':data/mKnapsack.3dsN',5,3,8193,-1.0,0.0,4,5,3,0.25);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,capwid,capht,combits,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (294,'Grav Pack',':data/c_GravPack.png',':data/mBackpack.3dsN',8,12,8193,1.0,0.0,12,8,12,2.20000005);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,newcond,sizewid,sizeht,wt) VALUES (295,'Silage',':data/c_Hay.png',':data/mHay.3dsN',20483,10,2,5,6,3,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Qty,ammo1Type,ammo2Type,combits,health,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn2Mode,wpn2Range,wt) VALUES (296,'Laser Rifle',':data/c_LaserRifle.png',':data/mLaserRifle.3ds',50,1,8,8449,10,0.0500000007,1.0,0.0,100,2,6,5,1200,5,1200,4);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (297,'Small Rocket BlBu Firecracker',':data/c_RocketSm.png',':data/mRocketSm.3dsN',7,8193,0.200000003,1.0,0.0,1,8,2500,8,151,49,20,12,131,39,18,44);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (298,'Large Rocket BlBu Molotov',':data/c_RocketLg.png',':data/mRocketLg.3dsN',3,1,3.79999995,3,12,25000,8,751,249,100,12,731,239,90,330);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (299,'Small Missile BlBu Barnburner',':data/c_MissileSm.png',':data/mMissileSm.3dsN',7,8193,0.100000001,1.0,0.0,1,8,2500,8,126,24,15,12,106,14,13,55);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (300,'Large Missile BlBu Fireball',':data/c_MissileLg.png',':data/mMissileLg.3dsN',3,1,3.79999995,3,12,25000,8,701,199,90,12,681,189,89,350);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (301,'Small Rocket SlPi Meathook',':data/c_RocketSm.png',':data/mRocketSm.3dsN',7,8193,0.200000003,1.0,0.0,1,8,2500,15,151,49,20,13,131,39,18,44);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (302,'Large Rocket BuDi Scudpox',':data/c_RocketLg.png',':data/mRocketLg.3dsN',3,1,3.79999995,3,12,25000,12,751,249,100,6,731,239,90,330);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (303,'Small Missile CrFr Snowball',':data/c_MissileSm.png',':data/mMissileSm.3dsN',7,8193,0.100000001,1.0,0.0,1,8,2500,10,126,24,15,9,106,14,13,55);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,sizewid,sizeht,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (304,'Large Missile SlPo Sideviper',':data/c_MissileLg.png',':data/mMissileLg.3dsN',3,1,3.79999995,3,12,25000,15,701,199,90,4,681,189,89,350);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Qty,ammo1Type,ammo2Type,combits,health,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn2Mode,wpn2Range,wt) VALUES (305,'Laser Pistol',':data/c_LaserPistol.png',':data/mLaserPistol.3ds',100,1,8,8449,6,0.100000001,1.0,0.0,80,1,1,5,250,5,250,2);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorY,upVectorZ,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1AP1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (306,'Heavy Arms Ammunition PiRa',':data/c_HeavyArmsAmmo.png',':data/mHeavyArmsAmmo.3ds',2,8193,0.0500000007,1.0,0.0,1,5,13,71,29,1,5,61,19,10,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorY,upVectorZ,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1AP1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (307,'Heavy Arms Ammunition PiBl',':data/c_HeavyArmsAmmo.png',':data/mHeavyArmsAmmo.3ds',2,8193,0.0500000007,1.0,0.0,1,5,13,71,29,1,8,61,19,10,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,insertHeight,upVectorY,upVectorZ,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1AP1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (308,'Heavy Arms Ammunition PiAc',':data/c_HeavyArmsAmmo.png',':data/mHeavyArmsAmmo.3ds',2,8193,0.0500000007,1.0,0.0,1,5,13,71,29,1,1,61,19,10,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (309,'Stasis Beam Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,3,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (310,'Neutron Beam Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,5,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (311,'Electron Beam Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,7,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (312,'Erg Ray Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,8,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (313,'Freeze Ray Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,9,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (314,'Compressor Beam Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,10,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (315,'Disruptor Beam Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,11,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (316,'Heat Ray Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,12,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (317,'Proton Beam Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,13,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,ammo1Type,combits,sizewid,sizeht,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wt) VALUES (318,'Disintegrator Ray Lens',':data/c_EnergyWeapon.png',':data/mEnergyWeaponLens.3ds',1,8193,2,3,15,151,99,3);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (319,'Weapon Deployment Bay Module',':data/c_BayModule.png',':data/mCrate.3dsN',8193,100,12,20,150);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (320,'Bomb BlRa Los Alamos',':data/c_Bomb.png',':data/mBomb.3dsX',8193,2,8,3,8,751,249,100,5,151,99,250,250);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (321,'Bomb BlPi Porcupine',':data/c_Bomb.png',':data/mBomb.3dsX',8193,2,8,3,8,751,249,100,13,251,199,120,250);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (322,'Bomb ElSt Tesla',':data/c_Bomb.png',':data/mBomb.3dsX',8193,2,8,3,7,751,249,200,3,151,99,100,250);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (323,'Bomb SuPo Muffler',':data/c_Bomb.png',':data/mBomb.3dsX',8193,2,8,3,2,651,249,100,4,151,199,150,250);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wpn1Mode,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1Radius1,wpn1Effect2,wpn1Damage2,wpn1DamageMod2,wpn1Radius2,wt) VALUES (324,'Bomb SlCr Meat Slicer',':data/c_Bomb.png',':data/mBomb.3dsX',8193,2,8,3,15,351,249,100,10,251,249,100,250);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,sizewid,sizeht,wt) VALUES (325,'Seed',':data/c_Seed.png',':data/mSeed.3ds',8193,1,1,0.00999999978);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (326,'Small Flower Pot',':data/c_FlowerPotSmall.png',':data/mFlowerPotSmall.3ds',8193,10,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (327,'Large Flower Pot',':data/c_FlowerPotLarge.png',':data/mFlowerPotLarge.3ds',8193,100,2,2,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (328,'Genesis Device',':data/c_GenesisDevice.png',':data/mGenesisDevice.3ds',8193,1.04999995,120,8,4,22);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (329,'Picture Frame 60x34cm',':data/c_PictureFrame.png',':data/mPictureFrame60x34.3ds',8193,1.22500002,2,2,1,0.400000006);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (330,'Picture Frame 22x28cm',':data/c_PictureFrame.png',':data/mPictureFrame22x28.3ds',8193,1.01199996,2,1,1,0.200000003);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (331,'Picture Frame 120x68cm',':data/c_PictureFrame.png',':data/mPictureFrame120x68.3ds',8193,2.3499999,2,4,2,0.600000024);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,insertHeight,sizewid,sizeht,wt) VALUES (332,'Head',':data/c_Skull.png','',3,13,0.5,1,1,0.800000012);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,light,newcond,sizewid,sizeht,wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wt) VALUES (333,'Candle',':data/c_Candle.png',':data/mCandle.3ds',8257,3,2,1,1,2,1,12,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (334,'Type O Preon Ring',':data/c_PreonRingO.png',':data/mPreonRing.3ds',1089470464,8193,0.100000001,1.0,0.0,9,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (335,'Type B Preon Ring',':data/c_PreonRingB.png',':data/mPreonRing.3ds',1089470464,8193,0.100000001,1.0,0.0,9,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (336,'Type A Preon Ring',':data/c_PreonRingA.png',':data/mPreonRing.3ds',1089470464,8193,0.100000001,1.0,0.0,9,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (337,'Type F Preon Ring',':data/c_PreonRingF.png',':data/mPreonRing.3ds',1089470464,8193,0.100000001,1.0,0.0,9,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (338,'Type G Preon Ring',':data/c_PreonRingG.png',':data/mPreonRing.3ds',1089470464,8193,0.100000001,1.0,0.0,9,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (339,'Type K Preon Ring',':data/c_PreonRingK.png',':data/mPreonRing.3ds',1089470464,8193,0.100000001,1.0,0.0,9,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,bitsWear,combits,insertHeight,upVectorX,upVectorZ,newcond,sizewid,sizeht,wt) VALUES (340,'Type M Preon Ring',':data/c_PreonRingM.png',':data/mPreonRing.3ds',1089470464,8193,0.100000001,1.0,0.0,9,1,1,0.100000001);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,capwid,capht,combits,sizewid,sizeht,wt) VALUES (341,'ParcX Box',':data/c_ParcX.png',':data/mParcX.3ds',3,2,1,3,2,0.219999999);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,light,newcond,sizewid,sizeht,wt) VALUES (342,'Fire',':data/c_StoneFirePit.png',':data/mStoneFirePit.3ds',10,50,12,12,100);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (343,'Tepee',':data/c_Tepee.png',':data/mTepee.3ds',8193,100,3,8,50);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,newcond,sizewid,sizeht,wt) VALUES (344,'Pressure Tent',':data/c_PressureTent.png',':data/mPressureTent.3ds',8193,100,3,8,50);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,light,newcond,sizewid,sizeht,wt) VALUES (345,'Beacon',':data/c_Beacon.png',':data/mBeacon.3ds',8193,0.150000006,10,10,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,newcond,sizewid,sizeht,wt) VALUES (346,'Drum',':data/c_Drum.png',':data/mDrum.3ds',8193,1.44000006,5,4,5,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,food,newcond,sizewid,sizeht,wt) VALUES (347,'Ice Cream',':data/c_IceCream.png',':data/mIceCreamBar.3ds',20483,20,2,1,1,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,combits,insertHeight,upVectorX,upVectorZ,sizewid,sizeht,wt) VALUES (348,'Dairy Technology',':data/c_TechLevel.png',':data/mTechDisk.3dsN',1,0.0500000007,1.0,0.0,1,1,0.0500000007);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,light,newcond,sizewid,sizeht,wt) VALUES (349,'Burning Barrel',':data/c_BurningBarrel.png',':data/mCrudeOil.3dsN',5,50,12,12,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,insertHeight,upVectorX,upVectorY,upVectorZ,combits,sizewid,sizeht,wt, newcond,health, wpn1Mode,wpn1Range,ammo1Type,ammo1Qty) VALUES (350,'Bow',':data/c_Bow.png',':data/mBow.3dsN',0.13,0,1,0,8192+256+1,2,8,2.5, 60,4, 5,300,10,1);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,insertHeight,upVectorX,upVectorY,upVectorZ,combits,sizewid,sizeht,wt, stackLimit, ammo1Type,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1AP1) VALUES (351,'Arrows',':data/c_Arrows.png',':data/mArrows.3dsN',0.1,0,0,1,8192+1,1,3,.1, 100, 10,13,24,6,5);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,insertHeight,upVectorX,upVectorY,upVectorZ,combits,sizewid,sizeht,wt, newcond, wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1AP1) VALUES (352,'Spear',':data/c_Spear.png',':data/mSpear.3dsN',0.17,0,0,1,8192+1,1,8,4, 80, 2,4,13,24,8,10);
INSERT INTO b_Commodity_Legacy (id,name,iconPath,modelPath,insertHeight,upVectorX,upVectorY,upVectorZ,combits,sizewid,sizeht,wt, newcond,stackLimit, wpn1Mode,wpn1Range,wpn1Effect1,wpn1Damage1,wpn1DamageMod1,wpn1AP1) VALUES (353,'Javelin',':data/c_Javelin.png',':data/mJavelin.3dsN',0.09,0,0,1,8192+1,1,8,2.5, 60,10, 5,90,13,30,8,10);

-- Industry capacities are stored in tens of units, as in modern b_Industry.
-- emp is signed; negative values retain their original jobs/troops meaning.
-- Modern building-design restrictions were not in GD and are not invented here.
CREATE TABLE b_Industry_Legacy
(
  id               integer primary key unique,
  name             text,
  pop              smallint DEFAULT 0,
  emp              smallint DEFAULT 0,
  storage          smallint DEFAULT 0,
  capacitor        smallint DEFAULT 0
);
GRANT ALL ON b_Industry_Legacy TO audog;
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (0,NULL,0,0,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (1,'Town Square',0,-5,10,5);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (2,'Road',0,0,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (3,'Design Studio',0,-2,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (4,'Warehouse',0,-1,200,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (10,'Military Flag',0,-5,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (11,'Military Weapon System',0,-1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (12,'Military Shield Generator',0,-1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (20,'House',2,-1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (21,'Condominium',3,-2,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (22,'Apartment',4,-1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (30,'Farm',2,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (31,'Logging Camp',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (32,'Orchard',2,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (34,'Wharf',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (35,'Dairy',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (40,'Bakery',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (41,'Brewery',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (42,'Distillery',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (43,'Grist Mill',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (44,'Lumber Mill',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (45,'Meat Processing Plant',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (46,'Sugar Mill',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (47,'Textile Mill',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (48,'Winery',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (50,'Mine',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (51,'Well',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (60,'Coal Power Plant',0,1,0,40);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (61,'Gasoline Power Plant',0,1,0,40);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (62,'Hydroelectric Power Plant',0,1,0,80);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (63,'Hydrogen Power Plant',0,1,0,60);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (64,'Natural Gas Power Plant',0,1,0,60);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (65,'Nuclear Power Plant',0,1,0,100);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (66,'Solar Power Plant',0,1,0,30);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (67,'Wind Power Plant',0,1,0,30);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (70,'Aircraft Factory',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (71,'Armorer',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (72,'Carpenter',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (73,'Clothing Factory',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (74,'Computer Factory',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (75,'Electronics Factory',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (76,'Explosives Factory',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (77,'Weapon Smith',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (78,'Jeweler',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (79,'Machine Shop',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (80,'Munitions Factory',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (81,'Pharmaceutical Lab',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (82,'Plastics Factory',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (83,'Refinery',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (84,'Rocket Motor Factory',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (85,'Ship Yard',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (86,'Smelter',0,1,20,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (87,'Spacecraft Factory',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (88,'Vehicle Factory',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (89,'Treatment Plant',0,1,10,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (100,'Bank',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (101,'Broker',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (102,'Casino',0,2,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (103,'Grocery',0,1,5,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (104,'Retail Store',0,1,5,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (105,'Cantina',0,1,2,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (110,'Airport Repair Shop',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (111,'Airport Terminal',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (112,'Arena',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (113,'Zoo',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (114,'Park',0,0,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (120,'Church',0,-2,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (121,'Hospital',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (122,'Fire Station',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (123,'Capitol',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (124,'Police Station',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (125,'University',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (126,'Hangar',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (127,'Observatory',0,1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (128,'Military Guard Tower',0,-1,0,0);
INSERT INTO b_Industry_Legacy (id,name,pop,emp,storage,capacitor) VALUES (129,'Biodome',0,-1,0,0);

-- All manufacturing recipes, including industry production and technology.
-- Preserve sourceOrder when choosing between default recipes for an industry.
CREATE TABLE b_ManufacturingProcess_Legacy
(
  id               integer primary key unique,
  name             text,
  idc              integer,                      -- Historical output commodity ID.
  idi              smallint,                     -- Historical industry ID; zero means none.
  qty              integer,                      -- Factory output quantity before runtime modifiers.
  labor            integer,                      -- Unpacked labor, without the default flag.
  craft            smallint DEFAULT 2,           -- 0=no handcraft, 1=yes, 2=matching industry.
  isdefault        boolean DEFAULT false,
  nat              boolean DEFAULT true,         -- Recovery policy; not serialized by GD.
  techLevel        smallint DEFAULT 0,            -- Zero-based minimum technology level.
  sourceOrder      serial unique                 -- Original catalogue order; new recipes append.
);
GRANT ALL ON b_ManufacturingProcess_Legacy TO audog;
GRANT ALL ON b_ManufacturingProcess_Legacy_sourceOrder_seq TO audog;

CREATE TABLE b_ManufacturingComponent_Legacy
(
  id               serial primary key unique,
  idmp             integer,                      -- Historical manufacturing process ID.
  idc              integer,                      -- Historical ingredient/modifier commodity ID.
  consumed         integer,                      -- Zero means reusable when required.
  ide              smallint                      -- 1=None, 2=DecreasesTime, 3=DecreasesMaterials,
                                                 -- 4=RequiredQ, 5=Required, 6=IncreasesQuantity.
);
GRANT ALL ON b_ManufacturingComponent_Legacy TO audog;
GRANT ALL ON b_ManufacturingComponent_Legacy_id_seq TO audog;

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (265,'Aircraft Factory Technology',158,70,1,0,0,false,true,0,1);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1237,265,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1238,265,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (266,'Aircraft Factory Technology',158,125,1,1000,0,false,true,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1239,266,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1240,266,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (270,'Airport Repair Shop Technology',160,125,1,1000,0,false,true,0,3);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1243,270,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1244,270,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (269,'Airport Repair Shop Technology',160,110,1,0,0,false,true,0,4);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1241,269,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1242,269,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (272,'Airport Terminal Technology',161,125,1,1000,0,false,true,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1247,272,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1248,272,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (271,'Airport Terminal Technology',161,111,1,0,0,false,true,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1245,271,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1246,271,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (2,'Air using Atmosphere',5,83,2,24,0,false,true,0,7);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (4,2,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (5,2,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (6,2,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (127,'Air using Water',5,83,2,24,2,false,false,0,8);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (580,127,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (581,127,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (582,127,83,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (242,'Animal Carcass using Hay',14,30,1,24,0,false,true,0,9);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1126,242,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1127,242,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1128,242,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1129,242,130,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (241,'Animal Carcass using Pasture',14,30,1,24,0,false,true,0,10);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1122,241,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1123,241,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1124,241,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1125,241,89,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (487,'Animal Carcass using Silage',14,30,1,24,0,false,true,0,11);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2019,487,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2020,487,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2021,487,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2022,487,295,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (100,'Animal Meat',49,45,1,4,1,false,true,0,12);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (454,100,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (455,100,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (456,100,116,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (457,100,14,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (193,'Antidote using Herbs',119,81,1,19,2,false,true,0,13);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (938,193,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (939,193,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (940,193,42,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (941,193,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (194,'Antidote using Minerals',119,81,1,19,2,false,true,0,14);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (942,194,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (943,194,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (944,194,53,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (945,194,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (195,'Antidote using Petrochemicals',119,81,1,19,2,false,true,0,15);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (946,195,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (947,195,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (948,195,61,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (949,195,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (447,'Antiflux Particles',271,0,1,1000,0,false,true,23,16);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1860,447,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1861,447,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1862,447,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (196,'Antitoxin using Animal Carcass',120,81,1,42,2,false,true,0,17);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (950,196,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (951,196,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (952,196,14,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (953,196,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (197,'Antitoxin using Eggs',120,81,1,48,2,false,true,0,18);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (954,197,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (955,197,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (956,197,26,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (957,197,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (274,'Apartment Technology',162,125,1,1000,0,false,true,0,19);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1251,274,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1252,274,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (273,'Apartment Technology',162,22,1,0,0,false,true,0,20);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1249,273,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1250,273,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (147,'APC',104,88,1,2500,0,false,true,0,21);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (685,147,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (686,147,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (687,147,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (688,147,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (689,147,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (690,147,27,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (691,147,50,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (692,147,51,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (693,147,102,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (694,147,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (276,'Arena Technology',163,125,1,1000,0,false,true,0,22);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1255,276,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1256,276,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (275,'Arena Technology',163,112,1,0,0,false,true,0,23);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1253,275,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1254,275,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (278,'Armorer Technology',164,125,1,1000,0,false,true,0,24);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1259,278,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1260,278,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (277,'Armorer Technology',164,71,1,0,0,false,true,0,25);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1257,277,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1258,277,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (432,'Atmosphere Condenser Bay Module',256,110,1,100,2,false,true,5,26);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1750,432,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1751,432,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1752,432,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1753,432,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1754,432,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1755,432,4,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (188,'Backpack using Leather',110,73,1,8,1,false,true,0,27);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (916,188,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (917,188,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (918,188,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (919,188,47,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (189,'Backpack using Textiles',110,73,1,8,1,false,true,0,28);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (920,189,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (921,189,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (922,189,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (923,189,79,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (279,'Bakery Technology',165,40,1,0,0,false,true,0,29);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1261,279,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1262,279,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (280,'Bakery Technology',165,125,1,1000,0,false,true,0,30);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1263,280,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1264,280,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (437,'Ballistic Weapon Bay Module',261,110,1,100,2,false,true,1,31);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1788,437,27,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1789,437,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1790,437,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1791,437,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1792,437,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1793,437,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1794,437,102,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1795,437,136,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (438,'Ballistic Weapon Turret Module',262,110,1,100,2,false,true,0,32);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1796,438,27,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1797,438,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1798,438,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1799,438,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1800,438,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1801,438,126,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1802,438,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1803,438,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1804,438,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1805,438,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1806,438,102,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1807,438,136,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (281,'Bank Technology',166,100,1,0,0,false,true,0,33);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1265,281,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1266,281,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (282,'Bank Technology',166,125,1,1000,0,false,true,0,34);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1267,282,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1268,282,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (157,'Battle Ship',71,85,1,4,0,false,true,0,35);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (754,157,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (755,157,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (756,157,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (757,157,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (758,157,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (759,157,27,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (760,157,48,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (761,157,50,16,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (762,157,51,45,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (763,157,63,12,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (764,157,79,16,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (765,157,102,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (766,157,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (547,'Beacon',345,75,2,35,2,false,true,0,36);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2400,547,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2401,547,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2402,547,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2403,547,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2404,547,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2405,547,127,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (425,'Beam Laser Turret Module',249,0,1,100,2,false,true,0,37);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1684,425,27,30,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1685,425,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1686,425,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1687,425,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1688,425,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1689,425,126,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1690,425,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1691,425,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1692,425,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1693,425,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1694,425,102,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1695,425,136,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1696,425,227,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1697,425,229,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (13,'Beans',15,30,1,24,0,false,true,0,38);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (82,13,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (83,13,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (84,13,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (85,13,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (86,13,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (87,13,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (15,'Beer',16,41,10,10,2,true,true,0,39);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (92,15,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (93,15,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (94,15,38,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (95,15,43,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (96,15,83,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (191,'Blank Disk using Metal',112,75,20,4,2,false,true,0,40);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (930,191,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (931,191,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (932,191,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (933,191,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (192,'Blank Disk using Plastic',112,75,20,4,2,false,true,0,41);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (934,192,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (935,192,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (936,192,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (937,192,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (249,'Bolite',141,50,1,200,0,false,true,23,42);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1148,249,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1149,249,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1150,249,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1151,249,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1152,249,114,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1153,249,128,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (18,'Bomb Bl Black Cat using Dynamite',12,80,1,64,2,false,true,0,43);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (107,18,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (108,18,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (109,18,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (110,18,25,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (111,18,51,4,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (17,'Bomb Bl Black Cat using Nitrate Explosive',12,80,1,64,2,false,true,0,44);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (102,17,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (103,17,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (104,17,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (105,17,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (106,17,93,100,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (10,'Bomb Bl Black Cat using Plastic Explosive',12,80,1,64,2,false,true,0,45);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (67,10,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (68,10,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (69,10,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (70,10,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (71,10,13,100,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (517,'Bomb BlPi Porcupine',321,80,1,32,2,false,true,0,46);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2257,517,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2258,517,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2259,517,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2260,517,12,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2261,517,51,100,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (516,'Bomb BlRa Los Alamos',320,80,1,32,2,false,true,0,47);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2251,516,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2252,516,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2253,516,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2254,516,12,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2255,516,66,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2256,516,128,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (518,'Bomb ElSt Tesla',322,80,1,32,2,false,true,0,48);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2262,518,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2263,518,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2264,518,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2265,518,12,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2266,518,24,25,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2267,518,51,50,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2268,518,53,25,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (520,'Bomb SlCr Meat Slicer',324,80,1,32,2,false,true,0,49);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2275,520,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2276,520,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2277,520,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2278,520,12,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2279,520,51,100,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (519,'Bomb SuPo Muffler',323,80,1,32,2,false,true,0,50);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2269,519,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2270,519,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2271,519,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2272,519,12,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2273,519,61,50,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2274,519,119,50,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (406,'Bone',233,45,1,4,1,false,true,0,51);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1532,406,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1533,406,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1534,406,116,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1535,406,14,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (448,'Borexino Precipitate',272,0,1,2000,0,false,true,29,52);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1863,448,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1864,448,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1865,448,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (20,'Bread',18,40,10,2,2,true,true,0,53);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (117,20,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (118,20,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (119,20,26,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (120,20,34,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (121,20,52,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (122,20,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (283,'Brewery Technology',167,41,1,0,0,false,true,0,54);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1269,283,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1270,283,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (284,'Brewery Technology',167,125,1,1000,0,false,true,0,55);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1271,284,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1272,284,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (286,'Broker Technology',168,125,1,1000,0,false,true,0,56);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1275,286,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1276,286,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (285,'Broker Technology',168,101,1,0,0,false,true,0,57);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1273,285,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1274,285,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (468,'Building Design Disk using Metal',287,0,20,4,0,false,true,0,58);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1943,468,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1944,468,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1945,468,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1946,468,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (469,'Building Design Disk using Plastic',287,0,20,4,0,false,true,0,59);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1947,469,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1948,469,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1949,469,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1950,469,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (470,'Building Design Drawing using Lumber',288,0,10,10,0,false,true,0,60);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1951,470,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1952,470,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1953,470,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1954,470,48,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (532,'Candle using Oil',333,72,6,12,1,false,true,0,61);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2334,532,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2335,532,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2336,532,23,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2337,532,128,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (533,'Candle using Oil',333,82,6,12,2,false,true,0,62);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2338,533,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2339,533,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2340,533,23,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2341,533,128,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (534,'Candle using Petrochemicals',333,82,6,12,2,false,true,0,63);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2342,534,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2343,534,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2344,534,61,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2345,534,128,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (21,'Candy',19,40,12,6,2,false,true,0,64);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (123,21,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (124,21,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (125,21,52,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (126,21,74,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (127,21,78,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (128,21,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (377,'Cantina Technology',215,105,1,0,0,false,true,0,65);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1453,377,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1454,377,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (378,'Cantina Technology',215,125,1,1000,0,false,true,0,66);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1455,378,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1456,378,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (288,'Capitol Technology',169,125,1,1000,0,false,true,0,67);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1279,288,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1280,288,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (287,'Capitol Technology',169,123,1,0,0,false,true,0,68);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1277,287,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1278,287,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (290,'Carpenter Technology',170,125,1,1000,0,false,true,0,69);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1283,290,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1284,290,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (289,'Carpenter Technology',170,72,1,0,0,false,true,0,70);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1281,289,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1282,289,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (291,'Casino Technology',171,102,1,0,0,false,true,0,71);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1285,291,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1286,291,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (292,'Casino Technology',171,125,1,1000,0,false,true,0,72);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1287,292,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1288,292,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (550,'Cheese',20,35,1,24,0,false,true,0,73);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2409,550,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2410,550,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2411,550,52,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (230,'Cheese using Hay',20,30,1,24,0,false,true,0,74);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1102,230,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1103,230,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1104,230,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1105,230,130,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (229,'Cheese using Pasture',20,30,1,24,0,false,true,0,75);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1098,229,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1099,229,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1100,229,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1101,229,89,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (488,'Cheese using Silage',20,30,1,24,0,false,true,0,76);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2023,488,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2024,488,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2025,488,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2026,488,295,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (293,'Church Technology',172,120,1,0,0,false,true,0,77);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1289,293,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1290,293,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (294,'Church Technology',172,125,1,1000,0,false,true,0,78);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1291,294,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1292,294,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (400,'Clamp',229,82,1,12,2,false,true,0,79);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1497,400,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1498,400,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1499,400,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1500,400,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (401,'Clamp',229,79,1,8,2,false,true,0,80);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1501,401,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1502,401,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1503,401,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1504,401,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (418,'Cloaking Shield Module',242,110,1,100,2,false,true,27,81);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1616,418,27,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1617,418,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1618,418,63,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1619,418,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1620,418,265,50,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1621,418,229,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1622,418,228,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1623,418,230,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1624,418,269,50,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1625,418,270,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (295,'Clothing Factory Technology',173,73,1,0,0,false,true,0,82);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1293,295,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1294,295,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (296,'Clothing Factory Technology',173,125,1,1000,0,false,true,0,83);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1295,296,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1296,296,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (156,'Coal',22,50,1,10,0,false,true,0,84);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (749,156,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (750,156,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (751,156,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (752,156,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (753,156,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (298,'Coal Power Plant Technology',174,125,1,1000,0,false,true,0,85);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1299,298,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1300,298,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (297,'Coal Power Plant Technology',174,60,1,0,0,false,true,0,86);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1297,297,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1298,297,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (410,'Command And Control Module',234,110,1,100,2,true,true,0,87);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1546,410,2,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1547,410,27,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1548,410,50,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1549,410,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1550,410,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1551,410,125,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1552,410,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1553,410,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1554,410,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1555,410,126,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (151,'Commercial Ship',70,85,1,4,0,false,true,0,88);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (723,151,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (724,151,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (725,151,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (726,151,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (727,151,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (728,151,27,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (729,151,48,25,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (730,151,50,9,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (731,151,51,30,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (732,151,63,15,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (733,151,79,22,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (734,151,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (510,'Compressor Beam Lens',314,77,1,250,2,false,true,29,89);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2194,510,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2195,510,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2196,510,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2197,510,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2198,510,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2199,510,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2200,510,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2201,510,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2202,510,265,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2203,510,272,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (27,'Computer',2,74,1,137,2,true,true,0,90);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (153,27,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (154,27,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (155,27,27,5,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (156,27,51,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (157,27,63,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (158,27,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (159,27,127,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (302,'Computer Factory Technology',176,125,1,1000,0,false,true,0,91);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1307,302,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1308,302,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (301,'Computer Factory Technology',176,74,1,0,0,false,true,0,92);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1305,301,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1306,301,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (326,'Condominium Technology',188,125,1,1000,0,false,true,0,93);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1353,326,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1354,326,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (325,'Condominium Technology',188,21,1,0,0,false,true,0,94);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1351,325,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1352,325,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (182,'Crate using Lumber',107,72,1,4,1,false,true,0,95);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (890,182,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (891,182,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (892,182,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (893,182,48,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (183,'Crate using Plastic',107,82,2,4,2,false,true,0,96);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (894,183,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (895,183,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (896,183,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (897,183,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (95,'Cryo Heat Sink',227,79,1,24,2,false,true,3,97);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (431,95,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (432,95,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (433,95,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (434,95,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (435,95,142,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (251,'Cryozine',142,83,1,200,0,false,true,3,98);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1158,251,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1159,251,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1160,251,79,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1161,251,150,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (97,'Crystal Necklace',46,78,6,8,2,false,true,0,99);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (441,97,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (442,97,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (443,97,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (444,97,24,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (445,97,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (39,'Crystals',24,50,1,30,0,false,true,0,100);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (205,39,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (206,39,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (207,39,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (208,39,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (209,39,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (421,'Cybernetics Medical Module',245,110,1,100,2,false,true,21,101);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1646,421,27,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1647,421,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1648,421,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1649,421,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1650,421,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1651,421,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1652,421,63,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1653,421,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1654,421,268,40,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1655,421,125,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (552,'Dairy Technology',348,35,1,0,0,false,true,0,102);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2416,552,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2417,552,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (553,'Dairy Technology',348,125,1,1000,0,false,true,0,103);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2418,553,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2419,553,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (304,'Design Studio Technology',177,125,1,1000,0,false,true,0,104);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1309,304,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1310,304,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (514,'Disintegrator Ray Lens',318,77,1,250,2,false,true,25,105);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2234,514,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2235,514,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2236,514,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2237,514,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2238,514,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2239,514,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2240,514,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2241,514,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2242,514,141,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2243,514,265,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (511,'Disruptor Beam Lens',315,77,1,250,2,false,true,12,106);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2204,511,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2205,511,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2206,511,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2207,511,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2208,511,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2209,511,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2210,511,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2211,511,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2212,511,143,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2213,511,264,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (306,'Distillery Technology',178,125,1,1000,0,false,true,0,107);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1313,306,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1314,306,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (305,'Distillery Technology',178,42,1,0,0,false,true,0,108);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1311,305,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1312,305,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (462,'DNA Sample',285,0,10,12,0,false,true,0,109);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1923,462,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1924,462,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1925,462,79,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1926,462,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (461,'DNA Sample Kit',284,81,10,12,2,false,true,0,110);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1919,461,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1920,461,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1921,461,79,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1922,461,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (548,'Drum',346,0,1,35,1,false,true,0,111);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2406,548,47,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2407,548,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (7,'Dynamite',25,80,4,9,1,true,true,0,112);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (54,7,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (55,7,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (56,7,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (57,7,40,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (234,'Eggs using Hay',26,30,1,24,0,false,true,0,113);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1110,234,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1111,234,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1112,234,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1113,234,130,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (233,'Eggs using Pasture',26,30,1,24,0,false,true,0,114);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1106,233,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1107,233,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1108,233,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1109,233,89,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (489,'Eggs using Silage',26,30,1,24,0,false,true,0,115);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2027,489,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2028,489,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2029,489,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2030,489,295,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (44,'Electricity using Coal',3,60,40,4,0,true,true,0,116);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (227,44,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (228,44,22,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (229,44,114,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (45,'Electricity using Gasoline',3,61,40,4,0,true,true,0,117);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (230,45,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (231,45,36,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (232,45,123,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (46,'Electricity using Hydrogen',3,63,60,4,0,true,true,0,118);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (233,46,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (234,46,44,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (235,46,123,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (47,'Electricity using Natural Gas',3,64,60,4,0,true,true,0,119);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (236,47,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (237,47,57,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (238,47,123,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (50,'Electricity using Radioactives',3,65,100,4,0,true,true,0,120);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (243,50,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (244,50,66,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (245,50,128,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (48,'Electricity using Sunlight',3,66,30,4,0,true,true,0,121);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (239,48,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (240,48,94,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (80,'Electricity using Water in Environment',3,62,80,4,0,true,true,0,122);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (373,80,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (374,80,88,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (49,'Electricity using Wind',3,67,30,4,0,true,true,0,123);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (241,49,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (242,49,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (507,'Electron Beam Lens',311,77,1,250,2,false,true,3,124);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2164,507,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2165,507,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2166,507,24,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2167,507,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2168,507,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2169,507,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2170,507,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2171,507,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2172,507,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2173,507,227,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (51,'Electronic Part',27,75,2,9,2,true,true,0,125);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (246,51,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (247,51,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (248,51,24,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (249,51,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (250,51,53,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (251,51,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (252,51,127,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (308,'Electronics Factory Technology',179,125,1,1000,0,false,true,0,126);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1317,308,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1318,308,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (307,'Electronics Factory Technology',179,75,1,0,0,false,true,0,127);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1315,307,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1316,307,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (205,'Eludium',134,50,1,110,0,false,true,0,128);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (987,205,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (988,205,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (989,205,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (990,205,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (991,205,114,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (992,205,128,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (416,'Energy Shield Module',240,110,1,100,2,false,true,3,129);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1602,416,27,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1603,416,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1604,416,63,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1605,416,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1606,416,227,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1607,416,229,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (428,'Energy Weapon Bay Module',252,110,1,100,2,false,true,3,130);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1722,428,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1723,428,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1724,428,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1725,428,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1726,428,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1727,428,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1728,428,227,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1729,428,229,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (424,'Energy Weapon Turret Module',248,110,1,100,2,false,true,0,131);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1670,424,27,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1671,424,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1672,424,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1673,424,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1674,424,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1675,424,126,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1676,424,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1677,424,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1678,424,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1679,424,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1680,424,102,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1681,424,136,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1682,424,227,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1683,424,229,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (166,'Environment Suit',80,71,1,608,2,false,true,0,132);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (804,166,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (805,166,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (806,166,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (807,166,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (808,166,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (809,166,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (810,166,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (811,166,63,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (812,166,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2394,166,63,3,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (168,'Environment Suit Helmet',72,71,1,120,2,false,true,0,133);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (818,168,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (819,168,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (820,168,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (821,168,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (822,168,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (823,168,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (824,168,27,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (825,168,63,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (826,168,79,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (508,'Erg Ray Lens',312,77,1,250,2,false,true,7,134);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2174,508,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2175,508,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2176,508,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2177,508,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2178,508,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2179,508,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2180,508,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2181,508,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2182,508,228,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2183,508,227,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (310,'Explosives Factory Technology',180,125,1,1000,0,false,true,0,135);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1321,310,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1322,310,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (309,'Explosives Factory Technology',180,76,1,0,0,false,true,0,136);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1319,309,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1320,309,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (69,'Fabric Clothing',21,73,2,15,1,false,true,0,137);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (327,69,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (328,69,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (329,69,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (330,69,126,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (52,'Farm Machinery',28,88,1,1500,0,false,true,0,138);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (253,52,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (254,52,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (255,52,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (256,52,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (257,52,50,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (258,52,51,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (259,52,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (311,'Farm Technology',181,30,1,0,0,false,true,0,139);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1323,311,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1324,311,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (312,'Farm Technology',181,125,1,1000,0,false,true,0,140);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1325,312,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1326,312,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (14,'Fertilizer using Air',29,83,1,25,0,false,false,0,141);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (88,14,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (89,14,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (90,14,44,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (91,14,5,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (12,'Fertilizer using Atmosphere',29,83,1,25,0,false,false,0,142);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (78,12,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (79,12,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (80,12,44,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (81,12,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (238,'Fertilizer using Hay',29,30,1,24,0,false,true,0,143);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1118,238,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1119,238,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1120,238,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1121,238,130,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (237,'Fertilizer using Pasture',29,30,1,24,0,false,true,0,144);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1114,237,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1115,237,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1116,237,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1117,237,89,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (490,'Fertilizer using Silage',29,30,1,24,0,false,true,0,145);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2031,490,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2032,490,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2033,490,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2034,490,295,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (313,'Fire Station Technology',182,122,1,0,0,false,true,0,146);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1327,313,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1328,313,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (314,'Fire Station Technology',182,125,1,1000,0,false,true,0,147);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1329,314,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1330,314,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (549,'Fire using Hay',342,0,1,1,1,false,true,0,148);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2408,549,130,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (543,'Fire using Log',342,0,1,1,1,false,true,0,149);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2385,543,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (554,'Fire using Oil',349,0,1,1,1,false,true,0,150);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2420,554,23,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (67,'Fish',33,34,1,24,0,true,true,0,151);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (321,67,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (322,67,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (323,67,88,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (99,'Fish Meat',100,45,1,3,1,true,true,0,152);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (450,99,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (451,99,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (452,99,116,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (453,99,33,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (208,'Fitting Wrench',123,79,3,9,2,false,true,0,153);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1002,208,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1003,208,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1004,208,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1005,208,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (207,'Fitting Wrench',123,82,3,14,2,false,true,0,154);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (998,207,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (999,207,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1000,207,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1001,207,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (25,'Flag',1,73,2,4,1,true,true,0,155);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (143,25,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (144,25,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (145,25,79,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (146,25,126,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (68,'Flour',34,43,4,8,2,true,true,0,156);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (324,68,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (325,68,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (326,68,38,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (509,'Freeze Ray Lens',313,77,1,250,2,false,true,5,157);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2184,509,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2185,509,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2186,509,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2187,509,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2188,509,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2189,509,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2190,509,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2191,509,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2192,509,142,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2193,509,227,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (70,'Fruit',35,32,1,24,0,true,true,0,158);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (331,70,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (332,70,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (333,70,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (334,70,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (335,70,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (336,70,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (53,'Fruit',35,30,1,24,0,false,true,0,159);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (260,53,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (261,53,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (262,53,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (263,53,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (264,53,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (265,53,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (72,'Gasoline',36,83,2,24,2,false,true,0,160);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (342,72,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (343,72,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (344,72,23,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (315,'Gasoline Power Plant Technology',183,61,1,0,0,false,true,0,161);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1331,315,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1332,315,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (316,'Gasoline Power Plant Technology',183,125,1,1000,0,false,true,0,162);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1333,316,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1334,316,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (96,'Gem Ring',99,78,2,77,2,true,true,0,163);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (436,96,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (437,96,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (438,96,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (439,96,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (440,96,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (73,'Gems',37,50,1,100,0,false,true,0,164);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (345,73,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (346,73,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (347,73,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (348,73,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (349,73,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (528,'Genesis Device',328,81,1,1000,2,false,true,30,165);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2304,528,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2305,528,3,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2306,528,27,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2307,528,50,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2308,528,51,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2309,528,63,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2310,528,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2311,528,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2312,528,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2313,528,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2314,528,141,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2315,528,142,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2316,528,144,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2317,528,266,21,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2318,528,267,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2319,528,268,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2320,528,270,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2321,528,272,6,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (420,'Genetics Medical Module',244,110,1,100,2,false,true,12,166);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1636,420,27,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1637,420,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1638,420,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1639,420,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1640,420,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1641,420,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1642,420,63,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1643,420,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1644,420,266,40,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1645,420,267,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (444,'Gigacell',268,75,1,30,2,false,true,4,167);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1837,444,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1838,444,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1839,444,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1840,444,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1841,444,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1842,444,140,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (172,'Gin',115,42,1,4,2,false,true,0,168);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (843,172,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (844,172,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (845,172,38,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (846,172,74,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (847,172,78,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (848,172,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (209,'Glue Gun',124,79,3,22,2,false,true,0,169);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1006,209,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1007,209,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1008,209,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1009,209,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1010,209,57,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (210,'Glue Gun',124,75,3,11,2,false,true,0,170);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1011,210,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1012,210,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1013,210,127,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1014,210,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1015,210,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (78,'Grain',38,30,1,24,0,false,true,0,171);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (367,78,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (368,78,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (369,78,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (370,78,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (371,78,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (372,78,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (112,'Grapes',39,30,1,24,0,false,true,0,172);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (503,112,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (504,112,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (505,112,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (506,112,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (507,112,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (508,112,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (93,'Grav Coupling',226,75,1,20,2,false,true,0,173);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (421,93,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (422,93,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (423,93,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (424,93,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (425,93,134,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (411,'Gravity Drive Module',235,110,1,100,2,false,true,0,174);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1556,411,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1557,411,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1558,411,50,600,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1559,411,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1560,411,51,600,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1561,411,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1562,411,63,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1563,411,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1564,411,226,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1565,411,135,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (227,'Grav Pack using Leather',294,73,1,12,2,false,true,1,175);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1082,227,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1083,227,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1084,227,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1085,227,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1086,227,135,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1087,227,47,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1088,227,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1089,227,226,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (228,'Grav Pack using Textiles',294,73,1,12,2,false,true,1,176);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1090,228,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1091,228,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1092,228,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1093,228,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1094,228,135,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1095,228,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1096,228,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1097,228,226,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (317,'Grist Mill Technology',184,43,1,0,0,false,true,0,177);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1335,317,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1336,317,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (318,'Grist Mill Technology',184,125,1,1000,0,false,true,0,178);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1337,318,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1338,318,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (320,'Grocery Technology',185,125,1,1000,0,false,true,0,179);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1341,320,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1342,320,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (319,'Grocery Technology',185,103,1,0,0,false,true,0,180);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1339,319,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1340,319,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (41,'Gun Powder',40,76,2,2,1,true,true,0,181);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (215,41,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (216,41,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (217,41,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (218,41,22,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (219,41,53,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (220,41,93,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (163,'Hammer using Log',4,72,3,23,1,false,true,0,182);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (792,163,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (793,163,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (794,163,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (795,163,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (179,'Hammer using Lumber',4,72,1,4,1,false,true,0,183);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (877,179,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (878,179,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (879,179,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (880,179,48,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (165,'Hammer using Metal',4,79,3,9,2,false,true,0,184);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (800,165,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (801,165,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (802,165,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (803,165,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (164,'Hammer using Plastic',4,82,3,14,2,false,true,0,185);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (796,164,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (797,164,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (798,164,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (799,164,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (180,'Hammer using Stone',4,72,2,10,1,true,true,0,186);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (881,180,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (882,180,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (883,180,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (884,180,76,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (42,'Hay',130,30,1,24,0,false,true,0,187);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (221,42,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (222,42,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (223,42,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (224,42,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (225,42,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (226,42,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (200,'Head',332,0,1,10,0,false,true,0,188);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (966,200,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (967,200,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (968,200,116,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (969,200,14,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (512,'Heat Ray Lens',316,77,1,250,2,false,true,6,189);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2214,512,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2215,512,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2216,512,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2217,512,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2218,512,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2219,512,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2220,512,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2221,512,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2222,512,140,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2223,512,227,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (8,'Heavy Arms Ammo Pi',11,80,1,62,2,false,true,0,190);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (58,8,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (59,8,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (60,8,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (61,8,40,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (62,8,51,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (504,'Heavy Arms Ammo PiAc',308,80,1,72,2,false,true,0,191);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2138,504,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2139,504,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2140,504,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2141,504,40,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2142,504,51,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2143,504,61,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (503,'Heavy Arms Ammo PiBl',307,80,1,72,2,false,true,0,192);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2132,503,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2133,503,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2134,503,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2135,503,40,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2136,503,51,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2137,503,13,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (502,'Heavy Arms Ammo PiRa',306,80,1,72,2,false,true,0,193);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2126,502,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2127,502,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2128,502,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2129,502,40,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2130,502,51,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2131,502,66,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (176,'Heavy Weapon',102,77,1,40,2,false,true,0,194);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (860,176,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (861,176,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (862,176,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (863,176,50,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (864,176,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (865,176,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (866,176,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (3,'Helicopter',6,70,1,100,0,false,true,0,195);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (7,3,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (8,3,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (9,3,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (10,3,50,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (11,3,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (12,3,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (13,3,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (14,3,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (15,3,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (16,3,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (76,'Herbs',42,32,1,24,0,false,true,0,196);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (361,76,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (362,76,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (363,76,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (364,76,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (365,76,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (366,76,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (86,'Herbs',42,30,1,24,0,false,true,0,197);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (395,86,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (396,86,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (397,86,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (398,86,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (399,86,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (400,86,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (89,'Hops',43,30,1,24,0,false,true,0,198);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (409,89,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (410,89,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (411,89,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (412,89,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (413,89,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (414,89,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (323,'Hospital Technology',187,121,1,0,0,false,true,0,199);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1347,323,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1348,323,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (324,'Hospital Technology',187,125,1,1000,0,false,true,0,200);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1349,324,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1350,324,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (328,'House Technology',189,125,1,1000,0,false,true,0,201);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1357,328,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1358,328,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (327,'House Technology',189,20,1,0,0,false,true,0,202);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1355,327,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1356,327,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (330,'Hydroelectric Power Plant Technology',190,125,1,1000,0,false,true,0,203);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1361,330,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1362,330,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (329,'Hydroelectric Power Plant Technology',190,62,1,0,0,false,true,0,204);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1359,329,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1360,329,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (331,'Hydrogen Power Plant Technology',191,63,1,0,0,false,true,0,205);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1363,331,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1364,331,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (332,'Hydrogen Power Plant Technology',191,125,1,1000,0,false,true,0,206);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1365,332,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1366,332,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (1,'Hydrogen using Atmosphere',44,83,1,24,0,true,true,0,207);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1,1,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2,1,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (3,1,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (23,'Hydrogen using Natural Gas',44,83,1,25,0,false,false,0,208);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (135,23,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (136,23,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (137,23,57,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (92,'Hydrogen using Water',44,83,1,24,2,false,false,0,209);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (418,92,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (419,92,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (420,92,83,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (94,'Ice',45,50,1,5,0,false,true,0,210);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (426,94,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (427,94,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (428,94,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (429,94,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (430,94,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (551,'Ice Cream',347,35,2,24,0,false,true,0,211);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2412,551,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2413,551,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2414,551,52,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2415,551,78,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (250,'Ioplasma',138,83,1,200,0,false,true,6,212);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1154,250,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1155,250,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1156,250,79,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1157,250,149,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (108,'Io Tube',228,75,2,90,2,false,true,6,213);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (486,108,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (487,108,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (488,108,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (490,108,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (491,108,138,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (333,'Jeweler Technology',192,78,1,0,0,false,true,0,214);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1367,333,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1368,333,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (334,'Jeweler Technology',192,125,1,1000,0,false,true,0,215);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1369,334,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1370,334,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (417,'Kinetic Shield Module',241,110,1,100,2,false,true,11,216);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1608,417,27,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1609,417,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1610,417,63,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1611,417,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1612,417,264,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1613,417,229,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1614,417,228,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1615,417,230,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (186,'Knapsack using Leather',109,73,1,4,1,false,true,0,217);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (908,186,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (909,186,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (910,186,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (911,186,47,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (187,'Knapsack using Textiles',109,73,1,4,1,false,true,0,218);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (912,187,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (913,187,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (914,187,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (915,187,79,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (56,'Knife using Metal',116,77,1,30,2,false,true,0,219);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (271,56,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (272,56,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (273,56,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (274,56,51,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (57,'Knife using Metal and Plastic',116,77,1,30,2,false,true,0,220);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (275,57,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (276,57,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (277,57,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (278,57,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (279,57,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (55,'Knife using Metal and Wood',116,77,1,30,2,true,true,0,221);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (266,55,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (267,55,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (268,55,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (269,55,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (270,55,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (184,'Lantern',108,79,2,22,2,false,true,0,222);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (898,184,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (899,184,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (900,184,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (901,184,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (902,184,57,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (185,'Lantern',108,75,2,11,2,false,true,0,223);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (903,185,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (904,185,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (905,185,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (906,185,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (907,185,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (525,'Large Flower Pot using Lumber',327,72,4,10,2,false,true,0,224);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2295,525,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2296,525,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2297,525,48,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (527,'Large Flower Pot using Metal',327,79,4,12,2,false,true,0,225);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2301,527,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2302,527,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2303,527,51,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (523,'Large Flower Pot using Plastic',327,82,5,8,2,false,true,0,226);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2289,523,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2290,523,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2291,523,63,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (496,'Large Missile BlBu Fireball',300,80,1,1582,2,false,true,0,227);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2072,496,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2073,496,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2074,496,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2075,496,27,14,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2076,496,64,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2077,496,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2078,496,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2079,496,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2080,496,12,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2081,496,36,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (120,'Large Missile Bl Thumper',56,80,1,1532,2,false,true,0,228);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (548,120,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (549,120,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (550,120,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (551,120,27,14,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (552,120,64,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (553,120,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (554,120,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (555,120,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (556,120,12,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (500,'Large Missile SlPo Sideviper',304,80,1,1582,2,false,true,0,229);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2106,500,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2107,500,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2108,500,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2109,500,27,14,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2110,500,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2111,500,64,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2112,500,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2113,500,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2114,500,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2115,500,12,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2116,500,119,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (494,'Large Rocket BlBu Molotov',298,80,1,798,2,false,true,0,230);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2055,494,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2056,494,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2057,494,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2058,494,64,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2059,494,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2060,494,12,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2061,494,36,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (140,'Large Rocket Bl Thud',68,80,1,748,2,false,true,0,231);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (641,140,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (642,140,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (643,140,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (644,140,64,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (645,140,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (646,140,12,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (498,'Large Rocket BuDi Scudpox',302,80,1,798,2,false,true,0,232);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2089,498,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2090,498,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2091,498,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2092,498,64,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2093,498,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2094,498,36,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2095,498,120,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (24,'Large Rocket Motor using Gun Powder',64,84,1,440,2,false,true,0,233);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (138,24,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (139,24,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (140,24,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (141,24,40,40,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (142,24,51,20,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (19,'Large Rocket Motor using Nitrate Explosive',64,84,1,440,2,true,true,0,234);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (112,19,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (113,19,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (114,19,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (115,19,51,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (116,19,93,40,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (30,'Large Rocket Motor using Petrochemical',64,84,1,440,2,false,true,0,235);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (168,30,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (169,30,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (170,30,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (171,30,51,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (172,30,61,40,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (501,'Laser Pistol',305,77,1,192,2,false,true,5,236);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2117,501,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2118,501,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2119,501,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2120,501,37,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2121,501,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2122,501,63,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2123,501,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2124,501,227,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2125,501,229,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (492,'Laser Rifle',296,77,1,255,2,false,true,4,237);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2039,492,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2040,492,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2041,492,27,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2042,492,37,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2043,492,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2044,492,63,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2045,492,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2046,492,227,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2047,492,229,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (101,'Leather',47,45,1,11,1,false,true,0,238);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (458,101,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (459,101,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (460,101,116,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (461,101,14,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (16,'Leather Body Armor',17,71,1,129,2,true,true,0,239);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (97,16,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (98,16,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (99,16,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (100,16,47,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (101,16,126,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (82,'Leather Clothing',95,73,2,19,1,false,true,0,240);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (379,82,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (380,82,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (381,82,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (382,82,47,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (84,'Leather Helmet',96,71,1,40,2,false,true,0,241);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (387,84,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (388,84,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (389,84,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (390,84,47,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (414,'Life Support Module',238,110,1,100,2,false,true,0,242);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1582,414,5,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1583,414,123,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1584,414,27,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1585,414,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1586,414,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1587,414,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1588,414,51,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1589,414,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1590,414,63,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1591,414,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1592,414,79,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1593,414,126,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (175,'Log',86,31,1,12,0,true,true,0,243);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (857,175,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (858,175,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (859,175,4,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (408,'Log',86,32,1,24,0,true,true,0,244);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1540,408,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1541,408,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1542,408,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1543,408,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1544,408,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1545,408,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (336,'Logging Camp Technology',193,125,1,1000,0,false,true,0,245);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1373,336,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1374,336,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (335,'Logging Camp Technology',193,31,1,0,0,false,true,0,246);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1371,335,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1372,335,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (102,'Lumber',48,44,2,4,1,true,true,0,247);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (462,102,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (463,102,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (464,102,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (465,102,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (338,'Lumber Mill Technology',194,125,1,1000,0,false,true,0,248);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1377,338,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1378,338,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (337,'Lumber Mill Technology',194,44,1,0,0,false,true,0,249);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1375,337,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1376,337,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (190,'Lumenite',111,50,1,120,0,false,true,0,250);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (924,190,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (925,190,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (926,190,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (927,190,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (928,190,114,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (929,190,128,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (339,'Machine Shop Technology',195,79,1,0,0,false,true,0,251);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1379,339,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1380,339,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (340,'Machine Shop Technology',195,125,1,1000,0,false,true,0,252);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1381,340,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1382,340,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (252,'Magmex',139,89,1,80,0,false,true,9,253);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1162,252,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1163,252,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1164,252,114,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1165,252,128,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1166,252,147,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (257,'Magmium',146,86,1,47,2,false,true,9,254);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1181,257,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1182,257,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1183,257,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1184,257,139,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (464,'Magmium Bullion',157,79,250,49,2,false,true,0,255);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1931,464,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1932,464,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1933,464,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1934,464,146,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (440,'Magmium Heat Sink',264,79,1,50,2,false,true,9,256);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1814,440,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1815,440,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1816,440,146,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1817,440,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1818,440,142,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (456,'Magmium Hull Module',280,110,1,500,2,false,true,0,257);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1887,456,27,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1888,456,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1889,456,50,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1890,456,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1891,456,146,5000,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1892,456,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1893,456,63,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1894,456,124,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (104,'Magmium Knife using Magmium',151,77,1,100,2,false,true,9,258);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (471,104,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (472,104,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (473,104,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (474,104,146,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (105,'Magmium Knife using Magmium and Plastic',151,77,1,100,2,false,true,9,259);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (475,105,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (476,105,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (477,105,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (478,105,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (479,105,146,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (489,105,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (103,'Magmium Knife using Magmium and Wood',151,77,1,100,2,false,true,9,260);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (466,103,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (467,103,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (468,103,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (469,103,86,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (470,103,146,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (150,'Magmium Power Armor Helmet',154,71,1,350,2,false,true,9,261);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (711,150,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (712,150,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (713,150,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (714,150,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (715,150,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (716,150,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (717,150,27,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (718,150,50,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (719,150,63,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (720,150,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (721,150,146,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (722,150,264,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (261,'Magmium Power Armor Suit',153,71,1,1220,2,false,true,9,262);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1199,261,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1200,261,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1201,261,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1202,261,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1203,261,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1204,261,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1205,261,27,16,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1206,261,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1207,261,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1208,261,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1209,261,146,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1210,261,264,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (341,'Meat Processing Plant Technology',196,45,1,0,0,false,true,0,263);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1383,341,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1384,341,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (342,'Meat Processing Plant Technology',196,125,1,1000,0,false,true,0,264);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1385,342,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1386,342,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (109,'Mechanical Part',50,79,1,9,2,true,true,0,265);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (492,109,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (493,109,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (494,109,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (495,109,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (131,'Mechanical Part using Plastic',50,82,1,9,2,false,true,0,266);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (595,131,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (596,131,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (597,131,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (598,131,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (31,'Medical Kit',73,81,1,3,1,true,true,0,267);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (173,31,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (174,31,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (175,31,62,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (176,31,79,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (177,31,126,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (111,'Metal',51,86,1,4,2,true,true,0,268);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (499,111,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (500,111,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (501,111,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (502,111,59,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (35,'Metal Body Armor',90,71,1,190,2,false,true,0,269);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (191,35,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (192,35,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (193,35,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (194,35,51,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (98,'Metal Bracelet',98,78,6,9,2,false,true,0,270);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (446,98,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (447,98,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (448,98,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (449,98,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (463,'Metal Bullion',157,79,25,4,2,false,true,0,271);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1927,463,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1928,463,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1929,463,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1930,463,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (148,'Metal Helmet',105,71,1,60,2,false,true,0,272);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (695,148,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (696,148,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (697,148,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (698,148,51,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (413,'Metal Hull Module',237,110,1,100,2,false,true,0,273);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1574,413,27,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1575,413,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1576,413,50,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1577,413,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1578,413,51,5000,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1579,413,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1580,413,63,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1581,413,124,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (149,'Metal Power Armor Helmet',106,71,1,250,2,false,true,3,274);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (699,149,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (700,149,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (701,149,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (702,149,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (703,149,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (704,149,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (705,149,27,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (706,149,50,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (707,149,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (708,149,63,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (709,149,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (710,149,227,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (134,'Metal Power Armor Suit',65,71,1,920,2,false,true,3,275);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (605,134,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (606,134,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (607,134,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (608,134,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (609,134,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (610,134,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (611,134,27,16,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (612,134,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (613,134,51,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (614,134,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (615,134,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (616,134,227,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (443,'Microscope',267,75,1,100,2,false,true,0,276);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1829,443,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1830,443,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1831,443,24,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1832,443,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1833,443,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1834,443,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1835,443,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1836,443,127,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (472,'Military Guard Tower Technology',159,125,1,1000,0,false,true,0,277);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1957,472,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1958,472,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (471,'Military Guard Tower Technology',159,128,1,0,0,false,true,0,278);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1955,471,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1956,471,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (299,'Military Shield Generator Technology',175,12,1,0,0,false,true,0,279);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1301,299,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1302,299,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (300,'Military Shield Generator Technology',175,125,1,1000,0,false,true,0,280);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1303,300,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1304,300,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (322,'Military Weapon System Technology',186,125,1,1000,0,false,true,0,281);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1345,322,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1346,322,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (321,'Military Weapon System Technology',186,11,1,0,0,false,true,0,282);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1343,321,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1344,321,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (226,'Milk using Hay',52,30,1,24,0,false,true,0,283);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1078,226,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1079,226,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1080,226,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1081,226,130,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (225,'Milk using Pasture',52,30,1,24,0,false,true,0,284);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1074,225,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1075,225,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1076,225,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1077,225,89,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (491,'Milk using Silage',52,30,1,24,0,false,true,0,285);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2035,491,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2036,491,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2037,491,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2038,491,295,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (116,'Mine Machinery',54,88,1,1500,0,false,true,0,286);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (514,116,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (515,116,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (516,116,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (517,116,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (518,116,50,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (519,116,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (520,116,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (114,'Minerals',53,50,1,20,0,false,true,0,287);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (509,114,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (510,114,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (511,114,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (512,114,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (513,114,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (343,'Mine Technology',197,50,1,0,0,false,true,0,288);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1387,343,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1388,343,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (344,'Mine Technology',197,125,1,1000,0,false,true,0,289);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1389,344,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1390,344,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (431,'Missile Launcher Bay Module',255,0,1,100,2,false,true,1,290);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1744,431,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1745,431,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1746,431,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1747,431,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1748,431,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1749,431,4,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (427,'Missile Launcher Turret Module',251,0,1,100,2,false,true,0,291);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1710,427,27,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1711,427,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1712,427,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1713,427,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1714,427,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1715,427,126,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1716,427,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1717,427,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1718,427,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1719,427,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1720,427,102,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1721,427,136,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (404,'Mission Disk using Metal',232,0,20,4,0,false,true,0,292);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1524,404,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1525,404,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1526,404,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1527,404,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (405,'Mission Disk using Plastic',232,0,20,4,0,false,true,0,293);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1528,405,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1529,405,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1530,405,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1531,405,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (28,'Motor Bike',133,88,1,150,0,true,true,0,294);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (160,28,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (161,28,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (162,28,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (163,28,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (164,28,50,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (165,28,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (166,28,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (167,28,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (345,'Munitions Factory Technology',198,80,1,0,0,false,true,0,295);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1391,345,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1392,345,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (346,'Munitions Factory Technology',198,125,1,1000,0,false,true,0,296);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1393,346,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1394,346,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (442,'Myrasplicer',266,81,1,140,2,false,true,12,297);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1824,442,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1825,442,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1826,442,24,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1827,442,143,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1828,442,267,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (253,'Myrathane',143,89,1,200,0,false,true,12,298);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1167,253,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1168,253,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1169,253,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1170,253,148,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (446,'Nanopath Calibrator',270,75,1,90,2,false,true,23,299);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1853,446,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1854,446,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1855,446,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1856,446,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1857,446,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1858,446,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1859,446,143,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (121,'Natural Gas',57,51,1,24,0,false,true,0,300);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (557,121,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (558,121,3,1,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (348,'Natural Gas Power Plant Technology',199,125,1,1000,0,false,true,0,301);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1397,348,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1398,348,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (347,'Natural Gas Power Plant Technology',199,64,1,0,0,false,true,0,302);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1395,347,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1396,347,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (506,'Neutron Beam Lens',310,77,1,250,2,false,true,4,303);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2154,506,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2155,506,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2156,506,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2157,506,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2158,506,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2159,506,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2160,506,66,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2161,506,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2162,506,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2163,506,227,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (81,'Nitrate Explosive',93,76,1,14,2,false,true,0,304);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (375,81,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (376,81,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (377,81,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (378,81,29,4,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (349,'Nuclear Power Plant Technology',200,65,1,0,0,false,true,0,305);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1399,349,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1400,349,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (350,'Nuclear Power Plant Technology',200,125,1,1000,0,false,true,0,306);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1401,350,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1402,350,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (122,'Nuts',58,30,1,24,0,false,true,0,307);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (559,122,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (560,122,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (561,122,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (562,122,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (563,122,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (564,122,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (106,'Nuts',58,32,1,24,0,false,true,0,308);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (480,106,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (481,106,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (482,106,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (483,106,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (484,106,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (485,106,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (467,'Observatory Technology',286,125,1,1000,0,false,true,0,309);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1941,467,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1942,467,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (466,'Observatory Technology',286,127,1,0,0,false,true,0,310);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1939,466,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1940,466,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (473,'Officer Log Disk using Metal',290,0,20,4,0,false,true,0,311);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1959,473,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1960,473,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1961,473,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1962,473,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (474,'Officer Log Disk using Plastic',290,0,20,4,0,false,true,0,312);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1963,474,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1964,474,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1965,474,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1966,474,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (38,'Oil',23,51,1,24,0,false,true,0,313);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (203,38,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (204,38,3,1,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (351,'Orchard Technology',201,32,1,0,0,false,true,0,314);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1403,351,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1404,351,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (352,'Orchard Technology',201,125,1,1000,0,false,true,0,315);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1405,352,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1406,352,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (124,'Ore',59,50,1,15,0,false,true,0,316);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (565,124,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (566,124,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (567,124,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (568,124,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (569,124,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (402,'Oscilloscope',230,75,1,80,2,false,true,0,317);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1505,402,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1506,402,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1507,402,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1508,402,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1509,402,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1510,402,127,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (459,'Paper using Log',282,44,20,30,1,false,true,0,318);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1907,459,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1908,459,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1909,459,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1910,459,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (458,'Paper using Lumber',282,44,10,10,1,false,true,0,319);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1903,458,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1904,458,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1905,458,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1906,458,48,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (542,'ParcX Box using Paper',341,0,25,1,2,false,true,0,320);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2381,542,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2382,542,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2383,542,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2384,542,282,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (354,'Park Technology',202,125,1,1000,0,false,true,0,321);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1407,354,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1408,354,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (460,'Particle Collector Bay Module',283,110,1,150,2,false,true,23,322);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1911,460,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1912,460,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1913,460,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1914,460,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1915,460,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1916,460,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1917,460,269,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1918,460,270,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (126,'Pastry',60,40,15,7,2,false,true,0,323);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (570,126,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (571,126,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (572,126,26,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (573,126,34,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (574,126,35,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (575,126,52,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (576,126,58,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (577,126,74,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (578,126,78,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (579,126,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (422,'Personnel Transporter Module',246,110,1,100,2,false,true,6,324);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1656,422,27,30,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1657,422,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1658,422,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1659,422,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1660,422,228,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1661,422,230,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (91,'Petrochemicals',61,83,1,24,2,false,true,0,325);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (415,91,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (416,91,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (417,91,23,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (355,'Pharmaceutical Lab Technology',203,81,1,0,0,false,true,0,326);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1409,355,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1410,355,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (356,'Pharmaceutical Lab Technology',203,125,1,1000,0,false,true,0,327);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1411,356,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1412,356,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (128,'Pharmaceuticals using Herbs',62,81,1,8,1,false,true,0,328);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (583,128,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (584,128,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (585,128,42,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (586,128,128,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (129,'Pharmaceuticals using Minerals',62,81,1,9,1,false,true,0,329);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (587,129,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (588,129,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (589,129,53,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (590,129,128,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (130,'Pharmaceuticals using Petrochemicals',62,81,1,9,2,false,true,0,330);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (591,130,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (592,130,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (593,130,61,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (594,130,128,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (254,'Phlogiston',140,51,1,200,0,false,true,4,331);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1171,254,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1172,254,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1173,254,123,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (531,'Picture Frame 120x68cm using Plastic',331,82,6,10,2,false,true,0,332);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2330,531,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2331,531,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2332,531,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2333,531,124,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (530,'Picture Frame 22x28cm using Plastic',330,82,10,10,2,false,true,0,333);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2326,530,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2327,530,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2328,530,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2329,530,124,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (529,'Picture Frame 60x34cm using Plastic',329,82,8,10,2,false,true,0,334);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2322,529,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2323,529,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2324,529,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2325,529,124,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (40,'Pistol Ammunition',10,80,5,32,2,false,true,0,335);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (210,40,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (211,40,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (212,40,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (213,40,40,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (214,40,51,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (61,'Pistol using Metal',31,77,1,137,2,false,true,0,336);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (294,61,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (295,61,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (296,61,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (297,61,51,3,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (62,'Pistol using Metal and Plastic',31,77,1,142,2,false,true,0,337);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (298,62,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (299,62,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (300,62,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (301,62,51,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (302,62,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (59,'Pistol using Metal and Wood',31,77,1,144,2,false,true,0,338);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (284,59,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (285,59,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (286,59,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (287,59,51,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (288,59,86,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (74,'Plant Fiber',30,32,1,24,0,false,true,0,339);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (350,74,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (351,74,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (352,74,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (353,74,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (354,74,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (355,74,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (22,'Plant Fiber',30,30,1,24,0,false,true,0,340);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (129,22,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (130,22,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (131,22,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (132,22,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (133,22,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (134,22,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (110,'Plastic',63,82,5,33,2,true,true,0,341);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (496,110,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (497,110,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (498,110,61,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (36,'Plastic Body Armor',91,71,1,140,2,false,true,0,342);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (195,36,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (196,36,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (197,36,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (198,36,124,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (9,'Plastic Explosive',13,80,1,8,1,false,true,0,343);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (63,9,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (64,9,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (65,9,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (66,9,61,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (85,'Plastic Helmet',97,71,1,50,2,false,true,0,344);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (391,85,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (392,85,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (393,85,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (394,85,63,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (357,'Plastics Factory Technology',204,82,1,0,0,false,true,0,345);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1413,357,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1414,357,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (358,'Plastics Factory Technology',204,125,1,1000,0,false,true,0,346);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1415,358,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1416,358,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (246,'Pliers',135,79,2,9,2,false,true,0,347);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1134,246,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1135,246,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1136,246,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1137,246,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (245,'Pliers',135,82,2,14,2,false,true,0,348);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1130,245,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1131,245,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1132,245,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1133,245,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (359,'Police Station Technology',205,124,1,0,0,false,true,0,349);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1417,359,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1418,359,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (360,'Police Station Technology',205,125,1,1000,0,false,true,0,350);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1419,360,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1420,360,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (255,'Polytaride',144,51,1,200,0,false,true,15,351);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1174,255,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1175,255,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1176,255,123,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (415,'Power Plant Module',239,110,1,100,2,false,true,0,352);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1594,415,27,400,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1595,415,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1596,415,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1597,415,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1598,415,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1599,415,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1600,415,63,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1601,415,124,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (546,'Pressure Tent',344,73,1,500,2,false,true,0,353);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2390,546,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2391,546,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2392,546,27,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2393,546,51,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2395,546,79,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2396,546,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2397,546,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2398,546,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2399,546,127,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (433,'Prospector Bay Module',257,110,1,100,2,false,true,1,354);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1756,433,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1757,433,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1758,433,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1759,433,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1760,433,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1761,433,4,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (513,'Proton Beam Lens',317,77,1,250,2,false,true,23,355);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2224,513,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2225,513,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2226,513,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2227,513,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2228,513,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2229,513,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2230,513,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2231,513,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2232,513,265,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2233,513,271,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (429,'Pulse Laser Bay Module',253,0,1,100,2,false,true,3,356);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1730,429,27,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1731,429,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1732,429,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1733,429,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1734,429,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1735,429,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1736,429,227,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1737,429,229,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (135,'Radioactives',66,50,1,40,0,false,true,0,357);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (617,135,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (618,135,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (619,135,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (620,135,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (621,135,114,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (622,135,128,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (362,'Refinery Technology',206,125,1,1000,0,false,true,0,358);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1423,362,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1424,362,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (361,'Refinery Technology',206,83,1,0,0,false,true,0,359);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1421,361,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1422,361,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (364,'Retail Store Technology',207,125,1,1000,0,false,true,0,360);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1427,364,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1428,364,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (363,'Retail Store Technology',207,104,1,0,0,false,true,0,361);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1425,363,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1426,363,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (33,'Rifle Ammunition',75,80,6,14,2,false,true,0,362);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (181,33,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (182,33,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (183,33,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (184,33,40,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (185,33,51,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (63,'Rifle using Metal',32,77,1,216,2,false,true,0,363);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (303,63,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (304,63,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (305,63,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (306,63,51,4,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (65,'Rifle using Metal and Plastic',32,77,1,205,2,false,true,0,364);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (311,65,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (312,65,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (313,65,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (314,65,51,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (315,65,63,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (66,'Rifle using Metal and Wood',32,77,1,223,2,false,true,0,365);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (316,66,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (317,66,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (318,66,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (319,66,51,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (320,66,86,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (412,'Rocket Drive Module',236,110,1,100,2,false,true,0,366);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1566,412,50,600,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1567,412,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1568,412,51,800,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1569,412,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1570,412,64,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1571,412,123,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1572,412,79,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1573,412,126,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (366,'Rocket Motor Factory Technology',208,125,1,1000,0,false,true,0,367);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1431,366,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1432,366,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (365,'Rocket Motor Factory Technology',208,84,1,0,0,false,true,0,368);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1429,365,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1430,365,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (177,'Rum',121,42,1,2,2,false,true,0,369);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (867,177,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (868,177,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (869,177,74,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (870,177,78,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (871,177,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (144,'Sailboat using Metal',132,85,1,200,0,false,true,0,370);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (663,144,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (664,144,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (665,144,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (666,144,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (667,144,51,7,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (668,144,79,3,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (145,'Sailboat using Plastic',132,85,1,200,0,false,true,0,371);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (669,145,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (670,145,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (671,145,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (672,145,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (673,145,63,7,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (674,145,79,3,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (143,'Sailboat using Wood',132,85,1,200,1,true,true,0,372);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (657,143,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (658,143,3,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (659,143,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (660,143,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (661,143,48,7,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (662,143,79,3,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (213,'Screw Driver',125,82,5,14,2,false,true,0,373);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1026,213,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1027,213,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1028,213,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1029,213,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (214,'Screw Driver',125,79,5,9,2,false,true,0,374);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1030,214,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1031,214,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1032,214,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1033,214,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (521,'Seed',325,0,1,24,0,false,true,0,375);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2280,521,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2281,521,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2282,521,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2283,521,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2284,521,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2285,521,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (439,'Sensor Module',263,110,1,100,2,false,true,1,376);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1808,439,27,300,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1809,439,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1810,439,50,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1811,439,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1812,439,63,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1813,439,124,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (216,'Sewing Needle',126,79,6,9,2,false,true,0,377);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1038,216,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1039,216,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1040,216,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1041,216,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (215,'Sewing Needle',126,82,6,14,2,false,true,0,378);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1034,215,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1035,215,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1036,215,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1037,215,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (407,'Sewing Needle using Bone',126,72,2,11,1,false,true,0,379);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1536,407,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1537,407,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1538,407,116,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1539,407,233,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (221,'Sewing Needle using Log',126,72,6,23,1,false,true,0,380);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1058,221,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1059,221,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1060,221,116,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1061,221,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (198,'Ship Design Disk using Metal',113,0,20,4,0,false,true,0,381);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (958,198,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (959,198,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (960,198,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (961,198,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (199,'Ship Design Disk using Plastic',113,0,20,4,0,false,true,0,382);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (962,199,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (963,199,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (964,199,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (965,199,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (368,'Ship Yard Technology',210,125,1,1000,0,false,true,0,383);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1435,368,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1436,368,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (367,'Ship Yard Technology',210,85,1,0,0,false,true,0,384);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1433,367,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1434,367,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (206,'Shotgun Ammunition',118,80,8,6,2,false,true,0,385);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (993,206,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (994,206,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (995,206,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (996,206,40,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (997,206,51,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (202,'Shotgun using Metal',117,77,1,166,2,false,true,0,386);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (973,202,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (974,202,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (975,202,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (976,202,51,4,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (203,'Shotgun using Metal and Plastic',117,77,1,171,2,false,true,0,387);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (977,203,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (978,203,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (979,203,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (980,203,51,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (981,203,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (204,'Shotgun using Metal and Wood',117,77,1,180,2,false,true,0,388);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (982,204,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (983,204,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (984,204,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (985,204,51,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (986,204,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (223,'Shovel',114,82,2,14,2,false,true,0,389);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1066,223,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1067,223,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1068,223,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1069,223,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (224,'Shovel',114,79,2,9,2,false,true,0,390);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1070,224,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1071,224,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1072,224,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1073,224,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (222,'Shovel using Log',114,72,2,23,1,false,true,0,391);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1062,222,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1063,222,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1064,222,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1065,222,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (477,'Silage using Beans',295,30,4,5,2,false,true,0,392);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1979,477,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1980,477,15,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1981,477,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1982,477,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (478,'Silage using Fruit',295,30,2,5,2,false,true,0,393);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1983,478,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1984,478,35,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1985,478,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1986,478,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (479,'Silage using Grain',295,30,3,5,2,false,true,0,394);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1987,479,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1988,479,38,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1989,479,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1990,479,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (480,'Silage using Grapes',295,30,2,5,2,false,true,0,395);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1991,480,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1992,480,39,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1993,480,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1994,480,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (481,'Silage using Hay',295,30,2,5,2,false,true,0,396);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1995,481,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1996,481,130,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1997,481,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1998,481,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (482,'Silage using Herbs',295,30,2,5,2,false,true,0,397);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1999,482,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2000,482,42,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2001,482,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2002,482,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (483,'Silage using Hops',295,30,2,5,2,false,true,0,398);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2003,483,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2004,483,43,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2005,483,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2006,483,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (484,'Silage using Nuts',295,30,3,5,2,false,true,0,399);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2007,484,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2008,484,58,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2009,484,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2010,484,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (485,'Silage using Spices',295,30,2,5,2,false,true,0,400);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2011,485,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2012,485,74,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2013,485,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2014,485,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (486,'Silage using Vegetables',295,30,2,5,2,false,true,0,401);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2015,486,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2016,486,81,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2017,486,28,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2018,486,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (476,'Siphon Bay Module',289,110,1,100,2,false,true,1,402);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1973,476,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1974,476,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1975,476,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1976,476,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1977,476,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1978,476,4,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (524,'Small Flower Pot using Lumber',326,72,4,8,2,false,true,0,403);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2292,524,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2293,524,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2294,524,48,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (526,'Small Flower Pot using Metal',326,79,4,10,2,false,true,0,404);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2298,526,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2299,526,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2300,526,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (522,'Small Flower Pot using Plastic',326,82,5,6,2,false,true,0,405);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2286,522,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2287,522,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2288,522,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (495,'Small Missile BlBu Barnburner',299,80,1,613,2,false,true,0,406);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2062,495,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2063,495,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2064,495,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2065,495,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2066,495,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2067,495,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2068,495,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2069,495,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2070,495,13,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2071,495,36,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (117,'Small Missile Bl Zipper using Dynamite',55,80,1,563,2,false,true,0,407);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (521,117,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (522,117,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (523,117,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (524,117,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (525,117,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (526,117,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (527,117,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (528,117,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (529,117,25,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (119,'Small Missile Bl Zipper using Nitrate Explosive',55,80,1,563,2,false,true,0,408);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (539,119,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (540,119,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (541,119,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (542,119,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (543,119,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (544,119,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (545,119,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (546,119,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (547,119,93,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (118,'Small Missile Bl Zipper using Plastic Explosive',55,80,1,563,2,false,true,0,409);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (530,118,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (531,118,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (532,118,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (533,118,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (534,118,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (535,118,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (536,118,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (537,118,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (538,118,13,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (499,'Small Missile CrFr Snowball',303,80,1,613,2,false,true,3,410);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2096,499,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2097,499,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2098,499,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2099,499,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2100,499,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2101,499,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2102,499,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2103,499,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2104,499,13,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2105,499,142,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (493,'Small Rocket BlBu Firecracker',297,80,1,523,2,false,true,0,411);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2048,493,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2049,493,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2050,493,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2051,493,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2052,493,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2053,493,13,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2054,493,36,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (137,'Small Rocket Bl Partypopper using Dynamite',67,80,1,473,2,false,true,0,412);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (623,137,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (624,137,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (625,137,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (626,137,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (627,137,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (628,137,25,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (139,'Small Rocket Bl Partypopper using Nitrate Explosive',67,80,1,473,2,false,true,0,413);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (635,139,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (636,139,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (637,139,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (638,139,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (639,139,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (640,139,93,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (138,'Small Rocket Bl Partypopper using Plastic Explosive',67,80,1,473,2,false,true,0,414);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (629,138,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (630,138,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (631,138,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (632,138,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (633,138,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (634,138,13,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (141,'Small Rocket Motor using Gun Powder',69,84,1,130,2,false,true,0,415);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (647,141,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (648,141,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (649,141,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (650,141,40,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (651,141,51,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (178,'Small Rocket Motor using Nitrate Explosive',69,84,1,130,2,false,true,0,416);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (872,178,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (873,178,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (874,178,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (875,178,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (876,178,93,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (142,'Small Rocket Motor using Petrochemicals',69,84,1,130,2,false,true,0,417);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (652,142,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (653,142,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (654,142,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (655,142,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (656,142,61,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (497,'Small Rocket SlPi Meathook',301,80,1,523,2,false,true,0,418);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2082,497,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2083,497,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2084,497,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2085,497,69,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2086,497,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2087,497,13,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2088,497,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (369,'Smelter Technology',211,86,1,0,0,false,true,0,419);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1437,369,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1438,369,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (370,'Smelter Technology',211,125,1,1000,0,false,true,0,420);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1439,370,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1440,370,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (372,'Solar Power Plant Technology',212,125,1,1000,0,false,true,0,421);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1443,372,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1444,372,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (371,'Solar Power Plant Technology',212,66,1,0,0,false,true,0,422);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1441,371,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1442,371,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (212,'Soldering Iron',127,75,2,11,2,false,true,0,423);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1021,212,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1022,212,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1023,212,127,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1024,212,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1025,212,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (211,'Soldering Iron',127,79,2,22,2,false,true,0,424);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1016,211,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1017,211,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1018,211,123,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1019,211,51,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1020,211,57,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (373,'Spacecraft Factory Technology',213,87,1,0,0,false,true,0,425);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1445,373,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1446,373,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (374,'Spacecraft Factory Technology',213,125,1,1000,0,false,true,0,426);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1447,374,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1448,374,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (5,'Space Fighter F1',8,70,1,500,0,false,true,0,427);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (28,5,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (29,5,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (30,5,5,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (31,5,27,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (32,5,50,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (33,5,51,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (34,5,79,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (35,5,102,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (36,5,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (37,5,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (38,5,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (39,5,136,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (40,5,226,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (403,'Space Fighter F2',231,70,1,700,0,false,true,0,428);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1511,403,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1512,403,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1513,403,5,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1514,403,27,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1515,403,50,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1516,403,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1517,403,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1518,403,102,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1519,403,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1520,403,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1521,403,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1522,403,136,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1523,403,226,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (4,'Space Rocket',7,70,1,350,0,true,true,0,429);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (17,4,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (18,4,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (19,4,5,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (20,4,50,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (21,4,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (22,4,64,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (23,4,79,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (24,4,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (25,4,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (26,4,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (27,4,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (6,'Space Transport',9,70,1,450,0,false,true,0,430);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (41,6,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (42,6,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (43,6,5,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (44,6,27,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (45,6,50,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (46,6,51,15,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (47,6,63,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (48,6,79,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (49,6,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (50,6,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (51,6,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (52,6,136,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (53,6,226,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (132,'Spices',74,32,1,24,0,false,true,0,431);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (599,132,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (600,132,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (601,132,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (602,132,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (603,132,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (604,132,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (152,'Spices',74,30,1,24,0,false,true,0,432);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (735,152,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (736,152,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (737,152,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (738,152,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (739,152,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (740,152,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (505,'Stasis Beam Lens',309,77,1,250,2,false,true,15,433);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2144,505,2,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2145,505,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2146,505,27,8,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2147,505,37,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2148,505,51,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2149,505,63,6,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2150,505,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2151,505,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2152,505,144,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2153,505,264,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (26,'Stone',76,50,1,10,0,true,true,0,434);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (147,26,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (148,26,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (149,26,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (150,26,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (151,26,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (152,26,114,0,2);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (158,'Submarine',77,85,1,300,0,false,true,0,435);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (767,158,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (768,158,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (769,158,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (770,158,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (771,158,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (772,158,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (773,158,27,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (774,158,50,7,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (775,158,51,9,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (776,158,63,9,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (777,158,79,4,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (376,'Sugar Mill Technology',214,125,1,1000,0,false,true,0,436);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1451,376,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1452,376,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (375,'Sugar Mill Technology',214,46,1,0,0,false,true,0,437);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1449,375,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1450,375,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (160,'Sugar using Fruit',78,46,2,11,2,false,true,0,438);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (782,160,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (783,160,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (784,160,35,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (785,160,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (154,'Sugar using Grain',78,46,3,2,2,false,true,0,439);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (745,154,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (746,154,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (747,154,38,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (748,154,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (159,'Sugar using Grapes',78,46,2,11,2,false,true,0,440);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (778,159,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (779,159,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (780,159,39,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (781,159,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (153,'Sugar using Vegetables',78,46,2,8,2,true,true,0,441);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (741,153,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (742,153,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (743,153,81,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (744,153,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (419,'Surgery Medical Module',243,110,1,100,2,false,true,2,442);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1626,419,27,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1627,419,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1628,419,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1629,419,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1630,419,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1631,419,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1632,419,63,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1633,419,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1634,419,79,40,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1635,419,126,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (169,'SUV',82,88,1,250,0,false,true,0,443);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (827,169,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (828,169,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (829,169,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (830,169,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (831,169,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (832,169,50,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (833,169,51,5,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (834,169,63,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (835,169,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (836,169,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (146,'Tank',103,88,1,5000,0,false,true,0,444);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (675,146,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (676,146,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (677,146,125,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (678,146,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (679,146,129,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (680,146,27,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (681,146,50,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (682,146,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (683,146,102,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (684,146,136,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (544,'Tepee using Leather',343,0,1,1,1,false,true,0,445);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2386,544,47,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2387,544,86,3,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (545,'Tepee using Textiles',343,0,1,1,1,false,true,0,446);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2388,545,79,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2389,545,86,3,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (181,'Tequila',122,42,1,8,2,false,true,0,447);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (885,181,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (886,181,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (887,181,42,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (888,181,78,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (889,181,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (445,'Tesloid Dissipator',269,75,1,1000,2,false,true,23,448);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1843,445,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1844,445,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1845,445,24,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1846,445,27,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1847,445,51,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1848,445,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1849,445,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1850,445,135,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1851,445,141,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1852,445,270,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (380,'Textile Mill Technology',216,125,1,1000,0,false,true,0,449);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1459,380,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1460,380,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (379,'Textile Mill Technology',216,47,1,0,0,false,true,0,450);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1457,379,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1458,379,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (201,'Textiles using Leather',79,47,2,12,1,false,true,0,451);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (970,201,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (971,201,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (972,201,47,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (161,'Textiles using Plant Fiber',79,47,1,9,1,true,true,0,452);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (786,161,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (787,161,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (788,161,30,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (162,'Textiles using Plastic',79,47,3,13,2,false,true,0,453);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (789,162,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (790,162,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (791,162,63,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (217,'Tongs',128,82,1,14,2,false,true,0,454);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1042,217,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1043,217,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1044,217,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1045,217,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (218,'Tongs',128,79,1,9,2,false,true,0,455);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1046,218,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1047,218,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1048,218,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1049,218,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (167,'Torch',131,72,5,23,1,false,true,0,456);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (813,167,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (814,167,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (815,167,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (816,167,23,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (817,167,86,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (247,'Torque Wrench',136,79,1,24,2,false,true,0,457);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1138,247,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1139,247,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1140,247,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1141,247,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (434,'Tractor Beam Bay Module',258,110,1,100,2,false,true,7,458);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1762,434,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1763,434,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1764,434,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1765,434,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1766,434,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1767,434,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1768,434,227,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1769,434,229,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (383,'Treatment Plant Technology',217,125,1,1000,0,false,true,0,459);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1463,383,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1464,383,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (381,'Treatment Plant Technology',217,89,1,0,0,false,true,0,460);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1461,381,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1462,381,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (537,'Type A Preon Ring',336,78,2,1000,2,false,true,24,461);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2356,537,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2357,537,3,2,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2358,537,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2359,537,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2360,537,275,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (451,'Type A Preons',275,0,1,5000,0,false,true,31,462);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1872,451,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1873,451,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1874,451,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (536,'Type B Preon Ring',335,78,2,1000,2,false,true,24,463);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2351,536,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2352,536,3,2,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2353,536,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2354,536,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2355,536,274,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (450,'Type B Preons',274,0,1,4000,0,false,true,31,464);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1869,450,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1870,450,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1871,450,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (538,'Type F Preon Ring',337,78,2,1000,2,false,true,24,465);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2361,538,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2362,538,3,2,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2363,538,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2364,538,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2365,538,276,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (452,'Type F Preons',276,0,1,6000,0,false,true,31,466);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1875,452,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1876,452,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1877,452,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (539,'Type G Preon Ring',338,78,2,1000,2,false,true,24,467);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2366,539,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2367,539,3,2,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2368,539,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2369,539,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2370,539,277,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (453,'Type G Preons',277,0,1,7000,0,false,true,31,468);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1878,453,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1879,453,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1880,453,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (540,'Type K Preon Ring',339,78,2,1000,2,false,true,24,469);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2371,540,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2372,540,3,2,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2373,540,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2374,540,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2375,540,278,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (454,'Type K Preons',278,0,1,8000,0,false,true,31,470);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1881,454,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1882,454,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1883,454,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (541,'Type M Preon Ring',340,78,2,1000,2,false,true,24,471);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2376,541,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2377,541,3,2,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2378,541,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2379,541,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2380,541,279,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (455,'Type M Preons',279,0,1,9000,0,false,true,31,472);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1884,455,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1885,455,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1886,455,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (535,'Type O Preon Ring',334,78,2,1000,2,false,true,24,473);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2346,535,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2347,535,3,2,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2348,535,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2349,535,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2350,535,273,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (449,'Type O Preons',273,0,1,3000,0,false,true,31,474);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1866,449,2,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1867,449,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1868,449,87,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (264,'University Technology',209,125,1,0,0,true,true,0,475);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1235,264,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1236,264,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (87,'Utility Pouch using Leather',293,73,1,3,1,false,true,0,476);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (401,87,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (402,87,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (403,87,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (404,87,47,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (88,'Utility Pouch using Textiles',293,73,1,3,1,false,true,0,477);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (405,88,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (406,88,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (407,88,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (408,88,79,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (11,'Vegetables',81,30,1,24,0,true,true,0,478);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (72,11,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (73,11,28,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (74,11,29,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (75,11,88,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (76,11,94,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (77,11,114,0,6);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (384,'Vehicle Factory Technology',218,88,1,0,0,false,true,0,479);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1465,384,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1466,384,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (385,'Vehicle Factory Technology',218,125,1,1000,0,false,true,0,480);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1467,385,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1468,385,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (423,'Vehicle Transporter Module',247,110,1,100,2,false,true,13,481);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1662,423,27,60,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1663,423,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1664,423,63,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1665,423,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1666,423,228,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1667,423,230,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1668,423,264,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1669,423,229,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1770,423,228,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1771,423,230,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (60,'Vodka',101,42,1,2,2,false,true,0,482);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (289,60,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (290,60,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (291,60,38,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (292,60,78,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (293,60,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (248,'Vulcanite',137,50,1,535,0,false,true,18,483);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1142,248,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1143,248,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1144,248,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1145,248,25,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1146,248,54,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1147,248,128,0,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (256,'Vulcium',145,86,1,501,2,false,true,18,484);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1177,256,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1178,256,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1179,256,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1180,256,137,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (465,'Vulcium Bullion',157,79,2500,499,2,false,true,0,485);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1935,465,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1936,465,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1937,465,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1938,465,145,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (441,'Vulcium Heat Sink',265,79,1,100,2,false,true,18,486);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1819,441,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1820,441,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1821,441,145,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1822,441,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1823,441,142,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (457,'Vulcium Hull Module',281,110,1,1000,2,false,true,0,487);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1895,457,27,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1896,457,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1897,457,50,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1898,457,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1899,457,145,5000,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1900,457,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1901,457,63,500,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1902,457,124,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (259,'Vulcium Knife using Vulcium',152,77,1,200,2,false,true,18,488);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1190,259,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1191,259,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1192,259,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1193,259,145,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (260,'Vulcium Knife using Vulcium and Plastic',152,77,1,200,2,false,true,18,489);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1194,260,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1195,260,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1196,260,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1197,260,63,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1198,260,145,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (258,'Vulcium Knife using Vulcium and Wood',152,77,1,200,2,false,true,18,490);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1185,258,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1186,258,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1187,258,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1188,258,86,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1189,258,145,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (263,'Vulcium Power Armor Helmet',156,71,1,450,2,false,true,18,491);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1223,263,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1224,263,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1225,263,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1226,263,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1227,263,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1228,263,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1229,263,27,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1230,263,50,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1231,263,63,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1232,263,79,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1233,263,145,4,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1234,263,265,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (262,'Vulcium Power Armor Suit',155,71,1,1520,2,false,true,18,492);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1211,262,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1212,262,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1213,262,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1214,262,123,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1215,262,124,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1216,262,127,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1217,262,27,16,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1218,262,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1219,262,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1220,262,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1221,262,145,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1222,262,265,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (386,'Warehouse Technology',219,4,1,0,0,false,true,0,493);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1469,386,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1470,386,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (387,'Warehouse Technology',219,125,1,1000,0,false,true,0,494);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1471,387,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1472,387,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (436,'Warp Drive FTL Module',260,110,1,100,2,false,true,23,495);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1780,436,27,600,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1781,436,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1782,436,51,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1783,436,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1784,436,63,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1785,436,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1786,436,271,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1787,436,128,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (32,'Water',83,51,1,24,0,true,true,0,496);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (178,32,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (179,32,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (180,32,88,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (58,'Water using Air and Hydrogen',83,83,1,24,2,false,false,0,497);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (280,58,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (281,58,3,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (282,58,5,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (283,58,44,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (170,'Water using Environment',83,89,1,12,0,true,false,0,498);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (837,170,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (838,170,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (839,170,88,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (171,'Water using Ice',83,89,1,12,1,false,false,0,499);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (840,171,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (841,171,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (842,171,45,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (515,'Weapon Deployment Bay Module',319,110,1,100,2,false,true,1,500);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2244,515,27,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2245,515,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2246,515,50,300,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2247,515,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2248,515,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2249,515,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (2250,515,136,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (430,'Weapon Launcher Bay Module',254,110,1,100,2,false,true,1,501);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1738,430,27,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1739,430,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1740,430,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1741,430,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1742,430,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1743,430,4,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (426,'Weapon Launcher Turret Module',250,110,1,100,2,false,true,0,502);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1698,426,27,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1699,426,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1700,426,50,20,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1701,426,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1702,426,79,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1703,426,126,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1704,426,51,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1705,426,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1706,426,63,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1707,426,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1708,426,102,10,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1709,426,136,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (389,'Weapon Smith Technology',220,125,1,1000,0,false,true,0,503);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1475,389,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1476,389,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (388,'Weapon Smith Technology',220,77,1,0,0,false,true,0,504);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1473,388,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1474,388,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (390,'Well Technology',221,51,1,0,0,false,true,0,505);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1477,390,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1478,390,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (391,'Well Technology',221,125,1,1000,0,false,true,0,506);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1479,391,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1480,391,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (393,'Wharf Technology',222,125,1,1000,0,false,true,0,507);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1483,393,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1484,393,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (392,'Wharf Technology',222,34,1,0,0,false,true,0,508);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1481,392,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1482,392,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (34,'Whiskey',84,42,1,6,2,true,true,0,509);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (186,34,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (187,34,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (188,34,38,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (189,34,78,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (190,34,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (475,'Wildcatter Bay Module',291,110,1,100,2,false,true,2,510);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1967,475,27,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1968,475,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1969,475,50,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1970,475,129,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1971,475,51,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1972,475,4,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (394,'Wind Power Plant Technology',223,67,1,0,0,false,true,0,511);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1485,394,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1486,394,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (395,'Wind Power Plant Technology',223,125,1,1000,0,false,true,0,512);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1487,395,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1488,395,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (397,'Winery Technology',224,125,1,1000,0,false,true,0,513);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1491,397,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1492,397,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (396,'Winery Technology',224,48,1,0,0,false,true,0,514);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1489,396,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1490,396,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (174,'Wine using Fruit',85,48,1,3,2,false,true,0,515);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (853,174,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (854,174,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (855,174,35,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (856,174,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (173,'Wine using Grapes',85,48,1,3,2,true,true,0,516);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (849,173,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (850,173,3,1,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (851,173,39,2,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (852,173,83,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (435,'Wormhole Drive FTL Module',259,110,1,100,2,false,true,0,517);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1772,435,27,600,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1773,435,127,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1774,435,51,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1775,435,4,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1776,435,63,200,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1777,435,124,1,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1778,435,111,100,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1779,435,128,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (37,'Woven Body Armor',92,71,1,40,2,false,true,0,518);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (199,37,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (200,37,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (201,37,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (202,37,79,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (83,'Woven Helmet',41,71,1,30,2,false,true,0,519);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (383,83,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (384,83,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (385,83,126,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (386,83,79,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (219,'Wrench',129,82,4,14,2,false,true,0,520);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1050,219,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1051,219,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1052,219,124,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1053,219,63,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (220,'Wrench',129,79,4,9,2,false,true,0,521);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1054,220,2,0,6);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1055,220,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1056,220,4,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1057,220,51,1,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (399,'Zoo Technology',225,125,1,1000,0,false,true,0,522);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1495,399,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1496,399,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (398,'Zoo Technology',225,113,1,0,0,false,true,0,523);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1493,398,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (1494,398,157,10,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (64,'Кала́шникова using Metal',292,77,1,216,2,false,true,0,524);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (307,64,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (308,64,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (309,64,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (310,64,51,4,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (71,'Кала́шникова using Metal and Plastic',292,77,1,205,2,false,true,0,525);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (337,71,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (338,71,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (339,71,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (340,71,51,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (341,71,63,2,5);

INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft,isdefault,nat,techLevel,sourceOrder) VALUES (75,'Кала́шникова using Metal and Wood',292,77,1,223,2,false,true,0,526);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (356,75,2,0,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (357,75,3,1,2);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (358,75,4,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (359,75,51,3,5);
INSERT INTO b_ManufacturingComponent_Legacy (id,idmp,idc,consumed,ide) VALUES (360,75,86,2,5);

-- Explicit recovered IDs must not leave the serial sequences at their defaults. This MUST happen before any new additions!
SELECT setval(pg_get_serial_sequence('b_manufacturingcomponent_legacy', 'id'),
              (SELECT MAX(id) FROM b_ManufacturingComponent_Legacy));
SELECT setval(pg_get_serial_sequence('b_manufacturingprocess_legacy', 'sourceorder'),
              (SELECT MAX(sourceOrder) FROM b_ManufacturingProcess_Legacy));

-- Oni's new handcraft weapons --
INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft) VALUES (555,'Bow',350,77,1,40,1);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (555,116,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (555,48,1,5); -- Lumber
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (555,79,1,5); -- Textiles
INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft) VALUES (556,'Arrows',351,77,10,15,1);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (556,116,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (556,48,1,5); -- Lumber
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (556,76,1,5); -- Stone
INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft) VALUES (557,'Spear',352,77,1,20,1);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (557,116,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (557,86,1,5); -- Log
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (557,76,1,5); -- Stone
INSERT INTO b_ManufacturingProcess_Legacy (id,name,idc,idi,qty,labor,craft) VALUES (558,'Javelin',353,77,1,15,1);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (558,116,0,5);
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (558,48,1,5); -- Lumber
INSERT INTO b_ManufacturingComponent_Legacy (idmp,idc,consumed,ide) VALUES (558,76,1,5); -- Stone

-- Legacy construction processes and components.
-- Complete GD2016 sections: 82 processes and 476 components, with historical IDs.
-- Layout verified against DbConstructionProcess::Rx (AuData13 0x114d20),
-- DbConstructionComponent::Rx (0x1146e0), and their original SQL Load methods.
-- Construction rules and material/environment adjustments remain in C++.
CREATE TABLE b_ConstructionProcess_Legacy
(
  id               integer primary key unique,
  name             text,
  idi              smallint,                     -- Historical industry ID.
  sitePrep         smallint DEFAULT 0,            -- Original site-preparation value.
  stackLimit       smallint DEFAULT 0,            -- Packed flags; not a literal level count.
  idt              smallint DEFAULT 0,            -- Original terrain ID.
  buildRadius      smallint DEFAULT 0,            -- Original build-radius value, unscaled.
  sourceOrder      serial unique                 -- Preserve first-process-per-industry selection.
);
GRANT ALL ON b_ConstructionProcess_Legacy TO audog;
GRANT ALL ON b_ConstructionProcess_Legacy_sourceOrder_seq TO audog;

CREATE TABLE b_ConstructionComponent_Legacy
(
  id               serial primary key unique,
  idcp             integer,                      -- Historical construction process ID.
  idc              integer,                      -- Historical commodity ID.
  consumed         smallint DEFAULT 0,            -- Original quantity; zero can denote a reusable tool.
  ide              smallint,                     -- Original component effect (CE_ value).
  lvl              smallint DEFAULT 0             -- Original level byte, retained even where unused.
);
GRANT ALL ON b_ConstructionComponent_Legacy TO audog;
GRANT ALL ON b_ConstructionComponent_Legacy_id_seq TO audog;

-- Original section SHA-256: 4d8f0ad41fe2f2ebb3a51a3ce8a4ea2f0b44693eb67193691882bbc635ef106c
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (1,'Town Square',1,16,0,1,15,1);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (2,'Dirt',2,0,0,2,5,2);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (3,'Asphalt',2,15,0,2,8,3);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (4,'Concrete',2,15,0,2,8,4);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (5,'Graded Area',2,0,0,2,5,5);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (6,'Park',114,0,0,14,23,6);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (7,'Pressurized',2,15,0,2,10,7);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (8,'House',20,3,3,2,13,8);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (9,'Condominium',21,15,31,2,20,9);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (10,'Apartment',22,15,31,2,20,10);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (11,'Farm',30,3,1,2,50,11);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (12,'Logging Camp',31,1,34,7,50,12);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (13,'Wharf',34,7,0,4,20,13);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (14,'Bakery',40,15,7,2,20,14);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (15,'Brewery',41,15,7,2,20,15);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (16,'Distillery',42,15,7,2,20,16);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (17,'Grist Mill',43,15,7,2,20,17);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (18,'Lumber Mill',44,15,7,2,20,18);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (19,'Meat Processing Plant',45,15,7,2,20,19);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (20,'Sugar Mill',46,15,7,2,20,20);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (21,'Textile Mill',47,15,7,2,20,21);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (22,'Winery',48,15,7,2,20,22);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (23,'Mine',50,38,32,8,25,23);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (24,'Well',51,37,1,2,25,24);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (25,'Coal Power Plant',60,15,3,2,30,25);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (26,'Gasoline Power Plant',61,15,3,2,30,26);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (27,'Hydroelectric Power Plant',62,39,3,4,30,27);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (28,'Hydrogen Power Plant',63,15,3,2,30,28);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (29,'Natural Gas Power Plant',64,15,3,2,30,29);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (30,'Nuclear Power Plant',65,15,3,2,30,30);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (31,'Solar Power Plant',66,5,3,2,30,31);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (32,'Wind Power Plant',67,5,3,2,30,32);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (33,'Aircraft Factory',70,15,0,3,24,33);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (34,'Armorer',71,15,7,2,20,34);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (35,'Carpenter',72,15,7,2,20,35);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (36,'Clothing Factory',73,15,7,2,20,36);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (37,'Computer Factory',74,15,7,2,20,37);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (38,'Electronics Factory',75,15,7,2,20,38);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (39,'Explosives Factory',76,15,7,2,20,39);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (40,'Weapon Smith',77,15,7,2,20,40);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (41,'Jeweler',78,15,7,2,20,41);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (42,'Machine Shop',79,15,7,2,20,42);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (43,'Munitions Factory',80,15,7,2,20,43);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (44,'Pharmaceutical Lab',81,15,7,2,20,44);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (45,'Plastics Factory',82,15,7,2,20,45);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (46,'Refinery',83,15,7,2,20,46);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (47,'Rocket Motor Factory',84,15,7,2,20,47);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (48,'Ship Yard',85,7,0,5,24,48);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (49,'Smelter',86,15,7,2,20,49);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (50,'Spacecraft Factory',87,15,0,3,24,50);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (51,'Vehicle Factory',88,15,0,3,24,51);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (52,'Treatment Plant',89,7,3,11,20,52);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (53,'Bank',100,15,31,2,20,53);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (54,'Broker',101,15,31,2,20,54);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (55,'Casino',102,15,31,2,20,55);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (56,'Design Studio',3,15,15,2,20,56);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (57,'Grocery',103,15,31,2,20,57);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (58,'Retail Store',104,15,31,2,20,58);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (59,'Cantina',105,15,31,2,20,59);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (60,'Airport Repair Shop',110,15,1,3,20,60);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (61,'Airport Terminal',111,15,0,3,20,61);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (62,'Arena',112,7,0,2,27,62);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (63,'Zoo',113,3,0,13,40,63);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (64,'Church',120,15,31,2,20,64);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (65,'Hospital',121,15,31,2,20,65);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (66,'Fire Station',122,15,3,2,20,66);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (67,'Capitol',123,15,31,2,20,67);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (68,'Police Station',124,15,3,2,20,68);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (69,'University',125,15,7,2,20,69);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (70,'Military Flag',10,12,0,2,10,70);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (71,'Military Weapon System',11,15,0,2,35,71);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (72,'Military Shield Generator',12,15,0,2,35,72);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (73,'Orchard',32,7,1,2,50,73);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (74,'Irrigated Area',2,0,0,16,5,74);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (75,'Warehouse',4,7,0,2,22,75);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (76,'Hangar',126,15,0,3,23,76);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (77,'Observatory',127,15,0,2,20,77);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (78,'Military Guard Tower',128,15,0,11,8,78);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (79,'Biodome',129,0,0,15,0,79);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (80,'Defoliated Area',2,0,0,16,5,80);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (81,'Cleared Area',2,0,0,16,5,81);
INSERT INTO b_ConstructionProcess_Legacy (id,name,idi,sitePrep,stackLimit,idt,buildRadius,sourceOrder) VALUES (82,'Dairy',35,15,7,2,20,82);

-- Original section SHA-256: 1589822ac9f3d86287f97c7c701c8551a34773e1bedcaac2a816f729bb72f280
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (1,1,1,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (2,1,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (3,1,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (4,1,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (5,2,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (6,2,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (7,2,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (8,2,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (9,3,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (10,3,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (11,3,23,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (12,3,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (13,3,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (14,4,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (15,4,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (16,4,76,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (17,4,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (18,4,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (19,5,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (20,5,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (21,5,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (22,5,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (23,6,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (24,6,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (25,6,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (26,6,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (27,7,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (28,7,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (29,7,27,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (30,7,51,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (31,7,63,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (32,7,76,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (33,7,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (34,7,114,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (35,7,124,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (36,7,127,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (37,8,48,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (38,8,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (39,8,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (40,8,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (41,8,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (42,9,48,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (43,9,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (44,9,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (45,9,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (46,9,79,4,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (47,9,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (48,10,48,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (49,10,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (50,10,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (51,10,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (52,10,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (53,11,48,30,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (54,11,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (55,11,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (56,11,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (57,11,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (58,12,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (59,12,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (60,12,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (61,12,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (62,13,48,85,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (63,13,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (64,13,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (65,13,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (66,13,70,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (67,13,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (68,14,48,25,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (69,14,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (70,14,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (71,14,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (72,14,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (73,15,48,50,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (74,15,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (75,15,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (76,15,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (77,15,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (78,16,48,50,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (79,16,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (80,16,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (81,16,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (82,16,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (83,17,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (84,17,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (85,17,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (86,17,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (87,17,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (88,18,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (89,18,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (90,18,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (91,18,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (92,18,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (93,19,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (94,19,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (95,19,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (96,19,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (97,19,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (98,20,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (99,20,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (100,20,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (101,20,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (102,20,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (103,21,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (104,21,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (105,21,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (106,21,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (107,21,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (108,22,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (109,22,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (110,22,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (111,22,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (112,22,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (113,23,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (114,23,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (115,23,25,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (116,23,54,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (117,23,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (118,23,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (119,24,48,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (120,24,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (121,24,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (122,24,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (123,24,114,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (124,25,48,125,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (125,25,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (126,25,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (127,25,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (128,25,50,18,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (129,25,51,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (130,25,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (131,25,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (132,26,48,120,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (133,26,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (134,26,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (135,26,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (136,26,50,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (137,26,51,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (138,26,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (139,26,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (140,27,48,140,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (141,27,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (142,27,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (143,27,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (144,27,50,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (145,27,51,40,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (146,27,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (147,27,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (148,28,48,135,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (149,28,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (150,28,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (151,28,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (152,28,50,15,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (153,28,51,45,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (154,28,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (155,28,123,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (156,29,48,115,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (157,29,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (158,29,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (159,29,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (160,29,50,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (161,29,51,35,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (162,29,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (163,29,123,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (164,30,48,150,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (165,30,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (166,30,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (167,30,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (168,30,27,2,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (169,30,50,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (170,30,51,54,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (171,30,63,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (172,30,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (173,30,123,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (174,30,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (175,31,48,90,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (176,31,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (177,31,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (178,31,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (179,31,27,2,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (180,31,51,16,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (181,31,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (182,31,127,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (183,32,48,75,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (184,32,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (185,32,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (186,32,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (187,32,50,15,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (188,32,51,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (189,32,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (190,32,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (191,33,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (192,33,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (193,33,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (194,33,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (195,33,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (196,34,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (197,34,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (198,34,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (199,34,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (200,34,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (201,35,48,25,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (202,35,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (203,35,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (204,35,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (205,35,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (206,36,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (207,36,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (208,36,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (209,36,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (210,36,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (211,37,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (212,37,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (213,37,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (214,37,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (215,37,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (216,38,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (217,38,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (218,38,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (219,38,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (220,38,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (221,39,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (222,39,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (223,39,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (224,39,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (225,39,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (226,40,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (227,40,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (228,40,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (229,40,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (230,40,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (231,41,48,15,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (232,41,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (233,41,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (234,41,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (235,41,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (236,42,48,100,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (237,42,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (238,42,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (239,42,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (240,42,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (241,43,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (242,43,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (243,43,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (244,43,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (245,43,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (246,44,48,60,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (247,44,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (248,44,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (249,44,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (250,44,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (251,45,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (252,45,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (253,45,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (254,45,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (255,45,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (256,46,48,85,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (257,46,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (258,46,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (259,46,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (260,46,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (261,47,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (262,47,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (263,47,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (264,47,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (265,47,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (266,48,48,180,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (267,48,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (268,48,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (269,48,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (270,48,70,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (271,48,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (272,49,48,90,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (273,49,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (274,49,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (275,49,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (276,49,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (277,50,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (278,50,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (279,50,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (280,50,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (281,50,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (282,51,48,130,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (283,51,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (284,51,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (285,51,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (286,51,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (287,52,48,50,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (288,52,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (289,52,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (290,52,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (291,52,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (292,53,48,30,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (293,53,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (294,53,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (295,53,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (296,53,51,3,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (297,53,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (298,54,48,40,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (299,54,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (300,54,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (301,54,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (302,54,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (303,55,48,40,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (304,55,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (305,55,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (306,55,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (307,55,27,4,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (308,55,50,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (309,55,51,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (310,55,63,40,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (311,55,79,50,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (312,55,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (313,55,99,2,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (314,55,127,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (315,56,48,30,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (316,56,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (317,56,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (318,56,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (319,56,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (320,57,48,15,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (321,57,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (322,57,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (323,57,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (324,57,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (325,58,48,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (326,58,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (327,58,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (328,58,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (329,58,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (330,59,48,15,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (331,59,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (332,59,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (333,59,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (334,59,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (335,60,48,35,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (336,60,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (337,60,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (338,60,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (339,60,27,4,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (340,60,50,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (341,60,51,15,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (342,60,63,4,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (343,60,79,2,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (344,60,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (345,60,127,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (346,60,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (347,61,48,75,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (348,61,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (349,61,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (350,61,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (351,61,27,6,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (352,61,50,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (353,61,51,25,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (354,61,63,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (355,61,79,24,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (356,61,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (357,61,127,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (358,62,48,50,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (359,62,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (360,62,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (361,62,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (362,62,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (363,63,48,50,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (364,63,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (365,63,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (366,63,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (367,63,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (368,64,48,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (369,64,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (370,64,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (371,64,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (372,64,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (373,65,48,40,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (374,65,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (375,65,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (376,65,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (377,65,79,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (378,65,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (379,66,48,25,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (380,66,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (381,66,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (382,66,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (383,66,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (384,67,48,40,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (385,67,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (386,67,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (387,67,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (388,67,51,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (389,67,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (390,68,48,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (391,68,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (392,68,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (393,68,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (394,68,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (395,69,48,30,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (396,69,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (397,69,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (398,69,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (399,69,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (400,70,1,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (401,70,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (402,70,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (403,70,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (404,71,48,220,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (405,71,2,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (406,71,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (407,71,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (408,71,50,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (409,71,51,45,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (410,71,79,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (411,71,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (412,71,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (413,72,48,200,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (414,72,2,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (415,72,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (416,72,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (417,72,27,10,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (418,72,50,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (419,72,51,40,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (420,72,79,12,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (421,72,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (422,72,129,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (423,73,48,30,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (424,73,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (425,73,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (426,73,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (427,73,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (428,74,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (429,74,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (430,74,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (431,74,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (432,75,48,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (433,75,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (434,75,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (435,75,4,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (436,75,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (437,76,48,100,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (438,76,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (439,76,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (440,76,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (441,76,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (442,77,48,35,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (443,77,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (444,77,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (445,77,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (446,77,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (447,78,48,100,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (448,78,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (449,78,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (450,78,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (451,78,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (452,78,102,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (453,79,48,20,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (454,79,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (455,79,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (456,79,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (457,79,27,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (458,79,51,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (459,79,63,1,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (460,79,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (461,79,114,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (462,79,124,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (463,79,127,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (464,80,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (465,80,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (466,80,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (467,80,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (468,81,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (469,81,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (470,81,82,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (471,81,114,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (472,82,48,25,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (473,82,2,0,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (474,82,3,1,2,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (475,82,4,0,5,0);
INSERT INTO b_ConstructionComponent_Legacy (id,idcp,idc,consumed,ide,lvl) VALUES (476,82,82,0,2,0);

-- Align sequences before any future construction additions.
SELECT setval(pg_get_serial_sequence('b_constructioncomponent_legacy', 'id'),
              (SELECT MAX(id) FROM b_ConstructionComponent_Legacy));
SELECT setval(pg_get_serial_sequence('b_constructionprocess_legacy', 'sourceorder'),
              (SELECT MAX(sourceOrder) FROM b_ConstructionProcess_Legacy));

COMMIT;
