"""ZiiNAN: Offline M11 original selection renderer, all eight races, no network."""
import app
import builtins
import chr
import chrmgr
import event
import grp
import introSelect
import playersettingmodule
import systemSetting
import time
import ui
import uitip
import uiQuest
import wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("Metin2 special paths milestone 11 test", width, height, 1)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Original locale missing")
for race, job, sex, folder in ((0,"warrior","m","pc"),(1,"assassin","w","pc"),
    (2,"sura","m","pc"),(3,"shaman","w","pc"),(4,"warrior","w","pc2"),
    (5,"assassin","m","pc2"),(6,"sura","w","pc2"),(7,"shaman","m","pc2")):
    chrmgr.CreateRace(race)
    chrmgr.SelectRace(race)
    chrmgr.LoadLocalRaceData("msm/%s_%s.msm" % (job, sex))
    playersettingmodule.SetIntroMotions(chr.MOTION_MODE_GENERAL,"d:/ymir work/%s/%s/intro/" % (folder,job))

log = builtins.old_open("special-test.log", "w")

class Preview(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.start = time.monotonic()
        self.phase = -1
        self.shot = -1
        self.SetSize(width,height)
        self.Show()
        # ZiiNAN: Match the normal selection screen's opaque background on every frame.
        self.background = ui.Bar()
        self.background.SetParent(self)
        self.background.SetSize(width,height)
        self.background.SetColor(0xff000000)
        self.background.Show()
        self.renderer = introSelect.SelectCharacterWindow.CharacterRenderer()
        self.renderer.SetParent(self)
        self.renderer.SetSize(width,height)
        self.renderer.Show()
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20,20)
        self.label.SetOutline()
        self.label.Show()
        self.banner = uitip.TextBar(250,60)
        self.banner.SetParent(self)
        self.banner.SetPosition(15,75)
        self.banner.TextOut(4,4,"M11 original notice banner")
        self.banner.Show()
        self.quest=event.RegisterEventSetFromString("M11 original quest dialog current line")
        if self.quest<0: raise RuntimeError("Quest fixture event registration failed")
        event.SetEventSetWidth(self.quest,320)
        event.SetRestrictedCount(self.quest,45)
        for line in range(24):
            event.InsertText(self.quest,"Quest line %02d: |cff60ff80original event text|r" % line)
        self.questView=uiQuest.DescriptionWindow(self.quest)
        self.questView.Show()

    def OnUpdate(self):
        elapsed = time.monotonic()-self.start
        phase = int(elapsed/10)
        if phase>=16:
            app.Exit()
            return
        if phase!=self.phase:
            if self.phase>=0:
                chr.DeleteInstance(1)
            self.phase=phase
            race=phase%8
            chr.CreateInstance(1)
            chr.SelectInstance(1)
            chr.SetVirtualID(1)
            chr.SetNameString("M11 race %d" % race)
            chr.SetRace(race)
            chr.SetArmor(0 if phase<8 else (11200,11400,11600,11800)[race%4])
            chr.SetHair(0 if phase<8 else 1001)
            chr.Refresh()
            chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
            chr.SetLoopMotion(chr.MOTION_INTRO_WAIT)
            self.label.SetText("M11 selection race %d / phase %d - auto close" % (race,phase))
            log.write("phase=%d race=%d\n" % (phase,race)); log.flush()
            event.SetVisibleStartLine(self.quest,(phase%3)*8)
            log.write("quest_page=%d\n" % ((phase%3)*8)); log.flush()
        chr.SelectInstance(1)
        chr.SetRotation((elapsed%10)*36)
        chr.Update()
        event.UpdateEventSet(self.quest,20,-180)
        if elapsed-phase*10>3 and self.shot!=phase:
            self.shot=phase
            log.write("screenshot=%s\n" % (grp.SaveScreenShotToPath("m11-preview-%02d-" % phase),)); log.flush()

    def OnPressEscapeKey(self):
        app.Exit()
        return True

window=Preview()
app.Loop()
chr.Destroy()
event.ClearEventSet(window.quest)
window.questView.Hide()
window.questView.Destroy()
window.renderer.Hide()
window.renderer.Destroy()
window.background.Hide()
window.background.Destroy()
window.banner.Hide()
window.banner.Destroy()
window.label.Hide()
window.label.Destroy()
window.Hide()
window.Destroy()
log.write("completed phases=%d\n" % (window.phase+1))
log.close()
