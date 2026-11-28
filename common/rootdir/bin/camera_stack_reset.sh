#!/system/bin/sh
# The wrapped Android 6 camera stack (mm-qcamera-daemon) leaks one ISP hw
# stream for every JPEG capture.  Once they run out the next camera session
# fails to link isp<->pproc, the HAL reports ERROR_DEVICE and camera apps fail
# with "unknown error".  Recycle the daemon + provider so the next open starts
# from a clean daemon.
#
# This is done in the background only while the phone is idle (no camera
# client, screen off) so that it can never collide with a camera open.

i=0
while [ $i -lt 300 ]; do
    cam_busy=0
    screen_off=0
    dumpsys media.camera 2>/dev/null | grep -q "is open" && cam_busy=1
    dumpsys power 2>/dev/null | grep -q "Display Power: state=OFF" && screen_off=1
    [ $cam_busy -eq 0 ] && [ $screen_off -eq 1 ] && break
    sleep 2
    i=$((i+1))
done

# give up if the phone never went idle (another capture will ask again)
if [ $i -ge 300 ]; then
    setprop vendor.camera.stack_reset 0
    exit 0
fi

setprop ctl.stop vendor.camera-provider-2-4
setprop ctl.stop mm-qcamera-daemon
sleep 1
setprop ctl.start mm-qcamera-daemon
sleep 5
setprop ctl.start vendor.camera-provider-2-4
setprop vendor.camera.stack_reset 0
