"""Focused QuestCurtain lifecycle regression, using the runtime's actual class.

Usage: python tests/Graphics/test_quest_curtain_resize.py ../m2dev-client
Only native widget storage/time are stubbed; the production curtain code runs.
"""
import ast
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest

runtime = Path(sys.argv.pop(1)) if len(sys.argv) > 1 else Path(__file__).resolve().parents[3] / 'm2dev-client'
script = runtime if runtime.is_file() else runtime / 'assets/root/uiquest.py'
tree = ast.parse(script.read_text(encoding='utf-8-sig'))
curtain_code = compile(ast.Module(body=[next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'QuestCurtain')], type_ignores=[]), 'uiquest.py', 'exec')


class Widget:
    def __init__(self, *args):
        self.position = (0, 0)
        self.size = (0, 0)
        self.visible = False
        self.writes = 0
    def SetPosition(self, x, y): self.position = (int(x), int(y)); self.writes += 1
    def SetSize(self, w, h): self.size = (w, h); self.writes += 1
    def GetGlobalPosition(self): return self.position
    GetLocalPosition = GetGlobalPosition
    def GetHeight(self): return self.size[1]
    def SetColor(self, color): self.color = color
    def Show(self): self.visible = True


class QuestCurtainResize(unittest.TestCase):
    def setUp(self):
        self.screen = [1280, 960]
        self.clock = 0.0
        namespace = dict(ui=SimpleNamespace(Window=Widget, Bar=Widget),
                         wndMgr=SimpleNamespace(GetScreenWidth=lambda: self.screen[0], GetScreenHeight=lambda: self.screen[1]),
                         time=SimpleNamespace(perf_counter=lambda: self.clock))
        exec(curtain_code, namespace)
        cls = namespace['QuestCurtain']
        cls.BarHeight = 120
        self.curtain = cls()
        self.callbacks = []
        self.cls = cls

    def tick(self, seconds=0.0):
        self.clock += seconds
        self.curtain.OnUpdate()

    def resize(self, w, h):
        self.screen[:] = w, h
        self.tick()

    def assert_closed(self):
        for bar in (self.curtain.TopBar, self.curtain.BottomBar):
            x, y = bar.position
            w, h = bar.size
            self.assertEqual(w, self.screen[0])
            self.assertTrue(y + h <= 0 or y >= self.screen[1], (bar.position, bar.size, self.screen))

    def test_closed_grow_shrink_width_height_and_repeated_return(self):
        self.curtain.Close()
        initial = (self.curtain.TopBar.position, self.curtain.BottomBar.position, self.curtain.TopBar.size)
        for _ in range(5):
            for size in ((1920, 1200), (1920, 960), (1024, 768), (1280, 960)):
                self.resize(*size)
                self.assert_closed()
            self.assertEqual(initial, (self.curtain.TopBar.position, self.curtain.BottomBar.position, self.curtain.TopBar.size))

    def test_open_cinema_keeps_both_bars_at_live_edges(self):
        self.curtain.CurtainMode = 1
        self.tick(1)
        for size in ((1920, 1200), (1280, 1024), (1280, 720), (1920, 900), (1280, 960)):
            self.resize(*size)
            top, bottom = self.curtain.TopBar, self.curtain.BottomBar
            self.assertEqual(top.position, (0, 0))
            self.assertEqual(bottom.position, (0, size[1] - bottom.size[1]))
            self.assertEqual(top.size, bottom.size)
            self.assertEqual(top.size[0], size[0])

    def test_resize_during_animation_preserves_progress_and_callback(self):
        self.curtain.CurtainMode = 1
        self.tick(.3)
        self.assertEqual(self.curtain.TopBar.position[1], -60)
        self.resize(1600, 1200)
        top, bottom = self.curtain.TopBar, self.curtain.BottomBar
        self.assertEqual(top.position[1], -75)
        self.assertEqual(bottom.position[1], 1125)
        self.assertEqual(self.curtain.CurtainMode, 1)
        self.tick(1)
        self.curtain.CurtainMode = -1
        self.tick(.3)
        self.resize(1280, 960)
        self.assertEqual(self.curtain.CurtainMode, -1)
        self.cls.OnDoneEventList.append(lambda curtain: self.callbacks.append(curtain))
        self.tick(1)
        self.assert_closed()
        self.assertEqual(self.callbacks, [self.curtain])
        self.resize(1920, 1200)
        self.assertEqual(self.callbacks, [self.curtain])

    def test_zero_height_closed_and_idle_do_not_rewrite_geometry(self):
        self.curtain.Close()
        self.resize(1280, 720)
        self.assert_closed()
        self.resize(1920, 1200)
        self.assert_closed()
        writes = self.curtain.TopBar.writes + self.curtain.BottomBar.writes
        for _ in range(20): self.tick(.02)
        self.assertEqual(writes, self.curtain.TopBar.writes + self.curtain.BottomBar.writes)


if __name__ == '__main__':
    unittest.main()
