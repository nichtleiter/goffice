/*
 * go-color-selector.h - A color selector
 *
 * Copyright (c) 2006 Emmanuel Pacaud (emmanuel.pacaud@lapp.in2p3.fr)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#ifndef GO_COLOR_SELECTOR_H
#define GO_COLOR_SELECTOR_H

#include <goffice/goffice.h>

G_BEGIN_DECLS

GtkWidget *go_selector_new_color	(GOColor initial_color,
					 GOColor default_color,
					 char const *color_group);
#ifndef GOFFICE_DISABLE_DEPRECATED
GOFFICE_DEPRECATED_FOR(go_selector_new_color)
GtkWidget *go_color_selector_new	(GOColor initial_color,
					 GOColor default_color,
					 char const *color_group);
#endif
GOColor    go_color_selector_get_color 	(GOSelector *selector, gboolean *is_auto);
gboolean   go_color_selector_set_color  (GOSelector *selector, GOColor color);
void       go_color_selector_set_allow_alpha   (GOSelector *selector, gboolean allow_alpha);

G_END_DECLS

#endif /* GO_COLOR_SELECTOR_H */
