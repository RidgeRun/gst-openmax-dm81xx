# makefile to build shared lib
all:
	@mkdir -p lib
	@echo "  CCLD   lib/libOMX_Core.so"
	@$(CROSS_COMPILE)gcc -Wl,-soname,libOMX_Core -shared  \
	$(OMX_INSTALL_DIR)/lib/omxcore.av5T \
	$(OMX_INSTALL_DIR)/lib/ti/omx/domx/lib/a8host/debug/domx.av5T \
	$(OSAL_INSTALL_DIR)/packages/linuxdist/build/lib/osal.a \
	$(OMX_INSTALL_DIR)/lib/omxobjs/obj/$(OMX_PLATFORM)/a8host/debug/omxcore_cfg.ov5T \
	$(OMX_INSTALL_DIR)/lib/omxobjs/obj/$(OMX_PLATFORM)/a8host/debug/domx_cfg.ov5T \
    $(OMX_INSTALL_DIR)/lib/memcfg.av5T \
    $(OMX_INSTALL_DIR)/lib/omxobjs/obj/$(OMX_PLATFORM)/a8host/debug/memtbl_cfg.ov5T  \
    $(OMX_INSTALL_DIR)/lib/timmosal.av5T \
	$(SYSLINK_INSTALL_DIR)/packages/ti/syslink/lib/syslink.a_release \
	$(FC_INSTALL_DIR)/packages/ti/sdo/rcm/lib/debug/rcm_syslink.av5T \
	$(OMX_INSTALL_DIR)/lib/domx_delegates_shmem.av5T \
	$(FC_INSTALL_DIR)/packages/ti/sdo/fc/memutils/lib/release/memutils.av5T \
	$(OSAL_INSTALL_DIR)/packages/linuxdist/cstubs/lib/cstubs.a \
	$(OMX_INSTALL_DIR)/lib/omxobjs/obj/$(OMX_PLATFORM)/a8host/debug/domxProxyWrappers.ov5T \
	$(FC_INSTALL_DIR)/packages/ti/sdo/fc/global/lib/debug/fcsettings.av5T \
	$(LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV \
	$(OMX_INSTALL_DIR)/lib/ti/omx/domx/obj/a8host/debug/OmxRpcCbSkel.ov5T \
	$(OMX_INSTALL_DIR)/lib/omxobjs/obj/$(OMX_PLATFORM)/a8host/debug/domx_delegates_shmem_cfg.ov5T \
	$(UIA_INSTALL_DIR)/packages/ti/uia/linux/lib/servicemgr.a \
	-lc -lstdc++ -lm -pthread -lrt -o lib/libOMX_Core.so -Wl,--fix-cortex-a8 -Wl,--no-undefined \
	-Wl,-u,OMX_Init -Wl,-u,OMX_Deinit -Wl,-u,OMX_ComponentNameEnum -Wl,-u,OMX_GetHandle -Wl,-u,OMX_FreeHandle \
	 -Wl,-u,OMX_GetRolesOfComponent

clean:
	rm -rf lib

install: all

distdir: 
	mkdir -p $(distdir)
	cp -f Makefile $(distdir)

distclean:
