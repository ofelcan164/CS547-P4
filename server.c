#include <stdio.h>
#include "udp.h"
#include "types.h"
#include "mfs.h"

int fs_lookup(int pinum, char* name) {

}

int fs_stat(int inum) {

}

int fs_write(int inum, char* buffer, int block) {

}

int fs_read(int inum, int block) {

}

int fs_create(int pinum, int type, char* name) {

}

int fs_unlink(int pinum, char* name) {

}

int fs_shutdown() {

}


int fd;
struct checkpoint_region cr;
// server code
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: prompt> server [portnum] [file-system-image]\n");
    }

    // TODO: Initialize / Setup FS

    // File system image file does not exist
    if (access(argv[2], F_OK) == 0) {
        fd = open(argv[2], O_RDWR); // Open image file

        // Read checkpoint region
        read(fd, (char *)&cr, sizeof(struct checkpoint_region));
        fsync(fd);
    } else {
        fd = open(argv[2], O_RDWR | O_CREAT); // Open and create image file

        // Create checkpoint region
        cr.log_end_ptr = 0;
        for (int i = 0; i < NUM_IMAP_PIECES; i++) {
            cr.imap_pieces[i] = -1; // TODO et to -1 or init all pieces
        }

        // Add root directory
        MFS_DirEnt_t root_entry_dot;
        strcpy(root_entry_dot.name, ".");
        root_entry_dot.inum = 0;
        MFS_DirEnt_t root_entry_dot_dot;
        strcpy(root_entry_dot_dot.name, "..");
        root_entry_dot_dot.inum = 0;
        
        // Write . and .. entries to the data block and then the remaining empty entries
        int root_data_ptr = lseek(fd, sizeof(cr), SEEK_SET); // Move file image offset TODO where to seek end of inode map or end of CR
        write(fd, (char *)&root_entry_dot, sizeof(root_entry_dot));
        write(fd, (char *)&root_entry_dot_dot, sizeof(root_entry_dot_dot));
        for (int j = 0; int j < (MFS_BLOCK_SIZE - sizeof(root_entry_dot) - sizeof(root_entry_dot_dot)) / sizeof(MFS_DirEnt_t); j++) {
            MFS_DirEnt_t new_ent;

            new_ent.inum = -1;
            write(fd, (char *)&new_ent, sizeof(MFS_DirEnt_t));
        }

        // Create root directory inode
        struct inode root_dir;
        root_dir.type = MFS_DIRECTORY;
        for (int j = 0; j < NUM_POINTERS_PER_INODE; j++) {
            if (j == 0) {
                root_dir.pointers[j] = root_data_ptr;
            }
            else {
                root_dir.pointers[j] = -1;
            }
        }
        int root_dir_inode_ptr = lseek(fd, 0, SEEK_CUR);
        // TODO SIZE IN INODE
        write(fd, (char *)&root_dir, sizeof(root_dir));
        
        // Update the imap piece
        struct imap_piece root_piece;
        for (int j = 0; j < NUM_INODES_PER_PIECE; j++) {
            if (j == 0) {
                root_piece.inodes[j] = root_dir_inode_ptr;
            }
            else {
                root_dir.pointers[j] = -1;
            }
        }
        int root_imap_piece_ptr = lseek(fd, 0, SEEK_CUR);
        write(fd, (char *)&root_piece, sizeof(root_piece)); // Write the imap piece

        cr.imap_pieces[0] = root_imap_piece_ptr;
        cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

        lseek(fd, 0, SEEK_SET);
        write(fd, (char *)&cr, sizeof(cr));
        
        fsync(fd); // Force to disk
    }

    int sd = UDP_Open(atoi(argv[1]));
    assert (sd > -1);

    while (1) {
        struct sockaddr_in addr;
        struct request req;
        int rc = UDP_Read(sd, &addr, (char *) &req, REQ_SIZE);

        if (rc > 0) {
            switch (req.type) {
            case LOOKUP:
                break;
            case STAT:
                break;
            case WRITE:
                break;
            case READ:
                break;
            case CREAT:
                break;
            case UNLINK:
                break;
            case SHUTDOWN:
                break;
            default:
                break;
            }
        }
    }

    return 0;
}