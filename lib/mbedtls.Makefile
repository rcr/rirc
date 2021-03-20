MBEDTLS_VER     := 2.25.0
MBEDTLS_VER_SHA := f838f670f51070bc6b4ebf0c084affd9574652ded435b064969f36ce4e8b586d

MBEDTLS_CFG := $(abspath $(PATH_LIB)/mbedtls.h)
MBEDTLS_SHA := $(abspath $(PATH_LIB)/mbedtls.sha256)
MBEDTLS_SRC := $(abspath $(PATH_LIB)/mbedtls-$(MBEDTLS_VER))
MBEDTLS_TAR := $(abspath $(PATH_LIB)/mbedtls-$(MBEDTLS_VER).tar.gz)
MBEDTLS_URL := https://github.com/ARMmbed/mbedtls/archive/v$(MBEDTLS_VER).tar.gz

MBEDTLS_LIBS := \
	$(MBEDTLS_SRC)/library/libmbedtls.a \
	$(MBEDTLS_SRC)/library/libmbedx509.a \
	$(MBEDTLS_SRC)/library/libmbedcrypto.a

$(MBEDTLS_LIBS): $(MBEDTLS_CFG) $(MBEDTLS_SRC)
	@$(MAKE) --silent -C $(MBEDTLS_SRC) clean
	@$(MAKE) --silent -C $(MBEDTLS_SRC) CFLAGS="$(CFLAGS) -DMBEDTLS_CONFIG_FILE='<$(MBEDTLS_CFG)>'" lib

$(MBEDTLS_SRC): $(MBEDTLS_TAR)
	@tar -xmf $(MBEDTLS_TAR) --directory $(PATH_LIB)

$(MBEDTLS_TAR):
	@echo "$(MBEDTLS_TAR)..."
	@curl -LfsS $(MBEDTLS_URL) -o $(MBEDTLS_TAR)
	@eval $(MBEDTLS_SHA_FILE)
	@eval $(MBEDTLS_SHA_CHECK)

ifneq ($(shell command -v shasum 2>/dev/null),)
MBEDTLS_SHA_FILE  := 'echo "$(MBEDTLS_VER_SHA)  $(MBEDTLS_TAR)" > $(MBEDTLS_SHA)'
MBEDTLS_SHA_CHECK := 'shasum -c $(MBEDTLS_SHA)'
endif

RIRC_CFLAGS += -I$(MBEDTLS_SRC)/include/
RIRC_CFLAGS += -DMBEDTLS_CONFIG_FILE='<$(MBEDTLS_CFG)>'

RIRC_LIBS += $(MBEDTLS_LIBS)

.DELETE_ON_ERROR:
