# ZiiNAN: Original persistent fire emitters plus burst skills, then release all test-owned effects.
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
import effects_smoke
effects_smoke.PHASES = [("a1", "dense"), ("a1", "drain")]
effects_smoke.PHASE_SECONDS = 20.0
effects_smoke.run()
