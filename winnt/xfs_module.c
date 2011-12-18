
/*
 * CROSSMETA Windows porting changes. http://www.crossmeta.org
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#include <ntifs.h>
#include <sys/param.h>
#include <sys/kern_svcs.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/errno.h>

/*
 * Windows NT file system driver interface routines
 */

#define	XFS_DEVICE_NAME		L"\\Device\\sgixfs"
#define	XFS_DOSDEV_NAME		L"\\DosDevices\\sgixfs"
#define	XFS_DOSDEV		"\\\\.\\sgixfs"

NTSTATUS 	DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING);
NTSTATUS 	xfs_mod_iodispatch(PDEVICE_OBJECT devp, PIRP);
void		xfs_mod_unload(PDRIVER_OBJECT);
NTSTATUS 	xfs_mod_ioctl(PDEVICE_OBJECT, PIRP, PIO_STACK_LOCATION);

extern int  	xfs_fs_init(struct vfssw *, int fstype);
extern void 	uuid_init(void);
extern void 	xfs_cleanup(void);
extern struct vfsops xfs_vfsops;

PDRIVER_OBJECT	xfs_drvobptr;
PDEVICE_OBJECT	xfs_fsdevptr;	/* our device object pointer */
static int xfsmod_refcnt = 0;

NTSTATUS
DriverEntry(PDRIVER_OBJECT drvobp, PUNICODE_STRING regpath)
{
	UNICODE_STRING devname, dosdev;
	NTSTATUS stat;
	int i;
	

	/* Create a device object for this driver */

	RtlInitUnicodeString(&devname, XFS_DEVICE_NAME);
	stat = IoCreateDevice(drvobp, 0, &devname, FILE_DEVICE_FILE_SYSTEM,
				0, FALSE, &xfs_fsdevptr);
	if (!NT_SUCCESS(stat)) {
		printf("XFS: unable to create device object: %x\n", stat);
		return (stat);
	}

	xfs_fsdevptr->Flags |= DO_DIRECT_IO;
	xfs_drvobptr = drvobp;

	RtlInitUnicodeString(&dosdev, XFS_DOSDEV_NAME);
	stat = IoCreateSymbolicLink(&dosdev, &devname);
	if (!NT_SUCCESS(stat)) {
		printf("nfsrv: Unable to create dosdevice : %x\n", stat);
		IoDeleteDevice(xfs_fsdevptr);
		return (stat);
	}

	/* Setup the driver entrypoints */
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
		if (i == IRP_MJ_FLUSH_BUFFERS)
			continue;
		drvobp->MajorFunction[i] = xfs_mod_iodispatch;
	}


	/*
	 * Register XFS file system with VFS layer
	 */
	vfs_registerfs("xfs", xfs_fs_init, &xfs_vfsops);

	uuid_init();

#if 0
	nfssysctl_ctxp = sysctl_register_external_set(nfssysctl_set,
							nfs_numsysctl);
#endif

	drvobp->DriverUnload = xfs_mod_unload;
	printf("XFS: with log version 2 support loaded\n");
	return (STATUS_SUCCESS);
}


/*
 * Driver dispatch routine for any I/O requests
 */
NTSTATUS
xfs_mod_iodispatch(PDEVICE_OBJECT devp, PIRP irp)
{
	PIO_STACK_LOCATION irpsp;
	NTSTATUS stat;

	PAGED_CODE();

	irpsp = IoGetCurrentIrpStackLocation(irp);

#if 0
	printf("nfsdrvio: MajorFunction code = 0x%x\n", irpsp->MajorFunction);
#endif

	switch (irpsp->MajorFunction) {

	case IRP_MJ_CREATE:
		stat = STATUS_SUCCESS;
		break;

	case IRP_MJ_CLEANUP:
		stat = STATUS_SUCCESS;
		break;

	case IRP_MJ_CLOSE:
		stat = STATUS_SUCCESS;
		break;

	case IRP_MJ_DEVICE_CONTROL:
		stat = xfs_mod_ioctl(devp, irp, irpsp);
		break;
	default:
		stat = STATUS_NOT_IMPLEMENTED;
		break;
	}

	irp->IoStatus.Status = stat;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return (stat);
}

NTSTATUS
xfs_mod_ioctl(PDEVICE_OBJECT devp, PIRP irp, PIO_STACK_LOCATION irpsp)
{
	int error;
	void *sysarg;
	uint_t inlen;
	ULONG code;
	NTSTATUS stat;
	PIRP toplevel;

	code = irpsp->Parameters.DeviceIoControl.IoControlCode;
	inlen = irpsp->Parameters.DeviceIoControl.InputBufferLength;
	if ((code & METHOD_NEITHER) == METHOD_NEITHER)
		sysarg = irpsp->Parameters.DeviceIoControl.Type3InputBuffer;
	else
		sysarg = irp->AssociatedIrp.SystemBuffer;

	toplevel = filesys_enter();

	switch (code) {


	default:
		printf("xfsmod: invalid ioctl code = %lx\n", code);
		stat = STATUS_INVALID_PARAMETER;
		break;
	}

	filesys_leave(toplevel);

	irp->IoStatus.Status = stat;
	return (stat);
}

void
xfs_mod_unload(PDRIVER_OBJECT drvobp)
{
	UNICODE_STRING dosdev;
	extern int xfs_fstype;
	extern void xfs_free_uuidtab();

	if (xfs_fstype != 0) {
		xfs_cleanup();
		xfs_free_uuidtab();
	}

        vfs_unregisterfs("xfs");

#if 0
	sysctl_ctx_free(nfssysctl_ctxp);
	kmem_free(nfssysctl_ctxp);
#endif

	/* remove symbolic-link to our device */
	RtlInitUnicodeString(&dosdev, XFS_DOSDEV_NAME);
	(void)IoDeleteSymbolicLink(&dosdev);

	/* delete our device itself */
	IoDeleteDevice(xfs_fsdevptr);
}

void
xfs_mod_incr_usage()
{
	if (!xfsmod_refcnt++)
		xfs_drvobptr->DriverUnload = NULL;
}

void
xfs_mod_decr_usage()
{
	if (--xfsmod_refcnt <= 0)
		xfs_drvobptr->DriverUnload = xfs_mod_unload;
}
