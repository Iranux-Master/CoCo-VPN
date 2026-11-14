# CoCo VPN: lets the login of a server bought from webpack.ir run sudo without a password, which
# CoCo's installer needs (check_user_in_sudo.sh runs sudo -n). Runs as root through sudo -S; the
# password goes through stdin, never on a command line. $LOGIN_USER is validated before use.
set -e
U="$LOGIN_USER"
F="/etc/sudoers.d/90-coco-$U"
T="$(mktemp)"
printf '%s ALL=(ALL) NOPASSWD:ALL\n' "$U" > "$T"
chmod 0440 "$T"
if visudo -cf "$T" > /dev/null 2>&1; then
  chown root:root "$T"
  mv -f "$T" "$F"
else
  rm -f "$T"
  echo COCO_SUDO_INVALID
  exit 3
fi
echo COCO_SUDO_OK
