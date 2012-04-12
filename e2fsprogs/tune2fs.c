/* vi: set sw=4 ts=4: */
/*
 * tune2fs: utility to modify EXT2 filesystem
 *
 * Busybox'ed (2009) by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include <linux/fs.h>
/*
 * Work around linux/ext2_fs.h breakage.
 * See https://bugzilla.kernel.org/show_bug.cgi?id=42986.
 */
typedef mode_t bb__umode_t;
#define umode_t bb__umode_t
#include <linux/ext2_fs.h>

// storage helpers
char BUG_wrong_field_size(void);
#define STORE_LE(field, value) \
do { \
	if (sizeof(field) == 4) \
		field = SWAP_LE32(value); \
	else if (sizeof(field) == 2) \
		field = SWAP_LE16(value); \
	else if (sizeof(field) == 1) \
		field = (value); \
	else \
		BUG_wrong_field_size(); \
} while (0)

#define FETCH_LE32(field) \
	(sizeof(field) == 4 ? SWAP_LE32(field) : BUG_wrong_field_size())

//usage:#define tune2fs_trivial_usage
//usage:       "[-c MAX_MOUNT_COUNT] "
////usage:     "[-e errors-behavior] [-g group] "
//usage:       "[-i DAYS] "
////usage:     "[-j] [-J journal-options] [-l] [-s sparse-flag] "
////usage:     "[-m reserved-blocks-percent] [-o [^]mount-options[,...]] "
////usage:     "[-r reserved-blocks-count] [-u user] "
//usage:       "[-C MOUNT_COUNT] "
//usage:       "[-L LABEL] "
////usage:     "[-M last-mounted-dir] [-O [^]feature[,...]] "
////usage:     "[-T last-check-time] [-U UUID] "
//usage:       "BLOCKDEV"
//usage:
//usage:#define tune2fs_full_usage "\n\n"
//usage:       "Adjust filesystem options on ext[23] filesystems"

enum {
	OPT_L = 1 << 0, // label
	OPT_c = 1 << 1, // max mount count
	OPT_i = 1 << 2, // check interval
	OPT_C = 1 << 3, // current mount count
};

int tune2fs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tune2fs_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opts;
	const char *label, *str_c, *str_i, *str_C;
	struct ext2_super_block *sb;
	int fd;

	opt_complementary = "=1";
	opts = getopt32(argv, "L:c:i:C:", &label, &str_c, &str_i, &str_C);
	if (!opts)
		bb_show_usage();
	argv += optind; // argv[0] -- device

	// read superblock
	fd = xopen(argv[0], O_RDWR);
	xlseek(fd, 1024, SEEK_SET);
	sb = xzalloc(1024);
	xread(fd, sb, 1024);

	// mangle superblock
	//STORE_LE(sb->s_wtime, time(NULL)); - why bother?

	if (opts & OPT_C) {
		int n = xatoi_range(str_C, 1, 0xfffe);
		STORE_LE(sb->s_mnt_count, (unsigned)n);
	}

	// set the label
	if (opts & OPT_L)
		safe_strncpy((char *)sb->s_volume_name, label, sizeof(sb->s_volume_name));

	if (opts & OPT_c) {
		int n = xatoi_range(str_c, -1, 0xfffe);
		if (n == 0)
			n = -1;
		STORE_LE(sb->s_max_mnt_count, (unsigned)n);
	}

	if (opts & OPT_i) {
		unsigned n = xatou_range(str_i, 0, (unsigned)0xffffffff / (24*60*60)) * 24*60*60;
		STORE_LE(sb->s_checkinterval, n);
	}

	// write superblock
	xlseek(fd, 1024, SEEK_SET);
	xwrite(fd, sb, 1024);

	if (ENABLE_FEATURE_CLEAN_UP) {
		free(sb);
	}

	xclose(fd);
	return EXIT_SUCCESS;
}
