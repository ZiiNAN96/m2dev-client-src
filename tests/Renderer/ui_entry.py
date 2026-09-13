import builtins
import json
import water_smoke
from ui_fixture_panels import UiPanels
try:
    with builtins.old_open("config/ui-fixture.json") as stream:
        settings = json.load(stream)
except FileNotFoundError:
    settings = {}
water_smoke.PHASE_SECONDS = float(settings.get("map_seconds", 30))
UiPanels.SECONDS = water_smoke.PHASE_SECONDS / 2
size = None
if "width" in settings:
    size = (int(settings["width"]), int(settings["height"]))
water_smoke.run(UiPanels, "Metin2 UI milestone 9 test", size)
