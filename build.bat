call ndk-build NDK_DEBUG=1 && echo "Build Done"

for /f "delims=" %%A in ('adb shell getprop ro.product.cpu.abi') do SET "abi=%%A"

adb push libs/%abi%/minitouch /data/local/tmp
adb shell chmod 755 /data/local/tmp/minitouch

adb push libs/%abi%/gdbserver /data/local/tmp
adb shell chmod 755 /data/local/tmp/gdbserver

adb shell /data/local/tmp/gdbserver :5039 /data/local/tmp/minitouch