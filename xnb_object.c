/* XNB Generic Object Interface
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 */

#include <string.h>

#include "xnb_object.h"

/* Add to this for new object types */
const struct xnb_object_reader *readers[] = {
	&sound_effect_reader,
	NULL,
};

void dump_object(struct xnb_object_head *obj)
{
	int i = 0;

	if (obj == NULL)
		return;

	while (readers[i]) {
		const struct xnb_object_reader *reader = readers[i];
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
		const struct xnb_object_reader *reader = readers[i];
		if (reader->type == obj->type)
			return reader->destroy(obj);
		i++;
	}
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

