/* XNB File Decoder
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

struct type_reader {
	char name[256];
	int32_t version;
};

struct xnb_container {
	struct xnb_header hdr;
	int32_t type_reader_count;
	struct type_reader *readers;
	int32_t shared_resource_count;
	void *primary_asset_data;
	void **shared_resource_data;
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

void dump_reader(struct type_reader *rdr)
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
		struct type_reader *r = &cont->readers[i];
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

	return cont;

fail:
	destroy_container(cont);
	return NULL;
}

int main(int argc, char *argv[])
{
	FILE *fp;
	struct xnb_container *cont;

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

	dump_container(cont);
	destroy_container(cont);

	return 0;

}
