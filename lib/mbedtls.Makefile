MBEDTLS_VER     := 2.25.0
MBEDTLS_VER_SHA := f838f670f51070bc6b4ebf0c084affd9574652ded435b064969f36ce4e8b586d

MBEDTLS_CFG := $(abspath $(DIR_L)/mbedtls.h)
MBEDTLS_SHA := $(abspath $(DIR_L)/mbedtls.sha256)
MBEDTLS_SRC := $(abspath $(DIR_L)/mbedtls-$(MBEDTLS_VER))
MBEDTLS_TAR := $(abspath $(DIR_L)/mbedtls-$(MBEDTLS_VER).tar.gz)
MBEDTLS_URL := https://github.com/ARMmbed/mbedtls/archive/v$(MBEDTLS_VER).tar.gz

MBEDTLS_LIBS := \
	$(MBEDTLS_SRC)/library/libmbedtls.a \
	$(MBEDTLS_SRC)/library/libmbedx509.a \
	$(MBEDTLS_SRC)/library/libmbedcrypto.a

$(MBEDTLS_LIBS): $(MBEDTLS_CFG) $(MBEDTLS_SRC)
	@$(MAKE) --silent -C $(MBEDTLS_SRC) clean
	@$(MAKE) --silent -C $(MBEDTLS_SRC) CFLAGS="$(CFLAGS) -DMBEDTLS_CONFIG_FILE='<$(MBEDTLS_CFG)>'" lib

$(MBEDTLS_SRC): $(MBEDTLS_SHA)
	@echo "curl $(MBEDTLS_TAR)..."
	@curl -LfsS $(MBEDTLS_URL) -o $(MBEDTLS_TAR)
	@shasum -a 256 -q --check $(MBEDTLS_SHA)
	@tar -xmf $(MBEDTLS_TAR) --directory $(DIR_L)

$(MBEDTLS_SHA):
	@echo "$(MBEDTLS_VER_SHA)  $(MBEDTLS_TAR)" > $(MBEDTLS_SHA)
