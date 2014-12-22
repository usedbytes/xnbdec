/* XNB File Decoder
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 */

#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xnb_object.h"

enum actions {
	ACTION_LIST =   (1 << 0),
	ACTION_EXPORT = (1 << 1),
};

struct exec_context {
	bool quiet;
	int actions;
	char *basename;
	char *output_prefix;
	int n_input_files;
	char **input_files;
};

struct exec_context ctx = {
	.quiet = false,
	.actions = 0,
	.basename = NULL,
	.output_prefix = NULL,
	.n_input_files = 0,
	.input_files = NULL,
};

/* Returns the number of filenames found, or < 0 on error */
int read_file_list(char *filename, char ***file_list)
{
	FILE *fp;
	char buf[MAX_NAME_LEN];
	int n_files = 0;
	int res = 0;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Couldn't open '%s' for reading\n", filename);
		return -1;
	}

	/* One pass to count files */
	while (fgets(buf, MAX_NAME_LEN, fp)) {
		n_files++;
	}
	*file_list = malloc(sizeof(**file_list) * n_files);
	if (!*file_list) {
		fprintf(stderr, "Couldn't allocate space for input files\n");
		res = -1;
		goto done;
	}

	/* Second pass to read them */
	fseek(fp, 0, SEEK_SET);
	n_files = 0;
	while (fgets(buf, MAX_NAME_LEN, fp)) {
		char *infile = malloc(strlen(buf));;
		(*file_list)[n_files] = infile;
		strncpy(infile, buf, strlen(buf) - 1);
		infile[strlen(buf) - 1] = '\0';
		n_files++;
	}

done:
	fclose(fp);
	return res < 0 ? res : n_files;

}

/*
 * Usage: xnbdec [OPTION]... [ACTION]... FILE...
 *
 * Decode XNB container FILE(s)
 *
 * Options:
 * -f  --file FILE should be treated as a list of input files, one per line.
 * -q  --quiet Suppress output
 *
 * Actions:
 * -l --list Print information about the container
 *         (this is the default action if another is not specified)
 * -e --export[=basename] Export the container's object(s) to file(s), using
 *         basename as the base filename if specified. Note that basename may
 *         not be specified if there are multiple input files.
 * -o --output-prefix=dir Prepend this path to all output filenames
 */
void print_usage(int argc, char *argv[])
{
 printf("Usage: %s [OPTION]... [ACTION]... [FILE]...\n"
 "\n"
 "Decode XNB container FILE(s) or standard input\n"
 "\n"
 " Options:\n"
 " -f  --file FILE should be treated as a list of input files, one per line.\n"
 " -q  --quiet Suppress output\n"
 "\n"
 " Actions:\n"
 " -l --list Print information about the container\n"
 "         (this is the default action if another is not specified)\n"
 " -e --export[=basename] Export the container's object(s) to file(s), using\n"
 "         basename as the base filename if specified. Note that basename\n"
 "         may not be specified if there are multiple input files.\n"
 " -o --output-prefix=dir Prepend this path to all output filenames\n",
 argv[0]);
}

static struct option long_options[] = {
	{"file",    no_argument,       NULL, 'f' },
	{"quiet",   no_argument,       NULL, 'q' },
	{"list",    no_argument,       NULL, 'l' },
	{"export",  optional_argument, NULL, 'e' },
	{"output-prefix", required_argument, NULL, 'o' },
	{ "", 0, NULL, 0 },
};

int parse_options(int argc, char *argv[])
{
	bool input_list = false;
	int opt_index;
	int opt;

	while (1) {
		opt = getopt_long(argc, argv, ":fqle::o:", long_options, &opt_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 'f':
			input_list = true;
			break;
		case 'q':
			ctx.quiet = true;
			break;
		case 'l':
			ctx.actions |= ACTION_LIST;
			break;
		case 'e':
			ctx.actions |= ACTION_EXPORT;
			ctx.basename = optarg;
			break;
		case 'o':
			ctx.output_prefix = optarg;
			break;
		case ':':
			fprintf(stderr, "Missing argument\n");
			return -1;
		case '?':
			fprintf(stderr, "Unknown option '%c'\n", optopt);
			return -1;
		}
	}

	if (!ctx.actions) {
		ctx.actions |= ACTION_LIST;
	}

	if (optind < argc) {
		ctx.n_input_files = argc - optind;
		if (input_list) {
			if (ctx.n_input_files != 1) {
				fprintf(stderr,
						"Only a single input file may be used with --file\n");
				return -1;
			}
			ctx.n_input_files = read_file_list(argv[optind], &ctx.input_files);
			if (ctx.n_input_files < 0) {
				return -1;
			}
		} else {
			int i;
			ctx.input_files =
				malloc(sizeof(*ctx.input_files) * ctx.n_input_files);
			if (!ctx.input_files) {
				fprintf(stderr,
						"Couldn't allocate space for input filenames\n");
				ctx.n_input_files = 0;
			}
			for (i = 0; i < ctx.n_input_files; i++) {
				char *infile = argv[optind + i];
				ctx.input_files[i] = malloc(strlen(infile) + 1);
				if (!ctx.input_files[i]) {
					fprintf(stderr, "Couldn't allocate input filename\n");
					ctx.n_input_files = i;
				}
				strcpy(ctx.input_files[i], infile);
			}
		}
	}

	return 0;
}

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
	if (cont->primary_asset) {
		dump_object(cont->primary_asset);
	} else {
		printf("[NULL]\n");
	}

	if (cont->shared_resource_count) {
		printf("Shared Assets:\n");
		for (i = 0; i < cont->shared_resource_count; i++) {
			if (cont->shared_resources[i]) {
				dump_object(cont->shared_resources[i]);
			} else {
				printf("[NULL]\n");
			}
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

	if (cont->hdr.flags & FLAG_COMPRESSED) {
		fprintf(stderr, "Compressed files not supported\n");
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
	int i;
	int res = 0;

	res = parse_options(argc, argv);
	if (res < 0) {
		print_usage(argc, argv);
		res = 1;
		goto exit;
	} else if (res != 0) {
		res = 1;
		goto exit;
	}

	for (i = 0; i < ctx.n_input_files; i++) {
		struct xnb_container *cont;
		FILE *fp;

		if (!ctx.quiet)
			printf("Loading file %i/%i: %s\n", i + 1, ctx.n_input_files,
					ctx.input_files[i]);

		fp = fopen(ctx.input_files[i], "r");
		if (!fp) {
			fprintf(stderr, "Opening '%s' for reading failed\n",
					ctx.input_files[i]);
			continue;
		}

		cont = read_container(fp);
		fclose(fp);
		if (!cont) {
			continue;
		}

		if ((ctx.actions & ACTION_LIST) & !ctx.quiet)
			dump_container(cont);

		if (ctx.actions * ACTION_EXPORT) {
			char filename[MAX_NAME_LEN];
			char *p;
			int j;

			p = ctx.basename;
			if (!p) {
				p = ctx.input_files[i];
			}

			if (cont->primary_asset) {
				if (ctx.output_prefix) {
					snprintf(filename, MAX_NAME_LEN, "%s/%s",
							ctx.output_prefix, p);
				} else {
					snprintf(filename, MAX_NAME_LEN, "%s", p);
				}

				if (!ctx.quiet)
					printf("Exporting primary asset to (base): %s\n",
							filename);

				res = export_object(cont->primary_asset, filename);
				if (res)
					fprintf(stderr, "Couldn't export primary asset\n");
			}
			for (j = 0; j < cont->shared_resource_count; j++) {
				if (ctx.output_prefix) {
					snprintf(filename, MAX_NAME_LEN, "%s/%s_shared_%d",
							ctx.output_prefix, p, j + 1);
				} else {
					snprintf(filename, MAX_NAME_LEN, "%s_shared_%d", p, j + 1);
				}

				if (!ctx.quiet)
					printf("Exporting shared resource %i to (base): %s\n",
							j + 1, filename);

				res = export_object(cont->shared_resources[j], filename);
				if (res)
					fprintf(stderr, "Couldn't export shared resource %d\n", j);
			}
		}
		destroy_container(cont);
	}

exit:
	for (i = 0; i < ctx.n_input_files; i++) {
		free(ctx.input_files[i]);
	}
	free(ctx.input_files);
	return res;
}
