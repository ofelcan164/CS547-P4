#include <stdio.h>
#include "udp.h"
#include "types.h"
#include "mfs.h"

// INODE # math:
//  Given inode #:
//      imap piece # = inode # / 16 (truncated/rounded down)
//      imap piece index = inode # % 16

// GLobals
int sd; // Socekt descriptor of server
struct sockaddr_in addr; // Address of socket for communcation with client
int fd; // File descriptor of open FS image file
struct checkpoint_region cr; // Checkpoint region - in memory
struct imap_piece cr_imap_pieces[NUM_IMAP_PIECES]; // Full imap - array of imap pieces

int fs_lookup(int pinum, char* name) {
    // Nate
    return 0;
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
    if (cr_imap_pieces[piece_num].inode_ptrs[idx] != -1) {
        lseek(fd, cr_imap_pieces[piece_num].inode_ptrs[idx], SEEK_SET);
        struct inode node;
        read(fd, (char *)&node, sizeof(node));
        m.size = node.size+1;
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
    return 0;
}

int fs_read(int inum, int block) {
    // Nate
    return 0;
}

/**
 * Makes a file (type == MFS_REGULAR_FILE) or directory (type == MFS_DIRECTORY) 
 * in the parent directory specified by pinum of name name. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, or name is too long. If name already exists, return success (think about why).
 */
int fs_create(int pinum, int type, char* name) {
    // TODO USE NUM_DIR_ENTRIES_PER_BLOCK and new struct where possible
    // Name length checked in MFS_Creat
    // Check if pinum exists
    int piece_num = pinum / NUM_INODES_PER_PIECE;
    int idx = pinum / NUM_INODES_PER_PIECE;

    int rc = -1;
    struct response resp;
    if (cr_imap_pieces[piece_num].inode_ptrs[idx] != -1) {
        // Pinum exists
        // Get p imap piece and pinode
        struct imap_piece ppiece = cr_imap_pieces[piece_num];
        lseek(fd, cr_imap_pieces[piece_num].inode_ptrs[idx], SEEK_SET);
        struct inode pnode;
        read(fd, (char *)&pnode, sizeof(pnode));

        // Search data blocks for name existing
        for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
            // Loop through this data block
            if (pnode.pointers[i] != -1) {
                lseek(fd, pnode.pointers[i], SEEK_SET); // Seek to the data block
                for (int j = 0; j < (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)); j++) {
                    MFS_DirEnt_t entry;
                    read(fd, (char *)&entry, sizeof(MFS_DirEnt_t));

                    if (entry.inum != -1) {
                        if (strcmp(entry.name, name) == 0) {
                            rc = 0;
                            break;
                        }
                    }
                }
            }

            if (rc != -1)
                break;
        }

        // Send response if already success (name found)
        if (rc != -1) {
            resp.rc = rc;
            UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
            return rc;
        }

        // Send response if no more entries availble in parent
        if ((pnode.size + 1) / MFS_BLOCK_SIZE / NUM_POINTERS_PER_INODE < 1) { 
            resp.rc = -1;
            UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
            return resp.rc;
        }

        // Find free inode number
        int inode_num = -1;
        for (int i = 0; i < NUM_IMAP_PIECES; i++) {
            for (int j = 0; j < NUM_INODES_PER_PIECE; j++) {
                if (cr_imap_pieces[i].inode_ptrs[j] == -1) {
                    inode_num = (i * NUM_INODES_PER_PIECE) + j;
                    break;
                }
            }
            if (inode_num != -1) {
                break;
            }
        }

        if (inode_num == -1) {
            // Return failure, no avaiable inode numbers
            resp.rc = -1;
            UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
            return -1;
        }

        // Determine type (must create new file)
        if (type == MFS_REGULAR_FILE) {
            // Create regular file

            // Create new inode
            struct inode new_node;
            new_node.size = 0;
            new_node.type = type;
            for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
                new_node.pointers[i] = -1;
            }
            
            // Write the new inode
            lseek(fd, cr.log_end_ptr, SEEK_SET);
            int new_node_ptr = lseek(fd, 0, SEEK_CUR);
            write(fd, (char *)&new_node, sizeof(new_node));

            // Create new version imap piece
            int new_piece_num = inode_num / NUM_INODES_PER_PIECE;
            int new_piece_idx = inode_num % NUM_INODES_PER_PIECE;
            struct imap_piece new_piece = cr_imap_pieces[new_piece_num];
            new_piece.inode_ptrs[new_piece_idx] = new_node_ptr;
            int new_piece_ptr = lseek(fd, 0, SEEK_CUR);

            // Write new imap piece
            write(fd, (char *)&new_piece, sizeof(new_piece));

            // Save to CR and in memory imap
            cr.imap_piece_ptrs[new_piece_num] = new_piece_ptr;
            cr_imap_pieces[new_piece_num] = new_piece;
            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

            // Update p data block, p inode and p imap piece (if necessary) 
            for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
                int dir_entry_written = 0;
                int data_ptr = pnode.pointers[i];
                if (data_ptr != -1) {
                    lseek(fd, data_ptr, SEEK_SET); // Seek to data block

                    // Loop through data block (Should the above check for existence of name be moved here? TODO)
                    for (int j = 0; j < (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)); j++) {
                        MFS_DirEnt_t entry;
                        read(fd, (char *)&entry, sizeof(MFS_DirEnt_t));

                        if (entry.inum == -1) {
                            // Insert directory entry
                            entry.inum = inode_num;
                            strcpy(entry.name, name);

                            lseek(fd, -sizeof(entry), SEEK_CUR); // Seek back to beggining of this entry
                            // Write dir entry to data block
                            write(fd, (char *)&entry, sizeof(entry));
                            dir_entry_written = 1;
                            break;
                        }
                    }
                    
                    if (dir_entry_written)
                        break;
                }
                else if (data_ptr == -1 && dir_entry_written != 1) {
                    // New block needs to be written to/allocated
                    data_ptr = cr.log_end_ptr;
                    lseek(fd, cr.log_end_ptr, SEEK_SET); // Seek to end of log

                    // Write/allocate data block
                    // Write first entry in this data block
                    MFS_DirEnt_t entry;
                    entry.inum = inode_num;
                    strcpy(entry.name, name);
                    write(fd, (char *)&entry, sizeof(entry));
                    // Write remaining, empty entries
                    for (int j = 0; j < (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)) - 1; j++) {
                        MFS_DirEnt_t empty_entry;
                        empty_entry.inum = -1;
                        write(fd, (char *)&empty_entry, sizeof(empty_entry));
                    }

                    // Update inode
                    pnode.pointers[i] = data_ptr;

                    // Write new pinode version
                    int new_pinode_ptr = lseek(fd, 0, SEEK_CUR);
                    write(fd, (char *)&pnode, sizeof(pnode));

                    // Write new imap piece version
                    int new_ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                    ppiece.inode_ptrs[idx] = new_pinode_ptr;
                    write(fd, (char *)&ppiece, sizeof(ppiece));

                    // Save to CR and in mem imap
                    cr_imap_pieces[piece_num] = ppiece;
                    cr.imap_piece_ptrs[piece_num] = new_ppiece_ptr;
                    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
                }
            }

            // Write CR
            lseek(fd, 0, SEEK_SET);
            write(fd, (char *)&cr, sizeof(cr));
            rc = 0;

        } else if (type == MFS_DIRECTORY) {
            // Create directory

            // Create new data block
            lseek(fd, cr.log_end_ptr, SEEK_SET); // Seek to log end
            // Write . and .. entries
            MFS_DirEnt_t dot;
            dot.inum = inode_num;
            strcpy(dot.name, ".");
            write(fd, (char *)&dot, sizeof(dot));
            MFS_DirEnt_t dot_dot;
            dot_dot.inum = pinum;
            strcpy(dot_dot.name, "..");
            write(fd, (char *)&dot_dot, sizeof(dot_dot));
            // Write remaining empty entries
            for (int i = 0; i < (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)) - 2*sizeof(MFS_DirEnt_t); i++) {
                MFS_DirEnt_t empty_entry;
                empty_entry.inum = -1;
                write(fd, (char *)&empty_entry, sizeof(empty_entry));
            }
            int data_block_ptr = cr.log_end_ptr; // Save pointer
            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR); // Update log end ptr

            // Create new inode
            struct inode new_node;
            new_node.size = (cr.log_end_ptr-1) - data_block_ptr;
            new_node.type = type;
            for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
                new_node.pointers[i] = -1;
            }
            new_node.pointers[0] = data_block_ptr;
            
            // Write the new inode
            int new_node_ptr = lseek(fd, 0, SEEK_CUR);
            write(fd, (char *)&new_node, sizeof(new_node));

            // Create new version imap piece
            int new_piece_num = inode_num / NUM_INODES_PER_PIECE;
            int new_piece_idx = inode_num % NUM_INODES_PER_PIECE;
            struct imap_piece new_piece = cr_imap_pieces[new_piece_num];
            new_piece.inode_ptrs[new_piece_idx] = new_node_ptr;
            int new_piece_ptr = lseek(fd, 0, SEEK_CUR);

            // Write new imap piece
            write(fd, (char *)&new_piece, sizeof(new_piece));

            // Save to CR and in memory imap
            cr.imap_piece_ptrs[new_piece_num] = new_piece_ptr;
            cr_imap_pieces[new_piece_num] = new_piece;
            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

            // Update p data block, p inode and p imap piece (if necessary) 
            for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
                int dir_entry_written = 0;
                int data_ptr = pnode.pointers[i];
                if (data_ptr != -1) {
                    lseek(fd, data_ptr, SEEK_SET); // Seek to data block

                    // Loop through data block (Should the above check for existence of name be moved here? TODO)
                    for (int j = 0; j < (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)); j++) {
                        MFS_DirEnt_t entry;
                        read(fd, (char *)&entry, sizeof(MFS_DirEnt_t));

                        if (entry.inum == -1) {
                            // Insert directory entry
                            entry.inum = inode_num;
                            strcpy(entry.name, name);

                            lseek(fd, -sizeof(entry), SEEK_CUR); // Seek back to beggining of this entry
                            // Write dir entry to data block
                            write(fd, (char *)&entry, sizeof(entry));
                            dir_entry_written = 1;
                            break;
                        }
                    }
                    
                    if (dir_entry_written)
                        break;
                }
                else if (data_ptr == -1 && dir_entry_written != 1) {
                    // New block needs to be written to/allocated
                    data_ptr = cr.log_end_ptr;
                    lseek(fd, cr.log_end_ptr, SEEK_SET); // Seek to end of log

                    // Write/allocate data block
                    // Write first entry in this data block
                    MFS_DirEnt_t entry;
                    entry.inum = inode_num;
                    strcpy(entry.name, name);
                    write(fd, (char *)&entry, sizeof(entry));
                    // Write remaining, empty entries
                    for (int j = 0; j < (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)) - 1; j++) {
                        MFS_DirEnt_t empty_entry;
                        empty_entry.inum = -1;
                        write(fd, (char *)&empty_entry, sizeof(empty_entry));
                    }

                    // Update inode
                    pnode.pointers[i] = data_ptr;

                    // Write new pinode version
                    int new_pinode_ptr = lseek(fd, 0, SEEK_CUR);
                    write(fd, (char *)&pnode, sizeof(pnode));

                    // Write new imap piece version
                    int new_ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                    ppiece.inode_ptrs[idx] = new_pinode_ptr;
                    write(fd, (char *)&ppiece, sizeof(ppiece));

                    // Save to CR and in mem imap
                    cr_imap_pieces[piece_num] = ppiece;
                    cr.imap_piece_ptrs[piece_num] = new_ppiece_ptr;
                    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
                }
            }

            // Write CR
            lseek(fd, 0, SEEK_SET);
            write(fd, (char *)&cr, sizeof(cr));
            rc = 0;
        }
    }

    fsync(fd);
    resp.rc = rc;
    UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
    return rc;
}

/**
 * Removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
 */
int fs_unlink(int pinum, char* name) {
    // Check if pinum exists
    int ppiece_num = pinum / NUM_INODES_PER_PIECE;
    int idx = pinum / NUM_INODES_PER_PIECE;

    int rc = -1;
    struct response resp;
    if (cr_imap_pieces[ppiece_num].inode_ptrs[idx] != -1) {
        // Pinum exists
        // Get p imap piece and pinode
        struct imap_piece ppiece = cr_imap_pieces[ppiece_num];
        lseek(fd, ppiece.inode_ptrs[idx], SEEK_SET);
        struct inode pnode;
        read(fd, (char *)&pnode, sizeof(pnode));

        // Search data blocks for name existing
        rc = 0;
        for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
            // Loop through this data block
            if (pnode.pointers[i] != -1) {
                lseek(fd, pnode.pointers[i], SEEK_SET); // Seek to the data block
                // Read directory data block
                MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
                read(fd, (char *)&block, sizeof(block));

                // Loop through dir entries in data block
                for (int j = 0; j < (NUM_DIR_ENTRIES_PER_BLOCK); j++) {
                    if (block[j].inum != -1) {
                        // Find name
                        if (strcmp(block[j].name, name) == 0) {
                            // Get inode to determine type
                            struct inode cur_node;
                            lseek(fd, cr_imap_pieces[block[j].inum / NUM_INODES_PER_PIECE].inode_ptrs[block[j].inum % NUM_INODES_PER_PIECE], SEEK_SET); 
                            read(fd, (char *)&cur_node, sizeof(cur_node));

                            // Regular File Case
                            if (cur_node.type == MFS_REGULAR_FILE) {
                                // Just set the inum to be -1
                                block[j].inum = -1;

                                // Write data block
                                int data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
                                write(fd, (char *)&block, sizeof(block));

                                // Update and write pinode
                                int pinode_ptr = lseek(fd, 0, SEEK_CUR);
                                pnode.pointers[i] = data_ptr;
                                write(fd, (char *)&pnode, sizeof(pnode));

                                // Update and write new imap piece
                                int ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                                ppiece.inode_ptrs[idx] = pinode_ptr;
                                write(fd, (char *)&ppiece, sizeof(ppiece));

                                // Update in memory imap and CR
                                cr_imap_pieces[ppiece_num] = ppiece;
                                cr.imap_piece_ptrs[ppiece_num] = ppiece_ptr;
                                rc = 1;
                            }
                            // Directory Case
                            else {
                                
                            }
                            
                            
                        }
                    }
                }
            }

            if (rc != 0) {
                if (rc == 1) {
                    rc = 0; // Hacky logic TODO
                }
                break;
            }
    }

    fsync(fd);
    resp.rc = rc;
    UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
    return rc;
}

int fs_shutdown() {
    // Nate
    return 0;
}

// Run the server
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
                piece.inode_ptrs[j] = -1;
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
        for (int j = 0; j < (MFS_BLOCK_SIZE - sizeof(root_entry_dot) - sizeof(root_entry_dot_dot)) / sizeof(MFS_DirEnt_t); j++) {
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
                root_piece.inode_ptrs[j] = root_dir_inode_ptr;
            }
            else {
                root_dir.pointers[j] = -1;
            }
        }
        int root_imap_piece_ptr = lseek(fd, 0, SEEK_CUR);
        write(fd, (char *)&root_piece, sizeof(root_piece)); // Write the imap piece

        cr.imap_piece_ptrs[0] = root_imap_piece_ptr;
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
                fs_lookup(req.pinum, req.name);
                break;
            case STAT:
                fs_stat(req.inum);
                break;
            case WRITE:
                fs_write(req.inum, req.buffer, req.block);
                break;
            case READ:
                fs_read(req.inum, req.block);
                break;
            case CREAT:
                fs_create(req.pinum, req.type, req.name);
                break;
            case UNLINK:
                fs_unlink(req.pinum, req.name);
                break;
            case SHUTDOWN:
                fs_shutdown();
                break;
            default:
                break;
            }
        }
    }

    return 0;
}