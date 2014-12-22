/* XNB Generic Object Interface
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 */

#include <stdint.h>
#include <stdio.h>

#define MAX_NAME_LEN 256

struct xnb_object_reader;
/* Add to these for new objects */
extern const struct xnb_object_reader sound_effect_reader;
enum xnb_object_type {
	XNB_OBJ_SOUND_EFFECT,
};

/* Put this at the top of any specific object structures */
struct xnb_object_head {
	enum xnb_object_type type;
};

struct type_reader_desc {
	char name[MAX_NAME_LEN];
	int32_t version;
};

struct xnb_object_reader {
	char name[MAX_NAME_LEN];
	enum xnb_object_type type;
	struct xnb_object_head *(*deserialize)(FILE *fp);
	void (*destroy)(struct xnb_object_head *obj);
	void (*print)(struct xnb_object_head *obj);
	int (*export)(struct xnb_object_head *obj, char basename);
};

void dump_object(struct xnb_object_head *obj);
void destroy_object(struct xnb_object_head *obj);
struct xnb_object_head *read_object(struct type_reader_desc *rdr, FILE *fp);
