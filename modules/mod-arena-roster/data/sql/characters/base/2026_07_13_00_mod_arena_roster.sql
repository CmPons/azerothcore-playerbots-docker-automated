-- Partner pool: one persistent addclass-pool character per class, pinned to a player.
CREATE TABLE IF NOT EXISTS `mod_arena_roster` (
  `owner_guid` INT UNSIGNED NOT NULL,
  `bot_guid`   INT UNSIGNED NOT NULL,
  `class`      TINYINT UNSIGNED NOT NULL,
  `spec_tab`   TINYINT UNSIGNED NOT NULL,   -- 0-based talent tab forced by sync (may be overridden)
  PRIMARY KEY (`owner_guid`, `class`),
  UNIQUE KEY `uk_ar_bot` (`bot_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Opponent ladder: pool bot pins and team membership matrix.
CREATE TABLE IF NOT EXISTS `mod_arena_pool` (
  `tier`       TINYINT UNSIGNED NOT NULL,   -- 1..4
  `roster_idx` TINYINT UNSIGNED NOT NULL,   -- 0..14 within the tier
  `bot_guid`   INT UNSIGNED NOT NULL,
  `class`      TINYINT UNSIGNED NOT NULL,
  `spec_tab`   TINYINT UNSIGNED NOT NULL,
  `team2`      INT UNSIGNED NOT NULL DEFAULT 0,  -- arena_team id per bracket (0 = none)
  `team3`      INT UNSIGNED NOT NULL DEFAULT 0,
  `team5`      INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`tier`, `roster_idx`),
  UNIQUE KEY `uk_ap_bot` (`bot_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
