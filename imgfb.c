#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int usage(char *argv0) {
	fprintf(stderr, "Usage: %s [<framebuffer>] <image file> [<x> <y>]\n", argv0);
	fprintf(stderr, "framebuffer : framebuffer name, defaults to fb0\n");
	fprintf(stderr, "image file : path to the image file to draw or - for stdin, can either be farbfeld or jpeg\n");
	fprintf(stderr, "x y : offset of image to draw, defaults to 0 0\n");
	return 2;
}

void rgba2bgra(uint8_t *pixels, int width, int height, int bpp) {
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			long i = (x + (y * width)) * bpp;
			uint8_t b = pixels[i + 0];
			uint8_t r = pixels[i + 2];
			pixels[i + 0] = r;
			pixels[i + 2] = b;
		}
	}
}
bool isfarbfeld(FILE *fp) {
	char buff[9];
	fread(buff, 1, 8, fp);
	fseek(fp, 0, SEEK_SET);
	buff[8] = 0;
	return strncmp(buff, "farbfeld", 8) == 0;
}
bool isjpeg(FILE *fp) {
	char buff[4];
	fread(buff, 1, 3, fp);
	fseek(fp, 0, SEEK_SET);
	buff[3] = 0;
	return strncmp(buff, "\xFF\xD8\xFF", 3) == 0;
}
uint8_t *decode_farbfeld(FILE *fp, int *width_, int *height_) {
	fseek(fp, 8, SEEK_SET);
#define SIZE (fgetc(fp) << 030 | fgetc(fp) << 020 | fgetc(fp) << 010 | fgetc(fp))
	int width = SIZE;
	int height = SIZE;
#undef SIZE
	uint8_t *pixels;
	pixels = malloc(width * height * 4);
	*width_ = width;
	*height_ = height;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
#define VAL fgetc(fp); fgetc(fp); // discard low byte
			long i = (x + (y * width)) * 4;
			pixels[i + 2] = VAL; // R
			pixels[i + 1] = VAL; // G
			pixels[i + 0] = VAL; // B
			pixels[i + 3] = VAL; // A
#undef VAL
		}
	}
	return pixels;
}

int main(int argc, char* argv[]) {
#define INVALID { return usage(argv[0]); }
	if (argc < 2 || argc > 5) INVALID;
	char* fb_path;
	char* size_path;
	char* image_path;
	int offset_x;
	int offset_y;
	{
		char* fb_ = "fb0";
		char* x_ = "0";
		char* y_ = "0";
		if (argc == 3 || argc == 5) { fb_ = argv[1]; }
		if (argc == 2 || argc == 4) { image_path = argv[1]; }
		if (argc == 3 || argc == 5) { image_path = argv[2]; }
		if (argc == 4) { x_ = argv[2]; y_ = argv[3]; }
		if (argc == 5) { x_ = argv[3]; y_ = argv[4]; }
		if (strlen(x_) == 0) INVALID;
		if (strlen(y_) == 0) INVALID;
		if (strlen(x_) > (x_[0] == '-' ? 7 : 6)) INVALID; // too high of a number causes weirdness
		if (strlen(y_) > (y_[0] == '-' ? 7 : 6)) INVALID;
		if (strlen(image_path) == 0) INVALID;
		if (strlen(fb_) == 0) INVALID;
#define NUMERIC(x) (x>='0'&&x<='9')
#define ALPHABETIC(x) (x>='a'&&x<='z')
#define ALPHANUMERIC(x) (ALPHABETIC(x) || NUMERIC(x))
		for (size_t i = 0; i < strlen(fb_); ++i) { if (!ALPHANUMERIC(fb_[i])) INVALID; }
		for (size_t i = 0; i < strlen(x_); ++i) { if (!NUMERIC(x_[i]) && !(i == 0 && x_[0] == '-')) INVALID; }
		for (size_t i = 0; i < strlen(y_); ++i) { if (!NUMERIC(y_[i]) && !(i == 0 && y_[0] == '-')) INVALID; }
		fb_path = malloc(strlen(fb_) + 10);
		sprintf(fb_path, "/dev/%s", fb_);
		size_path = malloc(strlen(fb_) + 50);
		sprintf(size_path, "/sys/class/graphics/%s/virtual_size", fb_);
		offset_x = atoi(x_);
		offset_y = atoi(y_);
	}
#undef INVALID

#define ERROR { fprintf(stderr, "%s\n", strerror(errno)); return errno; }
	int fb_w;
	int fb_h;
	{
		FILE* size = fopen(size_path, "r");
		free(size_path);
		if (!size) ERROR;
		size_t i = 0;
		uint8_t b = 0;
		char* w_ = malloc(32);
		char* h_ = malloc(32);
		while (1) {
			char c = getc(size);
			if (NUMERIC(c)) {
				if (b) { h_[i++] = c; } else { w_[i++] = c; }
			} else {
				if (!b) { b = 1; i = 0; } else { break; }
			}
		}
		fb_w = atoi(w_);
		fb_h = atoi(h_);
		fclose(size);
		free(w_);
		free(h_);
	}

	FILE* fb;
	fb = fopen(fb_path, "w");
	free(fb_path);
	if (!fb) ERROR;

	FILE* image;
	if (strlen(image_path) == 1 && *image_path == '-') {
		image = stdin;
		fprintf(stderr, "stdin currently doesn't work");
		return;
	} else {
		image = fopen(image_path, "r");
	}
	//free(image_path); // this crashes for some reason
	if (!image) ERROR;

	int image_w, image_h;
	uint8_t *pixels;
	bool isfarbfeld_ = isfarbfeld(image);
	bool isjpeg_ = isjpeg(image);
	printf("%i %i",isfarbfeld_,isjpeg_);
	if (isfarbfeld_) {
		pixels = decode_farbfeld(image, &image_w, &image_h);
		if (!pixels) return 1;
	} else if (isjpeg_) {
		int image_bpp;
		pixels = stbi_load_from_file(image, &image_w, &image_h, &image_bpp, STBI_rgb_alpha);
		if (!pixels) return 1;
		rgba2bgra(pixels, image_w, image_h, image_bpp);
	} else {
		fprintf(stderr, "Image isn't farbfeld or jpeg\n");
		fclose(image);
		return 1;
	}
	fclose(image);

	for (int y = 0; y < image_h; ++y) {
		if (offset_x>=0 && y+offset_y>=0 && offset_x<fb_w && y+offset_y<fb_h) {
			int j = (offset_x + (y+offset_y) * fb_w) * 4;
			fseek(fb, j, SEEK_SET);
		}
		fwrite((pixels + (y * image_w * 4)), 1, image_w * 4, fb);
	}
	fclose(fb);
	if (isfarbfeld_) {
		free(pixels);
	} else if (isjpeg_) {
		stbi_image_free(pixels);
	}

	return 0;
}
