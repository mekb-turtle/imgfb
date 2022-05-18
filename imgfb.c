#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "stb_image.h"

int usage(char *argv0) {
	fprintf(stderr, "Usage: %s <image file>\n", argv0);
	fprintf(stderr, "path to the image file to draw or - for stdin, can be either farbfeld or jpeg\n");
	fprintf(stderr, "--framebuffer -f : framebuffer name, defaults to fb0\n");
	fprintf(stderr, "--offsetx -x --offsety -y : offset of image to draw, defaults to 0 0\n");
	return 2;
}

void rgba2bgra(uint8_t *pixels, uint32_t width, uint32_t height, uint8_t bpp) {
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			uint64_t i = (x + (y * width)) * 4;
			uint8_t b = pixels[i + 0];
			uint8_t r = pixels[i + 2];
			pixels[i + 0] = r;
			pixels[i + 2] = b;
		}
	}
}
void detect_type(FILE *fp, uint8_t *data, bool *is_farbfeld, bool *is_jpeg, bool *is_png) {
	fread(data, 1, 8, fp);
	*is_farbfeld = strncmp((char*)data, "farbfeld", 8) == 0;
	*is_jpeg = strncmp((char*)data, "\xFF\xD8\xFF", 3) == 0;
	*is_png = strncmp((char*)data, "\x89PNG\x0D\x0A\x1A\x0A", 8) == 0;
}
uint8_t *decode_farbfeld(FILE *fp, uint32_t *width_, uint32_t *height_) {
	uint32_t width  = fgetc(fp) << 030 | fgetc(fp) << 020 | fgetc(fp) << 010 | fgetc(fp); // big endian = highest byte comes first
	uint32_t height = fgetc(fp) << 030 | fgetc(fp) << 020 | fgetc(fp) << 010 | fgetc(fp);
	uint8_t *pixels;
	pixels = malloc(width * height * 4);
	*width_ = width;
	*height_ = height;

	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			uint64_t i = x + (y * width);
			pixels[i*4+2] = fgetc(fp); fgetc(fp); // R
			pixels[i*4+1] = fgetc(fp); fgetc(fp); // G
			pixels[i*4+0] = fgetc(fp); fgetc(fp); // B
			pixels[i*4+3] = fgetc(fp); fgetc(fp); // A
		}
	}
	return pixels;
}

int main(int argc, char* argv[]) {
#define INVALID { return usage(argv[0]); }
	char* fb_path    = NULL;
	char* size_path  = NULL;
	char* bpp_path   = NULL;
	char* image_path = NULL;
	int32_t offset_x;
	int32_t offset_y;
	{
		char* fb_ = "fb0"; // default framebuffer name
		char* x_ = "0";
		char* y_ = "0";

		bool x_flag = 0; bool y_flag = 0; bool fb_flag = 0;
		bool x_done = 0; bool y_done = 0; bool fb_done = 0; bool flag_done = 0;
		for (int i = 1; i < argc; ++i) {
			if      (x_flag ) { x_  = argv[i]; x_flag  = 0; }
			else if (y_flag ) { y_  = argv[i]; y_flag  = 0; }
			else if (fb_flag) { fb_ = argv[i]; fb_flag = 0; }
			else if (argv[i][0] == '-' && argv[i][1] != '\0' && !flag_done) {
				if (argv[i][1] == '-' && argv[i][2] == '\0') flag_done = 1;
				else {
					if (i >= argc - 1) INVALID;
					if      ((strcmp(argv[i], "--offsetx")     == 0 || strcmp(argv[i], "-x")) == 0 && !x_done ) x_done  = x_flag  = 1;
					else if ((strcmp(argv[i], "--offsety")     == 0 || strcmp(argv[i], "-y")) == 0 && !y_done ) y_done  = y_flag  = 1;
					else if ((strcmp(argv[i], "--framebuffer") == 0 || strcmp(argv[i], "-f")) == 0 && !fb_done) fb_done = fb_flag = 1;
					else INVALID;
				}
			} else if (!image_path) { image_path = argv[i]; }
			else INVALID;
		}
		if (!image_path) INVALID;

		if (strlen(x_) == 0) INVALID;
		if (strlen(y_) == 0) INVALID;
		if (strlen(x_) > (x_[0] == '-' ? 7 : 6)) INVALID; // too high of a number causes weirdness
		if (strlen(y_) > (y_[0] == '-' ? 7 : 6)) INVALID;
		if (strncmp(fb_, "/dev/", 5) == 0) fb_ += 5;
		if (strlen(fb_) == 0) INVALID;
		if (strlen(image_path) == 0) INVALID;
#define NUMERIC(x) (x>='0'&&x<='9')
#define ALPHABETIC(x) (x>='a'&&x<='z')
#define ALPHANUMERIC(x) (ALPHABETIC(x) || NUMERIC(x))
		for (size_t i = 0; i < strlen(fb_); ++i) { if (!ALPHANUMERIC(fb_[i])) INVALID; } // fb can only be lowercase alphanumeric
		for (size_t i = 0; i < strlen(x_); ++i) { if (!NUMERIC(x_[i]) && !(i == 0 && x_[0] == '-')) INVALID; } // x and y can only be numeric or start with a - for negative
		for (size_t i = 0; i < strlen(y_); ++i) { if (!NUMERIC(y_[i]) && !(i == 0 && y_[0] == '-')) INVALID; }
#undef INVALID

		fb_path = malloc(strlen(fb_) + 10);
		sprintf(fb_path, "/dev/%s", fb_);
		size_path = malloc(strlen(fb_) + 50);
		sprintf(size_path, "/sys/class/graphics/%s/virtual_size", fb_);
		bpp_path = malloc(strlen(fb_) + 50);
		sprintf(bpp_path, "/sys/class/graphics/%s/bits_per_pixel", fb_);
		offset_x = atoi(x_);
		offset_y = atoi(y_);
	}

#define ERROR(x) { fprintf(stderr, "%s: %s\n", x, strerror(errno)); return errno; }
	uint32_t fb_w;
	uint32_t fb_h;
	{
		FILE* bpp = fopen(bpp_path, "r");
		if (!bpp) ERROR(bpp_path);
		// this must be 32
#define BPP { fprintf(stderr, "bits per pixel is not 32"); return 1; }
		if (getc(bpp) != '3') BPP;
		if (getc(bpp) != '2') BPP;
		int a = getc(bpp);
		if (NUMERIC(a)) BPP;
#undef BPP
	}
	{
		FILE* size = fopen(size_path, "r");
		if (!size) ERROR(size_path);
		free(size_path);
		size_t i = 0;
		bool b = 0;
		char* w_ = malloc(32);
		char* h_ = malloc(32);
		while (1) {
			char c = getc(size);
			if (NUMERIC(c)) { // if character is a number, add it to h_ if b else w_
				if (b) { h_[i++] = c; } else { w_[i++] = c; }
			} else { // if character isn't a number, set b to 1 if b is 0 else break
				if (!b) { b = 1; i = 0; } else { break; }
			}
		}
		fb_w = atoi(w_);
		fb_h = atoi(h_);
		fclose(size);
	}

	FILE* fb;
	fb = fopen(fb_path, "w");
	if (!fb) ERROR(fb_path);
	free(fb_path);

	FILE* image_stream;
	if (strlen(image_path) == 1 && *image_path == '-') {
		image_stream = stdin;
	} else {
		image_stream = fopen(image_path, "r");
	}
	if (!image_stream) ERROR(image_path);

	uint32_t image_w;
	uint32_t image_h;
	uint8_t *image_pixels;
	bool is_farbfeld;
	bool is_jpeg;
	bool is_png;
	uint8_t *image_data = malloc(8);
	detect_type(image_stream, image_data, &is_farbfeld, &is_jpeg, &is_png);

	if (is_farbfeld) {
		image_pixels = decode_farbfeld(image_stream, &image_w, &image_h);
		if (!image_pixels) { fprintf(stderr, "Couldn't decode farbfeld image\n"); return 1; }
	} else if (is_jpeg/* || is_png*/) { // png doesn't work yet
		unsigned long len = 8;
		// kinda hacky way of reading image data of unknown size, fstat/stat won't work if stdin is not file, TODO: make this better
#define READ_SIZE 4096
		do {
			len += READ_SIZE;
			image_data = realloc(image_data, len);
		} while (fread(image_data + len - READ_SIZE, 1, READ_SIZE, image_stream) > 0);
		int image_bpp;
		image_pixels = stbi_load_from_memory(image_data, len + READ_SIZE - 1, (int*)&image_w, (int*)&image_h, &image_bpp, STBI_rgb_alpha);
		if (!image_pixels) { fprintf(stderr, "Couldn't decode %s image: %s\n", is_png ? "png" : "jpeg", stbi_failure_reason()); return 1; }
		rgba2bgra(image_pixels, image_w, image_h, image_bpp);
	} else {
		fprintf(stderr, "Image isn't farbfeld or jpeg\n");
		fclose(image_stream);
		return 1;
	}
	fclose(image_stream);

	if (offset_y >= -(int32_t)image_h) { // check if image is in bounds
		for (uint32_t y = offset_y<0?-offset_y:0; y < (offset_y<0?-offset_y:0)+image_h && y+offset_y < fb_h; ++y) { // i no longer know how any of this code works
			fseek(fb, ((offset_x<0?0:offset_x) + (y+offset_y)*fb_w) * 4, SEEK_SET);
			uint32_t w = image_w + (offset_x<0?offset_x:0) - (offset_x+image_w>=fb_w?offset_x-(fb_w-image_w):0);
			uint32_t i = y*image_w - (offset_x<0?offset_x:0);
			if (i > image_w*image_h) break; // rare crash for some reason
			if (offset_x>=-(int32_t)image_w && w>0 && offset_x<(int32_t)fb_w)
				fwrite(image_pixels + i * 4, 1, w * 4, fb);
		}
	}
	fclose(fb);
	if (is_farbfeld) {
		free(image_pixels);
	} else if (is_jpeg || is_png) {
		stbi_image_free(image_pixels);
	}

	return 0;
}
