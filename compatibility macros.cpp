#define USE_THE_INDEX_COMPATIBILITY_MACROS
#include "cache.h"
#include "config.h"
#include "builtin.h"
#include "diff.h"
#include "parse-options.h"
#include "userdiff.h"
#include "streaming.h"
#include "tree-walk.h"
#include "oid-array.h"
#include "packfile.h"
#include "object-store.h"
#include "promisor-remote.h"

struct batch_options {
	int enabled;
	int follow_symlinks;
	int print_contents;
	int buffer_output;
	int all_objects;
	int unordered;
	int cmdmode; /* may be 'w' or 'c' for --filters or --textconv */
	const char *format;
};

static const char *force_path;

static int filter_object(const char *path, unsigned mode,
			 const struct object_id *oid,
			 char **buf, unsigned long *size)
{
	enum object_type type;

	*buf = read_object_file(oid, &type, size);
	if (!*buf)
		return error(_("cannot read object %s '%s'"),
			     oid_to_hex(oid), path);
	if ((type == OBJ_BLOB) && S_ISREG(mode)) {
		struct strbuf strbuf = STRBUF_INIT;
		struct checkout_metadata meta;

		init_checkout_metadata(&meta, NULL, NULL, oid);
		if (convert_to_working_tree(&the_index, path, *buf, *size, &strbuf, &meta)) {
			free(*buf);
			*size = strbuf.len;
			*buf = strbuf_detach(&strbuf, NULL);
		}
	}

	return 0;
}

static int stream_blob(const struct object_id *oid)
{
	if (stream_blob_to_fd(1, oid, NULL, 0))
		die("unable to stream %s to stdout", oid_to_hex(oid));
	return 0;
}

static int cat_one_file(int opt, const char *exp_type, const char *obj_name,
			int unknown_type)
{
	struct object_id oid;
	enum object_type type;
	char *buf;
	unsigned long size;
	struct object_context obj_context;
	struct object_info oi = OBJECT_INFO_INIT;
	struct strbuf sb = STRBUF_INIT;
	unsigned flags = OBJECT_INFO_LOOKUP_REPLACE;
	const char *path = force_path;

	if (unknown_type)
		flags |= OBJECT_INFO_ALLOW_UNKNOWN_TYPE;

	if (get_oid_with_context(the_repository, obj_name,
				 GET_OID_RECORD_PATH,
				 &oid, &obj_context))
		die("Not a valid object name %s", obj_name);

	if (!path)
		path = obj_context.path;
	if (obj_context.mode == S_IFINVALID)
		obj_context.mode = 0100644;

	buf = NULL;
	switch (opt) {
	case 't':
		oi.type_name = &sb;
		if (oid_object_info_extended(the_repository, &oid, &oi, flags) < 0)
			die("git cat-file: could not get object info");
		if (sb.len) {
			printf("%s\n", sb.buf);
			strbuf_release(&sb);
			return 0;
		}
		break;

	case 's':
		oi.sizep = &size;
		if (oid_object_info_extended(the_repository, &oid, &oi, flags) < 0)
			die("git cat-file: could not get object info");
		printf("%"PRIuMAX"\n", (uintmax_t)size);
		return 0;

	case 'e':
		return !has_object_file(&oid);

	case 'w':
		if (!path)
			die("git cat-file --filters %s: <object> must be "
			    "<sha1:path>", obj_name);

		if (filter_object(path, obj_context.mode,
				  &oid, &buf, &size))
			return -1;
		break;

	case 'c':
		if (!path)
			die("git cat-file --textconv %s: <object> must be <sha1:path>",
			    obj_name);

		if (textconv_object(the_repository, path, obj_context.mode,
				    &oid, 1, &buf, &size))
			break;
		/* else fallthrough */

	case 'p':
		type = oid_object_info(the_repository, &oid, NULL);
		if (type < 0)
			die("Not a valid object name %s", obj_name);

		/* custom pretty-print here */
		if (type == OBJ_TREE) {
			const char *ls_args[3] = { NULL };
			ls_args[0] =  "ls-tree";
			ls_args[1] =  obj_name;
			return cmd_ls_tree(2, ls_args, NULL);
		}

		if (type == OBJ_BLOB)
			return stream_blob(&oid);
		buf = read_object_file(&oid, &type, &size);
		if (!buf)
			die("Cannot read object %s", obj_name);

		/* otherwise just spit out the data */
		break;

	case 0:
		if (type_from_string(exp_type) == OBJ_BLOB) {
			struct object_id blob_oid;
			if (oid_object_info(the_repository, &oid, NULL) == OBJ_TAG) {
				char *buffer = read_object_file(&oid, &type,
								&size);
				const char *target;
				if (!skip_prefix(buffer, "object ", &target) ||
				    get_oid_hex(target, &blob_oid))
					die("%s not a valid tag", oid_to_hex(&oid));
				free(buffer);
			} else
				oidcpy(&blob_oid, &oid);

			if (oid_object_info(the_repository, &blob_oid, NULL) == OBJ_BLOB)
				return stream_blob(&blob_oid);
			/*
			 * we attempted to dereference a tag to a blob
			 * and failed; there may be new dereference
			 * mechanisms this code is not aware of.
			 * fall-back to the usual case.
			 */
		}
		buf = read_object_with_reference(the_repository,
						 &oid, exp_type, &size, NULL);
		break;

	default:
		die("git cat-file: unknown option: %s", exp_type);
	}

	if (!buf)
		die("git cat-file %s: bad file", obj_name);

	write_or_die(1, buf, size);
	free(buf);
	free(obj_context.path);
	return 0;
}
