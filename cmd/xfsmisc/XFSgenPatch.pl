#!/usr/bin/perl

use Data::Dumper;
use POSIX qw(strftime);

$date = strftime "%m%d%Y", localtime;

$corePatch = "linux-2.4.2-core-xfs-".$date.".patch";
$xfsPatch = "linux-2.4.2-xfs-".$date.".patch";

$baseLinux = "/export/extra/lxfs-cvs/linux-2.4.2";

@xfs_dir = (
			"fs/xfs_support",
			"fs/xfs",
			"fs/pagebuf",
			"fs/ext_attr.c",
			"fs/noposix_acl.c",
			"fs/posix_acl.c",
			"Documentation/filesystems/xfs.txt",
			"include/linux/xfs_fs_i.h",
			"include/linux/page_buf.h",
			"include/linux/vnode.h",
			"include/linux/xfs_fs_sb.h",
			"include/linux/xfs_fs.h",
			"include/linux/avl.h",
			"include/linux/page_buf_trace.h",
			"include/linux/dmapi_kern.h",
			"include/linux/dmapi.h",
			"include/linux/behavior.h",
			"include/linux/grio.h",
			"include/linux/acl.h",
			"include/linux/attr_kern.h",
			"include/linux/attributes.h",
			"include/linux/xqm.h"
		   );

@file = (	
		 "Makefile",
		 "Rules.make",
		 "scripts/mkdep.c", # note remove for 2.4.3
		 "fs/Makefile", #Add xfs and pagebuf directories
		 "fs/buffer.c", # Delayed buffers
		 "fs/inode.c", #ihold4
		 "fs/filesystems.c", #init_xfs call
		 "fs/dquot.c", #XFS quota changes
		 "fs/Config.in", #xfs config options
		 "init/main.c", #pagebuf init call
		 "kernel/ksyms.c", #extra symbol exports
		 "mm/filemap.c", #Some tweaks from Rik Van Reil, pagebuf hooks
		 "mm/vmscan.c", #GFP_PAGE_IO support from Marcelo
		 "mm/slab.c", #kmem_cache_zalloc
		 "mm/vmalloc.c",
		 "include/linux/iobuf.h", # alloc kiobuf changes
		 "fs/iobuf.c",
		# kiobuf mount flag, blksetsize ioctl, extra buffer flags, xfs includes and
		# additions to inode and super block. ops vector changes for pagebuf and xfs.
		 "include/linux/fs.h",
		 "include/linux/sysctl.h", # pagebuf sysctl changes
		 "include/linux/pagemap.h", #Added new function prototypes
		 "include/linux/mm.h", #DelallocPage macro, GFP_PAGE_IO define
		 "include/linux/slab.h", #PAGE_IO defines, kmem_cache_zalloc define
		 "include/linux/vmalloc.h",
		 "arch/sparc64/kernel/ioctl32.c",
		 "arch/mips64/kernel/ioctl32.c",
		 "drivers/scsi/sd.c",
		 "drivers/block/blkpg.c",
		 "drivers/block/ll_rw_blk.c", # small delalloc check
		 "drivers/md/md.c",
		 "drivers/ide/ide.c",  # blkset size addtions
# LVM files
		 "drivers/md/lvm-internal.h",
		 "drivers/md/lvm-fs.c",
		 "include/linux/lvm.h",
		 "drivers/md/lvm.c",
		 "drivers/md/raid1.c",
		 "drivers/md/raid5.c", # raid fixes
		 "drivers/md/Makefile",
		 "drivers/md/lvm-snap.c",
# end LVM files
#		 "fs/partitions/check.c", # Sar additions
#		 "include/linux/genhd.h", # Sar addtions
		 "Documentation/filesystems/00-INDEX", #XFS documentation
		 "Documentation/Changes",
		 "MAINTAINERS", # entry for XFS
		 "Documentation/Configure.help",#Config option help
		 "arch/i386/boot/Makefile" # Change -oformat to --oformat for newer binutils
		); 

#DMAPI specific changes (14 files)
@dmapi = (
		  "include/asm-i386/fcntl.h",
		  "include/asm-mips/fcntl.h",
		  "include/asm-alpha/fcntl.h",
		  "include/asm-m68k/fcntl.h",
		  "include/asm-sparc/fcntl.h",
		  "include/asm-ppc/fcntl.h",
		  "include/asm-sparc64/fcntl.h",
		  "include/asm-arm/fcntl.h",
		  "include/asm-sh/fcntl.h",
		  "include/asm-ia64/fcntl.h",
		  "include/asm-mips64/fcntl.h",
		  "include/asm-s390/fcntl.h", #O_INVISIBLE definition
		  "include/linux/miscdevice.h", #DMAPI device definition - should be submitted to device number maintainer
		  "fs/super.c"); #dmapi code in mount path

#KIOBUF I/O specific changes (17 files)
@kio = (
		"fs/iobuf.c",
#		"fs/buffer.c", #kiobuf bounce page support
#		"drivers/scsi/sd.c",
		"drivers/block/rd.c",
		"drivers/scsi/scsi_merge.c",
		"drivers/scsi/scsi_lib.c",
		"drivers/ide/ide-disk.c",
		"drivers/ide/ide-dma.c",
#		"drivers/ide/ide.c",
#		"drivers/md/raid1.c",
#		"drivers/md/raid5.c", #Kiobuf I/O support
		"drivers/char/raw.c", #Kiobuf I/O support for Raw I/O
		"include/linux/blkdev.h", #kiobuf I/O function definition changes and require structure changes
		"include/linux/major.h", # kiobuf I/O added macros
		"include/linux/ide.h", #Kiobuf I/O ide structure changes
#		"drivers/block/ll_rw_blk.c",
		"drivers/block/elevator.c", #kiobuf I/O changes
		"include/linux/iobuf.h"); #Kiobuf I/O changes for bounce buffers

@attr = (
#	 "fs/Makefile", #add posix_acl code and extended attribute code
#	 "fs/Config.in", #posix acl config options
		 "include/asm-i386/unistd.h",
		 "include/asm-ia64/unistd.h",
		 "arch/i386/kernel/entry.S",
		 "arch/ia64/ia32/ia32_entry.S",
		 "arch/ia64/kernel/entry.S",
		 "arch/ia64/kernel/ivt.S"); #extended attribute and acl syscall numbers

@genfix = ( "fs/namei.c", #nested symlink fix - is this real? If it is lets get it submitted
            "mm/memory.c",#kiovec locking fixes - needed for generic pagebuf code
			"fs/iobuf.c", # change calls to suppport kiovec locking 
			"include/linux/iobuf.h");


push (@full, \@genfix);
push (@full, \@attr);
#push (@full, \@kio);
push (@full, \@dmapi);
push (@full, \@file);
#push (@full, \@xfs_dir);


foreach $dir (@full){
	push(@fullList,@{$dir});
}

foreach $dir (sort(@fullList)){
  print "$dir\n";
}	

coreFiles();
newFile();

exit;

sub coreFiles {
  open(CORE,"> $corePatch") || die "$corePatch $!\n";
  foreach $dir (sort(@fullList)){
	if ( -f "$baseLinux/linux/$dir" ){
	  $cmd = "diff -rNu $baseLinux/linux/$dir linux/$dir";
	} else {
	  $cmd = "diff -rNu /dev/null linux/$dir";
	}
	print STDERR "$cmd\n";
	open (DIFF, "$cmd |") || die "$cmd $!\n";
	while (<DIFF>){
	  print CORE $_;
	}
	close(DIFF);
  }
  close(CORE);
}

sub newFile {
  open(XFS,"> $xfsPatch") || die "$xfsPatch $!\n";
  foreach $dir (@xfs_dir) {
	#  $cmd = "diff -rNu $baseLinux/linux/$dir linux/$dir";
	if (! -d "/tmp/null") {
	  mkdir ("/tmp/null",0755) || die "/tmp/null $!\n";
	}
	if (-d "linux/$dir" ) {
	  $cmd = "diff -rNu /tmp/null linux/$dir";
	} elsif (-f "linux/$dir" ) {
	  $cmd = "diff -rNu /dev/null linux/$dir";
	}
	print STDERR "$cmd\n";
	open (DIFF, "$cmd |") || die "$cmd $!\n";
	while (<DIFF>){
	  print XFS $_;
	}
	close(DIFF);
  }
}
