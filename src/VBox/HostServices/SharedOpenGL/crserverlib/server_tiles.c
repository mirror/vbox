/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include <stdlib.h>
#include "server.h"
#include "cr_mem.h"
#include "cr_warp.h"
#include "cr_hull.h"
#include "cr_string.h"


/*
 * Given the mural->extents[i].imagewindow parameters, compute the
 * mural->extents[i].bounds and outputwindow parameters.
 * This includes allocating space in the render SPU's window for each
 * of the tiles.
 */
static void
initializeExtents(CRMuralInfo *mural)
{
    const int leftMargin = cr_server.useL2 ? 2 : 0;
    int i;
    int x, y, w, h, maxTileHeight;

    x = leftMargin;
    y = 0;
    maxTileHeight = 0;

    /* Basically just copy the server's list of tiles to the RunQueue
     * and compute some derived tile information.
     */
    for ( i = 0; i < mural->numExtents; i++ )
    {
        CRExtent *extent = &mural->extents[i];

        CRASSERT(mural->width > 0);
        CRASSERT(mural->height > 0);

        /* extent->display = find_output_display( extent->imagewindow ); */

        /* Compute normalized tile bounds.
         * That is, x1, y1, x2, y2 will be in the range [-1, 1] where
         * x1=-1, y1=-1, x2=1, y2=1 corresponds to the whole mural.
         */
        extent->bounds.x1 = ( (GLfloat) (2*extent->imagewindow.x1) /
                mural->width - 1.0f );
        extent->bounds.y1 = ( (GLfloat) (2*extent->imagewindow.y1) /
                mural->height - 1.0f );
        extent->bounds.x2 = ( (GLfloat) (2*extent->imagewindow.x2) /
                mural->width - 1.0f );
        extent->bounds.y2 = ( (GLfloat) (2*extent->imagewindow.y2) /
                mural->height - 1.0f );

        /* Width and height of tile, in mural pixels */
        w = mural->extents[i].imagewindow.x2 - mural->extents[i].imagewindow.x1;
        h = mural->extents[i].imagewindow.y2 - mural->extents[i].imagewindow.y1;

        if (cr_server.useDMX) {
            /* Tile layout is easy!
             * Remember, the X window we're drawing into (tileosort::xsubwin)
             * is already adjusted to the right mural position.
             */
            extent->outputwindow.x1 = 0;
            extent->outputwindow.y1 = 0;
            extent->outputwindow.x2 = w;
            extent->outputwindow.y2 = h;
#if 0
            printf("OutputWindow %d, %d .. %d, %d\n",
                         extent->outputwindow.x1,
                         extent->outputwindow.y1,
                         extent->outputwindow.x2,
                         extent->outputwindow.y2);
#endif
        }
        else {
            /* Carve space out of the underlying (render SPU) window for this tile.
             */
            if ( x + w > (int) mural->underlyingDisplay[2] )
            {
                if (x == leftMargin) {
                    crWarning("No room for %dx%d tile in this server's window (%d x %d)!",
                                        w, h,
                                        mural->underlyingDisplay[2], mural->underlyingDisplay[3]);
                }
                y += maxTileHeight;
                x = leftMargin;
                maxTileHeight = 0;
            }

            /* Allocate space from window in bottom-to-top order.
             * This allows multi-level tilesort configurations to work better.
             */
            extent->outputwindow.x1 = x;
            extent->outputwindow.y1 = y;
            extent->outputwindow.x2 = x + w;
            extent->outputwindow.y2 = y + h;

            if ((unsigned int)extent->outputwindow.y2 > mural->underlyingDisplay[3])
                crWarning("No room for %dx%d tile in this server's window (%d x %d)!",
                                    w, h,
                                    mural->underlyingDisplay[2], mural->underlyingDisplay[3]);

            if (h > maxTileHeight)
                maxTileHeight = h;

            x += w + leftMargin;
        } /* useDMX */
    }
}


/*
 * This needs to be called when the tiling changes.  Compute max tile
 * height and check if optimized bucketing can be used, etc.
 */
void
crServerInitializeTiling(CRMuralInfo *mural)
{
    int i;

    /* The imagespace rect is useful in a few places (but redundant) */
    /* XXX not used anymore??? (grep) */
    mural->imagespace.x1 = 0;
    mural->imagespace.y1 = 0;
    mural->imagespace.x2 = mural->width;
    mural->imagespace.y2 = mural->height;

    /* find max tile height */
    mural->maxTileHeight = 0;
    for (i = 0; i < mural->numExtents; i++)
    {
        const int h = mural->extents[i].imagewindow.y2 -
            mural->extents[i].imagewindow.y1;

        if (h > mural->maxTileHeight)
            mural->maxTileHeight = h;
    }

    /* compute extent bounds, outputwindow, etc */
    initializeExtents(mural);

    /* optimized hash-based bucketing setup */
    if (cr_server.optimizeBucket) {
        mural->optimizeBucket = crServerInitializeBucketing(mural);
    }
    else {
        mural->optimizeBucket = GL_FALSE;
    }
}


/*
 * Change the tiling for a mural.
 * The boundaries are specified in mural space.
 * Input: muralWidth/muralHeight - new window/mural size
 *        numTiles - number of tiles
 * Input: tileBounds[0] = bounds[0].x
 *        tileBounds[1] = bounds[0].y
 *        tileBounds[2] = bounds[0].width
 *        tileBounds[3] = bounds[0].height
 *        tileBounds[4] = bounds[1].x
 *        ...
 */
void
crServerNewMuralTiling(CRMuralInfo *mural,
                                             GLint muralWidth, GLint muralHeight,
                                             GLint numTiles, const GLint *tileBounds)
{
    int i;

    CRASSERT(numTiles >= 0);

    crDebug("Reconfiguring tiles in crServerNewMuralTiling:");
    crDebug("  New mural size: %d x %d", muralWidth, muralHeight);
    for (i = 0; i < numTiles; i++)
    {
        crDebug("  Tile %d: %d, %d  %d x %d", i,
                        tileBounds[i*4], tileBounds[i*4+1],
                        tileBounds[i*4+2], tileBounds[i*4+3]);
    }

    /*
     * This section basically mimics what's done during crServerGetTileInfo()
     */
    mural->width = muralWidth;
    mural->height = muralHeight;
    mural->numExtents = numTiles;
    for (i = 0; i < numTiles; i++)
    {
        const GLint x = tileBounds[i * 4 + 0];
        const GLint y = tileBounds[i * 4 + 1];
        const GLint w = tileBounds[i * 4 + 2];
        const GLint h = tileBounds[i * 4 + 3];
        mural->extents[i].imagewindow.x1 = x;
        mural->extents[i].imagewindow.y1 = y;
        mural->extents[i].imagewindow.x2 = x + w;
        mural->extents[i].imagewindow.y2 = y + h;
    }

    crServerInitializeTiling(mural);
}


static float
absf(float a)
{
    if (a < 0)
        return -a;
    return a;
}
