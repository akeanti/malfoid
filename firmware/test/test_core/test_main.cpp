// Host-side unit tests for malfoid_core:  pio test -e native
#include <unity.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include <malfoid_core.h>

using namespace malfoid;

namespace {

Recognizer& builtins() {
  static Recognizer r;
  static bool loaded = false;
  if (!loaded) {
    r.loadBuiltins();
    loaded = true;
  }
  return r;
}

size_t templatePath(const char* id, Vec2* out) {
  for (const auto& def : spellbook::kTemplates) {
    if (std::strcmp(spellbook::kSpells[def.spell].id, id) == 0) {
      for (size_t i = 0; i < def.count; ++i) out[i] = {def.xy[2 * i], def.xy[2 * i + 1]};
      return def.count;
    }
  }
  return 0;
}

Stroke preparedTemplate(const char* id) {
  Vec2 path[64];
  Stroke s;
  prepare(path, templatePath(id, path), s);
  return s;
}

uint32_t rngState = 0x4d414c46;
float uniform(float lo, float hi) {
  rngState = rngState * 1664525u + 1013904223u;
  return lo + (hi - lo) * static_cast<float>(rngState >> 8) / 16777216.0f;
}

// A deliberately sloppy rendition of a template: tilted, stretched, wobbly.
size_t sloppy(const Vec2* path, size_t n, Vec2* out, size_t outCap) {
  const float rot = uniform(-0.25f, 0.25f), sx = uniform(0.75f, 1.3f), sy = uniform(0.75f, 1.3f);
  const float wob = uniform(0.0f, 0.05f), phase = uniform(0.0f, 6.28f);
  size_t m = 0;
  for (size_t seg = 0; seg + 1 < n; ++seg) {
    for (int k = 0; k < 12 && m < outCap; ++k) {
      const float t = k / 12.0f;
      Vec2 p = path[seg] + (path[seg + 1] - path[seg]) * t;
      p = {p.x * sx, p.y * sy};
      p = {p.x * std::cos(rot) - p.y * std::sin(rot), p.x * std::sin(rot) + p.y * std::cos(rot)};
      const float u = static_cast<float>(m) * 0.15f + phase;
      out[m++] = {p.x + wob * std::sin(u) + uniform(-0.01f, 0.01f), p.y + wob * std::cos(u) + uniform(-0.01f, 0.01f)};
    }
  }
  if (m < outCap) out[m++] = path[n - 1];
  return m;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_resample_spacing_and_endpoints() {
  const Vec2 path[] = {{0, 0}, {3, 0}, {3, 4}};  // length 7, uneven segments
  Stroke s;
  TEST_ASSERT_TRUE(resample(path, 3, s));
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, s.pts[0].x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3.0f, s.pts[kStrokePoints - 1].x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f, s.pts[kStrokePoints - 1].y);
  const float step = 7.0f / (kStrokePoints - 1);
  for (size_t i = 1; i < kStrokePoints; ++i) {
    // Points near the corner are closer as the crow flies; never farther.
    TEST_ASSERT_TRUE(distance(s.pts[i - 1], s.pts[i]) <= step + 1e-4f);
  }
}

void test_resample_rejects_degenerate_paths() {
  const Vec2 dot[] = {{1, 1}, {1, 1}, {1, 1}};
  Stroke s;
  TEST_ASSERT_FALSE(resample(dot, 3, s));
  TEST_ASSERT_FALSE(resample(dot, 1, s));
}

void test_normalize_keeps_lines_thin() {
  Stroke s = preparedTemplate("lumen");
  for (const Vec2& p : s.pts) TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, p.x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -0.5f, s.pts[0].y);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, s.pts[kStrokePoints - 1].y);
}

void test_normalize_squares_shapes() {
  const Vec2 wideM[] = {{0, 0}, {0, 1}, {1, 0.35f}, {2, 1}, {2, 0}};  // twice as wide as the template
  Stroke s;
  TEST_ASSERT_TRUE(prepare(wideM, 5, s));
  float minX = 9, maxX = -9, minY = 9, maxY = -9;
  for (const Vec2& p : s.pts) {
    minX = std::fmin(minX, p.x);
    maxX = std::fmax(maxX, p.x);
    minY = std::fmin(minY, p.y);
    maxY = std::fmax(maxY, p.y);
  }
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, maxX - minX);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, maxY - minY);
  const Match m = builtins().classify(s);
  TEST_ASSERT_EQUAL_STRING("malfoid", builtins().at(m.index).id);
}

void test_matches_javascript_twin() {
  // Golden values produced by docs/js/recognizer.js - keeps the website honest.
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.680923f, Recognizer::dtw(preparedTemplate("malfoid"), preparedTemplate("fulgur"), 4));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.364958f, Recognizer::dtw(preparedTemplate("lumen"), preparedTemplate("dextra"), 4));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.659375f, Recognizer::dtw(preparedTemplate("serpens"), preparedTemplate("circulus"), 4));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.516129f, Recognizer::dtw(preparedTemplate("umbra"), preparedTemplate("lumen"), 4));
  const Stroke m = preparedTemplate("malfoid");
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -0.5f, m.pts[5].x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.028558f, m.pts[5].y);
}

void test_every_template_recognizes_itself() {
  const Recognizer& r = builtins();
  TEST_ASSERT_EQUAL(spellbook::kTemplateCount, r.size());
  for (size_t i = 0; i < r.size(); ++i) {
    const Match m = r.classify(r.at(i).stroke);
    TEST_ASSERT_EQUAL_STRING(r.at(i).id, r.at(m.index).id);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, m.score);
    TEST_ASSERT_TRUE(Recognizer::accepts(m));
  }
}

void test_sloppy_casts_are_mostly_recognized() {
  const Recognizer& r = builtins();
  int correct = 0, wrongAccepted = 0, total = 0;
  Vec2 path[64], cast[512];
  for (size_t s = 0; s < spellbook::kSpellCount; ++s) {
    const char* id = spellbook::kSpells[s].id;
    const size_t n = templatePath(id, path);
    for (int trial = 0; trial < 40; ++trial) {
      Stroke stroke;
      TEST_ASSERT_TRUE(prepare(cast, sloppy(path, n, cast, 512), stroke));
      const Match m = r.classify(stroke);
      const bool right = std::strcmp(r.at(m.index).id, id) == 0;
      correct += right;
      wrongAccepted += !right && Recognizer::accepts(m);
      ++total;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(correct >= total * 95 / 100, "top-1 accuracy below 95%");
  TEST_ASSERT_TRUE_MESSAGE(wrongAccepted <= total / 100, "more than 1% accepted as the wrong spell");
}

void test_direction_matters() {
  // The same line drawn the other way round must not score at all.
  const Match m = builtins().classify(preparedTemplate("umbra"));
  TEST_ASSERT_EQUAL_STRING("umbra", builtins().at(m.index).id);
  const float reversed = Recognizer::dtw(preparedTemplate("umbra"), preparedTemplate("lumen"), Recognizer::kDefaultWindow);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, Recognizer::score(reversed));
  TEST_ASSERT_TRUE(m.runnerUpScore < 0.5f);
}

void test_custom_templates_add_and_remove() {
  Recognizer r;
  r.loadBuiltins();
  const size_t before = r.size();
  const Vec2 check[] = {{0, 0.4f}, {0.35f, 0}, {1, 1}};  // a tick mark
  TEST_ASSERT_TRUE(r.addPath("accio", check, 3, true));
  TEST_ASSERT_TRUE(r.addPath("lumen", check, 3, true));
  Stroke s;
  prepare(check, 3, s);
  TEST_ASSERT_TRUE(Recognizer::accepts(r.classify(s)) || std::strcmp(r.at(r.classify(s).index).id, "accio") == 0);
  TEST_ASSERT_EQUAL(1, r.removeCustom("accio"));
  TEST_ASSERT_EQUAL(before + 1, r.size());
  TEST_ASSERT_EQUAL(1, r.removeCustom(nullptr));
  TEST_ASSERT_EQUAL(before, r.size());
}

void test_madgwick_stays_level_at_rest() {
  Madgwick ahrs;
  ahrs.initFromAccel(0, 0, 1);
  for (int i = 0; i < 2000; ++i) ahrs.update(0, 0, 0, 0, 0, 1, 1.0f / 208);
  const Vec3 tip = tipDirection(ahrs.orientation());
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, tip.x);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, tip.z);
}

void test_madgwick_levels_from_tilted_accel() {
  Madgwick ahrs;
  const float pitch = 0.5f;  // tip raised ~29 degrees: the accelerometer's "up" leans onto +X
  ahrs.initFromAccel(std::sin(pitch), 0, std::cos(pitch));
  const Sky sky = toSky(tipDirection(ahrs.orientation()));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, pitch, sky.elevation);
}

void test_turning_left_draws_left_and_raising_draws_up() {
  Madgwick ahrs;
  ahrs.initFromAccel(0, 0, 1);
  StrokeRecorder rec;
  rec.begin(ahrs.orientation());
  for (int i = 0; i < 52; ++i) {  // 0.25 s at +2 rad/s about world-up = turn left
    ahrs.update(0, 0, 2.0f, 0, 0, 1, 1.0f / 208);
    rec.add(ahrs.orientation());
  }
  TEST_ASSERT_TRUE(rec.points()[rec.size() - 1].x < -0.4f);

  Madgwick ahrs2;
  ahrs2.initFromAccel(0, 0, 1);
  rec.begin(ahrs2.orientation());
  for (int i = 0; i < 52; ++i) {  // negative pitch rate raises the tip
    ahrs2.update(0, -2.0f, 0, 0, 0, 0, 1.0f / 208);  // accel far from 1 g mid-flick: gyro only
    rec.add(ahrs2.orientation());
  }
  TEST_ASSERT_TRUE(rec.points()[rec.size() - 1].y > 0.4f);
}

void test_flick_up_with_rolled_wrist_casts_lumen() {
  // Wrist rolled 70 degrees: the raw gyro axes are scrambled, the tip path is not.
  const float roll = 1.22f;
  Madgwick ahrs;
  ahrs.initFromAccel(0, std::sin(roll), std::cos(roll));
  StrokeRecorder rec;
  rec.begin(ahrs.orientation());
  for (int i = 0; i < 60; ++i) {
    // "raise the tip" expressed in the rolled body frame
    const float rate = -3.0f;
    ahrs.update(0, rate * std::cos(roll), -rate * std::sin(roll), 0, 0, 0, 1.0f / 208);
    rec.add(ahrs.orientation());
  }
  Stroke s;
  TEST_ASSERT_TRUE(prepare(rec.points(), rec.size(), s));
  const Match m = builtins().classify(s);
  TEST_ASSERT_EQUAL_STRING("lumen", builtins().at(m.index).id);
  TEST_ASSERT_TRUE(Recognizer::accepts(m));
}

void test_gyro_bias_is_learned_while_still() {
  GyroBias bias;
  const Vec3 offset{0.02f, -0.015f, 0.01f};
  for (int i = 0; i < 260; ++i) bias.update(offset, Vec3{0, 0, 1}, 1.0f / 208);
  TEST_ASSERT_TRUE(bias.settled());
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, offset.x, bias.bias().x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, offset.y, bias.bias().y);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, offset.z, bias.bias().z);

  GyroBias moving;
  for (int i = 0; i < 400; ++i) moving.update(Vec3{1.0f, 0, 0}, Vec3{0, 0, 1}, 1.0f / 208);
  TEST_ASSERT_FALSE(moving.settled());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_resample_spacing_and_endpoints);
  RUN_TEST(test_resample_rejects_degenerate_paths);
  RUN_TEST(test_normalize_keeps_lines_thin);
  RUN_TEST(test_normalize_squares_shapes);
  RUN_TEST(test_matches_javascript_twin);
  RUN_TEST(test_every_template_recognizes_itself);
  RUN_TEST(test_sloppy_casts_are_mostly_recognized);
  RUN_TEST(test_direction_matters);
  RUN_TEST(test_custom_templates_add_and_remove);
  RUN_TEST(test_madgwick_stays_level_at_rest);
  RUN_TEST(test_madgwick_levels_from_tilted_accel);
  RUN_TEST(test_turning_left_draws_left_and_raising_draws_up);
  RUN_TEST(test_flick_up_with_rolled_wrist_casts_lumen);
  RUN_TEST(test_gyro_bias_is_learned_while_still);
  return UNITY_END();
}
