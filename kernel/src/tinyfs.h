#ifndef KMZ_TINYFS_H
#define KMZ_TINYFS_H

#include <stdint.h>

#define TINYFS_MAX_FILES 16
#define TINYFS_NAME_MAX 24
#define TINYFS_MAX_FILE_SIZE 2048

#define TINYFS_LBA_SUPERBLOCK 64
#define TINYFS_LBA_TABLE_START 65
#define TINYFS_LBA_TABLE_END 72
#define TINYFS_LBA_DATA_START 73
#define TINYFS_LBA_DATA_END 127

struct tinyfs_file {
    uint8_t used;
    char name[TINYFS_NAME_MAX + 1];
    uint32_t size;
    uint32_t start_lba;
    uint32_t sector_count;
    uint32_t checksum;
    uint8_t flags;
};

struct tinyfs_status_info {
    int available;
    int formatted;
    uint32_t file_count;
    uint32_t max_files;
    uint32_t data_start_lba;
    uint32_t max_file_size;
    const char *last_error;
};

void tinyfs_init(void);
int tinyfs_format(void);
uint32_t tinyfs_list(const struct tinyfs_file **out, uint32_t capacity);
int tinyfs_create(const char *name);
int tinyfs_write_text(const char *name, const char *text);
int tinyfs_read_text(const char *name, char *out,
                     uint32_t capacity, uint32_t *size_out);
int tinyfs_delete(const char *name);
const struct tinyfs_status_info *tinyfs_status(void);
void tinyfs_export_report(void);

#endif
