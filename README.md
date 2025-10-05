### LilyGO GPS Tracker

A tiny GPS tracking app for the LilyGO T-SIM7600 board

![banner](https://github.com/nmrr/lilygo-gpstracker/blob/main/img/gpstracker16bit.jpg)

Tested on the **T-SIM7600E**. May work on other LTE+GPS LilyGO boards.

The position is transmitted every minute to a server via **UDP**. The transmission is encrypted using **AES-256-CTR** and authenticated with **HMAC-SHA256**. There is only an uplink connection: the server never communicates with the LilyGO board (for now).

## Setup

<p align="center"><img alt="T-SIM7600E" src="https://github.com/nmrr/lilygo-gpstracker/blob/main/img/lilygo.jpg" width=30% height=30%></p>
<p align="center">
T-SIM7600E board
</p>

Dependencies to install in the Arduino IDE:
* libmbedtls
* TinyGsmClient

In the LilyGO project file, add the IP address of your server. Destination port can be changed if necessary:

```
#define SERVER_ADDR "XX.XX.XX.XX"
#define SERVER_PORT 55100
```

You must generate the **AES** key and **HMAC** secret yourself. Execute the following command twice, then insert the hexadecimal string into the Node.js server and as a C-style array in the LilyGO source project. Keys must be identical on both sides.
```
openssl rand -hex 32 | tee >(sed 's/../0x&, /g' | sed 's/, $/};/;s/^/{/' | sed '1s/^/\r\n/')
```

Add the keys here in the LilyGO project:
```
unsigned char keyAES[32] = { /* YOUR AES256 KEY*/ };
const unsigned char keyHMAC[32] = { /*YOUR HMAC SECRET*/ };
```

Add the keys here in the Node.js server:
```
const AESKey = Buffer.from('YOUR AES256 KEY', 'hex')
const HMACKey = Buffer.from('YOUR HMAC SECRET', 'hex')
```

For now, a **static key** is generated. The security is sufficient for the purpose and the amount of data that will be transmitted.

If Node.js server is running on a VPS (with a dedicated IP) inside a container (like LXC) and has a local IP, you need to forward packets: 
```
sysctl -w net.ipv4.ip_forward=1
iptables -I INPUT 1 -i eno1 -p udp -m state --state NEW,ESTABLISHED -m udp --dport 55100 -j ACCEPT
iptables -t nat -A PREROUTING -i eno1 -p udp --dport 55100 -j DNAT --to-destination XX.XX.XX.XX:55100
```

Node.js server needs **express** web framework to work:
```
npm install express
```

Webpage is available here: http://XX.XX.XX.XX:3000/


## Run the project

UDP packets are arriving after a few minutes. All packets are encrypted:

<p align="center"><img src="https://github.com/nmrr/lilygo-gpstracker/blob/main/img/tcpdump.png" width=40% height=40%></p>

Your postion on the map:

<p align="center"><img src="https://github.com/nmrr/lilygo-gpstracker/blob/main/img/map.png" width=40% height=40%></p>

Currently, there’s a basic auto-refresh every 75 seconds. Real-time positioning will be available in a future release.

## What's next?

* Real-time positioning via WebSocket
* Limit data transmission only when tracker is moving
* Use esp32/modem sleep function to conserve battery

## Changelog

* 2025-10-02
  * Fix encryption issue: include IV in HMAC calculation
  * Improve timing accuracy of pause between GPS acquisitions

* 2025-09-28
  * Initial release
