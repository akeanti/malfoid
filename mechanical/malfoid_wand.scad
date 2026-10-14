// Malfoid wand body - parametric, print-in-place-free, PETG recommended.
//
// Render one part at a time for printing:
//   openscad -D 'part="handle"' -o handle.stl malfoid_wand.scad
//   parts: "handle", "shaft", "cap", "gem", "all" (assembled preview)
//
// Handle layout, pommel -> tip: USB-C behind the cap window -> Core board
// (or XIAO + breakouts) -> antenna gap -> 14500 cell beside the light fibre.

part = "all";

/* [Handle] */
handle_len   = 170;   // pommel face to shaft joint
bore_d       = 18.8;  // fits the 18 mm Core board edge-on, and a 14500 cell + 3 mm fibre
bore_len     = 162;
cap_seat_d   = 25;    // the cap's skirt slides over this
grip_d       = 23;
neck_d       = 21.6;

/* [Thumb pad gem] */
gem_z        = 85;    // along the handle, over the board's touch pad
gem_w        = 9;
gem_stretch  = 1.6;   // gem is gem_w wide, gem_w * gem_stretch long
gem_floor    = 1.0;   // plastic left between the gem and the touch pad

/* [Shaft] */
shaft_len    = 190;
shaft_d0     = 13;
shaft_d1     = 7;
fibre_d      = 3.4;   // 3 mm PMMA fibre + clearance
tenon_d      = 8;
tenon_len    = 6;

/* [Fit] */
clearance    = 0.2;

$fn = 96;

module handle() {
  difference() {
    rotate_extrude() polygon([
      [0, 0], [cap_seat_d / 2, 0], [cap_seat_d / 2, 6],          // cap seat
      [13.2, 7], [13.2, 10], [12.2, 12],                          // pommel flare
      [grip_d / 2, 14], [grip_d / 2, 22], [12.3, 23.5], [grip_d / 2, 25],  // silver ring
      [grip_d / 2 - 0.1, 60], [11.3, 130],
      [12.0, 132], [11.3, 134],                                   // second ring
      [neck_d / 2, 158], [11.6, 162], [11.6, 166], [10.4, handle_len],
      [0, handle_len]
    ]);
    // board + cell bore, open at the pommel
    translate([0, 0, -1]) cylinder(d = bore_d, h = bore_len + 1);
    // light fibre through the end wall
    cylinder(d = fibre_d, h = handle_len + 1);
    // shaft socket
    translate([0, 0, handle_len - tenon_len]) cylinder(d = tenon_d + 2 * clearance, h = tenon_len + 1);
    // gem pocket, thinned down to gem_floor above the bore
    translate([0, bore_d / 2 + gem_floor, gem_z]) rotate([-90, 0, 0])
      scale([1, gem_stretch, 1]) cylinder(d = gem_w + 2 * clearance, h = 10);
  }
}

module shaft() {
  difference() {
    union() {
      translate([0, 0, -tenon_len]) cylinder(d = tenon_d, h = tenon_len + 0.01);
      rotate_extrude() polygon(concat(
        [[0, 0], [shaft_d0 / 2 + 0.6, 0], [shaft_d0 / 2 + 0.6, 2.5], [shaft_d0 / 2, 4]],
        // a few hawthorn-ish knots along an otherwise smooth taper
        [for (i = [1 : 5]) let(z = shaft_len * i / 7, r = shaft_d0 / 2 - (shaft_d0 - shaft_d1) / 2 * z / shaft_len)
          each [[r, z - 2], [r + 0.5, z], [r, z + 2]]],
        [[shaft_d1 / 2, shaft_len - 3], [shaft_d1 / 2 - 0.8, shaft_len], [0, shaft_len]]
      ));
    }
    translate([0, 0, -tenon_len - 1]) cylinder(d = fibre_d, h = shaft_len + tenon_len + 2);
  }
}

module cap() {
  // Thin face so a USB-C plug seats fully; the skirt slides over the cap seat.
  face = 1.6;
  skirt = 6;
  difference() {
    union() {
      cylinder(d = cap_seat_d + 2 * clearance + 3.2, h = face + skirt);
      translate([0, 0, -1.2]) cylinder(d1 = cap_seat_d - 4, d2 = cap_seat_d + 3, h = 1.2);  // domed face
    }
    translate([0, 0, face]) cylinder(d = cap_seat_d + 2 * clearance, h = skirt + 1);
    // USB-C window, 9.4 x 3.6 mm with rounded ends
    hull() for (x = [-(9.4 - 3.6) / 2, (9.4 - 3.6) / 2]) translate([x, 0, -2]) cylinder(d = 3.6, h = face + 3);
  }
}

module gem() {
  // Faceted cabochon. Print in translucent green, 100% infill, so the
  // emerald catches the tip light.
  scale([1, gem_stretch, 1]) {
    translate([0, 0, -1.5]) cylinder(d = gem_w, h = 1.5);
    cylinder(d1 = gem_w, d2 = gem_w * 0.62, h = 3.2, $fn = 8);
  }
}

if (part == "handle") handle();
else if (part == "shaft") shaft();
else if (part == "cap") rotate([180, 0, 0]) cap();
else if (part == "gem") gem();
else {
  color("#1b1f1d") handle();
  color("#c9d1d9") translate([0, 0, handle_len]) shaft();
  color("#c9d1d9") translate([0, 0, -1.6]) cap();  // face below the pommel, skirt over the seat
  color("#2fd38a", 0.85) translate([0, bore_d / 2 + gem_floor, gem_z]) rotate([-90, 0, 0]) translate([0, 0, 1.5]) gem();
}
