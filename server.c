#include <stdio.h>
#include "udp.h"
#include "types.h"
#include "mfs.h"

// INODE # math:
//  Given inode #:
//      imap piece # = inode # / 16 (truncated/rounded down)
//      imap piece index = inode # % 16

int fs_lookup(int pinum, char* name) {
    // Nate
}

/**
 * Returns some information about the file specified by inum. 
 * Upon success, return 0, otherwise -1. 
 * The exact info returned is defined by MFS_Stat_t. 
 * Failure modes: inum does not exist.
 */
int fs_stat(int inum) {
    // Stat struct to be returned in respose
    MFS_Stat_t m;

    // Check for valid inum
    int piece_num = inum / NUM_INODES_PER_PIECE;
    int idx = inum % NUM_INODES_PER_PIECE;
    int rc = -1;
    if (cr_imap_pieces[piece_num][idx] != NULL) {
        struct inode node = cr_imap_pieces[piece_num][idx];
        m.size = node.size;
        m.type = node.type;
        rc = 0;
    }

    // Construct response to client
    struct response resp;
    resp.rc = rc;
    resp.m = m;

    // Send response
    UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);

    return rc;
}

int fs_write(int inum, char* buffer, int block) {
    // Nate
}

int fs_read(int inum, int block) {
    // Nate
}

int fs_create(int pinum, int type, char* name) {
    // OScar
}

int fs_unlink(int pinum, char* name) {
    // Oscar
}

int fs_shutdown() {
    // Nate
}

int sd; // Socekt descriptor of server
struct sockaddr_in addr; // Address of socket for communcation with client
int fd; // File descriptor of open FS image file
struct checkpoint_region cr; // Checkpoint region - in memory
struct imap_piece cr_imap_pieces[NUM_IMAP_PIECES]; // Full imap - array of imap pieces
// server code
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: prompt> server [portnum] [file-system-image]\n");
    }

    // Initialize / Setup FS

    // File system image file does not exist
    if (access(argv[2], F_OK) == 0) {
        fd = open(argv[2], O_RDWR); // Open image file

        // Read checkpoint region
        read(fd, (char *)&cr, sizeof(struct checkpoint_region));
        fsync(fd);
    } else {
        fd = open(argv[2], O_RDWR | O_CREAT); // Open and create image file

        // Create checkpoint region
        for (int i = 0; i < NUM_IMAP_PIECES; i++) {
            cr.imap_piece_ptrs[i] = -1; // TODO set to -1 or init all pieces
            // Create empty pieces and fill in memory imap
            struct imap_piece piece;
            for (int j = 0; j < NUM_INODES_PER_PIECE; j++) {
                piece.inodes[j] = NULL;
            }
            cr_imap_pieces[i] = piece;
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
        cr_imap_pieces[0] = root_piece; // Update in memory imap
        cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

        lseek(fd, 0, SEEK_SET);
        write(fd, (char *)&cr, sizeof(cr));
        
        fsync(fd); // Force to disk
    }

    sd = UDP_Open(atoi(argv[1]));
    assert (sd > -1);

    while (1) {
        struct request req;
        int rc = UDP_Read(sd, &addr, (char *) &req, REQ_SIZE);

        if (rc > 0) {
            switch (req.type) {
            case LOOKUP:
                break;
            case STAT:
                fs_stat(req.inum);
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