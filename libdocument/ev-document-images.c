/* ev-document-images.c
 *  this file is part of evince, a gnome document_links viewer
 * 
 * Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "ev-document-images.h"

EV_DEFINE_INTERFACE (EvDocumentImages, ev_document_images, 0)

static void
ev_document_images_class_init (EvDocumentImagesIface *klass)
{
}

GList *
ev_document_images_get_image_mapping (EvDocumentImages *document_images,
				      EvPage           *page)
{
	EvDocumentImagesIface *iface = EV_DOCUMENT_IMAGES_GET_IFACE (document_images);

	return iface->get_image_mapping (document_images, page);
}

GdkPixbuf *
ev_document_images_get_image (EvDocumentImages *document_images,
			      EvImage          *image)
{
	EvDocumentImagesIface *iface = EV_DOCUMENT_IMAGES_GET_IFACE (document_images);

	return iface->get_image (document_images, image);
}
