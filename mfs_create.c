void fs_create(int pinum, int type, char* name) {
    if (pinum < 0 || pinum >= NUM_INDODES || imap[pinum].size == -1 || imap[pinum].type == MFS_DIRECTORY) {
        sendFailedResponse();
        return;
    }

    int found = fs_lookup(pinum, name, 1);
    if (found != -1) {
        // Return success TODO
    }

    struct inode pnode = imap[pinum]; // get pnode
    int full = 1;
    int block_num = -1; // Block where new entry will reside
    int entry_off = -1; // Entry number/offset into block
    // Loop through pnode data blocks
    for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {

        MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
        lseek(fd, pnode.pointers[i], SEEK_SET); // TODO CHECK i != -1 AND THEN ALLOCATE NEW DATA BLOCK IF NEEDED
        read(fd, &block, sizeof(block));

        // Loop through dir entries
        for (int j = 0; j < NUM_DIR_ENTRIES_PER_BLOCK; j++) {
            if (block[j].inum == -1 && full == 1) {
                // Found a empty entry
                full = 0;
                block_num = i;
                entry_off = j;
            }
        }
    }

    if (full == 1) {
        sendFailedResponse();
        return;
    }

    int inode_num = -1;
    for (int i = 0; i < NUM_INODES; i++) {
        if (imap[i].size == -1) {
            inode_num = i;
            break;
        }
    }
    if (inode_num == -1) {
        sendFailedResponse();
        return;
    }

    // Create inode
    struct inode node;
    node.size = 0;

    int data_ptr = -1;
    if (type == MFS_DIRECTORY) {
        // Write data block
        data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
        MFS_DirEnt_t new_block[NUM_DIR_ENTRIES_PER_BLOCK];
        // Write . and .. entries to the data block and then the remaining empty entries
        strcpy(new_block[0].name, ".");
        new_block[0].inum = inode_num;
        strcpy(new_block[1].name, "..");
        new_block[1].inum = pinum;
        write(fd, &new_block, sizeof(new_block));
        node.size = MFS_BLOCK_SIZE;
    }

    // Update and write inode
    node.pointers[0] = data_ptr;
    for (int i = 1; i < NUM_POINTERS_PER_INODE; i++) {
        node.pointers[i] = -1;
    }
    node.type = type;
    int node_ptr = lseek(fd, 0, SEEK_CUR);
    write(fd, &node, sizeof(node))
    
    // Update the imap piece
    struct imap_piece piece;
    int piece_num = inode_num / NUM_INODES_PER_PIECE;
    int piece_idx = inode_num % NUM_INODES_PER_PIECE;
    lseek(fd, cr.imap_piece_pts[piece_num], SEEK_SET);
    read(fd, &piece, sizeof(piece));
    piece.inode_ptrs[idx] = node_ptr;
    int piece_ptr = lseek(fd, 0, SEEK_CUR);
    write(fd, (char *)&piece, sizeof(piece)); // Write the imap piece

    // Update cr and in memory imap
    cr.imap_piece_ptrs[piece_num] = piece_ptr;
    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
    imap[inode_num] = node;

    // Update parent
    // Get ppiece
    struct imap_piece ppiece;
    lseek(fd, cr.imap_piece_ptrs[pinum / NUM_INODES_PER_PIECE], SEEK_SET);
    read(fd, &ppiece, sizeof(ppiece));

    // Get data block to insert entry
    MFS_DirEnt_t pblock[NUM_DIR_ENTRIES_PER_BLOCK];
    lseek(fd, pnode.pointers[block_num], SEEK_SET);
    read(fd, &pblock, sizeof(pblock));
    
    // Add entry
    pblock[entry_off].inum = inode_num;
    strcpy(pblock[entry_off].name, name);

    // Write pblock
    int pblock_data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
    write(fd, &pblock, sizeof(pblock));

    // Update and write pnode
    pnode.pointers[block_num] = pblock_data_ptr;
    int pnode_ptr = lseek(fd, 0, SEEK_CUR);
    write(fd, &pnode, sizeof(pnode));

    // Update and write ppiece
    ppiece.inode_ptrs[pinum % NUM_INODES_PER_PIECE];
    int ppiece_ptr = lseek(fd, 0, SEEK_CUR);
    write(fd, &ppiece, sizeof(ppiece));

    // Update CR and in mem imap
    cr.imap_piece_ptrs[pinum / NUM_INODES_PER_PIECE] = ppiece_ptr;
    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
    imap[pinum] = pnode;
    lseek(fd, 0, SEEK_SET);
    write(fd, &cr, sizeof(cr));

    fsync(fd);
    // SEND SUCCESS MESSAGE
}