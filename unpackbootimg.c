#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>

#include "mincrypt/sha.h"
#include "bootimg.h"

typedef unsigned char byte;

int read_padding(FILE* f, unsigned itemsize, int pagesize)
{
	byte* buf = (byte*)malloc(sizeof(byte) * pagesize);
	unsigned pagemask = pagesize - 1;
	unsigned count;

	if ((itemsize & pagemask) == 0) {
		free(buf);
		return 0;
	}

	count = pagesize - (itemsize & pagemask);

	fread(buf, count, 1, f);
	free(buf);
	return count;
}

void write_string_to_file(char* file, char* string)
{
	FILE* f = fopen(file, "w");
	fwrite(string, strlen(string), 1, f);
	fwrite("\n", 1, 1, f);
	fclose(f);
}

int usage() {
	fprintf(stderr, "usage: unpackbootimg\n"
		"  -i|--input boot.img\n"
		"  [ -o|--output output_directory]\n"
		"  [ -p|--pagesize <size-in-hexadecimal> ]\n"
	);
	return 1;
}

int main(int argc, char** argv)
{
	struct stat st = {0};
	char tmp[PATH_MAX];
	char* directory = NULL;
	char* filename = NULL;
	int pagesize = 0;
	int base = 0;

	argc--;
	argv++;

	while(argc > 0){
		char *arg = argv[0];
		char *val = argv[1];
		argc -= 2;
		argv += 2;
		if(!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
			filename = val;
		} else if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
			directory = val;
		} else if(!strcmp(arg, "--pagesize") || !strcmp(arg, "-p")) {
			pagesize = strtoul(val, 0, 16);
		} else {
			return usage();
		}
	}

	if (filename == NULL)
		return usage();

	if (directory == NULL)
		directory = "split-img";

	if (stat(directory, &st) == -1) {
		mkdir(directory, 0755);
		stat(directory, &st);
	}

	if (!S_ISDIR(st.st_mode)) {
		printf("Could not create output directory.\n");
		return 1;
	}

	int total_read = 0;
	FILE* f = fopen(filename, "rb");
	boot_img_hdr header;

	int i;
	for (i = 0; i <= 512; i++) {
		fseek(f, i, SEEK_SET);
		fread(tmp, BOOT_MAGIC_SIZE, 1, f);
		if (memcmp(tmp, BOOT_MAGIC, BOOT_MAGIC_SIZE) == 0)
			break;
	}
	total_read = i;
	if (i > 512) {
		printf("Android boot magic not found.\n");
		return 1;
	}
	fseek(f, i, SEEK_SET);
	printf("Android magic found at: %d\n", i);

	fread(&header, sizeof(header), 1, f);
	base = header.kernel_addr - 0x00008000;

	printf("BOARD_KERNEL_CMDLINE %s\n", header.cmdline);
	printf("BOARD_KERNEL_BASE %08x\n", base);
	printf("BOARD_NAME %s\n", header.name);
	printf("BOARD_PAGE_SIZE %d\n", header.page_size);
	printf("BOARD_KERNEL_OFFSET %08x\n", header.kernel_addr - base);
	printf("BOARD_RAMDISK_OFFSET %08x\n", header.ramdisk_addr - base);

	if (header.second_size != 0)
		printf("BOARD_SECOND_OFFSET %08x\n", header.second_addr - base);

	printf("BOARD_TAGS_OFFSET %08x\n", header.tags_addr - base);

	if (header.dt_size != 0)
		printf("BOARD_DT_SIZE %d\n", header.dt_size);

	if (pagesize == 0)
		pagesize = header.page_size;

	sprintf(tmp, "%s/%s-cmdline", directory, basename(filename));
	write_string_to_file(tmp, header.cmdline);

	sprintf(tmp, "%s/%s-board", directory, basename(filename));
	write_string_to_file(tmp, header.name);

	sprintf(tmp, "%s/%s-base", directory, basename(filename));
	char basetmp[200];
	sprintf(basetmp, "%08x", base);
	write_string_to_file(tmp, basetmp);

	sprintf(tmp, "%s/%s-kernel_offset", directory, basename(filename));
	char kerneltmp[200];
	sprintf(kerneltmp, "%08x", header.kernel_addr - base);
	write_string_to_file(tmp, kerneltmp);

	sprintf(tmp, "%s/%s-ramdisk_offset", directory, basename(filename));
	char ramdisktmp[200];
	sprintf(ramdisktmp, "%08x", header.ramdisk_addr - header.kernel_addr + 0x00008000);
	write_string_to_file(tmp, ramdisktmp);

	sprintf(tmp, "%s/%s-second_offset", directory, basename(filename));
	char secondtmp[200];
	sprintf(secondtmp, "%08x", header.second_addr - header.kernel_addr + 0x00008000);
	write_string_to_file(tmp, secondtmp);

	sprintf(tmp, "%s/%s-tags_offset", directory, basename(filename));
	char tagstmp[200];
	sprintf(tagstmp, "%08x", header.tags_addr - header.kernel_addr + 0x00008000);
	write_string_to_file(tmp, tagstmp);

	sprintf(tmp, "%s/%s-pagesize", directory, basename(filename));
	char pagesizetmp[200];
	sprintf(pagesizetmp, "%d", header.page_size);
	write_string_to_file(tmp, pagesizetmp);

	total_read += sizeof(header);
	total_read += read_padding(f, sizeof(header), pagesize);

	sprintf(tmp, "%s/%s-kernel", directory, basename(filename));
	FILE *k = fopen(tmp, "wb");
	byte* kernel = (byte*)malloc(header.kernel_size);
	fread(kernel, header.kernel_size, 1, f);
	total_read += header.kernel_size;
	fwrite(kernel, header.kernel_size, 1, k);
	fclose(k);

	total_read += read_padding(f, header.kernel_size, pagesize);

	byte* ramdisk = (byte*)malloc(header.ramdisk_size);

	fread(ramdisk, header.ramdisk_size, 1, f);
	total_read += header.ramdisk_size;
	sprintf(tmp, "%s/%s-ramdisk", directory, basename(filename));
	FILE *r = fopen(tmp, "wb");
	fwrite(ramdisk, header.ramdisk_size, 1, r);
	fclose(r);

	total_read += read_padding(f, header.ramdisk_size, pagesize);

	sprintf(tmp, "%s/%s-second", directory, basename(filename));
	FILE *s = fopen(tmp, "wb");
	byte* second = (byte*)malloc(header.second_size);
	fread(second, header.second_size, 1, f);
	total_read += header.second_size;
	fwrite(second, header.second_size, 1, r);
	fclose(s);

	total_read += read_padding(f, header.second_size, pagesize);

	sprintf(tmp, "%s/%s-dt", directory, basename(filename));
	FILE *d = fopen(tmp, "wb");
	byte* dt = (byte*)malloc(header.dt_size);
	fread(dt, header.dt_size, 1, f);
	total_read += header.dt_size;
	fwrite(dt, header.dt_size, 1, r);
	fclose(d);

	fclose(f);

	printf("Total read: %d\n", total_read);
	return 0;
}
