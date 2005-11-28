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
        "@(#) $Id: htmlparse.c,v 1.49 2005/11/28 13:27:37 danielk1977 Exp $";

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "html.h"

static void
AppendTextToken(pTree, pToken)
    HtmlTree *pTree;
    HtmlToken *pToken;
{
    if (!pTree->pTextFirst) {
        assert(!pTree->pTextLast);
        pTree->pTextFirst = pToken;
        pTree->pTextLast = pToken;
        pToken->pPrev = 0;
    } else {
        assert(pTree->pTextLast);
        pTree->pTextLast->pNext = pToken;
        pToken->pPrev = pTree->pTextLast;
        pTree->pTextLast = pToken;
    }
    pToken->pNext = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * AppendToken --
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
AppendToken(pTree, pToken)
    HtmlTree *pTree;
    HtmlToken *pToken;
{
    if (pTree->pTextFirst) {
        HtmlToken *pTextFirst = pTree->pTextFirst;
        HtmlToken *pTextLast = pTree->pTextLast;
        pTree->pTextLast = 0;
        pTree->pTextFirst = 0;

        HtmlAddToken(pTree, pTextFirst);
        if (pTree->pFirst) {
            assert(pTree->pLast);
            pTree->pLast->pNext = pTextFirst;
            pTextFirst->pPrev = pTree->pLast;
        } else {
            assert(!pTree->pLast);
            pTree->pFirst = pTextFirst;
        }
        pTree->pLast = pTextLast;
    }

    if (pToken) {
        pToken->pNext = 0;
        pToken->pPrev = 0;
        HtmlAddToken(pTree, pToken);
        if (pTree->pFirst) {
            assert(pTree->pLast);
            pTree->pLast->pNext = pToken;
            pToken->pPrev = pTree->pLast;
        } else {
            assert(!pTree->pLast);
            pTree->pFirst = pToken;
            pToken->pPrev = 0;
        }
        pTree->pLast = pToken;
    }
}

static void
AppendImplicitToken(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    HtmlNode *pCurrent = pTree->pCurrent;
    HtmlToken *pImplicit = (HtmlToken *)HtmlAlloc(sizeof(HtmlToken));
    memset(pImplicit, 0, sizeof(HtmlToken));
    pImplicit->type = tag;

    pTree->pCurrent = pNode;
    AppendToken(pTree, pImplicit);
    pTree->pCurrent = pCurrent;
}

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
    if (tag==Html_LI || tag==Html_EndLI) return TAG_OK;
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
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
    if (!(flags&HTMLTAG_INLINE)) {
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
static int 
HtmlColgroupContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    assert(0);
}

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
    if (
        tag==Html_EndTR ||
        tag==Html_EndTD || 
        tag==Html_EndTH ||
        tag==Html_TR    ||
        tag==Html_TD    ||
        tag==Html_TH    ||
        tag==Html_Space
    ) { 
        return TAG_OK;
    }

    if (!(HtmlMarkupFlags(tag) & HTMLTAG_END)) {
        AppendImplicitToken(pTree, pNode, Html_TR);
        return TAG_IMPLICIT;
    }

    return TAG_PARENT;
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
static int 
HtmlTableSectionContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    assert(0);
}

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
    if (
        tag == Html_FORM || 
        tag == Html_TD || 
        tag == Html_TH || 
        tag == Html_Space
    ) {
        return TAG_OK;
    }
    if (HtmlMarkupFlags(tag) & HTMLTAG_END) {
        return TAG_PARENT;
    }
    AppendImplicitToken(pTree, pNode, Html_TD);
    return TAG_IMPLICIT;
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
    if (!(HtmlMarkupFlags(tag) & HTMLTAG_END)) return TAG_OK;
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

static HtmlTokenMap *HtmlHashLookup(void *htmlPtr, CONST char *zType);

/****************** Begin Escape Sequence Translator *************/

/*
** The next section of code implements routines used to translate
** the '&' escape sequences of SGML to individual characters.
** Examples:
**
**         &amp;          &
**         &lt;           <
**         &gt;           >
**         &nbsp;         nonbreakable space
*/

/* Each escape sequence is recorded as an instance of the following
** structure
*/
struct sgEsc {
    char *zName;                       /* The name of this escape sequence.
                                        * ex: "amp" */
    char value[8];                     /* The value for this sequence.  ex:
                                        * "&" */
    struct sgEsc *pNext;               /* Next sequence with the same hash on 
                                        * zName */
};

/* The following is a table of all escape sequences.  Add new sequences
** by adding entries to this table.
*/
static struct sgEsc esc_sequences[] = {
    {"quot", "\"", 0},
    {"amp", "&", 0},
    {"lt", "<", 0},
    {"gt", ">", 0},
    {"nbsp", " ", 0},
    {"iexcl", "\302\241", 0},
    {"cent", "\302\242", 0},
    {"pound", "\302\243", 0},
    {"curren", "\302\244", 0},
    {"yen", "\302\245", 0},
    {"brvbar", "\302\246", 0},
    {"sect", "\302\247", 0},
    {"uml", "\302\250", 0},
    {"copy", "\302\251", 0},
    {"ordf", "\302\252", 0},
    {"laquo", "\302\253", 0},
    {"not", "\302\254", 0},
    {"shy", "\302\255", 0},
    {"reg", "\302\256", 0},
    {"macr", "\302\257", 0},
    {"deg", "\302\260", 0},
    {"plusmn", "\302\261", 0},
    {"sup2", "\302\262", 0},
    {"sup3", "\302\263", 0},
    {"acute", "\302\264", 0},
    {"micro", "\302\265", 0},
    {"para", "\302\266", 0},
    {"middot", "\302\267", 0},
    {"cedil", "\302\270", 0},
    {"sup1", "\302\271", 0},
    {"ordm", "\302\272", 0},
    {"raquo", "\302\273", 0},
    {"frac14", "\302\274", 0},
    {"frac12", "\302\275", 0},
    {"frac34", "\302\276", 0},
    {"iquest", "\302\277", 0},
    {"Agrave", "\303\200", 0},
    {"Aacute", "\303\201", 0},
    {"Acirc", "\303\202", 0},
    {"Atilde", "\303\203", 0},
    {"Auml", "\303\204", 0},
    {"Aring", "\303\205", 0},
    {"AElig", "\303\206", 0},
    {"Ccedil", "\303\207", 0},
    {"Egrave", "\303\210", 0},
    {"Eacute", "\303\211", 0},
    {"Ecirc", "\303\212", 0},
    {"Euml", "\303\213", 0},
    {"Igrave", "\303\214", 0},
    {"Iacute", "\303\215", 0},
    {"Icirc", "\303\216", 0},
    {"Iuml", "\303\217", 0},
    {"ETH", "\303\220", 0},
    {"Ntilde", "\303\221", 0},
    {"Ograve", "\303\222", 0},
    {"Oacute", "\303\223", 0},
    {"Ocirc", "\303\224", 0},
    {"Otilde", "\303\225", 0},
    {"Ouml", "\303\226", 0},
    {"times", "\303\227", 0},
    {"Oslash", "\303\230", 0},
    {"Ugrave", "\303\231", 0},
    {"Uacute", "\303\232", 0},
    {"Ucirc", "\303\233", 0},
    {"Uuml", "\303\234", 0},
    {"Yacute", "\303\235", 0},
    {"THORN", "\303\236", 0},
    {"szlig", "\303\237", 0},
    {"agrave", "\303\240", 0},
    {"aacute", "\303\241", 0},
    {"acirc", "\303\242", 0},
    {"atilde", "\303\243", 0},
    {"auml", "\303\244", 0},
    {"aring", "\303\245", 0},
    {"aelig", "\303\246", 0},
    {"ccedil", "\303\247", 0},
    {"egrave", "\303\250", 0},
    {"eacute", "\303\251", 0},
    {"ecirc", "\303\252", 0},
    {"euml", "\303\253", 0},
    {"igrave", "\303\254", 0},
    {"iacute", "\303\255", 0},
    {"icirc", "\303\256", 0},
    {"iuml", "\303\257", 0},
    {"eth", "\303\260", 0},
    {"ntilde", "\303\261", 0},
    {"ograve", "\303\262", 0},
    {"oacute", "\303\263", 0},
    {"ocirc", "\303\264", 0},
    {"otilde", "\303\265", 0},
    {"ouml", "\303\266", 0},
    {"divide", "\303\267", 0},
    {"oslash", "\303\270", 0},
    {"ugrave", "\303\271", 0},
    {"uacute", "\303\272", 0},
    {"ucirc", "\303\273", 0},
    {"uuml", "\303\274", 0},
    {"yacute", "\303\275", 0},
    {"thorn", "\303\276", 0},
    {"yuml", "\303\277", 0},
};

/* The size of the handler hash table.  For best results this should
** be a prime number which is about the same size as the number of
** escape sequences known to the system. */
#define ESC_HASH_SIZE (sizeof(esc_sequences)/sizeof(esc_sequences[0])+7)

/* The hash table 
**
** If the name of an escape sequences hashes to the value H, then
** apEscHash[H] will point to a linked list of Esc structures, one of
** which will be the Esc structure for that escape sequence.
*/
static struct sgEsc *apEscHash[ESC_HASH_SIZE];

/* Hash a escape sequence name.  The value returned is an integer
** between 0 and ESC_HASH_SIZE-1, inclusive.
*/
static int
EscHash(zName)
    const char *zName;
{
    int h = 0;                         /* The hash value to be returned */
    char c;                            /* The next character in the name
                                        * being hashed */

    while ((c = *zName) != 0) {
        h = h << 5 ^ h ^ c;
        zName++;
    }
    if (h < 0) {
        h = -h;
    }
    else {
    }
    return h % ESC_HASH_SIZE;
}

#ifdef TEST

/* 
** Compute the longest and average collision chain length for the
** escape sequence hash table
*/
static void
EscHashStats(void)
{
    int i;
    int sum = 0;
    int max = 0;
    int cnt;
    int notempty = 0;
    struct sgEsc *p;

    for (i = 0; i < sizeof(esc_sequences) / sizeof(esc_sequences[0]); i++) {
        cnt = 0;
        p = apEscHash[i];
        if (p)
            notempty++;
        while (p) {
            cnt++;
            p = p->pNext;
        }
        sum += cnt;
        if (cnt > max)
            max = cnt;
    }
    printf("Longest chain=%d  avg=%g  slots=%d  empty=%d (%g%%)\n",
           max, (double) sum / (double) notempty, i, i - notempty,
           100.0 * (i - notempty) / (double) i);
}
#endif

/* Initialize the escape sequence hash table
*/
static void
EscInit()
{
    int i;                             /* For looping thru the list of escape 
                                        * sequences */
    int h;                             /* The hash on a sequence */

    for (i = 0; i < sizeof(esc_sequences) / sizeof(esc_sequences[i]); i++) {

/* #ifdef TCL_UTF_MAX */
#if 0
        {
            int c = esc_sequences[i].value[0];
            Tcl_UniCharToUtf(c, esc_sequences[i].value);
        }
#endif
        h = EscHash(esc_sequences[i].zName);
        esc_sequences[i].pNext = apEscHash[h];
        apEscHash[h] = &esc_sequences[i];
    }
#ifdef TEST
    EscHashStats();
#endif
}

/*
** This table translates the non-standard microsoft characters between
** 0x80 and 0x9f into plain ASCII so that the characters will be visible
** on Unix systems.  Care is taken to translate the characters
** into values less than 0x80, to avoid UTF-8 problems.
*/
#ifndef __WIN32__
static char acMsChar[] = {
    /*
     * 0x80 
     */ 'C',
    /*
     * 0x81 
     */ ' ',
    /*
     * 0x82 
     */ ',',
    /*
     * 0x83 
     */ 'f',
    /*
     * 0x84 
     */ '"',
    /*
     * 0x85 
     */ '.',
    /*
     * 0x86 
     */ '*',
    /*
     * 0x87 
     */ '*',
    /*
     * 0x88 
     */ '^',
    /*
     * 0x89 
     */ '%',
    /*
     * 0x8a 
     */ 'S',
    /*
     * 0x8b 
     */ '<',
    /*
     * 0x8c 
     */ 'O',
    /*
     * 0x8d 
     */ ' ',
    /*
     * 0x8e 
     */ 'Z',
    /*
     * 0x8f 
     */ ' ',
    /*
     * 0x90 
     */ ' ',
    /*
     * 0x91 
     */ '\'',
    /*
     * 0x92 
     */ '\'',
    /*
     * 0x93 
     */ '"',
    /*
     * 0x94 
     */ '"',
    /*
     * 0x95 
     */ '*',
    /*
     * 0x96 
     */ '-',
    /*
     * 0x97 
     */ '-',
    /*
     * 0x98 
     */ '~',
    /*
     * 0x99 
     */ '@',
    /*
     * 0x9a 
     */ 's',
    /*
     * 0x9b 
     */ '>',
    /*
     * 0x9c 
     */ 'o',
    /*
     * 0x9d 
     */ ' ',
    /*
     * 0x9e 
     */ 'z',
    /*
     * 0x9f 
     */ 'Y',
};
#endif

/* Translate escape sequences in the string "z".  "z" is overwritten
** with the translated sequence.
**
** Unrecognized escape sequences are unaltered.
**
** Example:
**
**      input =    "AT&amp;T &gt MCI"
**      output =   "AT&T > MCI"
*/
void
HtmlTranslateEscapes(z)
    char *z;
{
    int from;                          /* Read characters from this position
                                        * in z[] */
    int to;                            /* Write characters into this position 
                                        * in z[] */
    int h;                             /* A hash on the escape sequence */
    struct sgEsc *p;                   /* For looping down the escape
                                        * sequence collision chain */
    static int isInit = 0;             /* True after initialization */

    from = to = 0;
    if (!isInit) {
        EscInit();
        isInit = 1;
    }
    while (z[from]) {
        if (z[from] == '&') {
            if (z[from + 1] == '#') {
                int i = from + 2;
                int v = 0;
                while (isdigit(z[i])) {
                    v = v * 10 + z[i] - '0';
                    i++;
                }
                if (z[i] == ';') {
                    i++;
                }

                /*
                 * On Unix systems, translate the non-standard microsoft **
                 * characters in the range of 0x80 to 0x9f into something **
                 * we can see. 
                 */
#ifndef __WIN32__
                if (v >= 0x80 && v < 0xa0) {
                    v = acMsChar[v & 0x1f];
                }
#endif
                /*
                 * Put the character in the output stream in place of ** the
                 * "&#000;".  How we do this depends on whether or ** not we
                 * are using UTF-8. 
                 */
#ifdef TCL_UTF_MAX
                {
                    int j, n;
                    char value[8];
                    n = Tcl_UniCharToUtf(v, value);
                    for (j = 0; j < n; j++) {
                        z[to++] = value[j];
                    }
                }
#else
                z[to++] = v;
#endif
                from = i;
            }
            else {
                int i = from + 1;
                int c;
                while (z[i] && isalnum(z[i])) {
                    i++;
                }
                c = z[i];
                z[i] = 0;
                h = EscHash(&z[from + 1]);
                p = apEscHash[h];
                while (p && strcmp(p->zName, &z[from + 1]) != 0) {
                    p = p->pNext;
                }
                z[i] = c;
                if (p) {
                    int j;
                    for (j = 0; p->value[j]; j++) {
                        z[to++] = p->value[j];
                    }
                    from = i;
                    if (c == ';') {
                        from++;
                    }
                }
                else {
                    z[to++] = z[from++];
                }
            }

            /*
             * On UNIX systems, look for the non-standard microsoft
             * characters ** between 0x80 and 0x9f and translate them into
             * printable ASCII ** codes.  Separate algorithms are required to 
             * do this for plain ** ascii and for utf-8. 
             */
#ifndef __WIN32__
#ifdef TCL_UTF_MAX
        }
        else if ((z[from] & 0x80) != 0) {
            Tcl_UniChar c;
            int n;
            n = Tcl_UtfToUniChar(&z[from], &c);
            if (c >= 0x80 && c < 0xa0) {
                z[to++] = acMsChar[c & 0x1f];
                from += n;
            }
            else {
                while (n--)
                    z[to++] = z[from++];
            }
#else /* if !defined(TCL_UTF_MAX) */
        }
        else if (((unsigned char) z[from]) >= 0x80
                 && ((unsigned char) z[from]) < 0xa0) {
            z[to++] = acMsChar[z[from++] & 0x1f];
#endif /* TCL_UTF_MAX */
#endif /* __WIN32__ */
        }
        else {
            z[to++] = z[from++];
        }
    }
    z[to] = 0;
}

/******************* End Escape Sequence Translator ***************/

/******************* Begin HTML tokenizer code *******************/

/*
** The following variable becomes TRUE when the markup hash table
** (stored in HtmlMarkupMap[]) is initialized.
*/
static int isInit = 0;

/* The hash table for HTML markup names.
**
** If an HTML markup name hashes to H, then apMap[H] will point to
** a linked list of sgMap structure, one of which will describe the
** the particular markup (if it exists.)
*/
static HtmlTokenMap *apMap[HTML_MARKUP_HASH_SIZE];

/* Hash a markup name
**
** HTML markup is case insensitive, so this function will give the
** same hash regardless of the case of the markup name.
**
** The value returned is an integer between 0 and HTML_MARKUP_HASH_SIZE-1,
** inclusive.
*/
static int
HtmlHash(htmlPtr, zName)
    void *htmlPtr;
    const char *zName;
{
    int h = 0;
    char c;
    while ((c = *zName) != 0) {
        if (isupper(c)) {
            c = tolower(c);
        }
        h = h << 5 ^ h ^ c;
        zName++;
    }
    if (h < 0) {
        h = -h;
    }
    return h % HTML_MARKUP_HASH_SIZE;
}

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

/* Initialize the escape sequence hash table
*/
static void
HtmlHashInit(htmlPtr, start)
    void *htmlPtr;
    int start;
{
    int i;                             /* For looping thru the list of markup 
                                        * names */
    int h;                             /* The hash on a markup name */

    for (i = start; i < HTML_MARKUP_COUNT; i++) {
        h = HtmlHash(htmlPtr, HtmlMarkupMap[i].zName);
        HtmlMarkupMap[i].pCollide = apMap[h];
        apMap[h] = &HtmlMarkupMap[i];
    }
#ifdef TEST
    HtmlHashStats(htmlPtr);
#endif
}


/*
 *---------------------------------------------------------------------------
 *
 * NextColumn --
 *
 *     Compute the new column index following the given character.
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
NextColumn(iCol, c)
    int iCol;
    char c;
{
    switch (c) {
        case '\n':
            return 0;
        case '\t':
            return (iCol | 7) + 1;
        default:
            return iCol + 1;
    }
    /*
     * NOT REACHED 
     */
}

/*
** Convert a string to all lower-case letters.
*/
void
ToLower(z)
    char *z;
{
    while (*z) {
        if (isupper(*z))
            *z = tolower(*z);
        z++;
    }
}

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
 * Tokenize --
 *
 *     Process as much of the input HTML as possible.  Construct new
 *     HtmlElement structures and appended them to the list.
 *
 * Results:
 *     Return the number of characters actually processed.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
Tokenize(pTree)
    HtmlTree *pTree;             /* The HTML widget doing the parsing */
{
    char *z;                     /* The input HTML text */
    int c;                       /* The next character of input */
    int n;                       /* Number of characters processed so far */
    int iCol;                    /* Local copy of HtmlTree.iCol */
    int i, j;                    /* Loop counters */
    int nByte;                   /* Space allocated for a single HtmlElement */
    int selfClose;               /* True for content free elements. Ex: <br/> */
    int argc;                    /* The number of arguments on a markup */
    HtmlTokenMap *pMap;          /* For searching the markup name hash table */
    char *zBuf;                  /* For handing out buffer space */
# define mxARG 200               /* Max parameters in a single markup */
    char *argv[mxARG];           /* Pointers to each markup argument. */
    int arglen[mxARG];           /* Length of each markup argument */

    Tcl_Obj *pScript = 0;
    HtmlToken *pScriptToken = 0;
    int rc;

    iCol = pTree->iCol;
    n = pTree->nParsed;
    z = Tcl_GetString(pTree->pDocument);

    while ((c = z[n]) != 0) {

        /* TODO: What is the significance of -64 and -128? BOM or something? */
        if ((signed char) c == -64 && (signed char) (z[n + 1]) == -128) {
            n += 2;
            continue;
        }

	/* If pScript is not NULL, then we are parsing a node that tkhtml
	 * treats as a "script". Essentially this means we will pass the
	 * entire text of the node to some user callback for processing and
	 * take no further action. So we just search through the text until
	 * we encounter </script>, </noscript> or whatever closing tag
	 * matches the tag that opened the script node.
         */
        if (pScript) {
            int nEnd, sqcnt;
            char zEnd[64];
            char *zScript;
            int nScript;
            Tcl_Obj *pEval;

            /* Figure out the string we are looking for as a end tag */
            sprintf(zEnd, "</%s>", HtmlMarkupName(pScriptToken->type));
            nEnd = strlen(zEnd);
          
            /* Skip through the input until we find such a string. We
             * respect strings quoted with " and ', so long as they do not
             * include new-lines.
             */
            zScript = &z[n];
            sqcnt = 0;
            for (i = n; z[i]; i++) {
                if (z[i] == '\'' || z[i] == '"')
                    sqcnt++;    /* Skip if odd # quotes */
                else if (z[i] == '\n')
                    sqcnt = 0;
                if (strnicmp(&z[i], zEnd, nEnd)==0 && (sqcnt%2)==0) {
                    nScript = i - n;
                    break;
                }
            }

            if (z[i] == 0) {
                goto incomplete;
            }

            /* Execute the script */
            pEval = Tcl_DuplicateObj(pScript);
            Tcl_IncrRefCount(pEval);
            Tcl_ListObjAppendElement(0,pEval,Tcl_NewStringObj(zScript,nScript));
            rc = Tcl_EvalObjEx(pTree->interp, pEval, TCL_EVAL_GLOBAL);
            Tcl_DecrRefCount(pEval);
            n += (nScript+nEnd);
 
            /* If the script executed successfully, append the output to
             * the document text (it will be the next thing tokenized).
             */
            if (rc==TCL_OK) {
                Tcl_Obj *pResult;
                Tcl_Obj *pTail;
                Tcl_Obj *pHead;

                pTail = Tcl_NewStringObj(&z[n], -1);
                pResult = Tcl_GetObjResult(pTree->interp);
                pHead = Tcl_NewStringObj(z, n);
                Tcl_IncrRefCount(pTail);
                Tcl_IncrRefCount(pResult);
                Tcl_IncrRefCount(pHead);

                Tcl_AppendObjToObj(pHead, pResult);
                Tcl_AppendObjToObj(pHead, pTail);
                
                Tcl_DecrRefCount(pTail);
                Tcl_DecrRefCount(pResult);
                Tcl_DecrRefCount(pTree->pDocument);
                pTree->pDocument = pHead;
                z = Tcl_GetString(pHead);
            } 
            Tcl_ResetResult(pTree->interp);

            pScript = 0;
            HtmlFree((char *)pScriptToken);
            pScriptToken = 0;
        }

        /*
         * White space 
         */
        else if (isspace(c)) {
            HtmlToken *pSpace;
            for (
                 i = 0;
                 (c = z[n + i]) != 0 && isspace(c) && c != '\n' && c != '\r';
                 i++
            );
            if (c == '\r' && z[n + i + 1] == '\n') {
                i++;
            }
            
            pSpace = (HtmlToken *)HtmlAlloc(sizeof(HtmlToken));
            pSpace->type = Html_Space;

            if (c == '\n' || c == '\r') {
                pSpace->x.newline = 1;
                pSpace->count = 1;
                i++;
                iCol = 0;
            }
            else {
                int iColStart = iCol;
                pSpace->x.newline = 0;
                for (j = 0; j < i; j++) {
                    iCol = NextColumn(iCol, z[n + j]);
                }
                pSpace->count = iCol - iColStart;
            }
            AppendTextToken(pTree, pSpace);
            n += i;
        }

        /*
         * Ordinary text 
         */
        else if (c != '<' || 
                 (!isalpha(z[n + 1]) && z[n + 1] != '/' && z[n + 1] != '!'
                  && z[n + 1] != '?')) {

            HtmlToken *pText;
            int nBytes;

            for (i = 1; (c = z[n + i]) != 0 && !isspace(c) && c != '<'; i++);
            if (c == 0) {
                goto incomplete;
            }

            nBytes = 1 + i + sizeof(HtmlToken) + (i%sizeof(char *));
            pText = (HtmlToken *)HtmlAlloc(nBytes);
            pText->type = Html_Text;
            pText->x.zText = (char *)&pText[1];
            strncpy(pText->x.zText, &z[n], i);
            pText->x.zText[i] = 0;
            AppendTextToken(pTree, pText);
            HtmlTranslateEscapes(pText->x.zText);
            pText->count = strlen(pText->x.zText);
            n += i;
            iCol += i;
        }

        /*
         * An HTML comment. Just skip it. DK: This should be combined
         * with the script case above to reduce the amount of code.
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
            for (j = 0; j < i + 3; j++) {
                iCol = NextColumn(iCol, z[n + j]);
            }
            n += i + 3;
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
            argc = 1;
            argv[0] = &z[n + 1];
            assert( c=='<' );

            /* Increment i until &z[n+i] is the first byte past the
             * end of the tag name. Then set arglen[0] to the length of
             * argv[0].
             */
            i = 0;
            do {
                i++;
                c = z[n + i];
            } while( c!=0 && !isspace(c) && c!='>' && (i<2 || c!='/') );
            arglen[0] = i - 1;
            i--;

            /* Now prepare to parse the markup attributes. Advance i until
             * &z[n+i] points to the first character of the first attribute,
             * the closing '>' character, the closing "/>" string
	     * of a self-closing tag, or the end of the document. If the end of
	     * the document is reached, bail out via the 'incomplete' 
	     * exception handler.
             */
            while (isspace(z[n + i])) {
                i++;
            }
            if (z[n + i] == 0) {
                goto incomplete;
            }

            /* This loop runs until &z[n+i] points to '>', "/>" or the
             * end of the document. The argv[] array is completely filled
             * by the time the loop exits.
             */
            while (
                (c = z[n+i]) != 0 &&          /* End of document */
                (c != '>') &&                 /* '>'             */
                (c != '/' || z[n+i+1] != '>') /* "/>"            */
            ){
                if (argc > mxARG - 3) {
                    argc = mxARG - 3;
                }

                /* Set the next element of the argv[] array to point at
                 * the attribute name. Then figure out the length of the
                 * attribute name by searching for one of ">", "=", "/>", 
                 * white-space or the end of the document.
                 */
                argv[argc] = &z[n+i];
                j = 0;
                while ((c = z[n + i + j]) != 0 && !isspace(c) && c != '>'
                       && c != '=' && (c != '/' || z[n + i + j + 1] != '>')) {
                    j++;
                }
                arglen[argc] = j;

                if (c == 0) {
                    goto incomplete;
                }
                i += j;

                while (isspace(c)) {
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
                while (isspace(c)) {
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
                         (c = z[n + i + j]) != 0 && !isspace(c) && c != '>';
                         j++) {
                    }
                    if (c == 0) {
                        goto incomplete;
                    }
                    arglen[argc] = j;
                    i += j;
                }
                argc++;
                while (isspace(z[n + i])) {
                    i++;
                }
            }
            if( c==0 ){
                goto incomplete;
            }

            /* If this was a self-closing tag, set selfClose to 1 and 
             * increment i so that &z[n+i] points to the '>' character.
             */
            if (c == '/') {
                i++;
                c = z[n + i];
                selfClose = 1;
            } else {
                selfClose = 0;
            }
            assert( c!=0 );

            for (j = 0; j < i + 1; j++) {
                iCol = NextColumn(iCol, z[n + j]);
            }
            n += i + 1;

            /* Look up the markup name in the hash table. If it is an unknown
             * tag, just ignore it by jumping to the next iteration of
             * the while() loop. The data in argv[] is discarded in this case.
             *
             * DK: We jump through hoops to pass a NULL-terminated string to 
             * HtmlHashLookup(). It would be easy enough to fix 
             * HtmlHashLookup() to understand a length argument.
             */
            if (!isInit) {
                HtmlHashInit(0, 0);
                isInit = 1;
            }
            c = argv[0][arglen[0]];
            argv[0][arglen[0]] = 0;
            pMap = HtmlHashLookup(0, argv[0]);
            argv[0][arglen[0]] = c;
            if (pMap == 0) {
                continue;
            }

          makeMarkupEntry: {
            /* If we get here, we need to allocate a structure to store
             * the markup element. 
             */
            HtmlToken *pMarkup;
            nByte = sizeof(HtmlToken);
            if (argc > 1) {
                nByte += sizeof(char *) * (argc + 1);
                for (j = 1; j < argc; j++) {
                    nByte += arglen[j] + 1;
                }
            }
            pMarkup = (HtmlToken *)HtmlAlloc(nByte);
            pMarkup->type = pMap->type;
            pMarkup->count = argc - 1;
            pMarkup->x.zArgs = 0;

            /* If the tag had attributes, then copy all the attribute names
             * and values into the space just allocated. Translate escapes
	     * on the way. The idea is that calling HtmlFree() on pToken frees
	     * the space used by the attributes as well as the HtmlToken.
             */
            if (argc > 1) {
                pMarkup->x.zArgs = (char **)&pMarkup[1];
                zBuf = (char *)&pMarkup->x.zArgs[argc + 1];
                for (j=1; j < argc; j++) {
                    pMarkup->x.zArgs[j-1] = zBuf;
                    zBuf += arglen[j]+1;

                    strncpy(pMarkup->x.zArgs[j-1], argv[j], arglen[j]);
                    pMarkup->x.zArgs[j - 1][arglen[j]] = 0;
                    HtmlTranslateEscapes(pMarkup->x.zArgs[j - 1]);
                    if ((j&1) == 1) {
                        ToLower(pMarkup->x.zArgs[j-1]);
                    }
                }
                pMarkup->x.zArgs[argc - 1] = 0;
            }

            pScript = getScriptHandler(pTree, pMarkup->type);
            if (!pScript) {
                /* No special handler for this markup. Just append it to the 
                 * list of all tokens. 
                 */
                AppendToken(pTree, pMarkup);
            } else {
                pScriptToken = pMarkup;
            }
          }

            /* If this is self-closing markup (ex: <br/> or <img/>) then
             * synthesize a closing token. 
             */
            if (selfClose && argv[0][0] != '/'
                && strcmp(&pMap[1].zName[1], pMap->zName) == 0) {
                selfClose = 0;
                pMap++;
                argc = 1;
                goto makeMarkupEntry;
            }
        }
    }

  incomplete:
    pTree->iCol = iCol;
    return n;
}

/************************** End HTML Tokenizer Code ***************************/

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

    if (!pTree->pDocument) {
        pTree->pDocument = Tcl_NewObj();
        Tcl_IncrRefCount(pTree->pDocument);
    }
    Tcl_AppendToObj(pTree->pDocument, zText, nText);

    pTree->nParsed = Tokenize(pTree);

    if (isFinal) {
        AppendToken(pTree, 0);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlHashLookup --
 *
 *     Look up an HTML tag name in the hash-table.
 *
 * Results: 
 *     Return the corresponding HtmlTokenMap if the tag name is recognized,
 *     or NULL otherwise.
 *
 * Side effects:
 *     May initialise the hash table from the autogenerated array
 *     in htmltokens.c (generated from tokenlist.txt).
 *
 *---------------------------------------------------------------------------
 */
static HtmlTokenMap * 
HtmlHashLookup(htmlPtr, zType)
    void *htmlPtr;
    const char *zType;          /* Null terminated tag name. eg. "br" */
{
    HtmlTokenMap *pMap;         /* For searching the markup name hash table */
    int h;                      /* The hash on zType */
    char buf[256];
    if (!isInit) {
        HtmlHashInit(htmlPtr, 0);
        isInit = 1;
    }
    h = HtmlHash(htmlPtr, zType);
    for (pMap = apMap[h]; pMap; pMap = pMap->pCollide) {
        if (stricmp(pMap->zName, zType) == 0) {
            return pMap;
        }
    }
    strncpy(buf, zType, 255);
    buf[255] = 0;

    return NULL;
}

/*
** Convert a markup name into a type integer
*/
int
HtmlNameToType(htmlPtr, zType)
    void *htmlPtr;
    char *zType;
{
    HtmlTokenMap *pMap = HtmlHashLookup(htmlPtr, zType);
    return pMap ? pMap->type : Html_Unknown;
}

/*
** Convert a type into a symbolic name
*/
const char *
HtmlTypeToName(htmlPtr, type)
    void *htmlPtr;
    int type;
{
    if (type >= Html_A && type < Html_TypeCount) {
        HtmlTokenMap *pMap = apMap[type - Html_A];
        return pMap->zName;
    }
    else {
        return "???";
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
char * HtmlMarkupArg(pToken, zTag, zDefault)
    HtmlToken *pToken;
    const char *zTag;
    char *zDefault;
{
    int i;
    if (pToken->type==Html_Space || pToken->type==Html_Text) {
        return 0;
    }
    for (i = 0; i < pToken->count; i += 2) {
        if (strcmp(pToken->x.zArgs[i], zTag) == 0) {
            return pToken->x.zArgs[i + 1];
        }
    }
    return zDefault;
}
