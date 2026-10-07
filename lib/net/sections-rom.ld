#include <zephyr/linker/iterable_sections.h>

/* Resource table for HTTP_SERVICE_DEFINE(poko_http_service, ...) in web_server.c */
ITERABLE_SECTION_ROM(http_resource_desc_poko_http_service, Z_LINK_ITERABLE_SUBALIGN)
