/******************************************************************************
*******************************************************************************
**
**  Copyright (C) 2005 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include <inttypes.h>
#include <linux_endian.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fsck.h"
#include "fsck_incore.h"
#include "bio.h"
#include "inode.h"
#include "util.h"
#include "block_list.h"
#include "metawalk.h"

static int remove_eattr_entry(struct fsck_sb *sdp, struct buffer_head *leaf_bh,
			struct gfs2_ea_header *curr,
			struct gfs2_ea_header *prev)
{
	log_warn("Removing EA located in block #%"PRIu64".\n",
		 BH_BLKNO(leaf_bh));
	if(!prev){
		curr->ea_type = GFS2_EATYPE_UNUSED;
	} else {
		prev->ea_rec_len =
			cpu_to_gfs2_32(gfs2_32_to_cpu(curr->ea_rec_len) +
				     gfs2_32_to_cpu(prev->ea_rec_len));
	}
	if(write_buf(sdp, leaf_bh, 0)){
		stack;
		log_err("EA removal failed.\n");
		return -1;
	}
	return 0;
}

int check_eattr_indir(struct fsck_inode *ip, uint64_t block,
		      uint64_t parent, struct buffer_head **bh,
		      void *private)
{
	int *update = (int *) private;
	struct fsck_sb *sbp = ip->i_sbd;
	struct block_query q;
	struct buffer_head *indir_bh;

	if(check_range(sbp, block)) {
		log_err("Extended attributes indirect block out of range...removing\n");
		ip->i_di.di_eattr = 0;
		*update = 1;
		return 1;
	}
	else if (block_check(sbp->bl, block, &q)) {
		stack;
		return -1;
	}
	else if(q.block_type != indir_blk) {
		log_err("Extended attributes indirect block invalid...removing\n");
		ip->i_di.di_eattr = 0;
		*update = 1;
		return 1;
	}
	else if(get_and_read_buf(sbp, block, &indir_bh, 0)){
		log_warn("Unable to read EA leaf block #%"PRIu64".\n",
			 block);
		ip->i_di.di_eattr = 0;
		*update = 1;
		return 1;
	}

	*bh = indir_bh;
	return 0;
}
int check_eattr_leaf(struct fsck_inode *ip, uint64_t block,
		     uint64_t parent, struct buffer_head **bh, void *private)
{
	int *update = (int *) private;
	struct fsck_sb *sbp = ip->i_sbd;
	struct block_query q;
	struct buffer_head *leaf_bh;

	if(check_range(sbp, block)) {
		log_err("Extended attributes block out of range...removing\n");
		ip->i_di.di_eattr = 0;
		*update = 1;
		return 1;
	}
	else if (block_check(sbp->bl, block, &q)) {
		stack;
		return -1;
	}
	else if(q.block_type != meta_eattr) {
		log_err("Extended attributes block invalid...removing\n");
		ip->i_di.di_eattr = 0;
		*update = 1;
		return 1;
	}
	else if(get_and_read_buf(sbp, block, &leaf_bh, 0)){
		log_warn("Unable to read EA leaf block #%"PRIu64".\n",
			 block);
		ip->i_di.di_eattr = 0;
		*update = 1;
		return 1;
	}

	*bh = leaf_bh;

	return 0;

}


static int check_eattr_entry(struct fsck_inode *ip,
			     struct buffer_head *leaf_bh,
			     struct gfs2_ea_header *ea_hdr,
			     struct gfs2_ea_header *ea_hdr_prev,
			     void *private)
{
	struct fsck_sb *sdp = ip->i_sbd;
	char ea_name[256];

	if(!ea_hdr->ea_name_len){
		log_err("EA has name length == 0\n");
		if(remove_eattr_entry(sdp, leaf_bh, ea_hdr, ea_hdr_prev)){
			stack;
			return -1;
		}
		return 1;
	}

	memset(ea_name, 0, sizeof(ea_name));
	strncpy(ea_name, (char *)ea_hdr + sizeof(struct gfs2_ea_header),
		ea_hdr->ea_name_len);

	if(!GFS2_EATYPE_VALID(ea_hdr->ea_type) &&
	   ((ea_hdr_prev) || (!ea_hdr_prev && ea_hdr->ea_type))){
		log_err("EA (%s) type is invalid (%d > %d).\n",
			ea_name, ea_hdr->ea_type, GFS2_EATYPE_LAST);
		if(remove_eattr_entry(sdp, leaf_bh, ea_hdr, ea_hdr_prev)){
			stack;
			return -1;
		}
		return 1;
	}

	if(ea_hdr->ea_num_ptrs){
		uint32_t avail_size;
		int max_ptrs;

		avail_size = sdp->sb.sb_bsize - sizeof(struct gfs2_meta_header);
		max_ptrs = (gfs2_32_to_cpu(ea_hdr->ea_data_len)+avail_size-1)/avail_size;

		if(max_ptrs > ea_hdr->ea_num_ptrs){
			log_err("EA (%s) has incorrect number of pointers.\n", ea_name);
			log_err("  Required:  %d\n"
				"  Reported:  %d\n",
				max_ptrs, ea_hdr->ea_num_ptrs);
			if(remove_eattr_entry(sdp, leaf_bh, ea_hdr, ea_hdr_prev)){
				stack;
				return -1;
			}
			return 1;
		} else {
			log_debug("  Pointers Required: %d\n"
				  "  Pointers Reported: %d\n",
				  max_ptrs,
				  ea_hdr->ea_num_ptrs);
		}

	}

	return 0;
}

int check_eattr_extentry(struct fsck_inode *ip, uint64_t *ea_ptr,
			 struct buffer_head *leaf_bh,
			 struct gfs2_ea_header *ea_hdr,
			 struct gfs2_ea_header *ea_hdr_prev,
			 void *private)
{
	struct block_query q;
	struct fsck_sb *sbp = ip->i_sbd;
	if(block_check(sbp->bl, gfs2_64_to_cpu(*ea_ptr), &q)) {
		stack;
		return -1;
	}
	if(q.block_type != meta_eattr) {
		if(remove_eattr_entry(sbp, leaf_bh, ea_hdr, ea_hdr_prev)){
			stack;
			return -1;
		}
		return 1;
	}
	return 0;
}

/* Go over all inodes with extended attributes and verify the EAs are
 * valid */
int pass1c(struct fsck_sb *sbp)
{
	uint64_t block_no = 0;
	struct buffer_head *bh;
	struct fsck_inode *ip = NULL;
	int update = 0;
	struct metawalk_fxns pass1c_fxns = { 0 };
	int error = 0;

	pass1c_fxns.check_eattr_indir = &check_eattr_indir;
	pass1c_fxns.check_eattr_leaf = &check_eattr_leaf;
	pass1c_fxns.check_eattr_entry = &check_eattr_entry;
	pass1c_fxns.check_eattr_extentry = &check_eattr_extentry;
	pass1c_fxns.private = (void *) &update;

	log_info("Looking for inodes containing ea blocks...\n");
	while (!find_next_block_type(sbp->bl, eattr_block, &block_no)) {

		log_info("EA in inode %"PRIu64"\n", block_no);
		if(get_and_read_buf(sbp, block_no, &bh, 0)) {
			stack;
			return -1;
		}
		if(copyin_inode(sbp, bh, &ip)) {
			stack;
			return -1;
		}

		log_debug("Found eattr at %"PRIu64"\n", ip->i_di.di_eattr);
		/* FIXME: Handle walking the eattr here */
		error = check_inode_eattr(ip, &pass1c_fxns);
		if(error < 0) {
			stack;
			return -1;
		}

		if(update) {
			gfs2_dinode_out(&ip->i_di, BH_DATA(bh));
			write_buf(sbp, bh, 0);
		}

		free_inode(&ip);
		relse_buf(sbp, bh);

		block_no++;
	}
	return 0;
}