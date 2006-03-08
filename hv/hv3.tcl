#
# Public API to hv3 functionality.
#
# hv3Init PATH ?option value...?
#
#     Initialise an instance of the Hv3 embedded app. The parameter PATH is a
#     Tk window path-name for a frame widget that will be created. The 
#     following widgets are created when this command is invoked:
#
#         $PATH                 Frame widget
#         $PATH.html            Html widget
#         $PATH.vsb             Vertical scrollbar widget
#         $PATH.hsb             Horizontal scrollbar widget
#         $PATH.status          Label widget (browser "status")
#
#     Options are:
#
#         -gotocallback         Goto callback.
#
#
# hv3Destroy PATH
#
#     Destroy the widgets created by hv3Init and deallocate all resources
#     for application instance PATH.
#
#
# hv3Goto PATH URI
#
#     Retrieve and display the document at URI $URI.
#
#
# hv3RegisterProtocol PATH protocol cmd
#
#     Register a protocol handler command. The protocol is the first part of
#     the fully-qualified URIs that the command can handle, i.e "file" or 
#     "http". A protocol command must support the following invocations:
#
#         $cmd URI -callback SCRIPT ?-binary BOOLEAN?
#         $cmd -reset
#     
package require Itcl

swproc hv3Init {PATH {gotocallback ""}} {
  ::hv3::initVars $PATH
  ::hv3::importVars $PATH

  set myUrl [Hv3Uri #auto [pwd]/]
  set myGotoCallback $gotocallback

  # Create the widgets and pack them all into the frame. The caller 
  # must pack the frame itself.
  frame $PATH
  html $PATH.html
  scrollbar $PATH.vsb -orient vertical
  scrollbar $PATH.hsb -orient horizontal
  label $PATH.status -height 1 -anchor w
  pack $PATH.vsb -fill y -side right
  pack $PATH.status -fill x -side bottom 
  pack $PATH.hsb -fill x -side bottom
  pack $PATH.html -fill both -expand true

  # Set up scrollbar callbacks.
  $PATH.html configure -yscrollcommand "::hv3::scrollCallback $PATH.vsb"
  $PATH.hsb configure -command "$PATH.html xview"
  $PATH.html configure -xscrollcommand "::hv3::scrollCallback $PATH.hsb"
  $PATH.vsb configure -command "$PATH.html yview"

  # Image callback.
  $PATH.html configure -imagecmd [list ::hv3::imageCmd $PATH]

  # Set up Handler callbacks for <link>, <style>, <script> and <img> tags.
  $PATH.html handler script script "::hv3::handleScriptScript"
  $PATH.html handler node link     "::hv3::handleLinkNode     $PATH"
  $PATH.html handler script style  "::hv3::handleStyleScript  $PATH"

  # Widget bindings
  bind $PATH.html <Motion> "::hv3::guiMotion $PATH %x %y"
  bind $PATH.html <1> "::hv3::guiLeftClick $PATH \[$PATH.html node %x %y\]"
  bind $PATH.html <2> "::hv3::guiMiddleClick $PATH"
  bind $PATH.html <3> "::hv3::guiRightClick $PATH \[$PATH.html node %x %y\]"

  bind $PATH.html <ButtonPress-1>   "::hv3::guiLeftPress $PATH %x %y"
  bind $PATH.html <ButtonRelease-1> "::hv3::guiLeftRelease $PATH %x %y"

  # Set up a selection handler callback
  selection handle $PATH.html [list ::hv3::guiGetSelection $PATH]

  # Register the built-in URI protocol "file"
  hv3RegisterProtocol $PATH file Hv3FileProtocol

  # Force the status bar variables to initialise by pretending someone 
  # middle-clicked on the html widget.
  ::hv3::guiMiddleClick $PATH

  return $PATH
}

swproc hv3Goto {PATH url {noresolve 0 1} {nocallback 0 1}} {
  ::hv3::importVars $PATH

  set current     [$myUrl get -nofragment]
  if {$noresolve} {
    $myUrl configure -authority "" -path / -query "" -fragment ""
  }
  $myUrl load $url

  if {$myGotoCallback != "" && !$nocallback} {
    eval $myGotoCallback [$myUrl get]
  }

  set f [$myUrl cget -fragment]
  set prefragment [$myUrl get -nofragment]

  if {$current == $prefragment && $f != ""} {
    ::hv3::goto_fragment $PATH $f
  } else {
    set script [list ::hv3::parse $PATH $f] 
    ::hv3::download $PATH $prefragment -script $script -type Document -reset
  }
}

proc hv3Destroy {PATH} {
}

proc hv3RegisterProtocol {PATH protocol cmd} {
  ::hv3::importVars $PATH
  array set protocols $myProtocols
  set protocols($protocol) $cmd
  set myProtocols [array get protocols]
}

namespace eval hv3 {

  proc VAR {n {d {}}} {uplevel [list lappend vars $n $d]}

  VAR myProxyHost 
  VAR myProxyPort
  VAR myGotoCallback
  VAR myStateVar 

  VAR myStyleCount 0       ;# Used by [styleId] to assign style-ids
  VAR myUrl                ;# Current URL being displayed (or loaded)
  VAR myStatus1            ;# Cursor position status text
  VAR myStatus2            ;# Download progress status text
  VAR myStatusVar 1        ;# Either 1 or 2 - the current status text
  VAR myStatusInfo         ;# Serialized array of download status info
  VAR myProtocols          ;# Serialized array of protocol commands
  VAR myDrag 0             ;# True when dragging the cursor

  proc initVars {PATH} {
    foreach {var default} $::hv3::vars {
      set ::hv3::objectvars(${PATH},$var) $default
    }
  }
  proc importVars {PATH} {
    foreach {var default} $::hv3::vars {
      uplevel [subst {
        upvar #0 ::hv3::objectvars(${PATH},$var) $var
      }]
    }
  }

  set ::hv3::image_name 0

  # scrollCallback SB args
  #
  #     This proc is registered as the -xscrollcommand and -yscrollcommand 
  #     options of the html widget. The first argument is the name of a 
  #     scrollbar - either "$PATH.hsb" or "$PATH.vsb". The remaining args
  #     are those appended by the widget.
  #
  #     The position of the scrollbar is adjusted. If it is not required,
  #     the scrollbar is made to disappear by setting it's width and border
  #     width to zero.
  proc scrollCallback {sb args} {
    eval [concat $sb set $args]
    if {$args == "0.0 1.0"} {
      $sb configure -width 0 -borderwidth 0
    } else {
      $sb configure -width 15 -borderwidth 2 
    }
  }

  # handleStyleScript PATH STYLE
  #
  #     This is called as a [node handler script] to handle a <style> tag.
  #     The second argument, $script, contains the text of the stylesheet.
  proc handleStyleScript {PATH style} {
    importVars $PATH
    set id [styleId $PATH author]
    ::hv3::styleCallback $PATH [$myUrl get] $id $style
  }

  # handleScriptScript SCRIPT
  #
  #     This is invoked as a [node handler script] callback to handle 
  #     the contents of a <script> tag. It is a no-op.
  proc handleScriptScript {script} {
    return ""
  }

  # handleLinkNode PATH NODE
  #
  #     Handle a <link> node. The only <link> nodes we care about are those
  #     that load stylesheets for media-types "all" or "visual". i.e.:
  #
  #         <link rel="stylesheet" media="all" href="style.css">
  #
  proc handleLinkNode {PATH node} {
    importVars $PATH
    set rel   [$node attr -default "" rel]
    set media [$node attr -default all media]
    if {$rel=="stylesheet" && [regexp all|screen $media]} {
      set id [styleId $PATH author]

      set url [resolve [$myUrl get] [$node attr href]]
      set cmd [list ::hv3::styleCallback $PATH $url $id]

      download $PATH $url -script $cmd -type Stylesheet
    }
  }
  
  # imageCallback IMAGE-NAME DATA
  #
  #     This proc is called when an image requested by the -imagecmd callback
  #     ([imageCmd]) has finished downloading. The first argument is the name of
  #     a Tk image. The second argument is the downloaded data (presumably a
  #     binary image format like gif). This proc sets the named Tk image to
  #     contain the downloaded data.
  #
  proc imageCallback {name data} {
    if {[info commands $name] == ""} return 
    $name configure -data $data
  }
  
  # imageCmd PATH URL
  # 
  #     This proc is registered as the -imagecmd script for the Html widget.
  #     The first argument is the name of the Html widget. The second argument
  #     is the URI of the image required.
  #
  #     This proc creates a Tk image immediately. It also kicks off a fetch 
  #     request to obtain the image data. When the fetch request is complete,
  #     the contents of the Tk image are set to the returned data in proc 
  #     ::hv3::imageCallback.
  #
  proc imageCmd {PATH url} {
    importVars $PATH
    set name hv3_image[incr ::hv3::image_name]
    image create photo $name
    set fullurl [resolve [$myUrl get] $url]
    set cmd [list ::hv3::imageCallback $name] 
    download $PATH $fullurl -script $cmd -binary -type Image
    return [list $name [list image delete $name]]
  }

  # styleCallback PATH url id style
  #
  #     This proc is invoked when a stylesheet has finished downloading 
  #     (stylesheet downloads may be started by procs ::hv3::handleLinkNode 
  #     or ::hv3::styleImportCmd).
  proc styleCallback {PATH url id style} {
    set importcmd [list ::hv3::styleImport $PATH $id] 
    set urlcmd [itcl::code resolve $url]
    $PATH.html style -id $id -importcmd $importcmd -urlcmd $urlcmd $style
  }

  # styleImport styleImport PATH PARENTID URL
  #
  #     This proc is passed as the -importcmd option to a [html style] 
  #     command (see proc styleCallback).
  proc styleImport {PATH parentid url} {
    importVars $PATH
    set id [styleId $PATH $parentid]
    set script [list ::hv3::styleCallback $PATH $url $id] 
    download $PATH $url -script $script -type Stylesheet
  }

  # styleId PATH parentid
  #
  #     Return the id string for a stylesheet with parent stylesheet 
  #     $parentid.
  proc styleId {PATH parentid} {
    importVars $PATH
    format %s.%.4d $parentid [incr myStyleCount]
  }

  # guiRightClick PATH x y
  #
  #     Called when the user right-clicks on the html window. 
  proc guiRightClick {PATH node} {
    if {$node!=""} {
      HtmlDebug::browse $PATH.html [lindex $node 0]
    }
  }

  # guiMiddleClick
  #
  #     Called when the user right-clicks on the html window. 
  proc guiMiddleClick {PATH} {
    importVars $PATH
    set myStatusVar [expr ($myStatusVar % 2) + 1]
    set v ::hv3::objectvars($PATH,myStatus$myStatusVar)
    $PATH.status configure -textvariable $v
  }

  proc guiDrag {PATH x y} {
    set to [$PATH.html node -index $x $y]
    if {[llength $to]==2} {
      foreach {node index} $to {}
      $PATH.html select to $node $index
    }
    selection own $PATH.html
  }
  proc guiLeftPress {PATH x y} {
    importVars $PATH
    set from [$PATH.html node -index $x $y]
    if {[llength $from]==2} {
      foreach {node index} $from {}
      $PATH.html select from $node $index
      $PATH.html select to $node $index
    }

    foreach n [$PATH.html node $x $y] {
      for {} {$n!=""} {set n [$n parent]} {
        if {[$n tag]=="a" && [$n attr -default "" href]!=""} {
          hv3Goto $PATH [$n attr href]
          break
        }
      }
    }

    set myDrag 1
  }
  proc guiLeftRelease {PATH x y} {
    importVars $PATH
    set myDrag 0
  }

  # guiMotion PATH node
  #
  #     Called when the mouse moves over the html window. Parameter $node is
  #     the node the mouse is currently floating over, or "" if the pointer is
  #     not over any node.
  proc guiMotion {PATH x y} {
    importVars $PATH
    set txt ""
    $PATH configure -cursor ""

    set node ""
    foreach node [$PATH.html node $x $y] {
      for {set n $node} {$n!=""} {set n [$n parent]} {
        set tag [$n tag]
        if {$tag=="a" && [$n attr -default "" href]!=""} {
          $PATH configure -cursor hand2
          set txt "hyper-link: [$n attr href]"
          break
        } 
      }
    }

    if {$txt == "" && $node != ""} {
      for {set n $node} {$n!=""} {set n [$n parent]} {
        set tag [$n tag]
        if {$tag==""} {
          $PATH configure -cursor xterm
          set txt [string range [$n text] 0 20]
        } else {
          set txt "<[$n tag]>$txt"
        }
      }
    }

    set myStatus1 [string range $txt 0 80]

    if {$myDrag} {
      guiDrag $PATH $x $y
    }
  }

  # guiGetSelection PATH offset maxChars
  #
  #     This command is invoked whenever the current selection is selected
  #     while it is owned by the html widget. The text of the selected
  #     region is returned.
  #
  proc guiGetSelection {PATH offset maxChars} {
catch {
    set span [$PATH.html select span]
    if {[llength $span] != 4} {
      return ""
    }
    foreach {n1 i1 n2 i2} $span {}

    set not_empty 0
    set T ""
    set N $n1
    while {1} {

      if {[$N tag] eq ""} {
        set index1 0
        set index2 end
        if {$N == $n1} {set index1 $i1}
        if {$N == $n2} {set index2 $i2}

        set text [string range [$N text] $index1 $index2]
        append T $text
        if {[string trim $text] ne ""} {set not_empty 1}
      } else {
        array set prop [$N prop]
        if {$prop(display) ne "inline" && $not_empty} {
          append T "\n"
          set not_empty 0
        }
      }

      if {$N eq $n2} break 

      if {[$N nChild] > 0} {
        set N [$N child 0]
      } else {
        while {[set next_node [$N right_sibling]] eq ""} {
          set N [$N parent]
        }
        set N $next_node
      }

      if {$N eq ""} {error "End of tree!"}
    }

    set T [string range $T $offset [expr $offset + $maxChars]]
} msg
    return $msg
  }

  proc goto_fragment {PATH fragment} {
    set H $PATH.html
    set selector [format {[name="%s"]} $fragment]
    set goto_node [lindex [$H search $selector] 0]
    if {$goto_node!=""} {
      $H yview $goto_node
    }
  }

  # parse PATH fragment text 
  #
  #     Append the text TEXT to the current document.  If argument FRAGMENT is
  #     not "", then it is the name of an anchor within the document to jump
  #     to.
  #
  proc parse {PATH fragment text} {
    if {$text != ""} {
      $PATH.html reset
  
      importVars $PATH
      set myStyleCount 0
      $PATH.html parse $text
  
      if {$fragment != ""} {
        goto_fragment $PATH $fragment
      }
    }
  }

  # download PATH url ?-script script? ?-type type? ?-binary? ?-reset?
  #
  #     Retrieve the contents of the url $url, by invoking one of the 
  #     protocol callbacks registered via proc hv3RegisterProtocol.
  swproc download {PATH url \
    {script ""} \
    {type Document} \
    {binary 0 1} \
    {reset 0 1}} \
  {
    importVars $PATH

    set obj [Hv3Uri #auto $url]
    set scheme [$obj cget -scheme]
    ::itcl::delete object $obj

    array set protocols $myProtocols
    if {[catch {set cmd $protocols($scheme)}]} {
      error "Unknown protocol: \"$scheme\""
    }

    if {$reset} {
      # TODO
      set myStatusInfo [list]
    }

    array set status $myStatusInfo
    if {[info exists status($type)]} {
      lset status($type) 1 [expr [lindex $status($type) 1] + 1]
    } else {
      set status($type) [list 0 1]
    }
    set myStatusInfo [array get status]
    
    if {[catch {
      set dl [Hv3Download ::#auto]
      set callback [list ::hv3::downloadCallback $dl $PATH $script $type]
      $dl configure -finscript $callback -binary $binary -uri $url
      $cmd $dl
    } msg]} {
      puts "Error: Cannot fetch $url: $msg"
      return
    }
    downloadSetStatus $PATH
  }

  proc downloadCallback {downloadHandle PATH script type data} {
    importVars $PATH

    if {$type eq "Document"} {
      $myUrl load [$downloadHandle cget -uri]
    }

    array set status $myStatusInfo
    lset status($type) 0 [expr [lindex $status($type) 0] + 1]
    set myStatusInfo [array get status]
    lappend script $data
    eval $script
    downloadSetStatus $PATH
  }

  proc downloadSetStatus {PATH} {
    importVars $PATH
    set myStatus2 ""
    foreach {k v} $myStatusInfo {
      append myStatus2 "$k [lindex $v 0]/[lindex $v 1]  "
    }
  }

  # resolve baseurl url
  #
  #     This command is used to transform a (possibly) relative URL into an
  #     absolute URL. Example:
  #
  #         $ resolve http://host.com/dir1/dir2/doc.html ../dir3/doc2.html
  #         http://host.com/dir1/dir3/doc2.html
  #
  #     This is purely a string manipulation procedure.
  #
  proc resolve {baseurl url} {
    set obj [Hv3Uri #auto $baseurl]
    $obj load $url
    set ret [$obj get]
    ::itcl::delete object $obj
    return $ret
  }
}

#--------------------------------------------------------------------------
# Class Hv3Download
#
#     Instances of this class are used to interface between 
#     protocol-implementations and the hv3 widget. Refer to the hv3
#     man page for a description of the interface as used by protocol 
#     implementations.
#
itcl::class Hv3Download {
  variable myData ""

  public variable binary 0
  public variable uri ""

  public variable incrscript  ""
  public variable finscript   ""
  public variable redirscript ""

  # Query interface used by protocol implementations
  method binary    {} {return $binary}
  method uri       {} {return $uri}

  # Interface for returning data.
  method append {data} {
    if {$incrscript != ""} {
      eval [linsert $incrscript end $data]
    } else {
      ::append myData $data
    }
  }

  # Called after all data has been passed to [append].
  method finish {} {
    if {$finscript != ""} {
      eval [linsert $finscript end $myData] 
    } 
    itcl::delete object $this
  }

  # Interface for returning a redirect
  method redirect {new_uri} {
    if {$redirscript != ""} {
      eval [linsert $redirscript end $new_uri]
    } 
    set uri $new_uri
  }
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Protocol implementation for file:// protocol.
#
proc Hv3FileProtocol {downloadHandle} {
  set uri [$downloadHandle uri]
  set url_obj [Hv3Uri #auto $uri]
  set fname [$url_obj cget -path]
  itcl::delete object $url_obj

  set rc [catch {
    set f [open $fname]
    if {[$downloadHandle binary]} {
      fconfigure $f -encoding binary -translation binary
    }
    set data [read $f]
    $downloadHandle append $data
    close $f
  } msg]

  $downloadHandle finish

  if {$rc} {
    error $msg $::errorInfo
  }
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class Hv3Uri:
#
#     A very simple class for handling URI references. A partial 
#     implementation of the syntax specification found at: 
#
#         http://www.gbiv.com/protocols/uri/rfc/rfc3986.html
# 
# Usage:
#
#     set uri_obj [Hv3Uri #auto $URI]
#
#     $uri_obj load $URI
#     $uri_obj get
#     $uri_obj cget ?option?
#
#     ::itcl::delete object $uri_obj
#
itcl::class Hv3Uri {

  # Public get/set variables for URI components
  public variable scheme    file
  public variable path      "/"
  public variable authority ""
  public variable query     ""
  public variable fragment  ""

  # Constructor and destructor
  constructor {{url {}}} {$this load $url}
  destructor  {}

  # Return a copy of this object (must be passed to [delete object])
  method clone {} {
    set obj [Hv3Uri #auto]
    foreach var [list scheme authority path query fragment] {
      lappend script -${var} [set $var]
    }
    eval $obj configure $script
    return $obj
  }

  # Set the contents of the object to the specified URI.
  method load {uri} {

    proc OPT     {args} { eval append res (?: $args )? }
    proc CAPTURE {args} { eval append res ( $args ) }
    
    set SCHEME    {[A-Za-z][A-Za-z0-9+-\.]*}
    set AUTHORITY {[^/#?]*}
    set PATH      {[^#?]+}
    set QUERY     {[^#]+}
    set FRAGMENT  {.*}

    append re \
        [CAPTURE [OPT $SCHEME :]]            \
        [CAPTURE [OPT // $AUTHORITY]]        \
        [CAPTURE [OPT $PATH]]                \
        [CAPTURE [OPT $QUERY]]               \
        [CAPTURE [OPT $FRAGMENT]]

    regexp $re $uri X r(scheme) r(authority) r(path) r(query) r(fragment)
    if {![info exists r]} {
      error "Bad URL: $url"
    }
    set ok 0
    foreach var [list scheme authority path query fragment] {
      if {$r($var) != "" } {
        set ok 1
      }
      if {$ok} {
        if {$var eq "path"} {
          if {![string match /* $r(path)] && $r(authority) eq ""} {
            if {[string match */ $path]} {
              set r(path) "${path}$r(path)"
            } else {
              set dir [file dirname $path]
              if {[string match */ $dir]} {
                set dir [string range $dir 0 end-1]
              } 
              set r(path) "${dir}/$r(path)"
            }
          }
          set ret [list]
          foreach c [split $r(path) /] {
            if {$c == ".."} {
              set ret [lrange $ret 0 end-1]
            } elseif {$c == "."} {
              # Do nothing...
            } else {
              lappend ret $c
            }
          }
          set r(path) [join $ret /]
        }
        set $var $r($var)
        if {$var eq "scheme"} {set scheme [string range $scheme 0 end-1]}
        if {$var eq "authority"} {set authority [string range $authority 2 end]}
        if {$var eq "fragment"} {set fragment [string range $fragment 1 end]}
        if {$var eq "query"} {set query [string range $query 1 end]}
        if {$var eq "path" && $path eq ""} {set path "/"}
      }
    }
  }

  # Return the contents of the object formatted as a URI.
  method get {{nofragment ""}} {
    set result "${scheme}://${authority}"
    if {$path != ""}     {::append result "${path}"}
    if {$query != ""}    {::append result "?${query}"}
    if {$nofragment eq "" && $fragment ne ""} {::append result "#${fragment}"}
    return $result
  }
}
# End of class Hv3Uri
#--------------------------------------------------------------------------


#--------------------------------------------------------------------------
# Automated tests for Hv3Uri:
#
#     The following block runs some quick regression tests on the Hv3Uri 
#     implementation. These take next to no time to run, so there's little
#     harm in leaving them in.
#
if 1 {
  set test_data [list                                                 \
    {http://tkhtml.tcl.tk/index.html}                                 \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path /index.html  -query "" -fragment ""                     \
    {http:///index.html}                                              \
        -scheme http       -authority ""                              \
        -path /index.html  -query "" -fragment ""                     \
    {http://tkhtml.tcl.tk}                                            \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path "/"          -query "" -fragment ""                     \
    {/index.html}                                                     \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path /index.html  -query "" -fragment ""                     \
    {other.html}                                                      \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path /other.html  -query "" -fragment ""                     \
    {http://tkhtml.tcl.tk:80/a/b/c/index.html}                        \
        -scheme http       -authority tkhtml.tcl.tk:80                \
        -path "/a/b/c/index.html" -query "" -fragment ""              \
    {http://wiki.tcl.tk/}                                             \
        -scheme http       -authority wiki.tcl.tk                     \
        -path "/"          -query "" -fragment ""                     \
    {file:///home/dan/fbi.html}                                       \
        -scheme file       -authority ""                              \
        -path "/home/dan/fbi.html"  -query "" -fragment ""            \
    {http://www.tclscripting.com}                                     \
        -scheme http       -authority "www.tclscripting.com"          \
        -path "/"  -query "" -fragment ""                             \
  ]

  set obj [Hv3Uri #auto]
  for {set ii 0} {$ii < [llength $test_data]} {incr ii} {
    set uri [lindex $test_data $ii]
    $obj load $uri 
    while {[string match -* [lindex $test_data [expr $ii+1]]]} {
      set switch [lindex $test_data [expr $ii+1]]
      set value [lindex $test_data [expr $ii+2]]
      if {[$obj cget $switch] ne $value} {
        puts "URI: $uri"
        puts "SWITCH: $switch"
        puts "EXPECTED: $value"
        puts "GOT: [$obj cget $switch]"
        puts ""
      }
      incr ii 2
    }
  }
  itcl::delete object $obj
}
# End of tests for Hv3Uri.
#--------------------------------------------------------------------------
