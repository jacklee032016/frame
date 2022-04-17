# How to use wifi

## Steps to enable wifi

* connect ethernet cable from camera to wifi router
* download and install App icSee;
* start icSee, select 'Local Login';
* Select "Add My Camera", input wifi data: SSID/password of wifi;
* Select new password of user 'admin' of camera;
* Then wait...

### Notes:

* App icSee will broadcast to find your camera in LAN: so your camera must connect to same wifi router which your cell phone connected;
* In factory default, password user 'admin' of camera is null;

Then

* If everything is OK, you can watch video from camera in icSee;
* then, diconnect the ethernet from wifi router, reboot camera;
* Check the video form camera with icSee again;

### Notes

* Now login in 'Internet Explorer', you must use your new password;
* No password is needed in 'Onvif device Manager';
* If the ethernet cable is still connected, icSee will see video also; but normally, the video is come from ethernet, not wifi;
* There are 2 network links in camera. If you want to check which link we connected to, input following command in 'Command' prompt window in Windows:

```
   arp -a

...
192.168.12.11         64-51-7e-xx-xx-xx     dynamic        # this MAC address is wifi link
...
192.168.12.11         00:12:31:xx:xx:xx     dynamic        # this MAC address is ethernet link
...
```

* both network link will use DHCP if DHCP server is available;
   * for ethernet, IP address is **192.168.1.148** when DHCP server is not available;

* When wifi is not enabled, both network link are configured in same LAN and with **same MAC address**:
   * they are 192.168.1.12 and 192.168.1.148 in my camera when DHCP server is not available;
