/* This file is part of GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2003 Calvin Williamson
 *           2006-2008 Øyvind Kolås
 *           2013 Daniel Sabo
 */

#ifndef __GEGL_OPERATION_CONTEXT_H__
#define __GEGL_OPERATION_CONTEXT_H__

G_BEGIN_DECLS

typedef struct _GeglOperationPipeLine GeglOperationPipeLine;

GeglBuffer     *gegl_operation_context_get_target      (GeglOperationContext *self,
                                                        const gchar          *padname);
GeglBuffer     *gegl_operation_context_get_source      (GeglOperationContext *self,
                                                        const gchar          *padname) G_GNUC_DEPRECATED;
GObject        *gegl_operation_context_dup_object      (GeglOperationContext *self,
                                                        const gchar          *padname);
GObject        *gegl_operation_context_get_object      (GeglOperationContext *context,
                                                        const gchar          *padname);

void            gegl_operation_context_set_object      (GeglOperationContext *context,
                                                        const gchar          *padname,
                                                        GObject              *data);
void            gegl_operation_context_take_object     (GeglOperationContext *context,
                                                        const gchar          *padname,
                                                        GObject              *data);

void            gegl_operation_context_purge           (GeglOperationContext *self);

gint            gegl_operation_context_get_level       (GeglOperationContext *self);

/* the rest of these functions are for internal use only */

GeglBuffer *    gegl_operation_context_get_output_maybe_in_place (GeglOperation        *operation,
                                                                  GeglOperationContext *context,
                                                                  GeglBuffer           *input,
                                                                  const GeglRectangle  *roi);
GeglBuffer *    gegl_operation_context_dup_input_maybe_copy      (GeglOperationContext *context,
                                                                  const gchar          *padname,
                                                                  const GeglRectangle  *roi);


GeglOperationContext *gegl_operation_context_node_get_context (GeglOperationContext *context,
                                                               GeglNode             *node);

GeglOperationPipeLine *gegl_operation_context_get_pipeline (GeglOperationContext *context);

void      gegl_operation_context_set_pipeline (GeglOperationContext  *context,
                                               GeglOperationPipeLine *pipeline);


G_END_DECLS

#endif /* __GEGL_OPERATION_CONTEXT_H__ */
