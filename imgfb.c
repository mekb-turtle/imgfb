#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
int usage(char* a) {
	fprintf(stderr, "Usage: %s [<framebuffer>] <image file> [<x> <y>]\n", a);
	fprintf(stderr, "framebuffer : framebuffer name, defaults to fb0\n");
	fprintf(stderr, "image file : path to the farbfeld file to draw, - for stdin\n");
	fprintf(stderr, "x y : offset of image to draw, defaults to 0 0\n");
	return 2;
}
void rgba2bgra(uint8_t *pixels, int x, int y) {
    for (int i = 0; i < y * 4; i++) {
        for (int j = 0; j < x; j+=4) {
            uint8_t b = *(pixels + j + (i * x));
            uint8_t r = *(pixels + j + (i * x) + 2);
            *(pixels + j + (i * x)) = r;
            *(pixels + j + (i * x) + 2) = b;
        }
    }
}
int isjpeg(char *file) { /* temporary fast way to determine if jpeg */
    return strstr(file, ".jpg") || strstr(file, ".jpeg");
}
uint8_t *decode_farbfeld(FILE *fp, int *x, int *y) {
    uint8_t *pixels, r, g, b, a;
    int width, height;
#define IMG(x) { if (fgetc(fp) != x) { fprintf(stderr, "Not a valid farbfeld image\n"); return NULL; }}
    IMG('f'); IMG('a'); IMG('r'); IMG('b'); IMG('f'); IMG('e'); IMG('l'); IMG('d');
#define SIZE (fgetc(fp) << 030 | fgetc(fp) << 020 | fgetc(fp) << 010 | fgetc(fp))
    width = SIZE;
    height = SIZE;

    pixels = malloc(width * height * 4);
    *x = width;
    *y = height;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j+=4) {
#define VAL fgetc(fp); fgetc(fp); // discard low byte
			r = VAL;
			g = VAL;
			b = VAL;
			a = VAL;
			*(pixels + j + (i * width)) = b;
			*(pixels + j + (i * width)) = g;
			*(pixels + j + (i * width)) = r;
			*(pixels + j + (i * width)) = a;
		}
	}

    return pixels;
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
        fclose(size);
        free(w_);
        free(h_);
	}
	fb = fopen(fb_, "w");
	if (!fb) ERROR;
	if (strlen(image_) == 1 && image_[0] == '-') {
		image = stdin;
    } else if (isjpeg(image_)) {
        int img_w, img_h, img_bpp;
        unsigned char *pixels = stbi_load(image_, &img_w, &img_h, &img_bpp, STBI_rgb_alpha);
        rgba2bgra(pixels, img_w, img_h);
        for (uint32_t y = 0; y < img_h; y++) {
            if (o_x>=0 && y+o_y>=0 && o_x<fb_w && y+o_y<fb_h) {
                uint32_t j = (o_x + (y+o_y) * fb_w) * 4;
                fseek(fb, j, SEEK_SET);
            }
            fwrite((pixels + (y * img_w * 4)), 1, img_w * 4, fb);
        }
        stbi_image_free(pixels);
    } else {
        int img_w, img_h;
        image = fopen(image_, "r");
        if (!image) ERROR;
	    unsigned char *pixels = decode_farbfeld(image, &img_w, &img_h);
        fclose(image);
        for (uint32_t y = 0; y < img_h; y++) {
            if (o_x>=0 && y+o_y>=0 && o_x<fb_w && y+o_y<fb_h) {
                uint32_t j = (o_x + (y+o_y) * fb_w) * 4;
                fseek(fb, j, SEEK_SET);
            }
            fwrite((pixels + (y * img_w * 4)), 1, img_w * 4, fb);
        }
        free(pixels);
    }
    fclose(fb);
	return 0;
}
