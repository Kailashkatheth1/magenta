// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include "kernel/vm/vm_object.h"

#include "vm_priv.h"

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <kernel/auto_lock.h>
#include <kernel/vm.h>
#include <kernel/vm/vm_address_region.h>
#include <lib/console.h>
#include <mxtl/ref_ptr.h>
#include <safeint/safe_math.h>
#include <stdlib.h>
#include <string.h>
#include <trace.h>

#define LOCAL_TRACE MAX(VM_GLOBAL_TRACE, 0)

VmObject::VmObject(mxtl::RefPtr<VmObject> parent)
    : lock_(parent ? parent->lock_ref() : local_lock_),
      parent_(mxtl::move(parent)) {
    LTRACEF("%p\n", this);
}

VmObject::~VmObject() {
    canary_.Assert();
    LTRACEF("%p\n", this);

    // remove ourself from our parent (if present)
    if (parent_) {
        LTRACEF("removing ourself from our parent %p\n", parent_.get());

        // conditionally grab our shared lock with the parent, but only if it's
        // not held. There are some destruction paths that may try to tear
        // down the object with the parent locks held.
        bool need_lock = !lock_.IsHeld();
        if (need_lock)
            lock_.Acquire();
        parent_->RemoveChildLocked(this);
        if (need_lock)
            lock_.Release();
    }

    DEBUG_ASSERT(mapping_list_.is_empty());
    DEBUG_ASSERT(children_list_.is_empty());
}

bool VmObject::is_cow_clone() const {
    canary_.Assert();
    AutoLock a(&lock_);
    return parent_ != nullptr;
}

void VmObject::AddMappingLocked(VmMapping* r) {
    canary_.Assert();
    DEBUG_ASSERT(lock_.IsHeld());
    mapping_list_.push_front(r);
    mapping_list_len_++;
}

void VmObject::RemoveMappingLocked(VmMapping* r) {
    canary_.Assert();
    DEBUG_ASSERT(lock_.IsHeld());
    mapping_list_.erase(*r);
    DEBUG_ASSERT(mapping_list_len_ > 0);
    mapping_list_len_--;
}

uint32_t VmObject::num_mappings() const {
    canary_.Assert();
    AutoLock a(&lock_);
    return mapping_list_len_;
}

void VmObject::AddChildLocked(VmObject* o) {
    canary_.Assert();
    DEBUG_ASSERT(lock_.IsHeld());
    children_list_.push_front(o);
    children_list_len_++;
}

void VmObject::RemoveChildLocked(VmObject* o) {
    canary_.Assert();
    DEBUG_ASSERT(lock_.IsHeld());
    children_list_.erase(*o);
    DEBUG_ASSERT(children_list_len_ > 0);
    children_list_len_--;
}

uint32_t VmObject::num_children() const {
    canary_.Assert();
    AutoLock a(&lock_);
    return children_list_len_;
}

void VmObject::RangeChangeUpdateLocked(uint64_t offset, uint64_t len) {
    canary_.Assert();
    DEBUG_ASSERT(lock_.IsHeld());

    // offsets for vmos needn't be aligned, but vmars use aligned offsets
    const uint64_t aligned_offset = ROUNDDOWN(offset, PAGE_SIZE);
    const uint64_t aligned_len = ROUNDUP(offset + len, PAGE_SIZE) - aligned_offset;

    // other mappings may have covered this offset into the vmo, so unmap those ranges
    for (auto& m : mapping_list_) {
        m.UnmapVmoRangeLocked(aligned_offset, aligned_len);
    }

    // inform all our children this as well, so they can inform their mappings
    for (auto& child : children_list_) {
        child.RangeChangeUpdateFromParentLocked(offset, len);
    }
}

static int cmd_vm_object(int argc, const cmd_args* argv, uint32_t flags) {
    if (argc < 2) {
    notenoughargs:
        printf("not enough arguments\n");
    usage:
        printf("usage:\n");
        printf("%s dump <address>\n", argv[0].str);
        printf("%s dump_pages <address>\n", argv[0].str);
        return ERR_INTERNAL;
    }

    if (!strcmp(argv[1].str, "dump")) {
        if (argc < 2)
            goto notenoughargs;

        VmObject* o = reinterpret_cast<VmObject*>(argv[2].u);

        o->Dump(0, false);
    } else if (!strcmp(argv[1].str, "dump_pages")) {
        if (argc < 2)
            goto notenoughargs;

        VmObject* o = reinterpret_cast<VmObject*>(argv[2].u);

        o->Dump(0, true);
    } else {
        printf("unknown command\n");
        goto usage;
    }

    return NO_ERROR;
}

STATIC_COMMAND_START
#if LK_DEBUGLEVEL > 0
STATIC_COMMAND("vm_object", "vm object debug commands", &cmd_vm_object)
#endif
STATIC_COMMAND_END(vm_object);
