# Team Dashboard Sharing

## Option 1: Same WiFi

Run:

```bash
./start_lan_dashboard.sh
```

Open the dashboard on your laptop:

```text
http://localhost:5001
```

Your team should open the **Same WiFi Team URL** shown at the top of the dashboard.

## Option 2: Public Tunnel

Install ngrok first if it is not installed:

```text
https://ngrok.com/download
```

Then run:

```bash
./start_public_tunnel.sh
```

Share the **Public Tunnel URL** shown in the terminal or at the top of the dashboard.

Keep your laptop, MQTT broker, ESP32, and dashboard running while your team is watching live data.
