ENTRY(nvc_entrypoint)

SECTIONS {
    .interp :
    {
        KEEP(*(.interp))
    }
    .note.ABI-tag :
    {
        KEEP(*(.note.ABI-tag))
    }
}

VERSION {
    NVC_1.0 {
        global:
            nvc_version;
            nvc_context_new;
            nvc_context_free;
            nvc_config_new;
            nvc_config_free;
            nvc_init;
            nvc_shutdown;
            nvc_error;
            nvc_ldcache_update;
            nvc_container_config_new;
            nvc_container_config_free;
            nvc_container_new;
            nvc_container_free;
            nvc_driver_info_new;
            nvc_driver_info_free;
            nvc_device_info_new;
            nvc_device_info_free;
            nvc_driver_mount;
            nvc_device_mount;
        local:
            *;
    };
}
