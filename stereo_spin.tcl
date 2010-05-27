#############################################################################
# Sets up the control panel for the stereo_spin program.

###########################################################
# Put in a big "Quit" button at the top of the main window.

button .quit -text "Quit" -command { set quit 1 }
pack .quit -side top -fill x

###########################################################
# Write the instructions for use of the program.

label .instruct1 -text "Use the mouse to move select a viewpoint"
label .instruct2 -text "Keys: (q)uit, (<) spin left, (>) spin right, spacebar stop,"
label .instruct3 -text "(R)ight-spinning movie, (L)eft-spinning movie (default)"
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

