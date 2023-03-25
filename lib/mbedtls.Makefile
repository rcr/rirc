MBEDTLS_VER = 3.3.0
MBEDTLS_SHA = 113fa84bc3cf862d56e7be0a656806a5d02448215d1e22c98176b1c372345d33

MBEDTLS_DIR = $(shell dirname $(realpath lib/mbedtls.Makefile))

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
	@tar xf $(MBEDTLS_TAR) -C $(MBEDTLS_DIR)
