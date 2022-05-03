#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
int usage(char* a) {
	fprintf(stderr, "Usage: %s [<framebuffer>] <image file> [<x> <y>]\n", a);
	fprintf(stderr, "framebuffer : framebuffer name, defaults to fb0\n");
	fprintf(stderr, "image file : path to the farbfeld file to draw, - for stdin\n");
	fprintf(stderr, "x y : offset of image to draw, defaults to 0 0\n");
	return 2;
}
int main(int argc, char* argv[]) {
#define INVALID { return usage(argv[0]); }
	if (argc < 2 || argc > 5) INVALID;
	FILE* fb;
	char* fb_;
	char* size_;
	uint32_t o_x;
	uint32_t o_y;
	FILE* image;
	char* image_ = "";
	{
		char* fb__ = "fb0";
		char* x_ = "0";
		char* y_ = "0";
		if (argc == 3 || argc == 5) { fb__ = argv[1]; }
		if (argc == 2 || argc == 4) { image_ = argv[1]; }
		if (argc == 3 || argc == 5) { image_ = argv[2]; }
		if (argc == 4) { x_ = argv[2]; y_ = argv[3]; }
		if (argc == 5) { x_ = argv[3]; y_ = argv[4]; }
		if (strlen(x_) == 0) INVALID;
		if (strlen(y_) == 0) INVALID;
		if (strlen(x_) > (x_[0] == '-' ? 7 : 6)) INVALID; // too high of a number causes weirdness
		if (strlen(y_) > (y_[0] == '-' ? 7 : 6)) INVALID;
		if (strlen(image_) == 0) INVALID;
		if (strlen(fb__) == 0) INVALID;
#define NUMERIC(x) (x>='0' && x<='9')
#define ALPHABETIC(x) (x>='a' && x<='z')
#define ALPHANUMERIC(x) (ALPHABETIC(x) || NUMERIC(x))
#define V(x) for (size_t i = 0; i < strlen(x); ++i) { if (!NUMERIC(x[i]) && !(i == 0 && x[0] == '-')) INVALID; }
		for (size_t i = 0; i < strlen(fb__); ++i) {
			if (!ALPHANUMERIC(fb__[i])) INVALID; }
		V(x_); V(y_);
#undef V
		fb_ = malloc(strlen(fb__) + 10);
		sprintf(fb_, "/dev/%s", fb__);
		size_ = malloc(strlen(fb__) + 50);
		sprintf(size_, "/sys/class/graphics/%s/virtual_size", fb__);
		o_x = atoi(x_);
		o_y = atoi(y_);
	}
	uint32_t fb_w;
	uint32_t fb_h;
#define ERROR { fprintf(stderr, "%s\n", strerror(errno)); return errno; }
	{
		FILE* size = fopen(size_, "r");
		if (!size) ERROR;
		size_t i = 0;
		size_t j = 0;
		uint8_t b = 0;
		char* w_ = malloc(32);
		char* h_ = malloc(32);
		while (1) {
			char c = getc(size);
			if (NUMERIC(c)) { if (b) h_[i++] = c; else w_[i++] = c;
			} else if (!b) { b = 1; i = 0; } else break;
		}
		fb_w = atoi(w_);
		fb_h = atoi(h_);
	}
	fb = fopen(fb_, "w");
	if (!fb) ERROR;
	if (strlen(image_) == 1 && image_[0] == '-') {
		image = stdin;
	} else {
		image = fopen(image_, "r");
	}
	if (!image) ERROR;
#define IMG(x) { if (fgetc(image) != x) { fprintf(stderr, "Not a valid farbfeld image\n"); return 3; }}
	IMG('f'); IMG('a'); IMG('r'); IMG('b'); IMG('f'); IMG('e'); IMG('l'); IMG('d');
#define SIZE (fgetc(image) << 030 | fgetc(image) << 020 | fgetc(image) << 010 | fgetc(image))
	uint32_t img_w = SIZE;
	uint32_t img_h = SIZE;
	uint32_t i = 0;
	for (uint32_t y = 0; y < img_h; ++y) {
		for (uint32_t x = 0; x < img_w; ++x) {
#define VAL fgetc(image); fgetc(image); // discard low byte
			uint8_t r = VAL;
			uint8_t g = VAL;
			uint8_t b = VAL;
			uint8_t a = VAL;
			if (a == 0) continue;
			if (x+o_x>=0 && y+o_y>=0 && x+o_x<fb_w && y+o_y<fb_h) {
				uint32_t j = ((x+o_x) + (y+o_y) * fb_w) * 4;
				if (i != j) fseek(fb, j, SEEK_SET);
				fputc(b, fb);
				fputc(g, fb);
				fputc(r, fb);
				fputc(a, fb);
				i += 4;
			}
		}
	}
	return 0;
}
