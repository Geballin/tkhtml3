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
static char const rcsid[] =
        "@(#) $Id: htmlparse.c,v 1.120 2007/11/15 05:16:24 danielk1977 Exp $";

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "html.h"

#define ISSPACE(x) isspace((unsigned char)(x))
#define ISALPHA(x) isalpha((unsigned char)(x))

/*
 * The following elements have optional opening and closing tags:
 *
 *     <tbody>
 *     <html>
 *     <head>
 *     <body>
 *
 * These have optional end tags:
 *
 *     <dd>
 *     <dt>
 *     <li>
 *     <option>
 *     <p>
 *
 *     <colgroup>
 *     <td>
 *     <th>
 *     <tr>
 *     <thead>
 *     <tfoot>
 *
 * The following functions:
 *
 *     * HtmlFormContent
 *     * HtmlInlineContent
 *     * HtmlFlowContent
 *     * HtmlColgroupContent
 *     * HtmlTableSectionContent
 *     * HtmlTableRowContent
 *     * HtmlDlContent
 *     * HtmlUlContent
 *     * HtmlPcdataContent
 *
 * Are used to detect implicit close tags in HTML documents.  When a markup
 * tag encountered, one of the above functions is called with the parent
 * node and new markup tag as arguments. Three return values are possible:
 *
 *     TAG_CLOSE
 *     TAG_OK
 *     TAG_PARENT
 *
 * If TAG_CLOSE is returned, then the tag closes the tag that opened the
 * parent node. If TAG_OK is returned, then it does not. If TAG_PARENT is
 * returned, then the same call is made using the parent of pNode.
 */

/*
 *---------------------------------------------------------------------------
 *
 * HtmlFormContent --
 *
 *     "Node content" callback for nodes generated by empty HTML tags. All
 *     tokens close this kind of node.
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
HtmlFormContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_TR || tag == Html_TD || tag == Html_TH) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPcdataContent --
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
HtmlPcdataContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_Space || tag == Html_Text) {
        return TAG_PARENT;
    }
    return TAG_CLOSE;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDlContent --
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
HtmlDlContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_DD || tag==Html_DT) return TAG_OK;
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    return TAG_PARENT;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlUlContent --
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
HtmlUlContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_LI) return TAG_OK;
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    return TAG_PARENT;
}

static int 
HtmlHeadContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_BODY || tag==Html_FRAMESET) return TAG_CLOSE;
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContent --
 *
 *     "Node content" callback for nodes that can only handle inline
 *     content. i.e. those generated by <p>. Return CLOSE if content is not
 *     inline, else PARENT.
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
HtmlInlineContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    Html_u8 flags = HtmlMarkupFlags(tag);
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;

    /* Quirks mode exception: <p> tags can contain <table> */
    if( 
        pTree->options.mode == HTML_MODE_QUIRKS && 
        HtmlNodeTagType(pNode) == Html_P && 
        tag == Html_TABLE 
    ){
        return TAG_OK;
    }

    if (!(flags&HTMLTAG_INLINE)) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlAnchorContent --
 *
 *     "Node content" callback for anchor nodes (<a>).
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
HtmlAnchorContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    /* Html_u8 flags = HtmlMarkupFlags(tag); */
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;

    /* The DTD says that the content of an A element is
     * restricted to "(%inline;)* -(A)". But in practice only
     * the second restriction ("-(A)") seems to apply.
     */
    if (/* !(flags&HTMLTAG_INLINE) || */ tag == Html_A) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlFlowContent --
 *
 *     The SGML specification says that some elements may only contain
 *     %flow items. %flow is either %block or %inline - i.e. only tags for
 *     which the HTMLTAG_INLINE or HTMLTAG_BLOCK flag is set.
 *
 *     We apply this rule to the following elements, which may only contain
 *     %flow and are also allowed implicit close tags - according to HTML
 *     4.01. This is a little scary, it's not clear right now how other
 *     rendering engines handle this.
 *
 *         * <li>
 *         * <td>
 *         * <th>
 *         * <dd>
 *         * <dt>
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int 
HtmlFlowContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    Html_u8 flags = HtmlMarkupFlags(tag);
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    if (!(flags&(HTMLTAG_INLINE|HTMLTAG_BLOCK|HTMLTAG_END))) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlColgroupContent --
 *
 *     Todo! <colgroup> is not supported yet so it doesn't matter so
 *     much... But when we do support it make sure it can be implicitly
 *     closed here.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int 
HtmlColgroupContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    assert(0);
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableContent --
 *
 *     No tags do an implicit close on <table>. But if there is a stray
 *     </tr> or </td> tag in the table somewhere, it cannot match a <tr> or
 *     <td> above the table node in the document hierachy.
 *
 *     This is specified nowhere I can find, but all the other rendering
 *     engines seem to do it. Unfortunately, this might not be the whole
 *     story...
 *
 *     Also, return TAG_OK for <tr>, <td> and <th> so that they do not
 *     close a like tag above the <table> node.
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
HtmlTableContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_TABLE) return TAG_CLOSE;
    return TAG_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableSectionContent --
 *
 *     Todo! This will be for managing implicit closes of <tbody>, <tfoot>
 *     and <thead>. But we don't support any of them yet so it isn't really
 *     a big deal.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int 
HtmlTableSectionContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    assert(0);
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableRowContent --
 *
 *     According to the SGML definition of HTML, a <tr> node should contain
 *     nothing but <td> and <th> elements. So perhaps we should return
 *     TAG_CLOSE unless 'tag' is a <td> a <th> or some kind of closing tag.
 *
 *     For now though, just return TAG_CLOSE for another <tr> tag, and
 *     TAG_PARENT otherwise. Todo: Need to check how other browsers handle
 *     this.
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
HtmlTableRowContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_TR) {
        return TAG_CLOSE;
    }

    return TAG_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableCellContent --
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
HtmlTableCellContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_TH || tag==Html_TD || tag==Html_TR) return TAG_CLOSE;
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLiContent --
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
HtmlLiContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_LI || tag==Html_DD || tag==Html_DT) return TAG_CLOSE;
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    return TAG_PARENT;
}

/* htmltokens.c is generated from source file tokenlist.txt during the
 * build process. It contains the HtmlMarkupMap constant array, declared as:
 *
 * HtmlTokenMap HtmlMarkupMap[] = {...};
 */
#include "htmltokens.c"

#ifdef TEST

/* 
** Compute the longest and average collision chain length for the
** markup hash table
*/
static void
HtmlHashStats(void * htmlPtr)
{
    int i;
    int sum = 0;
    int max = 0;
    int cnt;
    int notempty = 0;
    struct sgMap *p;

    for (i = 0; i < HTML_MARKUP_COUNT; i++) {
        cnt = 0;
        p = apMap[i];
        if (p)
            notempty++;
        while (p) {
            cnt++;
            p = p->pCollide;
        }
        sum += cnt;
        if (cnt > max)
            max = cnt;

    }
    printf("longest chain=%d  avg=%g  slots=%d  empty=%d (%g%%)\n",
           max, (double) sum / (double) notempty, i, i - notempty,
           100.0 * (i - notempty) / (double) i);
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * getScriptHandler --
 *
 *     If there is a script handler for tag type 'tag', return the Tcl_Obj*
 *     containing the script. Otherwise return NULL.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_Obj *
getScriptHandler(pTree, tag)
    HtmlTree *pTree;
    int tag;
{
    Tcl_HashEntry *pEntry;
    pEntry = Tcl_FindHashEntry(&pTree->aScriptHandler, (char *)tag);
    if (pEntry) {
        return (Tcl_Obj *)Tcl_GetHashValue(pEntry);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * findEndOfScript --
 *
 *     Search the input string for the end of a script block (i.e. a node
 *     for which a tkhtml script-handler callback is defined). The 
 *     script is said to end at the next occurence of the string:
 *
 *         "</NAME"
 *
 *     followed by either whitespace or a '>' character, where NAME is 
 *     replaced with the name of the opening tag (i.e.  "script" or "style").
 *     Case does not matter.
 *
 *     No account is given to quotation marks within the body of the
 *     script block.
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
findEndOfScript(eTag, z, pN)
    int eTag;                 /* Tag type for this block (i.e. Html_Script) */
    char const *z;            /* Input string */
    int *pN;                  /* IN/OUT: Current index in z */
{
    char zEnd[64];
    int nEnd;
    int ii;
    int nLen = (strlen(&z[*pN]) + *pN);

    /* Figure out the string we are looking for as an end tag */
    sprintf(zEnd, "</%s", HtmlMarkupName(eTag));
    nEnd = strlen(zEnd);

    for (ii = *pN; ii < (nLen - nEnd - 1); ii++) {
        if (
            strnicmp(&z[ii], zEnd, nEnd) == 0 &&
            (z[ii+nEnd] == '>' || ISSPACE(z[ii+nEnd]))
        ) {
            int nScript = ii - (*pN);
            ii += (nEnd + 1);
            *pN = ii;
            return nScript;
        }
    }

    return -1;
}

/*
 *---------------------------------------------------------------------------
 *
 * executeScript --
 *
 *     Execute a tkhtml script-handler callback.
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
executeScript(pTree, pCallback, pAttributes, zScript, nScript)
    HtmlTree *pTree;
    Tcl_Obj *pCallback;
    HtmlAttributes *pAttributes;
    const char *zScript;
    int nScript;
{
    Tcl_Obj *pAttr;
    Tcl_Obj *pEval;
    int jj;
    int rc;

    /* Create the attributes list */
    pAttr = Tcl_NewObj();
    Tcl_IncrRefCount(pAttr);
    for (jj = 0; pAttributes && jj < pAttributes->nAttr; jj++) {
        Tcl_Obj *pArg;
        pArg = Tcl_NewStringObj(pAttributes->a[jj].zName, -1);
        Tcl_ListObjAppendElement(0, pAttr, pArg);
        pArg = Tcl_NewStringObj(pAttributes->a[jj].zValue, -1);
        Tcl_ListObjAppendElement(0, pAttr, pArg);
    }

    /* Execute the script */
    pEval = Tcl_DuplicateObj(pCallback);
    Tcl_IncrRefCount(pEval);
    Tcl_ListObjAppendElement(0, pEval, pAttr);
    Tcl_ListObjAppendElement(0,pEval,Tcl_NewStringObj(zScript,nScript));
    rc = Tcl_EvalObjEx(pTree->interp, pEval, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(pEval);

    /* Free the attributes list */
    Tcl_DecrRefCount(pAttr);

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTokenize --
 *
 *     Process as much of the input HTML as possible. This results in 
 *     zero or more calls to the following functions:
 *
 *         xAddText()
 *         xAddElement()
 *         xAddClosingTag()
 *
 *     The input HTML may come from one of two sources. If argument zText 
 *     is not NULL, then it is a pointer to UTF-8 encoded HTML text. In
 *     this case, "script-handler" callbacks are not made, element types
 *     with script handlers are built into the tree..
 *
 *     If zText is NULL, then the input text is in the Tcl_Obj* at
 *     HtmlTree.pDocument, starting at byte HtmlTree.nParsed. These
 *     two variables may be modified by this function.
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlTokenize(pTree, zText, isFinal, xAddText, xAddElement, xAddClosing)
    HtmlTree *pTree;             /* The HTML widget doing the parsing */
    char const *zText;
    int isFinal;
    void (*xAddText)(HtmlTree *, HtmlTextNode *, int);
    void (*xAddElement)(HtmlTree *, int, const char *, HtmlAttributes *, int);
    void (*xAddClosing)(HtmlTree *, int, const char *, int);
{
    char *z;                     /* The input HTML text */
    int c;                       /* The next character of input */
    int n;                       /* Number of bytes processed so far */
    int i, j;                    /* Loop counters */
    int argc;                    /* The number of arguments on a markup */
    HtmlTokenMap *pMap;          /* For searching the markup name hash table */
# define mxARG 200               /* Max parameters in a single markup */
    char *argv[mxARG];           /* Pointers to each markup argument. */
    int arglen[mxARG];           /* Length of each markup argument */

    /* True if a leading line-break should be trimmed if the next node
     * is a text node.
     */
    int isTrimStart = 0;

    if (zText) {
        /* This is an [$html fragment] command */
        n = 0;
        z = (char *)zText;
    } else {
        /* This is an [$html parse] command */
        n = pTree->nParsed;
        z = Tcl_GetString(pTree->pDocument);
    }

    while ((c = z[n]) != 0) {
        /* assert(n <= strlen(z)); */
        
        /* TEXT, HTML Comment, TAG (opening or closing) */

        /* A text (or whitespace) node */
        if (c != '<' && c != 0) {
            int isTrimEnd = 0;
            for (i = 0; (c = z[n + i]) != 0 && c != '<'; i++);

            /* If the next tag is a </PRE>, then skip the final newline
             * of this text node by setting isTrimEnd to true. TODO: It
             * might be required to do this for some other types of tag
             * too - </XMP> etc.
             */
            if (c == '<') {
                int iTmp = n+i+1;
                while (ISSPACE(z[iTmp])) iTmp++;
                if (z[iTmp] == '/') {
                    int iTmp2;
                    iTmp++;
                    while (ISSPACE(z[iTmp])) iTmp++;
                    if( !z[iTmp] ) goto incomplete;
                    iTmp2 = iTmp;
                    while (ISALPHA(z[iTmp2])) iTmp2++;
                    if( !z[iTmp2] ) goto incomplete;
                    if( 0==strnicmp(&z[iTmp], "pre", iTmp2-iTmp) ){
                        isTrimEnd = 1;
                    }
                }
            }

            if (c || isFinal) {
                int ts = isTrimStart;
                HtmlTextNode *pTextNode = HtmlTextNew(i, &z[n], isTrimEnd, ts);
                xAddText(pTree, pTextNode, n);
                n += i;
            } else {
                goto incomplete;
            }
            isTrimStart = 0;
        }

        /* An HTML comment. Just skip it. Tkhtml uses the non-SGML (i.e.
         * defacto standard) version of HTML comments - they begin with
         * "<!--" and end with "-->".
         */
        else if (strncmp(&z[n], "<!--", 4) == 0) {
            for (i = 4; z[n + i]; i++) {
                if (z[n + i] == '-' && strncmp(&z[n + i], "-->", 3) == 0) {
                    break;
                }
            }
            if (z[n + i] == 0) {
                goto incomplete;
            }
            n += i + 3;
            isTrimStart = 0;
        }

        else if (
            pTree->options.parsemode == HTML_PARSEMODE_XML && 
            0 == strncmp(&z[n], "<![CDATA[", 9)
        ) {
            const char *zData = &z[n+9];
            int nData;
            for (i = 9; z[n + i]; i++) {
                if (z[n + i] == ']' && strncmp(&z[n + i], "]]>", 3) == 0) {
                    break;
                }
            }
            if (z[n + i] == 0) {
                goto incomplete;
            }
            n += i + 3;

            nData = i - 9;
            xAddText(pTree, HtmlTextNew(nData, zData, 0, 0), 0);

            isTrimStart = 0;
        }

        /* A markup tag (i.e "<p>" or <p color="red"> or </p>). We parse 
         * this into a vector of strings stored in the argv[] array. The
         * length of each string is stored in the corresponding element
         * of arglen[]. Variable argc stores the length of both arrays.
         *
         * The first element of the vector is the markup tag name (i.e. "p" 
         * or "/p"). Each attribute consumes two elements of the vector, 
         * the attribute name and the value.
         */
        else {
            /* At this point, &z[n] points to the "<" character that opens
             * a markup tag. Variable 'i' is used to record the current
             * position, relative to &z[n], while parsing the tags name
             * and attributes. The pointer to the tag name, argv[0], is 
             * therefore &z[n+1].
             */
            int isClosingTag = 0;
            int isSelfClosing = 0;
            int i = 1;
            int nStartScript = n;
            const char *zAtom = 0;
            int eType = 0;

            argc = 1;
            argv[0] = &z[n + 1];
            assert( c=='<' );

            /* Check if we are dealing with a closing tag. */
            if (*argv[0] == '/' && argv[0][1]) {
                isClosingTag = 1;
                argv[0]++;
                i = 2;
            }

            /* Increment i until &z[n+i] is the first byte past the
             * end of the tag name. Then set arglen[0] to the length of
             * argv[0].
             */
            do {
                i++;
                c = z[n + i];
            } while( c!=0 && !ISSPACE(c) && c!='>' && (i<2 || c!='/') );
            arglen[0] = i - 1 - isClosingTag;

            /* Now prepare to parse the markup attributes. Advance i until
             * &z[n+i] points to the first character of the first attribute,
             * the closing '>' character, the closing "/>" string
	     * of a self-closing tag, or the end of the document. If the end of
	     * the document is reached, bail out via the 'incomplete' 
	     * exception handler.
             */
            while (ISSPACE(z[n + i])) {
                i++;
            }
            if (z[n + i] == 0) {
                goto incomplete;
            }

            /* This loop runs until &z[n+i] points to '>', "/>" or the
             * end of the document. The argv[] array is completely filled
             * by the time the loop exits.
             */
            while ((c = z[n+i]) != 0 && c != '>'){
                if (argc > mxARG - 3) {
                    argc = mxARG - 3;
                }

                if (z[n+i] == '/') {
                    i++;
                    continue;
                }

                /* Set the next element of the argv[] array to point at
                 * the attribute name. Then figure out the length of the
                 * attribute name by searching for one of ">", "=", "/>", 
                 * white-space or the end of the document.
                 */
                argv[argc] = &z[n+i];

                j = 0;
                while (
                    (c = z[n + i + j]) != 0 && 
                    !ISSPACE(c) && c != '>' && c != '=' 
                ) {
                    j++;
                }
                arglen[argc] = j;

                if (c == 0) {
                    goto incomplete;
                }
                i += j;

                while (ISSPACE(c)) {
                    i++;
                    c = z[n + i];
                }
                if (c == 0) {
                    goto incomplete;
                }
                argc++;
                if (c != '=') {
                    argv[argc] = "";
                    arglen[argc] = 0;
                    argc++;
                    continue;
                }
                i++;
                c = z[n + i];
                while (ISSPACE(c)) {
                    i++;
                    c = z[n + i];
                }
                if (c == 0) {
                    goto incomplete;
                }
                if (c == '\'' || c == '"') {
                    int cQuote = c;
                    i++;
                    argv[argc] = &z[n + i];
                    for (j = 0; (c = z[n + i + j]) != 0 && c != cQuote; j++) {
                    }
                    if (c == 0) {
                        goto incomplete;
                    }
                    arglen[argc] = j;
                    i += j + 1;
                }
                else {
                    argv[argc] = &z[n + i];
                    for (j = 0;
                         (c = z[n + i + j]) != 0 && !ISSPACE(c) && c != '>';
                         j++) {
                    }
                    if (c == 0) {
                        goto incomplete;
                    }
                    arglen[argc] = j;
                    i += j;
                }
                argc++;
                while (ISSPACE(z[n + i])) {
                    i++;
                }
            }
            if( c==0 ){
                goto incomplete;
            }
            assert(c == '>');
            n += i + 1;

            if (pTree->options.parsemode > HTML_PARSEMODE_HTML) {
                for (i = n - 2; i>=0 && z[i] == ' '; i--);
                if (z[i] == '/') isSelfClosing = 1;
            }

            /* Look up the markup name in the hash table. If it is an unknown
             * tag, just ignore it by jumping to the next iteration of
             * the while() loop. The data in argv[] is discarded in this case.
             *
             * DK: We jump through hoops to pass a NULL-terminated string to 
             * HtmlHashLookup(). It would be easy enough to fix 
             * HtmlHashLookup() to understand a length argument.
             */
            HtmlHashInit(0, 0);
            c = argv[0][arglen[0]];
            argv[0][arglen[0]] = 0;
            pMap = HtmlHashLookup(0, argv[0]);
            if (pMap == 0) {
                Tcl_HashEntry *pEntry;
                int dummy;
                if (pTree->options.parsemode != HTML_PARSEMODE_XML){
                    argv[0][arglen[0]] = c;
                    continue;
                }
                pEntry = Tcl_CreateHashEntry(&pTree->aAtom, argv[0], &dummy);
                zAtom = Tcl_GetHashKey(&pTree->aAtom, pEntry);
                eType = 0;
            } else {
                zAtom = pMap->zName;
                eType = pMap->type;
            }
            argv[0][arglen[0]] = c;

            if (isClosingTag) {
                /* Closing tag (i.e. "</p>"). */
                xAddClosing(pTree, eType, zAtom, nStartScript);
            } else {

                char *zScript = 0;
                int nScript = 0;

                HtmlAttributes *pAttr;
                Tcl_Obj *pScript = 0;
                const char **zArgs = (const char **)(&argv[1]);
                pAttr = HtmlAttributesNew(argc - 1, zArgs, &arglen[1], 1);


                /* Unless a fragment is being parsed, search for a 
                 * script-handler for this element. Script handlers are
                 * never fired from within [$html fragment] commands.
                 */
                if (!zText) {
                    pScript = getScriptHandler(pTree, eType);
                }

                if (pScript || (pMap && pMap->flags & HTMLTAG_PCDATA)) {
                    zScript = &z[n];
                    nScript = findEndOfScript(eType, z, &n);
                    if (nScript < 0) {
                        n = nStartScript;
                        HtmlFree(pAttr);
                        goto incomplete;
                    }
                }

                if (!pScript) {

                    /* No special handler for this markup. Just append 
                     * it to the list of all tokens. 
                     */
                    assert(nStartScript >= 0);
                    xAddElement(pTree, eType, zAtom, pAttr, nStartScript);
                    if( pTree->eWriteState==HTML_WRITE_INHANDLERRESET ){
                        goto incomplete;
                    }
                    if (zScript) {
                        HtmlTextNode *pTextNode;
                        pTextNode = HtmlTextNew(nScript, zScript, 1, 1);
                        xAddText(pTree, pTextNode, n);
                        xAddClosing(pTree, eType, zAtom, n);
                    } else {
                        if (eType == Html_PRE) {
                            isTrimStart = 1;
                        }
                        if (isSelfClosing) {
                            xAddClosing(pTree, eType, zAtom, n);
                        }
                    }

                } else {
                    /* If pScript is not NULL, then we are parsing a node that
                     * tkhtml treats as a "script". Essentially this means we
                     * will pass the entire text of the node to some user
                     * callback for processing and take no further action. So
                     * we just search through the text until we encounter
                     * </script>, </noscript> or whatever closing tag matches
                     * the tag that opened the script node.
                     */
                    int rc;
                    HtmlCallbackRestyle(pTree, pTree->state.pCurrent);

                    assert(pTree->eWriteState == HTML_WRITE_NONE);
                    pTree->eWriteState = HTML_WRITE_INHANDLER;
                    pTree->iWriteInsert = n;
                    rc = executeScript(pTree, pScript, pAttr, zScript, nScript);

                    assert(
                        pTree->eWriteState == HTML_WRITE_INHANDLER || 
                        pTree->eWriteState == HTML_WRITE_INHANDLERWAIT ||
                        pTree->eWriteState == HTML_WRITE_INHANDLERRESET
                    );
                    switch (pTree->eWriteState) {
                        case HTML_WRITE_INHANDLER:
                            pTree->eWriteState = HTML_WRITE_NONE;
                            break;
                        case HTML_WRITE_INHANDLERWAIT:
                            pTree->eWriteState = HTML_WRITE_WAIT;
                            break;
                        case HTML_WRITE_INHANDLERRESET:
                            pTree->eWriteState = HTML_WRITE_NONE;
                            return 0;
                    }
                    z = Tcl_GetString(pTree->pDocument);

                    HtmlFree(pAttr);
                    isTrimStart = 0;

                    if (pTree->eWriteState == HTML_WRITE_WAIT) {
                        goto incomplete;
                    }
                }
            }
        }
    }

  incomplete:
    if (!zText && pTree->eWriteState != HTML_WRITE_INHANDLERRESET) {
        pTree->nParsed = n;
    }
    return n;
}

/************************** End HTML Tokenizer Code ***************************/

static int 
tokenizeWrapper(pTree, isFin, xAddText, xAddElement, xAddClosing)
    HtmlTree *pTree;             /* The HTML widget doing the parsing */
    int isFin;
    void (*xAddText)(HtmlTree *, HtmlTextNode *, int);
    void (*xAddElement)(HtmlTree *, int, const char *, HtmlAttributes *, int);
    void (*xAddClosing)(HtmlTree *, int, const char *, int);
{
    int rc;
    HtmlNode *pCurrent = pTree->state.pCurrent;

    assert(pTree->eWriteState == HTML_WRITE_NONE);
    HtmlCheckRestylePoint(pTree);

    HtmlCallbackRestyle(pTree, pCurrent ? pCurrent : pTree->pRoot);
    HtmlCallbackLayout(pTree, pCurrent);
    rc = HtmlTokenize(pTree, 0, isFin, xAddText, xAddElement, xAddClosing);
    if (pTree->isParseFinished && pTree->eWriteState==HTML_WRITE_NONE) {
        HtmlFinishNodeHandlers(pTree);
    }

    if (pTree->eWriteState != HTML_WRITE_INHANDLERRESET) {
        pCurrent = pTree->state.pCurrent;
        HtmlCallbackRestyle(pTree, pCurrent ? pCurrent : pTree->pRoot);
        
        /* The theory is that the above calls to CallbackRestyle() ensure that
         * any nodes added to the tree by HtmlTokenize() are styled in the next
         * idle callback. This call, which is a no-op in -DNDEBUG builds, 
         * checks if that is true.
         *
         * TODO. Each time an element is added to a foster-tree in htmltree.c
         * it calls HtmlCheckRestylePoint(). This is inefficient. But otherwise
         * the following assert() fails (HtmlCheckRestylePoint() is a complex
         * assert() function).
         */
        HtmlCheckRestylePoint(pTree);
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTokenizerAppend --
 *
 *     Append text to the tokenizer engine.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     This routine (actually the Tokenize() subroutine that is called
 *     by this routine) may invoke a callback procedure which could delete
 *     the HTML widget. 
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlTokenizerAppend(pTree, zText, nText, isFinal)
    HtmlTree *pTree;
    const char *zText;
    int nText;
    int isFinal;
{
    /* TODO: Add a flag to prevent recursive calls to this routine. */
    const char *z = zText;
    int n = nText;
    /* Tcl_DString utf8; */

    if (!pTree->pDocument) {
        pTree->pDocument = Tcl_NewObj();
        Tcl_IncrRefCount(pTree->pDocument);
        assert(!Tcl_IsShared(pTree->pDocument));
    }

    assert(!Tcl_IsShared(pTree->pDocument));
    Tcl_AppendToObj(pTree->pDocument, z, n);

    if (pTree->eWriteState == HTML_WRITE_NONE) {
        tokenizeWrapper(pTree, isFinal, 
            HtmlTreeAddText,
            HtmlTreeAddElement,
            HtmlTreeAddClosingTag
        );
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkupArg --
 *
 *     Lookup an argument in the given markup with the name given.
 *     Return a pointer to its value, or the given default
 *     value if it doesn't appear.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char * HtmlMarkupArg(pAttr, zTag, zDefault)
    HtmlAttributes *pAttr;
    const char *zTag;
    char *zDefault;
{
    int i;
    if (pAttr) {
        for (i = 0; i < pAttr->nAttr; i++) {
            if (strcmp(pAttr->a[i].zName, zTag) == 0) {
                return pAttr->a[i].zValue;
            }
        }
    }
    return zDefault;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWriteWait --
 *
 *     $widget write wait
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
HtmlWriteWait(pTree)
    HtmlTree *pTree;
{
    if (pTree->eWriteState != HTML_WRITE_INHANDLER) {
        char *zErr = "Cannot call [write wait] here";
        Tcl_SetResult(pTree->interp, zErr, TCL_STATIC);
        return TCL_ERROR;
    }

    pTree->eWriteState = HTML_WRITE_INHANDLERWAIT;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWriteText --
 *
 *     $widget write text HTML
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
HtmlWriteText(pTree, pText)
    HtmlTree *pTree;
    Tcl_Obj *pText;
{
    int iInsert = pTree->iWriteInsert;
  
    Tcl_Obj *pDocument = pTree->pDocument;
    Tcl_Obj *pHead;
    Tcl_Obj *pTail;

    if (pTree->eWriteState == HTML_WRITE_NONE) {
        char *zErr = "Cannot call [write text] here";
        Tcl_SetResult(pTree->interp, zErr, TCL_STATIC);
        return TCL_ERROR;
    }

    pHead = Tcl_NewStringObj(Tcl_GetString(pDocument), iInsert);
    pTail = Tcl_NewStringObj(&(Tcl_GetString(pDocument)[iInsert]), -1);

    Tcl_IncrRefCount(pHead);
    Tcl_AppendObjToObj(pHead, pText);
    Tcl_GetStringFromObj(pHead, &pTree->iWriteInsert);
    Tcl_AppendObjToObj(pHead, pTail);

    Tcl_DecrRefCount(pDocument);
    pTree->pDocument = pHead;
 
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWriteContinue --
 *
 *     $widget write continue
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
HtmlWriteContinue(pTree)
    HtmlTree *pTree;
{
    int eState = pTree->eWriteState;
    if (eState != HTML_WRITE_WAIT && eState != HTML_WRITE_INHANDLERWAIT) {
        char *zErr = "Cannot call [write continue] here";
        Tcl_SetResult(pTree->interp, zErr, TCL_STATIC);
        return TCL_ERROR;
    }

    switch (eState) {
        case HTML_WRITE_WAIT: {
            pTree->eWriteState = HTML_WRITE_NONE;
            tokenizeWrapper(pTree, pTree->isParseFinished, 
                HtmlTreeAddText,
                HtmlTreeAddElement,
                HtmlTreeAddClosingTag
            );
            break;
        }
        case HTML_WRITE_INHANDLERWAIT:
            pTree->eWriteState = HTML_WRITE_INHANDLER;
            break;
    }

    return TCL_OK;
}


