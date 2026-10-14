# Mechanical

[`malfoid_wand.scad`](malfoid_wand.scad) is a parametric OpenSCAD model of the wand body: a 170 mm handle, a 190 mm tapered shaft, a pommel cap with a USB-C window, and a faceted emerald over the thumb pad.

```bash
openscad -D 'part="handle"' -o handle.stl malfoid_wand.scad
openscad -D 'part="shaft"'  -o shaft.stl  malfoid_wand.scad
openscad -D 'part="cap"'    -o cap.stl    malfoid_wand.scad
openscad -D 'part="gem"'    -o gem.stl    malfoid_wand.scad
```

| Part | Material | Orientation | Notes |
|---|---|---|---|
| Handle | PETG, black or deep green | standing, pommel down | 0.2 mm layers; the bore needs no supports |
| Shaft | PETG, silver or wood-fill | standing, tenon down | brim recommended, it's tall and thin |
| Cap | PETG, silver | skirt down (as exported) | the face bridges about 25 mm |
| Gem | translucent green PETG/resin | flat side down | 100% infill so it glows |

Assembly, pommel → tip: the board slides in from the pommel with USB-C facing out. The cell follows on path B. The 3 mm PMMA fibre runs from the tip LED through the end wall and up the shaft, then the shaft's tenon presses into the socket. Glue the gem over the thumb pad, then slide on the cap.

Measure your own board and cell, and adjust `bore_d`, `bore_len` and `gem_z` at the top of the file. Everything else follows from those.
