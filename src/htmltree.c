
/*
 * HtmlTree.c ---
 *
 *     This file implements the tree structure that can be used to access
 *     elements of an HTML document.
 *
 * TODO: Copyright.
 */
static char rcsid[] = "@(#) $Id:";

#include "html.h"
#include <assert.h>
#include <string.h>

/*
 * The following functions:
 *
 *     * HtmlEmptyContent
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
#define TAG_CLOSE 1
#define TAG_PARENT 2
#define TAG_OK 3

/*
 *---------------------------------------------------------------------------
 *
 * isExplicitClose --
 *
 *     Return true if tag is the explicit closing tag for pNode.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int isExplicitClose(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    int opentag = pNode->pToken->type;
    return (tag==(opentag+1));
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
int HtmlDlContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_DD || tag==Html_DT) return TAG_OK;
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
int HtmlUlContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_LI || tag==Html_EndLI) return TAG_OK;
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlEmptyContent --
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
int HtmlEmptyContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    return TAG_CLOSE;
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
int HtmlInlineContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    Html_u8 flags = HtmlMarkupFlags(tag);
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
int HtmlFlowContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    Html_u8 flags = HtmlMarkupFlags(tag);
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
int HtmlColgroupContent(pNode, tag)
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
int HtmlTableContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    if (    tag==Html_EndTR || 
            tag==Html_EndTD ||
            tag==Html_EndTH ||
            tag==Html_TR    ||
            tag==Html_TD    ||
            tag==Html_TH
    ) { 
        return TAG_OK;
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
int HtmlTableSectionContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
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
int HtmlTableRowContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_TR) return TAG_CLOSE;
    return TAG_PARENT;
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
int 
HtmlTableCellContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_TH || tag==Html_TD) return TAG_CLOSE;
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
int 
HtmlLiContent(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_LI || tag==Html_DD || tag==Html_DT) return TAG_CLOSE;
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * isEndTag --
 *
 *     Check if token pToken closes the document node currently
 *     being constructed. The algorithm for detecting a closing tag works
 *     like this:
 *
 *         1. If the tag is the explicit closing tag for pNode (i.e. pNode
 *            was created by a <p> and tag is a </p>), return true.
 *         2. Call the content function assigned to pNode with the -content
 *            option in tokenlist.txt. If it returns TAG_CLOSE, return
 *            true. If it returns TAG_OK, return false.
 *         3. If the content function returned TAG_PARENT, set pNode to the
 *            parent of pNode and if pNode is not now NULL goto step 1. If
 *            it is NULL, return false.
 *
 * Results:
 *     True if pToken does close the current node, otherwise false.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
isEndTag(pNode, pToken)
    HtmlNode *pNode;
    HtmlToken *pToken;
{
    HtmlNode *pN;
    Html_u8 type;
    
    /* If pToken is NULL, this means the end of the token list has been
     * reached. i.e. Close everything.
     */
    if (!pToken) {
        return 1;
    }
    type = pToken->type;

    for (pN=pNode; pN; pN=HtmlNodeParent(pN)) {
        HtmlContentTest xClose; 

        /* Check for explicit close */
        if (isExplicitClose(pN, type)) {
            return 1;

        /* Check for implicit close */
        } else {
            HtmlContentTest xClose = HtmlMarkup(pN->pToken->type)->xClose;
            if (xClose) {
                switch (xClose(pN, type)) {
                    case TAG_OK:
                        return 0;
                    case TAG_CLOSE:
                        return 1;
                }
            }
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * buildNode --
 *
 *     Build a document node from the element pointed to by pStart.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
buildNode(pTree, pParent, pStart, ppNext, ppNode, expect_inline)
    HtmlTree *pTree;             /* Tree the node belongs to */
    HtmlNode *pParent;           /* Parent of this node */
    HtmlToken *pStart;           /* Start token */
    HtmlToken **ppNext;          /* OUT: Next token for parent to process. */
    HtmlNode **ppNode;           /* OUT: The node constructed (if any) */
    int expect_inline;           /* True if this is an inline context */
{
    HtmlNode *pNode;                       /* The new node */
    HtmlToken *pNext = pStart;

    Html_u8 opentype = pStart->type;
    Html_u8 flags = HtmlMarkupFlags((int)opentype);

    /* If this function is called with a closing tag, do not create a new
     * node, just advance to the next token.
     */
    if (flags&HTMLTAG_END) {
        *ppNode = 0;
        *ppNext = pNext->pNext;
        return TCL_OK;
    }

    /* Allocate the node itself. If required, we change the size of the
     * allocation using ckrealloc() below.
     */
    pNode = (HtmlNode *)ckalloc(sizeof(HtmlNode));
    memset(pNode, 0, sizeof(HtmlNode));
    pNode->pToken = pStart;
    pNode->pParent = pParent;
    pNext = pStart;

    /* If the HTMLTAG_EMPTY flag is true for this kind of markup, then
     * the node consists of a single element only. An easy case. Simply
     * advance the iterator to the next token.
     */
    if( flags&HTMLTAG_EMPTY ){
        pNext = pNext->pNext;
    }

    /* If the element this document node points to is of type Text or
     * Space, then advance pNext until it points to an element of type
     * other than Text or Space. Only a single node is required for
     * a contiguous list of such elements.
     */
    else if( opentype==Html_Text || opentype==Html_Space ){
        while (pNext && 
              (pNext->type==Html_Text || pNext->type==Html_Space)
        ){
            pNext = pNext->pNext;
        }
    }

    /* We must be dealing with a non-empty markup tag. */
    else {
        Html_u8 closetype = opentype+1;
        assert( HtmlMarkupFlags(closetype)&HTMLTAG_END );
 
        pNext = pStart->pNext;
        while (!isEndTag(pNode, pNext)) {
            int n = (pNode->nChild+1)* sizeof(HtmlNode);
            pNode->apChildren = (HtmlNode **)
                    ckrealloc((char *)pNode->apChildren, n);
            buildNode(pTree, pNode, pNext, &pNext, 
                    &pNode->apChildren[pNode->nChild], expect_inline);
            if (pNode->apChildren[pNode->nChild]) {
                pNode->nChild++;
            }
        }

        if (pNext && isExplicitClose(pNode, pNext->type)) {
            pNext = pNext->pNext;
        }
    }

    *ppNext = pNext;
    *ppNode = pNode;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * freeNode --
 *
 *     Free the memory allocated for pNode and all of it's children. If the
 *     node has attached style information, either from stylesheets or an
 *     Html style attribute, this is deleted here too.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     pNode and children are made invalid.
 *
 *---------------------------------------------------------------------------
 */
static void 
freeNode(pNode)
    HtmlNode *pNode;
{
    if( pNode ){
        int i;
        for(i=0; i<pNode->nChild; i++){
            freeNode(pNode->apChildren[i]);
        }
        HtmlCssPropertiesFree(pNode->pProperties);
        HtmlCssPropertiesFree(pNode->pStyle);
        ckfree((char *)pNode->apChildren);
        ckfree((char *)pNode);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeFree --
 *
 *     Delete the internal tree representation.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Deletes the tree stored in p->pTree, if any. p->pTree is set to 0.
 *
 *---------------------------------------------------------------------------
 */
void HtmlTreeFree(pTree)
    HtmlTree *pTree;
{
    if( pTree->pRoot ){
        freeNode(pTree->pRoot);
    }
    pTree->pRoot = 0;
    pTree->pCurrent = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * treeBuild --
 *
 *     Build the document tree using the linked list of tokens currently
 *     stored in the widget. 
 *
 * Results:
 *
 * Side effects:
 *     The current tree is deleted. p->pTree is set to point at the
 *     new tree.
 *
 *---------------------------------------------------------------------------
 */
static int 
treeBuild(pTree)
    HtmlTree *pTree;
{
    HtmlToken *pStart = pTree->pFirst;
    HtmlTreeFree(pTree);

    /* We need to force the root of the document to be an <html> tag. 
     * So skip over all the white-space at the start of the document. If
     * the first thing we strike is not an <html> tag, then insert
     * an artficial one.
     *
     * TODO: Need to construct the other implicit tags, <head> and
     *       <body>, if they are not missing. This should be done
     *       in buildNode() though, not here. Maybe the <html> element
     *       should be dealt with there as well.
     */
    while( pStart && pStart->type==Html_Space ){
        pStart = pStart->pNext;
    }
    assert(pStart);
#if 0
    if( !pStart || pStart->base.type!=Html_HTML ){
        /* Allocate HtmlTree and a pretend <html> token */
        int n = sizeof(HtmlTree) + sizeof(HtmlBaseElement);
        HtmlBaseElement *pHtml;
        p->pTree = (HtmlTree *)ckalloc(n);
        memset(p->pTree, 0, n);
        pHtml = (HtmlBaseElement *)&p->pTree[1];
        pHtml->pNext = pStart;
        pHtml->type = Html_HTML;
        pStart = (HtmlElement *)pHtml;
    }else{
        /* Allocate just the HtmlTree. */
        int n = sizeof(HtmlTree);
        p->pTree = (HtmlTree *)ckalloc(n);
        memset(p->pTree, 0, n);
    }
#endif

    buildNode(pTree, 0, pStart, &pTree->pCurrent, &pTree->pRoot, 0);
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * walkTree --
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
walkTree(pTree, xCallback, pNode)
    HtmlTree *pTree;
    int (*xCallback)(HtmlTree *, HtmlNode *);
    HtmlNode *pNode;
{
    int i;
    if( pNode ){
        for (i = 0; i<pNode->nChild; i++) {
            int rc = walkTree(pTree, xCallback, pNode->apChildren[i]);
            if (rc) return rc;
        }
        xCallback(pTree, pNode);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWalkTree --
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
HtmlWalkTree(pTree, xCallback)
    HtmlTree *pTree;
    int (*xCallback)(HtmlTree *, HtmlNode *);
{
    if( !pTree->pRoot ){
        treeBuild(pTree);
    }
    return walkTree(pTree, xCallback, pTree->pRoot);
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeHandlerCallbacks --
 *
 *     This is called for every tree node by HtmlWalkTree() immediately
 *     after the document tree is constructed. It calls the node handler
 *     script for the node, if one exists.
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
nodeHandlerCallbacks(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    Tcl_HashEntry *pEntry;
    int tag;
    Tcl_Interp *interp = pTree->interp;

    tag = HtmlNodeTagType(pNode);
    pEntry = Tcl_FindHashEntry(&pTree->aNodeHandler, (char *)tag);
    if (pEntry) {
        Tcl_Obj *pEval;
        Tcl_Obj *pScript;
        Tcl_Obj *pNodeCmd;

        pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        pEval = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pEval);

        pNodeCmd = HtmlNodeCommand(interp, pNode); 
        Tcl_ListObjAppendElement(0, pEval, pNodeCmd);
        Tcl_EvalObjEx(interp, pEval, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);

        Tcl_DecrRefCount(pEval);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeBuild --
 *
 *     Construct the internal representation of the document tree.
 *     This is a front-end to treeBuild().
 *
 *     Tcl: $widget tree build
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Modify HtmlWidget.pTree
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlTreeBuild(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    treeBuild(pTree);
    HtmlWalkTree(pTree, nodeHandlerCallbacks);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeRoot --
 *
 *     $widget tree root
 *
 *     Returns the node command for the root node of the document. Or, if
 *     the tree has not been built, throws an error.
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
HtmlTreeRoot(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if (!pTree->pRoot) {
        Tcl_SetResult(interp, "", TCL_STATIC);
    } else {
        Tcl_Obj *pCmd = HtmlNodeCommand(interp, pTree->pRoot);
        Tcl_SetObjResult(interp, pCmd);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeNumChildren --
 *
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlNodeNumChildren(pNode)
    HtmlNode *pNode;
{
    return pNode->nChild;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeChild --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode * 
HtmlNodeChild(pNode, n)
    HtmlNode *pNode;
    int n;
{
    if (!pNode || pNode->nChild<=n) return 0;
    return pNode->apChildren[n];
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeIsText --
 *
 *     Test if a node is a text node.
 *
 * Results:
 *     Non-zero if the node is text, else zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlNodeIsText(pNode)
    HtmlNode *pNode;
{
    int type = HtmlNodeTagType(pNode);
    return (type==Html_Text || type==Html_Space);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeTagType --
 *
 *     Return the tag-type of the node, i.e. Html_P, Html_Text or
 *     Html_Space.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Html_u8 HtmlNodeTagType(pNode)
    HtmlNode *pNode;
{
    if (pNode && pNode->pToken) {
        return pNode->pToken->type;
    } 
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeRightSibling --
 * 
 *     Get the right-hand sibling to a node, if it has one.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *HtmlNodeRightSibling(pNode)
    HtmlNode *pNode;
{
    HtmlNode *pParent = pNode->pParent;
    if( pParent ){
        int i;
        for (i=0; i<pParent->nChild-1; i++) {
            if (pNode==pParent->apChildren[i]) {
                return pParent->apChildren[i+1];
            }
        }
        assert(pNode==pParent->apChildren[pParent->nChild-1]);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeParent --
 *
 *     Get the parent of the current node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *HtmlNodeParent(pNode)
    HtmlNode *pNode;
{
    return pNode?pNode->pParent:0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAttr --
 *
 *     Return a pointer to the value of node attribute zAttr. Attributes
 *     are always represented as NULL-terminated strings.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char CONST *HtmlNodeAttr(pNode, zAttr)
    HtmlNode *pNode; 
    char CONST *zAttr;
{
    if (pNode) {
        return HtmlMarkupArg(pNode->pToken, zAttr, 0);
    }
    return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * nodeCommand --
 *
 *     $node tag
 *     $node attr HTML-ATTRIBUTE-NAME
 *     $node nChildren 
 *     $node child CHILD-NUMBER 
 *     $node parent
 *     $node text
 *
 *     This function is the implementation of the Tcl node command. A
 *     pointer to the HtmlNode struct is passed as clientData.
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
nodeCommand(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    HtmlNode *pNode = (HtmlNode *)clientData;
    int choice;

    static CONST char *NODE_strs[] = {
        "attr", "tag", "nChildren", "child", "text", 
        "parent", 0
    };
    enum NODE_enum {
        NODE_ATTR, NODE_TAG, NODE_NCHILDREN, NODE_CHILD, NODE_TEXT,
        NODE_PARENT
    };

    if (objc<2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], NODE_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    switch ((enum NODE_enum)choice) {
        case NODE_ATTR: {
            char CONST *zAttr;
            if (objc!=3) {
                Tcl_WrongNumArgs(interp, 2, objv, "ATTRIBUTE");
                return TCL_ERROR;
            }
            zAttr = HtmlNodeAttr(pNode, Tcl_GetString(objv[2]));
            if (zAttr==0) {
                zAttr = "";
            }
            Tcl_SetResult(interp, (char *)zAttr, TCL_VOLATILE);
            break;
        }
        case NODE_TAG: {
            char CONST *zTag;
            if (objc!=2) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            zTag = HtmlMarkupName(HtmlNodeTagType(pNode));
            Tcl_SetResult(interp, (char *)zTag, TCL_VOLATILE);
            break;
        }
        case NODE_NCHILDREN: {
            if (objc!=2) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            Tcl_SetObjResult(interp, Tcl_NewIntObj(HtmlNodeNumChildren(pNode)));
            break;
        }
        case NODE_CHILD: {
            Tcl_Obj *pCmd;
            int n;
            if (objc!=3) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            if (TCL_OK!=Tcl_GetIntFromObj(interp, objv[2], &n)) {
                return TCL_ERROR;
            }
            if (n>=HtmlNodeNumChildren(pNode) || n<0) {
                Tcl_SetResult(interp, "Parameter out of range", TCL_STATIC);
                return TCL_ERROR;
            }
            pCmd = HtmlNodeCommand(interp, HtmlNodeChild(pNode, n));
            Tcl_SetObjResult(interp, pCmd);
            break;
        }
        case NODE_TEXT: {
            int tag;
            int space_ok = 0;
            HtmlToken *pToken;
            Tcl_Obj *pRet = Tcl_NewObj();

            Tcl_IncrRefCount(pRet);
            pToken = pNode->pToken;
            while (pToken && 
                    (pToken->type==Html_Space || pToken->type==Html_Text)) {
                if (pToken->type==Html_Text) {
                    Tcl_AppendToObj(pRet, pToken->x.zText, pToken->count);
                    space_ok = 1;
                } else {
                    Tcl_AppendToObj(pRet, " ", 1);
                    space_ok = 0;
                }
                pToken = pToken->pNext;
            }

            Tcl_SetObjResult(interp, pRet);
            Tcl_DecrRefCount(pRet);
            break;
        }

        case NODE_PARENT: {
            HtmlNode *pParent;
            pParent = HtmlNodeParent(pNode);
            if (pParent) {
                Tcl_SetObjResult(interp, HtmlNodeCommand(interp, pParent));
            } 
            break;
        }

        default:
            assert(!"Impossible!");
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeCommand --
 *
 *     Return a Tcl object containing the name of the Tcl command used to
 *     access pNode. If the command does not already exist it is created.
 *
 *     The Tcl_Obj * returned is always a pointer to pNode->pCommand.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj *
HtmlNodeCommand(interp, pNode)
    Tcl_Interp *interp;
    HtmlNode *pNode;
{
    static int nodeNumber = 0;
    Tcl_Obj *pCmd = pNode->pCommand;

    if (!pCmd) {
        char zBuf[100];
        sprintf(zBuf, "::tkhtml::node%d", nodeNumber++);

        pCmd = Tcl_NewStringObj(zBuf, -1);
        Tcl_IncrRefCount(pCmd);
        Tcl_CreateObjCommand(interp, zBuf, nodeCommand, pNode, 0);
        pNode->pCommand = pCmd;
    }

    return pCmd;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeToString --
 *
 *     Return a human-readable string representation of pNode in memory
 *     allocated by ckfree(). This function is only used for debugging.
 *     Code to build string representations of nodes for other purposes
 *     should be done in Tcl using the node-command interface.
 *
 * Results:
 *     Pointer to string allocated by ckalloc().
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char * 
HtmlNodeToString(pNode)
    HtmlNode *pNode;
{
    int len;
    char *zStr;
    int tag;

    Tcl_Obj *pStr = Tcl_NewObj();
    Tcl_IncrRefCount(pStr);

    tag = HtmlNodeTagType(pNode);

    if (tag==Html_Text || tag==Html_Space) {
        HtmlToken *pToken;
        pToken = pNode->pToken;

        Tcl_AppendToObj(pStr, "\"", -1);
        while (pToken && (pToken->type==Html_Text||pToken->type==Html_Space)) {
            if (pToken->type==Html_Space) {
                int i;
                for (i=0; i<(pToken->count - (pToken->x.newline?1:0)); i++) {
                    Tcl_AppendToObj(pStr, " ", 1);
                }
                if (pToken->x.newline) {
                    Tcl_AppendToObj(pStr, "<nl>", 4);
                }
            } else {
                Tcl_AppendToObj(pStr, pToken->x.zText, pToken->count);
            }
            pToken = pToken->pNext;
        }
        Tcl_AppendToObj(pStr, "\"", -1);

    } else {
        int i;
        HtmlToken *pToken = pNode->pToken;
        Tcl_AppendToObj(pStr, "<", -1);
        Tcl_AppendToObj(pStr, HtmlMarkupName(tag), -1);
        for (i = 0; i < pToken->count; i += 2) {
            Tcl_AppendToObj(pStr, " ", -1);
            Tcl_AppendToObj(pStr, pToken->x.zArgs[i], -1);
            Tcl_AppendToObj(pStr, "=\"", -1);
            Tcl_AppendToObj(pStr, pToken->x.zArgs[i+1], -1);
            Tcl_AppendToObj(pStr, "\"", -1);
        }
        Tcl_AppendToObj(pStr, ">", -1);
    }

    /* Copy the string from the Tcl_Obj* to memory obtained via ckalloc().
     * Then release the reference to the Tcl_Obj*.
     */
    Tcl_GetStringFromObj(pStr, &len);
    zStr = ckalloc(len+1);
    strcpy(zStr, Tcl_GetString(pStr));
    Tcl_DecrRefCount(pStr);

    return zStr;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeClear --
 *
 *     Completely reset the widgets internal structures - for example when
 *     loading a new document.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlTreeClear(pTree)
    HtmlTree *pTree;
{
    HtmlToken *pToken;
    Tcl_HashSearch s;
    Tcl_HashEntry *p;

    /* Free the font-cache - pTree->aFontCache */
    for (
        p = Tcl_FirstHashEntry(&pTree->aFontCache, &s); 
        p; 
        p = Tcl_NextHashEntry(&s)) 
    {
        Tk_FreeFont((Tk_Font)Tcl_GetHashValue(p));
        /* Tcl_DeleteHashEntry(p); */
    }
    Tcl_DeleteHashTable(&pTree->aFontCache);
    Tcl_InitHashTable(&pTree->aFontCache, TCL_STRING_KEYS);

    /* Free the image-cache - pTree->aImage */
    HtmlClearImageArray(pTree);

    /* Free the tree representation - pTree->pRoot */
    HtmlTreeFree(pTree);

    /* Free the token representation */
    for (pToken=pTree->pFirst; pToken; pToken = pToken->pNext) {
        ckfree((char *)pToken->pPrev);
    }
    ckfree((char *)pTree->pLast);
    pTree->pFirst = 0;
    pTree->pLast = 0;

    /* Free the canvas representation */
    HtmlDrawDeleteControls(pTree, &pTree->canvas);
    HtmlDrawCleanup(&pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

    /* Free the plain text representation */
    if (pTree->pDocument) {
        Tcl_DecrRefCount(pTree->pDocument);
    }
    pTree->nParsed = 0;
    pTree->pDocument = 0;
    pTree->iCol = 0;

    /* Free the stylesheets */
    HtmlCssStyleSheetFree(pTree->pStyle);
    pTree->pStyle = 0;
    return TCL_OK;
}

