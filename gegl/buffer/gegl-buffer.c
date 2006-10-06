/* This file is part of GEGL.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */

#include <glib.h>
#include <glib/gstdio.h>

#include "gegl-buffer-types.h"
#include "gegl-buffer.h"
#include "gegl-storage.h"
#include "gegl-tile-backend.h"
#include "gegl-tile-trait.h"
#include "gegl-tile.h"
#include "gegl-tile-cache.h"
#include "gegl-tile-log.h"
#include "gegl-tile-empty.h"
#include "gegl-buffer-allocator.h"
#include "gegl-types.h"
#include "gegl-utils.h"

G_DEFINE_TYPE(GeglBuffer, gegl_buffer, GEGL_TYPE_TILE_TRAITS)

static GObjectClass *parent_class = NULL;

enum {
  PROP_0,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_SHIFT_X,
  PROP_SHIFT_Y,
  PROP_ABYSS_X,
  PROP_ABYSS_Y,
  PROP_ABYSS_WIDTH,
  PROP_ABYSS_HEIGHT,
  PROP_FORMAT
};

/* compute the tile indice of a coordinate
 * the stride is the width/height of tiles along the axis of coordinate
 */
static inline gint
tile_indice (gint coordinate,
             gint stride)
{
  if (coordinate>=0)
    return coordinate/stride;
  return (((coordinate+1)/stride)-1);
}

/* computes the positive integer remainder (also for negative dividends)
 */
#define REMAINDER(dividend, divisor)                       \
   ((((dividend) < 0) ?                                    \
   ((divisor) - ((-(dividend)) % (divisor))) % (divisor) : \
   ((dividend) % (divisor))))

/* compute the offset into the containing tile a coordinate has,
 * the stride is the width/height of tiles along the axis of coordinate
 */
static inline gint
tile_offset (gint coordinate,
             gint stride)
{
  return REMAINDER (coordinate,stride);
}

static inline gint needed_tiles (gint w,
                                 gint stride)
{
  return ((w-1)/stride)+1;
}

static inline gint needed_width (gint w,
                                 gint stride)
{
  return needed_tiles (w, stride)*stride;
}

static void
get_property (GObject    *gobject,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GeglBuffer *buffer = GEGL_BUFFER (gobject);
  switch(property_id)
    {
      case PROP_WIDTH:
        g_value_set_int (value, buffer->width);
        break;
      case PROP_HEIGHT:
        g_value_set_int (value, buffer->height);
        break;
      case PROP_FORMAT:
        /* might already be set the first time, if it was set during
         * construction
         */

        if (buffer->format == NULL)
          buffer->format = gegl_buffer_get_format (buffer);

        g_value_set_pointer (value, buffer->format);
        break;
      case PROP_X:
        g_value_set_int (value, buffer->x);
        break;
      case PROP_Y:
        g_value_set_int (value, buffer->y);
        break;
      case PROP_SHIFT_X:
        g_value_set_int (value, buffer->shift_x);
        break;
      case PROP_SHIFT_Y:
        g_value_set_int (value, buffer->shift_y);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

static void
set_property (GObject      *gobject,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GeglBuffer *buffer= GEGL_BUFFER (gobject);
  switch(property_id)
    {
      case PROP_X:
        buffer->x = g_value_get_int (value);
        break;
      case PROP_Y:
        buffer->y = g_value_get_int (value);
        break;
      case PROP_WIDTH:
        buffer->width = g_value_get_int (value);
        break;
      case PROP_HEIGHT:
        buffer->height = g_value_get_int (value);
        break;

      case PROP_SHIFT_X:
        buffer->shift_x = g_value_get_int (value);
        break;
      case PROP_SHIFT_Y:
        buffer->shift_y = g_value_get_int (value);
        break;
      case PROP_ABYSS_X:
        buffer->abyss_x = g_value_get_int (value);
        break;
      case PROP_ABYSS_Y:
        buffer->abyss_y = g_value_get_int (value);
        break;
      case PROP_ABYSS_WIDTH:
        buffer->abyss_width = g_value_get_int (value);
        break;
      case PROP_ABYSS_HEIGHT:
        buffer->abyss_height = g_value_get_int (value);
        break;
      case PROP_FORMAT:
        buffer->format = g_value_get_pointer (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

static gint allocated_buffers = 0;
static gint de_allocated_buffers = 0;

void gegl_buffer_stats (void)
{
  g_warning ("Buffer statistics: allocated:%i deallocated:%i balance:%i",
            allocated_buffers, de_allocated_buffers, allocated_buffers-de_allocated_buffers);
}

gint gegl_buffer_leaks (void)
{
  return allocated_buffers-de_allocated_buffers;
}

#include "gegl-buffer-allocator.h"

#include <string.h>

static void gegl_buffer_void (GeglBuffer *buffer);

static void
gegl_buffer_dispose (GObject *object)
{
  GeglBuffer *buffer;
  GeglTileTrait *trait;

  buffer = (GeglBuffer*) object;
  trait = GEGL_TILE_TRAIT (object);

  if (trait->source &&
      GEGL_IS_BUFFER_ALLOCATOR (trait->source))
    {
      gegl_buffer_void (buffer);
#if 0
      trait->source = NULL; /* this might be a dangerous way of marking that we have already voided */
#endif
    }

  de_allocated_buffers++;
  (* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static GeglTileBackend *
gegl_buffer_backend (GeglBuffer *buffer)
{
    GeglTileStore *tmp = GEGL_TILE_STORE (buffer);

    if (!tmp)
      return NULL;

    do {
      tmp = GEGL_TILE_TRAIT (tmp)->source;
    } while (tmp &&
             /*GEGL_IS_TILE_TRAIT (tmp) &&*/
             !GEGL_IS_TILE_BACKEND (tmp));
    if (!tmp &&
        !GEGL_IS_TILE_BACKEND (tmp))
      return NULL;

    return (GeglTileBackend *)tmp;
}

GeglStorage *
gegl_buffer_storage (GeglBuffer *buffer)
{
    GeglTileStore *tmp = GEGL_TILE_STORE (buffer);
    do {
      tmp = GEGL_TILE_TRAIT (tmp)->source;
    } while (!GEGL_IS_STORAGE (tmp));

    return (GeglStorage *)tmp;
}

void babl_backtrack (void);

static GObject *
gegl_buffer_constructor (GType                  type,
                         guint                  n_params,
                         GObjectConstructParam *params)
{
  GObject         *object;
  GeglBuffer      *buffer;
  GeglTileTraits  *traits;
  GeglTileBackend *backend;
  GeglTileTrait   *trait;
  GeglTileStore   *source;
  gint             tile_width;
  gint             tile_height;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  buffer  = GEGL_BUFFER (object);
  traits  = GEGL_TILE_TRAITS (object);
  trait   = GEGL_TILE_TRAIT (object);
  source  = trait->source;
  backend = gegl_buffer_backend (buffer);

  if (source)
    {
      if (GEGL_IS_STORAGE (source))
        buffer->format = GEGL_STORAGE (source)->format;
      else if (GEGL_IS_BUFFER (source))
        buffer->format = GEGL_BUFFER (source)->format;
    }

  if (!source)
    {
      /* if no source is specified if a format is specified, we
       * we need to create our own
       * source (this adds a redirectin buffer in between for
       * all "allocated from format", type buffers.
       */
      g_assert (buffer->format);

      source = GEGL_TILE_STORE (gegl_buffer_new_from_format (buffer->format,
                                                             buffer->x,
                                                             buffer->y,
                                                             buffer->width,
                                                             buffer->height));
      /* after construction,. x and y should be set to reflect
       * the top level behavior exhibited by this buffer object.
       */
      g_object_set (buffer,
                    "source", source,
                    NULL);
      g_object_unref (source);

      g_assert (source);
      backend = gegl_buffer_backend (GEGL_BUFFER (source));
      g_assert (backend);
    }

  g_assert (backend);

  tile_width = backend->tile_width;
  tile_height = backend->tile_height;

  if (buffer->width == -1 &&
      buffer->height == -1) /* no specified extents, inheriting from source */
    {
      if (GEGL_IS_BUFFER (source))
        {
          buffer->width = GEGL_BUFFER (source)->width;
          buffer->height = GEGL_BUFFER (source)->height;
        }
      else if (GEGL_IS_STORAGE (source))
        {
          buffer->width = GEGL_STORAGE (source)->width;
          buffer->height = GEGL_STORAGE (source)->height;
        }
    }

  if (buffer->abyss_width  == 0 &&
      buffer->abyss_height == 0 &&
      buffer->abyss_x      == 0 &&
      buffer->abyss_y      == 0) /* 0 sized extent == inherit buffer extent
                                  */
    {
      buffer->abyss_x = buffer->x;
      buffer->abyss_y = buffer->y;
      buffer->abyss_width = buffer->width;
      buffer->abyss_height = buffer->height;
    }
  else if (buffer->abyss_width == 0 &&
           buffer->abyss_height == 0)
    {
      g_warning ("peculiar abyss dimensions: %i,%i %ix%i",
       buffer->abyss_x,
       buffer->abyss_y,
       buffer->abyss_width,
       buffer->abyss_height);
    }
  else if (buffer->abyss_width == -1 ||
           buffer->abyss_height == -1)
    {
       buffer->abyss_x = GEGL_BUFFER (source)->abyss_x - buffer->shift_x;
       buffer->abyss_y = GEGL_BUFFER (source)->abyss_y - buffer->shift_y;
       buffer->abyss_width = GEGL_BUFFER (source)->abyss_width;
       buffer->abyss_height = GEGL_BUFFER (source)->abyss_height;
    }

   /* intersect our own abyss with parent's abyss if it exists
    */
   if (GEGL_IS_BUFFER (source))
     {
       GeglRect parent = {
         GEGL_BUFFER (source)->abyss_x - buffer->shift_x,
         GEGL_BUFFER (source)->abyss_y - buffer->shift_y,
         GEGL_BUFFER (source)->abyss_width,
         GEGL_BUFFER (source)->abyss_height};
       GeglRect request = {
         buffer->abyss_x,
         buffer->abyss_y,
         buffer->abyss_width,
         buffer->abyss_height
       };
       GeglRect self;
       gegl_rect_intersect (&self, &parent, &request);

       buffer->abyss_x = self.x;
       buffer->abyss_y = self.y;
       buffer->abyss_width = self.w;
       buffer->abyss_height = self.h;
     }

  /* compute our own total shift */
  if (GEGL_IS_BUFFER (source))
    {
      GeglBuffer *source_buf;

      source_buf = GEGL_BUFFER (source);

      buffer->total_shift_x = source_buf->total_shift_x;
      buffer->total_shift_y = source_buf->total_shift_y;
    }
  else
    {
      buffer->total_shift_x = 0;
      buffer->total_shift_y = 0;
    }
  buffer->total_shift_x += buffer->shift_x;
  buffer->total_shift_y += buffer->shift_y;

  if(0)gegl_tile_traits_add (traits, g_object_new (GEGL_TYPE_TILE_EMPTY,
                                              "backend", backend,
                                              NULL));

 /* add a full width (should probably add height as well) sized cache for
  * this gegl_buffer only if the width is < 16384. The huge buffers used
  * by the allocator are thus not affected, but all other buffers should
  * have a enough tiles to be scanline iteratable.
  */

  if (0 && buffer->width < 1<<14)
    gegl_tile_traits_add (traits, g_object_new (GEGL_TYPE_TILE_CACHE,
                                                "size",
                                                needed_tiles (buffer->width, tile_width)+1,
                                                NULL));

  return object;
}

static GeglTile *
get_tile (GeglTileStore *tile_store,
          gint           x,
          gint           y,
          gint           z)
{
  GeglTileTraits *traits = GEGL_TILE_TRAITS (tile_store);
  GeglTileStore  *source = GEGL_TILE_TRAIT (tile_store)->source;
  GeglTile       *tile   = NULL;

  if (traits->chain!=NULL)
    tile = gegl_tile_store_get_tile (GEGL_TILE_STORE (traits->chain->data),
                                     x, y, z);
  else if (source)
    tile = gegl_tile_store_get_tile (source, x, y, z);
  else
    g_assert (0);

  if (tile)
    {
      tile->x = x;
      tile->y = y;
      tile->z = z;
      tile->buffer  = GEGL_BUFFER (tile_store);

      /* storing information in tile, to enable the dispose
       * function of the tile instance to "hook" back to the storage with correct coordinates.
       *
       * this used to involve the shifting that had occured through TILE_OFFSETS, the shift
       * handling has been moved to the buffer level of abstraction instead to allow pixel
       * precise shifts.
       */
      {
        tile->storage = gegl_buffer_storage (GEGL_BUFFER (tile_store));
        tile->storage_x = x;
        tile->storage_y = y;
        tile->storage_z = z;
      }
    }

  return tile;
}


#include "values.h"

static void
gegl_buffer_class_init (GeglBufferClass *class)
{
  GObjectClass       *gobject_class;
  GeglTileStoreClass *tile_store_class;
  gobject_class = (GObjectClass*) class;
  tile_store_class = (GeglTileStoreClass*) class;

  parent_class = g_type_class_peek_parent (class);
  gobject_class->dispose = gegl_buffer_dispose;
  gobject_class->constructor = gegl_buffer_constructor;
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;
  tile_store_class->get_tile = get_tile;

  g_object_class_install_property (gobject_class, PROP_WIDTH,
                                   g_param_spec_int ("width", "width", "pixel width of buffer",
                                                     -1, G_MAXINT, -1,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
                                   g_param_spec_int ("height", "height", "pixel height of buffer",
                                                     -1, G_MAXINT, -1,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_X,
                                   g_param_spec_int ("x", "x", "local origin's offset relative to source origin",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_Y,
                                   g_param_spec_int ("y", "y", "local origin's offset relative to source origin",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_ABYSS_WIDTH,
                                   g_param_spec_int ("abyss-width", "abyss-width", "pixel width of abyss",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_ABYSS_HEIGHT,
                                   g_param_spec_int ("abyss-height", "abyss-height", "pixel height of abyss",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_ABYSS_X,
                                   g_param_spec_int ("abyss-x", "abyss-x", "",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_ABYSS_Y,
                                   g_param_spec_int ("abyss-y", "abyss-y", "",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_SHIFT_X,
                                   g_param_spec_int ("shift-x", "shift-x", "",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_SHIFT_Y,
                                   g_param_spec_int ("shift-y", "shift-y", "",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_FORMAT,
                                   g_param_spec_pointer ("format", "format", "babl format",
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_CONSTRUCT));
}

static void
gegl_buffer_init (GeglBuffer *buffer)
{
  buffer->x=0;
  buffer->y=0;
  buffer->width = 0;
  buffer->height = 0;
  buffer->shift_x = 0;
  buffer->shift_y = 0;
  buffer->total_shift_x = 0;
  buffer->total_shift_y = 0;
  buffer->abyss_x = 0;
  buffer->abyss_y = 0;
  buffer->abyss_width = 0;
  buffer->abyss_height = 0;
  buffer->format = NULL;
  allocated_buffers++;
}

gboolean
gegl_buffer_void_tile (GeglBuffer *buffer,
                       gint        x,
                       gint        y)
{
  gint z=0;
  return gegl_tile_store_message (GEGL_TILE_STORE (buffer),
                                  GEGL_TILE_VOID, x, y, z, NULL);
}

/* returns TRUE if something was done */
gboolean
gegl_buffer_idle (GeglBuffer *buffer)
{
  return gegl_tile_store_message (GEGL_TILE_STORE (buffer),
                                  GEGL_TILE_IDLE, 0, 0, 0, NULL);
}

/***************************************************************************/

void *gegl_buffer_get_format      (GeglBuffer *buffer)
{
  g_assert (buffer);
  if (buffer->format != NULL)
    return buffer->format;
  return gegl_buffer_backend (buffer)->format;
}

void
gegl_buffer_add_dirty (GeglBuffer *buffer,
                       gint        x,
                       gint        y)
{
  gint z=0;
  gegl_tile_store_message (GEGL_TILE_STORE (buffer),
                           GEGL_TILE_DIRTY, x, y, z, NULL);
}

void
gegl_buffer_flush_dirty (GeglBuffer *buffer)
{
  gegl_tile_store_message (GEGL_TILE_STORE (buffer),
                           GEGL_TILE_FLUSH_DIRTY, 0, 0, 0, NULL);
}

gboolean
gegl_buffer_is_dirty (GeglBuffer *buffer,
                      gint        x,
                      gint        y)
{
  gint z=0;
  return gegl_tile_store_message (GEGL_TILE_STORE (buffer),
                                  GEGL_TILE_IS_DIRTY, x, y, z, NULL);
}

gint gegl_buffer_pixels (GeglBuffer *buffer)
{
  return buffer->width * buffer->height;
}

gint gegl_buffer_px_size (GeglBuffer *buffer)
{
  return gegl_buffer_storage (buffer)->px_size;
}


static void
gegl_buffer_void (GeglBuffer *buffer)
{
  gint width       = buffer->width;
  gint height      = buffer->height;
  gint tile_width  = gegl_buffer_storage (buffer)->tile_width;
  gint tile_height = gegl_buffer_storage (buffer)->tile_height;
  gint bufy        = 0;


  while (bufy < height)
    {
      gint tiledy  = buffer->y + buffer->total_shift_y + bufy;
      gint offsety = tile_offset (tiledy, tile_height);
      gint bufx    = 0;

      while (bufx < width)
        {
          gint      tiledx  = buffer->x + bufx + buffer->total_shift_x;
          gint      offsetx = tile_offset (tiledx, tile_width);

          gegl_tile_store_message (GEGL_TILE_STORE (buffer),
                                   GEGL_TILE_VOID,
                                   tile_indice(tiledx,tile_width), tile_indice(tiledy,tile_height),0,
                                   NULL);
          bufx += (tile_width - offsetx);
        }
      bufy += (tile_height - offsety);
    }
}


/*
 * babl conversion should probably be done on a tile by tile, or even scanline by
 * scanline basis instead of allocating large temporary buffers. (using babl for "memcpy")
 */

#include <string.h>

#ifdef BABL
#undef BABL
#endif

#define BABL(o)     ((Babl*)(o))

#ifdef FMTPXS
#undef FMTPXS
#endif
#define FMTPXS(fmt) (BABL(fmt)->format.bytes_per_pixel)

static void inline
gegl_buffer_iterate_fmt (GeglBuffer *buffer,
                         guchar     *buf,
                         gboolean    write,
                         BablFormat *format)
{
  gint width         = buffer->width;
  gint height        = buffer->height;
  gint tile_width    = gegl_buffer_storage (buffer)->tile_width;
  gint tile_height   = gegl_buffer_storage (buffer)->tile_height;
  gint abyss_x_total = buffer->abyss_x + buffer->abyss_width;
  gint abyss_y_total = buffer->abyss_y + buffer->abyss_height;
  gint px_size       = gegl_buffer_px_size (buffer);
  gint bpx_size      = FMTPXS (format);
  gint tile_stride   = px_size * tile_width;
  gint buf_stride    = width * bpx_size;
  gint bufy          = 0;
  Babl *fish;

  if (format == buffer->format)
    {
      fish = NULL;
    }
  else
    {
      if (write)
        {
          fish = babl_fish (format, buffer->format);
        }
      else
        {
          fish = babl_fish (buffer->format, format);
        }
    }

  while (bufy < height)
    {
      gint tiledy  = buffer->y + buffer->total_shift_y + bufy;
      gint offsety = tile_offset (tiledy, tile_height);
      gint bufx    = 0;

      if (!(buffer->y + bufy + (tile_height) >= buffer->abyss_y &&
            buffer->y + bufy                  < abyss_y_total))
        { /* entire row of tiles is in abyss */
          if (!write)
            {
              gint row;
              gint y = bufy;
              guchar *bp = buf + ((bufy) * width) * bpx_size;

              for (row = offsety;
                   row < tile_height && y < height;
                   row++, y++)
                {
                  memset (bp, 0x00, buf_stride);
                  bp += buf_stride;
                }
            }
        }
      else

      while (bufx < width)
        {
          gint    tiledx  = buffer->x + bufx + buffer->total_shift_x;
          gint    offsetx = tile_offset (tiledx, tile_width);
          gint    pixels;
          guchar *bp;

          bp = buf + (bufy * width + bufx) * bpx_size;

          if (width + offsetx - bufx < tile_width)
            pixels = (width + offsetx - bufx) - offsetx;
          else
            pixels = tile_width - offsetx;

          if (!(buffer->x + bufx + tile_width >= buffer->abyss_x &&
                buffer->x + bufx              <  abyss_x_total))
            { /* entire tile is in abyss */
              if (!write)
                {
                  gint row;
                  gint y = bufy;

                  for (row = offsety;
                       row < tile_height && y < height;
                       row++, y++)
                    {
                      memset (bp, 0x00, pixels * bpx_size);
                      bp += buf_stride;
                    }
                }
            }
          else
            {
              guchar   *tile_base, *tp;
              GeglTile *tile = gegl_tile_store_get_tile (GEGL_TILE_STORE (buffer),
                                  tile_indice(tiledx,tile_width), tile_indice(tiledy,tile_height),
                                  0);
              if (!tile)
                {
                  g_warning ("didn't get tile, trying to continue");
                  bufx += (tile_width - offsetx);
                  continue;
                }

              if (write)
                gegl_tile_lock (tile);

              tile_base = gegl_tile_get_data (tile);
              tp = ((guchar*)tile_base) + (offsety * tile_width + offsetx) * px_size;

              if (write)
                {
                  gint row;
                  gint y = bufy;

                  if (fish)
                    {
                      for (row = offsety;
                           row < tile_height &&
                           y < height &&
                           buffer->y + y < abyss_y_total
                           
                           ;
                           row++, y++)
                        {
                          if (buffer->y + y >= buffer->abyss_y &&
                              buffer->y + y <  abyss_y_total)
                            babl_process (fish, bp, tp, pixels);

                          tp += tile_stride;
                          bp += buf_stride;
                        }
                    }
                  else
                    {
                      for (row = offsety;
                           row < tile_height && y < height;
                           row++, y++)
                        {
                          if (buffer->y + y >= buffer->abyss_y &&
                              buffer->y + y <  abyss_y_total)
                            memcpy (tp, bp, pixels * px_size);

                          tp += tile_stride;
                          bp += buf_stride;
                        }
                    }
                  gegl_tile_unlock (tile);
                }
              else
                {
                  gint row;
                  gint y = bufy;

                  for (row = offsety;
                       row < tile_height && y < height;
                       row++, y++)
                    {

                      if (buffer->y + y >= buffer->abyss_y &&
                          buffer->y + y <  abyss_y_total)
                        {
                          if (fish)
                            babl_process (fish, tp, bp, pixels);
                          else
                            memcpy (bp, tp, pixels * px_size);
                        }
                      else 
                        {
                          /* entire row in abyss */
                          memset (bp, 0x00, pixels * bpx_size);
                        }

                        {
                          gint zeros = (buffer->abyss_x) - (buffer->x + bufx);

                          /* left hand zeroing of abyss in tile */
                          if (zeros > 0 && zeros < pixels)
                            {
                              memset (bp, 0x00, bpx_size * zeros);
                            }

                          /* right side zeroing of abyss in tile */
                          zeros = (buffer->x + bufx + pixels) - abyss_x_total;
                          if (zeros > 0 && zeros < pixels)
                            {
                              memset (bp + (pixels-zeros)*bpx_size, 0x00, bpx_size * zeros);
                            }
                        }

                      tp += tile_stride;
                      bp += buf_stride;
                    }
                }
              g_object_unref (G_OBJECT (tile));
            }
          bufx += (tile_width - offsetx);
        }
      bufy += (tile_height - offsety);
    }
}


void
gegl_buffer_set_fmt (GeglBuffer *buffer,
                     void       *src,
                     void       *format)
{
  gboolean write;
  gegl_buffer_iterate_fmt (buffer, src, write = TRUE, format);
}

void
gegl_buffer_get_fmt (GeglBuffer *buffer,
                     void       *dst,
                     void       *format)
{
  gboolean write;
  gegl_buffer_iterate_fmt (buffer, dst, write = FALSE, format);
}

void
gegl_buffer_set (GeglBuffer *buffer,
                 void       *src)
{
  gegl_buffer_set_fmt (buffer, src, buffer->format);
}

void
gegl_buffer_get (GeglBuffer *buffer,
                 void       *dst)
{
  gegl_buffer_get_fmt (buffer, dst, buffer->format);
}

void          gegl_buffer_set_rect_fmt        (GeglBuffer *buffer,
                                               GeglRect   *rect,
                                               void       *src,
                                               void       *format)
{
  GeglBuffer *sub_buf = g_object_new (GEGL_TYPE_BUFFER,
                                      "source", buffer,
                                      "x", rect->x,
                                      "y", rect->y,
                                      "width", rect->w,
                                      "height", rect->h,
                                      NULL);
  gegl_buffer_set_fmt (sub_buf, src, format);
  g_object_unref (sub_buf);
}

void          gegl_buffer_get_rect_fmt        (GeglBuffer *buffer,
                                               GeglRect   *rect,
                                               void       *dst,
                                               void       *format)
{
  GeglBuffer *sub_buf = g_object_new (GEGL_TYPE_BUFFER,
                                      "source", buffer,
                                      "x", rect->x,
                                      "y", rect->y,
                                      "width", rect->w,
                                      "height", rect->h,
                                      NULL);
  gegl_buffer_get_fmt (sub_buf, dst, format);
  g_object_unref (sub_buf);
}

void          gegl_buffer_set_rect            (GeglBuffer *buffer,
                                               GeglRect   *rect,
                                               void       *src)
{
  gegl_buffer_set_rect_fmt (buffer, rect, src, buffer->format);
}

void          gegl_buffer_get_rect            (GeglBuffer *buffer,
                                               GeglRect   *rect,
                                               void       *dst)
{
  gegl_buffer_get_rect_fmt (buffer, rect, dst, buffer->format);
}
