#############################################################################
# Sets up the control panel for the Video Spot Tracker program.
# XXX Eventually, it should handle all of the controls.


# Global variable to remember where they are saving files.
set fileinfo(open_dir) ""

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
frame .kernel.radius
pack .kernel.radius -side left
frame .kernel.optimize
pack .kernel.optimize -side left

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
wm geometry .log +690+10
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
