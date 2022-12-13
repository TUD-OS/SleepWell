MWAIT_MODULE_VERSION = 0.1
MWAIT_MODULE_SITE = $(BR2_EXTERNAL_MWAIT_MODULE_PATH)
MWAIT_MODULE_SITE_METHOD = local
$(eval $(kernel-module))
$(eval $(generic-package))
