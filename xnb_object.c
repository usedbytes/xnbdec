/* XNB Generic Object Interface
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "xnb_object.h"

/* Add to this for new object types */
const struct xnb_object_reader *readers[] = {
	&sound_effect_reader,
	NULL,
};

void dump_object(struct xnb_object_head *obj)
{
	assert(obj != NULL);
	assert(obj->reader != NULL);
	obj->reader->print(obj);
}

void destroy_object(struct xnb_object_head *obj)
{
	if (obj == NULL)
		return;
	assert(obj->reader != NULL);
	obj->reader->destroy(obj);
}

struct xnb_object_head *read_object(struct type_reader_desc *rdr, FILE *fp)
{
	int i = 0;
	while (readers[i]) {
		const struct xnb_object_reader *reader = readers[i];
		if (!strcmp(reader->name, rdr->name))
			return reader->deserialize(fp);
		i++;
	}
	fprintf(stderr, "Unsupported reader '%s'\n", rdr->name);
	return NULL;
}

int export_object(struct xnb_object_head *obj, char *basename)
{
	assert(obj != NULL);
	assert(obj->reader != NULL);

	if (obj->reader->export) {
		return obj->reader->export(obj, basename);
	} else {
		fprintf(stderr, "No exporter found for reader '%s'\n",
				obj->reader->name);
		return -ENOENT;
	}
}
