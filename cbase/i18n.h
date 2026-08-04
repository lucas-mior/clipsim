// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(I18N_H)
#define I18N_H

#include "platform_detection.h"

#if !defined(GETTEXT_PACKAGE)
#define GETTEXT_PACKAGE "cecup"
#endif

#if CBASE_HAS_GETTEXT
  #include <libintl.h>

  #define _(String) gettext(String)
  #define N_(String) String
#else
  #define _(String) String
  #define N_(String) String
  #define bindtextdomain(Domain, Directory) \
    ((void)(Domain), (char *)(Directory))
  #define bind_textdomain_codeset(Domain, Codeset) \
    ((void)(Domain), (char *)(Codeset))
  #define textdomain(Domain) (char *)(Domain)
#endif

#endif /* I18N_H */
