#############################################################################
# Sets up the control panel for the Video Spot Tracker program.
# XXX Eventually, it should handle all of the controls.

# Global variable to remember where they are saving files.
set fileinfo(open_dir) "C:\\"

###########################################################
# Put in a big "Quit" button at the top of the main window.

button .quit -text "Quit" -command { set quit 1 }
pack .quit -side top -fill x

###########################################################
# Put the places for the controls to let the user pick a kernel.

toplevel .kernel
wm geometry .kernel +190+10
frame .kernel.aperture
pack .kernel.aperture -side left
frame .kernel.wavelength
pack .kernel.wavelength -side left
frame .kernel.pixel
pack .kernel.pixel -side left
frame .kernel.pixelhide
pack .kernel.pixelhide -side left

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
wm geometry .log +10+550
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
