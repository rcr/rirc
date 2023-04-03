MBEDTLS_VER = 3.4.0
MBEDTLS_SHA = 1b899f355022e8d02c4d313196a0a16af86c5a692456fa99d302915b8cf0320a

MBEDTLS_DIR = $(shell dirname $(realpath $(filter %mbedtls.Makefile,$(MAKEFILE_LIST))))

MBEDTLS_CFG = $(MBEDTLS_DIR)/mbedtls.h
MBEDTLS_SRC = $(MBEDTLS_DIR)/mbedtls-$(MBEDTLS_VER)
MBEDTLS_TAR = $(MBEDTLS_DIR)/mbedtls-$(MBEDTLS_VER).tar.gz
MBEDTLS_URL = https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v$(MBEDTLS_VER).tar.gz

MBEDTLS = \
	$(MBEDTLS_SRC)/library/libmbedtls.a \
	$(MBEDTLS_SRC)/library/libmbedx509.a \
	$(MBEDTLS_SRC)/library/libmbedcrypto.a

MBEDTLS_CFLAGS = -I$(MBEDTLS_SRC)/include -DMBEDTLS_CONFIG_FILE='<$(MBEDTLS_CFG)>'

%/libmbedtls.a: $(MBEDTLS_SRC)
	@$(MAKE) -C $(MBEDTLS_SRC) CC="$(CC)" CFLAGS="$(CFLAGS) -DMBEDTLS_CONFIG_FILE='<$(MBEDTLS_CFG)>'" LDFLAGS="$(LDFLAGS)" lib

%/libmbedx509.a: %/libmbedtls.a
	@/bin/sh -c true

%/libmbedcrypto.a: %/libmbedtls.a
	@/bin/sh -c true

$(MBEDTLS_SRC):
	@echo "$(MBEDTLS_TAR)..."
	@curl -LfsS $(MBEDTLS_URL) -o $(MBEDTLS_TAR)
	-@command -v shasum > /dev/null || echo ' -- NO SHASUM -- '
	-@command -v shasum > /dev/null && echo "$(MBEDTLS_SHA) *$(MBEDTLS_TAR)" | shasum -c -
	@tar xzf $(MBEDTLS_TAR) -C $(MBEDTLS_DIR)
