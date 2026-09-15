"""A2: fixed cameras at trees nearest the nine original A1 bridges."""
import app, background, builtins, grp, systemSetting, time, ui, wndMgr
width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("H-X A2 bridge parity", width, height, 1)
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(24000)
if not app.LoadLocaleData(app.GetLocalePath()): raise RuntimeError("locale load")
SCENES = ((19759,71610,13516),(17078,107819,12774),(50021,33769,17007),(31430,49260,16013),(27709,105432,12782),(73068,32122,20396),(57126,23782,18569),(74282,61383,20358),(77124,110880,13716))
CAMERAS = ((1600,25,0),(4800,35,0),(1600,25,135),(4800,35,135))
log=builtins.old_open("a2-bridge.log","w")
class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width,height);self.Show()
        self.sample=0;self.frame=0;self.loaded=False;self.started=time.monotonic()
    def OnUpdate(self):
        if self.sample>=len(SCENES)*len(CAMERAS) or time.monotonic()-self.started>85:
            app.Exit();return
        x,y,z=SCENES[self.sample//4]
        if not self.loaded:
            background.Initialize();background.LoadMap("metin2_map_a1",float(x),float(y),float(z))
            background.SetViewDistanceSet(background.DISTANCE0,24000.0);background.SelectViewDistanceNum(background.DISTANCE0)
            self.loaded=True
        self.position=(x,y,z+650);self.camera=CAMERAS[self.sample%4]
        app.SetCenterPosition(x,-y,z+650);app.SetCamera(*self.camera,0.0)
        background.Update(x,-y,z)
    def OnRender(self):
        if self.sample>=len(SCENES)*4:return
        x,y,z=self.position
        grp.SetPositionCamera(x,-y,z,*self.camera);app.RenderGame();grp.SetInterfaceRenderState()
        self.frame+=1
        if self.frame==35:
            success,path=grp.SaveScreenShotToPath("a2-bridge-%02d-%d-"%(self.sample//4,self.sample%4))
            if not success:raise RuntimeError("capture failed")
            log.write("sample=%d center=%s camera=%s image=%s\n"%(self.sample,self.position,self.camera,path));log.flush()
            self.sample+=1;self.frame=0
    def OnPressEscapeKey(self):app.Exit();return True
window=World();app.Loop();background.Destroy();window.Hide();window.Destroy()
log.write("completed samples=%d\n"%window.sample);log.close()
