#!/usr/bin/perl -w
use strict;

# 
# Display raw XFS statistics from /proc/fs/xfs/stat.
# Quota statistics are handled via a separate xqmstats command.
# 

my $file = '/proc/fs/xfs/stat';
my $pbfile = '/proc/fs/pagebuf/stat';
my $loop = 0;

if (@ARGV > 0) {
    if ($ARGV[0] eq '-f') {
        $loop=1;
        print "\e[H\e[2J";
        shift(@ARGV);
    }
}

do {
    my @values;
    my @tmp;

    unless (open(STATS, $file)) {
        print STDERR "Running kernel is not exporting XFS statistics.\n";
        die "$file: $!";
    }
    while (<STATS>) {
        chomp;
        /^qm/ && next; # use xqmstats command from quota-tools
        /^(extent_alloc     #  0 -  3
            |abt            #  4 -  7
            |blk_map        #  8 - 14
            |bmbt           # 15 - 18
            |dir            # 19 - 22
            |trans          # 23 - 25
            |ig             # 26 - 32
            |log            # 33 - 37
            |push_ail       # 38 - 47
            |xstrat         # 48 - 49
            |rw             # 50 - 51
            |attr           # 52 - 55
            |icluster       # 56 - 58
            |vnodes         # 59 - 66
            |xpc            # 67 - 69
        )/x || next; #die "Unrecognised line in $file:\n\t'$_'\n";
        foreach (split(' ', $')) {
            push @values, sprintf("%11s", $_);
        }
    }
    @tmp = @values[67..69];    # reorder some items to get Ted's format.
    splice(@values, 67);
    splice(@values, 48, 0, ($tmp[0]));
    splice(@values, 52, 0, ($tmp[1]));
    splice(@values, 54, 0, ($tmp[2]));

    ($#values == 69) || die "Found $#values XFS values, expected 69";

    unless (open(STATS1, $pbfile)) {
        print STDERR "Running kernel is not exporting pagebuf stats.\n",
        die "$pbfile: $!";
    }
    while (<STATS1>) {
        chomp;
        /^(pagebuf)/ || next; #die "Unrecognised line in file";
        foreach (split(' ', $')) {
            push @values, sprintf("%11s", $_);
        }
    }

    ($#values == 78) || die "Found $#values XFS/pagebuf values, expected 78";

my $time = localtime(time);

print qq(XFS Statistics  [$time]
  Extent Allocation                      Tail-Pushing Stats
    xs_allocx............   $values[ 0]    xs_sleep_logspace.....   $values[39]
    xs_allocb............   $values[ 1]    xs_try_logspace.......   $values[38]
    xs_freex.............   $values[ 2]    xs_push_ail...........   $values[40]
    xs_freeb.............   $values[ 3]    xs_push_ail_success...   $values[41]
  Allocation Btree                         xs_push_ail_pushbuf...   $values[42]
    xs_abt_lookup........   $values[ 4]    xs_push_ail_pinned....   $values[43]
    xs_abt_compare.......   $values[ 5]    xs_push_ail_locked....   $values[44]
    xs_abt_insrec........   $values[ 6]    xs_push_ail_flushing..   $values[45]
    xs_abt_delrec........   $values[ 7]    xs_push_ail_restarts..   $values[46]
  Block Mapping                            xs_push_ail_flush.....   $values[47]
    xs_blk_mapr..........   $values[ 8]  IoMap Write Convert
    xs_blk_mapw..........   $values[ 9]    xs_xstrat_bytes.......   $values[48]
    xs_blk_unmap.........   $values[10]    xs_xstrat_quick.......   $values[49]
    xs_add_exlist........   $values[11]    xs_xstrat_split.......   $values[50]
    xs_del_exlist........   $values[12]  Read/Write Stats
    xs_look_exlist.......   $values[13]    xs_write_calls........   $values[51]
    xs_cmp_exlist........   $values[14]    xs_write_bytes........   $values[52]
  Block Map Btree                          xs_read_calls.........   $values[53]
    xs_bmbt_lookup.......   $values[15]    xs_read_bytes.........   $values[54]
    xs_bmbt_compare......   $values[16]  Attribute Operations
    xs_bmbt_insrec.......   $values[17]    xs_attr_get...........   $values[55]
    xs_bmbt_delrec.......   $values[18]    xs_attr_set...........   $values[56]
  Directory Operations                     xs_attr_remove........   $values[57]
    xs_dir_lookup........   $values[19]    xs_attr_list..........   $values[58]
    xs_dir_create........   $values[20]  Inode Clustering
    xs_dir_remove........   $values[21]    xs_iflush_count.......   $values[59]
    xs_dir_getdents......   $values[22]    xs_icluster_flushcnt..   $values[60]
  Transactions                             xs_icluster_flushinode   $values[61]
    xs_trans_sync........   $values[23]  Vnode Statistics
    xs_trans_async.......   $values[24]    vn_active.............   $values[62]
    xs_trans_empty.......   $values[25]    vn_alloc..............   $values[63]
  Inode Operations                         vn_get................   $values[64]
    xs_ig_attempts.......   $values[26]    vn_hold...............   $values[65]
    xs_ig_found..........   $values[27]    vn_rele...............   $values[66]
    xs_ig_frecycle.......   $values[28]    vn_reclaim............   $values[67]
    xs_ig_missed.........   $values[29]    vn_remove.............   $values[68]
    xs_ig_dup............   $values[30]  Pagebuf Statistics
    xs_ig_reclaims.......   $values[31]    pb_get................   $values[70]
    xs_ig_attrchg........   $values[32]    pb_create.............   $values[71]
  Log Operations                           pb_get_locked.........   $values[72]
    xs_log_writes........   $values[33]    pb_get_locked_waited..   $values[73]
    xs_log_blocks........   $values[34]    pb_busy_locked........   $values[74]
    xs_log_noiclogs......   $values[35]    pb_miss_locked........   $values[75]
    xs_log_force.........   $values[36]    pb_page_retries.......   $values[76]
    xs_log_force_sleep...   $values[37]    pb_page_found.........   $values[77]
                                           pb_get_read...........   $values[78]
);
    if ($loop) {
        print "\e[H";
        sleep(1);
    }
} while ($loop);

__END__

=head1 NAME

xfs_stats - display XFS performance statistics

=head1 SYNOPSIS

B<xfs_stats> [ B<-h> I<host> ] [ B<-a> I<archive> ]

=head1 DESCRIPTION

I<xfs_stats> uses the /proc interface to extract XFS performance
data from the running kernel.
Alternatively, the performance data can be sourced from a Performance
Co-Pilot (PCP) host running the I<pmcd>(1) daemon or from a PCP
archive created by the I<pmlogger>(1) utility.

=head1 OPTIONS

=head2 B<-h> I<host>

The named I<host> should be the source of live performance data,
rather than /proc.

=head2 B<-a> I<archive>

The historical data contained in the named I<archive> should be
displayed, rather than /proc.

=head1 STATISTICS

=head2 B<xs_allocx> (I<xfs.allocs.alloc_extent>)

Number of file system extents allocated over all XFS filesystems.

=head2 B<xs_allocb> (I<xfs.allocs.alloc_block>)

Number of file system blocks allocated over all XFS filesystems.

=head2 B<xs_freex> (I<xfs.allocs.free_extent>)

Number of file system extents freed over all XFS filesystems.

=head2 B<xs_freeb> (I<xfs.allocs.free_block>)

Number of file system blocks freed over all XFS filesystems.

=head2 B<xs_abt_lookup> (I<xfs.alloc_btree.lookup>)

Number of lookup operations in XFS filesystem allocation btrees.

=head2 B<xs_abt_compare> (I<xfs.alloc_btree.compare>)

Number of compares in XFS filesystem allocation btree lookups.

=head2 B<xs_abt_insrec> (I<xfs.alloc_btree.insrec>)

Number of extent records inserted into XFS filesystem allocation btrees.

=head2 B<xs_abt_delrec> (I<xfs.alloc_btree.delrec>)

Number of extent records deleted from XFS filesystem allocation btrees.

=head2 B<xs_blk_mapr> (I<xfs.block_map.read_ops>)

Number of block map for read operations performed on XFS files.

=head2 B<xs_blk_mapw> (I<xfs.block_map.write_ops>)

Number of block map for write operations performed on XFS files.

=head2 B<xs_blk_unmap> (I<xfs.block_map.unmap>)

Number of block unmap (delete) operations performed on XFS files.

=head2 B<xs_add_exlist> (I<xfs.block_map.add_exlist>)

Number of extent list insertion operations for XFS files.

=head2 B<xs_del_exlist> (I<xfs.block_map.del_exlist>)

Number of extent list deletion operations for XFS files.

=head2 B<xs_look_exlist> (I<xfs.block_map.look_exlist>)

Number of extent list lookup operations for XFS files.

=head2 B<xs_cmp_exlist> (I<xfs.block_map.cmp_exlist>)

Number of extent list comparisons in XFS extent list lookups.

=head2 B<xs_bmbt_lookup> (I<xfs.bmap_btree.lookup>)

Number of block map btree lookup operations on XFS files.

=head2 B<xs_bmbt_compare> (I<xfs.bmap_btree.compare>)

Number of block map btree compare operations in XFS block map lookups.

=head2 B<xs_bmbt_insrec> (I<xfs.bmap_btree.insrec>)

Number of block map btree records inserted for XFS files.

=head2 B<xs_bmbt_delrec> (I<xfs.bmap_btree.delrec>)

Number of block map btree records deleted for XFS files.

=head2 B<xs_dir_lookup> (I<xfs.dir_ops.lookup>)

This is a count of the number of file name directory lookups in XFS
filesystems. It counts only those lookups which miss in the operating
system's directory name lookup cache and must search the real directory
structure for the name in question.  The count is incremented once for
each level of a pathname search that results in a directory lookup.

=head2 B<xs_dir_create> (I<xfs.dir_ops.create>)

This is the number of times a new directory entry was created in XFS
filesystems. Each time that a new file, directory, link, symbolic link,
or special file is created in the directory hierarchy the count is
incremented.

=head2 B<xs_dir_remove> (I<xfs.dir_ops.remove>)

This is the number of times an existing directory entry was removed in
XFS filesystems. Each time that a file, directory, link, symbolic link,
or special file is removed from the directory hierarchy the count is
incremented.

=head2 B<xs_dir_getdents> (I<xfs.dir_ops.getdents>)

This is the number of times the XFS directory getdents operation was
performed. The getdents operation is used by programs to read the
contents of directories in a file system independent fashion.  This
count corresponds exactly to the number of times the getdents(2) system
call was successfully used on an XFS directory.

=head2 B<xs_trans_sync> (I<xfs.transactions.sync>)

This is the number of meta-data transactions which waited to be
committed to the on-disk log before allowing the process performing the
transaction to continue. These transactions are slower and more
expensive than asynchronous transactions, because they force the in
memory log buffers to be forced to disk more often and they wait for
the completion of the log buffer writes. Synchronous transactions
include file truncations and all directory updates when the file system
is mounted with the 'wsync' option.

=head2 B<xs_trans_async> (I<xfs.transactions.async>)

This is the number of meta-data transactions which did not wait to be
committed to the on-disk log before allowing the process performing the
transaction to continue. These transactions are faster and more
efficient than synchronous transactions, because they commit their data
to the in memory log buffers without forcing those buffers to be
written to disk. This allows multiple asynchronous transactions to be
committed to disk in a single log buffer write. Most transactions used
in XFS file systems are asynchronous.

=head2 B<xs_trans_empty> (I<xfs.transactions.empty>)

This is the number of meta-data transactions which did not actually
change anything. These are transactions which were started for some
purpose, but in the end it turned out that no change was necessary.

=head2 B<xs_ig_attempts> (I<xfs.inode_ops.ig_attempts>)

This is the number of times the operating system looked for an XFS
inode in the inode cache. Whether the inode was found in the cache or
needed to be read in from the disk is not indicated here, but this can
be computed from the ig_found and ig_missed counts.

=head2 B<xs_ig_found> (I<xfs.inode_ops.ig_found>)

This is the number of times the operating system looked for an XFS
inode in the inode cache and found it. The closer this count is to the
ig_attempts count the better the inode cache is performing.

=head2 B<xs_ig_frecycle> (I<xfs.inode_ops.ig_frecycle>)

This is the number of times the operating system looked for an XFS
inode in the inode cache and saw that it was there but was unable to
use the in memory inode because it was being recycled by another
process.

=head2 B<xs_ig_missed> (I<xfs.inode_ops.ig_missed>)

This is the number of times the operating system looked for an XFS
inode in the inode cache and the inode was not there. The further this
count is from the ig_attempts count the better.

=head2 B<xs_ig_dup> (I<xfs.inode_ops.ig_dup>)

This is the number of times the operating system looked for an XFS
inode in the inode cache and found that it was not there but upon
attempting to add the inode to the cache found that another process had
already inserted it.

=head2 B<xs_ig_reclaims> (I<xfs.inode_ops.ig_reclaims>)

This is the number of times the operating system recycled an XFS inode
from the inode cache in order to use the memory for that inode for
another purpose. Inodes are recycled in order to keep the inode cache
from growing without bound. If the reclaim rate is high it may be
beneficial to raise the vnode_free_ratio kernel tunable variable to
increase the size of the inode cache.

=head2 B<xs_ig_attrchg> (I<xfs.inode_ops.ig_attrchg>)

This is the number of times the operating system explicitly changed the
attributes of an XFS inode. For example, this could be to change the
inode's owner, the inode's size, or the inode's timestamps.

=head2 B<xs_log_writes> (I<xfs.log.writes>)

This variable counts the number of log buffer writes going to the
physical log partitions of all XFS filesystems. Log data traffic is
proportional to the level of meta-data updating. Log buffer writes get
generated when they fill up or external syncs occur.

=head2 B<xs_log_blocks> (I<xfs.log.blocks>)

This variable counts the number of Kbytes of information being written
to the physical log partitions of all XFS filesystems. Log data traffic
is proportional to the level of meta-data updating. The rate with which
log data gets written depends on the size of internal log buffers and
disk write speed. Therefore, filesystems with very high meta-data
updating may need to stripe the log partition or put the log partition
on a separate drive.

=head2 B<xs_log_noiclogs> (I<xfs.log.noiclogs>)

This variable keeps track of times when a logged transaction can not
get any log buffer space. When this occurs, all of the internal log
buffers are busy flushing their data to the physical on-disk log.

=head2 B<xs_log_force> (I<xfs.log.force>)

The number of times the in-core log is forced to disk.  It is
equivalent to the number of successful calls to the function
xfs_log_force().

=head2 B<xs_log_force_sleep> (I<xfs.log.force_sleep>)

Value exported from the xs_log_force_sleep field of struct xfsstats.

=head2 B<xs_xstrat_quick> (I<xfs.xstrat.quick>)

This is the number of buffers flushed out by the XFS flushing daemons
which are written to contiguous space on disk. The buffers handled by
the XFS daemons are delayed allocation buffers, so this count gives an
indication of the success of the XFS daemons in allocating contiguous
disk space for the data being flushed to disk.

=head2 B<xs_xstrat_split> (I<xfs.xstrat.split>)

This is the number of buffers flushed out by the XFS flushing daemons
which are written to non-contiguous space on disk. The buffers handled
by the XFS daemons are delayed allocation buffers, so this count gives
an indication of the failure of the XFS daemons in allocating
contiguous disk space for the data being flushed to disk. Large values
in this counter indicate that the file system has become fragmented.

=head2 B<xs_write_calls> (I<xfs.write>)

This is the number of write(2) system calls made to files in
XFS file systems.

=head2 B<xs_read_calls> (I<xfs.read>)

This is the number of read(2) system calls made to files in XFS file
systems.

=head2 B<xs_attr_get> (I<xfs.attr.get>)

The number of "get" operations performed on extended file attributes
within XFS filesystems.  The "get" operation retrieves the value of an
extended attribute.

=head2 B<xs_attr_set> (I<xfs.attr.set>)

The number of "set" operations performed on extended file attributes
within XFS filesystems.  The "set" operation creates and sets the value
of an extended attribute.

=head2 B<xs_attr_remove> (I<xfs.attr.remove>)

The number of "remove" operations performed on extended file attributes
within XFS filesystems.  The "remove" operation deletes an extended
attribute.

=head2 B<xs_attr_list> (I<xfs.attr.list>)

The number of "list" operations performed on extended file attributes
within XFS filesystems.  The "list" operation retrieves the set of
extended attributes associated with a file.

=head2 B<xs_try_logspace> (I<xfs.log_tail.try_logspace>)

Value from the xs_try_logspace field of struct xfsstats.

=head2 B<xs_sleep_logspace> (I<xfs.log_tail.sleep_logspace>)

Value from the xs_sleep_logspace field of struct xfsstats.

=head2 B<xs_push_ail> (I<xfs.log_tail.push_ail.pushes>)

The number of times the tail of the AIL is moved forward.  It is
equivalent to the number of successful calls to the function
xfs_trans_push_ail().

=head2 B<xs_push_ail_success> (I<xfs.log_tail.push_ail.success>)

Value from xs_push_ail_success field of struct xfsstats.

=head2 B<xs_push_ail_pushbuf> (I<xfs.log_tail.push_ail.pushbuf>)

Value from xs_push_ail_pushbuf field of struct xfsstats.

=head2 B<xs_push_ail_pinned> (I<xfs.log_tail.push_ail.pinned>)

Value from xs_push_ail_pinned field of struct xfsstats.

=head2 B<xs_push_ail_locked> (I<xfs.log_tail.push_ail.locked>)

Value from xs_push_ail_locked field of struct xfsstats.

=head2 B<xs_push_ail_flushing> (I<xfs.log_tail.push_ail.flushing>)

Value from xs_push_ail_flushing field of struct xfsstats.

=head2 B<xs_push_ail_restarts> (I<xfs.log_tail.push_ail.restarts>)

Value from xs_push_ail_restarts field of struct xfsstats.

=head2 B<xs_push_ail_flush> (I<xfs.log_tail.push_ail.flush>)

Value from xs_push_ail_flush field of struct xfsstats.

=head2 B<xs_iflush_count> (I<xfs.iflush_count>)

This is the number of calls to xfs_iflush which gets called when an
inode is being flushed (such as by bdflush or tail pushing).
xfs_iflush searches for other inodes in the same cluster which are
dirty and flushable.

=head2 B<xs_icluster_flushcnt> (I<xfs.icluster_flushcnt>)

Value from xs_icluster_flushcnt field of struct xfsstats.

=head2 B<xs_icluster_flushinode> (I<xfs.icluster_flushinode>)

This is the number of times that the inode clustering was not able to
flush anything but the one inode it was called with.

=head2 B<vn_active> (I<xfs.vnodes.active>)

Number of vnodes not on free lists.

=head2 B<vn_alloc> (I<xfs.vnodes.alloc>)

Number of times vn_alloc called.

=head2 B<vn_get> (I<xfs.vnodes.get>)

Number of times vn_get called.

=head2 B<vn_hold> (I<xfs.vnodes.hold>)

Number of times vn_hold called.

=head2 B<vn_rele> (I<xfs.vnodes.rele>)

Number of times vn_rele called.

=head2 B<vn_reclaim> (I<xfs.vnodes.reclaim>)

Number of times vn_reclaim called.

=head2 B<vn_remove> (I<xfs.vnodes.remove>)

Number of times vn_remove called.

=head2 B<xs_write_bytes> (I<xfs.write_bytes>)

This is a count of bytes written via B<write>(2) system calls to files
in XFS file systems. It can be used in conjunction with the write_calls
count to calculate the average size of the write operations to files in
XFS file systems.

=head2 B<xs_read_bytes> (I<xfs.read_bytes>)

This is a count of bytes read via B<read>(2) system calls to files in
XFS file systems. It can be used in conjunction with the read_calls
count to calculate the average size of the read operations to files in
XFS file systems.

=head2 B<xs_xstrat_bytes> (I<xfs.xstrat.bytes>)

This is a count of bytes of file data flushed out by the XFS
flushing daemons.

=head1 NOTES

Many of these statistics are monotonically increasing
counters, and of course are subject to counter overflow
(the final three listed above are 64-bit values, all others
are 32-bit values).
As such they are of limited value in this raw form - if you
are interested in monitoring throughput (e.g. bytes read/written
per second), or other rates of change, you will be better served
by investigating the PCP package more thoroughly - it contains a
number of performance analysis tools which can help in this regard.

=head1 FILES

F</proc/fs/xfs/stat> - XFS statistical data

=head1 SEE ALSO

I<pmcd>(1), I<pmlogger>(1), I<xfs>(5), I<xfs_info>(8).

=cut
