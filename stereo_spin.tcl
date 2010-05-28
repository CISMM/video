#############################################################################
# Sets up the control panel for the stereo_spin program.

###########################################################
# Put in a big "Quit" button at the top of the main window.

button .quit -text "Quit" -command { set quit 1 }
pack .quit -side top -fill x

# Quit the program if the main window is destroyed.
bind . <Destroy> {global quit ; set quit 1} 

###########################################################
# Write the instructions for use of the program.

label .instruct1 -text "Use the mouse to move select a viewpoint"
label .instruct2 -text "Keys: (q)uit, (<) spin left, (>) spin right, spacebar stop,"
label .instruct3 -text "(-) Decrease stereo, (+) Increase stereo, (R)ight-spinning movie, (L)eft-spinning movie (default)"
pack .instruct1 -side top
pack .instruct2 -side top
pack .instruct3 -side top

###########################################################
# Create and destroy a quit dialog box.

proc show_quit_loading_dialog { } {
	global quit

	toplevel .quitload
	wm geometry .quitload +50+170
	frame .quitload.bottom
	pack .quitload.bottom -side bottom -fill x

	label .quitload.xlabel -text "Loading video into graphics memory"
	pack .quitload.xlabel

	button .quitload.bottom.quit -text "Quit" -command { set quit 1 }
	pack .quitload.bottom.quit -fill x
}

proc remove_quit_loading_dialog { } {
	wm withdraw .quitload
}

###########################################################
# Create a stereo-not-available dialog box.

proc show_nostereo_dialog { } {
	global quit

	toplevel .nostereo
	wm geometry .nostereo +500+10
	frame .nostereo.bottom
	pack .nostereo.bottom -side bottom -fill x

	label .nostereo.xlabel -text "Stereo not available on this computer (check display settings)"
	pack .nostereo.xlabel

	button .nostereo.bottom.quit -text "Quit" -command { set quit 1 }
	pack .nostereo.bottom.quit -fill x
	button .nostereo.bottom.continue -text "Continue (Cross-eyed)" -command { wm withdraw .nostereo }
	pack .nostereo.bottom.continue -fill x
}

###########################################################
# Global variable to remember where they are saving files.
set fileinfo(open_dir) "C:\\"

###########################################################
# Ask user for the name of the video file they want to open,
# or else set the quit value.  The variable to set for the
# name is "device_filename".

set device_filename ""
proc ask_user_for_filename { } {
	global device_filename quit fileinfo
		
	set types { {"Image Stack Files" "*.avi *.tif *.bmp *.raw *.spe"} }
	set device_filename [tk_getOpenFile -filetypes $types \
		-defaultextension ".avi" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a video file to track in"]
	# If we don't have a name, quit.
	if {$device_filename == ""} {
		set quit 1
	} 	
}

