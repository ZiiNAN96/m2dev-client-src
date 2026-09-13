"""ZiiNAN: Original world/minimap/atlas paths; offline, no packets or account changes."""
import app, background, builtins, chr, chrmgr, grp, miniMap, player
import playersettingmodule, systemSetting, time, ui, wndMgr

width,height=systemSetting.GetWidth(),systemSetting.GetHeight()
wndMgr.SetScreenSize(width,height)
app.Create("Metin2 special world milestone 11 test",width,height,1)
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(24000)
if not app.LoadLocaleData(app.GetLocalePath()): raise RuntimeError("Original locale missing")
playersettingmodule.__LoadGameNPC()
playersettingmodule.__LoadGameEffect()
chrmgr.CreateRace(0); chrmgr.SelectRace(0); chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
playersettingmodule.__LoadGameWarriorEx(0,"d:/ymir work/pc/warrior/")
miniMap.Create(); miniMap.SetMiniMapSize(128.0,128.0); miniMap.SetScale(2.0); miniMap.Show()
SCENES=(("a1",44000,27200,5000,35),("b1",70400,53600,5000,40),
        ("a1",75200,56000,6500,25),("monkeydungeon",7260,11390,2600,20),
        ("guild_01",27000,26000,5000,35),("a1",44000,27200,5000,50))
log=builtins.old_open("special-world-test.log","w")

class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width,height); self.Show()
        self.started=time.monotonic(); self.phase=-1; self.shot=-1; self.position=(0,0,0)
        self.label=ui.TextLine(); self.label.SetParent(self); self.label.SetPosition(20,20)
        self.label.SetOutline(); self.label.Show()
    def OnUpdate(self):
        elapsed=time.monotonic()-self.started; phase=int(elapsed/25)
        if phase>=len(SCENES): app.Exit(); return
        name,x,y,distance,pitch=SCENES[phase]
        if phase!=self.phase:
            chr.Destroy()
            if self.phase>=0: background.Destroy()
            background.Initialize(); background.LoadMap("metin2_map_"+name,float(x),float(y),0.0)
            background.SetViewDistanceSet(background.DISTANCE0,24000.0); background.SelectViewDistanceNum(background.DISTANCE0)
            background.SetShadowLevel(5 if phase%2 else 0)
            miniMap.LoadAtlas(); miniMap.Show()
            if name!="monkeydungeon": background.testGuildArea(x-1200,y-800,x+1200,y+800)
            for i,(race,kind) in enumerate(((0,6),(9003,1),(101,0),(8001,2),(20101,1),(20016,1))):
                vid=57100+i; chr.CreateInstance(vid); chr.SelectInstance(vid); chr.SetVirtualID(vid)
                chr.SetNameString("M11 %d" % race); chr.SetInstanceType(kind); chr.SetRace(race); chr.SetArmor(0)
                if not i: chr.SetWeapon(19); chr.SetHair(1001)
                chr.SetMotionMode(chr.MOTION_MODE_GENERAL); chr.SetLoopMotion(chr.MOTION_WAIT)
                px=x+(i-2)*350; chr.SetPixelPosition(int(px),int(y),int(background.GetHeight(px,y))); chr.Show()
            player.SetMainCharacterIndex(57100)
            self.phase=phase; self.label.SetText("M11 %s / minimap + atlas / shadow setting %d" % (name,5 if phase%2 else 0))
            log.write("phase=%d map=%s\n" % (phase,name)); log.flush()
        z=background.GetHeight(x,y); background.Update(x,-y,z); self.position=(x,y,z)
        self.camera=(distance,pitch,(elapsed-phase*25)*10.0)
        app.SetCenterPosition(x,-y,z+160); app.SetCamera(*self.camera,0.0)
        chr.Update(); miniMap.Update(float(x),float(y)); miniMap.UpdateAtlas()
        if elapsed-phase*25>12: miniMap.ShowAtlas()
        else: miniMap.HideAtlas()
        if elapsed-phase*25>3 and self.shot!=phase:
            self.shot=phase
            log.write("screenshot=%s\n" % (grp.SaveScreenShotToPath("m11-world-%02d-" % phase),)); log.flush()
    def OnRender(self):
        x,y,z=self.position; grp.SetPositionCamera(x,-y,z+160,*self.camera)
        app.RenderGame(); grp.SetInterfaceRenderState()
        miniMap.Render(float(width-155),40.0); miniMap.RenderAtlas(20.0,200.0)
    def OnPressEscapeKey(self): app.Exit(); return True

window=World(); app.Loop()
miniMap.Destroy(); chr.Destroy(); background.Destroy()
window.label.Hide(); window.label.Destroy(); window.Hide(); window.Destroy()
log.write("completed phases=%d\n" % (window.phase+1)); log.close()
