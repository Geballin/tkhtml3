/*
** Routines to implement the HTML widget commands
** $Revision: 1.2 $
**
** Copyright (C) 1997,1998 D. Richard Hipp
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.
** 
** You should have received a copy of the GNU Library General Public
** License along with this library; if not, write to the
** Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA  02111-1307, USA.
**
** Author contact information:
**   drh@acm.org
**   http://www.hwaci.com/drh/
*/
#include <tk.h>
#include <stdlib.h>
#include "htmlcmd.h"

/*
** WIDGET base ?URL?
**
** Set or query the base URL for the current document.
*/
int HtmlBaseCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  if( argc==2 ){
    Tcl_DString str;
    Tcl_DStringInit(&str);
    if( htmlPtr->zProtocol ){
      Tcl_DStringAppend(&str, htmlPtr->zProtocol, -1);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( htmlPtr->zHost ){
      Tcl_DStringAppend(&str, htmlPtr->zHost, -1);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( htmlPtr->zDir ){
      Tcl_DStringAppend(&str, htmlPtr->zDir, -1);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    Tcl_DStringResult(interp, &str);
  }else{
    HtmlChangeUrl(htmlPtr,argv[2]);
    TestPoint(0);
  }
  return TCL_OK;
}

/*
** WIDGET cget CONFIG-OPTION
**
** Retrieve the value of a configuration option
*/
int HtmlCgetCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  TestPoint(0);
  return Tk_ConfigureValue(interp, htmlPtr->tkwin, configSpecs,
		(char *) htmlPtr, argv[2], 0);
}

/*
** WIDGET clear
**
** Erase all HTML from this widget and clear the screen.  This is
** typically done before loading a new document.
*/
int HtmlClearCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlClear(htmlPtr);
  htmlPtr->flags |= REDRAW_TEXT | VSCROLL | HSCROLL;
  HtmlScheduleRedraw(htmlPtr);
  TestPoint(0);
  return TCL_OK;
}

/*
** WIDGET configure ?OPTIONS?
**
** The standard Tk configure command.
*/
int HtmlConfigCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  if (argc == 2) {
     TestPoint(0);
     return Tk_ConfigureInfo(interp, htmlPtr->tkwin, configSpecs,
        (char *) htmlPtr, (char *) NULL, 0);
  } else if (argc == 3) {
     TestPoint(0);
     return Tk_ConfigureInfo(interp, htmlPtr->tkwin, configSpecs,
        (char *) htmlPtr, argv[2], 0);
  } else {
     TestPoint(0);
     return ConfigureHtmlWidget(interp, htmlPtr, argc-2, argv+2,
                                TK_CONFIG_ARGV_ONLY);
  }
}

/*
** WIDGET href X Y
**
** Returns the URL on the hyperlink that is beneath the position X,Y.
** Returns {} if there is no hyperlink beneath X,Y.
*/
int HtmlHrefCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int x, y;
  char *z;

  if( Tcl_GetInt(interp, argv[2], &x) != TCL_OK 
   || Tcl_GetInt(interp, argv[3], &y) != TCL_OK
  ){
    TestPoint(0);
    return TCL_ERROR;
  }
  z = HtmlGetHref(htmlPtr, x + htmlPtr->xOffset, y + htmlPtr->yOffset);
  if( z ){
    Tcl_SetResult(interp, HtmlCompleteUrl(htmlPtr,z), TCL_DYNAMIC);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  return TCL_OK;
}

/*
** WIDGET names
**
** Returns a list of names associated with <a name=...> tags.
*/
int HtmlNamesCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  char *z;
  TestPoint(0);
  for(p=htmlPtr->pFirst; p; p=p->pNext){
    if( p->base.type!=Html_A ) continue;
    z = HtmlMarkupArg(p,"name",0);
    if( z ){
      Tcl_AppendElement(interp,z);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }
  return TCL_OK;
}

/*
** WIDGET parse HTML
**
** Appends the given HTML text to the end of any HTML text that may have
** been inserted by prior calls to this command.  Then it runs the
** tokenizer, parser and layout engine as far as possible with the
** text that is available.  The display is updated appropriately.
*/
int HtmlParseCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *endPtr;
  int died;
  endPtr = htmlPtr->pLast;
  Tcl_Preserve(htmlPtr);
  HtmlTokenizerAppend(htmlPtr, argv[2]);
  if( htmlPtr->zText==0 || htmlPtr->tkwin==0 ){
    Tcl_Release(htmlPtr);
    TestPoint(0);
    return TCL_OK;
  }
  if( endPtr ){
    if( endPtr->pNext ){
      HtmlAddStyle(htmlPtr, endPtr->pNext);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }else if( htmlPtr->pFirst ){
    htmlPtr->paraAlignment = ALIGN_None;
    htmlPtr->rowAlignment = ALIGN_None;
    htmlPtr->anchorFlags = 0;
    htmlPtr->inDt = 0;
    htmlPtr->anchorStart = 0;
    htmlPtr->formStart = 0;
    htmlPtr->innerList = 0;
    HtmlAddStyle(htmlPtr, htmlPtr->pFirst);
    TestPoint(0);
  }
  died = htmlPtr->tkwin==0;
  Tcl_Release(htmlPtr);
  if( !died ){
    htmlPtr->flags |= EXTEND_LAYOUT;
    HtmlScheduleRedraw(htmlPtr);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  return TCL_OK;
}

/*
** WIDGET xview ?SCROLL-OPTIONS...?
**
** Implements horizontal scrolling in the usual Tk way.
*/
int HtmlXviewCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  if( argc==2 ){
    HtmlComputeHorizontalPosition(htmlPtr,interp->result);
    TestPoint(0);
  }else{
    int count;
    double fraction;
    int maxX = htmlPtr->maxX;
    int w = HtmlUsableWidth(htmlPtr);
    int offset = htmlPtr->xOffset;
    int type = Tk_GetScrollInfo(interp,argc,argv,&fraction,&count);
    switch( type ){
      case TK_SCROLL_ERROR:
        TestPoint(0);
        return TCL_ERROR;
      case TK_SCROLL_MOVETO:
        offset = fraction * maxX;
        TestPoint(0);
        break;
      case TK_SCROLL_PAGES:
        offset += count * w;
        TestPoint(0);
        break;
      case TK_SCROLL_UNITS:
        offset += count * 20;
        TestPoint(0);
        break;
    }
    if( offset + w > maxX ){
      offset = maxX - w;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( offset < 0 ){
      offset = 0;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    HtmlHorizontalScroll(htmlPtr, offset);
  }
  return TCL_OK;
}

/*
** WIDGET yview ?SCROLL-OPTIONS...?
**
** Implements vertical scrolling in the usual Tk way, but with one
** enhancement.  If the argument is a single word, the widget looks
** for a <a name=...> tag with that word as the "name" and scrolls
** to the position of that tag.
*/
int HtmlYviewCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  if( argc==2 ){
    HtmlComputeVerticalPosition(htmlPtr,interp->result);
    TestPoint(0);
  }else if( argc==3 ){
    char *z;
    HtmlElement *p;
    for(p=htmlPtr->pFirst; p; p=p->pNext){
      if( p->base.type!=Html_A ) continue;
      z = HtmlMarkupArg(p,"name",0);
      if( z==0 ){
        TestPoint(0);
        continue;
      }
      if( strcmp(z,argv[2])!=0 ){
        TestPoint(0);
        continue;
      }
      HtmlVerticalScroll(htmlPtr, p->anchor.y);
      TestPoint(0);
      break;
    }
  }else{
    int count;
    double fraction;
    int maxY = htmlPtr->maxY;
    int h = HtmlUsableHeight(htmlPtr);
    int offset = htmlPtr->yOffset;
    int type = Tk_GetScrollInfo(interp,argc,argv,&fraction,&count);
    switch( type ){
      case TK_SCROLL_ERROR:
        TestPoint(0);
        return TCL_ERROR;
      case TK_SCROLL_MOVETO:
        offset = fraction * maxY;
        TestPoint(0);
        break;
      case TK_SCROLL_PAGES:
        offset += count * h;
        TestPoint(0);
        break;
      case TK_SCROLL_UNITS:
        offset += count * 10;
        TestPoint(0);
        break;
    }
    if( offset + h > maxY ){
      offset = maxY - h;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( offset < 0 ){
      offset = 0;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    HtmlVerticalScroll(htmlPtr, offset);
  }
  return TCL_OK;
}

/*
** WIDGET _su ID
**
** This routine is called by a Submit button on the canvas when the
** button is pressed.
*/
int HtmlPrivateSuCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlSubmit(htmlPtr, atoi(argv[2]) );
  TestPoint(0);
  return TCL_OK;
}

/*
** WIDGET _re ID
**
** This routine is called by a Reset button on the canvas when the
** button is pressed.
*/
int HtmlPrivateReCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlReset(htmlPtr, atoi(argv[2]) );
  TestPoint(0);
  return TCL_OK;
}

/*
** WIDGET _ff ID
**
** This routine is called to move the focus widget forward on
** the html canvas.
*/
int HtmlPrivateFfCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  TestPoint(0);
  return TCL_OK;
}

/*
** WIDGET _fb ID
**
** This routine is called to change the focus widget on the
** html canvas backwards.
*/
int HtmlPrivateFbCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  TestPoint(0);
  return TCL_OK;
}

/*
** WIDGET token handler TAG ?SCRIPT?
*/
int HtmlTokenHandlerCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int type = HtmlNameToType(argv[3]);
  if( type==Html_Unknown ){
    Tcl_AppendResult(interp,"unknown tag: \"", argv[3], "\"", 0);
    TestPoint(0);
    return TCL_ERROR;
  }
  if( argc==4 ){
    if( htmlPtr->zHandler[type]!=0 ){
      interp->result = htmlPtr->zHandler[type];
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }else{
    if( htmlPtr->zHandler[type]!=0 ){
      ckfree(htmlPtr->zHandler[type]);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    htmlPtr->zHandler[type] = ckalloc( strlen(argv[4]) + 1 );
    if( htmlPtr->zHandler[type] ){
      strcpy(htmlPtr->zHandler[type],argv[4]);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }
  return TCL_OK;
}

/*
** WIDGET index INDEX	
*/
int HtmlIndexCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  int i;

  if( HtmlGetIndex(htmlPtr, argv[2], &p, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[2], "\"", 0);
    TestPoint(0);
    return TCL_ERROR;
  }
  if( p ){
    sprintf(interp->result, "%d.%d", HtmlTokenNumber(p), i);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  return TCL_OK;
}

/* The pSelStartBlock and pSelEndBlock values have been changed.
** This routine's job is to loop over all HtmlBlocks and either
** set or clear the HTML_Selected bits in the .base.flags field
** as appropriate.  For every HtmlBlock where the bit changes,
** mark that block for redrawing.
*/
static void UpdateSelection(HtmlWidget *htmlPtr){
  int selected = 0;
  HtmlIndex tempIndex;
  HtmlBlock *pTempBlock;
  int temp;
  HtmlBlock *p;

  for(p=htmlPtr->firstBlock; p; p=p->pNext){
    if( p==htmlPtr->pSelStartBlock ){
      selected = 1;
      HtmlRedrawBlock(htmlPtr, p);
      TestPoint(0);
    }else if( !selected && p==htmlPtr->pSelEndBlock ){
      selected = 1;
      tempIndex = htmlPtr->selBegin;
      htmlPtr->selBegin = htmlPtr->selEnd;
      htmlPtr->selEnd = tempIndex;
      pTempBlock = htmlPtr->pSelStartBlock;
      htmlPtr->pSelStartBlock = htmlPtr->pSelEndBlock;
      htmlPtr->pSelEndBlock = pTempBlock;
      temp = htmlPtr->selStartIndex;
      htmlPtr->selStartIndex = htmlPtr->selEndIndex;
      htmlPtr->selEndIndex = temp;
      HtmlRedrawBlock(htmlPtr, p);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( p->base.flags & HTML_Selected ){
      if( !selected ){
        p->base.flags &= ~HTML_Selected;
        HtmlRedrawBlock(htmlPtr,p);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
    }else{
      if( selected ){
        p->base.flags |= HTML_Selected;
        HtmlRedrawBlock(htmlPtr,p);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
    }
    if( p==htmlPtr->pSelEndBlock ){
      selected = 0;
      HtmlRedrawBlock(htmlPtr, p);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }
}

/* Given the selection end-points in htmlPtr->selBegin
** and htmlPtr->selEnd, recompute pSelBeginBlock and
** pSelEndBlock, then call UpdateSelection to update the
** display.
**
** This routine should be called whenever the selection
** changes or whenever the set of HtmlBlock structures
** change.
*/
void HtmlUpdateSelection(HtmlWidget *htmlPtr, int forceUpdate){
  HtmlBlock *pBlock;
  int index;
  int needUpdate = forceUpdate;
  int temp;

  if( htmlPtr->selEnd.p==0 ){
    htmlPtr->selBegin.p = 0;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  HtmlIndexToBlockIndex(htmlPtr, htmlPtr->selBegin, &pBlock, &index);
  if( needUpdate || pBlock != htmlPtr->pSelStartBlock ){
    needUpdate = 1;
    HtmlRedrawBlock(htmlPtr, htmlPtr->pSelStartBlock);
    htmlPtr->pSelStartBlock = pBlock;
    htmlPtr->selStartIndex = index;
    TestPoint(0);
  }else if( index != htmlPtr->selStartIndex ){
    HtmlRedrawBlock(htmlPtr, pBlock);
    htmlPtr->selStartIndex = index;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  if( htmlPtr->selBegin.p==0 ){
    htmlPtr->selEnd.p = 0;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  HtmlIndexToBlockIndex(htmlPtr, htmlPtr->selEnd, &pBlock, &index);
  if( needUpdate || pBlock != htmlPtr->pSelEndBlock ){
    needUpdate = 1;
    HtmlRedrawBlock(htmlPtr, htmlPtr->pSelEndBlock);
    htmlPtr->pSelEndBlock = pBlock;
    htmlPtr->selEndIndex = index;
    TestPoint(0);
  }else if( index != htmlPtr->selEndIndex ){
    HtmlRedrawBlock(htmlPtr, pBlock);
    htmlPtr->selEndIndex = index;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  if( htmlPtr->pSelStartBlock 
  && htmlPtr->pSelStartBlock==htmlPtr->pSelEndBlock
  && htmlPtr->selStartIndex > htmlPtr->selEndIndex
  ){
    temp = htmlPtr->selStartIndex;
    htmlPtr->selStartIndex = htmlPtr->selEndIndex;
    htmlPtr->selEndIndex = temp;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  if( needUpdate ){
    UpdateSelection(htmlPtr);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
}

/*
** WIDGET selection set INDEX INDEX
*/
int HtmlSelectionSetCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv           /* List of all arguments */
){
  HtmlIndex selBegin, selEnd;

  if( HtmlGetIndex(htmlPtr, argv[3], &selBegin.p, &selBegin.i) ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    TestPoint(0);
    return TCL_ERROR;
  }
  if( HtmlGetIndex(htmlPtr, argv[4], &selEnd.p, &selEnd.i) ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[4], "\"", 0);
    TestPoint(0);
    return TCL_ERROR;
  }
  htmlPtr->selBegin = selBegin;
  htmlPtr->selEnd = selEnd;
  HtmlUpdateSelection(htmlPtr,0);
  TestPoint(0);
  return TCL_OK;
}

/*
** WIDGET selection clear
*/
int HtmlSelectionClearCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv           /* List of all arguments */
){
  htmlPtr->pSelStartBlock = 0;
  htmlPtr->pSelEndBlock = 0;
  htmlPtr->selBegin.p = 0;
  htmlPtr->selEnd.p = 0;
  UpdateSelection(htmlPtr);
  TestPoint(0);
  return TCL_OK;
}

/*
** Recompute the position of the insertion cursor based on the
** position in htmlPtr->ins.
*/
void HtmlUpdateInsert(HtmlWidget *htmlPtr){
  HtmlIndexToBlockIndex(htmlPtr, htmlPtr->ins, 
                        &htmlPtr->pInsBlock, &htmlPtr->insIndex);
  HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
  if( htmlPtr->insTimer==0 ){
    htmlPtr->insStatus = 0;
    HtmlFlashCursor(htmlPtr);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
}

/*
** WIDGET insert INDEX
*/
int HtmlInsertCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv           /* List of all arguments */
){
  HtmlIndex ins;
  if( argv[2][0]==0 ){
    HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
    htmlPtr->insStatus = 0;
    htmlPtr->pInsBlock = 0;
    htmlPtr->ins.p = 0;
    TestPoint(0);
  }else{
    if( HtmlGetIndex(htmlPtr, argv[2], &ins.p, &ins.i) ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[2], "\"", 0);
      TestPoint(0);
      return TCL_ERROR;
    }
    HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
    htmlPtr->ins = ins;
    HtmlUpdateInsert(htmlPtr);
    TestPoint(0);
  }
  return TCL_OK;
}

#ifdef DEBUG
/*
** WIDGET debug dump START END
*/
int HtmlDebugDumpCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart, *pEnd;
  int i;

  if( HtmlGetIndex(htmlPtr, argv[3], &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if( HtmlGetIndex(htmlPtr, argv[4], &pEnd, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[4], "\"", 0);
    return TCL_ERROR;
  }
  if( pStart ){
    HtmlPrintList(pStart,pEnd ? pEnd->base.pNext : 0);
  }
  return TCL_OK;
}

/*
** WIDGET debug testpt FILENAME
*/
int HtmlDebugTestPtCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlTestPointDump(argv[3]);
  return TCL_OK;
}
#endif
