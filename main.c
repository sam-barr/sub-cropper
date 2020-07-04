/* 
 * vim: tabstop=8 shiftwidth=8
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <png.h>

#define SUB_PNG_VERSION "1.6.37"
#define PNG_SIG_LENGTH 8

#define sub_image_row_size(image) (sizeof(uint8_t) * 3 * (image)->width)

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

#define DEBUG_BOOL(b) printf("%s\n", (b) ? "true" : "false")

/*
 * Divide the image width by this skillfully picked magic
 * to estimate the size of subtitle characters
 */
#define BOX_RADIUS_MAGIC_NUMBER 32

size_t MAX_BOX_RADIUS, MAX_BOX_DIAM;

#define true 1
#define false 0

struct sub_png_reader {
        png_struct *png;
        png_info *info;
};

struct sub_png_writer {
        png_struct *png;
        png_info *info;
        FILE *file;
};

/*
 * 8-bit RGB image data
 */
struct sub_image {
        size_t width;
        size_t height;
        uint8_t *data;
};

struct sub_pixel {
        uint8_t r;
        uint8_t g;
        uint8_t b;
};

struct sub_box {
        size_t top;
        size_t bottom;
        size_t left;
        size_t right;
};

/*
 * sub_pixel functions
 */

int sub_pixel_equal(struct sub_pixel p1, struct sub_pixel p2) {
        return p1.r == p2.r && p1.g == p2.g && p1.b == p2.b;
}

int sub_pixel_different(struct sub_pixel p1, struct sub_pixel p2) {
        int rd, gd, bd;
        rd = p1.r - p2.r;
        gd = p1.g - p2.g;
        bd = p1.b - p2.b;
        return (rd * rd) + (gd * gd) + (bd * bd) > 10000;
}

/*
 * Reads the first 8 bytes and checks them against the magic png header
 * Should probably be followed with a call to "png_set_sig_bytes"
 */
int sub_check_if_png(FILE *file) {
        uint8_t bytes[PNG_SIG_LENGTH];
        fread(bytes, sizeof(uint8_t), PNG_SIG_LENGTH, file);
        return png_sig_cmp(bytes, 0, PNG_SIG_LENGTH) == 0;
}

/*
 * struct sub_png reading functions
 */

void sub_png_reader_init(struct sub_png_reader *reader, char *file_name) {
        png_struct *png;
        png_info *info;
        FILE *file;

        png = png_create_read_struct(SUB_PNG_VERSION, NULL, NULL, NULL);
        if (png == NULL) {
                fprintf(stderr, "Unable to create read struct\n");
                exit(EXIT_FAILURE);
        }

        info = png_create_info_struct(png);
        if (info == NULL) {
                fprintf(stderr, "Unable to create info struct\n");
                exit(EXIT_FAILURE);
        }

        file = fopen(file_name, "rb");
        if (file == NULL) {
                fprintf(stderr, "Unable to open file %s for reading\n", file_name);
                exit(EXIT_FAILURE);
        } else if (!sub_check_if_png(file)) {
                fprintf(stderr, "File %s is not a png file\n", file_name);
                exit(EXIT_FAILURE);
        }

        png_init_io(png, file);
        png_set_sig_bytes(png, PNG_SIG_LENGTH);
        png_read_png(
                png, info,
                PNG_TRANSFORM_STRIP_16 |
                PNG_TRANSFORM_PACKING |
                PNG_TRANSFORM_GRAY_TO_RGB |
                PNG_TRANSFORM_STRIP_ALPHA,
                NULL);

        reader->png = png;
        reader->info = info;
        fclose(file);
}

void sub_png_reader_destroy(struct sub_png_reader *reader) {
        png_destroy_read_struct(&reader->png, &reader->info, NULL);
}

void sub_png_reader_load_image(struct sub_png_reader *reader, struct sub_image *image) {
        size_t i, row_size;
        uint8_t **image_rows;

        image->width = png_get_image_width(reader->png, reader->info);
        image->height = png_get_image_height(reader->png, reader->info);
        row_size = sub_image_row_size(image);
        image->data = malloc(row_size * image->height);

        image_rows = png_get_rows(reader->png, reader->info);
        for (i = 0; i < image->height; i++)
                memcpy(image->data + i * row_size, image_rows[i], row_size);
}

/*
 * struct sub_png writing functions
 */

void sub_png_writer_init(struct sub_png_writer *writer, char *file_name) {
        png_struct *png;
        png_info *info;
        FILE *file;

        png = png_create_write_struct(SUB_PNG_VERSION, NULL, NULL, NULL);
        if (png == NULL) {
                fprintf(stderr, "Unable to create write struct\n");
                exit(EXIT_FAILURE);
        }

        info = png_create_info_struct(png);
        if (info == NULL) {
                fprintf(stderr, "Unable to create info struct\n");
                exit(EXIT_FAILURE);
        }

        file = fopen(file_name, "wb");
        if (file == NULL) {
                fprintf(stderr, "Unable to open file %s for writing\n", file_name);
                exit(EXIT_FAILURE);
        }

        png_init_io(png, file);
        png_set_compression_level(png, 2);
        writer->png = png;
        writer->info = info;
        writer->file = file;
}

void sub_png_writer_destroy(struct sub_png_writer *writer) {
        png_destroy_write_struct(&writer->png, &writer->info);
        fclose(writer->file);
}

void sub_png_writer_save_image_cropped(struct sub_png_writer *writer,
                struct sub_image *image, struct sub_box *crop) {
        uint8_t **data;
        size_t row_size, i, width, height;

        width = crop->right - crop->left;
        height = crop->bottom - crop->top;

        row_size = sub_image_row_size(image);
        data = malloc(sizeof(uint8_t *) * height);
        for (i = 0; i < height; i++)
                data[i] = image->data + (i + crop->top) * row_size + 3 * crop->left;

        png_set_IHDR(
                writer->png,
                writer->info,
                width,
                height,
                8, /* bit depth of 8 */
                PNG_COLOR_TYPE_RGB,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
        png_set_rows(writer->png, writer->info, data);
        png_write_png(
                writer->png,
                writer->info,
                PNG_TRANSFORM_IDENTITY,
                NULL);
        free(data);
}

/*
 * struct sub_image functions
 */

void sub_image_destroy(struct sub_image *image) {
        free(image->data);
}

struct sub_pixel sub_image_get_pixel(struct sub_image *image, size_t x, size_t y) {
        struct sub_pixel pixel;
        size_t row_size;

        row_size = sub_image_row_size(image);
        pixel.r = image->data[row_size * y + 3 * x];
        pixel.g = image->data[row_size * y + 3 * x + 1];
        pixel.b = image->data[row_size * y + 3 * x + 2];

        return pixel;
}

void sub_image_set_pixel(struct sub_image *image, struct sub_pixel pixel,
                size_t x, size_t y) {
        size_t row_size = sub_image_row_size(image);
        image->data[row_size * y + 3 * x]     = pixel.r;
        image->data[row_size * y + 3 * x + 1] = pixel.g;
        image->data[row_size * y + 3 * x + 2] = pixel.b;
}

/*
 * Box functions
 */

size_t sub_box_area(struct sub_box *box) {
        return (box->right - box->left) * (box->bottom - box->top);
}

int sub_box_contains(struct sub_box *outer, struct sub_box *inner) {
        return outer->right > inner->right &&
                outer->left < inner->left &&
                outer->bottom > inner->bottom &&
                outer->top < inner->top;
}

/*
 * Box finding data & functions
 */

struct sub_point  {
        size_t x;
        size_t y;
};

void sub_stack_push(struct sub_point **stack, size_t *size, size_t x, size_t y) {
        (*stack)[*size].x = x;
        (*stack)[*size].y = y;
        *size += 1;
}

struct sub_point sub_stack_pop(struct sub_point **stack, size_t *size) {
        *size -= 1;
        *stack += 1;
        return (*stack)[-1];
}

enum sub_quad_state {
        SUB_NONE = 0,
        SUB_QUEUE,
        SUB_FALSE,
        SUB_TRUE
};

/* Reserve memory for the "stack" in sub_image_find_box */
#define STACK_SIZE (sizeof(struct sub_point) * MAX_BOX_DIAM * MAX_BOX_DIAM)
#define STATE_SIZE (sizeof(enum sub_quad_state) * MAX_BOX_DIAM * MAX_BOX_DIAM)
struct sub_point *STACK;
enum sub_quad_state *PIXEL_STATES;

/* this has (n+1) off bye one errors */
void sub_image_find_box(struct sub_image *image, struct sub_box *box,
                struct sub_pixel color, size_t x, size_t y) {
        size_t stack_size = 0;
        size_t left, right, top, bottom;
        enum sub_quad_state *pixel_states = PIXEL_STATES;
        struct sub_point *stack = STACK;

#define get_state(x, y) (pixel_states[MAX_BOX_DIAM * ((y) - top) + ((x) - left)])
#define valid_x(x) ((x) >= left && (x) < right && (x))
#define valid_y(y) ((y) >= top && (y) < bottom && (y))
#define valid(x, y) (valid_x(x) && valid_y(y) && get_state(x, y) == SUB_NONE)

        memset(pixel_states, SUB_NONE, STATE_SIZE);
        sub_stack_push(&stack, &stack_size, x, y);

        left = (x > MAX_BOX_RADIUS) ? x - MAX_BOX_RADIUS : 0;
        right = min(image->width, x + MAX_BOX_RADIUS);
        top = (y > MAX_BOX_RADIUS) ? y - MAX_BOX_RADIUS : 0;
        bottom = min(image->height, y + MAX_BOX_RADIUS);

        box->left = box->right = x;
        box->top = box->bottom = y;

        while (stack_size > 0) {
                struct sub_point point;
                struct sub_pixel c;

                point = sub_stack_pop(&stack, &stack_size);
                c = sub_image_get_pixel(image, point.x, point.y);
                if (!sub_pixel_equal(color, c)) {
                        get_state(point.x, point.y) = SUB_FALSE;
                        continue;
                }

                get_state(point.x, point.y) = SUB_TRUE;
                box->left = min(box->left, point.x);
                box->right = max(box->right, point.x);
                box->top = min(box->top, point.y);
                box->bottom = max(box->bottom, point.y);

                if (valid(point.x - 1, point.y)) {
                        get_state(point.x - 1, point.y) = SUB_QUEUE;
                        sub_stack_push(&stack, &stack_size, point.x - 1, point.y);
                }
                if (valid(point.x + 1, point.y)) {
                        get_state(point.x + 1, point.y) = SUB_QUEUE;
                        sub_stack_push(&stack, &stack_size, point.x + 1, point.y);
                }
                if (valid(point.x, point.y - 1)) {
                        get_state(point.x, point.y - 1) = SUB_QUEUE;
                        sub_stack_push(&stack, &stack_size, point.x, point.y - 1);
                }
                if (valid(point.x, point.y + 1)) {
                        get_state(point.x, point.y + 1) = SUB_QUEUE;
                        sub_stack_push(&stack, &stack_size, point.x, point.y + 1);
                }
        }

#undef get_state
#undef valid_x
#undef valid_y
#undef valid
}

void sub_load_image(struct sub_image *image, char *file_name) {
        struct sub_png_reader reader;
        sub_png_reader_init(&reader, file_name);
        sub_png_reader_load_image(&reader, image);
        sub_png_reader_destroy(&reader);
}

void sub_save_image_cropped(struct sub_image *image, struct sub_box *box, char *file_name) {
        struct sub_png_writer writer;
        sub_png_writer_init(&writer, file_name);
        sub_png_writer_save_image_cropped(&writer, image, box);
        sub_png_writer_destroy(&writer);
}

/*
 * RETURN: how much more to increment/decrement i by
 */
int sub_scan_image_helper(struct sub_image *image, struct sub_box *crop,
                size_t y, size_t ox, size_t ix) {
        struct sub_box outer, inner;
        struct sub_pixel ocolor, icolor;
        size_t oarea, iarea;

        ocolor = sub_image_get_pixel(image, ox, y);
        icolor = sub_image_get_pixel(image, ix, y);
        if (!sub_pixel_different(ocolor, icolor))
                return 0;

        sub_image_find_box(image, &outer, ocolor, ox, y);
        oarea = sub_box_area(&outer);
        if (oarea == 0)
                return 0;
        sub_image_find_box(image, &inner, icolor, ix, y);
        iarea = sub_box_area(&inner);
        if (iarea == 0 || !sub_box_contains(&outer, &inner))
                return 0;

        crop->left = min(crop->left, outer.left);
        crop->right = max(crop->right, outer.right);
        crop->top = min(crop->top, outer.top);
        crop->bottom = max(crop->bottom, outer.bottom);
        return outer.right - outer.left;
}

enum sub_direction {
        DIR_RIGHT,
        DIR_LEFT
};

/*
 * Scan an image horizontally for subs at height y
 */
void sub_scan_image(struct sub_image *image, struct sub_box *crop,
                size_t y, enum sub_direction direction) {
        size_t i;

        /* TODO: this is also kinda bad */
        switch (direction) {
        case DIR_RIGHT:
                for (i = MAX_BOX_RADIUS; i <= image->width - MAX_BOX_RADIUS; i++)
                        i += sub_scan_image_helper(image, crop, y, i, i + 5);
                break;
        case DIR_LEFT:
                for (i = image->width - MAX_BOX_RADIUS; i >= MAX_BOX_RADIUS; i--)
                        i -= sub_scan_image_helper(image, crop, y, i, i - 5);
                break;
        }
}

int main(int argc, char **argv) {
        struct sub_image im;
        size_t i;
        int count = 0;
        char out_file[150], *in_file;

        if (argc == 1) {
                fprintf(stderr, "Please give an input\n");
                return EXIT_FAILURE;
        }

        in_file = argv[1];
        sub_load_image(&im, in_file);
        MAX_BOX_RADIUS = (MAX_BOX_DIAM = im.width / BOX_RADIUS_MAGIC_NUMBER) / 2;
        STACK = malloc(STACK_SIZE);
        PIXEL_STATES = malloc(STATE_SIZE);
        *strrchr(in_file, '.') = '\0'; /* chop of ".png" from file name */

        for (i = MAX_BOX_RADIUS; i < im.height - MAX_BOX_RADIUS; i += MAX_BOX_RADIUS/2) {
                struct sub_box crop;
                size_t top, bottom, y;

                crop.right = crop.bottom = 0;
                crop.top = im.height;
                crop.left = im.width;

                sub_scan_image(&im, &crop, i, DIR_RIGHT);
                if (crop.right == 0)
                        continue;
                top = crop.top;
                bottom = crop.bottom;

                for (y = top; y < bottom; y += 5) {
                        sub_scan_image(&im, &crop, y, DIR_RIGHT);
                        sub_scan_image(&im, &crop, y, DIR_LEFT);
                }

                if (crop.left > MAX_BOX_RADIUS)
                        crop.left -= MAX_BOX_RADIUS;
                if (im.width - crop.right > MAX_BOX_RADIUS)
                        crop.right += MAX_BOX_RADIUS;

                sprintf(out_file, "%s.cropped.%d.png", in_file, count);
                sub_save_image_cropped(&im, &crop, out_file);
                i = crop.bottom;
                count++;
        }

        sub_image_destroy(&im);
        free(STACK);
        free(PIXEL_STATES);

        printf("Found %d subtitles\n", count);
        return count;
}
