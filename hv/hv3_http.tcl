namespace eval hv3 { set {version($Id: hv3_http.tcl,v 1.6 2006/06/10 12:32:27 danielk1977 Exp $)} 1 }

#
# This file contains implementations of the -requestcmd and -cancelrequestcmd
# scripts used with the hv3 widget for the demo browser. Supported functions
# are:
#
#     * http:// (including cookies)
#     * file://
#

package require snit
package require Tk
package require http

#
# ::hv3::protocol
#
#     Connect hv3 to the outside world via download-handle objects.
#
# Synopsis:
#
#     set protocol [::hv3::protocol %AUTO%]
#
#     $protocol requestcmd DOWNLOAD-HANDLE
#     $protocol cancelrequestcmd DOWNLOAD-HANDLE
#
#     $protocol schemehandler scheme handler
#
#     $protocol destroy
#
snit::type ::hv3::protocol {

  option -proxyport -default 8123      -configuremethod ConfigureProxy
  option -proxyhost -default localhost -configuremethod ConfigureProxy

  # Lists of waiting and in-progress http URI download-handles.
  variable myWaitingHandles    [list]
  variable myInProgressHandles [list]
 
  # If not set to an empty string, contains the name of a global
  # variable to set to a short string describing the state of
  # the object. The string is always of the form:
  #
  #     "X1 waiting, X2 in progress"
  #
  # where X1 and X2 are integers. An http request is said to be "waiting"
  # until the header identifying the mimetype is received, and "in progress"
  # from that point on until the resource has been completely retrieved.
  #
  option -statusvar -default "" -configuremethod ConfigureStatusvar

  # Instance of ::hv3::cookiemanager
  variable myCookieManager ""

  # Both built-in ("http" and "file") and any configured scheme handlers 
  # (i.e. "home:") are stored in this array.
  variable mySchemeHandlers -array [list]

  constructor {args} {
    $self configurelist $args
    $self ConfigureProxy proxyport $options(-proxyport)
    set myCookieManager [::hv3::cookiemanager %AUTO%]

    $self schemehandler file [mymethod request_file]
    $self schemehandler http [mymethod request_http]
  }

  destructor {
    if {$myCookieManager ne ""} {$myCookieManager destroy}
  }

  # Register a custom scheme handler command (like "home:").
  method schemehandler {scheme handler} {
    set mySchemeHandlers($scheme) $handler
  }

  # This method is invoked as the -cancelrequestcmd script of an hv3 widget
  method cancelrequestcmd {downloadHandle} {
    # TODO
  }

  # This method is invoked as the -requestcmd script of an hv3 widget
  method requestcmd {downloadHandle} {

    # Extract the URI scheme to figure out what kind of URI we are
    # dealing with. Currently supported are "file" and "http" (courtesy 
    # Tcl built-in http package).
    set uri_obj [::hv3::uri %AUTO% [$downloadHandle uri]]
    set uri_scheme [$uri_obj cget -scheme]
    $uri_obj destroy

    # Fold the scheme to lower-case. Should ::hv3::uri have already done this?
    set uri_scheme [string tolower $uri_scheme]

    # Execute the scheme-handler, or raise an error if no scheme-handler
    # can be found.
    if {[info exists mySchemeHandlers($uri_scheme)]} {
      eval [concat $mySchemeHandlers($uri_scheme) $downloadHandle]
    } else {
      error "Unknown URI scheme: \"$uri_scheme\""
    }
  }

  # Handle an http:// URI.
  #
  method request_http {downloadHandle} {
    set uri       [$downloadHandle uri]
    set authority [$downloadHandle authority]
    set postdata  [$downloadHandle postdata]

    # Store the HTTP header containing the cookies in variable $headers
    set headers [$myCookieManager get_cookies $authority]
    if {$headers ne ""} {
      set headers [list Cookie $headers]
    }

    # Fire off a request via the http package.
    set geturl [list ::http::geturl $uri                     \
      -command [mymethod _DownloadCallback $downloadHandle]  \
      -handler [mymethod _AppendCallback $downloadHandle]    \
      -headers $headers                                      \
    ]
    if {$postdata ne ""} {
      lappend geturl -query $postdata
    }
    set token [eval $geturl]

    # Add this handle the the waiting-handles list. Also add a callback
    # to the -failscript and -finscript of the object so that it 
    # automatically removes itself from our lists (myWaitingHandles and
    # myInProgressHandles) after the retrieval is complete.
    lappend myWaitingHandles $downloadHandle
    ::hv3::download_destructor $downloadHandle [
      mymethod Finish_request $downloadHandle $token
    ]
    $self Updatestatusvar
  }

  # Handle a file:// URI.
  #
  method request_file {downloadHandle} {

    # Extract the "path" and "authority" components from the URI
    set uri_obj [::hv3::uri %AUTO% [$downloadHandle uri]]
    set path      [$uri_obj cget -path]
    set authority [$uri_obj cget -authority]
    $uri_obj destroy

    set filename $path
    if {$::tcl_platform(platform) eq "windows" && $authority ne ""} {
      set filename "$authority:$path"
    }

    # Read the file from the file system. The [open] or [read] command
    # might throw an exception. No problem, the hv3 widget will catch
    # it and automatically deem the request to have failed.
    #
    # Unless the expected mime-type begins with the string "text", 
    # configure the file-handle for binary mode.
    set fd [open $filename]
    if {![string match text* [$downloadHandle cget -mimetype]]} {
      fconfigure $fd -encoding binary -translation binary
    }
    set data [read $fd]
    close $fd

    $downloadHandle append $data
    $downloadHandle finish
  }

  # Configure the http package to use a proxy as specified by
  # the -proxyhost and -proxyport options on this object.
  #
  method ConfigureProxy {option value} {
    set options($option) $value
    # ::http::config -proxyhost $options(-proxyhost)
    # ::http::config -proxyport $options(-proxyport)
    ::http::config -useragent {Mozilla/5.0 Gecko/20050513}
    set ::http::defaultCharset utf-8
  }

  method Finish_request {downloadHandle token} {
    if {[set idx [lsearch $myInProgressHandles $downloadHandle]] >= 0} {
      set myInProgressHandles [lreplace $myInProgressHandles $idx $idx]
    }
    if {[set idx [lsearch $myWaitingHandles $downloadHandle]] >= 0} {
      set myWaitingHandles [lreplace $myWaitingHandles $idx $idx]
    }
    ::http::reset $token
    $self Updatestatusvar
  }

  # Update the value of the -statusvar variable, if the -statusvar
  # option is not set to an empty string.
  method Updatestatusvar {} {
    if {$options(-statusvar) ne ""} {
      set    value "[llength $myWaitingHandles] waiting, "
      append value "[llength $myInProgressHandles] in progress"
      uplevel #0 [list set $options(-statusvar) $value]
    }
  }
  
  # Invoked to set the value of the -statusvar option
  method ConfigureStatusvar {option value} {
    set options($option) $value
    $self Updatestatusvar
  }

  # Invoked when data is available from an http request. Pass the data
  # along to hv3 via the downloadHandle.
  #
  method _AppendCallback {downloadHandle socket token} {
    upvar \#0 $token state 

    # If this download-handle is still in the myWaitingHandles list,
    # process the http header and move it to the in-progress list.
    if {0 <= [set idx [lsearch $myWaitingHandles $downloadHandle]]} {

      # Remove the entry from myWaitingHandles.
      set myWaitingHandles [lreplace $myWaitingHandles $idx $idx]

      foreach {name value} $state(meta) {
        switch -- $name {
          Location {
            set redirect $value
          }
          Set-Cookie {
            regexp {^([^= ]*)=([^ ;]*)} $value dummy name value
            $myCookieManager add_cookie [$downloadHandle authority] $name $value
          }
          Content-Type {
            if {[set idx [string first ";" $value]] >= 0} {
              set value [string range $value 0 [expr $idx-1]]
            }
            $downloadHandle mimetype $value
          }
          Content-Length {
            $downloadHandle expected_size $value
          }
        }
      }


      if {[info exists redirect]} {
        ::http::reset $token
        $downloadHandle redirect $redirect
        $self requestcmd $downloadHandle
        return
      }

      lappend myInProgressHandles $downloadHandle 
      $self Updatestatusvar
    }

    set data [read $socket 2048]
    set rc [catch [list $downloadHandle append $data] msg]
    if {$rc} { puts "Error: $msg $::errorInfo" }
    set nbytes [string length $data]
    return $nbytes
  }

  # Invoked when an http request has concluded.
  #
  method _DownloadCallback {downloadHandle token} {
    if {
      [lsearch $myInProgressHandles $downloadHandle] >= 0 ||
      [lsearch $myWaitingHandles $downloadHandle] >= 0
    } {
      $downloadHandle finish
    }
  }

  method debug_cookies {} {
    $myCookieManager debug
  }
}

# A cookie manager is a database of http cookies. It supports the 
# following operations:
#
#     * Add cookie to database,
#     * Retrieve applicable cookies for an http request, and
#     * Delete the contents of the cookie database.
#
# Also, a GUI to inspect and manipulate the database in a new top-level 
# window is provided.
#
# Interface:
#
#     $pathName add_cookie AUTHORITY NAME VALUE
#     $pathName get_cookies AUTHORITY
#     $pathName debug
#
snit::type ::hv3::cookiemanager {

  variable myDebugWindow

  # The cookie data is stored in the following array variable. The
  # array keys are authority names. The array values are the list of cookies
  # associated with the authority. Each list element (a single cookie) is 
  # stored as a list of two elements, the cookie name and value. For
  # example, a two cookies from tkhtml.tcl.tk might be added to the database
  # using code such as:
  #
  #     set myCookies(tkhtml.tcl.tk) [list {login qwertyuio} {prefs 1234567}]
  # 
  variable myCookies -array [list]

  constructor {} {
    set myDebugWindow [string map {: _} ".${self}_toplevel"]
  }

  method add_cookie {authority name value} {
    if {0 == [info exists myCookies($authority)]} {
      set myCookies($authority) [list]
    }

    array set cookies $myCookies($authority)
    set cookies($name) $value
    set myCookies($authority) [array get cookies]

    if {[winfo exists $myDebugWindow]} {$self debug}
  }

  # Retrieve the cookies that should be sent to the specified authority.
  # The cookies are returned as a string of the following form:
  #
  #     "NAME1=OPAQUE_STRING1; NAME2=OPAQUE_STRING2 ..."
  #
  method get_cookies {authority} {
    set ret ""
    if {[info exists myCookies($authority)]} {
      foreach {name value} $myCookies($authority) {
        append ret [format "%s=%s; " $name $value]
      }
    }
    return $ret
  }

  method get_report {} {
    set Template {
      <html><head><style>$Style</style></head>
      <body>
        <h1>Hv3 Cookies</h1>
        <p>
	  <b>Note:</b> This window is automatically updated when Hv3's 
	  internal cookies database is modified in any way. There is no need to
          close and reopen the window to refresh it's contents.
        </p>
        <div id="clear"></div>
        <br clear=all>
        $Content
      </body>
      <html>
    }

    set Style {
      .authority { margin-top: 2em; font-weight: bold; }
      .name      { padding-right: 5ex; }
      #clear { 
        float: left; 
        margin: 1em; 
        margin-top: 0px; 
      }
    }

    set Content ""
    if {[llength [array names myCookies]] > 0} {
      append Content "<table>"
      foreach authority [array names myCookies] { 
        append Content "<tr><td><div class=authority>$authority</div>"
        foreach {name value} $myCookies($authority) {
          append Content [subst {
            <tr>
              <td><span class=name>$name</span>
              <td><span class=value>$value</span>
          }]
        }
      }
      append Content "</table>"
    } else {
      set Content {
        <p>The cookies database is currently empty.
      }
    }

    return [subst $Template]
  } 
  method download_report {downloadHandle} {
    $downloadHandle append [$self get_report]
    $downloadHandle finish
  }

  method debug {} {
    set path $myDebugWindow
    if {![winfo exists $path]} {
      toplevel $path
      ::hv3::hv3 ${path}.hv3
      ${path}.hv3 configure -width 400 -height 400
      pack ${path}.hv3 -expand true -fill both
      set HTML [${path}.hv3 html]

      # The "clear database button"
      button ${HTML}.clear   -text "Clear Database" -command [subst {
        array unset [myvar myCookies]
        [mymethod debug]
      }]

      ${path}.hv3 configure -requestcmd [mymethod download_report]
    }
    raise $path
    ${path}.hv3 goto report://

    set HTML [${path}.hv3 html]
    [lindex [${path}.hv3 search #clear] 0] replace ${HTML}.clear
  }
}

#
# This mega-widget creates a new top-level window to control 
# downloading a URI to the file-system. This isn't the most elegant
# way to handle downloads, but it is familiar to users and quick
# to implement. This is just a demo after all (sigh)...
#
# This widget is designed to interface with an hv3 download handle (an 
# instance of class ::hv3::download).
#
# SYNOPSIS:
#
#     set obj [::hv3::filedownload %AUTO% ?OPTIONS?]
#
#     $obj set_destination $PATH
#     $obj append $DATA
#     $obj finish
#
# Options are:
#
#     Option        Default   Summary
#     -------------------------------------
#     -source       ""        Source of download (for display only)
#     -size         ""        Expected size in bytes
#     -cancelcmd    ""        Script to invoke to cancel the download
#
snit::widget ::hv3::filedownloader {
  hulltype toplevel

  # The destination path (in the local filesystem) and the corresponding
  # tcl channel (if it is open). These two variables also define the 
  # three states that this object can be in:
  #
  # INITIAL:
  #     No destination path has been provided yet. Both myDestination and
  #     myChannel are set to an empty string.
  #
  # STREAMING:
  #     A destination path has been provided and the destination file is
  #     open. But the download is still in progress. Neither myDestination
  #     nor myChannel are set to an empty string.
  #
  # FINISHED:
  #     A destination path is provided and the entire download has been
  #     saved into the file. We're just waiting for the user to dismiss
  #     the GUI. In this state, myChannel is set to an empty string but
  #     myDestination is not.
  #
  variable myDestination ""
  variable myChannel ""

  # Buffer for data while waiting for a file-name. This is used only in the
  # state named INITIAL in the above description. The $myIsFinished flag
  # is set to true if the download is finished (i.e. [finish] has been 
  # called).
  variable myBuffer ""
  variable myIsFinished 0

  option -source    -default ""
  option -size      -default ""
  option -cancelcmd -default ""

  # Variables used to update the dynamic label widgets.
  variable myStatus ""
  variable myElapsed ""

  # Total bytes downloaded so far.
  variable myDownloaded 0

  # Time the download started, according to [clock seconds]
  variable myStartTime 

  constructor {args} {
    $self configurelist $args

    foreach e [list \
        [list 0 "Source:"      options(-source)]      \
        [list 1 "Destination:" myDestination] \
        [list 2 "Status:"      myStatus]      \
        [list 3 "Elapsed:"     myElapsed]     \
    ] {
      foreach {n text var} $e {}
      set strlabel [label ${win}.row${n}_0 -text $text]
      set varlabel [label ${win}.row${n}_1 -textvariable [myvar $var]]
      grid configure $strlabel -column 0 -row $n -sticky w
      grid configure $varlabel -column 1 -row $n -sticky w
    }
    grid columnconfigure ${win} 1 -minsize 400

    label ${win}.progress_label -text "Progress:"
    canvas ${win}.progress -height 12 -borderwidth 2 -relief sunken
    ${win}.progress create rectangle 0 0 0 12 -fill darkblue -tags rect

    # The "Progress:" label and canvas pretending to be a progress bar.
    grid configure ${win}.progress_label -column 0 -row 4 -sticky w
    grid configure ${win}.progress       -column 1 -row 4 -sticky ew

    button ${win}.button -text Cancel -command [mymethod Cancel]
    grid configure ${win}.button -column 1 -row 5 -sticky e

    set myStartTime [clock seconds]
    $self Timedcallback
  }

  method set_destination {dest} {

    # It is an error if this method has been called before.
    if {$myDestination ne ""} {
      error "This ::hv3::filedownloader already has a destination!"
    }

    if {$dest eq ""} {
      # Passing an empty string to this method cancels the download.
      # This is for conveniance, because [tk_getSaveFile] returns an 
      # empty string when the user selects "Cancel".
      $self Cancel
    } else {
      # Set the myDestination variable and open the channel to the
      # file to write. Todo: An error could occur opening the file.
      set myDestination $dest
      set myChannel [open $myDestination w]
      fconfigure $myChannel -encoding binary -translation binary

      # If a buffer has accumulated, write it to the new channel.
      puts -nonewline $myChannel $myBuffer
      set myBuffer ""

      # Update the GUI
      $self Updategui

      # If the myIsFinished flag is set, then the entire download
      # was already in the buffer. We're finished.
      if {$myIsFinished} {
        $self finish {}
      }
    }
  }

  # This internal method is called to cancel the download. When this
  # returns the object will have been destroyed.
  #
  method Cancel {} {
    # Evaluate the -cancelcmd script and destroy the object.
    eval $options(-cancelcmd)
    if {$myDestination ne ""} {
      catch {close $myChannel}
      catch {file delete $myDestination}
    }
    destroy $self
  }

  # Update the GUI to match the internal state of this object.
  #
  method Updategui {} {
    if {0 == $myIsFinished} {
      set tm [expr [clock seconds] - $myStartTime]
      set myElapsed "$tm seconds"
    }

    if {$myIsFinished} {
      set myStatus "$myDownloaded / $myDownloaded (finished)"
      set percentage 100.0
    } elseif {$options(-size) eq ""} {
      set myStatus "$myDownloaded / ??"
      set percentage 50.0
    } else {
      set percentage [expr ${myDownloaded}.0 * 100.0 / ${options(-size)}.0]
      set percentage [format "%.1f" $percentage]
      set myStatus "$myDownloaded / $options(-size) ($percentage%)"
    }

    set w [expr [winfo width ${win}.progress].0 * $percentage / 100.0]
    ${win}.progress coords rect 0 0 $w 12
  }

  method Timedcallback {} {
    $self Updategui
    after 500 [mymethod Timedcallback]
  }

  method append {data} {
    if {$myChannel ne ""} {
      puts -nonewline $myChannel $data
      set myDownloaded [file size $myDestination]
    } else {
      append myBuffer $data
      set myDownloaded [string length $myBuffer]
    }
    $self Updategui
  }

  # Called by the driver download-handle when the download is 
  # complete. All the data will have been already passed to [append].
  #
  method finish {data} {
    $self append $data

    # If the channel is open, close it. Also set the button to say "Ok".
    if {$myChannel ne ""} {
      close $myChannel
      set myChannel ""
      ${win}.button configure -text Ok -command [list destroy $self]
    }

    # If myIsFinished flag is not set, set it and then set myElapsed to
    # indicate the time taken by the download.
    if {!$myIsFinished} {
      set myIsFinished 1
      set myElapsed "[expr [clock seconds] - $myStartTime] seconds"
    }

    # Update the GUI.
    $self Updategui
  }

  destructor {
    catch { close $myChannel }
    after cancel [mymethod Timedcallback]
  }
}
