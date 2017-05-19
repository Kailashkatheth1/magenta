// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/driver.h>
#include <magenta/compiler.h>

__BEGIN_CDECLS;

typedef struct {
    mx_status_t (*find_protocol)(mx_device_t* dev, uint32_t proto_id,
                                 mx_device_t** out_dev, void** out_proto);
    mx_status_t (*set_protocol)(mx_device_t* dev, uint32_t proto_id, void* proto);
} platform_device_protocol_t;

// Looks for a platform device that implements a given protocol 
mx_status_t platform_device_find_protocol(mx_device_t* dev, uint32_t proto_id,
                                          mx_device_t** out_dev, void** out_proto);

// sets protocol for a platform device, to be looked up with platform_device_find_protocol()
mx_status_t platform_device_set_protocol(mx_device_t* pdev, uint32_t proto_id, void* proto);

__END_CDECLS;
