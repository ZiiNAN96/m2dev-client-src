"""ZiiNAN: Repeat only the two 5B NPC material transitions on original assets."""
import dbg
import app
import localeInfo
import wndMgr
import systemSetting
import mouseModule
import networkModule
import uiCandidate
import constInfo
import musicInfo
import stringCommander
import chrmgr
import chr
import actor_variants_smoke as scene

chrmgr.CreateRace(20095)
chrmgr.SelectRace(20095)
chrmgr.LoadLocalRaceData("season1/npc/sinseon/sinseon.msm")
chrmgr.SetPathName("season1/npc/sinseon/")
chrmgr.RegisterMotionMode(chr.MOTION_MODE_GENERAL)
chrmgr.RegisterMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_WAIT, "stand00.msa")
scene.RACES.update({20018: "doctor", 20095: "sinseon"})
scene.MIXED_RACES = [0, 9002, 101, 20018, 20095, 102, 9003]
scene.PHASE_SECONDS = 6
scene.PHASES = []
for map_name in ("a1", "b1", "a1"):
    scene.PHASES.extend([
        (map_name, "armor", 0, 12, "idle"),
        (map_name, "fade", 20018, 0, "idle"),
        (map_name, "restore", 20018, 0, "idle"),
        (map_name, "npc-goods", 20018, 0, "idle"),
        (map_name, "fade", 20095, 0, "idle"),
        (map_name, "restore", 20095, 0, "idle"),
        (map_name, "npc-goods", 20095, 0, "idle"),
        (map_name, "mixed", -1, 6, "run"),
        (map_name, "fade", -1, 6, "idle"),
        (map_name, "restore", -1, 9, "attack"),
        (map_name, "out-of-view", -1, 9, "idle"),
        (map_name, "reenter", -1, 12, "run"),
        (map_name, "hidden", -1, 12, "idle"),
        (map_name, "respawn", -1, 12, "idle")])
scene.PHASES.append(("a1", "final-delete", -3, 0, "idle"))
scene.run()
