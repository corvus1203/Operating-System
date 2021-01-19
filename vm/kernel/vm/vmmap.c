/******************************************************************************/
/* Important Spring 2020 CSCI 402 usage information:                          */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        vmmap_t *map = (vmmap_t *)slab_obj_alloc(vmmap_allocator);
        if (NULL != map) {
                list_init(&map->vmm_list);
                map->vmm_proc = NULL;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }
	dbg(DBG_PRINT, "(GRADING3A)\n");
        return map;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
        KASSERT(NULL != map); /* function argument must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.a)\n");

        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                list_remove(&vma->vma_plink);
                if (list_link_is_linked(&vma->vma_olink)) {
                        list_remove(&vma->vma_olink);
			dbg(DBG_PRINT, "(GRADING3A)\n");
                }
                if (vma->vma_obj) {
                        vma->vma_obj->mmo_ops->put(vma->vma_obj);
			dbg(DBG_PRINT, "(GRADING3A)\n");
                }
                vmarea_free(vma);
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();
        map->vmm_proc = NULL;
        slab_obj_free(vmmap_allocator, map);
	dbg(DBG_PRINT, "(GRADING3A)\n");
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
        KASSERT(NULL != map && NULL != newvma); /* both function arguments must not be NULL */
        KASSERT(NULL == newvma->vma_vmmap); /* newvma must be newly create and must not be part of any existing vmmap */
        KASSERT(newvma->vma_start < newvma->vma_end); /* newvma must not be empty */      
        KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start &&
                ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
                /* addresses in this memory segment must lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");

        newvma->vma_vmmap = map;
        if (list_empty(&map->vmm_list)) {
                list_insert_head(&map->vmm_list, &newvma->vma_plink);
		dbg(DBG_PRINT, "(GRADING3A)\n");
                return;
        }
        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                if (vma->vma_start > newvma->vma_start) {
                        list_insert_before(&vma->vma_plink, &newvma->vma_plink);
			dbg(DBG_PRINT, "(GRADING3A)\n");
                        return;
                }
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();
        list_insert_tail(&map->vmm_list, &newvma->vma_plink);
	dbg(DBG_PRINT, "(GRADING3A)\n");
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        /*uint32_t start;
        vmarea_t *vma;
        if (VMMAP_DIR_LOHI == dir) {
                start = ADDR_TO_PN(USER_MEM_LOW);
                list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                        if (vma->vma_start - start >= npages) {
                                return start;
                        }
                        start = vma->vma_end;
                } list_iterate_end();
                if (ADDR_TO_PN(USER_MEM_HIGH) - start >= npages) {
                        return start;
                }
        } else {
                start = ADDR_TO_PN(USER_MEM_HIGH);
                list_iterate_reverse(&map->vmm_list, vma, vmarea_t, vma_plink) {
                        if (start - vma->vma_end >= npages) {
				dbg(DBG_PRINT, "(GRADING3A)\n");
                                return start - npages;
                        }
                        start = vma->vma_start;
			dbg(DBG_PRINT, "(GRADING3D 1)\n");
                } list_iterate_end();
                if (start - ADDR_TO_PN(USER_MEM_LOW) >= npages) {
			dbg(DBG_PRINT, "(GRADING3D 2)\n");
                        return start - npages;
                }
		dbg(DBG_PRINT, "(GRADING3D 2)\n");
        }
	dbg(DBG_PRINT, "(GRADING3D 2)\n");*/
	uint32_t start;
        vmarea_t *vma;
	start = ADDR_TO_PN(USER_MEM_HIGH);
        list_iterate_reverse(&map->vmm_list, vma, vmarea_t, vma_plink) {
                if (start - vma->vma_end >= npages) {
			dbg(DBG_PRINT, "(GRADING3A)\n");
                        return start - npages;
                }
                start = vma->vma_start;
		dbg(DBG_PRINT, "(GRADING3D 1)\n");
        } list_iterate_end();
        if (start - ADDR_TO_PN(USER_MEM_LOW) >= npages) {
		dbg(DBG_PRINT, "(GRADING3D 2)\n");
                return start - npages;
        }
	dbg(DBG_PRINT, "(GRADING3D 2)\n");


        return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
        KASSERT(NULL != map); /* the first function argument must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.c)\n");

        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                if (vma->vma_start <= vfn && vfn < vma->vma_end) {
			dbg(DBG_PRINT, "(GRADING3A)\n");
                        return vma;
                }
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3C 5)\n");
        return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        vmmap_t *newmap = vmmap_create();
        /*if (NULL == newmap) {
                return NULL;
        }*/
        newmap->vmm_proc = map->vmm_proc;
        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                vmarea_t *newvma = vmarea_alloc();
                /*if (NULL == newvma) {
                        vmmap_destroy(newmap);
                        return NULL;
                }*/
                newvma->vma_start = vma->vma_start;
                newvma->vma_end	= vma->vma_end;
                newvma->vma_off = vma->vma_off;
                newvma->vma_prot = vma->vma_prot;
                newvma->vma_flags = vma->vma_flags;
                newvma->vma_vmmap = newmap;
                newvma->vma_obj = NULL;
                list_link_init(&newvma->vma_plink);
                list_link_init(&newvma->vma_olink);
                list_insert_tail(&newmap->vmm_list, &newvma->vma_plink);
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3A)\n");
        return newmap;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        KASSERT(NULL != map); /* must not add a memory segment into a non-existing vmmap */
        KASSERT(0 < npages); /* number of pages of this memory segment cannot be 0 */
        KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags)); /* must specify whether the memory segment is shared or private */
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage)); /* if lopage is not zero, it must be a user space vpn */
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
                                    /* if lopage is not zero, the specified page range must lie completely within the user space */
        KASSERT(PAGE_ALIGNED(off)); /* the off argument must be page aligned */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        if (0 == lopage) {
                int val = vmmap_find_range(map, npages, dir);
                if (-1 == val) {
			dbg(DBG_PRINT, "(GRADING3D 2)\n");
                        return -1; // ??? return value
                }
                lopage = (uint32_t)val;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } else if (!vmmap_is_range_empty(map, lopage, npages)) {
                int val = vmmap_remove(map, lopage, npages);
                /*if (val < 0) {
                        return val;
                }*/
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }

        vmarea_t *vma = vmarea_alloc();
        vma->vma_start = lopage;
        vma->vma_end = lopage + npages;
        vma->vma_off = off;
        vma->vma_prot = prot;
        vma->vma_flags = flags;
        list_link_init(&vma->vma_plink);
        list_link_init(&vma->vma_olink);

        mmobj_t *mmo;
        if (NULL == file) {
                mmo = anon_create();
                /*if (NULL == mmo) {
                        vmarea_free(vma);
                        return -1; // ??? return value
                }*/
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } else {
                int val = file->vn_ops->mmap(file, vma, &mmo);
                /*if (val < 0) {
                        vmarea_free(vma);
                        return val;
                }*/
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        vma->vma_obj = mmo;

        mmobj_t *bot = mmobj_bottom_obj(mmo);
        list_insert_tail(&bot->mmo_un.mmo_vmas, &vma->vma_olink);
        if (MAP_PRIVATE & flags) {
                mmobj_t *shadow = shadow_create();
                /*if (NULL == shadow) {
                        mmo->mmo_ops->put(mmo);
                        vmarea_free(vma);
                        return -1; // return value
                }*/
                shadow->mmo_shadowed = mmo;
                shadow->mmo_un.mmo_bottom_obj = bot;
                bot->mmo_ops->ref(bot);
                vma->vma_obj = shadow;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        if (NULL != new) {
                *new = vma;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        vmmap_insert(map, vma);
	dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        uint32_t hipage = lopage + npages;
        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                if (hipage <= vma->vma_start) {
			dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        return 0;
                } else if (vma->vma_end <= lopage) {
			dbg(DBG_PRINT, "(GRADING3A)\n");
                        continue;
                }
                if (vma->vma_start < lopage && hipage < vma->vma_end) {
                        vmarea_t *newvma = vmarea_alloc();
                        /*if (NULL == newvma) {
                                return -1; // ??? return value
                        }*/
                        newvma->vma_start = hipage;
                        newvma->vma_end = vma->vma_end;
                        newvma->vma_off = vma->vma_off + hipage - vma->vma_start;
                        newvma->vma_prot = vma->vma_prot;
                        newvma->vma_flags = vma->vma_flags;
                        newvma->vma_obj = vma->vma_obj;
                        newvma->vma_obj->mmo_ops->ref(newvma->vma_obj);
                        list_link_init(&newvma->vma_plink);
                        list_link_init(&newvma->vma_olink);
                        mmobj_t *bot = mmobj_bottom_obj(vma->vma_obj);
                        list_insert_tail(&bot->mmo_un.mmo_vmas, &newvma->vma_olink);
                        //bot->mmo_ops->ref(bot);
                        vma->vma_end = lopage;

                        vmmap_insert(map, newvma);
			dbg(DBG_PRINT, "(GRADING3D 2)\n");
                        return 0;
                        
                } else if (vma->vma_start < lopage && lopage < vma->vma_end) {
                        vma->vma_end = lopage;
			dbg(DBG_PRINT, "(GRADING3D 1)\n");
                } else if (vma->vma_start < hipage && hipage < vma->vma_end) {
                        vma->vma_off = vma->vma_off + hipage - vma->vma_start;
                        vma->vma_start = hipage;
			dbg(DBG_PRINT, "(GRADING3D 2)\n");
                        return 0;
                } else {
                        list_remove(&vma->vma_plink);
                        if (list_link_is_linked(&vma->vma_olink)) {
                                list_remove(&vma->vma_olink);
				dbg(DBG_PRINT, "(GRADING3A)\n");
                        }
                        if (vma->vma_obj) {
                                vma->vma_obj->mmo_ops->put(vma->vma_obj);
				dbg(DBG_PRINT, "(GRADING3A)\n");
                        }
                        vmarea_free(vma);
			dbg(DBG_PRINT, "(GRADING3A)\n");
                }
        } list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        /*if (npages == 0) {
                return 1;
        }*/
        vmarea_t *vma;
        uint32_t endvfn = startvfn + npages;

        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
                /* the specified page range must not be empty and lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.e)\n");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                if ((vma->vma_start <= startvfn && startvfn < vma->vma_end) ||
                    (vma->vma_start < endvfn && endvfn <= vma->vma_end) || 
                    (startvfn < vma->vma_start && vma->vma_end < endvfn)) {
			dbg(DBG_PRINT, "(GRADING3A)\n");
                        return 0;
                }
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3A)\n");
        return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        uint32_t addr = (uint32_t)vaddr;
        char *buffer = (char *)buf;
        while (count > 0) {
                uint32_t pn = ADDR_TO_PN(addr);
                vmarea_t *vma = vmmap_lookup(map, pn);
                KASSERT(NULL != vma);

                pframe_t *pf;
                int val = pframe_lookup(vma->vma_obj, 
                          pn - vma->vma_start + vma->vma_off, 0, &pf);
                /*if (val < 0) {
                        return val;
                }*/

                uint32_t offset = PAGE_OFFSET(addr);
                char *ptr = (char *)pf->pf_addr + offset;
                size_t len = MIN(count, PAGE_SIZE - offset);
                memcpy(buffer, ptr, len);

                addr += len;
                count -= len;
                buffer += len;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }
	dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        uint32_t addr = (uint32_t)vaddr;
        char* buffer = (char *)buf;
        while (count > 0) {
                uint32_t pn = ADDR_TO_PN(addr);
                vmarea_t *vma = vmmap_lookup(map, pn);
                KASSERT(NULL != vma);

                pframe_t *pf;
                int val = pframe_lookup(vma->vma_obj, 
                          pn - vma->vma_start + vma->vma_off, 1, &pf);
                /*if (val < 0) {
                        return val;
                }*/

                uint32_t offset = PAGE_OFFSET(addr);
                char *ptr = (char *)pf->pf_addr + offset;
                size_t len = MIN(count, PAGE_SIZE - offset);
                memcpy(ptr, buffer, len);
                pframe_dirty(pf);

                addr += len;
                count -= len;
                buffer += len;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }
	dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;
}
