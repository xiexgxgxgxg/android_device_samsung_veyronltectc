#!/system/bin/sh
# W2017 keypad backlight + hangup key helper
#
#  - keypad matrix keys light the backlight only while the screen is on; the
#    matrix_keypad driver turns it off after keypad_led_timeout_ms (5 s) and
#    every key press restarts that timer
#  - the backlight goes dark as soon as the screen turns off
#  - opening the inner screen (flip sensor, SW_LID 0) lights it once
#  - hangup key (KEY_ENDCALL, PMIC pon_1, code 551) keeps its ENDCALL function
#    and additionally behaves like the side power key.
#    The PMIC runs in switch_powerkey mode, so one physical long press is
#    reported as TWO event pairs: a ~70 ms pulse first and, one to two seconds
#    later, a second pair that lasts as long as the key is still held.  A plain
#    tap only produces the pulse.  Therefore:
#      pulse (< 700 ms) -> schedule the screen sleep, cancelled if a hold pair
#                          arrives in time (so a long press never sleeps first)
#      hold  (>= 700 ms) -> cancel the pending sleep and show the power menu
#    Waking is done by the WAKE flag in qpnp_pon.kl (kernel side, instant).
LED=/sys/class/sec/sec_keypad/brightness
LCD=/sys/class/leds/lcd-backlight/brightness
PON=/dev/input/event0
KEYPAD=/dev/input/event2
FLIP=/dev/input/event8
PEND=/data/local/tmp/.kb_sleep_pending
SE="sendevent $PON"

screen_on() {
  v=$(cat $LCD 2>/dev/null)
  [ -n "$v" ] && [ "$v" != "0" ]
}
light() { echo 1 > $LED 2>/dev/null; }
dark()  { echo 0 > $LED 2>/dev/null; }

power_long() {
  $SE 1 116 1; $SE 0 0 0
  sleep 0.9
  $SE 1 116 0; $SE 0 0 0
}

# --- hangup key ---------------------------------------------------------
kd=0
was_on=0
getevent -q $PON 2>/dev/null | while read -r line; do
  case "$line" in
    *"0001 0227 00000001"*)
      kd=$(cut -d' ' -f1 /proc/uptime)
      was_on=0
      screen_on && was_on=1
      light
      ;;
    *"0001 0227 00000000"*)
      now=$(cut -d' ' -f1 /proc/uptime)
      dur=$(awk -v a="$now" -v b="$kd" 'BEGIN{printf "%d", (a-b)*1000}')
      if [ "$dur" -ge 700 ]; then
        # long press: cancel any sleep armed by the leading pulse, then show
        # the power menu (the input dispatcher needs a moment after ENDCALL)
        rm -f $PEND
        sleep 0.5
        power_long
      elif [ "$was_on" = "1" ]; then
        # pulse: could still turn into a long press, so arm the sleep and let
        # the hold handler cancel it
        touch $PEND
        ( sleep 1.9
          if [ -f $PEND ]; then
            input keyevent 223
            echo 0 > $LED
            rm -f $PEND
          fi ) &
      fi
      ;;
  esac
done &

# --- keypad matrix ------------------------------------------------------
getevent -q $KEYPAD 2>/dev/null | while read -r line; do
  case "$line" in
    *"0001 "*" 00000001"*)
      screen_on && light
      ;;
  esac
done &

# --- flip opened (SW_LID 0) ---------------------------------------------
getevent -q $FLIP 2>/dev/null | while read -r line; do
  case "$line" in
    *"0005 0000 00000000"*)
      light
      ;;
  esac
done &

# --- screen turned off -> dark at once ----------------------------------
last=0
while true; do
  cur=$(cat $LCD 2>/dev/null)
  [ -z "$cur" ] || true
  if [ "$cur" = "0" ] && [ "$last" != "0" ]; then
    dark
  fi
  last=$cur
  sleep 0.2
done
