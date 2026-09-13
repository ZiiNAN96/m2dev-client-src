"""ZiiNAN: Original world-bound text, native damage queue and actor lifetimes; no login/network."""
import app
import background
import builtins
import chr
import chrmgr
import effect
import grp
import item
import json
import math
import player
import playersettingmodule
import systemSetting
import textTail
import time
import ui
import uiPrivateShopBuilder
import wndMgr

PHASES = [
    ("a1", "names", 0), ("a1", "mount", 20110), ("a1", "dismount", 0),
    ("a1", "far", 0), ("a1", "hidden", 0), ("a1", "return", 20110),
    ("b1", "names", 0), ("b1", "mount", 20110), ("b1", "despawn", 0),
    ("a1", "respawn", 0), ("a1", "camera-away", 0), ("a1", "final-drain", 0),
]

def run():
    try:
        with builtins.old_open("config/ui-fixture.json") as stream:
            settings = json.load(stream)
    except FileNotFoundError:
        settings = {}
    seconds = float(settings.get("map_seconds", 30)) / 2
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 floating text 10B test", width, height, 1)
    app.SetCameraMaxDistance(20000.0)
    app.SetSightRange(32000)
    grp.SetClearColor(.08, .16, .28)
    systemSetting.SetShowDamageFlag(1)
    systemSetting.SetShowSalesTextFlag(1)
    if not app.LoadLocaleData(app.GetLocalePath()):
        raise RuntimeError("Original locale missing")
    playersettingmodule.__LoadGameNPC()
    playersettingmodule.__LoadGameEffect()
    chrmgr.CreateRace(0)
    chrmgr.SelectRace(0)
    chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
    playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")
    for race, name in ((9003, "goods"), (9002, "defence"), (101, "stray_dog"), (691, "orc_lord")):
        chrmgr.RegisterRaceName(race, name)
    # Same legacy registration table used by GameWindow.Open, without opening the network phase.
    import colorInfo
    for index, color in ((chrmgr.NAMECOLOR_PC, colorInfo.CHR_NAME_RGB_PC),
                         (chrmgr.NAMECOLOR_NPC, colorInfo.CHR_NAME_RGB_NPC),
                         (chrmgr.NAMECOLOR_MOB, colorInfo.CHR_NAME_RGB_MOB)):
        chrmgr.RegisterNameColor(index, *color)
    chrmgr.SetEmpireNameMode(False)
    log = builtins.old_open("floating-test.log", "w")

    class FloatingWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self, "GAME")
            self.started = time.monotonic()
            self.phase = -1
            self.map = None
            self.actors = []
            self.items = []
            self.shop = None
            self.last_damage = -1
            self.last_sample = -1
            self.labels = {}
            self.SetSize(width, height)
            self.Show()
            self.status = ui.TextLine()
            self.status.SetParent(self)
            self.status.SetPosition(12, 12)
            self.status.SetOutline()
            self.status.Show()

        def cleanup(self):
            if self.shop:
                self.shop.Hide()
                uiPrivateShopBuilder.DeleteADBoard(self.actors[1])
                self.shop = None
            for vid in self.items:
                item.DeleteItem(vid)
            self.items = []
            for vid in self.actors:
                chr.DeleteInstance(vid)
            self.actors = []
            self.labels = {}
            info = app.GetInfo(app.INFO_TEXTTAIL)
            log.write("deleted owners: " + info + "\n")
            if "ChatTail 0, ChrTail (Map 0, List 0), ItemTail (Map 0, List 0)" not in info:
                raise RuntimeError("Stale floating entries after owner deletion: " + info)
            textTail.Clear()
            log.flush()

        def populate(self, mount):
            for index, (race, name) in enumerate(((0, "M10B Player"), (9003, "Merchant"),
                                                  (9002, "Armour NPC"), (101, "Stray dog"), (691, "Orc Lord"))):
                vid = 61000 + index
                if race == 0:
                    chr.CreateInstance(vid, {"horse": mount})
                else:
                    chr.CreateInstance(vid)
                chr.SelectInstance(vid)
                chr.SetVirtualID(vid)
                chr.SetInstanceType(chr.INSTANCE_TYPE_PLAYER if race == 0 else
                                    chr.INSTANCE_TYPE_NPC if race >= 9000 else chr.INSTANCE_TYPE_ENEMY)
                chr.SetRace(race)
                chr.SetArmor(0)
                if race == 0:
                    chr.ChangeShape(0)
                    chr.SetWeapon(19)
                    chr.SetHair(1001)
                chr.SetNameString(name)
                mode = chr.MOTION_MODE_HORSE_ONEHAND_SWORD if mount and race == 0 else chr.MOTION_MODE_GENERAL
                chr.SetMotionMode(mode)
                chr.SetLoopMotion(chr.MOTION_WAIT)
                chr.Show()
                self.labels[vid] = chr.testRefreshTextTail(vid, 1 if index == 0 else 0, 54 if index == 0 else 0)
                self.actors.append(vid)
                log.write("actor vid=%d race=%d mount=%d original-offset=%.2f\n" % (vid, race, mount if index == 0 else 0, self.labels[vid]))
            player.SetMainCharacterIndex(self.actors[0])
            textTail.AttachTitle(self.actors[0], "Friendly", .4, .8, 1.)
            self.shop = uiPrivateShopBuilder.PrivateShopAdvertisementBoard()
            self.shop.Open(self.actors[1], "Original shop name")
            x, y = self.xy
            for index, vnum in enumerate((19, 3009, 7009)):
                vid = 62000 + index
                item.CreateItem(vid, vnum, x + 100 + index * 30, y + 240, background.GetHeight(x + 100, y + 240), True)
                self.items.append(vid)

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            phase = min(int(elapsed / seconds), len(PHASES) - 1)
            suffix, mode, mount = PHASES[phase]
            if phase != self.phase:
                self.cleanup()
                if suffix != self.map:
                    if self.map:
                        background.Destroy()
                    background.Initialize()
                    self.xy = (58300., 63000.) if suffix == "a1" else (69100., 56000.)
                    background.LoadMap("metin2_map_" + suffix, *self.xy, 0.)
                    self.map = suffix
                    log.write("map loaded: " + suffix + "\n")
                background.SetShadowLevel(0)
                background.SetViewDistanceSet(background.DISTANCE0, 32000.)
                background.SelectViewDistanceNum(background.DISTANCE0)
                if mode not in ("despawn", "final-drain"):
                    self.populate(mount)
                    textTail.RegisterChatTail(self.actors[1], "Original floating chat")
                    textTail.RegisterInfoTail(self.actors[2], "Original info message")
                self.phase = phase
                self.status.SetText("M10B %s / %s - nameplates, guild, shop, damage; auto close" % (suffix, mode))
                log.write("phase=%d mode=%s\n" % (phase, mode))
                log.flush()
            x, y = self.xy
            z = background.GetHeight(x, y)
            background.Update(x, -y, z)
            orbit = (elapsed - phase * seconds) * 12.
            distance = 4200. if mode == "far" else 2300. + math.sin(elapsed * .4) * 350.
            self.camera = (distance, 30. + 10. * math.sin(elapsed * .2), orbit)
            for index, vid in enumerate(self.actors):
                chr.SelectInstance(vid)
                dx = (index - 2) * 310. + (math.sin(elapsed) * 100. if index == 0 else 0.)
                if mode == "far" and index > 1:
                    dx += 4000.
                chr.SetPixelPosition(int(x + dx), int(y), int(background.GetHeight(x + dx, y)))
                chr.SetRotation(elapsed * 20.)
                if mode == "hidden" and index > 1:
                    chr.Hide()
            center_x = x + (10000. if mode == "camera-away" else 0.)
            app.SetCenterPosition(center_x, -y, z + 160.)
            app.SetCamera(*self.camera, 0.)
            self.center = (center_x, -y, z + 160.)
            tick = int(elapsed * 2)
            if self.actors and tick != self.last_damage and mode != "camera-away":
                self.last_damage = tick
                flag = (1, 32, 16, 4, 8, 2)[tick % 6]
                chr.testAddDamageEffect(self.actors[3], 123456 if tick % 3 else 907, flag, False, True)
                chr.testAddDamageEffect(self.actors[0], 456, flag, True, False)
            chr.Update()
            item.Update()
            effect.Update()
            sample = int(elapsed / 2)
            if sample != self.last_sample:
                self.last_sample = sample
                log.write("seconds=%.1f %s | %s\n" % (elapsed, app.GetInfo(app.INFO_TEXTTAIL), app.GetInfo(app.INFO_EFFECT)))
                log.flush()
            if elapsed >= len(PHASES) * seconds:
                app.Exit()

        def OnRender(self):
            if not hasattr(self, "center"):
                return
            grp.SetPositionCamera(*self.center, *self.camera)
            app.RenderGame()
            textTail.UpdateAllTextTail()
            textTail.ShowAllTextTail()
            textTail.UpdateShowingTextTail()
            textTail.ArrangeTextTail()
            # The offline harness has no network GamePhase update; project the unchanged shop
            # widget while the original world matrices are current, as in the regular client.
            if self.shop:
                self.shop.OnUpdate()
            if self.items:
                textTail.SelectItemName(self.items[0])
            grp.PopState()
            grp.SetInterfaceRenderState()
            textTail.Render()
            textTail.HideAllTextTail()

        def OnPressEscapeKey(self):
            app.Exit()
            return True

    window = FloatingWindow()
    app.Loop()
    window.cleanup()
    chr.Destroy()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("completed phases=%d; normal floating-text shutdown\n" % (window.phase + 1))
    log.close()
