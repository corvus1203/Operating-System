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

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
    KASSERT(regs != NULL);	
    KASSERT(curproc != NULL);
    KASSERT(curproc->p_state == PROC_RUNNING);
    dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

    vmarea_t *vma, *clone_vma;
    pframe_t *pf;
    mmobj_t *to_delete, *new_shadowed;
    
    // step 2 & 3
    vmmap_t *parent_map = curproc->p_vmmap;
    vmmap_t *clone_map = vmmap_clone(parent_map);
    /*if(NULL == clone_map){
        curthr->kt_errno = ENOMEM;
        return -ENOMEM;
    }*/
    list_iterate_begin(&parent_map->vmm_list, vma, vmarea_t, vma_plink) {
        clone_vma = vmmap_lookup(clone_map, vma->vma_start);
        if(clone_vma->vma_flags & MAP_SHARED){
            clone_vma->vma_obj = vma->vma_obj;
            clone_vma->vma_obj->mmo_ops->ref(clone_vma->vma_obj);
            //maybe
            list_insert_tail(&clone_vma->vma_obj->mmo_un.mmo_vmas, &clone_vma->vma_olink);
            dbg(DBG_PRINT, "(GRADING3D 1)\n");
        }else{
            mmobj_t *clone_shadow = shadow_create();
            mmobj_t *parent_shadow = shadow_create();
            
            mmobj_t *shadowed = vma->vma_obj;
            mmobj_t *bot = mmobj_bottom_obj(shadowed);
            
            clone_shadow->mmo_shadowed = shadowed;
            parent_shadow->mmo_shadowed = shadowed;
            clone_shadow->mmo_un.mmo_bottom_obj = bot;
            parent_shadow->mmo_un.mmo_bottom_obj = bot;
            bot->mmo_ops->ref(bot);
            bot->mmo_ops->ref(bot);
            shadowed->mmo_ops->ref(shadowed);
            
            clone_vma->vma_obj = clone_shadow;
            vma->vma_obj = parent_shadow;
            
            list_insert_head(&bot->mmo_un.mmo_vmas, &clone_vma->vma_olink);
            dbg(DBG_PRINT, "(GRADING3A)\n");
        }
    } list_iterate_end();
    
    // step 4
    tlb_flush_all();
    pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
    
    // step 8
    kthread_t *parent_thr = list_item(curproc->p_threads.l_next, kthread_t, kt_plink);
    kthread_t *clone_thr = kthread_clone(parent_thr);
    /*if(NULL == clone_thr){
        curthr->kt_errno = ENOMEM;
        vmmap_destroy(clone_map);
        return -ENOMEM;
    }*/

    KASSERT(clone_thr->kt_kstack != NULL);
    dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
    
    // step 1, 7, proc need to be clean
    proc_t *clone_proc = proc_create("clone");
    /*if(NULL == clone_proc){
        curthr->kt_errno = ENOMEM;
        vmmap_destroy(clone_map);
        kthread_destroy(clone_thr);
        return -ENOMEM;
    }*/
    vmmap_destroy(clone_proc->p_vmmap);
    clone_proc->p_vmmap = clone_map;
    clone_map->vmm_proc = clone_proc;
    clone_thr->kt_proc = clone_proc;
    list_insert_tail(&clone_proc->p_threads, &clone_thr->kt_plink);

    KASSERT(clone_proc->p_state == PROC_RUNNING);  
    KASSERT(clone_proc->p_pagedir != NULL);  
    dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

    // step 5
    clone_thr->kt_ctx.c_pdptr = clone_proc->p_pagedir;
    clone_thr->kt_ctx.c_eip = (uintptr_t) userland_entry;
    regs->r_eax = 0;
    clone_thr->kt_ctx.c_esp = fork_setup_stack(regs, clone_thr->kt_kstack);
    // need to set ebp
    clone_thr->kt_ctx.c_ebp = clone_thr->kt_ctx.c_esp;
    regs->r_eax = clone_proc->p_pid;

    // step 6
    for(int i = 0; i < NFILES; i++) {
        if(NULL != curproc->p_files[i]){
            clone_proc->p_files[i] = curproc->p_files[i];
            fref(clone_proc->p_files[i]);
            dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        dbg(DBG_PRINT, "(GRADING3A)\n");
    }
    
    // step 9
    clone_proc->p_brk = curproc->p_brk;
    clone_proc->p_start_brk = curproc->p_start_brk;
    
    // step 10
    sched_make_runnable(clone_thr);
    dbg(DBG_PRINT, "(GRADING3A)\n");
    return clone_proc->p_pid;
}
