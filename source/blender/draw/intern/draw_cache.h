/*
 * Copyright 2016, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_cache.h
 *  \ingroup draw
 */

#ifndef __DRAW_CACHE_H__
#define __DRAW_CACHE_H__

struct Batch;
struct Object;

void DRW_shape_cache_free(void);

/* Common Shapes */
struct Batch *DRW_cache_fullscreen_quad_get(void);
struct Batch *DRW_cache_single_vert_get(void);

/* Empties */
struct Batch *DRW_cache_plain_axes_get(void);
struct Batch *DRW_cache_single_arrow_get(struct Batch **line);
struct Batch *DRW_cache_cube_get(void);
struct Batch *DRW_cache_circle_get(void);
struct Batch *DRW_cache_empty_sphere_get(void);
struct Batch *DRW_cache_empty_cone_get(void);
struct Batch *DRW_cache_arrows_get(void);

/* Lamps */
struct Batch *DRW_cache_lamp_get(void);
struct Batch *DRW_cache_lamp_sunrays_get(void);

/* Meshes */
struct Batch *DRW_cache_wire_overlay_get(struct Object *ob);
struct Batch *DRW_cache_wire_outline_get(struct Object *ob);
struct Batch *DRW_cache_surface_get(struct Object *ob);

#endif /* __DRAW_CACHE_H__ */