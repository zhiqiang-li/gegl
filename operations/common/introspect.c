/* This file is an image processing operation for GEGL
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
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */
#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_object(node, "Node", "GeglNode to introspect")
gegl_chant_pointer(buf, "Buffer", "Buffer")

#else

#define GEGL_CHANT_TYPE_SOURCE
#define GEGL_CHANT_C_FILE       "introspect.c"

#include "gegl-chant.h"
#include "gegl-dot.h" /* XXX: internal header file */
#include <stdio.h>
#include <string.h>

static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);

  GeglRectangle result = {0, 0, 4096, 4096};
//  process (operation, NULL, NULL, NULL);

  if (o->buf)
    {
      GeglBuffer *buffer = GEGL_BUFFER (o->buf);
      result = *gegl_buffer_get_extent (buffer);
    }
  return result;
}

static gboolean
process (GeglOperation       *operation,
         GeglOperationContext     *context,
         const gchar         *output_pad,
         const GeglRectangle *result)
{
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);

  if (o->buf)
    {
      if (context)
        {
          g_object_ref (o->buf); /* add an extra reference, since gegl_operation_set_data
                                      is stealing one */
          gegl_operation_context_set_object (context, "output", G_OBJECT (o->buf));
        }
      return TRUE;
    }

  {
    {
      gchar *dot = gegl_to_dot ((gpointer)o->node);
      g_file_set_contents ("/tmp/gegl-temp.dot", dot, -1, NULL);
      system ("dot -o/tmp/gegl-temp.png -Tpng /tmp/gegl-temp.dot");
      g_free (dot);
    }

    /* FIXME: copy behavior from magick-load to fix this op */

    {
      GeglNode *gegl = gegl_node_new ();
      GeglNode *png_load = gegl_node_new_child (gegl, "operation", "load", "path", "/tmp/gegl-temp.png", NULL);
      GeglNode *buffer_save;
      GeglRectangle defined;

      defined = gegl_node_get_bounding_box (png_load);

      o->buf = gegl_buffer_new (&defined, babl_format ("R'G'B' u8"));

      buffer_save = gegl_node_new_child (gegl, "operation", "save-buffer", "buffer", o->buf, NULL);
      gegl_node_link_many (png_load, buffer_save, NULL);

      gegl_node_process (buffer_save);
      g_object_unref (gegl);
      system ("rm /tmp/gegl-temp.*");
    }

  if (context)
    {
      g_object_ref (o->buf);
      gegl_operation_context_set_object (context, "output", G_OBJECT (o->buf));
    }
  }

  return  TRUE;
}


static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationSourceClass *source_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  source_class    = GEGL_OPERATION_SOURCE_CLASS (klass);

  operation_class->process = process;
  operation_class->get_bounding_box = get_bounding_box;

  operation_class->name        = "introspect";
  operation_class->categories  = "render";
  operation_class->description = "GEGL graph vizualizer.";
}

#endif

