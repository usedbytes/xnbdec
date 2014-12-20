/* XNB File Decoder
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN 256

enum xnb_object_type {
	XNB_OBJ_SOUND_EFFECT,
};

struct xnb_object_head {
	enum xnb_object_type type;
};

struct xnb_object_reader {
	char name[MAX_NAME_LEN];
	enum xnb_object_type type;
	struct xnb_object_head *(*deserialize)(FILE *fp);
	void (*destroy)(struct xnb_object_head *obj);
	void (*print)(struct xnb_object_head *obj);
};

struct waveformatex {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;
};

void dump_waveformatex(struct waveformatex *format)
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

struct xnb_obj_sound_effect {
	struct xnb_object_head head;
	uint32_t format_size;
	uint8_t *format;
	uint32_t data_size;
	uint8_t *data;
	int32_t loop_start;
	int32_t loop_length;
	int32_t	duration;
};

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

void sound_effect_print(struct xnb_object_head *obj)
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

void sound_effect_destroy(struct xnb_object_head *obj)
{
	struct xnb_obj_sound_effect *eff = (struct xnb_obj_sound_effect *)obj;
	if (obj == NULL)
		return
	assert(obj->type == XNB_OBJ_SOUND_EFFECT);

	free(eff->data);
	free(eff->format);
	free(eff);
}

struct xnb_object_head *sound_effect_read(FILE *fp)
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

struct xnb_object_reader sound_effect_reader = {
	.name = "Microsoft.Xna.Framework.Content.SoundEffectReader",
	.type = XNB_OBJ_SOUND_EFFECT,
	.deserialize = sound_effect_read,
	.destroy = sound_effect_destroy,
	.print = sound_effect_print,
};

struct xnb_object_reader *readers[] = {
	&sound_effect_reader,
	NULL,
};

struct xnb_header {
	char magic[3];
	char platform;
	uint8_t version;
#define FLAG_HIDEF      0x01
#define FLAG_COMPRESSED 0x80
	uint8_t flags;
	uint32_t file_size;
	uint32_t decompressed_size;
} __attribute__((packed)) ;

struct type_reader_desc {
	char name[256];
	int32_t version;
};

struct xnb_container {
	struct xnb_header hdr;
	int32_t type_reader_count;
	struct type_reader_desc *readers;
	int32_t shared_resource_count;
	struct xnb_object_head *primary_asset;
	struct xnb_object_head **shared_resources;
};

/* Modified from MS Document XNB Format.docx
 * http://xbox.create.msdn.com/en-US/sample/xnb_format
 */
int Read7BitEncodedInt(FILE *fp)
{
    int result = 0;
    int bitsRead = 0;
    char value;
	size_t read;

    do
    {
        read = fread(&value, 1, 1, fp);
		if (read != 1)
			return -1;
        result |= (value & 0x7f) << bitsRead;
        bitsRead += 7;
    }
    while (value & 0x80);

    return result;
}

void dump_object(struct xnb_object_head *obj)
{
	int i = 0;

	if (obj == NULL)
		return;

	while (readers[i]) {
		struct xnb_object_reader *reader = readers[i];
		if (reader->type == obj->type)
			return reader->print(obj);
		i++;
	}
}

void destroy_object(struct xnb_object_head *obj)
{
	int i = 0;

	if (obj == NULL)
		return;

	while (readers[i]) {
		struct xnb_object_reader *reader = readers[i];
		if (reader->type == obj->type)
			return reader->destroy(obj);
		i++;
	}
}

struct xnb_object_head *read_object(struct type_reader_desc *rdr, FILE *fp)
{
	int i = 0;
	while (readers[i]) {
		struct xnb_object_reader *reader = readers[i];
		if (!strcmp(reader->name, rdr->name))
			return reader->deserialize(fp);
		i++;
	}
	return NULL;
}

void dump_header(struct xnb_header *hdr)
{
	printf("[XNB Container Header]\n");
	printf("Magic: %.*s\n", 4, hdr->magic);
	printf("Version: %d\n", hdr->version);
	printf("Flags: 0x%02x\n", hdr->flags);
	printf("File Size: %d (0x%08x)\n", hdr->file_size, hdr->file_size);
	if (hdr->flags & FLAG_COMPRESSED)
		printf("Decompressed Size: %d (0x%08x)\n", hdr->decompressed_size,
				hdr->decompressed_size);
	printf("----------------------\n");
}

void dump_reader(struct type_reader_desc *rdr)
{
	printf("[Type Reader]\n");
	printf("Name: %s\n", rdr->name);
	printf("Version: %d\n", rdr->version);
	printf("-------------\n");
}

void dump_container(struct xnb_container *cont)
{
	int i;
	printf("XNB Container\n");
	printf("=============\n");
	dump_header(&cont->hdr);
	printf("\n");
	for (i = 0; i < cont->type_reader_count; i++) {
		dump_reader(&cont->readers[i]);
	}
	printf("\n");
	printf("Shared resource count: %d\n", cont->shared_resource_count);

	printf("Primary Asset:\n");
	dump_object(cont->primary_asset);

	if (cont->shared_resource_count) {
		printf("Shared Assets:\n");
		for (i = 0; i < cont->shared_resource_count; i++) {
			dump_object(cont->shared_resources[i]);
		}
	}
}

int read_header(struct xnb_header *hdr, FILE *fp)
{
	size_t read, size;
	char magic[] = "XNB";

	size = sizeof(*hdr) - sizeof(hdr->decompressed_size);
	read = fread(hdr, size, 1, fp);
	if (read != 1)
		return -1;

	if (memcmp(hdr->magic, magic, 3))
		return -1;

	if (hdr->flags & FLAG_COMPRESSED) {
		size = sizeof(hdr->decompressed_size);
		read = fread(&hdr->decompressed_size, size, 1, fp);
		if (read != 1)
			return -1;
	} else {
		hdr->decompressed_size = hdr->file_size;
	}

	return 0;
}

void destroy_container(struct xnb_container *cont)
{
	int i;
	for (i = 0; i < cont->shared_resource_count; i++) {
		destroy_object(cont->shared_resources[i]);
	}
	destroy_object(cont->primary_asset);
	free(cont->readers);
	free(cont);
}

struct xnb_container *read_container(FILE *fp)
{
	int res, i;
	struct xnb_container *cont = malloc(sizeof(*cont));
	if (!cont)
		return NULL;
	memset(cont, 0, sizeof(*cont));

	res = read_header(&cont->hdr, fp);
	if (res) {
		fprintf(stderr, "Couldn't read header\n");
		goto fail;
	}

	cont->type_reader_count = Read7BitEncodedInt(fp);
	if (cont->type_reader_count < 0) {
		fprintf(stderr, "Couldn't get type reader count\n");
		goto fail;
	}

	cont->readers = malloc(sizeof(*cont->readers) * cont->type_reader_count);
	if (!cont->readers) {
		fprintf(stderr, "Out-of-memory allocating readers\n");
		goto fail;
	}

	for (i = 0; i < cont->type_reader_count; i++) {
		int read;
		struct type_reader_desc *r = &cont->readers[i];
		read = Read7BitEncodedInt(fp);
		if (read < 0) {
			fprintf(stderr, "Couldn't read name of reader %d\n", i);
			goto fail;
		} else if (read > 255) {
			read = 255;
		}
		fgets(r->name, read + 1, fp);
		read = fread(&r->version, sizeof(r->version), 1, fp);
		if (read != 1) {
			fprintf(stderr, "Couldn't read version of reader %d\n", i);
			goto fail;
		}
	}

	cont->shared_resource_count = Read7BitEncodedInt(fp);
	if (cont->shared_resource_count < 0) {
		fprintf(stderr, "Couldn't read shared resource count\n");
		goto fail;
	}

	i = Read7BitEncodedInt(fp);
	if (i < 0) {
		fprintf(stderr, "Couldn't read primary asset type\n");
		goto fail;
	} else if (i > 0) {
		cont->primary_asset = read_object(&cont->readers[i - 1], fp);
		if (!cont->primary_asset) {
			fprintf(stderr, "Couldn't read primary asset\n");
			goto fail;
		}
	}

	for (i = 0; i < cont->shared_resource_count; i++) {
		int type_idx = Read7BitEncodedInt(fp);
		if (type_idx < 0) {
			fprintf(stderr, "Couldn't read shared asset %d type\n",
					type_idx);
			goto fail;
		} else if (type_idx > 0) {
			cont->shared_resources[i] =
				read_object(&cont->readers[type_idx - 1], fp);
			if (!cont->shared_resources[i]) {
				fprintf(stderr, "Couldn't read shared asset %d\n", type_idx);
				goto fail;
			}
		}
	}

	return cont;

fail:
	destroy_container(cont);
	return NULL;
}

int main(int argc, char *argv[])
{
	FILE *fp;
	struct xnb_container *cont;
	char filename[MAX_NAME_LEN];
	int res;

	if (argc < 2) {
		fprintf(stderr, "Need a file\n");
		return 1;
	}

	fp = fopen(argv[1], "r");
	if (!fp) {
		fprintf(stderr, "fopen failed\n");
		return 1;
	}

	cont = read_container(fp);
	if (!cont) {
		return 1;
	}
	fclose(fp);

	dump_container(cont);

	snprintf(filename, MAX_NAME_LEN, "%s.wav", argv[1]);
	res = write_wave_file((struct xnb_obj_sound_effect *)cont->primary_asset,
			filename);
	if (res) {
		fprintf(stderr, "Couldn't write wave file\n");
	}

	destroy_container(cont);

	return 0;

}
