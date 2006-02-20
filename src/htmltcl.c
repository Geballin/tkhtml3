/*
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static char const rcsid[] = "@(#) $Id: htmltcl.c,v 1.64 2006/02/20 12:19:06 danielk1977 Exp $";

#include <tk.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "html.h"
#include "swproc.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "htmldefaultstyle.c"

#define SafeCheck(interp,str) if (Tcl_IsSafe(interp)) { \
    Tcl_AppendResult(interp, str, " invalid in safe interp", 0); \
    return TCL_ERROR; \
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlLog --
 * HtmlTimer --
 *
 *     This function is used by various parts of the widget to output internal
 *     information that may be useful in debugging. 
 *
 *     The first argument is the HtmlTree structure. The second is a string
 *     identifying the sub-system logging the message. The third argument is a
 *     printf() style format string and subsequent arguments are substituted
 *     into it to form the logged message.
 *
 *     If -logcmd is set to an empty string, this function is a no-op.
 *     Otherwise, the name of the subsystem and the formatted error message are
 *     appended to the value of the -logcmd option and the result executed as a
 *     Tcl script.
 *
 *     This function is replaced with an empty macro if NDEBUG is defined at
 *     compilation time. If the "-logcmd" option is set to an empty string it
 *     returns very quickly.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Invokes the -logcmd script, if it is not "".
 *
 *---------------------------------------------------------------------------
 */
void 
logCommon(
    HtmlTree *pTree, 
    Tcl_Obj *pLogCmd, 
    CONST char *zSubject, 
    CONST char *zFormat, 
    va_list ap
)
{
    if (pLogCmd) {
        char zBuf[200];
        int nBuf;
        Tcl_Obj *pSubject;
        Tcl_Obj *pMessage;
        Tcl_Obj *apArgs[3];

        nBuf = vsnprintf(zBuf, 200, zFormat, ap);

        pMessage = Tcl_NewStringObj(zBuf, nBuf);
        Tcl_IncrRefCount(pMessage);
        pSubject = Tcl_NewStringObj(zSubject, -1);
        Tcl_IncrRefCount(pSubject);

        apArgs[0] = pLogCmd;
        apArgs[1] = pSubject;
        apArgs[2] = pMessage;

        if (Tcl_EvalObjv(pTree->interp, 3, apArgs, TCL_GLOBAL_ONLY)) {
            Tcl_BackgroundError(pTree->interp);
        }

        Tcl_DecrRefCount(pMessage);
        Tcl_DecrRefCount(pSubject);
    }
}

void 
HtmlTimer(HtmlTree *pTree, CONST char *zSubject, CONST char *zFormat, ...) {
    va_list ap;
    va_start(ap, zFormat);
    logCommon(pTree, pTree->options.timercmd, zSubject, zFormat, ap);
}
void 
HtmlLog(HtmlTree *pTree, CONST char *zSubject, CONST char *zFormat, ...) {
    va_list ap;
    va_start(ap, zFormat);
    logCommon(pTree, pTree->options.logcmd, zSubject, zFormat, ap);
}

/*
 *---------------------------------------------------------------------------
 *
 * doLoadDefaultStyle --
 *
 *     Load the default-style sheet into the current stylesheet configuration.
 *     The text of the default stylesheet is stored in the -defaultstyle
 *     option.
 *
 *     This function is called once when the widget is created and each time
 *     [.html reset] is called thereafter.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Loads the default style.
 *
 *---------------------------------------------------------------------------
 */
static void
doLoadDefaultStyle(pTree)
    HtmlTree *pTree;
{
    Tcl_Obj *pObj = pTree->options.defaultstyle;
    Tcl_Obj *pId = Tcl_NewStringObj("agent", 5);
    assert(pObj);
    Tcl_IncrRefCount(pId);
    HtmlStyleParse(pTree, pTree->interp, pObj, pId, 0, 0);
    Tcl_DecrRefCount(pId);
}

/*
 *---------------------------------------------------------------------------
 *
 * doSingleScrollCallback --
 *
 *     Helper function for doScrollCallback().
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May invoke the script in *pScript.
 *
 *---------------------------------------------------------------------------
 */
static void
doSingleScrollCallback(interp, pScript, iOffScreen, iTotal, iPage)
    Tcl_Interp *interp;
    Tcl_Obj *pScript;
    int iOffScreen;
    int iTotal;
    int iPage;
{
    if (pScript) {
        double fArg1;
        double fArg2;
        int rc;
        Tcl_Obj *pEval; 

        if (iTotal == 0){ 
            fArg1 = 0.0;
            fArg2 = 1.0;
        } else {
            fArg1 = (double)iOffScreen / (double)iTotal;
            fArg2 = (double)(iOffScreen + iPage) / (double)iTotal;
            fArg2 = MIN(1.0, fArg2);
        }

        pEval = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pEval);
        Tcl_ListObjAppendElement(interp, pEval, Tcl_NewDoubleObj(fArg1));
        Tcl_ListObjAppendElement(interp, pEval, Tcl_NewDoubleObj(fArg2));
        rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);
        if (TCL_OK != rc) {
            Tcl_BackgroundError(interp);
        }
        Tcl_DecrRefCount(pEval);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * doScrollCallback --
 *
 *     Invoke both the -xscrollcommand and -yscrollcommand scripts (unless they
 *     are set to empty strings). The arguments passed to the two scripts are
 *     calculated based on the current values of HtmlTree.iScrollY,
 *     HtmlTree.iScrollX and HtmlTree.canvas.
 *
 * Results:
 *     None.
 *
 * Side effects: 
 *     May invoke either or both of the -xscrollcommand and -yscrollcommand
 *     scripts.
 *
 *---------------------------------------------------------------------------
 */
static void 
doScrollCallback(pTree)
    HtmlTree *pTree;
{
    Tcl_Interp *interp = pTree->interp;
    Tk_Window win = pTree->tkwin;

    Tcl_Obj *pScrollCommand;
    int iOffScreen;
    int iTotal;
    int iPage;

    pScrollCommand = pTree->options.yscrollcommand;
    iOffScreen = pTree->iScrollY;
    iTotal = pTree->canvas.bottom;
    iPage = Tk_Height(win);
    doSingleScrollCallback(interp, pScrollCommand, iOffScreen, iTotal, iPage);

    pScrollCommand = pTree->options.xscrollcommand;
    iOffScreen = pTree->iScrollX;
    iTotal = pTree->canvas.right;
    iPage = Tk_Width(win);
    doSingleScrollCallback(interp, pScrollCommand, iOffScreen, iTotal, iPage);
}


/*
 *---------------------------------------------------------------------------
 *
 * callbackHandler --
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
callbackHandler(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int eCallbackAction = pTree->cb.eCallbackAction;
    pTree->cb.eCallbackAction = HTML_CALLBACK_NONE;

    int x, y;
    int w, h;

    int canvas_x = 0;
    int canvas_y = 0;

    clock_t styleClock = 0;
    clock_t layoutClock = 0;

    assert(
        eCallbackAction == HTML_CALLBACK_LAYOUT ||
        eCallbackAction == HTML_CALLBACK_STYLE ||
        eCallbackAction == HTML_CALLBACK_DAMAGE
    );

    switch (eCallbackAction) {
        case HTML_CALLBACK_STYLE:
            styleClock = clock();
            HtmlStyleApply((ClientData)pTree, pTree->interp, 0, 0);
            styleClock = clock() - styleClock;
        case HTML_CALLBACK_LAYOUT:
            layoutClock = clock();
            HtmlLayout(pTree);
            x = 0;
            y = 0;
            w = Tk_Width(pTree->tkwin);
            h = Tk_Height(pTree->tkwin);
            layoutClock = clock() - layoutClock;
            break;
        case HTML_CALLBACK_DAMAGE: {
            x = pTree->cb.x1;
            y = pTree->cb.y1;
            w = pTree->cb.x2 - x;
            h = pTree->cb.y2 - y;
            break;
        }
    }

    canvas_y = pTree->iScrollY;
    canvas_x = pTree->iScrollX;

    assert(w >= 0 && h >=0 && x >= 0 && y >= y);
    HtmlWidgetPaint(pTree, x + canvas_x, y + canvas_y, x, y, w, h);

    HtmlLog(pTree, "CALLBACK", "%s - %dx%d @ (%d, %d)",
        eCallbackAction == HTML_CALLBACK_STYLE ?  "STYLE" :
        eCallbackAction == HTML_CALLBACK_LAYOUT ? "LAYOUT" :
        eCallbackAction == HTML_CALLBACK_DAMAGE ? "DAMAGE" :
        "N/A",
        w, h, x, y
    );

    if (styleClock) {
        HtmlTimer(pTree, "STYLE",  "%f seconds", 
            (double)styleClock / (double)CLOCKS_PER_SEC);
    }
    if (layoutClock) {
        HtmlTimer(pTree, "LAYOUT",  "%f seconds", 
            (double)layoutClock / (double)CLOCKS_PER_SEC);
    }

    pTree->cb.x1 = Tk_Width(pTree->tkwin) + 1;
    pTree->cb.y1 = Tk_Height(pTree->tkwin) + 1;
    pTree->cb.x2 = 0;
    pTree->cb.y2 = 0;
    assert(pTree->cb.eCallbackAction == HTML_CALLBACK_NONE);

    if (
        eCallbackAction == HTML_CALLBACK_LAYOUT || 
        eCallbackAction == HTML_CALLBACK_STYLE
    ) {
        HtmlWidgetMapControls(pTree);
        doScrollCallback(pTree);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackForce --
 *
 *     If there is a callback scheduled, execute it now instead of waiting for
 *     the idle loop.
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
HtmlCallbackForce(pTree)
    HtmlTree *pTree;
{
    if (pTree->cb.eCallbackAction != HTML_CALLBACK_NONE) {
        ClientData clientData = (ClientData)pTree;
        Tcl_CancelIdleCall(callbackHandler, clientData);
        callbackHandler(clientData);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlScheduleCallback --
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
HtmlCallbackSchedule(pTree, eCallbackAction)
    HtmlTree *pTree;
    int eCallbackAction;
{
    assert(
        eCallbackAction == HTML_CALLBACK_LAYOUT ||
        eCallbackAction == HTML_CALLBACK_STYLE ||
        eCallbackAction == HTML_CALLBACK_DAMAGE
    );

    if (pTree->cb.eCallbackAction == HTML_CALLBACK_NONE) {
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }

    pTree->cb.eCallbackAction = MAX(eCallbackAction, pTree->cb.eCallbackAction);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackExtents --
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
HtmlCallbackExtents(pTree, x, y, width, height)
    HtmlTree *pTree;
    int x, y;
    int width, height;
{
    assert(width >= 0 && height >=0 && x >= 0 && y >= y);

    pTree->cb.x1 = MIN(x, pTree->cb.x1);
    pTree->cb.y1 = MIN(y, pTree->cb.y1);
    pTree->cb.x2 = MAX(x + width, pTree->cb.x2);
    pTree->cb.y2 = MAX(y + height, pTree->cb.y2);

    assert(
        pTree->cb.y1 >= 0 && 
        pTree->cb.x1 >= 0 &&
        pTree->cb.x2 >= pTree->cb.x1 && 
        pTree->cb.y2 >= pTree->cb.y1
    );
}

/*
 *---------------------------------------------------------------------------
 *
 * deleteWidget --
 *
 *     destroy $html
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
deleteWidget(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlTreeClear(pTree);

    /* Clear the remaining colors etc. from the styler code hash tables */
    HtmlComputedValuesCleanupTables(pTree);

    /* Delete the image-server */
    HtmlImageServerShutdown(pTree);

    /* Delete the structure itself */
    HtmlFree((char *)pTree);
}

/*
 *---------------------------------------------------------------------------
 *
 * widgetCmdDel --
 *
 *     This command is invoked by Tcl when a widget object command is
 *     deleted. This can happen under two circumstances:
 *
 *         * A script deleted the command. In this case we should delete
 *           the widget window (and everything else via the window event
 *           callback) as well.
 *         * A script deleted the widget window. In this case we need
 *           do nothing.
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
widgetCmdDel(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if( !pTree->isDeleted ){
      pTree->cmd = 0;
      Tk_DestroyWindow(pTree->tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * eventHandler --
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
eventHandler(clientData, pEvent)
    ClientData clientData;
    XEvent *pEvent;
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    switch (pEvent->type) {
        case ConfigureNotify: {
            /* XConfigureEvent *p = (XConfigureEvent*)pEvent; */

            HtmlLog(pTree, "EVENT", "ConfigureNotify:");

            if (Tk_Width(pTree->tkwin) != pTree->iCanvasWidth) {
                HtmlCallbackSchedule(pTree, HTML_CALLBACK_LAYOUT);
            }
            break;
        }

        case Expose: {
            XExposeEvent *p = (XExposeEvent *)pEvent;

            HtmlLog(pTree, "EVENT", "Expose: x=%d y=%d width=%d height=%d",
                p->x, p->y, p->width, p->height
            );

            HtmlCallbackSchedule(pTree, HTML_CALLBACK_DAMAGE);
            HtmlCallbackExtents(pTree, p->x, p->y, p->width, p->height);
            break;
        }

        case VisibilityNotify: {
            XVisibilityEvent *p = (XVisibilityEvent *)pEvent;

            HtmlLog(pTree, "EVENT", "VisibilityNotify: state=%s", 
                p->state == VisibilityFullyObscured ?
                        "VisibilityFullyObscured":
                p->state == VisibilityPartiallyObscured ?
                        "VisibilityPartiallyObscured":
                p->state == VisibilityUnobscured ?
                        "VisibilityUnobscured":
                "N/A"
            );

            pTree->eVisibility = p->state;
            break;
        }

        case DestroyNotify: {
            pTree->isDeleted = 1;
            Tcl_DeleteCommandFromToken(pTree->interp, pTree->cmd);
            deleteWidget(pTree);
            break;
        }

    }
}

/*
 *---------------------------------------------------------------------------
 *
 * configureCmd --
 *
 *     Implementation of the standard Tk "configure" command.
 *
 *         <widget> configure -OPTION VALUE ?-OPTION VALUE? ...
 *
 *     TODO: Handle configure of the forms:
 *         <widget> configure -OPTION 
 *         <widget> configure
 *
 * Results:
 *     Standard tcl result.
 *
 * Side effects:
 *     May set values of HtmlTree.options struct. May call
 *     Tk_GeometryRequest().
 *
 *---------------------------------------------------------------------------
 */
static int 
configureCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    /*
     * Mask bits for options declared in htmlOptionSpec.
     */
    #define GEOMETRY_MASK  0x00000001
    #define FT_MASK        0x00000002    

    /*
     * Macros to generate static Tk_OptionSpec structures for the
     * htmlOptionSpec() array.
     */
    #define PIXELS(v, s1, s2, s3) \
        {TK_OPTION_PIXELS, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, 0}
    #define GEOMETRY(v, s1, s2, s3) \
        {TK_OPTION_PIXELS, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, GEOMETRY_MASK}
    #define STRING(v, s1, s2, s3) \
        {TK_OPTION_STRING, "-" #v, s1, s2, s3, \
         Tk_Offset(HtmlOptions, v), -1, TK_OPTION_NULL_OK, 0, 0}
    #define XCOLOR(v, s1, s2, s3) \
        {TK_OPTION_COLOR, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, 0}

    #define OBJ(v, s1, s2, s3, f) \
        {TK_OPTION_STRING, "-" #v, s1, s2, s3, \
         Tk_Offset(HtmlOptions, v), -1, 0, 0, f}
    
    /* Option table definition for the html widget. */
    static Tk_OptionSpec htmlOptionSpec[] = {

        /* Standard geometry interface */
        GEOMETRY(height, "height", "Height", "600"),
        GEOMETRY(width, "width", "Width", "800"),

        /* Standard scroll interface - same as canvas, text */
        PIXELS(yscrollincrement, "yScrollIncrement", "ScrollIncrement", "20"),
        PIXELS(xscrollincrement, "xScrollIncrement", "ScrollIncrement", "20"),
        STRING(xscrollcommand, "xScrollCommand", "ScrollCommand", ""),
        STRING(yscrollcommand, "yScrollCommand", "ScrollCommand", ""),

        /* Non-debugging widget specific options */
        STRING(defaultstyle, "defaultStyle", "DefaultStyle", HTML_DEFAULT_CSS),
        STRING(imagecmd, "imageCmd", "ImageCmd", ""),
        STRING(encoding, "encoding", "Encoding", ""),
        XCOLOR(selectbackground, "selectBackground", "Background", "darkgrey"),
        XCOLOR(selectforeground, "selectForeground", "Foreground", "white"),
    
        /* Options for logging info to debugging scripts */
        STRING(logcmd, "logCmd", "LogCmd", ""),
        STRING(timercmd, "timerCmd", "TimerCmd", ""),

        OBJ(fonttable, "fontTable", "FontTable", "7 8 9 10 12 14 16", FT_MASK),
    
        {TK_OPTION_END, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    #undef PIXELS
    #undef STRING
    #undef XCOLOR

    HtmlTree *pTree = (HtmlTree *)clientData;
    char *pOptions = (char *)&pTree->options;
    Tk_Window win = pTree->tkwin;
    Tk_OptionTable otab = pTree->optionTable;

    int mask = 0;
    int init = 0;                /* True if Tk_InitOptions() is called */
    int rc;

    if (!otab) {
        pTree->optionTable = Tk_CreateOptionTable(interp, htmlOptionSpec);
        Tk_InitOptions(interp, pOptions, pTree->optionTable, win);
        init = 1;
        otab = pTree->optionTable;
    }

    rc = Tk_SetOptions(interp, pOptions, otab, objc-2, &objv[2], win, 0, &mask);
    if (TCL_OK == rc) {
        /* Hard-coded minimum values for width and height */
        pTree->options.height = MAX(pTree->options.height, 100);
        pTree->options.width = MAX(pTree->options.width, 100);

        if (init || (mask & GEOMETRY_MASK)) {
            int w = pTree->options.width;
            int h = pTree->options.height;
            Tk_GeometryRequest(pTree->tkwin, w, h);
        }
    
        if (init || mask & FT_MASK) {
            int nSize;
            Tcl_Obj **apSize;
            int aFontSize[7];
            Tcl_Obj *pFT = pTree->options.fonttable;
            if (
                Tcl_ListObjGetElements(interp, pFT, &nSize, &apSize) ||
                nSize != 7 ||
                Tcl_GetIntFromObj(interp, apSize[0], &aFontSize[0]) ||
                Tcl_GetIntFromObj(interp, apSize[1], &aFontSize[1]) ||
                Tcl_GetIntFromObj(interp, apSize[2], &aFontSize[2]) ||
                Tcl_GetIntFromObj(interp, apSize[3], &aFontSize[3]) ||
                Tcl_GetIntFromObj(interp, apSize[4], &aFontSize[4]) ||
                Tcl_GetIntFromObj(interp, apSize[5], &aFontSize[5]) ||
                Tcl_GetIntFromObj(interp, apSize[6], &aFontSize[6])
            ) {
                rc = TCL_ERROR;
            } else {
                memcpy(pTree->aFontSizeTable, aFontSize, sizeof(aFontSize));
                HtmlCallbackSchedule(pTree, HTML_CALLBACK_STYLE);
            }
        }
    }

    return rc;
    #undef GEOMETRY_MASK
    #undef FT_MASK
}

/*
 *---------------------------------------------------------------------------
 *
 * cgetCmd --
 *
 *     Standard Tk "cget" command for querying options.
 *
 *     <widget> cget -OPTION
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int cgetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_Obj *pRet;
    Tk_OptionTable otab = pTree->optionTable;
    Tk_Window win = pTree->tkwin;
    char *pOptions = (char *)&pTree->options;

    assert(otab);

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "-OPTION");
        return TCL_ERROR;
    }

    pRet = Tk_GetOptionValue(interp, pOptions, otab, objv[2], win);
    if( pRet ) {
        Tcl_SetObjResult(interp, pRet);
    } else {
        char * zOpt = Tcl_GetString(objv[2]);
        Tcl_AppendResult( interp, "unknown option \"", zOpt, "\"", 0);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * commandCmd --
 *
 *     <widget> command SUB-COMMAND SCRIPT
 *
 *     This command is used to dynamically add commands to the widget.
 *     
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
commandCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    Tcl_Obj *pCmd;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "SUB-COMMAND SCRIPT");
        return TCL_ERROR;
    }

    pCmd = objv[2];
    pScript = objv[3];

    pEntry = Tcl_CreateHashEntry(&pTree->aCmd, Tcl_GetString(pCmd), &newentry);
    if (!newentry) {
        Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        Tcl_DecrRefCount(pOld);
    }
    Tcl_IncrRefCount(pScript);
    Tcl_SetHashValue(pEntry, pScript);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * varCmd --
 *
 *     Set or get the value of a variable from the widgets built-in
 *     dictionary. This is used by the widget logic programmed in Tcl to
 *     store state data.
 *
 *     $html var VAR-NAME ?Value?
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
varCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    Tcl_HashEntry *pEntry; 
    HtmlTree *pTree = (HtmlTree *)clientData;
    char *zVar;

    if (objc != 3 && objc != 4) {
        Tcl_WrongNumArgs(interp, 2,objv, "VAR-NAME ?VALUE?");
        return TCL_ERROR;
    }

    zVar = Tcl_GetString(objv[2]);
    if (objc == 4) {
        int newentry;
        pEntry = Tcl_CreateHashEntry(&pTree->aVar, zVar, &newentry);
        if (!newentry) {
            Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
            Tcl_DecrRefCount(pOld);
        }
        Tcl_IncrRefCount(objv[3]);
        Tcl_SetHashValue(pEntry, objv[3]);
    } else {
        pEntry = Tcl_FindHashEntry(&pTree->aVar, zVar);
        if (!pEntry) {
            Tcl_AppendResult(interp, "No such variable: ", zVar, 0);
            return TCL_ERROR;
        }
    }

    Tcl_SetObjResult(interp, (Tcl_Obj *)Tcl_GetHashValue(pEntry));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * resetCmd --
 * 
 *     $html internal reset
 *
 *     Reset the widget so that no document or stylesheet is loaded.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
resetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlTreeClear(pTree);
    doLoadDefaultStyle(pTree);
    pTree->parseFinished = 0;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseCmd --
 *
 *         $widget parse ?-final? HTML-TEXT
 * 
 *     Appends the given HTML text to the end of any HTML text that may have
 *     been inserted by prior calls to this command. See Tkhtml man page for
 *     further details.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
parseCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    int isFinal;
    char *zHtml;
    int nHtml;

    Tcl_Obj *aObj[2];
    SwprocConf aConf[3] = {
        {SWPROC_SWITCH, "final", "0", "1"},   /* -final */
        {SWPROC_ARG, 0, 0, 0},                /* HTML-TEXT */
        {SWPROC_END, 0, 0, 0}
    };

    if (
        SwprocRt(interp, (objc - 2), &objv[2], aConf, aObj) ||
        Tcl_GetBooleanFromObj(interp, aObj[0], &isFinal)
    ) {
        return TCL_ERROR;
    }
    zHtml = Tcl_GetStringFromObj(aObj[1], &nHtml);

    assert(Tcl_IsShared(aObj[1]));
    Tcl_DecrRefCount(aObj[0]);
    Tcl_DecrRefCount(aObj[1]);

    if (pTree->parseFinished) {
        const char *zWidget = Tcl_GetString(objv[0]);
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, 
            "Cannot call [", zWidget, " parse]" 
            "until after [", zWidget, "] reset", 0
        );
        return TCL_ERROR;
    }

    /* Add the new text to the internal cache of the document. Also tokenize
     * it and add the new HtmlToken objects to the HtmlTree.pFirst/pLast 
     * linked list.
     */
    HtmlTokenizerAppend(pTree, zHtml, nHtml, isFinal);
    if (isFinal) {
        pTree->parseFinished = 1;
        HtmlFinishNodeHandlers(pTree);
    }

    HtmlCallbackSchedule(pTree, HTML_CALLBACK_STYLE);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * viewCommon --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
viewCommon(pTree, isXview, objc, objv)
    HtmlTree *pTree;
    int isXview;               /* True for [xview], zero for [yview] */
    int objc;
    Tcl_Obj * CONST objv[];
{
    Tcl_Interp *interp = pTree->interp;
    Tk_Window win = pTree->tkwin;

    int iUnitPixels;           /* Value of -[xy]scrollincrement in pixels */
    int iPagePixels;           /* Width or height of the viewport */
    int iMovePixels;           /* Width or height of canvas */
    int iOffScreen;            /* Current scroll position */
    double aRet[2];
    Tcl_Obj *pRet;
    Tcl_Obj *pScrollCommand;

    if (isXview) { 
        iPagePixels = Tk_Width(win);
        iUnitPixels = pTree->options.xscrollincrement;
        iMovePixels = pTree->canvas.right;
        iOffScreen = pTree->iScrollX;
        pScrollCommand = pTree->options.xscrollcommand;
    } else {
        iPagePixels = Tk_Height(win);
        iUnitPixels = pTree->options.yscrollincrement;
        iMovePixels = pTree->canvas.bottom;
        iOffScreen = pTree->iScrollY;
        pScrollCommand = pTree->options.yscrollcommand;
    }

    if (objc > 2) {
        double fraction;
        int count;
        int iNewVal;     /* New value of iScrollY or iScrollX */
        int eType;       /* One of the TK_SCROLL_ symbols */

        eType = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
        switch (eType) {
            case TK_SCROLL_MOVETO:
                iNewVal = (int)((double)iMovePixels * fraction);
                break;
            case TK_SCROLL_PAGES:
                iNewVal = iOffScreen + ((count * iPagePixels) * 0.9);
                break;
            case TK_SCROLL_UNITS:
                iNewVal = iOffScreen + (count * iUnitPixels);
                break;
            case TK_SCROLL_ERROR:
                return TCL_ERROR;
    
            default: assert(!"Not possible");
        }
        iNewVal = MIN(iNewVal, iMovePixels - iPagePixels);
        iNewVal = MAX(iNewVal, 0);

        if (iNewVal != iOffScreen) {
            int eVisibility = pTree->eVisibility;
            int canvas_x = pTree->iScrollX;
            int canvas_y = pTree->iScrollY;
            int w = Tk_Width(win);
            int h = Tk_Height(win);
            int iScrollV = 0;
            int iScrollH = 0;

            if( isXview ){
                canvas_x = iNewVal;
                iScrollH = iNewVal - iOffScreen;
            } else {
                iScrollV = iNewVal - iOffScreen;
                canvas_y = iNewVal;
            }

            if (eVisibility == VisibilityFullyObscured) {
                /* Do nothing, window is not visible */
            } else if (
                abs(iScrollV) >= iPagePixels || 
                abs(iScrollH) >= iPagePixels || 
                eVisibility == VisibilityPartiallyObscured 
            ) {
                /* Redraw the entire window. */
                HtmlWidgetPaint(pTree, canvas_x, canvas_y, 0, 0, w, h);
            } else {
                HtmlWidgetScroll(pTree, iScrollH, iScrollV);
                HtmlWidgetPaint(pTree, 
                    canvas_x, canvas_y + iPagePixels - iScrollV, 
                    0, iPagePixels - iScrollV, 
                    w, iScrollV
                );
                HtmlWidgetPaint(pTree, 
                    canvas_x, canvas_y, 
                    0, 0,
                    w, iScrollV * -1
                );
                HtmlWidgetPaint(pTree, 
                    canvas_x + iPagePixels - iScrollH, canvas_y,
                    iPagePixels - iScrollH, 0,
                    iScrollH, h
                );
                HtmlWidgetPaint(pTree, 
                    canvas_x, canvas_y, 
                    0, 0,
                    iScrollH * -1, h
                );
            }

            pTree->iScrollY = canvas_y;
            pTree->iScrollX = canvas_x;
            iOffScreen = iNewVal;
        }
    }

    if (iMovePixels <= iPagePixels) {
        aRet[0] = 0.0;
        aRet[1] = 1.0;
    } else {
        assert(iMovePixels > 0);
        assert(iOffScreen  >= 0);
        assert(iPagePixels >= 0);
        aRet[0] = (double)iOffScreen / (double)iMovePixels;
        aRet[1] = (double)(iOffScreen + iPagePixels) / (double)iMovePixels;
        aRet[1] = MIN(aRet[1], 1.0);
    }

    pRet = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewDoubleObj(aRet[0]));
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewDoubleObj(aRet[1]));
    Tcl_SetObjResult(interp, pRet);

    if (objc > 2) {
        HtmlWidgetMapControls(pTree);
        doScrollCallback(pTree);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * xviewCmd --
 * yviewCmd --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
xviewCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    return viewCommon((HtmlTree *)clientData, 1, objc, objv);
}
static int 
yviewCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    return viewCommon((HtmlTree *)clientData, 0, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlHandlerScriptCmd --
 *
 *     $widget handler script TAG SCRIPT
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
handlerScriptCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    Tcl_Obj *pTag;
    int tag;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc!=5) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAG SCRIPT");
        return TCL_ERROR;
    }

    pTag = objv[3];
    pScript = objv[4];
    tag = HtmlNameToType(0, Tcl_GetString(pTag));

    if (tag==Html_Unknown) {
        Tcl_AppendResult(interp, "Unknown tag type: ", Tcl_GetString(pTag), 0);
        return TCL_ERROR;
    }

    pEntry = Tcl_CreateHashEntry(&pTree->aScriptHandler,(char *)tag,&newentry);
    if (!newentry) {
        Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        Tcl_DecrRefCount(pOld);
    }

    Tcl_IncrRefCount(pScript);
    Tcl_SetHashValue(pEntry, (ClientData)pScript);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * handlerNodeCmd --
 *
 *     $widget handler node TAG SCRIPT
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
handlerNodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    Tcl_Obj *pTag;
    int tag;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc!=5) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAG SCRIPT");
        return TCL_ERROR;
    }

    pTag = objv[3];
    pScript = objv[4];
    tag = HtmlNameToType(0, Tcl_GetString(pTag));

    if (tag==Html_Unknown) {
        Tcl_AppendResult(interp, "Unknown tag type: ", Tcl_GetString(pTag), 0);
        return TCL_ERROR;
    }

    if (Tcl_GetCharLength(pScript) == 0) {
        pEntry = Tcl_FindHashEntry(&pTree->aNodeHandler, (char *)tag);
        if (pEntry) {
            Tcl_DeleteHashEntry(pEntry);
        }
    } else {
        pEntry = Tcl_CreateHashEntry(&pTree->aNodeHandler,(char*)tag,&newentry);
        if (!newentry) {
            Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
            Tcl_DecrRefCount(pOld);
        }
        Tcl_IncrRefCount(pScript);
        Tcl_SetHashValue(pEntry, (ClientData)pScript);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * styleCmd --
 *
 *         $widget style ?options? HTML-TEXT
 *
 *             -importcmd IMPORT-CMD
 *             -id ID
 *             -urlcmd URL-CMD
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
styleCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SwprocConf aConf[4 + 1] = {
        {SWPROC_OPT, "id", "author", 0},      /* -id <style-sheet id> */
        {SWPROC_OPT, "importcmd", 0, 0},      /* -importcmd <cmd> */
        {SWPROC_OPT, "urlcmd", 0, 0},         /* -urlcmd <cmd> */
        {SWPROC_ARG, 0, 0, 0},                /* STYLE-SHEET-TEXT */
        {SWPROC_END, 0, 0, 0}
    };
    Tcl_Obj *apObj[4];
    int rc;
    HtmlTree *pTree = (HtmlTree *)clientData;

    /* First assert() that the sizes of the aConf and apObj array match. Then
     * call SwprocRt() to parse the arguments. If the parse is successful then
     * apObj[] contains the following: 
     *
     *     apObj[0] -> Value passed to -id option (or default "author")
     *     apObj[1] -> Value passed to -importcmd option (or default "")
     *     apObj[2] -> Value passed to -urlcmd option (or default "")
     *     apObj[3] -> Text of stylesheet to parse
     *
     * Pass these on to the HtmlStyleParse() command to actually parse the
     * stylesheet.
     */
    assert(sizeof(apObj)/sizeof(apObj[0])+1 == sizeof(aConf)/sizeof(aConf[0]));
    if (TCL_OK != SwprocRt(interp, objc - 2, &objv[2], aConf, apObj)) {
        return TCL_ERROR;
    }
    rc = HtmlStyleParse(pTree, interp, apObj[3], apObj[0], apObj[1], apObj[2]);

    /* Clean up object references created by SwprocRt() */
    SwprocCleanup(apObj, sizeof(apObj)/sizeof(Tcl_Obj *));

    if (rc == TCL_OK) {
        HtmlCallbackSchedule(pTree, HTML_CALLBACK_STYLE);
    }
    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * imageCmd --
 * nodeCmd --
 * primitivesCmd --
 * bboxCmd --
 *
 *     New versions of gcc don't allow pointers to non-local functions to
 *     be used as constant initializers (which we need to do in the
 *     aSubcommand[] array inside widgetCmd(). So the following
 *     functions are wrappers around Tcl_ObjCmdProc functions implemented
 *     in other files.
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *     Whatever the called function does.
 *
 *---------------------------------------------------------------------------
 */
static int 
bboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutBbox(clientData, interp, objc, objv);
}
static int 
imageCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutImage(clientData, interp, objc, objv);
}
static int 
nodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutNode(clientData, interp, objc, objv);
}
static int 
primitivesCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutPrimitives(clientData, interp, objc, objv);
}
static int 
searchCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCssSearch(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * selectClearCmd --
 *
 *     html select clear 
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
selectClearCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    /* Check no arguments were passed to this command. */
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 3, objv, "");
        return TCL_ERROR;
    }

    if (pTree->pToNode) {
        /* If HtmlTree.pToNode is NULL, then the selection is already clear,
         * do nothing. Otherwise, call HtmlLayoutPaintText to repaint the
         * entire selected area, then clear the selection related HtmlTree 
         * variables. 
         *
	 * It doesn't matter that we call HtmlLayoutPaintText() before
	 * modifying the state of the widget, as HtmlLayoutPaintText() only
	 * schedules a drawing callback, it does not do any actual drawing. By
	 * the time the scheduled callback is dispatched, the HtmlTree state
	 * has been modified to reflect the cleared selection.
         */
        assert(pTree->pFromNode);
        HtmlLayoutPaintText(pTree, 
            pTree->pFromNode->iNode, pTree->iFromIndex,
            pTree->pToNode->iNode, pTree->iToIndex
        );
        pTree->pFromNode = 0;
        pTree->pToNode = 0;
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * selectCmd --
 *
 *     html select from ?NODE ?INDEX??
 *     html select to ?NODE ?INDEX??
 *
 *     The [html select clear] command is handled by the C function 
 *     selectClearCmd() (see above).
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
selectCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    const char *zArg = Tcl_GetString(objv[2]);
    int *piIndex = &pTree->iFromIndex;
    HtmlNode **ppNode = &pTree->pFromNode;
    Tcl_Obj *pRes;

    assert(0 == strcmp(zArg, "to") || 0 == strcmp(zArg, "from"));
    if (*zArg=='t') {
        piIndex = &pTree->iToIndex;
        ppNode = &pTree->pToNode;
    }

    if (objc > 3) {
        HtmlNode *pNewNode;
        HtmlNode *pOldNode = *ppNode;
        int iOldIndex = *piIndex;
        int iNewIndex = -1;

        pNewNode = HtmlNodeGetPointer(pTree, Tcl_GetString(objv[3]));
        if (0 == pNewNode) {
            return TCL_ERROR;
        }
        if (objc == 5 && Tcl_GetIntFromObj(interp, objv[4], &iNewIndex)) {
            return TCL_ERROR;
        }

        *piIndex = iNewIndex;
        *ppNode = pNewNode;

        if (pTree->pFromNode && !pTree->pToNode) {
            pTree->pToNode = pTree->pFromNode;
            pTree->iToIndex = pTree->iFromIndex;
        }
        if (!pTree->pFromNode && pTree->pToNode) {
            pTree->pFromNode = pTree->pToNode;
            pTree->iFromIndex = pTree->iToIndex;
        }
        if (pOldNode) {
            HtmlLayoutPaintText(
                pTree, pNewNode->iNode, iNewIndex, pOldNode->iNode, iOldIndex
            );
        }
    }

    Tcl_ResetResult(interp);
    if (*ppNode) {
        assert((*ppNode)->pNodeCmd);
        pRes = Tcl_DuplicateObj((*ppNode)->pNodeCmd->pCommand);
        if (*piIndex>=0) {
            Tcl_ListObjAppendElement(interp, pRes, Tcl_NewIntObj(*piIndex));
        }
        Tcl_SetObjResult(interp, pRes);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * widgetCmd --
 *
 *     This is the C function invoked for a widget command.
 *
 *         cget
 *         configure
 *         handler
 *         image
 *         node
 *         parse
 *         reset
 *         select (not implemented yet)
 *         style
 *         xview
 *         yview
 *
 *         var
 *         command
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Whatever the command does.
 *
 *---------------------------------------------------------------------------
 */
int widgetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    /* The following array defines all the built-in widget commands.  This
     * function just parses the first one or two arguments and vectors control
     * to one of the command service routines defined in the following array.
     */
    static struct SC {
        char *zCmd1;                /* First-level subcommand.  Required */
        char *zCmd2;                /* Second-level subcommand.  May be NULL */
        Tcl_ObjCmdProc *xFuncObj;   /* Object cmd */
    } aSubcommand[] = {
        {"bbox",       0,        bboxCmd},
        {"cget",       0,        cgetCmd},
        {"configure",  0,        configureCmd},
        {"handler",    "node",   handlerNodeCmd},
        {"handler",    "script", handlerScriptCmd},
        {"image",      0,        imageCmd},
        {"node",       0,        nodeCmd},
        {"parse",      0,        parseCmd},
        {"primitives", 0,        primitivesCmd},
        {"reset",      0,        resetCmd},
        {"search",     0,        searchCmd},
        {"select",     "to",     selectCmd},
        {"select",     "from",   selectCmd},
        {"select",     "clear",  selectClearCmd},
        {"style",      0,        styleCmd},
        {"xview",      0,        xviewCmd},
        {"yview",      0,        yviewCmd},

        {"command",    0, commandCmd}, 
        {"var",        0, varCmd},  

        /* Todo: [<widget> select ...] command */
    };

    int i;
    CONST char *zArg1 = 0;
    CONST char *zArg2 = 0;
    Tcl_Obj *pError;
    Tcl_HashEntry *pEntry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    int matchone = 0; /* True if the first argument matched something */
    int multiopt = 0; /* True if their were multiple options for second match */
    CONST char *zBad;

    if (objc>1) {
        zArg1 = Tcl_GetString(objv[1]);
    }
    if (objc>2) {
        zArg2 = Tcl_GetString(objv[2]);
    }

    /* Search the array of built-in commands */
    for (i=0; i<sizeof(aSubcommand)/sizeof(struct SC); i++) {
         struct SC *pCommand = &aSubcommand[i];
         if (zArg1 && 0==strcmp(zArg1, pCommand->zCmd1)) {
             matchone = 1;
             if (!pCommand->zCmd2 || 
                 (zArg2 && 0==strcmp(zArg2, pCommand->zCmd2))) 
             {
                 return pCommand->xFuncObj(clientData, interp, objc, objv);
             }
         }
    }

    /* See if this is a Tcl implemented command */
    pEntry = Tcl_FindHashEntry(&pTree->aCmd, zArg1);
    if (pEntry) {
        int rc;
        Tcl_Obj *pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        Tcl_Obj *pList = Tcl_NewListObj(objc - 2, &objv[2]);

        pScript = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pScript);
        Tcl_IncrRefCount(pList);

        Tcl_ListObjAppendList(interp, pScript, pList);
        rc = Tcl_EvalObjEx(interp, pScript, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);

        Tcl_DecrRefCount(pList);
        Tcl_DecrRefCount(pScript);

        return rc;
    }


    /* Failed to find a matching sub-command. The remainder of this routine
     * is generating an error message. 
     */
    zBad = matchone?zArg2:zArg1;
    pError = Tcl_NewStringObj("bad option \"", -1);
    Tcl_IncrRefCount(pError);
    Tcl_AppendToObj(pError, zBad, -1);
    Tcl_AppendToObj(pError, "\" must be ", -1);
    zBad = 0;
    for (i=0; i<sizeof(aSubcommand)/sizeof(struct SC); i++) {
        struct SC *pCommand = &aSubcommand[i];
        CONST char *zAdd = 0;

        if (matchone) { 
            if (0==strcmp(pCommand->zCmd1, zArg1)) {
                zAdd = pCommand->zCmd2;
            }
        } else if (!zBad || strcmp(pCommand->zCmd1, zBad)) {
            zAdd = pCommand->zCmd1;
        }

        if (zAdd) {
            if (zBad) {
                Tcl_AppendToObj(pError, zBad, -1);
                Tcl_AppendToObj(pError, ", ", -1);
                multiopt = 1;
            }
            zBad = zAdd;
        }
    }
    if (zBad) {
        if (multiopt) {
            Tcl_AppendToObj(pError, "or ", -1);
        }
        Tcl_AppendToObj(pError, zBad, -1);
    }
    Tcl_SetObjResult(interp, pError);
    Tcl_DecrRefCount(pError);
    
    return TCL_ERROR;
}



/*
 *---------------------------------------------------------------------------
 *
 * newWidget --
 *
 *     Create a new Html widget command:
 *
 *         html PATH ?<options>?
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
newWidget(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree;
    CONST char *zCmd;
    int rc;

    if (objc<2) {
        Tcl_WrongNumArgs(interp, 1, objv, "WINDOW-PATH ?OPTIONS?");
        return TCL_ERROR;
    }
    
    zCmd = Tcl_GetString(objv[1]);
    pTree = (HtmlTree *)HtmlClearAlloc(sizeof(HtmlTree));

    /* Create the Tk window.
     */
    pTree->win = Tk_MainWindow(interp);
    pTree->tkwin = Tk_CreateWindowFromPath(interp, pTree->win, zCmd, NULL); 
    if (!pTree->tkwin) {
        goto error_out;
    }
    Tk_SetClass(pTree->tkwin, "Html");

    pTree->interp = interp;
    Tcl_InitHashTable(&pTree->aScriptHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aNodeHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aScaledImage, TCL_STRING_KEYS);
    Tcl_InitHashTable(&pTree->aCmd, TCL_STRING_KEYS);
    Tcl_InitHashTable(&pTree->aVar, TCL_STRING_KEYS);
    pTree->cmd = Tcl_CreateObjCommand(interp,zCmd,widgetCmd,pTree,widgetCmdDel);

    /* TODO: Handle the case where configureCmd() returns an error. */
    rc = configureCmd(pTree, interp, objc, objv);
    assert(!pTree->options.logcmd);
    assert(!pTree->options.timercmd);

    /* Initialise the hash tables used by styler code */
    HtmlComputedValuesSetupTables(pTree);

    /* Set up an event handler for the widget window */
    Tk_CreateEventHandler(pTree->tkwin, 
            ExposureMask|StructureNotifyMask|VisibilityChangeMask, 
            eventHandler, (ClientData)pTree
    );

    /* Create the image-server */
    HtmlImageServerInit(pTree);

    /* Load the default style-sheet, ready for the first document. */
    doLoadDefaultStyle(pTree);

    /* Return the name of the widget just created. */
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;

    /* Exception handler. Jump here if an error occurs during
     * initialisation. An error message should already have been written
     * into the result of the interpreter.
     */
error_out:
    if (pTree->tkwin) {
        Tk_DestroyWindow(pTree->tkwin);
    }
    if (pTree) {
        HtmlFree((char *)pTree);
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * exitCmd --
 *
 *         ::tk::htmlexit
 *
 *     Call exit(0). This is included for when tkhtml is used in a starkit
 *     application. For reasons I don't understand yet, such deployments
 *     always seg-fault while cleaning up the main interpreter. Exiting the
 *     process this way avoids this unsightly seg-fault. Of course, this is
 *     a cosmetic fix only, the real problem is somewhere else.
 *
 * Results:
 *     None (does not return).
 *
 * Side effects:
 *     Exits process.
 *
 *---------------------------------------------------------------------------
 */
static int 
exitCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    exit(0);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * resolveCmd --
 *
 *         ::tk::htmlresolve HOST-NAME
 *
 *     Helper command for browser scripts that returns the dot-seperated ip
 *     address that corresponds to the HOST-NAME argument.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#include <netdb.h>
static int 
resolveCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    struct hostent *pHostent;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "HOST-NAME");
        return TCL_ERROR;
    }

    pHostent = gethostbyname(Tcl_GetString(objv[1]));
    if (!pHostent || pHostent->h_length < 1) {
        Tcl_SetObjResult(interp, objv[1]);
    } else {
	struct in_addr in;
        char *pAddr = pHostent->h_addr_list[0];
	memcpy(&in.s_addr, pAddr, sizeof(in.s_addr));
        Tcl_AppendResult(interp, inet_ntoa(in), 0);
    }

    return TCL_OK;
}

#ifndef NDEBUG
static int 
allocCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return Rt_AllocCommand(0, interp, objc, objv);
}
#endif

/*
 * Define the DLL_EXPORT macro, which must be set to something or other in
 * order to export the Tkhtml_Init and Tkhtml_SafeInit symbols from a win32
 * DLL file. I don't entirely understand the ins and outs of this, the
 * block below was copied verbatim from another program.
 */
#if INTERFACE
#define DLL_EXPORT
#endif
#if defined(USE_TCL_STUBS) && defined(__WIN32__)
# undef DLL_EXPORT
# define DLL_EXPORT __declspec(dllexport)
#endif
#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_Init --
 *
 *     Load the package into an interpreter.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Loads the tkhtml package into interpreter interp.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_Init(interp)
    Tcl_Interp *interp;
{
    int rc;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.3", 0) == 0) {
        return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.3", 0) == 0) {
        return TCL_ERROR;
    }
#endif

    Tcl_PkgProvide(interp, "Tkhtml", "3.0");
    Tcl_CreateObjCommand(interp, "html", newWidget, 0, 0);
    Tcl_CreateObjCommand(interp, "::tk::htmlexit", exitCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tk::htmlresolve", resolveCmd, 0, 0);

#ifndef NDEBUG
    Tcl_CreateObjCommand(interp, "::tk::htmlalloc", allocCmd, 0, 0);
#endif

    SwprocInit(interp);

    rc = Tcl_EvalEx(interp, HTML_DEFAULT_TCL, -1, TCL_EVAL_GLOBAL);
    assert(rc == TCL_OK);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_SafeInit --
 *
 *     Load the package into a safe interpreter.
 *
 *     Note that this function has to be below the Tkhtml_Init()
 *     implementation. Otherwise the Tkhtml_Init() invocation in this
 *     function counts as an implicit declaration which causes problems for
 *     MSVC somewhere on down the line.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Loads the tkhtml package into interpreter interp.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_SafeInit(interp)
    Tcl_Interp *interp;
{
    return Tkhtml_Init(interp);
}
