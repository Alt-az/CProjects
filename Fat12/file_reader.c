#include "file_reader.h"

struct disk_t* disk_open_from_file(const char* volume_file_name){
    if(volume_file_name==NULL){
        errno=EFAULT;
        return NULL;
    }
    struct disk_t* disk = malloc(sizeof(struct disk_t));
    if(disk==NULL){
        errno=ENOMEM;
        return NULL;
    }
    disk->fp= fopen(volume_file_name, "rb");
    if(disk->fp==NULL){
        free(disk);
        disk=NULL;
        errno=ENOENT;
    }
    return disk;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){
    if(pdisk==NULL || first_sector<=0 || buffer==NULL || sectors_to_read<=0){
        errno=EFAULT;
        return -1;
    }
    uint8_t *table=(uint8_t*)buffer;
    FILE *fp=pdisk->fp;
    fseek(fp,first_sector,SEEK_CUR);
    int amount=(int)fread(table,512,sectors_to_read,fp);
    if(amount/512!=sectors_to_read){
        errno=ERANGE;
        return -1;
    }
    return sectors_to_read;
}
int disk_close(struct disk_t* pdisk){
    if(pdisk==NULL){
        errno=EFAULT;
        return -1;
    }
    fclose(pdisk->fp);
    free(pdisk);
    return 0;
}
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if(pdisk==NULL){
        errno=EFAULT;
        return NULL;
    }
    struct volume_t* volume=malloc(sizeof(struct volume_t));
    if(volume==NULL){
        errno=ENOMEM;
        return NULL;
    }
    volume->super=malloc(sizeof(struct fat_super_t));
    if(volume->super==NULL){
        free(volume);
        errno=ENOMEM;
        return NULL;
    }
    fseek(pdisk->fp,first_sector,SEEK_SET);
    fread(volume->super,512,1,pdisk->fp);
    if(volume->super->magic!=0xAA55){
        free(volume->super);
        free(volume);
        errno=EINVAL;
        return NULL;
    }
    fseek(pdisk->fp,0,SEEK_END);
    volume->total_sectors=ftell(pdisk->fp)/volume->super->bytes_per_sector;
    volume->total_clusters=volume->total_sectors/volume->super->sectors_per_cluster;
    fseek(pdisk->fp,volume->super->reserved_sectors*volume->super->bytes_per_sector,SEEK_SET);
    volume->first_fat_sector=ftell(pdisk->fp)/volume->super->bytes_per_sector;
    volume->fat_size=volume->super->sectors_per_fat*volume->super->bytes_per_sector;
    volume->fat=malloc(sizeof(uint8_t)*volume->fat_size);
    fread(volume->fat,volume->fat_size,1,pdisk->fp);
    fseek(pdisk->fp,volume->fat_size,SEEK_CUR);
    volume->first_dir_sector=ftell(pdisk->fp)/volume->super->bytes_per_sector;
    volume->root_dir_sectors=(volume->super->root_dir_capacity*32)/volume->super->bytes_per_sector;//(volume->super->root_dir_capacity%volume->super->bytes_per_sector==0)?(volume->super->root_dir_capacity/volume->super->bytes_per_sector):(volume->super->root_dir_capacity/volume->super->bytes_per_sector)+1;
    fseek(pdisk->fp,volume->root_dir_sectors*volume->super->bytes_per_sector,SEEK_CUR);
    volume->first_data_sector= ftell(pdisk->fp)/volume->super->bytes_per_sector;
    volume->data_sectors=volume->total_sectors-volume->super->reserved_sectors-volume->super->sectors_per_fat*volume->super->fat_count-volume->root_dir_sectors;
    volume->disk=pdisk;
    return volume;
}
int fat_close(struct volume_t* pvolume){
    if(pvolume==NULL){
        errno=EFAULT;
        return -1;
    }
    free(pvolume->super);
    free(pvolume->fat);
    free(pvolume);
    return 0;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name){
    if(pvolume==NULL || file_name==NULL){
        errno=EFAULT;
        return NULL;
    }
    struct dir_entry_t* dirEntry = NULL;
    struct file_t *file=malloc(sizeof(struct file_t));
    if(file==NULL){
        errno=ENOMEM;
        return NULL;
    }
    dirInfo.isOpen = 0;
    dirEntry = read_directory_entry(pvolume);
    while(dirEntry!=NULL){
        if(strcmp(dirEntry->name,file_name)==0){
            if(dirEntry->is_directory==1 || dirEntry->is_volume==1){
                errno=EISDIR;
                free(file);
                free(dirEntry);
                return NULL;
            }
            file->vol=pvolume;
            file->clustersChain = get_chain_fat12(file->vol->fat,file->vol->fat_size,dirEntry->first_cluster);
            file->size = dirEntry->size;
            file->pos=0;
            free(dirEntry);
            return file;
        }
        free(dirEntry);
        dirEntry = read_directory_entry(NULL);
    }
    errno=ENOENT;
    free(file);
    return NULL;
}
int file_close(struct file_t* stream){
    if(stream==NULL){
        errno=EFAULT;
        return -1;
    }
    free(stream->clustersChain->clusters);
    free(stream->clustersChain);
    free(stream);
    return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){
    if(ptr==NULL || size==0 || nmemb==0 || stream==NULL){
        return -1;
    }
    uint32_t bytesleft = size*nmemb;
    bytesleft=bytesleft>stream->size-stream->pos?stream->size-stream->pos:bytesleft;
    uint32_t elemloaded = 0;
    uint8_t *table = (uint8_t*)ptr;
    int len=(int)strlen((char*)ptr);
    if(len<0){
        return 0;
    }
    uint16_t whichsector = stream->pos/stream->vol->super->bytes_per_sector;
    uint16_t whichcluster = whichsector/stream->vol->super->sectors_per_cluster;
    uint32_t startposition=stream->vol->first_data_sector*stream->vol->super->bytes_per_sector;
    fseek(stream->vol->disk->fp,startposition+((*(stream->clustersChain->clusters+whichcluster)-2)*stream->vol->super->bytes_per_sector*stream->vol->super->sectors_per_cluster),SEEK_SET);
    fseek(stream->vol->disk->fp,stream->pos%(stream->vol->super->bytes_per_sector*stream->vol->super->sectors_per_cluster),SEEK_CUR);
    uint32_t cluster_bytes=(stream->vol->super->bytes_per_sector*stream->vol->super->sectors_per_cluster);
    uint32_t bytestoload = ((cluster_bytes-(stream->pos%cluster_bytes))>(bytesleft)?bytesleft:(cluster_bytes-(stream->pos%cluster_bytes)));
    while(bytesleft>0){
        elemloaded+=fread(table,size,bytestoload/size,stream->vol->disk->fp);
        table += bytestoload;
        bytesleft -= bytestoload;
        bytestoload = cluster_bytes>bytesleft?bytesleft:cluster_bytes;
        whichcluster++;
        if(whichcluster>=stream->clustersChain->size){
            break;
        }
        fseek(stream->vol->disk->fp,startposition+((*(stream->clustersChain->clusters+whichcluster)-2)*stream->vol->super->bytes_per_sector*stream->vol->super->sectors_per_cluster),SEEK_SET);
    }
    stream->pos+=elemloaded*size;
    return elemloaded;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    if(stream==NULL || offset==0){
        errno=EFAULT;
        return -1;
    }
    if(whence!=SEEK_SET && whence!=SEEK_CUR && whence!=SEEK_END){
        errno=EINVAL;
        return -1;
    }
    if(whence==SEEK_SET){
        if(offset>(int32_t)stream->size){
            errno=ENXIO;
            return -1;
        }
        stream->pos = offset;
    }
    else if(whence==SEEK_END){
        if((-offset)>(int32_t)stream->size){
            errno=ENXIO;
            return -1;
        }
        stream->pos=(uint32_t)((int32_t)stream->size+offset);
    }
    else{
        if((offset>0 && offset>(int32_t)(stream->size-stream->pos)) || (offset<0 && (-offset)>(int32_t)stream->pos)){
            errno=ENXIO;
            return -1;
        }
        stream->pos+=offset;
    }
    return 1;
}
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path) {
    if (pvolume == NULL || dir_path == NULL) {
        errno=EFAULT;
        return NULL;
    }
    if(strcmp(dir_path,"\\")==0){
        struct dir_t* dir=malloc(sizeof(struct dir_t));
        if(dir==NULL){
            errno=ENOMEM;
            return NULL;
        }
        dir->vol=pvolume;
        dirInfo.isOpen = 0;
        return dir;
    }
    errno=ENOENT;
    return NULL;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    if(pdir==NULL || pentry==NULL){
        return -1;
    }
    struct dir_entry_t* tmp=NULL;
    if(dirInfo.isOpen == 1){
        tmp = read_directory_entry(NULL);
    }
    else{
        tmp = read_directory_entry(pdir->vol);
    }
    if(tmp==NULL){
        return 1;
    }
    memcpy(pentry,tmp,32);
    free(tmp);
    return 0;
}
int dir_close(struct dir_t* pdir){
    if(pdir==NULL){
        errno=EFAULT;
        return -1;
    }
    free(pdir);
    return 0;
}
struct dir_entry_t *read_directory_entry(struct volume_t* volume){
    if(volume!=NULL){
        if(dirInfo.isOpen){
            fclose(volume->disk->fp);
        }
        dirInfo.isOpen=1;
        fseek(volume->disk->fp,0L,SEEK_END);
        dirInfo.size=(int)ftell(volume->disk->fp);
        fseek(volume->disk->fp,volume->first_dir_sector*volume->super->bytes_per_sector,SEEK_SET);
        dirInfo.volume=volume;
    }
    else{
        volume=dirInfo.volume;
    }
    struct dir_entry_t* dirEntry = NULL;
    if((dirInfo.size - ftell(volume->disk->fp)) >= 32) {
        dirEntry = malloc(sizeof(struct dir_entry_t));
        if(dirEntry==NULL){
            errno=ENOMEM;
            return NULL;
        }
        u_int8_t check;
        fread(&check, 1, 1, volume->disk->fp);
        if(check==0xe5){
            fseek(volume->disk->fp,31,SEEK_CUR);
            free(dirEntry);
            return read_directory_entry(NULL);
        } else if(check==0x00){
            free(dirEntry);
            /*if(dirInfo.isOpen){
                fclose(volume->disk->fp);
            }*/
            dirInfo.isOpen=0;
            return NULL;
        }
        fseek(volume->disk->fp, -1, SEEK_CUR);
        memset(dirEntry->name, '\0', 13);
        char txt[9]={'\0'};
        fread(txt, 8, 1, volume->disk->fp);
        char *tmp=strtok(txt," ");
        strcat(dirEntry->name,tmp);
        char ext[3];
        fread(ext, 3, 1, volume->disk->fp);
        if(*ext!=' '){
            strcat(dirEntry->name,".");
            for(int i=0;i<3;i++){
                if(*(ext+i)!=' '){
                    strncat(dirEntry->name,ext+i,1);
                }
            }
        }
        u_int8_t atr;
        fread(&atr, 1, 1, volume->disk->fp);
        u_int8_t oneatr = atr & 0xf0;
        u_int8_t twoatr = atr & 0xf;
        dirEntry->is_system=0;
        dirEntry->is_hidden=0;
        dirEntry->is_readonly=0;
        dirEntry->is_archived=0;
        dirEntry->is_directory=0;
        dirEntry->is_volume = 0;
        if (oneatr == 0x10) {
            dirEntry->is_directory = 1;
        } else if (oneatr == 0x20) {
            dirEntry->is_archived = 1;
        } else if (oneatr ==0x30){
            dirEntry->is_directory = 1;
            dirEntry->is_archived = 1;
        }
        if (twoatr == 0x01) {
            dirEntry->is_readonly = 1;
        } else if (twoatr == 0x02) {
            dirEntry->is_hidden = 1;
        } else if(twoatr == 0x03){
            dirEntry->is_readonly = 1;
            dirEntry->is_hidden = 1;
        } else if (twoatr == 0x04) {
            dirEntry->is_system = 1;
        } else if (twoatr == 0x06){
            dirEntry->is_system = 1;
            dirEntry->is_hidden = 1;
        } else if(twoatr == 0x07){
            dirEntry->is_system = 1;
            dirEntry->is_readonly = 1;
            dirEntry->is_hidden = 1;
        } else if(twoatr == 0x05){
            dirEntry->is_system = 1;
            dirEntry->is_readonly = 1;
        } else if(twoatr == 0x08){
            dirEntry->is_volume = 1;
        }
        union datepack datepack;
        union timepack timepack;
        fseek(volume->disk->fp, 2, SEEK_CUR);
        fread(&timepack.time, 2, 1, volume->disk->fp);
        fread(&datepack.date, 2, 1, volume->disk->fp);
        dirEntry->creation_date.day = datepack.day;
        dirEntry->creation_date.month = datepack.month;
        dirEntry->creation_date.year = datepack.year;
        dirEntry->creation_time.hour = timepack.hour;
        dirEntry->creation_time.minute = timepack.minute;
        dirEntry->creation_time.second = timepack.second;
        fseek(volume->disk->fp, 8, SEEK_CUR);
        fread(&dirEntry->first_cluster,2,1,volume->disk->fp);
        fread(&dirEntry->size, 4, 1, volume->disk->fp);
    }
    else{
        //fclose(volume->disk->fp);
        dirInfo.isOpen=0;
    }
    return dirEntry;
}
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster){
    if(buffer==NULL || size==0 || first_cluster==0){
        return NULL;
    }
    uint8_t *table=(uint8_t*)buffer;
    struct clusters_chain_t* clustersChain=malloc(sizeof(struct clusters_chain_t));
    clustersChain->clusters= malloc(sizeof(uint16_t));
    *clustersChain->clusters=first_cluster;
    clustersChain->size=1;
    uint16_t nextone = *(table+(int)(first_cluster*1.5));
    uint16_t nexttwo = *(table+(int)(first_cluster*1.5)+1);
    uint16_t nextcluster;
    if(first_cluster%2==0){
        nextcluster=((nexttwo&0xf)<<8)|nextone;
    }
    else{
        nextcluster=(nexttwo<<4)|((nextone&0xf0)>>4);
    }
    while(nextcluster < 0xff8){
        clustersChain->clusters = realloc(clustersChain->clusters,sizeof(uint16_t)*(clustersChain->size+1));
        *(clustersChain->clusters+clustersChain->size)=nextcluster;
        clustersChain->size++;
        nextone = *(table+(int)(nextcluster*1.5));
        nexttwo = *(table+(int)(nextcluster*1.5)+1);
        if(nextcluster%2==0){
            nextcluster=((nexttwo&0xf)<<8)|nextone;
        }
        else{
            nextcluster=(nexttwo<<4)|((nextone&0xf0)>>4);
        }
    }
    return clustersChain;
}

