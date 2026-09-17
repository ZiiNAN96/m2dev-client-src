"""Controlled armour/weapon surface shimmer and aura in an isolated G56 root.

Uses original local armour, sword and sphere-map assets and their item strengths.
The regular client's armour-specular startup setting is enabled; no production
item data or material model is changed. Prepare with -Manual first,
run this script RUNTIME [ARMOUR], then package its private root. Default armour
11299 exercises surface shimmer; 12019 also has the special blue aura.
One eight-second view, with both character and attached-particle updates.
"""
from pathlib import Path
import runpy
import sys

runtime = Path(sys.argv[1])
armour = int(sys.argv[2]) if len(sys.argv) > 2 else 11299
sys.argv = [str(Path(__file__).with_name('prepare_g56_visual.py')), str(runtime), '0']
runpy.run_path(sys.argv[0], run_name='__main__')
entry = runtime / 'test-root/root/prototype.py'
text = entry.read_text(encoding='utf-8')
assert text.count('app.SetHairColorEnable(True)') == 1
text = text.replace('app.SetHairColorEnable(True)', 'app.SetHairColorEnable(True)\n    app.SetArmorSpecularEnable(True)')
text = text.replace('chr.ChangeShape(0)', 'chr.SetArmor(%d)' % armour)
# The standalone world fixture advances characters itself, so attached particle
# effects must be advanced here as they are by UpdateGame in the regular client.
text = text.replace('import app, background, builtins, chr,', 'import effect, app, background, builtins, chr,')
text = text.replace('            chr.Update()', '            chr.Update()\n            effect.Update()')
assert 'x += 250' in text and '(1800, 22, 0)' in text
text = text.replace('x += 250', 'x += 0')
text = text.replace('(1800, 22, 0)', '(850, 18, 0)')
text = text.replace('G56 fixed visual proof', 'G56 original item shimmer proof')
text = text.replace('g56-view-%d-%s-', 'g56-shimmer-%d-%s-')
text = text.replace('self.label.SetText("F1-X %s: player / NPC / mob / boss / mount; hair + armor + weapon" % name)',
                    'self.label.SetText("G56: original armour %d + sword 19, surface shimmer and attached aura")' % armour)
compile(text, str(entry), 'exec')
entry.write_text(text, encoding='utf-8')
