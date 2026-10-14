# Power budget

All figures are **estimates from datasheet typicals**, good enough for design decisions. Measure your own build (a Nordic PPK2 or a µCurrent beats a multimeter here) and send a PR with real numbers.

## States

| State | What's running | Path B (Core) | Path A (XIAO + breakouts) |
|---|---|---|---|
| **Casting** | CPU, IMU 208 Hz, LED bright, LRA pulses, Wi-Fi TX burst | 80–150 mA, peaks ~350 mA during TX | similar |
| **Awake, idle** | CPU (loop sleeps 5 ms per tick), IMU 208 Hz, Wi-Fi modem-sleep, dim LED breathing | ~45–70 mA | ~50–75 mA |
| **Deep sleep** | RTC only; IMU accel at 26 Hz low-power with motion wake | **~40–60 µA** | ~1–3 mA; ~0.3 mA with breakout LEDs cut |

### Deep-sleep breakdown (path B)

| Part | State | Estimate |
|---|---|---|
| ESP32-S3 | deep sleep, ext1 wake (RTC peripherals powered down) | ~8–10 µA |
| LSM6DSOX | accelerometer low-power 26 Hz, gyro off, wake-up engine on | ~10–30 µA |
| TPS63802 | quiescent, light load | ~11 µA |
| MAX17048 | hibernate (automatic when the cell is idle) | ~3–4 µA |
| DRV2605L | EN low (shutdown) | a few µA |
| MCP73831 | no VBUS, battery reverse leakage | < 2 µA |
| SK6812 | **rail switched off** (would idle at ~1 mA otherwise) | 0 |
| **Total** | | **≈ 40–60 µA** |

That LED rail switch matters: one dark SK6812 would draw about 20× the rest of the board combined.

## Battery life

Usage model: the wand is picked up a few times a day and awake for **10 minutes/day** in total (sleep timeout 2 min).

```
awake:  10 min/day × ~60 mA          ≈ 10 mAh/day
sleep:  23.8 h/day × 0.05 mA         ≈  1.2 mAh/day      (path B)
        23.8 h/day × 2 mA            ≈ 48   mAh/day      (path A, LEDs intact)
        23.8 h/day × 0.3 mA          ≈  7   mAh/day      (path A, LEDs cut)
```

| Build | Cell | Daily use | Runtime |
|---|---|---|---|
| Path B | 14500, 800 mAh (use ~85%) | ~11 mAh | **~9 weeks** |
| Path A, LEDs intact | 450 mAh LiPo | ~58 mAh | ~6–7 days |
| Path A, LEDs cut | 450 mAh LiPo | ~17 mAh | ~3 weeks |

Path B left on a shelf: 680 mAh / 0.05 mA ≈ 13,600 h, about 1.5 years, so the cell's self-discharge wins long before the electronics do.

## Where the remaining milliamps go (ideas for rev B)

- **Light sleep between IMU samples.** The ESP32-S3 can enter automatic light sleep with the IMU FIFO buffering ~100 ms of samples, which could take the awake-idle current from ~60 mA toward ~10–15 mA.
- **Wi-Fi only when casting.** BLE advertising to a Home Assistant Bluetooth proxy would avoid keeping Wi-Fi associated. The trade-off is that casts land ~1 s later.
- **LSM6DSOX machine-learning core.** It could classify "picked up" vs "bumped on the table" on-chip and cut false wake-ups.
