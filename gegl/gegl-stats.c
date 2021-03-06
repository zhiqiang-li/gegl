/* This file is part of GEGL.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2017 Ell
 */

#include "config.h"

#include <glib-object.h>

#include "gegl.h"
#include "gegl-types-internal.h"
#include "buffer/gegl-buffer-types.h"
#include "buffer/gegl-tile-handler-cache.h"
#include "buffer/gegl-tile-handler-zoom.h"
#include "buffer/gegl-tile-backend-swap.h"
#include "gegl-stats.h"


enum
{
  PROP_0,
  PROP_TILE_CACHE_TOTAL,
  PROP_TILE_CACHE_TOTAL_MAX,
  PROP_TILE_CACHE_TOTAL_UNCLONED,
  PROP_TILE_CACHE_HITS,
  PROP_TILE_CACHE_MISSES,
  PROP_SWAP_TOTAL,
  PROP_SWAP_TOTAL_UNCLONED,
  PROP_SWAP_FILE_SIZE,
  PROP_SWAP_BUSY,
  PROP_SWAP_QUEUED_TOTAL,
  PROP_SWAP_QUEUE_FULL,
  PROP_SWAP_QUEUE_STALLS,
  PROP_SWAP_READING,
  PROP_SWAP_READ_TOTAL,
  PROP_SWAP_WRITING,
  PROP_SWAP_WRITE_TOTAL,
  PROP_ZOOM_TOTAL
};


static void   gegl_stats_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec);
static void   gegl_stats_get_property (GObject      *object,
                                       guint         property_id,
                                       GValue       *value,
                                       GParamSpec   *pspec);


G_DEFINE_TYPE (GeglStats, gegl_stats, G_TYPE_OBJECT)


static void
gegl_stats_class_init (GeglStatsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gegl_stats_set_property;
  object_class->get_property = gegl_stats_get_property;

  g_object_class_install_property (object_class, PROP_TILE_CACHE_TOTAL,
                                   g_param_spec_uint64 ("tile-cache-total",
                                                        "Tile Cache total size",
                                                        "Total size of tile cache in bytes",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_TILE_CACHE_TOTAL_MAX,
                                   g_param_spec_uint64 ("tile-cache-total-max",
                                                        "Tile Cache maximal total size",
                                                        "Maximal total size of tile cache throughout the session in bytes",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_TILE_CACHE_TOTAL_UNCLONED,
                                   g_param_spec_uint64 ("tile-cache-total-uncloned",
                                                        "Tile Cache total uncloned size",
                                                        "Total size of tile cache if all tiles were uncloned in bytes",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_TILE_CACHE_HITS,
                                   g_param_spec_int ("tile-cache-hits",
                                                     "Tile Cache hits",
                                                     "Number of tile cache hits",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_TILE_CACHE_MISSES,
                                   g_param_spec_int ("tile-cache-misses",
                                                     "Tile Cache misses",
                                                     "Number of tile cache misses",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_SWAP_TOTAL,
                                   g_param_spec_uint64 ("swap-total",
                                                        "Swap total size",
                                                        "Total size of the data in the swap",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_SWAP_TOTAL_UNCLONED,
                                   g_param_spec_uint64 ("swap-total-uncloned",
                                                        "Swap total uncloned size",
                                                        "Total size of if the data in the swap if all the entries were uncloned in bytes",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_SWAP_FILE_SIZE,
                                   g_param_spec_uint64 ("swap-file-size",
                                                        "Swap file size",
                                                        "Size of the swap file",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_SWAP_BUSY,
                                   g_param_spec_boolean ("swap-busy",
                                                         "Swap busy",
                                                         "Whether there is work queued for the swap",
                                                         FALSE,
                                                         G_PARAM_READABLE));


  g_object_class_install_property (object_class, PROP_SWAP_QUEUED_TOTAL,
                                   g_param_spec_uint64 ("swap-queued-total",
                                                        "Swap total queued size",
                                                        "Total size of the data queued for writing to the swap",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));


  g_object_class_install_property (object_class, PROP_SWAP_QUEUE_FULL,
                                   g_param_spec_boolean ("swap-queue-full",
                                                         "Swap queue full",
                                                         "Whether the swap queue is full",
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_SWAP_QUEUE_STALLS,
                                   g_param_spec_int ("swap-queue-stalls",
                                                     "Swap queue stall count",
                                                     "Number of times writing to the swap has been stalled, "
                                                     "due to a full queue",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_SWAP_READING,
                                   g_param_spec_boolean ("swap-reading",
                                                         "Swap reading",
                                                         "Whether data is being read from the swap",
                                                         FALSE,
                                                         G_PARAM_READABLE));


  g_object_class_install_property (object_class, PROP_SWAP_READ_TOTAL,
                                   g_param_spec_uint64 ("swap-read-total",
                                                        "Swap read total",
                                                        "Total amount of data read from the swap",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_SWAP_WRITING,
                                   g_param_spec_boolean ("swap-writing",
                                                         "Swap writing",
                                                         "Whether data is being written to the swap",
                                                         FALSE,
                                                         G_PARAM_READABLE));


  g_object_class_install_property (object_class, PROP_SWAP_WRITE_TOTAL,
                                   g_param_spec_uint64 ("swap-write-total",
                                                        "Swap write total",
                                                        "Total amount of data written to the swap",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_ZOOM_TOTAL,
                                   g_param_spec_uint64 ("zoom-total",
                                                        "Zoom total",
                                                        "Total size of data processed by the zoom tile handler",
                                                        0, G_MAXUINT64, 0,
                                                        G_PARAM_READABLE));
}

static void
gegl_stats_init (GeglStats *self)
{
}

static void
gegl_stats_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gegl_stats_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_TILE_CACHE_TOTAL:
        g_value_set_uint64 (value, gegl_tile_handler_cache_get_total ());
        break;

      case PROP_TILE_CACHE_TOTAL_MAX:
        g_value_set_uint64 (value, gegl_tile_handler_cache_get_total_max ());
        break;

      case PROP_TILE_CACHE_TOTAL_UNCLONED:
        g_value_set_uint64 (value, gegl_tile_handler_cache_get_total_uncloned ());
        break;

      case PROP_TILE_CACHE_HITS:
        g_value_set_int (value, gegl_tile_handler_cache_get_hits ());
        break;

      case PROP_TILE_CACHE_MISSES:
        g_value_set_int (value, gegl_tile_handler_cache_get_misses ());
        break;

      case PROP_SWAP_TOTAL:
        g_value_set_uint64 (value, gegl_tile_backend_swap_get_total ());
        break;

      case PROP_SWAP_TOTAL_UNCLONED:
        g_value_set_uint64 (value, gegl_tile_backend_swap_get_total_uncloned ());
        break;

      case PROP_SWAP_FILE_SIZE:
        g_value_set_uint64 (value, gegl_tile_backend_swap_get_file_size ());
        break;

      case PROP_SWAP_BUSY:
        g_value_set_boolean (value, gegl_tile_backend_swap_get_busy ());
        break;

      case PROP_SWAP_QUEUED_TOTAL:
        g_value_set_uint64 (value, gegl_tile_backend_swap_get_queued_total ());
        break;

      case PROP_SWAP_QUEUE_FULL:
        g_value_set_boolean (value, gegl_tile_backend_swap_get_queue_full ());
        break;

      case PROP_SWAP_QUEUE_STALLS:
        g_value_set_int (value, gegl_tile_backend_swap_get_queue_stalls ());
        break;

      case PROP_SWAP_READING:
        g_value_set_boolean (value, gegl_tile_backend_swap_get_reading ());
        break;

      case PROP_SWAP_READ_TOTAL:
        g_value_set_uint64 (value, gegl_tile_backend_swap_get_read_total ());
        break;

      case PROP_SWAP_WRITING:
        g_value_set_boolean (value, gegl_tile_backend_swap_get_writing ());
        break;

      case PROP_SWAP_WRITE_TOTAL:
        g_value_set_uint64 (value, gegl_tile_backend_swap_get_write_total ());
        break;

      case PROP_ZOOM_TOTAL:
        g_value_set_uint64 (value, gegl_tile_handler_zoom_get_total ());
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

void
gegl_stats_reset (GeglStats *stats)
{
  gegl_tile_handler_cache_reset_stats ();
  gegl_tile_backend_swap_reset_stats ();
  gegl_tile_handler_zoom_reset_stats ();
}
