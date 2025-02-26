/* Copyright (C) 2019-2020 Greenbone Networks GmbH
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GVMD_GMP_CONFIGS_H
#define _GVMD_GMP_CONFIGS_H

#include "gmp_base.h"

#include <gvm/base/array.h>
#include <gvm/util/xmlutils.h>

void
create_config_start (gmp_parser_t *, const gchar **, const gchar **);

void
create_config_element_start (gmp_parser_t *, const gchar *, const gchar **,
                             const gchar **);

int
create_config_element_end (gmp_parser_t *, GError **error, const gchar *);

void
create_config_element_text (const gchar *, gsize);

int
parse_config_entity (entity_t, const char **, char **, char **, char **,
                     char **, int *, array_t **, array_t **);

#endif /* not _GVMD_GMP_CONFIGS_H */
