#############################################################################
# Sets up the control panel for the Video Spot Tracker program.
# XXX Eventually, it should handle all of the controls.


# Global variable to remember where they are saving files.
set fileinfo(open_dir) "C:\\"

###########################################################
# Put the places for the controls to let the user pick a kernel.

toplevel .kernel
wm geometry .kernel +170+10
frame .kernel.invert
pack .kernel.invert -side left
frame .kernel.interp
pack .kernel.interp -side left
frame .kernel.cone
pack .kernel.cone -side left
frame .kernel.symmetric
pack .kernel.symmetric -side left
frame .kernel.rod3
pack .kernel.rod3 -side left
frame .kernel.radius
pack .kernel.radius -side left
frame .kernel.optimize
pack .kernel.optimize -side left

###########################################################
# Put the places for the controls for the rod kernels.
# This window should only be visible when rod3 is turned on.

toplevel .rod3
wm geometry .rod3 +800+10
wm withdraw .rod3
set rod3 0
trace variable rod3 w update_rod_window_visibility

proc update_rod_window_visibility {nm el op} {
	global rod3
	if { $rod3 } {
		wm deiconify .rod3
	} else {
		wm withdraw .rod3
	}
}

###########################################################
# Put the place for the controls for the clipping.
# This window should only be visible when clipping is turned on.

toplevel .clipping
wm geometry .clipping +800+10
wm withdraw .clipping
set show_clipping 0
trace variable show_clipping w update_clipping_window_visibility

proc update_clipping_window_visibility {nm el op} {
	global show_clipping
	if { $show_clipping } {
		wm deiconify .clipping
	} else {
		wm withdraw .clipping
	}
}

###########################################################
# Put the controls that will let the user store a log file.
# It puts a checkbox down at the bottom of the main menu
# that causes a dialog box to come up when it is turned on.
# The dialog box fills in a non-empty value into the global
# variable "logfilename" if one is created.  The callback
# clears the variable "logfilename" when logging is turned off.

set logging 0
set logfilename ""
toplevel .log
wm geometry .log +750+10
checkbutton .log.button -text Logging -variable logging -anchor w
pack .log.button -side top -fill x
label .log.label -textvariable logfilename
pack .log.label -side top -fill x
trace variable logging w logging_changed

proc logging_changed { varName index op } {
    global logging logfilename fileinfo

    if {$logging == 1} {
	set types { {"VRPN tracker log files" "*.vrpn"} }
	set filename [tk_getSaveFile -filetypes $types \
		-defaultextension ".vrpn" \
		-initialdir $fileinfo(open_dir) \
		-title "Name for log file"]
	if {$filename != ""} {
	    # setting this variable triggers a callback in C code
	    # which opens the file.
	    # dialog check whether file exists.
	    set logfilename $filename
	    set fileinfo(open_dir) [file dirname $filename]
	} 	
    } else {
	set logfilename ""
    }
}

###########################################################
# Ask user for the name of the video file they want to open,
# or else set the quit value.  The variable to set for the
# name is "device_filename".

set device_filename ""
proc ask_user_for_filename { } {
	global device_filename quit fileinfo
		
	set types { {"Video Files" "*.avi"} }
	set device_filename [tk_getOpenFile -filetypes $types \
		-defaultextension ".avi" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a video file to track in"]
	# If we don't have a name, quit.
	if {$device_filename == ""} {
		set quit 1
	} 	
}

