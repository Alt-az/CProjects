#ifndef FAT_FILE_READER_H
#define FAT_FILE_READER_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
struct disk_t{
    FILE *fp;
};
struct fat_super_t {
    uint8_t  jump_code[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;//sectors in volume
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t chs_sectors_per_track;
    uint16_t chs_tracks_per_cylinder;
    uint32_t hidden_sectors;
    uint32_t logical_sectors32;//sectors in volume
    uint8_t  media_id;
    uint8_t  chs_head;
    uint8_t  ext_bpb_signature;
    uint32_t serial_number;
    char     volume_label[11];
    char     fsid[8];
    uint8_t  boot_code[448];
    uint16_t magic;
}__attribute__((packed));
struct volume_t {
    struct fat_super_t* super;
    struct disk_t* disk;
    uint8_t *fat;
    uint16_t total_sectors;
    int32_t fat_size;
    uint16_t root_dir_sectors;
    uint16_t first_data_sector;
    uint16_t first_fat_sector;
    uint16_t first_dir_sector;
    uint32_t data_sectors;
    uint32_t total_clusters;

} __attribute__ (( packed ));
struct dir_entry_t{
    char name[13];
    u_int32_t size;
    u_int8_t is_archived:1;
    u_int8_t is_readonly:1;
    u_int8_t is_system:1;
    u_int8_t is_hidden:1;
    u_int8_t is_directory:1;
    uint8_t is_volume:1;
    struct __attribute__((packed)){
        u_int16_t year:7;
        u_int16_t month:4;
        u_int16_t day:5;
    }creation_date;
    struct __attribute__((packed)){
        u_int16_t hour:5;
        u_int16_t minute:6;
        u_int16_t second:5;
    }creation_time;
    uint16_t first_cluster;
}__attribute__((packed));
struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};
struct dir_t{
    struct volume_t* vol;
};
struct file_t{
    struct volume_t* vol;
    struct clusters_chain_t* clustersChain;
    uint32_t size;
    uint32_t pos;
};
union datepack{
    struct{
        u_int16_t day:5;
        u_int16_t month:4;
        u_int16_t year:7;
    };
    u_int16_t date;
}__attribute__((packed));
union timepack{
    struct{
        u_int16_t second:5;
        u_int16_t minute:6;
        u_int16_t hour:5;
    };
    u_int16_t time;
}__attribute__((packed));
struct dir_info{
    struct volume_t* volume;
    int size;
    u_int8_t isOpen;
}dirInfo;

struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);
struct dir_entry_t *read_directory_entry(struct volume_t* volume);
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster);
#endif //FAT_FILE_READER_H
