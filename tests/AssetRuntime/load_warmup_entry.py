"""Bounded A1 performance fixture. Real GR2 actors; scripted combat motions, no server AI.
All evidence is retained in memory until app.Loop returns.
"""
import app, background, builtins, chr, chrmgr, grp, item, player
import json, math, playersettingmodule, systemSetting, time, ui, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("ZiiNAN F2-P A1 load/warmup", width, height, 1)
app.SetSightRange(24000)
app.SetHairColorEnable(True)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Original locale required")
item.LoadItemTable(app.GetLocalePath() + "/item_proto")
for phase in ("INIT", "WARRIOR"):
    playersettingmodule.LoadGameData(phase)
playersettingmodule.__LoadGameNPC()

X, Y = 44000, 27200
PLAYER, DOG = 58100, 58200
STAGES = ((1, "cold_idle", 10), (2, "cold_movement", 10), (3, "cold_combat", 12),
          (4, "warm_idle", 15), (5, "warm_movement", 15), (6, "warm_combat", 15))


def create_actor(vid, warrior=False):
    chr.CreateInstance(vid)
    chr.SelectInstance(vid)
    chr.SetVirtualID(vid)
    chr.SetInstanceType(6 if warrior else 0)
    chr.SetRace(0 if warrior else 101)
    chr.SetArmor(0)
    if warrior:
        chr.SetHair(1001)
        chr.SetWeapon(19)
    chr.SetMotionMode(chr.MOTION_MODE_ONEHAND_SWORD if warrior else chr.MOTION_MODE_GENERAL)
    chr.SetLoopMotion(chr.MOTION_WAIT)
    offset = 0 if warrior else (vid - DOG + 1) * 100
    chr.SetPixelPosition(X + offset, Y, int(background.GetHeight(X + offset, Y)))
    chr.Show()


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.index = -1
        self.started = time.monotonic()
        self.step = -1
        self.position = (X, Y, 0)
        self.frames = [0] * 8
        self.events = []
        self.loadingPresented = False
        self.phaseStarted = False
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20, 20)
        self.label.SetOutline()
        self.label.Show()

    def initialize(self):
        app.LoadWarmupPhase(0)
        background.Initialize()
        background.LoadMap("metin2_map_a1", float(X), float(Y), 0.0)
        background.SetViewDistanceSet(background.DISTANCE0, 24000.0)
        background.SelectViewDistanceNum(background.DISTANCE0)
        create_actor(PLAYER, True)
        for i in range(20):
            create_actor(DOG + i)
        player.SetMainCharacterIndex(PLAYER)
        if hasattr(chrmgr, "PrewarmVisibleActors"):
            chrmgr.PrewarmVisibleActors()
        self.index = 0
        self.started = time.monotonic()
        self.events.append({"stage": "map_load", "finished": self.started})

    def OnUpdate(self):
        if self.index < 0:
            self.initialize()
            return
        now = time.monotonic()
        if not self.loadingPresented:
            return
        if not self.phaseStarted:
            self.started = now
            self.phaseStarted = True
        phase, name, duration = STAGES[self.index]
        elapsed = now - self.started
        if elapsed >= duration:
            self.events.append({"stage": name, "duration": elapsed})
            self.index += 1
            if self.index == len(STAGES):
                app.Exit()
                return
            self.started = now
            self.step = -1
            phase, name, duration = STAGES[self.index]
            elapsed = 0
        app.LoadWarmupPhase(phase)
        activity = (phase - 1) % 3
        step = int(elapsed)
        if step != self.step:
            if activity == 0:
                for vid in [PLAYER] + list(range(DOG, DOG + 20)):
                    chr.SelectInstance(vid)
                    chr.SetLoopMotion(chr.MOTION_WAIT)
            elif activity == 1:
                motion = chr.MOTION_WALK if step % 2 == 0 else chr.MOTION_RUN
                for vid in [PLAYER] + list(range(DOG, DOG + 20)):
                    chr.SelectInstance(vid)
                    chr.SetLoopMotion(motion)
                # A newly visible instance reuses the same immutable assets.
                vid = DOG + step % 20
                chr.DeleteInstance(vid)
                create_actor(vid)
            else:
                chr.SelectInstance(PLAYER)
                chr.PushOnceMotion((chr.MOTION_COMBO_ATTACK_1, chr.MOTION_COMBO_ATTACK_2,
                                    chr.MOTION_COMBO_ATTACK_3)[step % 3], 0.1)
                motions = (chr.MOTION_NORMAL_ATTACK, chr.MOTION_DAMAGE,
                           chr.MOTION_DAMAGE_FLYING, chr.MOTION_DEAD)
                for i in range(20):
                    chr.SelectInstance(DOG + i)
                    chr.PushOnceMotion(motions[step % len(motions)], 0.1)
                if step % 4 == 0:
                    vid = DOG + step % 20
                    chr.DeleteInstance(vid)
                    create_actor(vid)
            self.step = step
        offset = 1000 * math.sin(elapsed * 0.4) if activity == 1 else 0
        z = background.GetHeight(X + offset, Y)
        self.position = (X + offset, Y, z)
        app.SetCenterPosition(X + offset, -Y, z + 100)
        app.SetCamera(2200, 30, 0, 0.0)
        background.Update(X + offset, -Y, z)
        chr.Update()
        self.label.SetText("F2-P A1 / 20 Wildhunde / " + name)

    def OnRender(self):
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 100, 2200, 30, 0)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        if not self.loadingPresented:
            self.loadingPresented = True
            self.frames[0] += 1
            return
        if 0 <= self.index < len(STAGES):
            self.frames[STAGES[self.index][0]] += 1

    def OnPressEscapeKey(self):
        app.Exit()
        return True


window = World()
app.Loop()
chr.Destroy()
background.Destroy()
window.label.Hide()
window.label.Destroy()
window.Hide()
window.Destroy()
with builtins.old_open("load-warmup-fixture.json", "w") as output:
    json.dump({"complete": window.index == len(STAGES), "frames": window.frames,
               "events": window.events, "actors": 21, "map": "metin2_map_a1", "width": width, "height": height,
               "combat": "scripted original attack/damage/knockdown/death; live combat checked separately"}, output)
