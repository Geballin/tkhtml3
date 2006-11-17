
#include "htmllayout.h"
#include <stdio.h>
#include <stdarg.h>

/*
 *
 * The InlineContext "object" encapsulates many of the details of
 * laying out an inline-context. The internals of the InlineContext 
 * and InlineBorder datatypes are both encapsulated within this file.
 *
 * ALLOCATION AND DEALLOCATION:
 *
 *     HtmlInlineContextNew()
 *     HtmlInlineContextCleanup()
 *
 * ADD INLINE BOXES:
 *
 *     HtmlInlineContextAddBox()
 *     HtmlInlineContextAddText()
 *
 * RETRIEVE LINE BOXES:
 *
 *     HtmlInlineContextGetLineBox()
 *
 * ADD INLINE BORDERS:
 *
 *     HtmlGetInlineBorder()
 *     HtmlInlineContextPushBorder()
 *     HtmlInlineContextPopBorder()
 *
 * QUERY:
 * 
 *     HtmlInlineContextIsEmpty()
 */
static const char rcsid[] = "$Id: htmlinline.c,v 1.39 2006/11/17 06:05:58 danielk1977 Exp $";

typedef struct InlineBox InlineBox;
typedef struct InlineMetrics InlineMetrics;

/*
 * Each inline element, including those that generate replaced boxes, is
 * represented by an instance of the following structure. The root element
 * also generates an instance of this structure.
 */
struct InlineMetrics {
  int iFontTop;           /* Distance to top of font box */
  int iBaseline;          /* Distance to base-line */
  int iFontBottom;        /* Distance to bottom of font box */
  int iLogical;           /* Distance to bottom of logical box */
};

struct InlineContent {
};

/* Values for InlineBorder.eLineboxAlign */
#define LINEBOX_ALIGN_PARENT 0
#define LINEBOX_ALIGN_BOTTOM 1
#define LINEBOX_ALIGN_TOP    2

struct InlineBorder {
  MarginProperties margin;
  BoxProperties box;

  InlineMetrics metrics;      /* Vertical metrics for inline box */

  /* For structures with InlineBorder.eLineboxAlign==LINEBOX_ALIGN_PARENT,
   * iVerticalAlign stores the number of pixels between the top of the 
   * parent's logical box and this elements logical box. A positive) value
   * indicates lower down the page.
   *
   * If the value of the 'vertical-align' property is "top" or "bottom", then
   * eLineboxAlign is set to LINEBOX_ALIGN_TOP or _BOTTOM, respectively. In
   * this case iVerticalAlign is not meaningful.
   */
  int iVerticalAlign;
  int eLineboxAlign;          /* One of the LINEBOX_ALIGN_XXX values below */

  int iStartBox;              /* Leftmost inline-box */
  int iStartPixel;            /* Leftmost pixel of left margin */
  HtmlNode *pNode;            /* Document node that generated this border */

  /* Todo: isParentBlock can be replaced by (InlineBorder.pParent==0) */
  int isParentBlock;

  /* The following boolean is true if this InlineBorder structure is
   * only being used to align an inline replaced object. In this case,
   * do not draw any border or underline graphics.
   */
  int isReplaced;

  InlineBorder *pNext;        /* Pointer to parent inline border, if any */

  /* Pointer to parent inline border, if any */
  InlineBorder *pParent;
};

/*
 * This structure is used internally by the InlineContext object functions.
 * A single instance represents a single inline-box, for example a word of
 * text, a widget or an inline image.
 */
struct InlineBox {
  HtmlCanvas canvas;          /* Canvas containing box content. */
  int nSpace;                 /* Pixels of space between this and next box. */
  int eType;                  /* One of the INLINE_XXX values below */

  InlineBorder *pBorderStart; /* List of borders that start with this box */
  HtmlNode *pNode;            /* Associated tree node */
  int nBorderEnd;             /* Number of borders that end here */
  int nLeftPixels;            /* Total left width of borders that start here */
  int nRightPixels;           /* Total right width of borders that start here */
  int nContentPixels;         /* Width of content. */

  int nAscentPixels;          /* Distance between baseline and content top */
  int nDescentPixels;         /* Distance between baseline and content bottom */
  int nEmPixels;              /* em pixels of the font, if any */
};

/* Values for InlineBox.eType */
#define INLINE_TEXT      22
#define INLINE_REPLACED  23
#define INLINE_NEWLINE   24

struct InlineContext {
    HtmlTree *pTree;        /* Pointer to owner widget */
    HtmlNode *pNode;        /* Pointer to the node that generated the context */
    int isSizeOnly;         /* Do not draw, just estimate sizes of things */

    /* The effective values of 'text-align' and 'white-space' used for this
     * inline context. The eWhite variable is a short-term solution, because
     * as of CSS2, the 'white-space' property applies to inline elements as
     * well as block.
     */
    int eTextAlign;         /* One of TEXTALIGN_LEFT, TEXTALIGN_RIGHT etc. */
    int eWhite;             /* One of WHITESPACE_PRE, WHITESPACE_NORMAL etc. */

    int iTextIndent;        /* Pixels of 'text-indent' for next line */
    int ignoreLineHeight;   /* Boolean - true to ignore lineHeight */

    int nInline;            /* Number of inline boxes in aInline */
    int nInlineAlloc;       /* Number of slots allocated in aInline */
    InlineBox *aInline;     /* Array of inline boxes. */

    int iVAlign;               /* Current vertical box offset */

    InlineBorder *pBorders;    /* Linked list of active inline-borders. */
    InlineBorder *pBoxBorders; /* Borders list for next box to be added */

    /* InlineBorder structure associated with the block that created this
     * inline layout context.
     */
    InlineBorder *pRootBorder;

    /* The current inline border is the most recently "pushed" border
     * (see PushBorder()) border that has not yet been popped (see 
     * PopBorder()). This is used to set InlineBorder.pParent for each 
     * structure.
     */
    InlineBorder *pCurrent;    /* Current inline border */
};

#define START_LOG(pLogNode) \
if (pContext->pTree->options.logcmd && !pContext->isSizeOnly &&                \
    pLogNode->iNode >= 0) {                                                    \
    Tcl_Obj *pLog = Tcl_NewObj();                                              \
    Tcl_Obj *pLogCmd = HtmlNodeCommand(pContext->pTree, pLogNode);             \
    Tcl_IncrRefCount(pLog);                                                    \
    {

#define END_LOG(zFunction) \
    }                                                                          \
    HtmlLog(pContext->pTree, "LAYOUTENGINE", "%s %s() -> %s",                  \
            Tcl_GetString(pLogCmd),                                            \
            zFunction, Tcl_GetString(pLog)                                     \
    );                                                                         \
    Tcl_DecrRefCount(pLog);                                                    \
}

static void 
oprintf(Tcl_Obj *pObj, CONST char *zFormat, ...) {
    int nBuf = 0;
    char zBuf[1024];
    va_list ap;
    va_start(ap, zFormat);
    nBuf = vsnprintf(zBuf, 1023, zFormat, ap);
    Tcl_AppendToObj(pObj, zBuf, nBuf);
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineBoxMetrics --
 *
 *     This function populates an InlineMetrics structure with the 
 *     vertical box-size metrics for the non-replaced inline element
 *     identified by pNode.
 *     
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Sets all values in *pMetrics.
 *
 *---------------------------------------------------------------------------
 */
static void
inlineBoxMetrics(pContext, pNode, pMetrics)
    InlineContext *pContext;
    HtmlNode *pNode;
    InlineMetrics *pMetrics;       /* OUT: Vertical inline-box metrics */
{
    int iLineHeight;
    int iTopLeading;
    int iBottomLeading;
    int iContentHeight;

    HtmlComputedValues *pComputed = HtmlNodeComputedValues(pNode);
    HtmlFont *pFont = pComputed->fFont;

    iLineHeight = pComputed->iLineHeight;
    if (iLineHeight == PIXELVAL_NORMAL) {
        /* A 'line-height' value of "normal" is equivalent to 1.2 */
        iLineHeight = -120;
    }

    assert(0 == (pComputed->mask & PROP_MASK_LINE_HEIGHT) || iLineHeight >= 0);
    if (iLineHeight < 0) {
        iLineHeight = -1 * INTEGER(((iLineHeight * pFont->em_pixels) / 100));
    } else if (pComputed->mask & PROP_MASK_LINE_HEIGHT) {
        iLineHeight = INTEGER(((iLineHeight * pFont->em_pixels) / 100));
    }

    iContentHeight = pFont->metrics.ascent + pFont->metrics.descent;
    iBottomLeading = (iLineHeight - iContentHeight) / 2;
    iTopLeading = (iLineHeight - iContentHeight) - iBottomLeading;

    pMetrics->iLogical = iLineHeight;
    pMetrics->iFontBottom = pMetrics->iLogical - iBottomLeading;
    pMetrics->iBaseline = pMetrics->iFontBottom - pFont->metrics.descent;
    pMetrics->iFontTop = pMetrics->iFontBottom - iContentHeight;

    /* Just for fun, log the metrics. */
    START_LOG(pNode);
        oprintf(pLog, "<table>");
        oprintf(pLog, "<tr><th colspan=2>Inline box metrics");
        oprintf(pLog, "<tr><td>iFontTop<td>%d", pMetrics->iFontTop);
        oprintf(pLog, "<tr><td>iBaseline<td>%d", pMetrics->iBaseline);
        oprintf(pLog, "<tr><td>iFontBottom<td>%d", pMetrics->iFontBottom);
        oprintf(pLog, "<tr><td>iLogical<td>%d", pMetrics->iLogical);
        oprintf(pLog, "</table>");
    END_LOG("inlineBoxMetrics()");
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextPushBorder --
 *
 *     Configure the inline-context object with an inline-border that
 *     should start before the inline-box about to be added. The
 *     inline-border object should be obtained with a call to
 *     HtmlGetInlineBorder(). 
 *
 *     If this function is called twice for the same inline-box, then the
 *     second call creates the innermost border.
 *
 *     This function is used with HtmlInlineContextPopBorder() to define the
 *     start and end of inline borders. For example, to create the
 *     following inline layout:
 *
 *             +------------ Border-1 --------+
 *             |              +-- Border-2 --+|
 *             | Inline-Box-1 | Inline-Box-2 ||
 *             |              +--------------+|
 *             +------------------------------+
 *
 *     The sequence of calls should be:
 *
 *         HtmlInlineContextPushBorder( <Border-1> )
 *         inlineContextAddInlineCanvas( <Inline-box-1> )
 *         HtmlInlineContextPushBorder( <Border-2> )
 *         inlineContextAddInlineCanvas( <Inline-box-2> )
 *         HtmlInlineContextPopBorder( <Border 2> )
 *         HtmlInlineContextPopBorder( <Border 1> )
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlInlineContextPushBorder(pContext, pBorder)
    InlineContext *pContext;
    InlineBorder *pBorder;
{
    if (pBorder) {
        HtmlNode *pNode = pBorder->pNode;
        InlineBorder *pParent;

        pBorder->pNext = pContext->pBoxBorders;
        pContext->pBoxBorders = pBorder;
        pBorder->pParent = pContext->pCurrent;
        pContext->pCurrent = pBorder;

        /* Figure out the vertical alignment. */
        pParent = pBorder->pParent;
        if (pParent) {
            HtmlComputedValues *pComputed = HtmlNodeComputedValues(pNode);
            InlineMetrics *pPM = &pParent->metrics;
            InlineMetrics *pM = &pBorder->metrics;
            int iVert = 0;

            switch (pComputed->eVerticalAlign) {

                case 0:  /* Pixel value in HtmlComputedValues.iVerticalAlign */
                    iVert = pPM->iBaseline - pM->iBaseline;
                    iVert -= pComputed->iVerticalAlign;
                    break;

                case CSS_CONST_BASELINE:
                    iVert = pPM->iBaseline - pM->iBaseline;
                    break;

                case CSS_CONST_SUB: {
                    HtmlNode *pNodeParent = HtmlNodeParent(pNode);
                    if (pNodeParent) {
                        HtmlFont *pF=HtmlNodeComputedValues(pNodeParent)->fFont;
                        iVert = pF->ex_pixels;
                    }
                    iVert += (pPM->iBaseline - pM->iBaseline);
                    break;
                }

                case CSS_CONST_SUPER: {
                    HtmlFont *pF = pComputed->fFont;
                    iVert = (pPM->iBaseline - pM->iBaseline);
                    iVert -= pF->ex_pixels;
                    break;
                }

                case CSS_CONST_TEXT_TOP:
                    iVert = pPM->iFontTop;
                    break;

                case CSS_CONST_MIDDLE: {
                    HtmlNode *pNodeParent = HtmlNodeParent(pNode);
                    iVert = pPM->iBaseline - (pM->iLogical / 2);
                    if (pNodeParent) {
                        HtmlFont *pF=HtmlNodeComputedValues(pNodeParent)->fFont;
                        iVert -= pF->ex_pixels;
                    }
                    break;
                }

                case CSS_CONST_TEXT_BOTTOM:
                    iVert = pPM->iFontBottom - pM->iLogical;
                    break;

                /* These two are unhandled as of yet. Treat as "baseline". */
                case CSS_CONST_TOP:               /* Todo. */
                case CSS_CONST_BOTTOM:            /* Todo. */
                    iVert = pPM->iBaseline - pM->iBaseline;
                    break;
            }

            pBorder->iVerticalAlign = iVert; 
            START_LOG(pBorder->pNode);
                oprintf(pLog, "Vertical offset is %d pixels\n", iVert);
            END_LOG("HtmlInlineContextPushBorder()");
        } else {
            assert(!pContext->pRootBorder);
            pContext->pRootBorder = pBorder;
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextPopBorder --
 *
 *     Configure the inline-context such that the innermost active border
 *     is closed after the inline-box most recently added is drawn.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlInlineContextPopBorder(p, pBorder)
    InlineContext *p;
    InlineBorder *pBorder;
{
    if (!pBorder) return;

    assert(pBorder == p->pCurrent);
    p->pCurrent = p->pCurrent->pParent;

    if (p->pBoxBorders) {
        /* If there are any borders in the InlineContext.pBoxBorders list,
         * then we are popping a border for a node that has no content.
         * i.e. from the markup:
         *
         *     <a href="www.google.com"></a>
         *
	 * For this case just remove an entry from
	 * InlineContext.pBoxBorders. The border will never be drawn.
         */
        InlineBorder *pBorder = p->pBoxBorders;
        p->pBoxBorders = pBorder->pNext;
        HtmlFree(pBorder);
    } else {
        if (p->nInline > 0) {
            InlineBox *pBox = &p->aInline[p->nInline-1];
            pBox->nBorderEnd++;
            pBox->nRightPixels += pBorder->box.iRight;
            pBox->nRightPixels += pBorder->margin.margin_right;
        } else {
            pBorder = p->pBorders;
            assert(pBorder);
            p->pBorders = pBorder->pNext;
            p->iVAlign -= pBorder->iVerticalAlign;
            HtmlFree(pBorder);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlGetInlineBorder --
 *
 *     This function retrieves the border, background, margin and padding
 *     properties for node pNode. If the properties still all have their
 *     default values, then NULL is returned. Otherwise an InlineBorder
 *     struct is allocated using HtmlAlloc(0, ), populated with the various
 *     property values and returned.
 *
 *     The returned struct is considered private to the inlineContextXXX()
 *     routines. The only legitimate use is to pass the pointer to
 *     HtmlInlineContextPushBorder().
 *
 * Results:
 *     NULL or allocated InlineBorder structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
InlineBorder *
HtmlGetInlineBorder(pLayout, pContext, pNode)
    LayoutContext *pLayout; 
    InlineContext *pContext; 
    HtmlNode *pNode;
{
    InlineBorder *pBorder = 0;

    pBorder = HtmlNew(InlineBorder);

    /* As long as this is not the InlineBorder structure associated with
     * the element generating the inline context (i.e. a <p> or something),
     * then retrieve the box and margin sizes.
     *
     * We don't do this for the root of the inline context, because it is
     * only supposed to contribute a 'text-decoration' (visually that is,
     * 'line-height' etc. is still important).
     */
    if (pContext->pCurrent) {
        nodeGetBoxProperties(pLayout, pNode, 0,&pBorder->box);
        nodeGetMargins(pLayout, pNode, 0, &pBorder->margin);
    } 

    /* Determine the vertical metrics of this box. */
    inlineBoxMetrics(pContext, pNode, &pBorder->metrics);

    pBorder->pNode = pNode;
    return pBorder;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextAddInlineCanvas --
 *
 *     This function is used to add inline box content to an inline
 *     context. The content is drawn by the caller into the canvas object
 *     returned by this function.
 *
 * Results:
 *     Returns a pointer to an empty html canvas to draw the content of the
 *     new inline-box to.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlCanvas * 
inlineContextAddInlineCanvas(p, eType, pNode)
    InlineContext *p;
    int eType;        /* One of INLINE_NEWLINE, INLINE_TEXT, INLINE_REPLACED */
    HtmlNode *pNode;
{
    InlineBox *pBox;
    InlineBorder *pBorder;

    p->nInline++;
    if(p->nInline > p->nInlineAlloc) {
        /* We need to grow the InlineContext.aInline array. Note that we
         * don't bother to zero the newly allocated memory. The InlineBox
         * for which the canvas is returned is zeroed below.
         */
        char *a = (char *)p->aInline;
        int nAlloc = p->nInlineAlloc + 25;
        p->aInline = (InlineBox *)HtmlRealloc(
            "InlineContext.aInline", a, nAlloc*sizeof(InlineBox)
        );
        p->nInlineAlloc = nAlloc;
    }

    pBox = &p->aInline[p->nInline - 1];
    memset(pBox, 0, sizeof(InlineBox));
    pBox->pBorderStart = p->pBoxBorders;
    for (pBorder = pBox->pBorderStart; pBorder; pBorder = pBorder->pNext) {
        pBox->nLeftPixels += pBorder->box.iLeft;
        pBox->nLeftPixels += pBorder->margin.margin_left;
    }
    p->pBoxBorders = 0;
    /* pBox->eReplaced = eReplaced; */
    pBox->eType = eType;
    pBox->pNode = pNode;
    return &pBox->canvas;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextSetBoxDimensions --
 *
 *     This is used to set the effective size of the inline-box most
 *     recently added to this inline-context via AddInlineCanvas().
 *
 *     Inline-box dimensions are specified as three quantities, all in
 *     pixel units:
 *
 *         width:   Width of content.
 *         ascent:  Distance between top of content and the baseline.
 *         descent: Distance between bottom of content and the baseline.
 *
 *     The total height of the content is calculated as (ascent+descent).
 * 
 *     The point (0, 0) on the canvas is assumed to correspond to the
 *     far left edge of the content, right on the baseline vertically.
 *     
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextSetBoxDimensions(p, width, ascent, descent, em_pixels)
    InlineContext *p;
    int width;
    int ascent;
    int descent;
    int em_pixels;
{
    InlineBox *pBox;
    assert(p->nInline>0);
    pBox = &p->aInline[p->nInline-1];
    pBox->nContentPixels = width;
    pBox->nAscentPixels = ascent;
    pBox->nDescentPixels = descent;
    pBox->nEmPixels = em_pixels;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextAddSpace --
 * 
 *     This function is used to add space generated by white-space
 *     characters to an inline context.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextAddSpace(p, nPixels)
    InlineContext *p; 
    int nPixels;
{
    if (p->nInline>0) {
        InlineBox *pBox = &p->aInline[p->nInline - 1];
        if (p->eWhite == CSS_CONST_PRE) {
            pBox->nSpace += nPixels;
        } else {
            pBox->nSpace = MAX(nPixels, pBox->nSpace);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextAddNewLine --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextAddNewLine(p, nHeight)
    InlineContext *p; 
    int nHeight;
{
    InlineBox *pBox;
    inlineContextAddInlineCanvas(p, INLINE_NEWLINE, 0);
    pBox = &p->aInline[p->nInline - 1];
    pBox->nEmPixels = nHeight;

    /* This inline-box is added only to account for space that may come
     * after the new line box.
     */
    inlineContextAddInlineCanvas(p, INLINE_TEXT, 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextDrawBorder --
 *
 *     The integer array aRepX[], size (2 * nRepX), stores the
 *     x-coordinates of any replaced inline boxes that have been added to
 *     the line. This is required so that we don't draw the
 *     'text-decoration' on replaced objects (i.e. we don't want to
 *     underline images). Every second entry in aRepX is the start of a
 *     replaced inline box. Each subsequent entry is the end of the
 *     replaced inline box. All values are in the same coordinate system as
 *     the x1 and x2 parameters.
 *
 *     The y1 and y2 arguments are the vertical coordinates of the top
 *     and bottom of the content area (around which the padding and borders
 *     should be drawn). By contrast, x1 and x2 are ....
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextDrawBorder(
pLayout, pCanvas, pBorder, x1, x2, iVerticalOffset, drb, aRepX, nRepX)
    LayoutContext *pLayout;
    HtmlCanvas *pCanvas;
    InlineBorder *pBorder;
    int x1, x2;
    int iVerticalOffset;      /* Vertical offset of logical box */
    int drb;                  /* Draw Right Border */
    int *aRepX;
    int nRepX;
{
    int iTop;
    int iHeight;
    int y_o;                  /* Y-coord for overline */
    int y_t;                  /* Y-coord for linethough */
    int y_u;                  /* Y-coord for underline */

    int dlb = (pBorder->iStartBox >= 0);        /* Draw Left Border */

    int flags = (dlb?0:CANVAS_BOX_OPEN_LEFT)|(drb?0:CANVAS_BOX_OPEN_RIGHT);
    int mmt = pLayout->minmaxTest;
    HtmlNode *pNode = pBorder->pNode;
    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);

    assert(pNode && pElem);
    assert(!pBorder->isReplaced);

    if (pNode->iNode < 0) return;

    x1 += (dlb ? pBorder->margin.margin_left : 0);
    x2 -= (drb ? pBorder->margin.margin_right : 0);

    iTop = iVerticalOffset + pBorder->metrics.iFontTop - pBorder->box.iTop - 1;
    iHeight  = (pBorder->metrics.iFontBottom - pBorder->metrics.iFontTop);
    iHeight += (pBorder->box.iTop + pBorder->box.iBottom) + 1;

    if (pBorder->pParent) {
        HtmlDrawBox(pCanvas, x1, iTop, x2 - x1, iHeight, pNode, flags, mmt);
    }

    x1 += (dlb ? pBorder->box.iLeft : 0);
    x2 -= (drb ? pBorder->box.iRight : 0);

    y_o = iVerticalOffset - 1;
    y_u = iVerticalOffset + pBorder->metrics.iBaseline + 1;

    y_t = iVerticalOffset + pBorder->metrics.iBaseline - 2;
    y_t -= (pElem->pPropertyValues->fFont->ex_pixels) / 2;

    /* At this point we draw a horizontal line for the underline,
     * linethrough or overline decoration. The line is to be drawn
     * between 'x1' and 'x2' x-coordinates, at y-coordinate 'y'.
     *
     * However, we don't want to draw this decoration on replaced
     * inline boxes. So use the aReplacedX[] array to avoid doing this.
     */
    if (nRepX > 0) {
        int xa = x1;
        int i;
        for (i = 0; i < nRepX; i++) {
            int xs = aRepX[i*2];         /* Start of replaced box $i */
            int xe = aRepX[i*2+1];       /* End of replaced box $i */
            if (xe <= xs) continue;

            if (xs > xa) {
                int xb = MIN(xs, x2);
                HtmlDrawLine(pCanvas, xa, xb-xa, y_o, y_t, y_u, pNode, mmt);
            }
            if (xe > xa) {
                xa = xe;
            }
        }
        if (xa < x2) {
            HtmlDrawLine(pCanvas, xa, x2-xa, y_o, y_t, y_u, pNode, mmt);
        }
    } else {
        HtmlDrawLine(pCanvas, x1, x2 - x1, y_o, y_t, y_u, pNode, mmt);
    }
}

static void
calculateLineBoxHeight(pContext, nBox, hasText, piTop, piBottom)
    InlineContext *pContext;
    int nBox;
    int hasText;               /* True if line-box contains text */
    int *piTop;                /* OUT: Top of line box */
    int *piBottom;             /* OUT: Bottom of line box */
{
    InlineBorder *p;
    int iTop;
    int iBottom;
    int ii;
    int iVerticalOffset = 0;
    int doLineHeightQuirk = 0;

    iTop = 0;
    iBottom = 0;

    if (!hasText && pContext->pTree->options.mode != HTML_MODE_STANDARDS) {
        doLineHeightQuirk = 1;
    }

    /* Inline boxes that flow over from previous lines. */
    for (p = pContext->pBorders; p; p = p->pNext) {
        assert(p->eLineboxAlign == 0);
        assert(p->isReplaced == 0);
        if (!doLineHeightQuirk) {
            iVerticalOffset += p->iVerticalAlign;
            iTop = MIN(iTop, iVerticalOffset);
            iBottom = MAX(iBottom, p->iVerticalAlign + p->metrics.iLogical);
        }
    }

    /* Inline boxes that start on this line. */
    for (ii = 0; ii < nBox; ii++) {
        for (p = pContext->aInline[ii].pBorderStart; p; p = p->pNext) {
            assert(p->eLineboxAlign == 0);
            if (!doLineHeightQuirk) {
                InlineBorder *p2;
                iVerticalOffset = 0;
                for (p2 = p; p2; p2 = p2->pParent) {
                    iVerticalOffset += p2->iVerticalAlign;
                }
                iTop = MIN(iTop, iVerticalOffset);
                iBottom = MAX(iBottom, iVerticalOffset + p->metrics.iLogical);
            } else if (p->isReplaced) {
                iBottom = MAX(iBottom, p->metrics.iLogical);
            }
        }
    }

    /* Set the functions output variables. */
    assert(iBottom >= iTop);
    *piTop = iTop;
    *piBottom = iBottom;

    START_LOG(pContext->pNode);
        oprintf(pLog, "<ul>");
        oprintf(pLog, "<li>iTop = %d\n", iTop);
        oprintf(pLog, "<li>iBottom = %d\n", iBottom);
        oprintf(pLog, "<li>iBottom-iTop = %d\n", iBottom-iTop);
        oprintf(pLog, "</ul>");
    END_LOG("calculateLineBoxHeight");
}

static int
calculateLineBoxWidth(p, flags, iReqWidth, piWidth, pnBox, pHasText)
    InlineContext *p;        /* Inline context */
    int flags;               /* As for HtmlInlineContextGetLineBox() */
    int iReqWidth;           /* Requested line box width */
    int *piWidth;            /* OUT: Width of line box */
    int *pnBox;              /* OUT: Number of inline tokens in line box */
    int *pHasText;           /* OUT: True if there is a text or newline box */
{
    int nBox = 0;
    int iWidth = 0;
    int ii = 0;
    int hasText = 0;

    int isForceLine = (flags & LINEBOX_FORCELINE);
    int isForceBox = (flags & LINEBOX_FORCEBOX);

    /* This block sets the local variables nBox and iWidth.
     *
     * If 'white-space' is not "nowrap", count how many of the inline boxes
     * fit within the requested line-box width. Store this in nBox. Also
     * remember the width of the line-box assuming normal word-spacing.
     * We'll need this to handle the 'text-align' attribute later on.
     * 
     * If 'white-space' is "nowrap", then this loop is used to determine
     * the width of the line-box only.
     */
    for(ii = 0; ii < p->nInline; ii++) {
        InlineBox *pBox = &p->aInline[ii];
        int eType = pBox->eType;
        int iBoxWidth = pBox->nContentPixels;

        if (eType == INLINE_TEXT || eType == INLINE_NEWLINE) {
            hasText = 1;
        }

        iBoxWidth += pBox->nRightPixels + pBox->nLeftPixels;
        if(ii > 0) {
            iBoxWidth += p->aInline[ii - 1].nSpace;
        }
        if (iWidth + iBoxWidth > iReqWidth && p->eWhite != CSS_CONST_NOWRAP) {
            if ( 
                ii < (p->nInline + 1) && 
                p->aInline[ii + 1].eType == INLINE_NEWLINE
            ) {
                ii++;
                hasText = 1;
            }
            break;
        }
        iWidth += iBoxWidth;
        if (pBox->eType == INLINE_NEWLINE) {
            ii++;
            hasText = 1;
            break;
        }
    }
    nBox = ii;

    if ((p->nInline == 0) || (!isForceLine && (nBox == p->nInline))) {
        /* Either the inline context contains no inline-boxes or there are
         * not enough to fill the line-box and the 'force-line' flag is not
         * set. In this case return 0 and set *pWidth to 0 too.
         *
         * This also catches the case where 'white-space' is "nowrap". In
         * that case, we only want to draw the line-box if the 'force-line'
         * flag is set.
         */
        iWidth = 0;
        nBox = 0;
        goto exit_calculatewidth;
    }

    if (nBox == 0) {
        assert(p->nInline > 0 && p->aInline[0].eType != INLINE_NEWLINE);

        if (isForceBox) {
	    /* The first inline-box is too wide for the supplied width, but
	     * the 'forcebox' flag is set so we have to lay out at least
	     * one box. A gotcha is that we don't want to lay out our last
	     * inline box unless the 'forceline' flag is set. We might need
	     * it to help close an inline-border.
             */
            if (p->nInline > 1 || isForceLine) {
                InlineBox *pBox = &p->aInline[0];
                int iBoxWidth = pBox->nContentPixels;
                iBoxWidth += pBox->nRightPixels + pBox->nLeftPixels;
                assert(iBoxWidth > iReqWidth);

                iWidth = iBoxWidth;
                nBox = 1;
                if (p->aInline[1].eType == INLINE_NEWLINE) {
                    nBox++;
                }

            } else {
                iWidth = 0;
                nBox = 0;
                goto exit_calculatewidth;
            }
        }
    }

    if (nBox == 0) {
	/* If we get here, then their are inline-boxes, but the first
         * of them is too wide for the width we've been offered and the
         * 'forcebox' flag is not true. Return zero, but set *pWidth to the
         * minimum width required before doing so.
         */
        InlineBox *pBox = &p->aInline[0];
        assert(p->nInline > 0 && pBox->eType != INLINE_NEWLINE);
        iWidth = pBox->nContentPixels;
        iWidth += pBox->nRightPixels + pBox->nLeftPixels;
        goto exit_calculatewidth;
    }

    if (p->eWhite == CSS_CONST_NOWRAP && iWidth > iReqWidth && !isForceBox) {
        /* If the 'white-space' property is set to "nowrap" and the linebox
         * is wider than the allocated width, then only draw it if the
         * 'forcebox' flag is true. Otherwise, give the caller the
         * opportunity to shift the line-box vertically downwards to clear
         * some floating margins.
         */
        nBox = 0;
        goto exit_calculatewidth;
    }

  exit_calculatewidth:
    if (nBox == 0) {
        hasText = 0;
    }
    *piWidth = iWidth;
    *pnBox = nBox;
    *pHasText = hasText;

    return ((nBox == 0) ? 0 : 1);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextGetLineBox --
 *
 *     Parameter pWidth is a little complicated. When this function is
 *     called, *pWidth should point to the width available for the
 *     current-line box. If not even one inline-box can fit within this
 *     width, and the LINEBOX_FORCEBOX flag is not true, then zero is
 *     returned and *pWidth set to the minimum width required to draw
 *     content. If zero is returned and *pWidth is set to 0, then the
 *     InlineContext is completely empty of inline-boxes and no line-box
 *     can be generated.
 *
 *     Two flags are supported (passed via the flags bitmask argument):
 *
 *         LINEBOX_FORCELINE 
 *             Return a partially filled line-box (i.e. the final line-box
 *             of a paragraph).
 *         LINEBOX_FORCEBOX 
 *             Return a line-box containing a at least a single inline-box,
 *             even if that inline-box is wider than *pWidth.
 *
 *     If non-zero is returned, a line box is returned by way of the
 *     three output arguments pCanvas, pVSpace and pAscent. The logical
 *     top-left of the line-box is at coordinates (0, 0) of *pCanvas.
 *     *pVSpace is set to the height of the line-box. *pAscent is the
 *     distance between the top of the line-box (0, 0) and the baseline
 *     of the line-box.
 *
 *  Algorithm:
 *
 *     1. Determine the number of tokens to stack horizontally. Take the
 *        InlineContext.iTextIndent into account here.
 *
 *     2. Determine the height of the resulting line-box.
 *
 *     3. Figure out if any extra pixels are required for "text-align:justify" 
 *
 *     4. Layout text and replaced boxes into the content canvas.
 *
 *     5. Draw required inline borders and underlines into the "border" canvas.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlInlineContextGetLineBox(pLayout, p, flags, pWidth, pCanvas, pVSpace,pAscent)
    LayoutContext *pLayout;
    InlineContext *p;
    int flags;                /* IN: See above */
    int *pWidth;              /* IN/OUT: See above */
    HtmlCanvas *pCanvas;      /* OUT: Canvas to render line box to */
    int *pVSpace;             /* OUT: Total height of generated linebox */
    int *pAscent;             /* OUT: Ascent of line box */
{
    InlineContext * const pContext = p;  /* For the benefit of the LOG macros */
    int i;                   /* Iterator variable for aInline */
    int j;
    int iLineWidth = 0;      /* Width of line-box in pixels */
    int nBox = 0;            /* Number of inline boxes to draw */
    int x = 0;               /* Current x-coordinate */
    double nExtra = -10.0;   /* Extra justification pixels between each box */
    InlineBorder *pBorder;
    int *aReplacedX = 0;     /* List of x-coords - borders of replaced objs. */
    int nReplacedX = 0;      /* Size of aReplacedX divided by 2 */

    /* True if this line-box contains one or more INLINE_NEWLINE or
     * INLINE_TEXT elements. This is used to activate a line-box height quirk
     * in both "quirks" and "almost standards" mode. This variable is set
     * by calculateLineBoxWidth() and used by both this function and
     * calculateLineBoxHeight().
     *
     */
    int hasText;
    
    /* Variables iTop and iBottom are both relative to the logical top of
     * the root inline box (i.e. the one generated by the block element that
     * creates the inline context).
     */
    int iTop;                /* Top of line-box. */
    int iBottom;             /* Bottom of line-box. */
    int iLeft = 0;           /* Leftmost pixel of line box */

    /* The amount of horizontal space available in which to stack boxes */
    const int iReqWidth = *pWidth - p->iTextIndent;

    HtmlCanvas content;      /* Canvas for content (as opposed to borders) */
    HtmlCanvas borders;      /* Canvas for borders */
    memset(&content, 0, sizeof(HtmlCanvas));
    memset(&borders, 0, sizeof(HtmlCanvas));

    /* Calculate the line box width, and the number of tokens that make
     * up the line box. This function may return 0 (meaning a line box
     * cannot be created) under two circumstances:
     *
     *     + There are not enough tokens to fill the line box 
     *       and the LINEBOX_FORCELINE flag is not set. iLineWidth
     *       is set to zero if this is the case.
     *     + The first token is wider than iReqWidth, and the 
     *       LINEBOX_FORCEBOX flag is not set. In this case iLineWidth
     *       is set to the width required by the first inline token.
     */
    if (!calculateLineBoxWidth(p,flags,iReqWidth,&iLineWidth,&nBox,&hasText)) {
        *pWidth = iLineWidth;
        return 0;
    }
    assert(nBox <= p->nInline);

    /* Figure out the line-box height */
    calculateLineBoxHeight(pContext, nBox, hasText, &iTop, &iBottom);
    assert(iTop <= 0);
    assert(iBottom >= 0);
    *pVSpace = iBottom - iTop;
    *pAscent = p->pRootBorder->metrics.iBaseline - iTop;

    /* Adjust the initial left-margin offset and the nExtra variable to 
     * account for the 'text-align' property. nExtra is the number of extra
     * pixels added between each inline-box. This is how text 
     * justification is implemented.
     */
    switch(p->eTextAlign) {
        case CSS_CONST_CENTER:
            iLeft = (iReqWidth - iLineWidth) / 2;
            break;
        case CSS_CONST_RIGHT:
            iLeft = (iReqWidth - iLineWidth);
            break;
        case CSS_CONST_JUSTIFY:
            if (nBox > 1 && iReqWidth > iLineWidth && nBox < p->nInline) {
                nExtra = (double)(iReqWidth - iLineWidth) / (double)(nBox-1);
            }
            break;
    }
    iLeft += p->iTextIndent;
    x += iLeft;

    /* Draw nBox boxes side by side in pCanvas to create the line-box. */
    for(i = 0; i < nBox; i++) {
        int extra_pixels = 0;   /* Number of extra pixels for justification */
        InlineBox *pBox = &p->aInline[i];
        int boxwidth = pBox->nContentPixels;
        int x1;
        int x2;
        int nBorderDraw = 0;

        /* If the 'text-align' property is set to "justify", then we add a
         * few extra pixels between each inline box to justify the line.
         * The calculation of exactly how many is slightly complicated
         * because we need to avoid rounding bugs. If the right margins
         * vertically adjacent lines of text don't align by 1 or 2 pixels,
         * it spoils the whole effect.
         */
        if (nExtra > 0.0) {
            if (i < nBox-1) {
                extra_pixels = (nExtra * i);
            } else {
                extra_pixels = iReqWidth - iLineWidth;
            }
        }

        if (
            !pContext->isSizeOnly &&
            pBox != &p->aInline[0] && pBox->eType == INLINE_TEXT && 
            pBox->pNode && pBox[-1].pNode &&
            pBox[-1].eType == INLINE_TEXT
        ) {
            HtmlFont *pFont = HtmlNodeComputedValues(pBox->pNode)->fFont;

            /* If two tokens from the same text node are drawn in succession,
             * and they are seperated by a single space (or really, by the
             * same number of pixels as a space, then they can be combined
             * into a single primitive using HtmlDrawTextExtend(). This
             * is possible due to the super-tricky way the HtmlTextNode
             * structure stores it's data (see htmltext.c).
             *
             * This is an optimization only.
             */
            if (
                pBox->pNode == pBox[-1].pNode && 
                nExtra <= 0.0 && 
                pFont->space_pixels == pBox[-1].nSpace
            ) {
                int iWidth = pBox->canvas.right;
                int nChar = HtmlDrawTextLength(&pBox->canvas) + 1;
                HtmlDrawTextExtend(&content, nChar, pBox[-1].nSpace + iWidth);
                HtmlDrawCleanup(pContext->pTree, &pBox->canvas);
            }

            /* Otherwise, if there are no borders drawn between the two
             * adjacent text boxes, stretch the previous box so that 
             * there is no gap between the two boxes.  This ensures that
             * selected regions (a.k.a. text tags) are drawn contigiously.
             */
            else if (
                pBox->nLeftPixels == 0 &&
                pBox[-1].nRightPixels == 0
            ) {
                int iExtra = 0;
                if (nExtra > 0.0) {
                    iExtra = (extra_pixels - (int)(nExtra * (i-1)));
                }
                HtmlDrawTextExtend(&content, 0, pBox[-1].nSpace + iExtra);
            }
        }

        /* If any inline-borders start with this box, then add them to the
         * active borders list now. Remember the current x-coordinate and
         * inline-box for when we have to go back and draw the border.
         */
        pBorder = pBox->pBorderStart;
        x1 = x + extra_pixels + pBox->nLeftPixels;
        for (pBorder=pBox->pBorderStart; pBorder; pBorder=pBorder->pNext) {
            x1 -= pBorder->margin.margin_left;
            x1 -= pBorder->box.iLeft;
            pBorder->iStartBox = i;
            pBorder->iStartPixel = x1;
            p->iVAlign += pBorder->iVerticalAlign;
            if (!pBorder->pNext) {
                pBorder->pNext = p->pBorders;
                p->pBorders = pBox->pBorderStart;
                break;
            }
        }

        /* Copy the inline box canvas into the line-content canvas. If this
         * is a replaced inline box, then add the right and left
         * coordinates for the box to the aReplacedX[] array. This is used
         * to make sure we don't underline replaced objects when drawing
         * inline borders. 
         */
        x1 = x + extra_pixels + pBox->nLeftPixels;
        if (pBox->eType == INLINE_REPLACED) {
            int nBytes;
            nReplacedX++;
            nBytes = nReplacedX * 2 * sizeof(int);
            aReplacedX = (int *)HtmlRealloc("temp", (char *)aReplacedX, nBytes);
            aReplacedX[(nReplacedX-1)*2] = x1;
            aReplacedX[(nReplacedX-1)*2+1] = x1 + boxwidth;
        }
        if (hasText || pContext->pTree->options.mode == HTML_MODE_STANDARDS) {
            DRAW_CANVAS(&content, &pBox->canvas, x1, p->iVAlign, pBox->pNode);
        } else {
            assert(pBox->eType == INLINE_REPLACED);
            assert(pBox->pBorderStart);
            assert(pBox->pBorderStart->isReplaced == 1);
            DRAW_CANVAS(&content, &pBox->canvas, x1, 0, pBox->pNode);
        }
        x += (boxwidth + pBox->nLeftPixels + pBox->nRightPixels);

        /* If any inline-borders end with this box, then draw them to the
         * border canvas and remove them from the active borders list now. 
         * When drawing borders, we have to traverse the list backwards, so
         * that inner borders (and backgrounds) are drawn on top of outer
         * borders. This is a little clumsy with the singly linked list,
         * but we don't expect the list to ever have more than a couple of
         * elements, so it should be Ok.
         */
        x2 = x + extra_pixels - pBox->nRightPixels;
        if (i == nBox-1) {
            for (pBorder = p->pBorders; pBorder; pBorder = pBorder->pNext) {
                nBorderDraw++;
            }
        } else {
            nBorderDraw = pBox->nBorderEnd;
        }
        for(j = 0; j < nBorderDraw; j++) {
            int k;
            int rb;
            HtmlCanvas tmpcanvas;
            int iVerticalOffset = 0;
            InlineBorder *pTmp;

            pBorder = p->pBorders;
            for (k=0; k<j; k++) {
                pBorder = pBorder->pNext;
            }
            assert(pBorder);

            /* Do nothing for borders used to align replaced boxes */
            if (pBorder->isReplaced) continue;

            /* Figure out the vertical offset of the inline box that
             * generates this border. Variable iVerticalOffset is relative
             * to the top of the inline box generated by the root of 
             * this inline context.
             */
            for (pTmp = pBorder; pTmp; pTmp = pTmp->pParent) {
                iVerticalOffset += pTmp->iVerticalAlign;
            }

            if (pBorder->iStartBox >= 0) {
                x1 = pBorder->iStartPixel;
            } else {
                x1 = iLeft;
            }
            rb = (j < pBox->nBorderEnd);
            if (rb) {
                x2 += pBorder->margin.margin_right;
                x2 += pBorder->box.iRight;
            }

            memset(&tmpcanvas, 0, sizeof(HtmlCanvas));
            DRAW_CANVAS(&tmpcanvas, &borders, 0, 0, 0);
            memset(&borders, 0, sizeof(HtmlCanvas));
            inlineContextDrawBorder(pLayout, &borders, pBorder, 
                 x1, x2, iVerticalOffset, rb, 
                 aReplacedX, nReplacedX
            );
            DRAW_CANVAS(&borders, &tmpcanvas, 0, 0, 0);
        }

        for(j = 0; j < pBox->nBorderEnd; j++) {
            pBorder = p->pBorders;
            if (!pBorder) {
                pBorder = p->pBoxBorders;
                assert(pBorder);
                p->pBoxBorders = pBorder->pNext;;
            } else {
                p->iVAlign -= pBorder->iVerticalAlign;
                p->pBorders = pBorder->pNext;
                HtmlFree(pBorder);
            }
        }

        x += pBox->nSpace;
    }

    /* If any borders are still in the InlineContext.pBorders list, then
     * they flow over onto the next line. Set InlineBorder.iStartBox to -1 
     * so that the next call to HtmlInlineContextGetLineBox() knows that 
     * these borders do not require a left-margin.
     */
    for(pBorder = p->pBorders; pBorder; pBorder = pBorder->pNext) {
        pBorder->iStartBox = -1;
    }

    /* Draw the borders and content canvas into the target canvas. Draw the
     * borders canvas first so that it is under the content.
     */
    DRAW_CANVAS(pCanvas, &borders, 0, -1 * iTop, 0);
    DRAW_CANVAS(pCanvas, &content, 0, -1 * iTop, 0);

    p->nInline -= nBox;
    memmove(p->aInline, &p->aInline[nBox], p->nInline * sizeof(InlineBox));

    if (aReplacedX) {
        HtmlFree(aReplacedX);
    }
    p->iTextIndent = 0;

    START_LOG(pContext->pNode);
        oprintf(pLog, "<ul>");
        oprintf(pLog, "<li>Requested line box width: %d", iReqWidth);
        oprintf(pLog, "<li>Generated a line box containing %d boxes", nBox);
        oprintf(pLog, "<li>line box height: %dpx", *pVSpace);
        oprintf(pLog, "<li>line box ascent: %dpx", *pAscent);
    END_LOG("HtmlInlineContextGetLineBox");
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextIsEmpty --
 *
 *     Return true if there are no inline-boxes currently accumulated in
 *     the inline-context.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlInlineContextIsEmpty(pContext)
    InlineContext *pContext;
{
    return (pContext->nInline==0);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextCleanup --
 *
 *     Clean-up all the dynamic allocations made during the life-time of
 *     this InlineContext object. The InlineContext structure itself is not
 *     deleted - as this is usually allocated on the stack, not the heap.
 *
 *     The InlineContext object should be considered unusable (as it's
 *     internal state is inconsistent) after this function is called.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlInlineContextCleanup(pContext)
    InlineContext *pContext;
{
    InlineBorder *pBorder;

    /* There are no scenarios where an inline-context is discarded
     * before all inline-boxes have been laid out into line-boxes. If
     * there are, there will be memory leaks (of InlineBorder structs).
     */
    assert(pContext->nInline == 0);
    
    pBorder = pContext->pBoxBorders;
    while (pBorder) {
        InlineBorder *pTmp = pBorder->pNext;
        HtmlFree(pBorder);
        pBorder = pTmp;
    }

    pBorder = pContext->pBorders;
    while (pBorder) {
        InlineBorder *pTmp = pBorder->pNext;
        HtmlFree(pBorder);
        pBorder = pTmp;
    }

    if (pContext->aInline) {
        HtmlFree(pContext->aInline);
    }

    HtmlFree(pContext);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextNew --
 *
 *     Allocate and return a new InlineContext object. pNode is a pointer to
 *     the block box that generates the inline context. 
 * 
 *     If argument isSizeOnly is non-zero, then the context uses a value of
 *     "left" for the 'text-align' property, regardless of the value of
 *     pNode->eTextAlign.
 *
 *     The third argument is the used value, in pixels, of the 'text-indent'
 *     property. This value is passed in seperately (instead of being extracted
 *     from pNode->pPropertyValues) because it may be specified as a percentage
 *     of the containing block. This module does not have access to that data,
 *     hence the used property value must be calculated by the caller.
 *
 * Results:
 *     Pointer to new InlineContext structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
InlineContext *
HtmlInlineContextNew(pTree, pNode, isSizeOnly, iTextIndent)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int isSizeOnly;
    int iTextIndent;    /* Pixel balue of 'text-indent' for parent block box */
{
    HtmlComputedValues *pValues = HtmlNodeComputedValues(pNode);
    InlineContext *pContext;

    pContext = HtmlNew(InlineContext);
    pContext->pTree = pTree;
    pContext->pNode = pNode;

    /* Set the value of the 'text-align' property to use when formatting an
     * inline-context. An entire inline context always has the same value
     * for 'text-align', the value assigned to the block that generates the
     * inline context. For example, in the following code:
     *
     *     <p style="text-align:center">
     *         .... text ....
     *         <span style="text-align:left">
     *         .... more text ....
     *     </p>
     *
     * all lines are centered. The style attribute of the <span> tag has no
     * effect on the layout.
     *
     * If the 'white-space' property is set to other than 'normal', then
     * any specified value of 'text-align' is ignored and inline blocks
     * are aligned against the left margin.
     */
    pContext->eWhite = pValues->eWhitespace;
    pContext->eTextAlign = pValues->eTextAlign;
    if (isSizeOnly) { 
        pContext->eTextAlign = CSS_CONST_LEFT;
    } else if (
        pValues->eWhitespace != CSS_CONST_NORMAL && 
        pContext->eTextAlign == CSS_CONST_JUSTIFY
    ) {
        pContext->eTextAlign = CSS_CONST_LEFT;
    }

    if (
        pTree->options.mode != HTML_MODE_STANDARDS &&
        pValues->eDisplay == CSS_CONST_TABLE_CELL
    ) {
        pContext->ignoreLineHeight = 1;
    }

    /* 'text-indent' property affects the geometry of the first line box
     * generated by this inline context. The value of isSizeOnly is passed
     * to all of the HtmlDrawXXX() calls (so that they don't allocate a screen
     * graph if we are just testing for the min/max size of a block).
     */
    pContext->iTextIndent = iTextIndent;
    pContext->isSizeOnly = isSizeOnly;

    START_LOG(pNode);
        const char *zWhiteSpace = HtmlCssConstantToString(pContext->eWhite);
        const char *zTextAlign = HtmlCssConstantToString(pContext->eTextAlign);

        oprintf(pLog, "<p>Created a new inline context initialised with:</p>");
        oprintf(pLog, "<ul><li>'white-space': %s", zWhiteSpace);
        oprintf(pLog, "    <li>'text-align': %s", zTextAlign);
        oprintf(pLog, "    <li>'text-indent': %dpx", pContext->iTextIndent);
    END_LOG("HtmlInlineContextNew");

    return pContext;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextAddText --
 *
 *     Argument pNode must be a pointer to a text node. All tokens that 
 *     make up the text node are added to the InlineContext object pContext.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlInlineContextAddText(pContext, pNode)
    InlineContext *pContext;
    HtmlNode *pNode;
{
    HtmlTextIter sIter;

    XColor *color;                 /* Color to render in */
    HtmlFont *pFont;               /* Font to render in */
    Tk_Font tkfont;                /* Copy of pFont->tkfont */

    int sw;                        /* Space-Width in pFont. */
    int nh;                        /* Newline-height in pFont */
    const int szonly = pContext->isSizeOnly;

    HtmlComputedValues *pValues;   /* Computed values (of parent node) */
    int iIndex = 0;

    assert(pNode && HtmlNodeIsText(pNode) && HtmlNodeParent(pNode));
    pValues = HtmlNodeComputedValues(pNode);
    assert(pValues);
    pFont = pValues->fFont;

    tkfont = pFont->tkfont;
    color = pValues->cColor->xcolor;

    sw = pFont->space_pixels;
    nh = pFont->metrics.ascent + pFont->metrics.descent;

    assert(HtmlNodeIsText(pNode));

    for (
        HtmlTextIterFirst((HtmlTextNode *)pNode, &sIter);
        HtmlTextIterIsValid(&sIter);
        HtmlTextIterNext(&sIter)
    ) {
        int nData = HtmlTextIterLength(&sIter);
        char const *zData = HtmlTextIterData(&sIter);
        int eType = HtmlTextIterType(&sIter);

        switch (eType) {
            case HTML_TEXT_TOKEN_TEXT: {
                Tcl_Obj *pText;
                HtmlCanvas *p; 
                int tw;            /* Text width */
                int ta;            /* Text ascent */
                int td;            /* Text descent */
                int tem;           /* Text em pixels */

                int y;             /* Y-offset */

                p = inlineContextAddInlineCanvas(pContext, INLINE_TEXT, pNode);
                tw = Tk_TextWidth(tkfont, zData, nData);
                ta = pFont->metrics.ascent;
                td = pFont->metrics.descent;
                tem = pFont->em_pixels;
                inlineContextSetBoxDimensions(pContext, tw, ta, td, tem);

                y = pContext->pCurrent->metrics.iBaseline;

                pText = Tcl_NewStringObj(zData, nData);
                Tcl_IncrRefCount(pText);
                HtmlDrawText(p, zData, nData, 0, y, tw, szonly, pNode, iIndex);
                Tcl_DecrRefCount(pText);

                iIndex += nData;
                pContext->ignoreLineHeight = 0;
                break;
            }

            case HTML_TEXT_TOKEN_SPACE: 
            case HTML_TEXT_TOKEN_NEWLINE: {
                int i;
                if (
                    pContext->eWhite == CSS_CONST_PRE && 
                    eType == HTML_TEXT_TOKEN_NEWLINE
                ) {
                    inlineContextAddNewLine(pContext, nh);
                } else {
                    if (
                        pContext->eWhite == CSS_CONST_PRE &&
                        HtmlInlineContextIsEmpty(pContext) &&
                        eType != HTML_TEXT_TOKEN_NEWLINE
                    ) {
                        inlineContextAddInlineCanvas(pContext, INLINE_TEXT, 0);
                        inlineContextSetBoxDimensions(pContext, 0, 0, 0, 0);
                    }
                    for (i = 0; i < nData; i++) {
                        inlineContextAddSpace(pContext, sw);
                    }
                }
                iIndex++;
                break;
            }

            default: assert(!"Illegal value returned by TextIterType()");
        }
    }

    return;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextAddBox --
 *
 *     Add a pre-rendered box to the InlineContext object *pContext. This
 *     is used by both replaced objects and stuff that is treated as a
 *     replaced object by the inline context (i.e. blocks with the 'display'
 *     property set to "inline-block" or "inline-table".
 *
 *     Border, padding and margins for the pre-rendered box should be 
 *     handled by the caller. The top-left corner of the margin-box should
 *     be at coordinates (0, 0) of the input canvas (i.e. pCanvas).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlInlineContextAddBox(pContext, pNode, pCanvas, iWidth, iHeight, iOffset)
    InlineContext *pContext;
    HtmlNode * pNode;
    HtmlCanvas *pCanvas;
    int iWidth;
    int iHeight;
    int iOffset;
{
    HtmlCanvas *pInline;
    int ascent = -1 * iOffset;           /* Ascent of added box */
    int descent = iHeight + iOffset;     /* Descent of added box */
    InlineBorder *pBorder;

    CHECK_INTEGER_PLAUSIBILITY(ascent);
    CHECK_INTEGER_PLAUSIBILITY(descent);

    if (iWidth == 0) {
        HtmlDrawCleanup(pContext->pTree, pCanvas);
        return;
    }

    pBorder = HtmlNew(InlineBorder);
    pBorder->isReplaced = 1;
    pBorder->pNode = pNode;
    pBorder->metrics.iLogical = iHeight;
    pBorder->metrics.iBaseline = iHeight;
    pBorder->metrics.iFontBottom = iHeight;
    pBorder->metrics.iFontTop = 0;

    HtmlInlineContextPushBorder(pContext, pBorder);
    pInline = inlineContextAddInlineCanvas(pContext, INLINE_REPLACED, pNode);
    DRAW_CANVAS(pInline, pCanvas, 0, 0, pNode);
    HtmlInlineContextPopBorder(pContext, pBorder);

    inlineContextSetBoxDimensions(pContext, iWidth, ascent, descent, 0);
}

void 
HtmlInlineContextSetTextIndent(pContext, iTextIndent)
    InlineContext *pContext;
    int iTextIndent;
{
    pContext->iTextIndent = iTextIndent;
}

HtmlNode *HtmlInlineContextCreator(pContext)
    InlineContext *pContext;
{
    return pContext->pNode;
}
