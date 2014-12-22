/* XNB SoundEffect object implementation
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xnb_object.h"

struct waveformatex {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;
};

struct xnb_obj_sound_effect {
	struct xnb_object_head head;

	uint32_t format_size;
	/* A struct waveformatex, possibly extended */
	uint8_t *format;
	uint32_t data_size;
	uint8_t *data;
	/* In bytes */
	int32_t loop_start;
	int32_t loop_length;
	/* In milliseconds */
	int32_t	duration;
};

static void dump_waveformatex(struct waveformatex *format)
{
	printf("[WAVEFORMATEX]\n");
	printf("wFormatTag: 0x%x\n", format->wFormatTag);
	printf("nChannels: %d\n", format->nChannels);
	printf("nSamplesPerSec: %d\n", format->nSamplesPerSec);
	printf("nAvgBytesPerSec %d\n", format->nAvgBytesPerSec);
	printf("nBlockAlign %d\n", format->nBlockAlign);
	printf("wBitsPerSample %d\n", format->wBitsPerSample);
	printf("cbSize %d\n", format->cbSize);
	printf("--------------\n");
}

int write_wave_file(struct xnb_obj_sound_effect *eff, char *filename)
{
	FILE *fp = fopen(filename, "w");
	uint32_t u32buf;
	uint16_t u16buf;
	size_t wrote;
	struct waveformatex *fmt = (struct waveformatex *)eff->format;
	int res = -1;

	wrote = fputs("RIFF", fp);
	if (wrote == EOF) {
		fprintf(stderr, "Couldn't write RIFF\n");
		goto done;
	}

	u32buf = 36 + eff->data_size;
	wrote = fwrite(&u32buf, sizeof(u32buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write ChunkSize\n");
		goto done;
	}

	wrote = fputs("WAVE", fp);
	if (wrote == EOF) {
		fprintf(stderr, "Couldn't write WAVE\n");
		goto done;
	}

	wrote = fputs("fmt ", fp);
	if (wrote == EOF) {
		fprintf(stderr, "Couldn't write fmt\n");
		goto done;
	}

	u32buf = 16;
	wrote = fwrite(&u32buf, sizeof(u32buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write SubChunk1Size\n");
		goto done;
	}

	u16buf = 1;
	wrote = fwrite(&u16buf, sizeof(u16buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write AudioFormat\n");
		goto done;
	}

	u16buf = fmt->nChannels;
	wrote = fwrite(&u16buf, sizeof(u16buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write NumChannels\n");
		goto done;
	}

	u32buf = fmt->nSamplesPerSec;
	wrote = fwrite(&u32buf, sizeof(u32buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write SampleRate\n");
		goto done;
	}

	u32buf = fmt->nAvgBytesPerSec;
	wrote = fwrite(&u32buf, sizeof(u32buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write ByteRate\n");
		goto done;
	}

	u16buf = fmt->nBlockAlign;
	wrote = fwrite(&u16buf, sizeof(u16buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write BlockAlign\n");
		goto done;
	}

	u16buf = fmt->wBitsPerSample;
	wrote = fwrite(&u16buf, sizeof(u16buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write BitsPerSample\n");
		goto done;
	}

	wrote = fputs("data", fp);
	if (wrote == EOF) {
		fprintf(stderr, "Couldn't write 'data'\n");
		goto done;
	}

	u32buf = eff->data_size;
	wrote = fwrite(&u32buf, sizeof(u32buf), 1, fp);
	if (wrote != 1) {
		fprintf(stderr, "Couldn't write SubChunk2Size\n");
		goto done;
	}

	wrote = fwrite(eff->data, 1, eff->data_size, fp);
	if (wrote != eff->data_size) {
		fprintf(stderr, "Couldn't write Data\n");
		goto done;
	}

	res = 0;

done:
	fclose(fp);
	return res;
}

static void sound_effect_print(struct xnb_object_head *obj)
{
	struct xnb_obj_sound_effect *eff = (struct xnb_obj_sound_effect *)obj;
	struct waveformatex format;
	if (obj == NULL)
		return
	assert(obj->type == XNB_OBJ_SOUND_EFFECT);

	memcpy(&format, eff->format, sizeof(format));

	printf("[SoundEffect]\n");
	printf("Format Size: %d\n", eff->format_size);
	dump_waveformatex(&format);
	printf("Data Size: %d\n", eff->data_size);
	printf("Loop Start: %d\n", eff->loop_start);
	printf("Loop Length: %d\n", eff->loop_length);
	printf("Duration: %d\n", eff->duration);
	printf("-------------\n");

}

static void sound_effect_destroy(struct xnb_object_head *obj)
{
	struct xnb_obj_sound_effect *eff = (struct xnb_obj_sound_effect *)obj;
	if (obj == NULL)
		return
	assert(obj->type == XNB_OBJ_SOUND_EFFECT);

	free(eff->data);
	free(eff->format);
	free(eff);
}

static struct xnb_object_head *sound_effect_read(FILE *fp)
{
	struct xnb_obj_sound_effect *eff;
	size_t read;

	eff = malloc(sizeof(*eff));
	if (!eff) {
		fprintf(stderr, "Couldn't alloc effect structure\n");
		return NULL;
	}
	memset(eff, 0, sizeof(*eff));
	eff->head.type = XNB_OBJ_SOUND_EFFECT;

	read = fread(&eff->format_size, sizeof(eff->format_size), 1, fp);
	if (read != 1) {
		fprintf(stderr, "Couldn't read format size\n");
		goto fail;
	}

	eff->format = malloc(eff->format_size);
	if (!eff->format) {
		fprintf(stderr, "Couldn't alloc format structure\n");
		goto fail;
	}

	read = fread(eff->format, 1, eff->format_size, fp);
	if (read != eff->format_size) {
		fprintf(stderr, "Couldn't read format structure\n");
		goto fail;
	}

	read = fread(&eff->data_size, sizeof(eff->data_size), 1, fp);
	if (read != 1) {
		fprintf(stderr, "Couldn't read data size\n");
		goto fail;
	}

	eff->data = malloc(eff->data_size);
	if (!eff->data) {
		fprintf(stderr, "Couldn't alloc data space\n");
		goto fail;
	}

	read = fread(eff->data, 1, eff->data_size, fp);
	if (read != eff->data_size) {
		fprintf(stderr, "Couldn't read data\n");
		goto fail;
	}

	read = fread(&eff->loop_start, sizeof(eff->loop_start), 1, fp);
	if (read != 1) {
		fprintf(stderr, "Couldn't read loop start\n");
		goto fail;
	}

	read = fread(&eff->loop_length, sizeof(eff->loop_length), 1, fp);
	if (read != 1) {
		fprintf(stderr, "Couldn't read loop length\n");
		goto fail;
	}

	read = fread(&eff->duration, sizeof(eff->duration), 1, fp);
	if (read != 1) {
		fprintf(stderr, "Couldn't read duration\n");
		goto fail;
	}

	return (struct xnb_object_head *)eff;

fail:
	free(eff->data);
	free(eff->format);
	free(eff);
	return NULL;
}

const struct xnb_object_reader sound_effect_reader = {
	.name = "Microsoft.Xna.Framework.Content.SoundEffectReader",
	.type = XNB_OBJ_SOUND_EFFECT,
	.deserialize = sound_effect_read,
	.destroy = sound_effect_destroy,
	.print = sound_effect_print,
};

