# Using an Android Emulator
Always use x86 emulators. Although arm emulators exist, they are so slow that
they are not worth your time. 

## Building for Emulation
You need to target the correct architecture via GN args:
```
target_cpu = "x86"
```

## Creating an Emulator Image
By far the easiest way to set up emulator images is to use Android Studio.
If you don't have an [Android Studio project](android_studio.md) already, you
can create a blank one to be able to reach the Virtual Device Manager screen.

Refer to: https://developer.android.com/studio/run/managing-avds.html

Where files live:
 * System partition images are stored within the sdk directory.
 * Emulator configs and data partition images are stored within
   `~/.android/avd/`.

When creating images:
 * Choose a skin with a small screen for better performance (unless you care
   about testing large screens).
 * Under "Advanced":
   * Set internal storage to 4000MB (component builds are really big).
    * Set SD card to 1000MB (our tests push a lot of files to /sdcard).

Known issues:
 * Our test & installer scripts do not work with pre-MR1 Jelly Bean.
 * Component builds do not work on pre-KitKat (due to the OS having a max
   number of shared libraries).
 * Jelly Bean and KitKat images sometimes forget to mount /sdcard :(.
   * This causes tests to fail.
   * To ensure it's there: `adb -s emulator-5554 shell mount` (look for /sdcard)
   * Can often be fixed by editing `~/.android/avd/YOUR_DEVICE/config.ini`.
     * Look for `hw.sdCard=no` and set it to `yes`

### Cloning an Image
Running tests on two emulators is twice as fast as running on one. Rather
than use the UI to create additional avds, you can clone an existing one via:

```shell
tools/android/emulator/clone_avd.py \
    --source-ini ~/.android/avd/EMULATOR_ID.ini \
    --dest-ini ~/.android/avd/EMULATOR_ID_CLONED.ini \
    --display-name "Cloned Emulator"
```

## Starting an Emulator from the Command Line
Refer to: https://developer.android.com/studio/run/emulator-commandline.html.

Note: Ctrl-C will gracefully close an emulator.

If running under remote desktop:
```
sudo apt-get install virtualgl
vglrun ~/Android/Sdk/tools/emulator @EMULATOR_ID
```

## Using an Emulator
 * Emulators show up just like devices via `adb devices`
   * Device serials will look like "emulator-5554", "emulator-5556", etc.

