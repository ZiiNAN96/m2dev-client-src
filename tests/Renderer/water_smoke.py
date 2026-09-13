"""ZiiNAN: Native effect runtime, original assets and real map/actor ownership."""
import app
import background
import builtins
import chr
import chrmgr
import effect
import fly
import item
import grp
import math
import os
import playersettingmodule
import systemSetting
import time
import ui
import wndMgr

PHASE_SECONDS = 30.0
SCENES = [
    ("a1", 44000.0, 27200.0, "fire", 4500.0, 35.0),
    ("a1", 24000.0, 89600.0, "buff", 12000.0, 15.0),
    ("b1", 70400.0, 53600.0, "melee", 3800.0, 65.0),
    ("b1", 45600.0, 26400.0, "fire", 16000.0, 25.0),
    ("a1", 44000.0, 27200.0, "melee", 7500.0, 10.0),
    ("a1", 75200.0, 56000.0, "fire", 4500.0, 50.0),
]
ASSETS = {
    "fire": "d:/ymir work/effect/background/fire_camp1.mse",
    "lamp": "d:/ymir work/effect/background/fire_general_obj_lamp.mse",
    "melee": "d:/ymir work/pc/warrior/effect/samyeon_d.mse",
    "aoe": "d:/ymir work/pc/warrior/effect/palbang_spin.mse",
    "ranged": "d:/ymir work/pc/assassin/effect/hwajopa_blow.mse",
    "hit": "d:/ymir work/effect/hit/blow_flame/flame_3_blow.mse",
    "ent": "d:/ymir work/effect/monster2/ent_black.mse",
    "magma": "d:/ymir work/effect/background/magmabublea.mse",
    "animation": "d:/ymir work/guild/effect/jumunsul_make.mse",
}


def run(ui_factory=None, title="Metin2 water milestone 8 test", size=None):
    width, height = size or (systemSetting.GetWidth(), systemSetting.GetHeight())
    if size and size != (systemSetting.GetWidth(), systemSetting.GetHeight()):
        raise RuntimeError("UI fixture size must match the native device/window configuration")
    wndMgr.SetScreenSize(width, height)
    if ui_factory:
        import mouseModule
        app.SetMouseHandler(mouseModule.mouseController)
        wndMgr.SetMouseHandler(mouseModule.mouseController)
    app.Create(title, width, height, 1)
    if ui_factory:
        mouseModule.mouseController.Create()
        mouseModule.mouseController.IsSoftwareCursor = True
    app.SetCameraMaxDistance(40000.0)
    app.SetSightRange(32000)
    app.SetArmorSpecularEnable(True)
    app.SetHairColorEnable(True)
    grp.SetClearColor(.08, .16, .28)
    log = builtins.old_open("water-test.log", "w")
    if not app.LoadLocaleData(app.GetLocalePath()):
        raise RuntimeError("Original locale data unavailable")
    playersettingmodule.__LoadGameNPC()
    playersettingmodule.__LoadGameEffect()
    chrmgr.CreateRace(0)
    chrmgr.SelectRace(0)
    chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
    playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")
    for path in ASSETS.values():
        effect.RegisterEffect(path)

    class EffectWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.phase = -1
            self.map = None
            self.effects = []
            self.actors = []
            self.last_spawn = -1
            self.last_sample = -1
            self.frames = 0
            self.panels = None
            self.SetSize(width, height)
            self.Show()

        def clear_effects(self):
            for index in self.effects:
                effect.DeleteEffect(index)
            self.effects = []
            for vid in self.actors:
                chrmgr.SetAffect(vid, 3, 0)
                chrmgr.SetAffect(vid, 29, 0)

        def populate(self):
            chr.Destroy()
            self.actors = []
            for index, race in enumerate((0, 9003, 9002, 101, 102)):
                vid = 57000 + index
                chr.CreateInstance(vid)
                chr.SelectInstance(vid)
                chr.SetVirtualID(vid)
                chr.SetInstanceType(chr.INSTANCE_TYPE_PLAYER if race == 0 else
                                    chr.INSTANCE_TYPE_NPC if race >= 9000 else chr.INSTANCE_TYPE_ENEMY)
                chr.SetRace(race)
                if race == 0:
                    chr.SetArmor(0)
                    chr.SetWeapon(19)
                    chr.SetHair(1001)
                else:
                    chr.SetArmor(0)
                chr.SetMotionMode(chr.MOTION_MODE_ONEHAND_SWORD if race == 0 else chr.MOTION_MODE_GENERAL)
                chr.SetLoopMotion(chr.MOTION_WAIT)
                chr.Show()
                self.actors.append(vid)

        def spawn(self, name, dx, dy):
            x, y, z = self.position
            chr.SelectInstance(self.actors[0])
            # Existing Python CreateEffect copies pixel coordinates directly into the render matrix.
            # Its SetPosition only changes the base object, not EffectInstance.m_matGlobal.
            chr.SetPixelPosition(int(x + dx), int(-y + dy), int(background.GetHeight(x + dx, y - dy) + 100.0))
            index = effect.CreateEffect(ASSETS[name])
            chr.SetPixelPosition(int(x), int(y), int(z))
            self.effects.append(index)

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            if self.panels:
                self.panels.update(elapsed)
            phase = min(int(elapsed / PHASE_SECONDS), len(SCENES) - 1)
            suffix, x, y, mode, distance, pitch = SCENES[phase]
            new_scene = phase != self.phase
            if phase != self.phase:
                self.clear_effects()
                if self.phase >= 0:
                    for vid in (58000, 58001, 58002):
                        item.DeleteItem(vid)
                if suffix != self.map:
                    chr.Destroy()
                    if self.map is not None:
                        background.Destroy()
                    background.Initialize()
                    background.LoadMap("metin2_map_" + suffix, x, y, 0.0)
                    self.map = suffix
                    self.populate()
                    log.write("map loaded: %s\n" % suffix)
                background.SetShadowLevel(0)
                for part in (background.PART_SKY, background.PART_CLOUD, background.PART_WATER):
                    background.SetVisiblePart(part, 1)
                background.SetViewDistanceSet(background.DISTANCE0, 32000.0)
                background.SelectViewDistanceNum(background.DISTANCE0)
                background.EnableSnow(int(mode == "snow"))
                self.phase = phase
                self.last_spawn = -1
                if mode == "buff":
                    chrmgr.SetAffect(self.actors[0], 3, 1)
                    chrmgr.SetAffect(self.actors[0], 29, 1)
                log.write("phase=%d map=%s mode=%s\n" % (phase, suffix, mode))
                log.flush()
            z = background.GetHeight(x, y)
            background.Update(x, -y, z)
            z = background.GetHeight(x, y)
            if new_scene:
                for index, vnum in enumerate((19, 3009, 7009)):
                    item.CreateItem(58000 + index, vnum, x + 600 + index * 180, y + 400, background.GetHeight(x + 600, y + 400), True)
            self.position = (x, y, z)
            self.camera = (distance, pitch, (elapsed - phase * PHASE_SECONDS) * 8.0)
            app.SetCenterPosition(x, -y, z + 160.0)
            app.SetCamera(*self.camera, 0.0)
            for index, vid in enumerate(self.actors):
                dx = (index - 2) * 400.0 + (math.sin(elapsed) * 100.0 if index == 0 else 0)
                chr.SelectInstance(vid)
                chr.SetPixelPosition(int(x + dx), int(y), int(background.GetHeight(x + dx, y)))
                chr.SetRotation(float(elapsed * 35.0))
            tick = int(elapsed / 3)
            if tick != self.last_spawn and mode not in ("drain", "snow"):
                self.last_spawn = tick
                if mode == "fire" and not self.effects:
                    self.spawn("fire", -650, -400)
                    self.spawn("lamp", 650, -400)
                elif mode == "special":
                    self.spawn("ent", -500, -400)
                    self.spawn("magma", 500, -400)
                elif mode == "animation":
                    self.spawn("animation", 0, -400)
                elif mode == "dense":
                    if not self.effects:
                        for k in range(20):
                            self.spawn("fire", (k % 5 - 2) * 250, (k // 5 - 1.5) * 250)
                    for k in range(20):
                        self.spawn(("hit", "melee", "ranged")[k % 3], (k % 5 - 2) * 250, (k // 5 - 1.5) * 250)
                elif mode in ("melee", "aoe", "ranged", "buff"):
                    self.spawn("hit", 0, -350)
                    if mode == "ranged":
                        self.spawn("ranged", 500, -350)
                    chr.SelectInstance(self.actors[0])
                    chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
                    chr.PushOnceMotion(chr.MOTION_SKILL + {"melee": 1, "aoe": 2, "buff": 4, "ranged": 16}[mode])
            chr.Update()
            item.Update()
            effect.Update()
            fly.Update()
            self.frames += 1
            sample = int(elapsed / 2)
            if sample != self.last_sample:
                log.write("sample seconds=%.1f frames=%d fps=%s\n" % (elapsed, self.frames, app.GetRenderFPS()))
                log.flush()
                self.last_sample = sample
            if elapsed >= len(SCENES) * PHASE_SECONDS:
                app.Exit()

        def OnRender(self):
            if self.panels and not self.panels.world:
                grp.SetInterfaceRenderState()
                return
            x, y, z = self.position
            grp.SetPositionCamera(x, -y, z + 160.0, *self.camera)
            # Exercise the actual world composition, including native Snow/Area/Actor effect calls.
            app.RenderGame()
            grp.SetInterfaceRenderState()

        def OnPressEscapeKey(self):
            app.Exit()
            return True

    window = EffectWindow()
    # ZiiNAN: Optional original-UI fixture; M8's default test and production scripts stay unchanged.
    if ui_factory:
        window.panels = ui_factory(width, height)
    app.Loop()
    if window.panels:
        window.panels.destroy()
        window.panels = None
    window.clear_effects()
    for vid in (58000, 58001, 58002):
        item.DeleteItem(vid)
    chr.Destroy()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("normal water/sky/ground-item/actor/map/window shutdown\n")
    log.close()
