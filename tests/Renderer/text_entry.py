import builtins
import json
import water_smoke
from text_fixture_panels import TextPanels

try:
    with builtins.old_open("config/ui-fixture.json") as stream:
        settings = json.load(stream)
except FileNotFoundError:
    settings = {}
water_smoke.PHASE_SECONDS = float(settings.get("map_seconds", 30))
TextPanels.SECONDS = water_smoke.PHASE_SECONDS / 2
size = (int(settings["width"]), int(settings["height"])) if "width" in settings else None
water_smoke.run(TextPanels, "Metin2 basis text 10A test", size)
