SiTech Indi Driver Setup

1. copy the source code:
Put telescope_sitech.h and telescope_sitech.cpp here:
your indi projects directory/indi/libindi/drivers/telescope/
For your information, mine is here, your mileage may vary.
/home/dan/Projects/indi/libindi/drivers/telescope/

2. Making  the source:
cd /home/dan/Projects/build
make
If you get an error, you may need to:
sudo make

then
sudo make install

3. Run indiserver.
indiserver -v indi_sitech_telescope

You may need to put other devices on the command line.

4. Run SiTechExe on any machine on your local intra-net. Make sure it’s initialized.
Make sure you know the INDI port number in the SiTechExe configuration.
It’s on the misc tab in the /config/change config/ on the lower left part of the page.
You can change it if you want.

5. Now run Kstars and set things up as follows.

Go to the menu item: 
Tools/Devices/Device Manager
Click on the Client tab
Click on the “Add” button

Name = indi_sitech_telescope
Host = localhost (this could be an IP address if indi is on another machine)
Port = 7624

Now highlight the new client (indi_sitech_telescope)
Click the Connect button
the INDI control panel should show up.
Click the Connect button.
Click the “Ethernet” Button
On the right hand side, set up the IP address of the machine where you’re running SiTechExe.
Below that textbox, you can set up the SiTechExe port number.
Click the SET button.
Go back to the Main Control Tab, and click on the “Connect” button
At this point, you should be able to control the SiTech Telescope.  
You can Close the INDI control panel, and then right click on stars in Kstars, and slew, and other things.

GOOD LUCK!











