#!/usr/bin/env bash
# One-shot installer for Ubuntu 22.04/24.04. Run as root from the unpacked folder:
#   bash deploy/setup.sh                 -> serves over plain HTTP on the server's IP (testing)
#   bash deploy/setup.sh api.example.net -> serves over HTTPS for that domain (DNS must already point here)
set -euo pipefail
DOMAIN="${1:-}"
APP=/opt/vnaaats
SRC="$(cd "$(dirname "$0")/.." && pwd)"

echo "== Installing packages"
export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y python3 ufw caddy openssl

echo "== Creating service user and copying files"
id vnaaats >/dev/null 2>&1 || useradd --system --home "$APP" --shell /usr/sbin/nologin vnaaats
mkdir -p "$APP" /var/lib/vnaaats
cp "$SRC/vnaaats_server.py" "$SRC/cpdlc.py" "$SRC/commands.py" "$SRC/topsky_cpdlc.txt" "$SRC/fixes.json" "$APP/"
[ -f "$APP/extra_fixes.json" ] || cp "$SRC/extra_fixes.json" "$APP/"      # never overwrite your own additions
if [ -d "$SRC/web" ]; then rm -rf "$APP/web"; cp -r "$SRC/web" "$APP/web"; fi   # built flight data display
chown -R vnaaats:vnaaats "$APP" /var/lib/vnaaats

echo "== Settings (API key)"
if [ ! -f /etc/vnaaats.env ]; then
  KEY="$(openssl rand -hex 24)"
  cat > /etc/vnaaats.env <<ENV
VNAAATS_API_KEY=$KEY
VNAAATS_PORT=8080
VNAAATS_BIND=127.0.0.1
VNAAATS_DB=/var/lib/vnaaats/vnaaats.db
ENV
  chmod 600 /etc/vnaaats.env
  NEWKEY="$KEY"
else
  NEWKEY=""
fi

# Web editing password (the FDD login) - created once, change it in /etc/vnaaats.env any time
if ! grep -q '^VNAAATS_FDD_PASSWORD=' /etc/vnaaats.env; then
  NEWPASS="$(openssl rand -hex 5)"
  echo "VNAAATS_FDD_PASSWORD=$NEWPASS" >> /etc/vnaaats.env
else
  NEWPASS=""
fi

echo "== Service"
cp "$SRC/deploy/vnaaats.service" /etc/systemd/system/vnaaats.service
systemctl daemon-reload
systemctl enable --now vnaaats
systemctl restart vnaaats

echo "== Web front (Caddy)"
if [ -n "$DOMAIN" ]; then
  printf '%s {\n    reverse_proxy 127.0.0.1:8080\n}\n' "$DOMAIN" > /etc/caddy/Caddyfile
else
  printf ':80 {\n    reverse_proxy 127.0.0.1:8080\n}\n' > /etc/caddy/Caddyfile
fi
systemctl enable caddy
systemctl restart caddy

echo "== Firewall"
ufw allow OpenSSH
ufw allow 80/tcp
ufw allow 443/tcp
ufw --force enable

sleep 3
echo
echo "== Health check"
curl -fsS http://127.0.0.1:8080/health || echo "(server not answering yet - run: journalctl -u vnaaats -n 50)"
echo
if [ -n "$NEWKEY" ]; then
  echo "=============================================================="
  echo " YOUR API KEY (save it somewhere safe, it is shown only once):"
  echo " $NEWKEY"
  echo "=============================================================="
else
  echo "API key unchanged (see /etc/vnaaats.env)."
fi
if [ -n "$NEWPASS" ]; then
  echo "=============================================================="
  echo " FDD CONTROLLER PASSWORD (for editing on the web page):"
  echo " $NEWPASS"
  echo " Change it any time: nano /etc/vnaaats.env  then  systemctl restart vnaaats"
  echo "=============================================================="
else
  echo "FDD password unchanged (VNAAATS_FDD_PASSWORD in /etc/vnaaats.env)."
fi
