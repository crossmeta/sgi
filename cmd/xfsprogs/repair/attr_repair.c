/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <libxfs.h>
#include <errno.h>

#include "globals.h"
#include "err_protos.h"
#include "attr_repair.h"
#include "dir.h"
#include "dinode.h"
#include "bmap.h"

static int acl_valid(struct acl *aclp);
static int mac_valid(mac_t lp);


/*
 * For attribute repair, there are 3 formats to worry about. First, is 
 * shortform attributes which reside in the inode. Second is the leaf
 * form, and lastly the btree. Much of this models after the directory
 * structure so code resembles the directory repair cases. 
 * For shortform case, if an attribute looks corrupt, it is removed.
 * If that leaves the shortform down to 0 attributes, it's okay and 
 * will appear to just have a null attribute fork. Some checks are done
 * for validity of the value field based on what the security needs are.
 * Calls will be made out to mac_valid or acl_valid libc libraries if
 * the security attributes exist. They will be cleared if invalid. No
 * other values will be checked. The DMF folks do not have current
 * requirements, but may in the future.
 *
 * For leaf block attributes, it requires more processing. One sticky
 * point is that the attributes can be local (within the leaf) or 
 * remote (outside the leaf in other blocks). Thinking of local only
 * if you get a bad attribute, and want to delete just one, its a-okay
 * if it remains large enough to still be a leaf block attribute. Otherwise,
 * it may have to be converted to shortform. How to convert this and when
 * is an issue. This call is happening in Phase3. Phase5 will capture empty
 * blocks, but Phase6 allows you to use the simulation library which knows
 * how to handle attributes in the kernel for converting formats. What we
 * could do is mark an attribute to be cleared now, but in phase6 somehow
 * have it cleared for real and then the format changed to shortform if
 * applicable. Since this requires more work than I anticipate can be
 * accomplished for the next release, we will instead just say any bad
 * attribute in the leaf block will make the entire attribute fork be
 * cleared. The simplest way to do that is to ignore the leaf format, and
 * call clear_dinode_attr to just make a shortform attribute fork with
 * zero entries. 
 *
 * Another issue with handling repair on leaf attributes is the remote
 * blocks. To make sure that they look good and are not used multiple times
 * by the attribute fork, some mechanism to keep track of all them is necessary.
 * Do this in the future, time permitting. For now, note that there is no
 * check for remote blocks and their allocations.
 *
 * For btree formatted attributes, the model can follow directories. That
 * would mean go down the tree to the leftmost leaf. From there moving down
 * the links and processing each. They would call back up the tree, to verify
 * that the tree structure is okay. Any problems will result in the attribute
 * fork being emptied and put in shortform format.
 */

/*
 * This routine just checks what security needs are for attribute values
 * only called when root flag is set, otherwise these names could exist in
 * in user attribute land without a conflict.
 * If value is non-zero, then a remote attribute is being passed in
 */

int
valuecheck(char *namevalue, char *value, int namelen, int valuelen)
{
	/* for proper alignment issues, get the structs and bcopy the values */
	mac_label macl;
	struct acl thisacl;
	void *valuep;
	int clearit = 0;

	if ((strncmp(namevalue, SGI_ACL_FILE, SGI_ACL_FILE_SIZE) == 0) || 
			(strncmp(namevalue, SGI_ACL_DEFAULT, 
				SGI_ACL_DEFAULT_SIZE) == 0)) {
		if (value == NULL) {	
			bzero(&thisacl, sizeof(struct acl));
			bcopy(namevalue+namelen, &thisacl, valuelen);
			valuep = &thisacl;
		} else
			valuep = value;

		if (acl_valid((struct acl *) valuep) != 0) { /* 0 means valid */
			clearit = 1;
			do_warn("entry contains illegal value in attribute named SGI_ACL_FILE or SGI_ACL_DEFAULT\n");
		}
	} else if (strncmp(namevalue, SGI_MAC_FILE, SGI_MAC_FILE_SIZE) == 0) {
		if (value == NULL) {
			bzero(&macl, sizeof(mac_label));
			bcopy(namevalue+namelen, &macl, valuelen);
			valuep = &macl;
		} else 
			valuep = value;

		if (mac_valid((mac_label *) valuep) != 1) { /* 1 means valid */
			 /*
			 *if sysconf says MAC enabled, 
			 *	temp = mac_from_text("msenhigh/mintlow", NULL)
			 *	copy it to value, update valuelen, totsize
			 *	This causes pushing up or down of all following
			 *	attributes, forcing a attribute format change!!
			 * else clearit = 1;
			 */
			clearit = 1;
			do_warn("entry contains illegal value in attribute named SGI_MAC_LABEL\n");
		}
	} else if (strncmp(namevalue, SGI_CAP_FILE, SGI_CAP_FILE_SIZE) == 0) {
		if ( valuelen != sizeof(cap_set_t)) {
			clearit = 1;
			do_warn("entry contains illegal value in attribute named SGI_CAP_FILE\n");
		}
	}

	return(clearit);
}


/*
 * this routine validates the attributes in shortform format.
 * a non-zero return repair value means certain attributes are bogus
 * and were cleared if possible. Warnings do not generate error conditions
 * if you cannot modify the structures. repair is set to 1, if anything
 * was fixed.
 */
int
process_shortform_attr(
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	int 		*repair)	
{
	xfs_attr_shortform_t	*asf;
	xfs_attr_sf_entry_t	*currententry, *nextentry, *tempentry;
	int			i, junkit;
	int			currentsize, remainingspace;
	
	*repair = 0;

	asf = (xfs_attr_shortform_t *) XFS_DFORK_APTR_ARCH(dip, ARCH_CONVERT);

	/* Assumption: hdr.totsize is less than a leaf block and was checked
	 * by lclinode for valid sizes. Check the count though.	
	*/
	if (INT_GET(asf->hdr.count, ARCH_CONVERT) == 0) 
		/* then the total size should just be the header length */
		if (INT_GET(asf->hdr.totsize, ARCH_CONVERT) != sizeof(xfs_attr_sf_hdr_t)) {
			/* whoops there's a discrepancy. Clear the hdr */
			if (!no_modify) {
				do_warn("there are no attributes in the fork for inode %llu \n", ino);
				INT_SET(asf->hdr.totsize, ARCH_CONVERT,
						sizeof(xfs_attr_sf_hdr_t));
				*repair = 1;
				return(1); 	
			} else {
				do_warn("would junk the attribute fork since the count is 0 for inode %llu\n",ino);
				return(1);
			}
                }
		
	currentsize = sizeof(xfs_attr_sf_hdr_t); 
	remainingspace = INT_GET(asf->hdr.totsize, ARCH_CONVERT) - currentsize;
	nextentry = &asf->list[0];
	for (i = 0; i < INT_GET(asf->hdr.count, ARCH_CONVERT); i++)  {
		currententry = nextentry;
		junkit = 0;

		/* don't go off the end if the hdr.count was off */
		if ((currentsize + (sizeof(xfs_attr_sf_entry_t) - 1)) > 
				INT_GET(asf->hdr.totsize, ARCH_CONVERT))
			break; /* get out and reset count and totSize */

		/* if the namelen is 0, can't get to the rest of the entries */
		if (INT_GET(currententry->namelen, ARCH_CONVERT) == 0) {
			do_warn("zero length name entry in attribute fork, ");
			if (!no_modify) {
				do_warn("truncating attributes for inode %llu to %d \n", ino, i);
				*repair = 1;
				break; 	/* and then update hdr fields */
			} else {
				do_warn("would truncate attributes for inode %llu to %d \n", ino, i);
				break;
			}
		} else {
			/* It's okay to have a 0 length valuelen, but do a
			 * rough check to make sure we haven't gone outside of
			 * totsize.
			 */
			if ((remainingspace < INT_GET(currententry->namelen, ARCH_CONVERT)) ||
				((remainingspace - INT_GET(currententry->namelen, ARCH_CONVERT))
					  < INT_GET(currententry->valuelen, ARCH_CONVERT))) {
				do_warn("name or value attribute lengths are too large, \n");
				if (!no_modify) {
					do_warn(" truncating attributes for inode %llu to %d \n", ino, i);
					*repair = 1; 
					break; /* and then update hdr fields */
				} else {
					do_warn(" would truncate attributes for inode %llu to %d \n", ino, i);	
					break;
				}	
			}
		}
	
		/* namecheck checks for / and null terminated for file names. 
		 * attributes names currently follow the same rules.
		*/
		if (namecheck((char *)&currententry->nameval[0], 
				INT_GET(currententry->namelen, ARCH_CONVERT)))  {
			do_warn("entry contains illegal character in shortform attribute name\n");
			junkit = 1;
		}

		if (INT_GET(currententry->flags, ARCH_CONVERT) & XFS_ATTR_INCOMPLETE) {
			do_warn("entry has INCOMPLETE flag on in shortform attribute\n");
			junkit = 1;
		}

		/* Only check values for root security attributes */
		if (INT_GET(currententry->flags, ARCH_CONVERT) & XFS_ATTR_ROOT) 
		       junkit = valuecheck((char *)&currententry->nameval[0], NULL, 
				INT_GET(currententry->namelen, ARCH_CONVERT), INT_GET(currententry->valuelen, ARCH_CONVERT));

		remainingspace = remainingspace - 
				XFS_ATTR_SF_ENTSIZE(currententry);

		if (junkit) {
			if (!no_modify) {
				/* get rid of only this entry */
				do_warn("removing attribute entry %d for inode %llu \n", i, ino);
				tempentry = (xfs_attr_sf_entry_t *)
					((__psint_t) currententry +
					 XFS_ATTR_SF_ENTSIZE(currententry));
				memmove(currententry,tempentry,remainingspace);
				INT_MOD(asf->hdr.count, ARCH_CONVERT, -1);
				i--; /* no worries, it will wrap back to 0 */
				*repair = 1;
				continue; /* go back up now */
			} else { 
				do_warn("would remove attribute entry %d for inode %llu \n", i, ino);
                        }
                }

		/* Let's get ready for the next entry... */
		nextentry = (xfs_attr_sf_entry_t *)
			 ((__psint_t) nextentry +
			 XFS_ATTR_SF_ENTSIZE(currententry));
		currentsize = currentsize + XFS_ATTR_SF_ENTSIZE(currententry);
	
		} /* end the loop */

	
	if (INT_GET(asf->hdr.count, ARCH_CONVERT) != i)  {
		if (no_modify)  {
			do_warn("would have corrected attribute entry count in inode %llu from %d to %d\n",
				ino, INT_GET(asf->hdr.count, ARCH_CONVERT), i);
		} else  {
			do_warn("corrected attribute entry count in inode %llu, was %d, now %d\n",
				ino, INT_GET(asf->hdr.count, ARCH_CONVERT), i);
			INT_SET(asf->hdr.count, ARCH_CONVERT, i);
			*repair = 1;
		}
	}
	
	/* ASSUMPTION: currentsize <= totsize */
	if (INT_GET(asf->hdr.totsize, ARCH_CONVERT) != currentsize)  {
		if (no_modify)  {
			do_warn("would have corrected attribute totsize in inode %llu from %d to %d\n",
				ino, INT_GET(asf->hdr.totsize, ARCH_CONVERT), currentsize);
		} else  {
			do_warn("corrected attribute entry totsize in inode %llu, was %d, now %d\n",
				ino, INT_GET(asf->hdr.totsize, ARCH_CONVERT), currentsize);
			INT_SET(asf->hdr.totsize, ARCH_CONVERT, currentsize);
			*repair = 1;
		}
	}

	return(*repair);
}

/* This routine brings in blocks from disk one by one and assembles them
 * in the value buffer. If get_bmapi gets smarter later to return an extent
 * or list of extents, that would be great. For now, we don't expect too
 * many blocks per remote value, so one by one is sufficient.
 */
static int
rmtval_get(xfs_mount_t *mp, xfs_ino_t ino, blkmap_t *blkmap,
		xfs_dablk_t blocknum, int valuelen, char* value)
{
	xfs_dfsbno_t	bno;
	xfs_buf_t	*bp;
	int		clearit = 0, i = 0, length = 0, amountdone = 0;
	
	/* ASSUMPTION: valuelen is a valid number, so use it for looping */
	/* Note that valuelen is not a multiple of blocksize */  
	while (amountdone < valuelen) {
		bno = blkmap_get(blkmap, blocknum + i);
		if (bno == NULLDFSBNO) {
			do_warn("remote block for attributes of inode %llu"
				" is missing\n", ino);
			clearit = 1;
			break;
		}
		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, bno),
				XFS_FSB_TO_BB(mp, 1), 0);
		if (!bp) {
			do_warn("can't read remote block for attributes"
				" of inode %llu\n", ino);
			clearit = 1;
			break;
		}
		ASSERT(mp->m_sb.sb_blocksize == XFS_BUF_COUNT(bp));
		length = MIN(XFS_BUF_COUNT(bp), valuelen - amountdone);
		bcopy(XFS_BUF_PTR(bp), value, length); 
		amountdone += length;
		value += length;
		i++;
		libxfs_putbuf(bp);
	}
	return (clearit);
}

/*
 * freespace map for directory and attribute leaf blocks (1 bit per byte)
 * 1 == used, 0 == free
 */
static da_freemap_t attr_freemap[DA_BMAP_SIZE];

/* The block is read in. The magic number and forward / backward
 * links are checked by the caller process_leaf_attr.
 * If any problems occur the routine returns with non-zero. In
 * this case the next step is to clear the attribute fork, by
 * changing it to shortform and zeroing it out. Forkoff need not
 * be changed. 
 */

int
process_leaf_attr_block(
	xfs_mount_t	*mp,
	xfs_attr_leafblock_t *leaf,
	xfs_dablk_t	da_bno,
	xfs_ino_t	ino,
	blkmap_t	*blkmap,
	xfs_dahash_t	last_hashval,
	xfs_dahash_t	*current_hashval,
	int 		*repair)	
{
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *local;
	xfs_attr_leaf_name_remote_t *remotep;
	int  i, start, stop, clearit, usedbs, firstb, thissize;

	clearit = usedbs = 0;
	*repair = 0;
	firstb = mp->m_sb.sb_blocksize; 
	stop = sizeof(xfs_attr_leaf_hdr_t);

	/* does the count look sorta valid? */
	if (INT_GET(leaf->hdr.count, ARCH_CONVERT)
				* sizeof(xfs_attr_leaf_entry_t)
				+ sizeof(xfs_attr_leaf_hdr_t)
							> XFS_LBSIZE(mp)) {
		do_warn("bad attribute count %d in attr block %u, inode %llu\n",
			(int) INT_GET(leaf->hdr.count, ARCH_CONVERT),
						da_bno, ino);
		return (1);
	}
 
	init_da_freemap(attr_freemap);
	(void) set_da_freemap(mp, attr_freemap, 0, stop);
	
	/* go thru each entry checking for problems */
	for (i = 0, entry = &leaf->entries[0]; 
			i < INT_GET(leaf->hdr.count, ARCH_CONVERT);
						i++, entry++) {
	
		/* check if index is within some boundary. */
		if (INT_GET(entry->nameidx, ARCH_CONVERT) > XFS_LBSIZE(mp)) {
			do_warn("bad attribute nameidx %d in attr block %u, inode %llu\n",
				(int)INT_GET(entry->nameidx, ARCH_CONVERT),
				da_bno,ino);
			clearit = 1;
			break;
			}

		if (INT_GET(entry->flags, ARCH_CONVERT) & XFS_ATTR_INCOMPLETE) {
			/* we are inconsistent state. get rid of us */
			do_warn("attribute entry #%d in attr block %u, inode %llu is INCOMPLETE\n",
				i, da_bno, ino);
			clearit = 1;
			break;
			}

		/* mark the entry used */
		start = (__psint_t)&leaf->entries[i] - (__psint_t)leaf;
		stop = start + sizeof(xfs_attr_leaf_entry_t);
		if (set_da_freemap(mp, attr_freemap, start, stop))  {
			do_warn("attribute entry %d in attr block %u, inode %llu claims already used space\n",
				i,da_bno,ino);
			clearit = 1;
			break;	/* got an overlap */
			}

		if (INT_GET(entry->flags, ARCH_CONVERT) & XFS_ATTR_LOCAL) {

			local = XFS_ATTR_LEAF_NAME_LOCAL(leaf, i);	
			if ((INT_GET(local->namelen, ARCH_CONVERT) == 0) || 
					(namecheck((char *)&local->nameval[0], 
						INT_GET(local->namelen, ARCH_CONVERT)))) {
				do_warn("attribute entry %d in attr block %u, inode %llu has bad name (namelen = %d)\n",
					i, da_bno, ino, (int) INT_GET(local->namelen, ARCH_CONVERT));

				clearit = 1;
				break;
				};

			/* Check on the hash value. Checking ordering of hash values
			 * is not necessary, since one wrong one clears the whole
			 * fork. If the ordering's wrong, it's caught here or 
 			 * the kernel code has a bug with transaction logging
			 * or attributes itself. For paranoia reasons, let's check
			 * ordering anyway in case both the name value and the 
		  	 * hashvalue were wrong but matched. Unlikely, however.
			*/
			if (INT_GET(entry->hashval, ARCH_CONVERT) != 
				libxfs_da_hashname((char *)&local->nameval[0],
					INT_GET(local->namelen, ARCH_CONVERT)) ||
				(INT_GET(entry->hashval, ARCH_CONVERT)
							< last_hashval)) {
				do_warn("bad hashvalue for attribute entry %d in attr block %u, inode %llu\n",
					i, da_bno, ino);
				clearit = 1;
				break;
			}

			/* Only check values for root security attributes */
			if (INT_GET(entry->flags, ARCH_CONVERT) & XFS_ATTR_ROOT) 
				if (valuecheck((char *)&local->nameval[0], NULL,
					    INT_GET(local->namelen, ARCH_CONVERT), INT_GET(local->valuelen, ARCH_CONVERT))) {
					do_warn("bad security value for attribute entry %d in attr block %u, inode %llu\n",
						i,da_bno,ino);
					clearit = 1;
					break;
				};
			thissize = XFS_ATTR_LEAF_ENTSIZE_LOCAL(
					INT_GET(local->namelen, ARCH_CONVERT), INT_GET(local->valuelen, ARCH_CONVERT));

		} else {
			/* do the remote case */
			remotep = XFS_ATTR_LEAF_NAME_REMOTE(leaf, i);
			thissize = XFS_ATTR_LEAF_ENTSIZE_REMOTE(
					INT_GET(remotep->namelen, ARCH_CONVERT)); 

			if ((INT_GET(remotep->namelen, ARCH_CONVERT) == 0) || 
				   (namecheck((char *)&remotep->name[0],
					INT_GET(remotep->namelen, ARCH_CONVERT))) ||
				   (INT_GET(entry->hashval, ARCH_CONVERT)
						!= libxfs_da_hashname(
					(char *)&remotep->name[0],
					 INT_GET(remotep->namelen, ARCH_CONVERT))) ||
				   (INT_GET(entry->hashval, ARCH_CONVERT)
						< last_hashval) ||
				   (INT_GET(remotep->valueblk, ARCH_CONVERT) == 0)) {
				do_warn("inconsistent remote attribute entry %d in attr block %u, ino %llu\n",
					i, da_bno, ino);
				clearit = 1;
				break;
			};

			if (INT_GET(entry->flags, ARCH_CONVERT) & XFS_ATTR_ROOT) {
				char*	value;
				if ((value = malloc(INT_GET(remotep->valuelen, ARCH_CONVERT)))==NULL){
					do_warn("cannot malloc enough for remotevalue attribute for inode %llu\n",ino);
					do_warn("SKIPPING this remote attribute\n");
					continue;
				}
				if (rmtval_get(mp, ino, blkmap,
						INT_GET(remotep->valueblk, ARCH_CONVERT),
						INT_GET(remotep->valuelen, ARCH_CONVERT), value)) {
					do_warn("remote attribute get failed for entry %d, inode %llu\n", i,ino);
					clearit = 1;
					free(value);
					break;
				}
				if (valuecheck((char *)&remotep->name[0], value,
					    INT_GET(remotep->namelen, ARCH_CONVERT), INT_GET(remotep->valuelen, ARCH_CONVERT))){
					do_warn("remote attribute value check  failed for entry %d, inode %llu\n", i, ino);
					clearit = 1;
					free(value);
					break;
				}
				free(value);
			}
		}

		*current_hashval = last_hashval 
				 = INT_GET(entry->hashval, ARCH_CONVERT);

		if (set_da_freemap(mp, attr_freemap, INT_GET(entry->nameidx, ARCH_CONVERT),
				INT_GET(entry->nameidx, ARCH_CONVERT) + thissize))  {
			do_warn("attribute entry %d in attr block %u, inode %llu claims used space\n",
				i, da_bno, ino);
			clearit = 1;
			break;	/* got an overlap */
		}			
		usedbs += thissize;
		if (INT_GET(entry->nameidx, ARCH_CONVERT) < firstb) 
			firstb = INT_GET(entry->nameidx, ARCH_CONVERT);

	} /* end the loop */

	if (!clearit) {
		/* verify the header information is correct */

		/* if the holes flag is set, don't reset first_used unless it's
		 * pointing to used bytes.  we're being conservative here
		 * since the block will get compacted anyhow by the kernel. 
		 */

		if (  (INT_GET(leaf->hdr.holes, ARCH_CONVERT) == 0
		    && firstb != INT_GET(leaf->hdr.firstused, ARCH_CONVERT))
		    || INT_GET(leaf->hdr.firstused, ARCH_CONVERT) > firstb)  {
			if (!no_modify)  {
				do_warn("- resetting first used heap value from %d to %d in block %u of attribute fork of inode %llu\n",
					(int)INT_GET(leaf->hdr.firstused,
						ARCH_CONVERT), firstb,
						da_bno, ino);
				INT_SET(leaf->hdr.firstused,
						ARCH_CONVERT, firstb);
				*repair = 1;
			} else  {
				do_warn("- would reset first used value from %d to %d in block %u of attribute fork of inode %llu\n",
					(int)INT_GET(leaf->hdr.firstused,
						ARCH_CONVERT), firstb,
						da_bno, ino);
			}
		}

		if (usedbs != INT_GET(leaf->hdr.usedbytes, ARCH_CONVERT))  {
			if (!no_modify)  {
				do_warn("- resetting usedbytes cnt from %d to %d in block %u of attribute fork of inode %llu\n",
					(int)INT_GET(leaf->hdr.usedbytes,
					  ARCH_CONVERT), usedbs, da_bno, ino);
				INT_SET(leaf->hdr.usedbytes,
						ARCH_CONVERT, usedbs);
				*repair = 1;
			} else  {
				do_warn("- would reset usedbytes cnt from %d to %d in block %u of attribute fork of %llu\n",
					(int)INT_GET(leaf->hdr.usedbytes,
					    ARCH_CONVERT), usedbs,da_bno,ino);
			}
		}

		/* there's a lot of work in process_leaf_dir_block to go thru
		* checking for holes and compacting if appropiate. I don't think
		* attributes need all that, so let's just leave the holes. If
		* we discover later that this is a good place to do compaction
		* we can add it then. 
		*/
	}
	return (clearit);  /* and repair */
}


/*
 * returns 0 if the attribute fork is ok, 1 if it has to be junked.
 */
int
process_leaf_attr_level(xfs_mount_t	*mp,
			da_bt_cursor_t	*da_cursor)
{
	int			repair;
	xfs_attr_leafblock_t	*leaf;
	xfs_buf_t		*bp;
	xfs_ino_t		ino;
	xfs_dfsbno_t		dev_bno;
	xfs_dablk_t		da_bno;
	xfs_dablk_t		prev_bno;
	xfs_dahash_t		current_hashval = 0;
	xfs_dahash_t		greatest_hashval;

	da_bno = da_cursor->level[0].bno;
	ino = da_cursor->ino;
	prev_bno = 0;

	do {
		repair = 0;
		dev_bno = blkmap_get(da_cursor->blkmap, da_bno);
		/*
		 * 0 is the root block and no block
		 * pointer can point to the root block of the btree
		 */
		ASSERT(da_bno != 0);

		if (dev_bno == NULLDFSBNO) {
			do_warn("can't map block %u for attribute fork "
				"for inode %llu\n", da_bno, ino);
			goto error_out; 
		}

		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, dev_bno),
					XFS_FSB_TO_BB(mp, 1), 0);
		if (!bp) {
			do_warn("can't read file block %u (fsbno %llu) for"
				" attribute fork of inode %llu\n",
				da_bno, dev_bno, ino);
			goto error_out;
		}

		leaf = (xfs_attr_leafblock_t *)XFS_BUF_PTR(bp);

		/* check magic number for leaf directory btree block */
		if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT)
						!= XFS_ATTR_LEAF_MAGIC) {
			do_warn("bad attribute leaf magic %#x for inode %llu\n",
				 leaf->hdr.info.magic, ino);
			libxfs_putbuf(bp);
			goto error_out;
		}

		/*
		 * for each block, process the block, verify it's path,
		 * then get next block.  update cursor values along the way
		 */
		if (process_leaf_attr_block(mp, leaf, da_bno, ino,
				da_cursor->blkmap, current_hashval,
				&greatest_hashval, &repair))  {
			libxfs_putbuf(bp);
			goto error_out;
		}

		/*
		 * index can be set to hdr.count so match the
		 * indexes of the interior blocks -- which at the
		 * end of the block will point to 1 after the final
		 * real entry in the block
		 */
		da_cursor->level[0].hashval = greatest_hashval;
		da_cursor->level[0].bp = bp;
		da_cursor->level[0].bno = da_bno;
		da_cursor->level[0].index
				= INT_GET(leaf->hdr.count, ARCH_CONVERT);
		da_cursor->level[0].dirty = repair; 

		if (INT_GET(leaf->hdr.info.back, ARCH_CONVERT) != prev_bno)  {
			do_warn("bad sibling back pointer for block %u in "
				"attribute fork for inode %llu\n", da_bno, ino);
			libxfs_putbuf(bp);
			goto error_out;
		}

		prev_bno = da_bno;
		da_bno = INT_GET(leaf->hdr.info.forw, ARCH_CONVERT);

		if (da_bno != 0 && verify_da_path(mp, da_cursor, 0))  {
			libxfs_putbuf(bp);
			goto error_out;
		}

		current_hashval = greatest_hashval;

		if (repair && !no_modify) {
			libxfs_writebuf(bp, 0);
		}
		else {
			libxfs_putbuf(bp);
		}
	} while (da_bno != 0);

	if (verify_final_da_path(mp, da_cursor, 0))  {
		/*
		 * verify the final path up (right-hand-side) if still ok
		 */
		do_warn("bad hash path in attribute fork for inode %llu\n",
			da_cursor->ino);
		goto error_out;
	}

	/* releases all buffers holding interior btree blocks */
	release_da_cursor(mp, da_cursor, 0);
	return(0);

error_out:
	/* release all buffers holding interior btree blocks */
	err_release_da_cursor(mp, da_cursor, 0);
	return(1);
}


/*
 * a node directory is a true btree  -- where the attribute fork
 * has gotten big enough that it is represented as a non-trivial (e.g.
 * has more than just a block) btree.
 *
 * Note that if we run into any problems, we will trash the attribute fork.
 * 
 * returns 0 if things are ok, 1 if bad
 * Note this code has been based off process_node_dir. 
 */
int
process_node_attr(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap)
{
	xfs_dablk_t			bno;
	int				error = 0;
	da_bt_cursor_t			da_cursor;

	/*
	 * try again -- traverse down left-side of tree until we hit
	 * the left-most leaf block setting up the btree cursor along
	 * the way.  Then walk the leaf blocks left-to-right, calling
	 * a parent-verification routine each time we traverse a block.
	 */
	bzero(&da_cursor, sizeof(da_bt_cursor_t));
	da_cursor.active = 0;
	da_cursor.type = 0;
	da_cursor.ino = ino;
	da_cursor.dip = dip;
	da_cursor.greatest_bno = 0;
	da_cursor.blkmap = blkmap;

	/*
	 * now process interior node. don't have any buffers held in this path.
	 */
	error = traverse_int_dablock(mp, &da_cursor, &bno, XFS_ATTR_FORK);
	if (error == 0) 
		return(1);  /* 0 means unsuccessful */

	/*
	 * now pass cursor and bno into leaf-block processing routine
	 * the leaf dir level routine checks the interior paths
	 * up to the root including the final right-most path.
	 */
	
	return (process_leaf_attr_level(mp, &da_cursor));
}

/*
 * Start processing for a leaf or fuller btree.
 * A leaf directory is one where the attribute fork is too big for
 * the inode  but is small enough to fit into one btree block
 * outside the inode. This code is modelled after process_leaf_dir_block.
 *
 * returns 0 if things are ok, 1 if bad (attributes needs to be junked)
 * repair is set, if anything was changed, but attributes can live thru it
 */

int
process_longform_attr(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap,
	int		*repair)	/* out - 1 if something was fixed */
{
	xfs_attr_leafblock_t	*leaf;
	xfs_dfsbno_t	bno;
	xfs_buf_t	*bp;
	xfs_dahash_t	next_hashval;
	int		repairlinks = 0;

	*repair = 0;

	bno = blkmap_get(blkmap, 0);

	if ( bno == NULLDFSBNO ) {
		if (INT_GET(dip->di_core.di_anextents, ARCH_CONVERT) == 0  &&
		    dip->di_core.di_aformat == XFS_DINODE_FMT_EXTENTS )
			/* it's okay the kernel can handle this state */
			return(0);
		else	{
			do_warn("block 0 of inode %llu attribute fork"
				" is missing\n", ino);
			return(1);
		}
	}
	/* FIX FOR bug 653709 -- EKN */
	if (mp->m_sb.sb_agcount < XFS_FSB_TO_AGNO(mp, bno)) {
		do_warn("agno of attribute fork of inode %llu out of "
			"regular partition\n", ino);
		return(1);
	}

	bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, bno),
				XFS_FSB_TO_BB(mp, 1), 0);
	if (!bp) {
		do_warn("can't read block 0 of inode %llu attribute fork\n",
			ino);
		return(1);
	}

	/* verify leaf block */
	leaf = (xfs_attr_leafblock_t *)XFS_BUF_PTR(bp);

	/* check sibling pointers in leaf block or root block 0 before
	* we have to release the btree block
	*/
	if (   INT_GET(leaf->hdr.info.forw, ARCH_CONVERT) != 0
	    || INT_GET(leaf->hdr.info.back, ARCH_CONVERT) != 0)  {
		if (!no_modify)  {
			do_warn("clearing forw/back pointers in block 0 "
				"for attributes in inode %llu\n", ino);
			repairlinks = 1;
			INT_SET(leaf->hdr.info.forw, ARCH_CONVERT, 0);
			INT_SET(leaf->hdr.info.back, ARCH_CONVERT, 0);
		} else  {
			do_warn("would clear forw/back pointers in block 0 "
				"for attributes in inode %llu\n", ino);
		}
	}

	/*
	 * use magic number to tell us what type of attribute this is.
	 * it's possible to have a node or leaf attribute in either an
	 * extent format or btree format attribute fork.
	 */
	switch (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT)) {
	case XFS_ATTR_LEAF_MAGIC:	/* leaf-form attribute */
		if (process_leaf_attr_block(mp, leaf, 0, ino, blkmap,
				0, &next_hashval, repair)) {
			/* the block is bad.  lose the attribute fork. */
			libxfs_putbuf(bp);
			return(1); 
		}
		*repair = *repair || repairlinks; 
		break;

	case XFS_DA_NODE_MAGIC:		/* btree-form attribute */
		/* must do this now, to release block 0 before the traversal */
		if (repairlinks) {
			*repair = 1;
			libxfs_writebuf(bp, 0);
		} else 
			libxfs_putbuf(bp);	
		return (process_node_attr(mp, ino, dip, blkmap)); /* + repair */
	default:
		do_warn("bad attribute leaf magic # %#x for dir ino %llu\n", 
			INT_GET(leaf->hdr.info.magic, ARCH_CONVERT), ino);
		libxfs_putbuf(bp);
		return(1);
	}

	if (*repair && !no_modify) 
		libxfs_writebuf(bp, 0);
	else
		libxfs_putbuf(bp);

	return(0);  /* repair may be set */
}


static void
xfs_acl_get_endian(struct acl *aclp)
{
    struct acl_entry *ace, *end;

    /* do the endian conversion */
    INT_SET(aclp->acl_cnt, ARCH_CONVERT, aclp->acl_cnt);

    /* loop thru ACEs of ACL */
    end = &aclp->acl_entry[0]+aclp->acl_cnt;
    for (ace=&aclp->acl_entry[0]; ace < end; ace++) {
        INT_SET(ace->ae_tag, ARCH_CONVERT, ace->ae_tag);
        INT_SET(ace->ae_id, ARCH_CONVERT, ace->ae_id);
        INT_SET(ace->ae_perm, ARCH_CONVERT, ace->ae_perm);
    }
}

/*
 * returns 1 if attributes got cleared
 * and 0 if things are ok. 
 */
int
process_attributes(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap,
	int		*repair)  /* returned if we did repair */
{
	int err;
	xfs_dinode_core_t *dinoc;
	/* REFERENCED */
	xfs_attr_shortform_t *asf;

	dinoc = &dip->di_core;
	asf = (xfs_attr_shortform_t *) XFS_DFORK_APTR_ARCH(dip, ARCH_CONVERT);

	if (dinoc->di_aformat == XFS_DINODE_FMT_LOCAL) {
		ASSERT(INT_GET(asf->hdr.totsize, ARCH_CONVERT) <= XFS_DFORK_ASIZE_ARCH(dip, mp, ARCH_CONVERT));
		err = process_shortform_attr(ino, dip, repair);
	} else if (dinoc->di_aformat == XFS_DINODE_FMT_EXTENTS ||
		   dinoc->di_aformat == XFS_DINODE_FMT_BTREE)  {
			err = process_longform_attr(mp, ino, dip, blkmap,
				repair);
			/* if err, convert this to shortform and clear it */
			/* if repair and no error, it's taken care of */
	} else  {
		do_warn("illegal attribute format %d, ino %llu\n",
			dinoc->di_aformat, ino);
		err = 1; 
	}
	return (err);  /* and repair */
}

/* 
 * Validate an ACL
 */
static int
acl_valid (struct acl *aclp)
{
	struct acl_entry *entry, *e;
	int user = 0, group = 0, other = 0, mask = 0, mask_required = 0;
	int i, j;

	if (aclp == NULL)
		goto acl_invalid;

	xfs_acl_get_endian(aclp);

	if (aclp->acl_cnt > ACL_MAX_ENTRIES)
		goto acl_invalid;

	for (i = 0; i < aclp->acl_cnt; i++)
	{

		entry = &aclp->acl_entry[i];

		switch (entry->ae_tag)
		{
			case ACL_USER_OBJ:
				if (user++)
					goto acl_invalid;
				break;
			case ACL_GROUP_OBJ:
				if (group++)
					goto acl_invalid;
				break;
			case ACL_OTHER_OBJ:
				if (other++)
					goto acl_invalid;
				break;
			case ACL_USER:
			case ACL_GROUP:
				for (j = i + 1; j < aclp->acl_cnt; j++)
				{
					e = &aclp->acl_entry[j];
					if (e->ae_id == entry->ae_id && e->ae_tag == entry->ae_tag)
						goto acl_invalid;
				}
				mask_required++;
				break;
			case ACL_MASK:
				if (mask++)
					goto acl_invalid;
				break;
			default:
				goto acl_invalid;
		}
	}
	if (!user || !group || !other || (mask_required && !mask))
		goto acl_invalid;
	else
		return 0;
acl_invalid:
	errno = EINVAL;
	return (-1);
}

/*
 * Check a category or division set to ensure that all values are in
 * ascending order and each division or category appears only once.
 */
static int
__check_setvalue(const unsigned short *list, unsigned short count)
{
        unsigned short i;

        for (i = 1; i < count ; i++)
                if (list[i] <= list[i-1])
                        return -1;
        return 0;
}


/*
 * mac_valid(lp)
 * check the validity of a mac label
 */
static int
mac_valid(mac_t lp)
{
	if (lp == NULL)
		return (0);

	/*
	 * if the total category set and division set is greater than 250
	 * report error
	 */
	if ((lp->ml_catcount + lp->ml_divcount) > MAC_MAX_SETS)
		return(0);

	/*
	 * check whether the msentype value is valid, and do they have
  	 * appropriate level, category association.
         */
	switch (lp->ml_msen_type) {
		case MSEN_ADMIN_LABEL:
		case MSEN_EQUAL_LABEL:
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			if (lp->ml_level != 0 || lp->ml_catcount > 0 )
				return (0);
			break;
		case MSEN_TCSEC_LABEL:
		case MSEN_MLD_LABEL:
			if (lp->ml_catcount > 0 &&
			    __check_setvalue(lp->ml_list,
					     lp->ml_catcount) == -1)
				return (0);
			break;
		case MSEN_UNKNOWN_LABEL:
		default:
			return (0);
	}

	/*
	 * check whether the minttype value is valid, and do they have
	 * appropriate grade, division association.
	 */
	switch (lp->ml_mint_type) {
		case MINT_BIBA_LABEL:
			if (lp->ml_divcount > 0 &&
			    __check_setvalue(lp->ml_list + lp->ml_catcount,
					     lp->ml_divcount) == -1)
				return(0);
			break;
		case MINT_EQUAL_LABEL:
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
			if (lp->ml_grade != 0 || lp->ml_divcount > 0 )
				return(0);
			break;
		default:
			return(0);
	}

	return (1);
}
