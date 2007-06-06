namespace eval hv3 { set {version($Id: hv3_dom_style.tcl,v 1.7 2007/06/06 15:56:39 danielk1977 Exp $)} 1 }

#-------------------------------------------------------------------------
# DOM Level 2 Style.
#
# This file contains the Hv3 implementation of the DOM Level 2 Style
# specification.
#
#     ElementCSSInlineStyle        (mixed into Element)
#     CSSStyleDeclaration          (mixed into ElementCSSInlineStyle)
#     CSS2Properties               (mixed into CSSStyleDeclaration)
#
# http://www.w3.org/TR/2000/REC-DOM-Level-2-Style-20001113/css.html
#
#

::hv3::dom2::stateless ElementCSSInlineStyle {} {
  dom_get style {
    list object [list ::hv3::DOM::CSSStyleDeclaration $myDom $myNode]
  }
}

namespace eval ::hv3::dom2::compiler {
  proc style_property {css_prop {dom_prop ""}} {
    if {$dom_prop eq ""} {
      set dom_prop $css_prop
    }
    set getcmd "CSSStyleDeclaration_getStyleProperty \$myNode $css_prop"
    dom_get $dom_prop $getcmd

    set putcmd "CSSStyleDeclaration_setStyleProperty \$myNode $css_prop \$value"
    dom_put -string $dom_prop {value} $putcmd
  }
}

# In a complete implementation of the DOM Level 2 style for an HTML 
# browser, the CSSStyleDeclaration interface is used for two purposes:
#
#     * As the ElementCSSInlineStyle.style property object. This 
#       represents the contents of an HTML "style" attribute.
#
#     * As part of the DOM representation of a parsed stylesheet 
#       document. Hv3 does not implement this function.
#
::hv3::dom2::stateless CSSStyleDeclaration CSS2Properties {

  # cssText attribute - access the text of the style declaration. 
  # TODO: Setting this to a value that does not parse is supposed to
  # throw a SYNTAX_ERROR exception.
  #
  dom_get cssText { list string [$myNode attribute -default "" style] }
  dom_put -string cssText val { 
    $myNode attribute style $val
  }

  dom_call_todo getPropertyValue
  dom_call_todo getPropertyCSSValue
  dom_call_todo removeProperty
  dom_call_todo getPropertyPriority

  dom_call -string setProperty {THIS propertyName value priority} {
    if {[info exists ::hv3::DOM::CSS2Properties_simple($propertyName)]} {
      CSSStyleDeclaration_setStyleProperty $myNode $propertyName $value
      return
    }
    error "DOMException SYNTAX_ERROR {unknown property $propertyName}"
  }
  
  # Interface to iterate through property names:
  #
  #     readonly unsigned long length;
  #     DOMString              item(in unsigned long index);
  #
  dom_get length {
    list number [expr {[llength [$myNode prop -inline]]}/2]
  }
  dom_call -string item {THIS index} {
    set idx [expr {2*int([lindex $index 1])}]
    list string [lindex [$myNode prop -inline] $idx]
  }

  # Read-only parentRule property. Always null in hv3.
  #
  dom_get parentRule { list null }
}

set ::hv3::DOM::CSS2Properties_simple(width) width
set ::hv3::DOM::CSS2Properties_simple(height) height
set ::hv3::DOM::CSS2Properties_simple(display) display
set ::hv3::DOM::CSS2Properties_simple(position) position
set ::hv3::DOM::CSS2Properties_simple(top) top
set ::hv3::DOM::CSS2Properties_simple(left) left
set ::hv3::DOM::CSS2Properties_simple(bottom) bottom
set ::hv3::DOM::CSS2Properties_simple(right) right
set ::hv3::DOM::CSS2Properties_simple(z-index) zIndex
set ::hv3::DOM::CSS2Properties_simple(border-top-width)    borderTopWidth
set ::hv3::DOM::CSS2Properties_simple(border-right-width)  borderRightWidth
set ::hv3::DOM::CSS2Properties_simple(border-left-width)   borderLeftWidth
set ::hv3::DOM::CSS2Properties_simple(border-bottom-width) borderBottomWidth

::hv3::dom2::stateless CSS2Properties {} {

  dom_parameter myNode

  foreach {k v} [array get ::hv3::DOM::CSS2Properties_simple] {
    style_property $k $v
  } 

  dom_put -string border value {
    set style [$myNode attribute -default {} style]
    if {$style ne ""} {append style ";"}
    append style "border: $value"
    $myNode attribute style $style
  }
}

namespace eval ::hv3::DOM {
  proc CSSStyleDeclaration_getStyleProperty {node css_property} {
    set val [$node property -inline $css_property]
    list string $val
  }

  proc CSSStyleDeclaration_setStyleProperty {node css_property value} {
    array set current [$node prop -inline]

# if {$value eq "NaNpx"} "error NAN"

    if {$value ne ""} {
      set current($css_property) $value
    } else {
      unset -nocomplain current($css_property)
    }

    set style ""
    foreach prop [array names current] {
      append style "$prop:$current($prop);"
    }

    $node attribute style $style
  }
}

