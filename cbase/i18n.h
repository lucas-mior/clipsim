// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(I18N_H)
#define I18N_H

#include "platform_detection.h"

#if OS_LINUX
  #include <libintl.h>
  #include <locale.h>
  
  #if !defined(GETTEXT_PACKAGE)
  #define GETTEXT_PACKAGE "cecup"
  #endif
  
  #define _(String) gettext(String)
  #define N_(String) String
#else
  #define GETTEXT_PACKAGE
  #define _(String) String
  #define N_(String) String
#endif

#endif /* I18N_H */
