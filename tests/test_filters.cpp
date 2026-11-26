// Unit and golden tests for the header-only image code. The images are built
// in memory (or written as small PNGs) by this file, so no course image is
// needed.
//
//   test_filters                 run every test (writes files in the cwd)
//   test_filters --fixtures DIR  write the four batch inputs to DIR
//   test_filters --same A B      exit 0 if two PNGs decode to the same pixels
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <png.h>
#include "../functions_png.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

// Allocate the global buffers for a w x h RGB image, the way load_file does,
// with every pixel set to f(x, y) in both row_pointers and write_rp.
template <class F>
static void make_image(int w, int h, F f) {
  *width = w; *height = h; size_pixel = 3; color_type = PNG_COLOR_TYPE_RGB; bit_depth = 8;
  row_pointers = (png_byte**)malloc(h * sizeof(png_byte*));
  write_rp = (png_byte**)malloc(h * sizeof(png_byte*));
  for (int y = 0; y < h; y++) {
    row_pointers[y] = (png_byte*)calloc(w * 3, 1);
    write_rp[y] = (png_byte*)calloc(w * 3, 1);
    for (int x = 0; x < w; x++) {
      int v = f(x, y);
      for (int c = 0; c < 3; c++) row_pointers[y][x * 3 + c] = write_rp[y][x * 3 + c] = v;
    }
  }
}
static int out(int x, int y) { return write_rp[y][x * size_pixel]; }

// ------------------------------------------------------------------ FFT
static void test_fft_known_answers() {
  cpx impulse[8] = {1}, y[8];
  fft(impulse, y, 1, 8, 1);
  for (int k = 0; k < 8; k++) CHECK(std::abs(y[k] - cpx(1, 0)) < 1e-12);   // flat spectrum

  cpx x[4] = {1, 2, 3, 4}, z[4];
  fft(x, z, 1, 4, 1);
  CHECK(std::abs(z[0] - cpx(10, 0)) < 1e-12);
  CHECK(std::abs(z[1] - cpx(-2, 2)) < 1e-12);
  CHECK(std::abs(z[2] - cpx(-2, 0)) < 1e-12);
  CHECK(std::abs(z[3] - cpx(-2, -2)) < 1e-12);

  cpx dc[16], s[16];   // constant 3 -> 3*N at the origin, 0 elsewhere
  for (int i = 0; i < 16; i++) dc[i] = 3;
  fft2d(dc, s, 4, 1);
  CHECK(std::abs(s[0] - cpx(48, 0)) < 1e-9);
  for (int i = 1; i < 16; i++) CHECK(std::abs(s[i]) < 1e-9);
}

static void test_fft2d_round_trip() {
  const int N = 16;
  std::vector<cpx> a(N * N), f(N * N), b(N * N);
  srand(7);
  for (int i = 0; i < N * N; i++) a[i] = cpx(rand() % 256, 0);
  fft2d(&a[0], &f[0], N, 1);
  fft2d(&f[0], &b[0], N, -1);
  double err = 0;
  for (int i = 0; i < N * N; i++) err = std::max(err, std::abs(a[i] - b[i]));
  CHECK(err < 1e-9);
}

// The image-level pair: to_fourier pads a 13x10 image to 16x16, and
// fourier_to_image must give back the same pixels.
static void test_image_fourier_round_trip() {
  make_image(13, 10, [](int x, int y) { return (x * 37 + y * 91) % 256; });
  std::vector<int> before;
  for (int y = 0; y < 10; y++) for (int x = 0; x < 13; x++) before.push_back(out(x, y));
  to_fourier();
  CHECK(maxn == 16);
  fourier_to_image();
  int diff = 0;
  for (int y = 0, i = 0; y < 10; y++) for (int x = 0; x < 13; x++, i++) diff = std::max(diff, std::abs(out(x, y) - before[i]));
  CHECK(diff <= 1);   // int() truncation of x.999999
}

// ------------------------------------------------------------- filters
// A vertical step edge: 0 on columns 0..7, 50 on columns 8..15.
static int step(int x, int) { return x < 8 ? 0 : 50; }

static void test_sobel_step_edge() {
  make_image(16, 16, step);
  filter_sobel();
  // the 3x3 window straddles the edge when centred on column 7 or 8:
  // |Gx| = (1+2+1) * 50 = 200, Gy = 0
  for (int y = 1; y < 15; y++)
    for (int x = 1; x < 15; x++)
      CHECK(out(x, y) == ((x == 7 || x == 8) ? 200 : 0));
}

static void test_laplace_step_edge() {
  make_image(16, 16, step);
  filter_laplace();
  // column 7: 8*0 - 3*50 < 0 -> 0; column 8: 8*50 - 5*50 = 150 (three neighbours are 0)
  for (int y = 1; y < 15; y++)
    for (int x = 1; x < 15; x++)
      CHECK(out(x, y) == (x == 8 ? 150 : 0));
}

static void test_median_removes_salt() {
  make_image(9, 9, [](int x, int y) { return (x == 4 && y == 4) ? 255 : 30; });
  filter_mediana(3);
  for (int y = 1; y < 8; y++) for (int x = 1; x < 8; x++) CHECK(out(x, y) == 30);
}

static void test_mean_keeps_constant() {
  make_image(8, 8, [](int, int) { return 77; });
  filter_media(3, 1);
  for (int y = 1; y < 7; y++) for (int x = 1; x < 7; x++) CHECK(out(x, y) == 77);
}

static void test_point_operations() {
  make_image(4, 1, [](int x, int) { return x * 80; });   // 0 80 160 240
  to_negative();
  CHECK(out(0, 0) == 255 && out(1, 0) == 175 && out(3, 0) == 15);
  make_image(4, 1, [](int x, int) { return x * 80; });
  to_white_and_black(128);
  CHECK(out(0, 0) == 0 && out(1, 0) == 0 && out(2, 0) == 255 && out(3, 0) == 255);
}

// ---------------------------------------------------------- morphology
// dilate/erosion treat black (0) as the object on a white background.
static void test_dilate_and_erode() {
  int mask[9];
  make_image(9, 9, [](int x, int y) { return (x == 4 && y == 4) ? 0 : 255; });
  dilate(mask, 3, 3);
  for (int y = 0; y < 9; y++)
    for (int x = 0; x < 9; x++)
      CHECK(out(x, y) == ((std::abs(x - 4) <= 1 && std::abs(y - 4) <= 1) ? 0 : 255));

  make_image(9, 9, [](int x, int y) { return (std::abs(x - 4) <= 1 && std::abs(y - 4) <= 1) ? 0 : 255; });
  erosion(mask, 3, 3);
  for (int y = 0; y < 9; y++)
    for (int x = 0; x < 9; x++)
      CHECK(out(x, y) == ((x == 4 && y == 4) ? 0 : 255));
}

// ------------------------------------------------------- CSV exports
static std::vector<std::string> read_lines(const char* f) {
  std::vector<std::string> v;
  std::ifstream in(f);
  std::string s;
  while (std::getline(in, s)) v.push_back(s);
  return v;
}

// Every pixel pure red: 64 pixels at R=255 and 64 at G=0, B=0.
static void test_histogram_counts_255() {
  make_image(8, 8, [](int, int) { return 0; });
  for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) row_pointers[y][x * 3] = 255;
  print_histogram("hist.csv");
  std::vector<std::string> l = read_lines("hist.csv");
  CHECK(l.size() == 257);
  if (l.size() != 257) return;
  CHECK(l[1] == "0; 0; 64; 64");      // intensity 0: no red, all green and blue
  CHECK(l[256] == "255; 64; 0; 0");   // intensity 255: all red
}

// Hue of the primaries is 0, 2pi/3 and 4pi/3; saturation 1; intensity 1/3.
static void test_hsi_of_primaries() {
  make_image(3, 1, [](int, int) { return 0; });
  for (int c = 0; c < 3; c++) row_pointers[0][c * 3 + c] = 255;   // red, green, blue
  print_hsi("hsi.csv");
  std::vector<std::string> l = read_lines("hsi.csv");
  CHECK(l.size() == 4);
  if (l.size() != 4) return;
  const double tp = 2 * acos(-1.0) / 3;
  for (int c = 0; c < 3; c++) {
    double H, S, I;
    CHECK(sscanf(l[c + 1].c_str(), "%lf; %lf; %lf", &H, &S, &I) == 3);
    CHECK(std::fabs(H - c * tp) < 1e-4);
    CHECK(std::fabs(S - 1) < 1e-9 && std::fabs(I - 1.0 / 3) < 1e-4);
  }
}

// ----------------------------------------------------------- PNG I/O
static void test_png_round_trip() {
  make_image(5, 3, [](int x, int y) { return x * 40 + y * 10; });
  write_rp[1][2 * 3 + 0] = 200; write_rp[1][2 * 3 + 1] = 100; write_rp[1][2 * 3 + 2] = 50;
  write_png_file("roundtrip.png");
  CHECK(load_file("roundtrip.png"));
  CHECK(*width == 5 && *height == 3 && size_pixel == 3);
  // load_file stores rows bottom-up; write_rp holds the copy as read
  bool same = true;
  for (int y = 0; y < 3; y++) for (int x = 0; x < 5; x++) {
    int want = x * 40 + y * 10;
    if (y == 1 && x == 2) { if (write_rp[1][6] != 200 || write_rp[1][7] != 100 || write_rp[1][8] != 50) same = false; }
    else if (write_rp[y][x * 3] != want) same = false;
  }
  CHECK(same);
  CHECK(!load_file("does-not-exist.png"));
}

// sum() adds the red channel of a PNG to the image being edited (clamped).
static void test_sum_accumulates() {
  make_image(4, 4, [](int x, int) { return x * 50; });   // 0 50 100 150
  write_png_file("addend.png");
  make_image(4, 4, [](int, int y) { return y * 40; });   // 0 40 80 120
  sum("addend.png");
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      CHECK(out(x, y) == std::min(255, x * 50 + y * 40));
  sum("addend.png");                                    // and again
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      CHECK(out(x, y) == std::min(255, 2 * x * 50 + y * 40));
}

// ------------------------------------------------- batch helper modes
static int write_fixtures(const std::string& dir) {
  struct { const char* name; int (*f)(int, int); } in[] = {
    {"camahorizontal.png", [](int, int y) { return y < 8 ? 200 : 60; }},
    {"camavertical.png",   [](int x, int)  { return x < 8 ? 10 : 0; }},
    {"cama45p.png",        [](int x, int y) { return x == y ? 20 : 0; }},
    {"cama45n.png",        [](int x, int y) { return x + y == 15 ? 30 : 0; }},
  };
  for (int i = 0; i < 4; i++) { make_image(16, 16, in[i].f); write_png_file(dir + "/" + in[i].name); }
  return 0;
}

static int same_pixels(const char* a, const char* b) {
  if (!load_file(a)) return 2;
  int w = *width, h = *height;
  std::vector<int> pa;
  for (int y = 0; y < h; y++) for (int x = 0; x < w * size_pixel; x++) pa.push_back(row_pointers[y][x]);
  if (!load_file(b)) return 2;
  if (w != *width || h != *height) { std::printf("size differs\n"); return 1; }
  for (int y = 0, i = 0; y < h; y++) for (int x = 0; x < w * size_pixel; x++, i++)
    if (pa[i] != row_pointers[y][x]) { std::printf("pixel differs at byte %d of row %d\n", x, y); return 1; }
  return 0;
}

int main(int argc, char** argv) {
  if (argc == 3 && !strcmp(argv[1], "--fixtures")) return write_fixtures(argv[2]);
  if (argc == 4 && !strcmp(argv[1], "--same")) return same_pixels(argv[2], argv[3]);
  test_fft_known_answers();
  test_fft2d_round_trip();
  test_image_fourier_round_trip();
  test_sobel_step_edge();
  test_laplace_step_edge();
  test_median_removes_salt();
  test_mean_keeps_constant();
  test_point_operations();
  test_dilate_and_erode();
  test_histogram_counts_255();
  test_hsi_of_primaries();
  test_png_round_trip();
  test_sum_accumulates();
  if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
  std::printf("all filter tests passed\n");
  return 0;
}
