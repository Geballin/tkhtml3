
# This Tcl script generates two files, cssprop.h and cssprop.c, that 
# implement a way to resolve CSS property names and constants to symbols.

#
# This list contains all the constant strings that are understood as
# property values by Tkhtml. For each entry, a '#define' constant with 
# the name "CSS_CONST_<string>" where <string> is the value of the constant
# folded to upper case. Any '-' characters are converted to '_'.
# 
# Sequential values are assigned to the '#define' starting at
# CSS_CONST_MIN_CONSTANT and ending with CSS_CONST_MAX_CONSTANT.  The value
# are always above 1000.
#
# i.e:
#
#     #define CSS_CONST_MIN_CONSTANT 1000
#     #define CSS_CONST_BLOCK        1000
#     #define CSS_CONST_INLINE       1001
#     #define CSS_CONST_LIST_ITEM    1002
#     #define CSS_CONST_NONE         1003
#     #define CSS_CONST_MAX_CONSTANT 1003
#
# Two functions are also generated:
#
#     int          HtmlCssStringToConstant(int n, const char *z);
#     const char * HtmlCssConstantToString(int e);
#
# See Tcl procedures [C_get_constants] and [C_get_functions] in this file
# for more details.
#
set ::constants [list]
proc C {args} {set ::constants [concat $::constants $args]}

C inherit
C block inline list-item none                     ;# 'display'
C run-in compact marker table inline-table 
C table-caption table-row-group table-cell
C table-header-group table-footer-group table-row 
C table-column-group table-column
C left right none                                 ;# 'float'
C left right none both                            ;# 'clear'
C left right center justify                       ;# 'text-align'
C auto                                            ;# 'margin'
C square disc circle none                         ;# 'list-style-type'
C italic oblique                                  ;# 'font-style'
C bold bolder                                     ;# 'font-weight'
C top middle bottom baseline sub super            ;# 'vertical-align'
C text-top text-bottom
C underline overline line-through none            ;# 'text-decoration'
C pre nowrap normal                               ;# 'white-space'
C xx-small x-small small medium large x-large     ;# 'font-size'
C xx-large larger smaller
C thin medium thick                               ;# 'border-width'
C none hidden dotted dashed solid double groove   ;# 'border-style'
C ridge outset inset
C scroll fixed                                    ;# 'background-attachment'
C repeat no-repeat repeat-x repeat-y              ;# 'background-repeat'
C top left right bottom center                    ;# 'background-position'
C black silver gray white maroon red purple aqua  ;# Standard web colors
C fuchsia green lime olive yellow navy blue teal

# This is a list of all property names for properties that are not (a)
# shortcut properties or (b) custom Tkhtml properties.
#
set properties [list \
azimuth background-attachment background-color background-image \
background-position background-repeat border-collapse border-spacing \
border-top-color border-right-color border-bottom-color border-left-color \
border-top-style border-right-style border-bottom-style border-left-style \
border-top-width border-right-width border-bottom-width border-left-width \
bottom caption-side clear clip color content counter-increment counter-reset \
cue-after cue-before cursor direction display elevation empty-cells float \
font-family font-size font-size-adjust font-stretch font-style font-variant \
font-weight height left letter-spacing line-height list-style list-style-image \
list-style-position list-style-type margin-top margin-right \
margin-bottom margin-left marker-offset marks max-height max-width \
min-height min-width orphans outline-color outline-style outline-width \
overflow padding-top padding-right padding-bottom padding-left \
page page-break-after page-break-before page-break-inside pause pause-after \
pause-before pitch pitch-range play-during position quotes richness right \
size speak speak-header speak-numeral speak-punctuation speech-rate stress \
table-layout text-align text-decoration text-indent text-shadow text-transform \
top unicode-bidi vertical-align visibility voice-family volume white-space \
widows width word-spacing z-index \
]

# Custom tkhtml properties:
lappend properties -tkhtml-replace

set shortcut_properties [list \
background border border-top border-right border-bottom border-left \
border-color border-style border-width cue font padding outline margin\
]

#########################################################################
#########################################################################
# End of configuration, start of code.
#########################################################################
#########################################################################

# Return the C-code that defines the #define symbols for the CSS constants.
#
proc C_get_constants {} {
    set val 1000
    append ret "#define CSS_CONST_MIN_CONSTANT $val\n"
    foreach c [lsort -unique $::constants] {
        set foldedname [string map [list - _] [string toupper $c]]
        append ret "#define CSS_CONST_$foldedname $val\n"
        incr val
    }
    incr val -1
    append ret "#define CSS_CONST_MAX_CONSTANT $val\n"
    append ret "EXTERN int HtmlCssStringToConstant(CONST char *);\n"
    append ret "EXTERN CONST char * HtmlCssConstantToString(int);\n"
    return $ret
}

proc C_get_functions {} {

    set template {
#include <assert.h>

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssConstantToString --
 *
 *    Retrieve the string value of a CSS constant. i.e:
 *
 *        char *zVal = HtmlCssConstantToString(CSS_CONST_BLOCK);
 *        assert(0 == strcmp(zVal, "block"));
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CONST char * 
HtmlCssConstantToString(e)
    int e;
{
    CONST char *aStrings[] = {
$strings
    };
    assert(e >= CSS_CONST_MIN_CONSTANT);
    assert(e <= CSS_CONST_MAX_CONSTANT);
    return aStrings[e - CSS_CONST_MIN_CONSTANT];
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssStringToConstant --
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
HtmlCssStringToConstant(z)
    CONST char *z;
{
    static int isInit = 0;
    static Tcl_HashTable h;
    Tcl_HashEntry *pEntry;

    /* TODO: Make this thread-safe */
    if( !isInit ){
        int i;
        Tcl_HashKeyType *pCaseKey = HtmlCaseInsenstiveHashType();
        Tcl_InitCustomHashTable(&h, TCL_CUSTOM_TYPE_KEYS, pCaseKey);
        for(i = CSS_CONST_MIN_CONSTANT; i <= CSS_CONST_MAX_CONSTANT; i++){
            int newEntry;
            char const *zVal = HtmlCssConstantToString(i);
            pEntry = Tcl_CreateHashEntry(&h, zVal, &newEntry);
            Tcl_SetHashValue(pEntry, i);
        }
        isInit = 1;
    }

    pEntry = Tcl_FindHashEntry(&h, z);

    if( pEntry ){
         return (int)Tcl_GetHashValue(pEntry);
    }
    return -1;
}

    }

    foreach c [lsort -unique $::constants] {
        append strings "        \"$c\", \n"
    }
    return [subst -nocommands $template]


}

set fd [open cssprop.h w]
puts $fd "#ifndef __CSSPROP_H__"
puts $fd "#define __CSSPROP_H__"
puts $fd 
puts $fd {#include <tcl.h>}
puts $fd 
puts $fd [C_get_constants]
set i 0
foreach p $properties {
  set str [string map {- _} [string toupper $p]]
  puts $fd "#define CSS_PROPERTY_$str $i"
  incr i
}
foreach s $shortcut_properties {
  set str [string map {- _} [string toupper $s]]
  puts $fd "#define CSS_SHORTCUTPROPERTY_$str $i"
  incr i
}
puts $fd "const char *tkhtmlCssPropertyToString(int i);"
puts $fd "int HtmlCssPropertyToString(int n, const char *z);"
puts $fd "#endif"
set property_count $i
close $fd

set fd [open cssprop.c w]
puts $fd {#include "cssprop.h"}
puts $fd {#include <tcl.h>}
puts $fd [C_get_functions]
puts $fd "const char *tkhtmlCssPropertyToString(int i){"
puts $fd "    static const char *property_names\[\] = {"
foreach p $properties {
  puts $fd "        \"$p\","
}
foreach s $shortcut_properties {
  puts $fd "        \"$s\","
}
puts $fd "    };"
puts $fd {    return property_names[i];}
puts $fd "}"
puts $fd ""
puts $fd ""

puts $fd [subst -nocommands -nobackslashes {

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssPropertyToString --
 *
 *     Parameter 'z' points to a string containing a property name (i.e.
 *     "border-top-width"). Return the corresponding property-id (i.e.
 *     CSS_PROPERTY_BORDER_TOP_WIDTH). -1 is returned if the string cannot
 *     be matched.
 *
 *     Parameter 'n' is the number of bytes in the string. If 'n' is less
 *     than 0, then the string is NULL-terminated.
 *
 * Results:
 *     Property-id constant. 
 *
 * Side effects:
 *     May intitialize static hash table.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlCssPropertyToString(n, z)
    int n;
    CONST char *z;
{
    static int isInit = 0;
    static Tcl_HashTable h;
    Tcl_HashEntry *pEntry;
    char *zTerm;

    if( !isInit ){
        int i;
        Tcl_HashKeyType *pCaseKey = HtmlCaseInsenstiveHashType();
        Tcl_InitCustomHashTable(&h, TCL_CUSTOM_TYPE_KEYS, pCaseKey);
        for(i=0; i<$i; i++){
            int newEntry;
            char const *zProp = tkhtmlCssPropertyToString(i);
            pEntry = Tcl_CreateHashEntry(&h, zProp, &newEntry);
            Tcl_SetHashValue(pEntry, i);
        }
        isInit = 1;
    }

    if( n<0 ){
      zTerm = (char *)z;
    }else{
      zTerm = ckalloc(n+1);
      memcpy(zTerm, z, n);
      zTerm[n] = '\0';
    }

    pEntry = Tcl_FindHashEntry(&h, zTerm);

    if( zTerm!=z ){
      ckfree(zTerm);
    }

    if( pEntry ){
         return (int)Tcl_GetHashValue(pEntry);
    }
    return -1;
}

}]

close $fd

