"""Check the real font metrics against the compact menu's visible bounds."""
import json
import re
import sys
from pathlib import Path
root = Path(sys.argv[1])
def load_font(path):
    text = re.sub(r"//[^\n]*", "", (root / path).read_text())
    return json.loads(text)
def width(font, text, extra=0):
    return sum(int(font['characters'][ch].split()[2]) + int(font['space']) + extra for ch in text)
large = load_font('Oxygen/sonic3air/data/font/sonic3_fontC.json')
regular = load_font('Oxygen/oxygenengine/data/font/oxyfont_regular.json')
small = load_font('Oxygen/oxygenengine/data/font/oxyfont_small.json')
for text in ['NORMAL GAME','ACT SELECT','TIME ATTACK']:
    assert 138 + width(large,text) + 4 <= 320, text
for text in ['OPTIONS','EXTRAS','MODS','CLASSIC MODE','ANNIVERSARY MODE']:
    assert 152 + width(regular,text,1) + 4 <= 320, text
for text in ['Character Rotation:','Time Display:','Lives Display:','Speed Shoes Effect:','Fast Run Animation:','Flicker Effects:']:
    assert 130 - width(small,text) >= 0, text
for text in ['Original','Mobile','None (Original)','As Original']:
    assert width(small,text) <= 160, text
print('Compact menu font bounds passed: main entries, edition toggle and locked preset columns')
