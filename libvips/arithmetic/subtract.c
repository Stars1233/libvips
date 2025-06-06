/* Subtract two images
 *
 * Copyright: 1990, N. Dessipris.
 *
 * Author: Nicos Dessipris
 * Written on: 02/05/1990
 * Modified on:
 * 29/4/93 J.Cupitt
 *	- now works for partial images
 * 1/7/93 JC
 * 	- adapted for partial v2
 * 9/5/95 JC
 *	- simplified: now just handles 10 cases (instead of 50), using
 *	  im_clip2*() to help
 *	- now uses im_wrapmany() rather than im_generate()
 * 12/6/95 JC
 * 	- new im_add() adapted to make this
 * 31/5/96 JC
 *	- what was this SWAP() stuff? failed for small - big!
 * 22/8/03 JC
 *	- cast up more quickly to help accuracy
 * 27/9/04
 *	- updated for 1 band $op n band image -> n band image case
 * 8/12/06
 * 	- add liboil support
 * 18/8/08
 * 	- revise upcasting system
 * 	- add gtkdoc comments
 * 	- remove separate complex case, just double size
 * 31/7/10
 * 	- remove liboil
 * 23/8/11
 * 	- rewrite as a class from add.c
 */

/*

	Copyright (C) 1991-2005 The National Gallery

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
	02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <glib/gi18n-lib.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <vips/vips.h>

#include "binary.h"

typedef VipsBinary VipsSubtract;
typedef VipsBinaryClass VipsSubtractClass;

G_DEFINE_TYPE(VipsSubtract, vips_subtract, VIPS_TYPE_BINARY);

#define LOOP(IN, OUT) \
	{ \
		IN *restrict left = (IN *) in[0]; \
		IN *restrict right = (IN *) in[1]; \
		OUT *restrict q = (OUT *) out; \
\
		for (x = 0; x < sz; x++) \
			q[x] = left[x] - right[x]; \
	}

static void
vips_subtract_buffer(VipsArithmetic *arithmetic,
	VipsPel *out, VipsPel **in, int width)
{
	VipsImage *im = arithmetic->ready[0];

	/* Complex just doubles the size.
	 */
	const int sz = width * vips_image_get_bands(im) *
		(vips_band_format_iscomplex(vips_image_get_format(im)) ? 2 : 1);

	int x;

	/* Keep types here in sync with bandfmt_subtract[]
	 * below.
	 */
	switch (vips_image_get_format(im)) {
	case VIPS_FORMAT_CHAR:
		LOOP(signed char, signed short);
		break;
	case VIPS_FORMAT_UCHAR:
		LOOP(unsigned char, signed short);
		break;
	case VIPS_FORMAT_SHORT:
		LOOP(signed short, signed int);
		break;
	case VIPS_FORMAT_USHORT:
		LOOP(unsigned short, signed int);
		break;
	case VIPS_FORMAT_INT:
		LOOP(signed int, signed int);
		break;
	case VIPS_FORMAT_UINT:
		LOOP(unsigned int, signed int);
		break;

	case VIPS_FORMAT_FLOAT:
	case VIPS_FORMAT_COMPLEX:
		LOOP(float, float);
		break;

	case VIPS_FORMAT_DOUBLE:
	case VIPS_FORMAT_DPCOMPLEX:
		LOOP(double, double);
		break;

	default:
		g_assert_not_reached();
	}
}

/* Save a bit of typing.
 */
#define UC VIPS_FORMAT_UCHAR
#define C VIPS_FORMAT_CHAR
#define US VIPS_FORMAT_USHORT
#define S VIPS_FORMAT_SHORT
#define UI VIPS_FORMAT_UINT
#define I VIPS_FORMAT_INT
#define F VIPS_FORMAT_FLOAT
#define X VIPS_FORMAT_COMPLEX
#define D VIPS_FORMAT_DOUBLE
#define DX VIPS_FORMAT_DPCOMPLEX

/* Type promotion for subtraction. Sign and value preserving. Make sure these
 * match the case statement in vips_subtract_buffer() above.
 */
static const VipsBandFormat vips_subtract_format_table[10] = {
	/* Band format:  UC C  US S  UI I  F  X  D  DX */
	/* Promotion: */ S, S, I, I, I, I, F, X, D, DX
};

static void
vips_subtract_class_init(VipsSubtractClass *class)
{
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsArithmeticClass *aclass = VIPS_ARITHMETIC_CLASS(class);

	object_class->nickname = "subtract";
	object_class->description = _("subtract two images");

	aclass->process_line = vips_subtract_buffer;

	vips_arithmetic_set_format_table(aclass, vips_subtract_format_table);
}

static void
vips_subtract_init(VipsSubtract *subtract)
{
}

/**
 * vips_subtract: (method)
 * @in1: input image
 * @in2: input image
 * @out: (out): output image
 * @...: `NULL`-terminated list of optional named arguments
 *
 * This operation calculates @in1 - @in2 and writes the result to @out.
 *
 * If the images differ in size, the smaller image is enlarged to match the
 * larger by adding zero pixels along the bottom and right.
 *
 * If the number of bands differs, one of the images
 * must have one band. In this case, an n-band image is formed from the
 * one-band image by joining n copies of the one-band image together, and then
 * the two n-band images are operated upon.
 *
 * The two input images are cast up to the smallest common format (see table
 * Smallest common format in
 * [arithmetic](libvips-arithmetic.html)), then the
 * following table is used to determine the output type:
 *
 * ## [method@Image.subtract] type promotion
 *
 * | input type     | output type    |
 * |----------------|----------------|
 * | uchar          | short          |
 * | char           | short          |
 * | ushort         | int            |
 * | short          | int            |
 * | uint           | int            |
 * | int            | int            |
 * | float          | float          |
 * | double         | double         |
 * | complex        | complex        |
 * | double complex | double complex |
 *
 * In other words, the output type is just large enough to hold the whole
 * range of possible values.
 *
 * ::: seealso
 *     [method@Image.add], [method@Image.linear].
 *
 * Returns: 0 on success, -1 on error
 */
int
vips_subtract(VipsImage *left, VipsImage *right, VipsImage **out, ...)
{
	va_list ap;
	int result;

	va_start(ap, out);
	result = vips_call_split("subtract", ap, left, right, out);
	va_end(ap);

	return result;
}
