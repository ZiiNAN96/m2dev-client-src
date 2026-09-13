"""ZiiNAN: Offline original UI scripts/widgets, real assets; no network or gameplay mutations."""
import app
import builtins
import grp
import item
import math
import mouseModule
import player
import skill
import ui
import uiToolTip


class UiPanels:
    SECONDS = 15.0
    PHASES = ("login", "select", "loading", "hud", "inventory", "character",
              "skill", "shop", "storage", "system", "login", "inventory")
    FILES = {"login": "loginwindow", "select": "selectcharacterwindow", "loading": "loadingwindow",
             "hud": "taskbar", "inventory": "inventorywindow", "character": "characterwindow",
             "skill": "characterwindow", "shop": "shopdialog", "storage": "safeboxwindow", "system": "systemdialog"}

    def __init__(self, width, height):
        self.width, self.height = width, height
        self.log = builtins.old_open("ui-test.log", "w")
        self.record("original UI viewport=%dx%d" % (width, height))
        self.phase = -1
        self.world = False
        self.windows = []
        self.extras = []
        self.tooltip = None
        self.clip = None
        self.scroll = None
        self.drag = False

    def record(self, message):
        self.log.write(message + "\n")
        self.log.flush()

    def clear(self):
        mouseModule.mouseController.DeattachObject()
        if self.tooltip:
            self.tooltip.Hide()
            self.tooltip.ClearToolTip()
            self.tooltip.Destroy()
            self.tooltip = None
        for widget in reversed(self.extras):
            widget.Hide()
            widget.Destroy()
        self.extras = []
        self.clip = self.scroll = None
        for window in self.windows:
            window.Hide()
            window.ClearDictionary()
            window.Destroy()
        self.windows = []

    def load(self, name):
        window = ui.ScriptWindow()
        ui.PythonScriptLoader().LoadScriptFile(window, "uiscript/" + self.FILES[name] + ".py")
        self.windows.append(window)
        window.Show()
        self.record("original script loaded: " + name)
        return window

    def bind_items(self, slots):
        for index, vnum in enumerate((19, 3009, 7009, 11209)):
            slots.SetItemSlot(index, vnum, 0)
        slots.SetSelectItemSlotEvent(self.pick)
        slots.SetSelectEmptySlotEvent(self.drop)
        slots.SetOverInItemEvent(self.over)
        slots.SetOverOutItemEvent(self.out)
        slots.SetSlotCoolTime(0, 12.0)
        slots.RefreshSlot()

    def pick(self, index):
        mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, index,
                                                (19, 3009, 7009, 11209)[index % 4])
        self.record("interaction: item drag attached slot=%d" % index)

    def drop(self, index=0):
        mouseModule.mouseController.DeattachObject()
        self.record("interaction: item drag detached slot=%d" % index)

    def over(self, index):
        if self.tooltip:
            self.tooltip.ShowToolTip()
        self.record("interaction: slot hover=%d" % index)

    def out(self):
        if self.tooltip:
            self.tooltip.HideToolTip()

    def make_clips(self):
        outer = ui.Window("TOP_MOST")
        outer.SetPosition(self.width // 2 - 90, 45)
        outer.SetSize(180, 110)
        outer.EnableScissorRect()
        outer.Show()
        inner = ui.Window()
        inner.SetParent(outer)
        inner.SetPosition(10, 10)
        inner.SetSize(140, 80)
        inner.EnableScissorRect()
        inner.Show()
        bar = ui.Bar()
        bar.SetParent(inner)
        bar.SetPosition(-50, -20)
        bar.SetSize(230, 130)
        bar.SetColor(0x7050b0f0)
        bar.Show()
        image = ui.ExpandedImageBox()
        image.SetParent(inner)
        image.LoadImage("d:/ymir work/ui/game/windows/tab_button_small_01.sub")
        image.SetScale(5, 3)
        image.SetPosition(-30, 5)
        image.Show()
        self.extras.extend((outer, inner, bar, image))
        self.clip = image
        self.clip_parent = inner
        scrollbar = ui.ScrollBar()
        scrollbar.SetPosition(self.width // 2 + 100, 45)
        scrollbar.SetScrollBarSize(110)
        scrollbar.SetScrollEvent(lambda: self.record("interaction: scroll=%.3f" % scrollbar.GetPos()))
        scrollbar.Show()
        self.extras.append(scrollbar)
        self.scroll = scrollbar

    def update(self, elapsed):
        phase = min(int(elapsed / self.SECONDS), len(self.PHASES) - 1)
        if phase != self.phase:
            self.clear()
            self.phase = phase
            name = self.PHASES[phase]
            self.world = name not in ("login", "select", "loading")
            if self.world:
                hud = self.load("hud")
                for gauge in ("HPGauge", "SPGauge", "STGauge"):
                    hud.GetChild(gauge).SetPercentage(65, 100)
                self.bind_items(hud.GetChild("quick_slot_1"))
            panel = self.load(name) if name != "hud" else hud
            if name == "login":
                for hidden in ("VirtualKeyboard", "bg2"):
                    widget = panel.GetChild2(hidden)
                    if widget:
                        widget.Hide()
            if name in ("character", "skill"):
                for page in ("Character_Page", "Skill_Page", "Emoticon_Page", "Quest_Page"):
                    panel.GetChild(page).Hide()
                panel.GetChild("Skill_Page" if name == "skill" else "Character_Page").Show()
                slots = panel.GetChild("Skill_Active_Slot")
                for index, skill_id in zip((1, 3, 5, 7), (1, 2, 3, 4)):
                    for grade in range(3):
                        if not skill.GetIconImageNew(skill_id, grade):
                            raise RuntimeError("Original graded skill icon unavailable")
                        slots.SetSkillSlotNew(index + grade * 20, skill_id, grade, 0)
                self.record("original graded skill icons: 12 bound")
                slots.SetSlotCoolTime(1, 12)
            if name in ("inventory", "shop"):
                self.bind_items(panel.GetChild("ItemSlot"))
            if name == "inventory":
                panel.GetChild("EquipmentSlot").SetItemSlot(90, 11209)
                panel.GetChild("EquipmentSlot").SetItemSlot(94, 19)
            if name == "storage":
                # Same dynamic slot primitive/placement as uiSafebox, without any server requests.
                slots = ui.GridSlotWindow()
                slots.SetParent(panel)
                slots.SetPosition(8, 35)
                slots.ArrangeSlot(0, 5, 3, 32, 32, 0, 0)
                slots.SetSlotBaseImage("d:/ymir work/ui/public/Slot_Base.sub", 1, 1, 1, 1)
                slots.Show()
                self.extras.append(slots)
                self.bind_items(slots)
            if self.world:
                self.tooltip = uiToolTip.ToolTip()
                self.tooltip.AppendSpace(70)
                self.tooltip.AppendHorizontalLine()
                self.make_clips()
            self.record("phase=%d ui=%s world=%d" % (phase, name, self.world))
        if self.clip:
            self.clip.SetPosition(int(-30 + 80 * self.scroll.GetPos()), int(10 + math.sin(elapsed) * 30))
        # Automatic real mouse-controller attachment supplements manual drag/drop, without moving the OS cursor.
        drag = self.world and (int(elapsed) % 15 in (10, 11, 12))
        if drag != self.drag:
            self.drag = drag
            self.pick(0) if drag else self.drop()

    def destroy(self):
        self.clear()
        self.record("UI fixture completed phases=%d: normal widget/texture shutdown" % (self.phase + 1))
        self.log.close()
