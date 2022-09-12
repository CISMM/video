Building:
  See README_build.txt

Running:
  The build process will have built video_spot_tracker_nogui in ~/build/video.

  If the instructions to build with the ability to read image files were followed,
  to find the precise location of a symmetric spot in the image test_image.tif
  where the location 100,100 is known to be somewhere inside the spot and the
  radius is 12 pixels, you can use the following:
    cp test_image.tif ~/build/video
    cd ~/build/video
    ./video_spot_tracker_nogui -tracker 100 100 12 -outfile test test_image.tif

  Once the program has completed, the file test.csv will hold information about
  the location of the spot in X,Y.