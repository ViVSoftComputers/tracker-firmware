# Install script for directory: C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Meshtastic")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/pio2/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/aes.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/aria.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/asn1.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/asn1write.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/base64.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/bignum.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/block_cipher.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/build_info.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/camellia.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ccm.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/chacha20.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/chachapoly.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/check_config.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/cipher.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/cmac.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/compat-2.x.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config_adjust_x509.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/config_psa.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/constant_time.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ctr_drbg.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/debug.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/des.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/dhm.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ecdh.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ecdsa.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ecjpake.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ecp.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/entropy.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/error.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/gcm.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/hkdf.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/hmac_drbg.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/lms.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/mbedtls_config.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/md.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/md5.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/net_sockets.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/nist_kw.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/oid.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/pem.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/pk.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/pkcs12.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/pkcs5.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/pkcs7.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/platform.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/platform_time.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/platform_util.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/poly1305.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/private_access.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/psa_util.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ripemd160.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/rsa.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/sha1.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/sha256.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/sha3.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/sha512.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ssl.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ssl_cache.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ssl_cookie.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/ssl_ticket.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/threading.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/timing.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/version.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/x509.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/x509_crl.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/x509_crt.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/build_info.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_dependencies.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_builtin_composites.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_builtin_primitives.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_compat.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_config.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_driver_common.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_extra.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_legacy.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_platform.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_se_driver.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_sizes.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_struct.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_types.h"
    "C:/pio2/packages/framework-espidf/components/mbedtls/mbedtls/include/psa/crypto_values.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/vivso/Documents/Friday/DEV/Meshtastic/.pio/build/heltec-v3/esp-idf/mbedtls/mbedtls/include/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
