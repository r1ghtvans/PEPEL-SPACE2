# Font slots

The plugin loads exactly three fonts, compiled into the binary with `juce_add_binary_data`
(see `CMakeLists.txt`). Source code never names a font: it only calls
`pixelfont::title()`, `pixelfont::vhs()` and `pixelfont::kanji()` from `Source/Theme.h`.

**To use your own fonts: replace the files below with your own `.ttf`, keep the file names, rebuild.**
No source changes are needed. (File name -> symbol: `FontTitle.ttf` becomes `BinaryData::FontTitle_ttf`.)

| Slot file       | Function              | Used for                                         | Must contain                          | Default (OFL 1.1)        |
|-----------------|-----------------------|--------------------------------------------------|---------------------------------------|--------------------------|
| `FontTitle.ttf` | `pixelfont::title(h)` | plugin title, panel names, signature             | Latin **and Cyrillic** (signature)    | Press Start 2P           |
| `FontText.ttf`  | `pixelfont::vhs(h)`   | parameter names, values, buttons, notes          | Latin, digits, `+ - % .`              | VT323                    |
| `FontKanji.ttf` | `pixelfont::kanji(h)` | the hieroglyphs in the header (currently 灰空)   | the characters used in the header     | DotGothic16 (subset)     |

Notes
* `h` is the **em size in pixels**; `pixelfont::make()` normalises JUCE's ascent+descent height.
* VT323 has no Cyrillic. That is fine today because `FontText` only draws Latin text; if you add
  Cyrillic labels, use a font that has them in this slot.
* `FontKanji.ttf` ships as a 4 KB subset (only 灰 空 間 夜 愛 and basic Latin/digits) so the
  repository does not carry a 2 MB CJK font. To change the hieroglyphs, edit the string in
  `PepelSpaceEditor::paintHeader()` (UTF-8 escapes) and re-cut the font:

```bash
pip install fonttools
# download DotGothic16-Regular.ttf from https://github.com/google/fonts/tree/main/ofl/dotgothic16
pyftsubset DotGothic16-Regular.ttf --text="灰空間夜愛 PEPELSPACE0123456789" \
           --output-file=Assets/FontKanji.ttf --layout-features='' --no-hinting --desubroutinize
```

Licenses of the bundled fonts are in `Assets/licenses/` (SIL Open Font License 1.1).
If you replace a font, replace/remove its license text accordingly.
