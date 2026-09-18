# Dunkelgrünes 2K-Gras: Map-Zuordnung

85 vorhandene Maps geprüft; 130 Grass-Layer in 50 Maps im isolierten Testclient hinterlegt. Davon haben 49 Maps tatsächlich verwendete passende Layer. Eine weitere Map referenziert die Layer nur im TextureSet.

Alle Dateien enthalten dieselbe freigegebene 2048²-DDS mit zwölf Mipstufen. Bisherige UV-Skalierung und Masken bleiben bestehen. Schnee, Seegras, eingebettete Felsen und dunkle Dungeon-Erde bleiben unverändert. Classic verwendet die bisherigen Materialien. Ein laufender Client muss für bereits geladene Maps neu gestartet werden.

Prüfung: kurzer nativer Lauf mit neun Ansichten auf sieben repräsentativen Maps bestanden; keine Grass- oder Shutdown-Ressourcen, keine Diligent ERROR/FATAL. 12189 Originaldateien hashgleich. Nicht jede Map wurde visuell im Spiel geprüft.

| Map | Slots | Benutzte Maskeneinträge |
|---|---|---|
| gm_guild_build | 5, 6, 7 | 16340 |
| map_a2 | 3, 4 | 573803 |
| map_b_fielddungeon | 5, 6, 7 | 0 |
| metin2_guild_village | 2 | 83609 |
| metin2_map_a1 | 5, 6, 7 | 488652 |
| metin2_map_a3 | 5, 6, 7 | 396871 |
| metin2_map_b1 | 5, 6, 7 | 469100 |
| metin2_map_b3 | 5, 6, 7 | 672339 |
| metin2_map_bayblacksand | 2, 6, 9 | 257746 |
| metin2_map_c1 | 5, 6, 7 | 395902 |
| metin2_map_c3 | 5, 6, 7 | 447764 |
| metin2_map_capedragonhead | 1, 2, 11 | 554480 |
| metin2_map_dawnmistwood | 2, 5, 12, 13 | 616885 |
| metin2_map_duel | 3 | 26249 |
| metin2_map_guild_01 | 3, 4 | 49095 |
| metin2_map_guild_02 | 5, 6, 7 | 31639 |
| metin2_map_guild_03 | 5, 6, 7 | 51912 |
| metin2_map_milgyo | 1, 2 | 116014 |
| metin2_map_mt_thunder | 5, 6 | 167164 |
| metin2_map_n_desert_01 | 10, 11, 12 | 40168 |
| metin2_map_t1 | 5, 6, 7 | 73127 |
| metin2_map_t2 | 5, 6, 7 | 9069 |
| metin2_map_trent | 4, 5, 6 | 41739 |
| metin2_map_trent02 | 4, 5, 6 | 328769 |
| metin2_map_wedding_01 | 5, 6, 7 | 22582 |
| season1/metin2_map_empirewar_a01 | 4, 5 | 49363 |
| season1/metin2_map_ew02 | 5, 6, 7 | 131228 |
| season1/metin2_map_nusluck01 | 6, 7, 9 | 54996 |
| season1/metin2_map_oxevent | 6, 7, 9 | 98162 |
| season1/metin2_map_shinsutest_01 | 6, 7 | 157623 |
| season1/metin2_map_siege_01 | 6, 7 | 158620 |
| season1/metin2_map_siege_02 | 6, 7 | 158620 |
| season1/metin2_map_siege_03 | 6, 7 | 158620 |
| season1/metin2_map_sungzi | 5, 6, 7 | 121457 |
| season1/metin2_map_sungzi_desert_01 | 10, 11, 12 | 3911 |
| season1/metin2_map_sungzi_desert_hill_01 | 10, 11, 12 | 6932 |
| season1/metin2_map_sungzi_desert_hill_02 | 10, 11, 12 | 6932 |
| season1/metin2_map_sungzi_desert_hill_03 | 10, 11, 12 | 6932 |
| season1/metin2_map_sungzi_milgyo | 1, 2 | 18239 |
| season1/metin2_map_sungzi_milgyo_pass_01 | 1, 2 | 19316 |
| season1/metin2_map_sungzi_milgyo_pass_02 | 1, 2 | 19316 |
| season1/metin2_map_sungzi_milgyo_pass_03 | 1, 2 | 19316 |
| season1/metin2_map_wl_01 | 6, 7, 9 | 203902 |
| season2/metin2_map_a2_1 | 3, 4 | 576573 |
| season2/metin2_map_empirewar02 | 4, 5 | 24452 |
| season2/metin2_map_empirewar03 | 5, 6 | 2565 |
| season2/metin2_map_milgyo_a | 1, 2 | 116014 |
| season2/metin2_map_n_desert_02 | 10, 11, 12 | 40168 |
| season2/metin2_map_trent02_a | 4, 5, 6 | 310096 |
| season2/metin2_map_trent_a | 4, 5, 6 | 41739 |

## Ausnahmen

- `d:/ymir work/terrainmaps/b/grass/grass 03_05.dds`: rock-bearing grass: preserve embedded rock
- `d:/ymir work/terrainmaps/bayblacksand/bayblacksand_seagrass.dds`: underwater seagrass
- `d:/ymir work/terrainmaps/empirewar/snow/grass01.dds`: snow-covered surface
- `d:/ymir work/terrainmaps/empirewar/snow/grass02.dds`: snow-covered surface
- `d:/ymir work/terrainmaps/dungeon/devilcave/dc_grass_00.dds`: dark dungeon earth despite grass filename
