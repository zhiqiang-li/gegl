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
 * Copyright 2006 Øyvind Kolås
 */


#define GEGL_ITERATOR2_API
#include "config.h"

#include <glib-object.h>

#include "gegl.h"
#include "gegl-debug.h"
#include "gegl-operation-point-composer.h"
#include "gegl-operation-context.h"
#include "gegl-operation-pipeline.h"
#include "gegl-config.h"
#include "gegl-types-internal.h"
#include "gegl-buffer-private.h"
#include "gegl-tile-storage.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "opencl/gegl-cl.h"
#include "gegl-buffer-cl-cache.h"
#include "gegl-buffer-cl-iterator.h"

typedef struct ThreadData
{
  GeglOperationPointComposerClass *klass;
  GeglOperation                   *operation;
  GeglBuffer                      *input;
  GeglBuffer                      *aux;
  GeglBuffer                      *output;
  gint                            *pending;
  gint                             level;
  gboolean                         success;
  GeglRectangle                    result;

  const Babl *input_format;
  const Babl *aux_format;
  const Babl *output_format;
} ThreadData;

static void thread_process (gpointer thread_data, gpointer unused)
{
  ThreadData *data = thread_data;
  gint read = 0;
  gint aux  = 0;
  GeglBufferIterator *i = gegl_buffer_iterator_new (data->output,
                                                    &data->result,
                                                    data->level,
                                                    data->output_format,
                                                    GEGL_ACCESS_WRITE,
                                                    GEGL_ABYSS_NONE, 3);

  if (data->input)
    read = gegl_buffer_iterator_add (i, data->input, &data->result, data->level,
                                     data->input_format,
                                     GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
  if (data->aux)
    aux = gegl_buffer_iterator_add (i, data->aux, &data->result, data->level,
                                    data->aux_format,
                                    GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

  while (gegl_buffer_iterator_next (i))
  {
     data->success =
     data->klass->process (data->operation, data->input?i->items[read].data:NULL,
                           data->aux?i->items[aux].data:NULL,
                           i->items[0].data, i->length, &(i->items[0].roi), data->level);
  }

  g_atomic_int_add (data->pending, -1);
}

static GThreadPool *thread_pool (void)
{
  static GThreadPool *pool = NULL;
  if (!pool)
    {
      pool =  g_thread_pool_new (thread_process, NULL, gegl_config_threads (),
                                 FALSE, NULL);
    }
  return pool;
}

static gboolean
gegl_operation_composer_process (GeglOperation        *operation,
                                 GeglOperationContext *context,
                                 const gchar          *output_prop,
                                 const GeglRectangle  *result,
                                 gint                  level)
{
  GeglOperationComposerClass *klass   = GEGL_OPERATION_COMPOSER_GET_CLASS (operation);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_GET_CLASS (operation);
  GeglBuffer                 *input;
  GeglBuffer                 *aux;
  GeglBuffer                 *output;
  gboolean                    success = FALSE;
  GeglOperationPipeLine      *pipeline;

  GeglRectangle scaled_result = *result;
  if (level)
  {
    scaled_result.x >>= level;
    scaled_result.y >>= level;
    scaled_result.width >>= level;
    scaled_result.height >>= level;
    result = &scaled_result;
  }

  if (strcmp (output_prop, "output"))
    {
      g_warning ("requested processing of %s pad on a composer", output_prop);
      return FALSE;
    }

  if (result->width == 0 || result->height == 0)
  {
    output = gegl_operation_context_get_target (context, "output");
    return TRUE;
  }

  input  = (GeglBuffer*) gegl_operation_context_dup_object (context, "input");
  aux   = (GeglBuffer*) gegl_operation_context_dup_object (context, "aux");

  if (!input && !aux)
    return FALSE;

  if (gegl_operation_is_pipelinable (operation))
  {
    pipeline = gegl_operation_pipeline_ensure (operation, context, input);

    gegl_operation_pipeline_add (pipeline, operation, 2,
         gegl_operation_get_format (operation, "input"),
         gegl_operation_get_format (operation, "output"),
         gegl_operation_get_format (operation, "aux"),
         NULL,
         aux, NULL,
         point_composer_class->process);

    if (gegl_operation_pipeline_is_intermediate_node (operation, pipeline))
    {
      gegl_operation_context_take_object (context, "output", G_OBJECT (input));

      return TRUE;
    }

    output = gegl_operation_context_get_output_maybe_in_place (operation,
                                                               context,
                                                               input,
                                                               result);

    gegl_operation_context_set_pipeline (context, NULL);
    if (gegl_operation_pipeline_get_entries (pipeline) > 1)
    {
      gegl_operation_pipeline_process (pipeline, output, result, level);
      gegl_operation_pipeline_destroy (pipeline);
      return TRUE;
    }
    g_object_ref (input);
    if (aux)
      g_object_ref (aux);
    gegl_operation_pipeline_destroy (pipeline);

  }
  else
  {
    output = gegl_operation_context_get_output_maybe_in_place (operation,
                                                               context,
                                                               input,
                                                               result);
  }

  /* A composer with a NULL aux, can still be valid, the
   * subclass has to handle it.
   */
  if (input != NULL ||
      aux != NULL)
    {
      success = klass->process (operation, input, aux, output, result, level);

      g_clear_object (&input);
      g_clear_object (&aux);
    }
  else
    {
      g_warning ("%s received NULL input, aux, and aux2",
                 gegl_node_get_operation (operation->node));
    }

  return success;
}

static gboolean gegl_operation_point_composer_process
                              (GeglOperation       *operation,
                               GeglBuffer          *input,
                               GeglBuffer          *aux,
                               GeglBuffer          *output,
                               const GeglRectangle *result,
                               gint                 level);

G_DEFINE_TYPE (GeglOperationPointComposer, gegl_operation_point_composer, GEGL_TYPE_OPERATION_COMPOSER)

static gboolean
gegl_operation_point_composer_cl_process (GeglOperation       *operation,
                                          GeglBuffer          *input,
                                          GeglBuffer          *aux,
                                          GeglBuffer          *output,
                                          const GeglRectangle *result,
                                          gint                 level)
{
  const Babl *in_format  = gegl_operation_get_format (operation, "input");
  const Babl *out_format = gegl_operation_get_format (operation, "output");

  GeglOperationClass *operation_class = GEGL_OPERATION_GET_CLASS (operation);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_GET_CLASS (operation);

  GeglBufferClIterator *iter = NULL;

  cl_int cl_err = 0;
  gboolean err;

  GEGL_NOTE (GEGL_DEBUG_OPENCL, "GEGL_OPERATION_POINT_COMPOSER: %s", operation_class->name);

  /* Process */
  iter = gegl_buffer_cl_iterator_new (output, result, out_format, GEGL_CL_BUFFER_WRITE);

  gegl_buffer_cl_iterator_add (iter, input, result, in_format, GEGL_CL_BUFFER_READ, GEGL_ABYSS_NONE);

  if (aux)
    {
      const Babl *aux_format = gegl_operation_get_format (operation, "aux");
      gegl_buffer_cl_iterator_add (iter, aux, result, aux_format, GEGL_CL_BUFFER_READ, GEGL_ABYSS_NONE);
    }

  while (gegl_buffer_cl_iterator_next (iter, &err))
    {
      if (err)
        return FALSE;

      if (point_composer_class->cl_process)
        {
          err = point_composer_class->cl_process (operation,
                                                  iter->tex[1],
                                                  (aux) ? iter->tex[2] : NULL,
                                                  iter->tex[0],
                                                  iter->size[0],
                                                  &iter->roi[0],
                                                  level);
          if (err)
            {
              GEGL_NOTE (GEGL_DEBUG_OPENCL, "Error: %s", operation_class->name);
              gegl_buffer_cl_iterator_stop (iter);
              return FALSE;
            }
        }
      else if (operation_class->cl_data)
        {
          gint p = 0;
          GeglClRunData *cl_data = operation_class->cl_data;

          cl_err = gegl_clSetKernelArg (cl_data->kernel[0], p++, sizeof(cl_mem), (void*)&iter->tex[1]);
          CL_CHECK;
          cl_err = gegl_clSetKernelArg (cl_data->kernel[0], p++, sizeof(cl_mem), (aux) ? (void*)&iter->tex[2] : NULL);
          CL_CHECK;
          cl_err = gegl_clSetKernelArg (cl_data->kernel[0], p++, sizeof(cl_mem), (void*)&iter->tex[0]);
          CL_CHECK;

          gegl_operation_cl_set_kernel_args (operation, cl_data->kernel[0], &p, &cl_err);
          CL_CHECK;

          cl_err = gegl_clEnqueueNDRangeKernel (gegl_cl_get_command_queue (),
                                                cl_data->kernel[0], 1,
                                                NULL, &iter->size[0], NULL,
                                                0, NULL, NULL);
          CL_CHECK;
        }
      else
        {
          g_warning ("OpenCL support enabled, but no way to execute");
          gegl_buffer_cl_iterator_stop (iter);
          return FALSE;
        }
    }

  return TRUE;

error:
  GEGL_NOTE (GEGL_DEBUG_OPENCL, "Error: %s", gegl_cl_errstring (cl_err));
  if (iter)
    gegl_buffer_cl_iterator_stop (iter);
  return FALSE;
}

static void prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");
  const Babl *format;
  if (!space)
    space = gegl_operation_get_source_space (operation, "aux");

  format  = babl_format_with_space ("RGBA float", space);

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "aux", format);
  gegl_operation_set_format (operation, "output", format);
}

static void
gegl_operation_point_composer_class_init (GeglOperationPointComposerClass *klass)
{
  GeglOperationClass          *operation_class = GEGL_OPERATION_CLASS (klass);
  GeglOperationComposerClass *composer_class  = GEGL_OPERATION_COMPOSER_CLASS (klass);

  composer_class->process = gegl_operation_point_composer_process;
  operation_class->process = gegl_operation_composer_process;
  operation_class->prepare = prepare;
  operation_class->no_cache =TRUE;
  operation_class->want_in_place = TRUE;
  operation_class->threaded = TRUE;
}

static void
gegl_operation_point_composer_init (GeglOperationPointComposer *self)
{

}

static gboolean
gegl_operation_point_composer_process (GeglOperation       *operation,
                                       GeglBuffer          *input,
                                       GeglBuffer          *aux,
                                       GeglBuffer          *output,
                                       const GeglRectangle *result,
                                       gint                 level)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_GET_CLASS (operation);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_GET_CLASS (operation);
  const Babl *in_format   = gegl_operation_get_format (operation, "input");
  const Babl *aux_format  = gegl_operation_get_format (operation, "aux");
  const Babl *out_format  = gegl_operation_get_format (operation, "output");


  if ((result->width > 0) && (result->height > 0))
    {
      if (gegl_operation_use_opencl (operation) && (operation_class->cl_data || point_composer_class->cl_process))
        {
          if (gegl_operation_point_composer_cl_process (operation, input, aux, output, result, level))
              return TRUE;
        }

      if (gegl_operation_use_threading (operation, result) && result->height > 1)
      {
        gint threads = gegl_config_threads ();
        ThreadData thread_data[GEGL_MAX_THREADS];
        GThreadPool *pool = thread_pool ();
        gint pending;
        gint j;

        if (result->width > result->height)
          for (j = 0; j < threads; j++)
          {
            GeglRectangle rect = *result;
            rect.width /= threads;
            rect.x += rect.width * j;

            if (j == threads-1)
              rect.width = (result->width + result->x) - rect.x;

            thread_data[j].result = rect;
          }
        else
          for (j = 0; j < threads; j++)
          {
            GeglRectangle rect = *result;
            rect = *result;
            rect.height /= threads;
            rect.y += rect.height * j;

            if (j == threads-1)
              rect.height = (result->height + result->y) - rect.y;

            thread_data[j].result = rect;
          }

        for (j = 0; j < threads; j++)
        {
          thread_data[j].klass = point_composer_class;
          thread_data[j].operation = operation;
          thread_data[j].input = input;
          thread_data[j].aux = aux;
          thread_data[j].output = output;
          thread_data[j].pending = &pending;
          thread_data[j].level = level;
          thread_data[j].input_format = in_format;
          thread_data[j].aux_format = aux_format;
          thread_data[j].output_format = out_format;
        }
        pending = threads;

        if (gegl_cl_is_accelerated ())
        {
          if (input)
            gegl_buffer_flush_ext (input, result);
          if (aux)
            gegl_buffer_flush_ext (aux, result);
        }

        for (gint j = 1; j < threads; j++)
          g_thread_pool_push (pool, &thread_data[j], NULL);

        thread_process (&thread_data[0], NULL);
        while (g_atomic_int_get (&pending)) {};

        return TRUE;
      }
      else
      {
        GeglBufferIterator *i = gegl_buffer_iterator_new (output, result, level, out_format, GEGL_ACCESS_WRITE, GEGL_ABYSS_NONE, 3);
        gint foo = 0, read = 0;

        if (input)
          read = gegl_buffer_iterator_add (i, input, result, level, in_format, GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
        if (aux)
          foo = gegl_buffer_iterator_add (i, aux, result, level, aux_format, GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

        while (gegl_buffer_iterator_next (i))
          {
            point_composer_class->process (operation, input?i->items[read].data:NULL,
                                                      aux?i->items[foo].data:NULL,
                                                      i->items[0].data, i->length, &(i->items[0].roi), level);
          }
        return TRUE;
      }
    }
  return TRUE;
}
